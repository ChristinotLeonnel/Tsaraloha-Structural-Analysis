#include "ModelTreeWidget.h"
#include "../../Model/ModelDiff.h"
#include "../../Grid/GridManager.h"
#include <QVBoxLayout>
#include <QHeaderView>

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

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({ tr("Element"), tr("Details") });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setAnimated(true);
    m_tree->setAlternatingRowColors(true);

    layout->addWidget(m_tree);

    createRootCategories();

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ModelTreeWidget::onItemSelectionChanged);
}

void ModelTreeWidget::setProjectName(const QString& name)
{
    m_projectName = name;
    if (m_projectRootItem)
    {
        m_projectRootItem->setText(0, m_projectName.isEmpty() ? tr("Projet.tsa") : m_projectName);
    }
}

void ModelTreeWidget::createRootCategories()
{
    m_tree->clear();

    QString rootText = m_projectName.isEmpty() ? tr("Projet.tsa") : m_projectName;
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

void ModelTreeWidget::refreshLoads()
{
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
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_nodesCategory->childCount(); ++i)
    {
        auto* child = m_nodesCategory->child(i);
        if (child->data(0, IdRole).toInt() == nodeId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectBeamItem(int beamId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_beamsCategory->childCount(); ++i)
    {
        auto* child = m_beamsCategory->child(i);
        if (child->data(0, IdRole).toInt() == beamId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectColumnItem(int columnId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_columnsCategory->childCount(); ++i)
    {
        auto* child = m_columnsCategory->child(i);
        if (child->data(0, IdRole).toInt() == columnId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectSlabItem(int slabId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_slabsCategory->childCount(); ++i)
    {
        auto* child = m_slabsCategory->child(i);
        if (child->data(0, IdRole).toInt() == slabId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectWallItem(int wallId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_wallsCategory->childCount(); ++i)
    {
        auto* child = m_wallsCategory->child(i);
        if (child->data(0, IdRole).toInt() == wallId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectFoundationItem(int foundationId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_foundationsCategory->childCount(); ++i)
    {
        auto* child = m_foundationsCategory->child(i);
        if (child->data(0, IdRole).toInt() == foundationId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectTrussMemberItem(int memberId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    for (int i = 0; i < m_trussCategory->childCount(); ++i)
    {
        auto* child = m_trussCategory->child(i);
        if (child->data(0, IdRole).toInt() == memberId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::selectCableItem(int cableId)
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
    if (!m_cablesCategory) return;
    for (int i = 0; i < m_cablesCategory->childCount(); ++i)
    {
        auto* child = m_cablesCategory->child(i);
        if (child->data(0, IdRole).toInt() == cableId)
        {
            child->setSelected(true);
            m_tree->scrollToItem(child);
            break;
        }
    }
}

void ModelTreeWidget::clearTreeSelection()
{
    QSignalBlocker blocker(m_tree);
    m_tree->clearSelection();
}

void ModelTreeWidget::onNodeAdded(const TSA::Model::Node& node)
{
    QString label = QString::fromStdString(node.formattedName());
    QString desc = QString("(%1, %2, %3) m").arg(node.x(), 0, 'f', 2).arg(node.y(), 0, 'f', 2).arg(node.z(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_nodesCategory, { label, desc });
    item->setData(0, TypeRole, TypeNode);
    item->setData(0, IdRole, node.id());
    m_nodesCategory->setText(1, QString("[%1]").arg(m_nodesCategory->childCount()));
}

void ModelTreeWidget::onNodeModified(const TSA::Model::Node& node)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_nodesCategory->childCount(); ++i)
    {
        auto* child = m_nodesCategory->child(i);
        if (child->data(0, IdRole).toInt() == node.id())
        {
            child->setText(0, QString::fromStdString(node.formattedName()));
            child->setText(1, QString("(%1, %2, %3) m").arg(node.x(), 0, 'f', 2).arg(node.y(), 0, 'f', 2).arg(node.z(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onNodeRemoved(int nodeId)
{
    for (int i = 0; i < m_nodesCategory->childCount(); ++i)
    {
        auto* child = m_nodesCategory->child(i);
        if (child->data(0, IdRole).toInt() == nodeId)
        {
            delete m_nodesCategory->takeChild(i);
            break;
        }
    }
    m_nodesCategory->setText(1, QString("[%1]").arg(m_nodesCategory->childCount()));
}

void ModelTreeWidget::onBeamAdded(const TSA::Model::Beam& beam)
{
    QString label = QString::fromStdString(beam.formattedName());
    QString desc = QString("N%1 -> N%2 (%3x%4 m)")
        .arg(beam.startNodeId())
        .arg(beam.endNodeId())
        .arg(beam.width(), 0, 'f', 2)
        .arg(beam.height(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_beamsCategory, { label, desc });
    item->setData(0, TypeRole, TypeBeam);
    item->setData(0, IdRole, beam.id());
    m_beamsCategory->setText(1, QString("[%1]").arg(m_beamsCategory->childCount()));
}

void ModelTreeWidget::onBeamModified(const TSA::Model::Beam& beam)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_beamsCategory->childCount(); ++i)
    {
        auto* child = m_beamsCategory->child(i);
        if (child->data(0, IdRole).toInt() == beam.id())
        {
            child->setText(0, QString::fromStdString(beam.formattedName()));
            child->setText(1, QString("N%1 -> N%2 (%3x%4 m)")
                .arg(beam.startNodeId())
                .arg(beam.endNodeId())
                .arg(beam.width(), 0, 'f', 2)
                .arg(beam.height(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onBeamRemoved(int beamId)
{
    for (int i = 0; i < m_beamsCategory->childCount(); ++i)
    {
        auto* child = m_beamsCategory->child(i);
        if (child->data(0, IdRole).toInt() == beamId)
        {
            delete m_beamsCategory->takeChild(i);
            break;
        }
    }
    m_beamsCategory->setText(1, QString("[%1]").arg(m_beamsCategory->childCount()));
}

void ModelTreeWidget::onColumnAdded(const TSA::Model::Column& column)
{
    QString label = QString::fromStdString(column.formattedName());
    QString desc = QString("N%1 -> N%2 (%3x%4 m)")
        .arg(column.startNodeId())
        .arg(column.endNodeId())
        .arg(column.width(), 0, 'f', 2)
        .arg(column.height(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_columnsCategory, { label, desc });
    item->setData(0, TypeRole, TypeColumn);
    item->setData(0, IdRole, column.id());
    m_columnsCategory->setText(1, QString("[%1]").arg(m_columnsCategory->childCount()));
}

void ModelTreeWidget::onColumnModified(const TSA::Model::Column& column)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_columnsCategory->childCount(); ++i)
    {
        auto* child = m_columnsCategory->child(i);
        if (child->data(0, IdRole).toInt() == column.id())
        {
            child->setText(0, QString::fromStdString(column.formattedName()));
            child->setText(1, QString("N%1 -> N%2 (%3x%4 m)")
                .arg(column.startNodeId())
                .arg(column.endNodeId())
                .arg(column.width(), 0, 'f', 2)
                .arg(column.height(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onColumnRemoved(int columnId)
{
    for (int i = 0; i < m_columnsCategory->childCount(); ++i)
    {
        auto* child = m_columnsCategory->child(i);
        if (child->data(0, IdRole).toInt() == columnId)
        {
            delete m_columnsCategory->takeChild(i);
            break;
        }
    }
    m_columnsCategory->setText(1, QString("[%1]").arg(m_columnsCategory->childCount()));
}

void ModelTreeWidget::onSlabAdded(const TSA::Model::Slab& slab)
{
    QString label = QString::fromStdString(slab.formattedName());
    QString desc = QString("%1 nodes, e=%2 m")
        .arg(slab.nodeIds().size())
        .arg(slab.thickness(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_slabsCategory, { label, desc });
    item->setData(0, TypeRole, TypeSlab);
    item->setData(0, IdRole, slab.id());
    m_slabsCategory->setText(1, QString("[%1]").arg(m_slabsCategory->childCount()));
}

void ModelTreeWidget::onSlabModified(const TSA::Model::Slab& slab)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_slabsCategory->childCount(); ++i)
    {
        auto* child = m_slabsCategory->child(i);
        if (child->data(0, IdRole).toInt() == slab.id())
        {
            child->setText(0, QString::fromStdString(slab.formattedName()));
            child->setText(1, QString("%1 nodes, e=%2 m")
                .arg(slab.nodeIds().size())
                .arg(slab.thickness(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onSlabRemoved(int slabId)
{
    for (int i = 0; i < m_slabsCategory->childCount(); ++i)
    {
        auto* child = m_slabsCategory->child(i);
        if (child->data(0, IdRole).toInt() == slabId)
        {
            delete m_slabsCategory->takeChild(i);
            break;
        }
    }
    m_slabsCategory->setText(1, QString("[%1]").arg(m_slabsCategory->childCount()));
}

void ModelTreeWidget::onWallAdded(const TSA::Model::Wall& wall)
{
    QString label = QString::fromStdString(wall.formattedName());
    QString desc = QString("N%1 -> N%2 (H=%3 m, e=%4 m)")
        .arg(wall.startNodeId())
        .arg(wall.endNodeId())
        .arg(wall.height(), 0, 'f', 2)
        .arg(wall.thickness(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_wallsCategory, { label, desc });
    item->setData(0, TypeRole, TypeWall);
    item->setData(0, IdRole, wall.id());
    m_wallsCategory->setText(1, QString("[%1]").arg(m_wallsCategory->childCount()));
}

void ModelTreeWidget::onWallModified(const TSA::Model::Wall& wall)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_wallsCategory->childCount(); ++i)
    {
        auto* child = m_wallsCategory->child(i);
        if (child->data(0, IdRole).toInt() == wall.id())
        {
            child->setText(0, QString::fromStdString(wall.formattedName()));
            child->setText(1, QString("N%1 -> N%2 (H=%3 m, e=%4 m)")
                .arg(wall.startNodeId())
                .arg(wall.endNodeId())
                .arg(wall.height(), 0, 'f', 2)
                .arg(wall.thickness(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onWallRemoved(int wallId)
{
    for (int i = 0; i < m_wallsCategory->childCount(); ++i)
    {
        auto* child = m_wallsCategory->child(i);
        if (child->data(0, IdRole).toInt() == wallId)
        {
            delete m_wallsCategory->takeChild(i);
            break;
        }
    }
    m_wallsCategory->setText(1, QString("[%1]").arg(m_wallsCategory->childCount()));
}

void ModelTreeWidget::onFoundationAdded(const TSA::Model::Foundation& foundation)
{
    QString label = QString::fromStdString(foundation.formattedName());
    QString desc = QString("Node N%1 (%2x%3x%4 m)")
        .arg(foundation.nodeId())
        .arg(foundation.widthA(), 0, 'f', 2)
        .arg(foundation.lengthB(), 0, 'f', 2)
        .arg(foundation.heightH(), 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_foundationsCategory, { label, desc });
    item->setData(0, TypeRole, TypeFoundation);
    item->setData(0, IdRole, foundation.id());
    m_foundationsCategory->setText(1, QString("[%1]").arg(m_foundationsCategory->childCount()));
}

void ModelTreeWidget::onFoundationModified(const TSA::Model::Foundation& foundation)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_foundationsCategory->childCount(); ++i)
    {
        auto* child = m_foundationsCategory->child(i);
        if (child->data(0, IdRole).toInt() == foundation.id())
        {
            child->setText(0, QString::fromStdString(foundation.formattedName()));
            child->setText(1, QString("Node N%1 (%2x%3x%4 m)")
                .arg(foundation.nodeId())
                .arg(foundation.widthA(), 0, 'f', 2)
                .arg(foundation.lengthB(), 0, 'f', 2)
                .arg(foundation.heightH(), 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onFoundationRemoved(int foundationId)
{
    for (int i = 0; i < m_foundationsCategory->childCount(); ++i)
    {
        auto* child = m_foundationsCategory->child(i);
        if (child->data(0, IdRole).toInt() == foundationId)
        {
            delete m_foundationsCategory->takeChild(i);
            break;
        }
    }
    m_foundationsCategory->setText(1, QString("[%1]").arg(m_foundationsCategory->childCount()));
}

void ModelTreeWidget::onTrussMemberAdded(const TSA::Model::TrussMember& member)
{
    QString label = QString::fromStdString(member.formattedName());
    QString desc = QString("N%1 -> N%2 (D=%3 m)")
        .arg(member.startNodeId())
        .arg(member.endNodeId())
        .arg(member.section().diameter, 0, 'f', 2);

    auto* item = new QTreeWidgetItem(m_trussCategory, { label, desc });
    item->setData(0, TypeRole, TypeTruss);
    item->setData(0, IdRole, member.id());
    m_trussCategory->setText(1, QString("[%1]").arg(m_trussCategory->childCount()));
}

void ModelTreeWidget::onTrussMemberModified(const TSA::Model::TrussMember& member)
{
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_trussCategory->childCount(); ++i)
    {
        auto* child = m_trussCategory->child(i);
        if (child->data(0, IdRole).toInt() == member.id())
        {
            child->setText(0, QString::fromStdString(member.formattedName()));
            child->setText(1, QString("N%1 -> N%2 (D=%3 m)")
                .arg(member.startNodeId())
                .arg(member.endNodeId())
                .arg(member.section().diameter, 0, 'f', 2));
            break;
        }
    }
}

void ModelTreeWidget::onTrussMemberRemoved(int memberId)
{
    for (int i = 0; i < m_trussCategory->childCount(); ++i)
    {
        auto* child = m_trussCategory->child(i);
        if (child->data(0, IdRole).toInt() == memberId)
        {
            delete m_trussCategory->takeChild(i);
            break;
        }
    }
    m_trussCategory->setText(1, QString("[%1]").arg(m_trussCategory->childCount()));
}

void ModelTreeWidget::onCableAdded(const TSA::Model::Cable& cable)
{
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
    item->setIcon(0, QIcon(":/icons/draw_cable.svg"));
    m_cablesCategory->setText(1, QString("[%1]").arg(m_cablesCategory->childCount()));
}

void ModelTreeWidget::onCableModified(const TSA::Model::Cable& cable)
{
    if (!m_cablesCategory) return;
    QSignalBlocker blocker(m_tree);
    for (int i = 0; i < m_cablesCategory->childCount(); ++i)
    {
        auto* child = m_cablesCategory->child(i);
        if (child->data(0, IdRole).toInt() == cable.id())
        {
            child->setText(0, QString::fromStdString(cable.formattedName()));
            child->setText(1, QString("N%1 -> N%2 | L=%3m | Ø%4mm")
                .arg(cable.startNodeId())
                .arg(cable.endNodeId())
                .arg(m_model ? cable.length(*m_model) : cable.length(), 0, 'f', 2)
                .arg(cable.diameter() * 1000.0, 0, 'f', 1));
            break;
        }
    }
}

void ModelTreeWidget::onCableRemoved(int cableId)
{
    if (!m_cablesCategory) return;
    for (int i = 0; i < m_cablesCategory->childCount(); ++i)
    {
        auto* child = m_cablesCategory->child(i);
        if (child->data(0, IdRole).toInt() == cableId)
        {
            delete m_cablesCategory->takeChild(i);
            break;
        }
    }
    m_cablesCategory->setText(1, QString("[%1]").arg(m_cablesCategory->childCount()));
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
    }
    for (int id : diff.modifiedCableIds)
    {
        if (const auto* c = m_model->getCable(id)) onCableModified(*c);
    }
    for (int id : diff.deletedCableIds)
    {
        onCableRemoved(id);
    }
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
