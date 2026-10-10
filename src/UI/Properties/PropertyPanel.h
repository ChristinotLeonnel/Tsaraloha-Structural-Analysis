#pragma once

#include <QWidget>
#include "../../Model/Model.h"
#include "../../Model/MultiEditSession.h"

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
class LoadPropertiesView;
class ElementResultsPanel;
}
namespace TSA::Coordinate { class WorkPlane; }
namespace TSA::Analysis { class ResultsModel; }
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

    /// Molette sur une liste ou un champ numérique sans le focus : fait défiler le panneau au lieu
    /// de changer la valeur (BUG-072 : un appui changeait en faisant défiler les propriétés).
    bool eventFilter(QObject* watched, QEvent* event) override;
    ~PropertyPanel() override;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

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
    void showNodalLoadProperties(int loadId);
    void showMemberLoadProperties(int loadId);
    void setWorkPlane(const TSA::Coordinate::WorkPlane& wp);
    void clearProperties();
    /// Sélection multiple : la vue affichée (élément principal) édite aussi les autres éléments du
    /// même type (champs modifiés seulement). À appeler après show*Properties(primaryId).
    void setMultiSelection(TSA::Model::ElementKind kind, int primaryId, const std::set<int>& sameKindIds);
    const TSA::Model::MultiEditSession& multiEdit() const { return m_multiEdit; }
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

    void onNodalLoadModified(int loadId) override;
    void onNodalLoadRemoved(int loadId) override;
    void onMemberLoadModified(int loadId) override;
    void onMemberLoadRemoved(int loadId) override;

    void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override;
    void onModelCleared() override;
    void onModelDestroyed() override { m_model = nullptr; }

private:
    void setupUi();
    void onViewModified();   ///< report de l'édition groupée, puis elementModified
    void endMultiEdit();

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
    LoadPropertiesView* m_loadView = nullptr;
    ElementResultsPanel* m_elementResults = nullptr;   ///< résultats de la barre sélectionnée (tout moteur)
    std::shared_ptr<TSA::Analysis::ResultsModel> m_resultsModel;
    TSA::Model::MultiEditSession m_multiEdit;
};

} // namespace TSA::UI
