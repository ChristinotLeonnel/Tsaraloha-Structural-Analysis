
#pragma once

#include "../../Model/Model.h"
#include <QTreeWidget>
#include <QWidget>


namespace TSA::Grid {
class GridManager;
}

namespace TSA::UI {

class ModelTreeWidget : public QWidget, public TSA::Model::IModelObserver {
  Q_OBJECT

public:
  explicit ModelTreeWidget(TSA::Model::Model *model, QWidget *parent = nullptr);
  ~ModelTreeWidget() override;

  void setGridManager(TSA::Grid::GridManager *gridManager);
  void refreshAll();
  void refreshLevels();
  void refreshGrids();

  void selectNodeItem(int nodeId);
  void selectBeamItem(int beamId);
  void selectColumnItem(int columnId);
  void selectSlabItem(int slabId);
  void selectWallItem(int wallId);
  void selectFoundationItem(int foundationId);
  void selectTrussMemberItem(int memberId);
  void selectCableItem(int cableId);
  void clearTreeSelection();

signals:
  void levelSelected(const QString &levelId);
  void nodeSelected(int nodeId);
  void beamSelected(int beamId);
  void columnSelected(int columnId);
  void slabSelected(int slabId);
  void wallSelected(int wallId);
  void foundationSelected(int foundationId);
  void trussMemberSelected(int memberId);
  void cableSelected(int cableId);
  void selectionCleared();

protected:
  // Implémentation de IModelObserver pour mise à jour temps réel
  void onNodeAdded(const TSA::Model::Node &node) override;
  void onNodeModified(const TSA::Model::Node &node) override;
  void onNodeRemoved(int nodeId) override;

  void onBeamAdded(const TSA::Model::Beam &beam) override;
  void onBeamModified(const TSA::Model::Beam &beam) override;
  void onBeamRemoved(int beamId) override;

  void onColumnAdded(const TSA::Model::Column &column) override;
  void onColumnModified(const TSA::Model::Column &column) override;
  void onColumnRemoved(int columnId) override;

  void onSlabAdded(const TSA::Model::Slab &slab) override;
  void onSlabModified(const TSA::Model::Slab &slab) override;
  void onSlabRemoved(int slabId) override;

  void onWallAdded(const TSA::Model::Wall &wall) override;
  void onWallModified(const TSA::Model::Wall &wall) override;
  void onWallRemoved(int wallId) override;

  void onFoundationAdded(const TSA::Model::Foundation &foundation) override;
  void onFoundationModified(const TSA::Model::Foundation &foundation) override;
  void onFoundationRemoved(int foundationId) override;

  void onTrussMemberAdded(const TSA::Model::TrussMember &member) override;
  void onTrussMemberModified(const TSA::Model::TrussMember &member) override;
  void onTrussMemberRemoved(int memberId) override;

  void onCableAdded(const TSA::Model::Cable &cable) override;
  void onCableModified(const TSA::Model::Cable &cable) override;
  void onCableRemoved(int cableId) override;

  void onModelDiffApplied(const TSA::Model::ModelDiff &diff) override;
  void onModelCleared() override;

private slots:
  void onItemSelectionChanged();

private:
  void setupUi();
  void createRootCategories();

private:
  TSA::Model::Model *m_model = nullptr;
  TSA::Grid::GridManager *m_gridManager = nullptr;
  QTreeWidget *m_tree = nullptr;

  QTreeWidgetItem *m_levelsCategory = nullptr;
  QTreeWidgetItem *m_gridsCategory = nullptr;
  QTreeWidgetItem *m_nodesCategory = nullptr;
  QTreeWidgetItem *m_beamsCategory = nullptr;
  QTreeWidgetItem *m_columnsCategory = nullptr;
  QTreeWidgetItem *m_slabsCategory = nullptr;
  QTreeWidgetItem *m_wallsCategory = nullptr;
  QTreeWidgetItem *m_foundationsCategory = nullptr;
  QTreeWidgetItem *m_trussCategory = nullptr;
  QTreeWidgetItem *m_cablesCategory = nullptr;
};

} // namespace TSA::UI
