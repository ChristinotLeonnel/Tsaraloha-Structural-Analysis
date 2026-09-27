#pragma once

#include <QWidget>
#include "../../Model/Model.h"

class QLabel;
class QStackedWidget;
class QScrollArea;

namespace TSA::UI
{

class NodePropertiesView;
class BeamPropertiesView;
class ColumnPropertiesView;
class CablePropertiesView;
class SlabPropertiesView;
class WallPropertiesView;
class FoundationPropertiesView;
class TrussMemberPropertiesView;
class WorkPlanePropertiesView;
}
namespace TSA::Coordinate { class WorkPlane; }
namespace TSA::UI
{

/**
 * @brief Panneau hôte centralisé d'inspection des propriétés CAO (Inspector).
 * Héberge et orchestre les vues de propriétés spécialisées par élément métier (Règle 14)
 * au sein d'une infrastructure commune partagée (Modèle, TSALib, Commandes, Undo/Redo).
 */
class PropertyPanel : public QWidget, public TSA::Model::IModelObserver
{
    Q_OBJECT

public:
    explicit PropertyPanel(TSA::Model::Model* model, QWidget* parent = nullptr);
    ~PropertyPanel() override;

    void setModel(TSA::Model::Model* model);

public slots:
    void showLevelProperties(const QString& levelId);
    void showNodeProperties(int nodeId);
    void showBeamProperties(int beamId);
    void showColumnProperties(int columnId);
    void showSlabProperties(int slabId);
    void showWallProperties(int wallId);
    void showFoundationProperties(int foundationId);
    void showTrussMemberProperties(int memberId);
    void showCableProperties(int cableId);
    void showWorkPlaneProperties(int workPlaneId);
    void setWorkPlane(const TSA::Coordinate::WorkPlane& wp);
    void clearProperties();
    void refreshLibraryLists();

    WorkPlanePropertiesView* workPlaneView() const { return m_workPlaneView; }

signals:
    void elementModified();
    void workPlaneModified(const TSA::Coordinate::WorkPlane& wp);

protected:
    // IModelObserver overrides pour la synchronisation bidirectionnelle 100% temps réel
    void onNodeModified(const TSA::Model::Node& node) override;
    void onNodeRemoved(int nodeId) override;

    void onBeamModified(const TSA::Model::Beam& beam) override;
    void onBeamRemoved(int beamId) override;

    void onColumnModified(const TSA::Model::Column& column) override;
    void onColumnRemoved(int columnId) override;

    void onSlabModified(const TSA::Model::Slab& slab) override;
    void onSlabRemoved(int slabId) override;

    void onWallModified(const TSA::Model::Wall& wall) override;
    void onWallRemoved(int wallId) override;

    void onFoundationModified(const TSA::Model::Foundation& foundation) override;
    void onFoundationRemoved(int foundationId) override;

    void onTrussMemberModified(const TSA::Model::TrussMember& member) override;
    void onTrussMemberRemoved(int memberId) override;

    void onCableModified(const TSA::Model::Cable& cable) override;
    void onCableRemoved(int cableId) override;

    void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override;
    void onModelCleared() override;

private:
    void setupUi();

    TSA::Model::Model* m_model = nullptr;

    QLabel* m_titleLabel = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_emptyView = nullptr;

    // Vues spécialisées par élément structural métier (Règle 14)
    NodePropertiesView* m_nodeView = nullptr;
    BeamPropertiesView* m_beamView = nullptr;
    ColumnPropertiesView* m_columnView = nullptr;
    CablePropertiesView* m_cableView = nullptr;
    SlabPropertiesView* m_slabView = nullptr;
    WallPropertiesView* m_wallView = nullptr;
    FoundationPropertiesView* m_foundationView = nullptr;
    TrussMemberPropertiesView* m_trussView = nullptr;
    WorkPlanePropertiesView* m_workPlaneView = nullptr;
};

} // namespace TSA::UI
