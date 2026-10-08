#include "ModelTreeWidget.h"
#include "App/ProductInfo.h"
#include "../../Model/ModelDiff.h"
#include "../../Grid/GridManager.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <functional>
#include <QLineEdit>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QTimer>

namespace TSA::UI
{

enum ItemRole
{
    TypeRole = Qt::UserRole + 1,
    IdRole = Qt::UserRole + 2,
    AxisRole = Qt::UserRole + 3,
    OffsetRole = Qt::UserRole + 4
};

enum ItemType
{
    TypeCategory = 0,
    TypeNode = 1,
    TypeBeam = 2,
    TypeColumn = 3,
    TypeSlab = 4,
    TypeGrid = 5,
    TypeLevel = 6,
    TypeWall = 7,
    TypeFoundation = 8,
    TypeTruss = 9,
    TypeCable = 10,
    TypeLoad = 11,
    TypeSupport = 12,
    TypeResult = 13,
    TypeProject = 14,
    TypeWorkPlane = 15
};

ModelTreeWidget::ModelTreeWidget(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();

    if (m_model)
    {
        m_model->addObserver(this);
        if (m_model->levelManager())
        {
            connect(m_model->levelManager(), &TSA::Coordinate::LevelManager::levelsChanged,
                    this, &ModelTreeWidget::refreshLevels);
        }
        refreshAll();
    }
}

ModelTreeWidget::~ModelTreeWidget()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void ModelTreeWidget::setGridManager(TSA::Grid::GridManager* gridManager)
{
    m_gridManager = gridManager;
    if (m_gridManager)
    {
        connect(m_gridManager, &TSA::Grid::GridManager::gridAdded, this, &ModelTreeWidget::refreshGrids);
        connect(m_gridManager, &TSA::Grid::GridManager::gridRemoved, this, &ModelTreeWidget::refreshGrids);
        connect(m_gridManager, &TSA::Grid::GridManager::gridModified, this, &ModelTreeWidget::refreshGrids);
        connect(m_gridManager, &TSA::Grid::GridManager::activeGridChanged, this, &ModelTreeWidget::refreshGrids);
        connect(m_gridManager, &TSA::Grid::GridManager::gridVisibilityChanged, this, &ModelTreeWidget::refreshGrids);
        refreshGrids();
    }
}

void ModelTreeWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // Barre de recherche + développer / réduire
    auto* bar = new QHBoxLayout();
    bar->setSpacing(3);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Rechercher dans le modèle (ex. P12, IPE)…"));
    m_search->setClearButtonEnabled(true);
    bar->addWidget(m_search, 1);
    auto* btnExpand = new QToolButton(this);
    btnExpand->setText(QStringLiteral("＋"));
    btnExpand->setToolTip(tr("Tout développer"));
    auto* btnCollapse = new QToolButton(this);
    btnCollapse->setText(QStringLiteral("－"));
    btnCollapse->setToolTip(tr("Tout réduire (sauf le projet)"));
    bar->addWidget(btnExpand);
    bar->addWidget(btnCollapse);
    layout->addLayout(bar);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({ tr("Element"), tr("Details") });
    // Colonne « Element » assez large pour les noms de catégories ; « Details » prend le reste.
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->resizeSection(0, 200);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setTextElideMode(Qt::ElideMiddle);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setAnimated(true);
    m_tree->setAlternatingRowColors(true);

    layout->addWidget(m_tree);

    createRootCategories();

    connect(m_search, &QLineEdit::textChanged, this, &ModelTreeWidget::applyFilter);
    connect(btnExpand, &QToolButton::clicked, m_tree, &QTreeWidget::expandAll);
    connect(btnCollapse, &QToolButton::clicked, this, [this] {
        m_tree->collapseAll();
        if (m_projectRootItem) m_projectRootItem->setExpanded(true);
    });

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ModelTreeWidget::showContextMenu);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ModelTreeWidget::onItemSelectionChanged);
}

void ModelTreeWidget::setContextActions(const QList<QAction*>& actions)
{
    m_contextActions = actions;
}

// Filtre : un élément reste visible s'il correspond, si l'un de ses descendants correspond,
// ou si son parent (catégorie) correspond lui-même.
void ModelTreeWidget::applyFilter(const QString& text)
{
    flushRemovals();
    const QString needle = text.trimmed();
    std::function<bool(QTreeWidgetItem*, bool)> visit = [&](QTreeWidgetItem* item, bool parentMatches) -> bool {
        const bool selfMatches = needle.isEmpty()
            || item->text(0).contains(needle, Qt::CaseInsensitive)
            || item->text(1).contains(needle, Qt::CaseInsensitive);
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChild = visit(item->child(i), parentMatches || selfMatches) || anyChild;
        const bool show = needle.isEmpty() || selfMatches || parentMatches || anyChild;
        item->setHidden(!show);
        if (!needle.isEmpty() && anyChild) item->setExpanded(true);
        return show;
    };
    m_tree->setUpdatesEnabled(false);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        visit(m_tree->topLevelItem(i), false);
    m_tree->setUpdatesEnabled(true);
}

void ModelTreeWidget::showContextMenu(const QPoint& pos)
{
    auto* item = m_tree->itemAt(pos);
    QMenu menu(this);
    const int type = item ? item->data(0, TypeRole).toInt() : TypeProject;

    // Déplacer / Copier / Supprimer n'ont de sens que pour les éléments structuraux.
    const bool isElement = item && (type == TypeNode || type == TypeBeam || type == TypeColumn || type == TypeSlab
                                    || type == TypeWall || type == TypeFoundation || type == TypeTruss || type == TypeCable);
    if (isElement)
    {
        // Sélection via le mécanisme existant (signaux de sélection → viewport, propriétés).
        m_tree->setCurrentItem(item);
        for (auto* act : m_contextActions)
        {
            if (act) menu.addAction(act);
            else menu.addSeparator(); // nullptr = séparateur
        }
        if (!menu.isEmpty()) menu.addSeparator();
    }
    if (item && item->childCount() > 0)
    {
        menu.addAction(item->isExpanded() ? tr("Réduire") : tr("Développer"), this,
                       [item] { item->setExpanded(!item->isExpanded()); });
    }
    menu.addAction(tr("Tout développer"), m_tree, &QTreeWidget::expandAll);
    menu.addAction(tr("Tout réduire"), this, [this] {
        m_tree->collapseAll();
        if (m_projectRootItem) m_projectRootItem->setExpanded(true);
    });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void ModelTreeWidget::setProjectName(const QString& name)
{
    m_projectName = name;
    if (m_projectRootItem)
    {
        m_projectRootItem->setText(0, m_projectName.isEmpty() ? tr("Projet") + TSA::Product::projectExtension() : m_projectName);
    }
}

void ModelTreeWidget::createRootCategories()
{
    m_tree->clear();
    m_itemIndex.clear();

    QString rootText = m_projectName.isEmpty() ? tr("Projet") + TSA::Product::projectExtension() : m_projectName;
    m_projectRootItem = new QTreeWidgetItem(m_tree, { rootText, "" });
    m_projectRootItem->setData(0, TypeRole, TypeProject);
    m_projectRootItem->setIcon(0, QIcon(":/icons/file/file_open.svg"));
    m_projectRootItem->setExpanded(true);

    m_levelsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Plans de travail / Niveaux"), "" });
    m_levelsCategory->setData(0, TypeRole, TypeCategory);
    m_levelsCategory->setIcon(0, QIcon(":/icons/modeling/levels.svg"));
    m_levelsCategory->setExpanded(true);

    m_gridsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Grilles"), "" });
    m_gridsCategory->setData(0, TypeRole, TypeCategory);
    m_gridsCategory->setIcon(0, QIcon(":/icons/modeling/grid_cartesian.svg"));
    m_gridsCategory->setExpanded(true);

    m_nodesCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Nœuds"), "" });
    m_nodesCategory->setData(0, TypeRole, TypeCategory);
    m_nodesCategory->setIcon(0, QIcon(":/icons/modeling/draw_node.svg"));
    m_nodesCategory->setExpanded(true);

    m_beamsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Poutres"), "" });
    m_beamsCategory->setData(0, TypeRole, TypeCategory);
    m_beamsCategory->setIcon(0, QIcon(":/icons/modeling/draw_beam.svg"));
    m_beamsCategory->setExpanded(true);

    m_columnsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Poteaux"), "" });
    m_columnsCategory->setData(0, TypeRole, TypeCategory);
    m_columnsCategory->setIcon(0, QIcon(":/icons/modeling/draw_column.svg"));
    m_columnsCategory->setExpanded(true);

    m_slabsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Dalles"), "" });
    m_slabsCategory->setData(0, TypeRole, TypeCategory);
    m_slabsCategory->setIcon(0, QIcon(":/icons/modeling/draw_slab.svg"));
    m_slabsCategory->setExpanded(true);

    m_wallsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Voiles"), "" });
    m_wallsCategory->setData(0, TypeRole, TypeCategory);
    m_wallsCategory->setIcon(0, QIcon(":/icons/modeling/draw_wall.svg"));
    m_wallsCategory->setExpanded(true);

    m_foundationsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Fondations"), "" });
    m_foundationsCategory->setData(0, TypeRole, TypeCategory);
    m_foundationsCategory->setIcon(0, QIcon(":/icons/modeling/struct_foundation.svg"));
    m_foundationsCategory->setExpanded(true);

    m_trussCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Treillis / Barres"), "" });
    m_trussCategory->setData(0, TypeRole, TypeCategory);
    m_trussCategory->setIcon(0, QIcon(":/icons/modeling/struct_truss.svg"));
    m_trussCategory->setExpanded(false);

    m_cablesCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Câbles"), "" });
    m_cablesCategory->setData(0, TypeRole, TypeCategory);
    m_cablesCategory->setIcon(0, QIcon(":/icons/modeling/draw_cable.svg"));
    m_cablesCategory->setExpanded(false);

    m_loadsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Charges"), "" });
    m_loadsCategory->setData(0, TypeRole, TypeCategory);
    m_loadsCategory->setIcon(0, QIcon(":/icons/modeling/load_dist.svg"));
    m_loadsCategory->setExpanded(true);

    m_supportsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Appuis"), "" });
    m_supportsCategory->setData(0, TypeRole, TypeCategory);
    m_supportsCategory->setIcon(0, QIcon(":/icons/modeling/support_fixed.svg"));
    m_supportsCategory->setExpanded(true);

    m_resultsCategory = new QTreeWidgetItem(m_projectRootItem, { tr("Résultats"), "" });
    m_resultsCategory->setData(0, TypeRole, TypeCategory);
    m_resultsCategory->setIcon(0, QIcon(":/icons/view/view_3d.svg"));
    m_resultsCategory->setExpanded(false);
}

void ModelTreeWidget::refreshLevels()
{
    if (!m_levelsCategory)
        return;

    while (m_levelsCategory->childCount() > 0)
    {
        delete m_levelsCategory->takeChild(0);
    }

    if (!m_model)
        return;

    // 1. Z (Niveaux horizontaux)
    auto zPlanes = m_model->detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::Z);
    auto* catZ = new QTreeWidgetItem(m_levelsCategory, { tr("Z (Niveaux horizontaux)"), QString("[%1]").arg(zPlanes.size()) });
    catZ->setData(0, TypeRole, TypeCategory);
    catZ->setIcon(0, QIcon(":/icons/modeling/levels.svg"));
    catZ->setExpanded(true);

    for (const auto& plane : zPlanes)
    {
        auto* item = new QTreeWidgetItem(catZ, { QString::fromStdString(plane.name), QString("Z = %1 m").arg(plane.offset, 0, 'f', 2) });
        item->setData(0, TypeRole, TypeWorkPlane);
        item->setData(0, IdRole, QString::fromStdString(plane.id));
        item->setData(0, AxisRole, static_cast<int>(TSA::Coordinate::WorkPlaneAxis::Z));
        item->setData(0, OffsetRole, plane.offset);
        item->setIcon(0, QIcon(":/icons/modeling/levels.svg"));
    }

    // 2. X (Coupes verticales YZ)
    auto xPlanes = m_model->detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::X);
    auto* catX = new QTreeWidgetItem(m_levelsCategory, { tr("X (Coupes YZ)"), QString("[%1]").arg(xPlanes.size()) });
    catX->setData(0, TypeRole, TypeCategory);
    catX->setIcon(0, QIcon(":/icons/view/coord_system.svg"));
    catX->setExpanded(false);

    for (const auto& plane : xPlanes)
    {
        auto* item = new QTreeWidgetItem(catX, { QString::fromStdString(plane.name), QString("X = %1 m").arg(plane.offset, 0, 'f', 2) });
        item->setData(0, TypeRole, TypeWorkPlane);
        item->setData(0, IdRole, QString::fromStdString(plane.id));
        item->setData(0, AxisRole, static_cast<int>(TSA::Coordinate::WorkPlaneAxis::X));
        item->setData(0, OffsetRole, plane.offset);
        item->setIcon(0, QIcon(":/icons/view/coord_system.svg"));
    }

    // 3. Y (Coupes verticales XZ)
    auto yPlanes = m_model->detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis::Y);
    auto* catY = new QTreeWidgetItem(m_levelsCategory, { tr("Y (Coupes XZ)"), QString("[%1]").arg(yPlanes.size()) });
    catY->setData(0, TypeRole, TypeCategory);
    catY->setIcon(0, QIcon(":/icons/view/coord_system.svg"));
    catY->setExpanded(false);

    for (const auto& plane : yPlanes)
    {
        auto* item = new QTreeWidgetItem(catY, { QString::fromStdString(plane.name), QString("Y = %1 m").arg(plane.offset, 0, 'f', 2) });
        item->setData(0, TypeRole, TypeWorkPlane);
        item->setData(0, IdRole, QString::fromStdString(plane.id));
        item->setData(0, AxisRole, static_cast<int>(TSA::Coordinate::WorkPlaneAxis::Y));
        item->setData(0, OffsetRole, plane.offset);
        item->setIcon(0, QIcon(":/icons/view/coord_system.svg"));
    }

    size_t total = zPlanes.size() + xPlanes.size() + yPlanes.size();
    m_levelsCategory->setText(1, QString("[%1]").arg(total));
}

void ModelTreeWidget::refreshGrids()
{
    if (!m_gridsCategory)
        return;

    while (m_gridsCategory->childCount() > 0)
    {
        delete m_gridsCategory->takeChild(0);
    }

    if (!m_gridManager)
        return;

    for (const auto& grid : m_gridManager->grids())
    {
        QString name = QString::fromStdString(grid->name());
        QString typeStr = (grid->type() == TSA::Grid::GridType::Cartesian) ? tr("Cartésienne") : tr("Cylindrique");
        QString details = QString("%1%2%3")
            .arg(typeStr)
            .arg(grid->isActive() ? tr(" (Active)") : "")
            .arg(grid->isVisible() ? "" : tr(" (Masquée)"));

        auto* item = new QTreeWidgetItem(m_gridsCategory, { name, details });
        item->setData(0, TypeRole, TypeGrid);
        item->setData(0, IdRole, QString::fromStdString(grid->id()));
    }

    m_gridsCategory->setText(1, QString("[%1]").arg(m_gridsCategory->childCount()));
}

void ModelTreeWidget::refreshAll()
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    createRootCategories();

    refreshLevels();
    refreshGrids();
    refreshLoads();
    refreshSupports();
    refreshResults();

    if (!m_model)
        return;

    for (const auto& [nodeId, node] : m_model->nodes())
    {
        onNodeAdded(node);
    }
    for (const auto& [beamId, beam] : m_model->beams())
    {
        onBeamAdded(beam);
    }
    for (const auto& [columnId, col] : m_model->columns())
    {
        onColumnAdded(col);
    }
    for (const auto& [slabId, slab] : m_model->slabs())
    {
        onSlabAdded(slab);
    }
    for (const auto& [wallId, wall] : m_model->walls())
    {
        onWallAdded(wall);
    }
    for (const auto& [fId, f] : m_model->foundations())
    {
        onFoundationAdded(f);
    }
    for (const auto& [trId, tr] : m_model->trussMembers())
    {
        onTrussMemberAdded(tr);
    }
    for (const auto& [cabId, cab] : m_model->cables())
    {
        onCableAdded(cab);
    }
}

void ModelTreeWidget::scheduleLoadsRefresh()
{
    if (m_loadsRefreshPending) return;
    m_loadsRefreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_loadsRefreshPending = false;
        refreshLoads();
    });
}

void ModelTreeWidget::refreshLoads()
{
    flushRemovals();
    if (!m_loadsCategory)
        return;

    while (m_loadsCategory->childCount() > 0)
    {
        delete m_loadsCategory->takeChild(0);
    }

    if (!m_model)
        return;

    const auto& lm = m_model->loadManager();
    for (const auto& [id, load] : lm.nodalLoads())
    {
        QString name = QString("Charge Nodale #%1 (Nœud %2)").arg(id).arg(load.nodeId());
        QString details = QString("F=[%1, %2, %3] kN")
            .arg(load.fx(), 0, 'f', 1)
            .arg(load.fy(), 0, 'f', 1)
            .arg(load.fz(), 0, 'f', 1);

        auto* item = new QTreeWidgetItem(m_loadsCategory, { name, details });
        item->setData(0, TypeRole, TypeLoad);
        item->setData(0, IdRole, id);
        item->setIcon(0, QIcon(":/icons/modeling/load_point.svg"));
    }

    for (const auto& [id, load] : lm.memberLoads())
    {
        QString name = QString("Charge Barre #%1 (Élém %2)").arg(id).arg(load.elementId());
        QString details = QString("q=%1 kN/m").arg(load.q1(), 0, 'f', 1);

        auto* item = new QTreeWidgetItem(m_loadsCategory, { name, details });
        item->setData(0, TypeRole, TypeLoad);
        item->setData(0, IdRole, id);
        item->setIcon(0, QIcon(":/icons/modeling/load_dist.svg"));
    }

    m_loadsCategory->setText(1, QString("[%1]").arg(m_loadsCategory->childCount()));
}

void ModelTreeWidget::refreshSupports()
{
    flushRemovals();
    if (!m_supportsCategory)
        return;

    while (m_supportsCategory->childCount() > 0)
    {
        delete m_supportsCategory->takeChild(0);
    }

    if (!m_model)
        return;

    for (int nid : m_model->supportedNodeIds())
    {
        const auto* n = m_model->getNode(nid);
        if (!n) continue;

        QString typeStr = tr("Appui");
        QString iconPath = ":/icons/modeling/support_pinned.svg";
        if (n->support().isFixed())
        {
            typeStr = tr("Encastrement");
            iconPath = ":/icons/modeling/support_fixed.svg";
        }
        else if (n->support().isPinned())
        {
            typeStr = tr("Articulation");
            iconPath = ":/icons/modeling/support_pinned.svg";
        }
        else if (n->support().isRoller())
        {
            typeStr = tr("Appui Simple");
            iconPath = ":/icons/modeling/support_roller.svg";
        }

        QString name = QString("%1 (Nœud %2)").arg(typeStr).arg(nid);
        QString details = QString("(%1, %2, %3)")
            .arg(n->x(), 0, 'f', 2)
            .arg(n->y(), 0, 'f', 2)
            .arg(n->z(), 0, 'f', 2);

        auto* item = new QTreeWidgetItem(m_supportsCategory, { name, details });
        item->setData(0, TypeRole, TypeSupport);
        item->setData(0, IdRole, nid);
        item->setIcon(0, QIcon(iconPath));
    }

    m_supportsCategory->setText(1, QString("[%1]").arg(m_supportsCategory->childCount()));
}

void ModelTreeWidget::refreshResults()
{
    if (!m_resultsCategory)
        return;

    while (m_resultsCategory->childCount() > 0)
    {
        delete m_resultsCategory->takeChild(0);
    }

    auto* itemDisp = new QTreeWidgetItem(m_resultsCategory, { tr("Déplacements"), tr("Nœuds & Déformée 3D") });
    itemDisp->setData(0, TypeRole, TypeResult);
    itemDisp->setData(0, IdRole, 1);
    itemDisp->setIcon(0, QIcon(":/icons/view/view_3d.svg"));

    auto* itemForces = new QTreeWidgetItem(m_resultsCategory, { tr("Diagrammes d'Efforts"), tr("N, Vy, Vz, Mx, My, Mz") });
    itemForces->setData(0, TypeRole, TypeResult);
    itemForces->setData(0, IdRole, 2);
    itemForces->setIcon(0, QIcon(":/icons/modeling/load_moment.svg"));

    auto* itemReact = new QTreeWidgetItem(m_resultsCategory, { tr("Réactions d'Appuis"), tr("Forces & Moments") });
    itemReact->setData(0, TypeRole, TypeResult);
    itemReact->setData(0, IdRole, 3);
    itemReact->setIcon(0, QIcon(":/icons/modeling/support_fixed.svg"));

    m_resultsCategory->setText(1, QString("[%1]").arg(m_resultsCategory->childCount()));
}

void ModelTreeWidget::selectNodeItem(int nodeId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_nodesCategory, nodeId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectBeamItem(int beamId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_beamsCategory, beamId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectColumnItem(int columnId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_columnsCategory, columnId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectSlabItem(int slabId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_slabsCategory, slabId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectWallItem(int wallId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_wallsCategory, wallId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectFoundationItem(int foundationId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_foundationsCategory, foundationId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectTrussMemberItem(int memberId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (auto* child = findElementItem(m_trussCategory, memberId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::selectCableItem(int cableId)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (!m_cablesCategory) return;
    if (auto* child = findElementItem(m_cablesCategory, cableId))
    {
        child->setSelected(true);
        m_tree->scrollToItem(child);
    }
}

void ModelTreeWidget::clearTreeSelection()
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
}

void ModelTreeWidget::onNodeAdded(const TSA::Model::Node& node)
{
    flushRemovals();
    QString label = QString::fromStdString(node.formattedName());
    QString desc = QString("(%1, %2, %3) m").arg(node.x(), 0, 'f', 2).arg(node.y(), 0, 'f', 2).arg(node.z(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_nodesCategory, { label, desc });
    item->setData(0, TypeRole, TypeNode);
    item->setData(0, IdRole, node.id());
    m_itemIndex[m_nodesCategory].insert(node.id(), item);
    m_nodesCategory->setText(1, QString("[%1]").arg(m_nodesCategory->childCount()));
}

void ModelTreeWidget::onNodeModified(const TSA::Model::Node& node)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_nodesCategory, node.id()))
    {
        child->setText(0, QString::fromStdString(node.formattedName()));
        child->setText(1, QString("(%1, %2, %3) m").arg(node.x(), 0, 'f', 2).arg(node.y(), 0, 'f', 2).arg(node.z(), 0, 'f', 2));
    }
}

void ModelTreeWidget::onNodeRemoved(int nodeId)
{
    queueRemoval(m_nodesCategory, nodeId);
}

void ModelTreeWidget::onBeamAdded(const TSA::Model::Beam& beam)
{
    flushRemovals();
    QString label = QString::fromStdString(beam.formattedName());
    QString desc = QString("N%1 -> N%2 (%3)")
        .arg(beam.startNodeId())
        .arg(beam.endNodeId())
        .arg(QString::fromStdString(beam.section().name));

    auto* item = new QTreeWidgetItem(m_beamsCategory, { label, desc });
    item->setData(0, TypeRole, TypeBeam);
    item->setData(0, IdRole, beam.id());
    m_itemIndex[m_beamsCategory].insert(beam.id(), item);
    m_beamsCategory->setText(1, QString("[%1]").arg(m_beamsCategory->childCount()));
}

void ModelTreeWidget::onBeamModified(const TSA::Model::Beam& beam)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_beamsCategory, beam.id()))
    {
        child->setText(0, QString::fromStdString(beam.formattedName()));
        child->setText(1, QString("N%1 -> N%2 (%3)")
            .arg(beam.startNodeId())
            .arg(beam.endNodeId())
    .arg(QString::fromStdString(beam.section().name)));
    }
}

void ModelTreeWidget::onBeamRemoved(int beamId)
{
    queueRemoval(m_beamsCategory, beamId);
}

void ModelTreeWidget::onColumnAdded(const TSA::Model::Column& column)
{
    flushRemovals();
    QString label = QString::fromStdString(column.formattedName());
    QString desc = QString("N%1 -> N%2 (%3)")
        .arg(column.startNodeId())
        .arg(column.endNodeId())
        .arg(QString::fromStdString(column.section().name));

    auto* item = new QTreeWidgetItem(m_columnsCategory, { label, desc });
    item->setData(0, TypeRole, TypeColumn);
    item->setData(0, IdRole, column.id());
    m_itemIndex[m_columnsCategory].insert(column.id(), item);
    m_columnsCategory->setText(1, QString("[%1]").arg(m_columnsCategory->childCount()));
}

void ModelTreeWidget::onColumnModified(const TSA::Model::Column& column)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_columnsCategory, column.id()))
    {
        child->setText(0, QString::fromStdString(column.formattedName()));
        child->setText(1, QString("N%1 -> N%2 (%3)")
            .arg(column.startNodeId())
            .arg(column.endNodeId())
    .arg(QString::fromStdString(column.section().name)));
    }
}

void ModelTreeWidget::onColumnRemoved(int columnId)
{
    queueRemoval(m_columnsCategory, columnId);
}

void ModelTreeWidget::onSlabAdded(const TSA::Model::Slab& slab)
{
    flushRemovals();
    QString label = QString::fromStdString(slab.formattedName());
    QString desc = QString("%1 nodes, e=%2 m")
        .arg(slab.nodeIds().size())
        .arg(slab.thickness(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_slabsCategory, { label, desc });
    item->setData(0, TypeRole, TypeSlab);
    item->setData(0, IdRole, slab.id());
    m_itemIndex[m_slabsCategory].insert(slab.id(), item);
    m_slabsCategory->setText(1, QString("[%1]").arg(m_slabsCategory->childCount()));
}

void ModelTreeWidget::onSlabModified(const TSA::Model::Slab& slab)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_slabsCategory, slab.id()))
    {
        child->setText(0, QString::fromStdString(slab.formattedName()));
        child->setText(1, QString("%1 nodes, e=%2 m")
            .arg(slab.nodeIds().size())
            .arg(slab.thickness(), 0, 'f', 2));
    }
}

void ModelTreeWidget::onSlabRemoved(int slabId)
{
    queueRemoval(m_slabsCategory, slabId);
}

void ModelTreeWidget::onWallAdded(const TSA::Model::Wall& wall)
{
    flushRemovals();
    QString label = QString::fromStdString(wall.formattedName());
    QString desc = QString("N%1 -> N%2 (H=%3 m, e=%4 m)")
        .arg(wall.startNodeId())
        .arg(wall.endNodeId())
        .arg(wall.height(), 0, 'f', 2)
        .arg(wall.thickness(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_wallsCategory, { label, desc });
    item->setData(0, TypeRole, TypeWall);
    item->setData(0, IdRole, wall.id());
    m_itemIndex[m_wallsCategory].insert(wall.id(), item);
    m_wallsCategory->setText(1, QString("[%1]").arg(m_wallsCategory->childCount()));
}

void ModelTreeWidget::onWallModified(const TSA::Model::Wall& wall)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_wallsCategory, wall.id()))
    {
        child->setText(0, QString::fromStdString(wall.formattedName()));
        child->setText(1, QString("N%1 -> N%2 (H=%3 m, e=%4 m)")
            .arg(wall.startNodeId())
            .arg(wall.endNodeId())
            .arg(wall.height(), 0, 'f', 2)
            .arg(wall.thickness(), 0, 'f', 2));
    }
}

void ModelTreeWidget::onWallRemoved(int wallId)
{
    queueRemoval(m_wallsCategory, wallId);
}

void ModelTreeWidget::onFoundationAdded(const TSA::Model::Foundation& foundation)
{
    flushRemovals();
    QString label = QString::fromStdString(foundation.formattedName());
    QString desc = QString("Node N%1 (%2x%3x%4 m)")
        .arg(foundation.nodeId())
        .arg(foundation.widthA(), 0, 'f', 2)
        .arg(foundation.lengthB(), 0, 'f', 2)
        .arg(foundation.heightH(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_foundationsCategory, { label, desc });
    item->setData(0, TypeRole, TypeFoundation);
    item->setData(0, IdRole, foundation.id());
    m_itemIndex[m_foundationsCategory].insert(foundation.id(), item);
    m_foundationsCategory->setText(1, QString("[%1]").arg(m_foundationsCategory->childCount()));
}

void ModelTreeWidget::onFoundationModified(const TSA::Model::Foundation& foundation)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_foundationsCategory, foundation.id()))
    {
        child->setText(0, QString::fromStdString(foundation.formattedName()));
        child->setText(1, QString("Node N%1 (%2x%3x%4 m)")
            .arg(foundation.nodeId())
            .arg(foundation.widthA(), 0, 'f', 2)
            .arg(foundation.lengthB(), 0, 'f', 2)
            .arg(foundation.heightH(), 0, 'f', 2));
    }
}

void ModelTreeWidget::onFoundationRemoved(int foundationId)
{
    queueRemoval(m_foundationsCategory, foundationId);
}

void ModelTreeWidget::onTrussMemberAdded(const TSA::Model::TrussMember& member)
{
    flushRemovals();
    QString label = QString::fromStdString(member.formattedName());
    QString desc = QString("N%1 -> N%2 (D=%3 m)")
        .arg(member.startNodeId())
        .arg(member.endNodeId())
        .arg(member.section().diameter, 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_trussCategory, { label, desc });
    item->setData(0, TypeRole, TypeTruss);
    item->setData(0, IdRole, member.id());
    m_itemIndex[m_trussCategory].insert(member.id(), item);
    m_trussCategory->setText(1, QString("[%1]").arg(m_trussCategory->childCount()));
}

void ModelTreeWidget::onTrussMemberModified(const TSA::Model::TrussMember& member)
{
    flushRemovals();
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_trussCategory, member.id()))
    {
        child->setText(0, QString::fromStdString(member.formattedName()));
        child->setText(1, QString("N%1 -> N%2 (D=%3 m)")
            .arg(member.startNodeId())
            .arg(member.endNodeId())
            .arg(member.section().diameter, 0, 'f', 2));
    }
}

void ModelTreeWidget::onTrussMemberRemoved(int memberId)
{
    queueRemoval(m_trussCategory, memberId);
}

void ModelTreeWidget::onCableAdded(const TSA::Model::Cable& cable)
{
    flushRemovals();
    if (!m_cablesCategory) return;
    QString label = QString::fromStdString(cable.formattedName());
    QString desc = QString("N%1 -> N%2 | L=%3m | Ø%4mm")
        .arg(cable.startNodeId())
        .arg(cable.endNodeId())
        .arg(m_model ? cable.length(*m_model) : cable.length(), 0, 'f', 2)
        .arg(cable.diameter() * 1000.0, 0, 'f', 1);

    auto* item = new QTreeWidgetItem(m_cablesCategory, { label, desc });
    item->setData(0, TypeRole, TypeCable);
    item->setData(0, IdRole, cable.id());
    m_itemIndex[m_cablesCategory].insert(cable.id(), item);
    item->setIcon(0, QIcon(":/icons/draw_cable.svg"));
    m_cablesCategory->setText(1, QString("[%1]").arg(m_cablesCategory->childCount()));
}

void ModelTreeWidget::onCableModified(const TSA::Model::Cable& cable)
{
    flushRemovals();
    if (!m_cablesCategory) return;
    QSignalBlocker blocker(m_tree);
    if (auto* child = findElementItem(m_cablesCategory, cable.id()))
    {
        child->setText(0, QString::fromStdString(cable.formattedName()));
        child->setText(1, QString("N%1 -> N%2 | L=%3m | Ø%4mm")
            .arg(cable.startNodeId())
            .arg(cable.endNodeId())
            .arg(m_model ? cable.length(*m_model) : cable.length(), 0, 'f', 2)
            .arg(cable.diameter() * 1000.0, 0, 'f', 1));
    }
}

void ModelTreeWidget::onCableRemoved(int cableId)
{
    queueRemoval(m_cablesCategory, cableId);
}

void ModelTreeWidget::onModelDiffApplied(const TSA::Model::ModelDiff& diff)
{
    QSignalBlocker blocker(m_tree);

    // 1. Éléments supprimés
    for (int id : diff.deletedNodeIds) onNodeRemoved(id);
    for (int id : diff.deletedBeamIds) onBeamRemoved(id);
    for (int id : diff.deletedColumnIds) onColumnRemoved(id);
    for (int id : diff.deletedSlabIds) onSlabRemoved(id);
    for (int id : diff.deletedWallIds) onWallRemoved(id);
    for (int id : diff.deletedFoundationIds) onFoundationRemoved(id);
    for (int id : diff.deletedTrussMemberIds) onTrussMemberRemoved(id);
    for (int id : diff.deletedCableIds) onCableRemoved(id);

    if (m_model)
    {
        // 2. Éléments créés
        for (int id : diff.createdNodeIds)
        {
            if (const auto* n = m_model->getNode(id)) onNodeAdded(*n);
        }
        for (int id : diff.createdBeamIds)
        {
            if (const auto* b = m_model->getBeam(id)) onBeamAdded(*b);
        }
        for (int id : diff.createdColumnIds)
        {
            if (const auto* c = m_model->getColumn(id)) onColumnAdded(*c);
        }
        for (int id : diff.createdSlabIds)
        {
            if (const auto* s = m_model->getSlab(id)) onSlabAdded(*s);
        }
        for (int id : diff.createdWallIds)
        {
            if (const auto* w = m_model->getWall(id)) onWallAdded(*w);
        }
        for (int id : diff.createdFoundationIds)
        {
            if (const auto* f = m_model->getFoundation(id)) onFoundationAdded(*f);
        }
        for (int id : diff.createdTrussMemberIds)
        {
            if (const auto* t = m_model->getTrussMember(id)) onTrussMemberAdded(*t);
        }
        for (int id : diff.createdCableIds)
        {
            if (const auto* c = m_model->getCable(id)) onCableAdded(*c);
        }

        // 3. Éléments modifiés
        for (int id : diff.modifiedNodeIds)
        {
            if (const auto* n = m_model->getNode(id)) onNodeModified(*n);
        }
        for (int id : diff.modifiedBeamIds)
        {
            if (const auto* b = m_model->getBeam(id)) onBeamModified(*b);
        }
        for (int id : diff.modifiedColumnIds)
        {
            if (const auto* c = m_model->getColumn(id)) onColumnModified(*c);
        }
        for (int id : diff.modifiedSlabIds)
        {
            if (const auto* s = m_model->getSlab(id)) onSlabModified(*s);
        }
        for (int id : diff.modifiedWallIds)
        {
            if (const auto* w = m_model->getWall(id)) onWallModified(*w);
        }
        for (int id : diff.modifiedFoundationIds)
        {
            if (const auto* f = m_model->getFoundation(id)) onFoundationModified(*f);
        }
        for (int id : diff.modifiedTrussMemberIds)
        {
            if (const auto* t = m_model->getTrussMember(id)) onTrussMemberModified(*t);
        }
        for (int id : diff.modifiedCableIds)
        {
            if (const auto* c = m_model->getCable(id)) onCableModified(*c);
        }
    }
}

QTreeWidgetItem* ModelTreeWidget::findElementItem(QTreeWidgetItem* category, int id) const
{
    if (!category) return nullptr;
    const auto it = m_itemIndex.constFind(category);
    return it == m_itemIndex.cend() ? nullptr : it->value(id, nullptr);
}

void ModelTreeWidget::queueRemoval(QTreeWidgetItem* category, int id)
{
    m_pendingRemovals[category].insert(id);
    if (!m_removalTimer)
    {
        m_removalTimer = new QTimer(this);
        m_removalTimer->setSingleShot(true);
        connect(m_removalTimer, &QTimer::timeout, this, &ModelTreeWidget::flushRemovals);
    }
    if (!m_removalTimer->isActive()) m_removalTimer->start(0);
}

void ModelTreeWidget::flushRemovals()
{
    if (m_pendingRemovals.isEmpty()) return;
    if (m_removalTimer) m_removalTimer->stop();

    QSignalBlocker blocker(m_tree);
    m_tree->setUpdatesEnabled(false);
    for (auto it = m_pendingRemovals.cbegin(); it != m_pendingRemovals.cend(); ++it)
    {
        QTreeWidgetItem* category = it.key();
        auto& index = m_itemIndex[category];
        for (int id : it.value())
        {
            // ~QTreeWidgetItem détache l'item de sa catégorie.
            if (QTreeWidgetItem* item = index.take(id)) delete item;
        }
        category->setText(1, QString("[%1]").arg(category->childCount()));
    }
    m_pendingRemovals.clear();
    m_tree->setUpdatesEnabled(true);
}

void ModelTreeWidget::onModelCleared()
{
    refreshAll();
}

void ModelTreeWidget::onItemSelectionChanged()
{
    auto selectedItems = m_tree->selectedItems();
    if (selectedItems.empty())
    {
        emit selectionCleared();
        return;
    }

    auto* item = selectedItems.first();
    int type = item->data(0, TypeRole).toInt();

    switch (type)
    {
    case TypeLevel:
        emit levelSelected(item->data(0, IdRole).toString());
        break;
    case TypeWorkPlane:
    {
        int axis = item->data(0, AxisRole).toInt();
        double offset = item->data(0, OffsetRole).toDouble();
        QString name = item->text(0);
        emit workPlaneSelected(axis, offset, name);
        if (axis == static_cast<int>(TSA::Coordinate::WorkPlaneAxis::Z))
        {
            emit levelSelected(item->data(0, IdRole).toString());
        }
        break;
    }
    case TypeNode:
        emit nodeSelected(item->data(0, IdRole).toInt());
        break;
    case TypeBeam:
        emit beamSelected(item->data(0, IdRole).toInt());
        break;
    case TypeColumn:
        emit columnSelected(item->data(0, IdRole).toInt());
        break;
    case TypeSlab:
        emit slabSelected(item->data(0, IdRole).toInt());
        break;
    case TypeWall:
        emit wallSelected(item->data(0, IdRole).toInt());
        break;
    case TypeFoundation:
        emit foundationSelected(item->data(0, IdRole).toInt());
        break;
    case TypeTruss:
        emit trussMemberSelected(item->data(0, IdRole).toInt());
        break;
    case TypeCable:
        emit cableSelected(item->data(0, IdRole).toInt());
        break;
    case TypeLoad:
        emit loadSelected(item->data(0, IdRole).toInt());
        break;
    case TypeSupport:
        emit supportSelected(item->data(0, IdRole).toInt());
        emit nodeSelected(item->data(0, IdRole).toInt());
        break;
    case TypeResult:
        emit resultsSelected();
        break;
    default:
        emit selectionCleared();
        break;
    }
}

} // namespace TSA::UI
