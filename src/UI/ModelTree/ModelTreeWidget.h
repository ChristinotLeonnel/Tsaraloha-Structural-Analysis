#pragma once

#include <QWidget>
#include <QTreeWidget>
#include "../../Model/Model.h"

namespace TSA::Grid
{
    class GridManager;
}

namespace TSA::UI
{

class ModelTreeWidget : public QWidget, public TSA::Model::IModelObserver
{
    Q_OBJECT

public:
    explicit ModelTreeWidget(TSA::Model::Model* model, QWidget* parent = nullptr);
    ~ModelTreeWidget() override;

    void setGridManager(TSA::Grid::GridManager* gridManager);
    void refreshAll();
    void refreshGrids();

    void selectNodeItem(int nodeId);
    void selectBeamItem(int beamId);
    void selectColumnItem(int columnId);
    void selectSlabItem(int slabId);
    void clearTreeSelection();

signals:
    void nodeSelected(int nodeId);
    void beamSelected(int beamId);
    void columnSelected(int columnId);
    void slabSelected(int slabId);
    void selectionCleared();

protected:
    // Implémentation de IModelObserver pour mise à jour temps réel
    void onNodeAdded(const TSA::Model::Node& node) override;
    void onNodeModified(const TSA::Model::Node& node) override;
    void onNodeRemoved(int nodeId) override;

    void onBeamAdded(const TSA::Model::Beam& beam) override;
    void onBeamModified(const TSA::Model::Beam& beam) override;
    void onBeamRemoved(int beamId) override;

    void onColumnAdded(const TSA::Model::Column& column) override;
    void onColumnModified(const TSA::Model::Column& column) override;
    void onColumnRemoved(int columnId) override;

    void onSlabAdded(const TSA::Model::Slab& slab) override;
    void onSlabModified(const TSA::Model::Slab& slab) override;
    void onSlabRemoved(int slabId) override;

    void onModelCleared() override;

private slots:
    void onItemSelectionChanged();

private:
    void setupUi();
    void createRootCategories();

private:
    TSA::Model::Model* m_model = nullptr;
    TSA::Grid::GridManager* m_gridManager = nullptr;
    QTreeWidget* m_tree = nullptr;

    QTreeWidgetItem* m_gridsCategory = nullptr;
    QTreeWidgetItem* m_nodesCategory = nullptr;
    QTreeWidgetItem* m_beamsCategory = nullptr;
    QTreeWidgetItem* m_columnsCategory = nullptr;
    QTreeWidgetItem* m_slabsCategory = nullptr;
    QTreeWidgetItem* m_wallsCategory = nullptr;
};

} // namespace TSA::UI
