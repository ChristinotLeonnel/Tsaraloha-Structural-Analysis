#include "ModelTreeWidget.h"
#include "../../Grid/GridManager.h"
#include <QVBoxLayout>
#include <QHeaderView>

namespace TSA::UI
{

enum ItemRole
{
    TypeRole = Qt::UserRole + 1,
    IdRole = Qt::UserRole + 2
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
    TypeTruss = 9
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

void ModelTreeWidget::createRootCategories()
{
    m_tree->clear();

    m_levelsCategory = new QTreeWidgetItem(m_tree, { tr("Levels"), "" });
    m_levelsCategory->setData(0, TypeRole, TypeCategory);
    m_levelsCategory->setExpanded(true);

    m_gridsCategory = new QTreeWidgetItem(m_tree, { tr("Grids"), "" });
    m_gridsCategory->setData(0, TypeRole, TypeCategory);
    m_gridsCategory->setExpanded(true);

    m_nodesCategory = new QTreeWidgetItem(m_tree, { tr("Nodes"), "" });
    m_nodesCategory->setData(0, TypeRole, TypeCategory);
    m_nodesCategory->setExpanded(true);

    m_beamsCategory = new QTreeWidgetItem(m_tree, { tr("Beams"), "" });
    m_beamsCategory->setData(0, TypeRole, TypeCategory);
    m_beamsCategory->setExpanded(true);

    m_columnsCategory = new QTreeWidgetItem(m_tree, { tr("Columns"), "" });
    m_columnsCategory->setData(0, TypeRole, TypeCategory);
    m_columnsCategory->setExpanded(true);

    m_slabsCategory = new QTreeWidgetItem(m_tree, { tr("Slabs"), "" });
    m_slabsCategory->setData(0, TypeRole, TypeCategory);
    m_slabsCategory->setExpanded(true);

    m_wallsCategory = new QTreeWidgetItem(m_tree, { tr("Walls"), "" });
    m_wallsCategory->setData(0, TypeRole, TypeCategory);
    m_wallsCategory->setExpanded(true);

    m_foundationsCategory = new QTreeWidgetItem(m_tree, { tr("Foundations"), "" });
    m_foundationsCategory->setData(0, TypeRole, TypeCategory);
    m_foundationsCategory->setExpanded(true);

    m_trussCategory = new QTreeWidgetItem(m_tree, { tr("Truss / Braces"), "" });
    m_trussCategory->setData(0, TypeRole, TypeCategory);
    m_trussCategory->setExpanded(true);
}

void ModelTreeWidget::refreshLevels()
{
    if (!m_levelsCategory)
        return;

    while (m_levelsCategory->childCount() > 0)
    {
        delete m_levelsCategory->takeChild(0);
    }

    if (!m_model || !m_model->levelManager())
        return;

    for (const auto& lvl : m_model->levelManager()->levels())
    {
        QString name = QString::fromStdString(lvl.name);
        QString details = QString("Z = %1 m%2")
            .arg(lvl.elevation, 0, 'f', 2)
            .arg(lvl.visible ? "" : tr(" (Masqué)"));

        auto* item = new QTreeWidgetItem(m_levelsCategory, { name, details });
        item->setData(0, TypeRole, TypeLevel);
        item->setData(0, IdRole, QString::fromStdString(lvl.id));
    }

    m_levelsCategory->setText(1, QString("[%1]").arg(m_levelsCategory->childCount()));
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
    default:
        emit selectionCleared();
        break;
    }
}

} // namespace TSA::UI
