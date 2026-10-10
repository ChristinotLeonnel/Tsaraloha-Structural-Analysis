#pragma once

#include <cstdint>

// ============================================================
// NORMATIVE REFERENCE
// Standard   : ISO/IEC 25010:2023 §4.2.7 (Maintainability - Modularity)
// Area       : Domain Model / Single Source of Truth
// Requirement: REQ-SW-ARCH-002 (Structural Model as Single Truth)
// Interface  : IModelObserver (Asserved Observers Pattern)
// Constraint : Model must remain agnostic of UI and OCCT shapes
// ============================================================

#include "Node.h"
#include "Beam.h"
#include "Column.h"
#include "Slab.h"
#include "Wall.h"
#include "Foundation.h"
#include "TrussMember.h"
#include "Cable/Cable.h"
#include "SelectionQuery.h"
#include "Load/LoadManager.h"
#include "../Coordinate/CoordinateSystem.h"
#include "../Coordinate/WorkPlaneManager.h"
#include "../ExtensionSystem/ExtensionTypes.h"
#include "../BIM/Core/BimModel.h"
#include "../Annotation/Dimension.h"

#include <map>
#include <vector>
#include <set>
#include <memory>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>

namespace TSA::UndoRedo { class UndoManager; }
namespace TSA::Grid { class GridManager; }

namespace TSA::Model
{

struct ModelDiff;

class IModelObserver
{
public:
    virtual ~IModelObserver() = default;
    virtual void onNodeAdded(const Node& /*node*/) {}
    virtual void onNodeModified(const Node& /*node*/) {}
    virtual void onNodeRemoved(int /*nodeId*/) {}

    virtual void onBeamAdded(const Beam& /*beam*/) {}
    virtual void onBeamModified(const Beam& /*beam*/) {}
    virtual void onBeamRemoved(int /*beamId*/) {}

    virtual void onColumnAdded(const Column& /*column*/) {}
    virtual void onColumnModified(const Column& /*column*/) {}
    virtual void onColumnRemoved(int /*columnId*/) {}

    virtual void onSlabAdded(const Slab& /*slab*/) {}
    virtual void onSlabModified(const Slab& /*slab*/) {}
    virtual void onSlabRemoved(int /*slabId*/) {}

    virtual void onWallAdded(const Wall& /*wall*/) {}
    virtual void onWallModified(const Wall& /*wall*/) {}
    virtual void onWallRemoved(int /*wallId*/) {}

    virtual void onFoundationAdded(const Foundation& /*foundation*/) {}
    virtual void onFoundationModified(const Foundation& /*foundation*/) {}
    virtual void onFoundationRemoved(int /*foundationId*/) {}

    virtual void onTrussMemberAdded(const TrussMember& /*member*/) {}
    virtual void onTrussMemberModified(const TrussMember& /*member*/) {}
    virtual void onTrussMemberRemoved(int /*memberId*/) {}

    virtual void onCableAdded(const Cable& /*cable*/) {}
    virtual void onCableModified(const Cable& /*cable*/) {}
    virtual void onCableRemoved(int /*cableId*/) {}

    virtual void onNodalLoadAdded(int /*loadId*/) {}
    virtual void onNodalLoadModified(int /*loadId*/) {}
    virtual void onNodalLoadRemoved(int /*loadId*/) {}

    virtual void onMemberLoadAdded(int /*loadId*/) {}
    virtual void onMemberLoadModified(int /*loadId*/) {}
    virtual void onMemberLoadRemoved(int /*loadId*/) {}

    virtual void onLoadAdded(int /*loadId*/) {}
    virtual void onLoadModified(int /*loadId*/) {}
    virtual void onLoadRemoved(int /*loadId*/) {}
    virtual void onLoadCaseChanged(int /*caseId*/) {}

    virtual void onModelDiffApplied(const ModelDiff& /*diff*/) {}
    /// Cotations ajoutées, modifiées ou supprimées (ids ; vide = toutes, ex. style ou rechargement).
    virtual void onDimensionsChanged(const std::vector<int>& /*ids*/) {}
    virtual void onModelCleared() {}

    /// Une modification quelconque du modèle vient d'avoir lieu (ou va avoir lieu : appelée aussi
    /// par pushUndoState). Point d'accroche générique, ex. invalidation des résultats de calcul.
    /// Peut être appelée plusieurs fois pour une même opération : rester peu coûteux.
    virtual void onModelEdited() {}

    /// Le modèle est en cours de destruction : l'observateur doit oublier son pointeur (et ne plus
    /// appeler removeObserver). Sans cette notification, un observateur détruit après le modèle
    /// (widget enfant de MainWindow détruit par ~QWidget, après les membres de MainWindow)
    /// appelait removeObserver() sur un modèle libéré : violation d'accès à chaque fermeture.
    virtual void onModelDestroyed() {}
};

class Model
{
public:
    Model();
    ~Model();

    // Système de coordonnées et niveaux centralisés
    TSA::Coordinate::CoordinateSystem* coordinateSystem() { return m_coordinateSystem.get(); }
    const TSA::Coordinate::CoordinateSystem* coordinateSystem() const { return m_coordinateSystem.get(); }

    TSA::Coordinate::LevelManager* levelManager();
    const TSA::Coordinate::LevelManager* levelManager() const;

    /// Grilles du projet (possédées par la fenêtre principale) : rattachées pour que l'historique
    /// Annuler / Rétablir les couvre. nullptr = grilles hors historique.
    void setGridManager(TSA::Grid::GridManager* gridManager) noexcept { m_gridManager = gridManager; }
    TSA::Grid::GridManager* gridManager() const noexcept { return m_gridManager; }

    TSA::Coordinate::WorkPlaneManager* workPlaneManager() { return m_workPlaneManager.get(); }
    const TSA::Coordinate::WorkPlaneManager* workPlaneManager() const { return m_workPlaneManager.get(); }

    // Détection des plans de travail structurels (Z, X, Y)
    std::vector<TSA::Coordinate::DetectedPlaneInfo> detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis axis) const;

    // Observateurs
    void addObserver(IModelObserver* observer);
    void removeObserver(IModelObserver* observer);

    // Gestion des nœuds
    int addNode(double x, double y, double z, const std::string& levelId = "", const std::string& name = "");
    bool addNodeWithId(int id, double x, double y, double z, const std::string& levelId = "", const std::string& name = "");
    int addNodeAtGridIntersection(int ix, int iy, int iz);
    int addColumnBetweenLevels(int levelStartIndex, int levelEndIndex, double x, double y, double width = 0.30, double height = 0.30);
    bool removeNode(int nodeId);
    Node* getNode(int nodeId);
    const Node* getNode(int nodeId) const;
    const std::map<int, Node>& nodes() const { return m_nodes; }

    // Identification des nœuds (libres vs connectés, appuis)
    bool isNodeFree(int nodeId) const;
    /// Vrai si placer le nœud en (x, y, z) rendrait un élément connecté de longueur quasi nulle
    /// (extrémités confondues à tol près) : modification à refuser.
    bool wouldCollapseConnectedElement(int nodeId, double x, double y, double z, double tol = 1e-6) const;
    std::vector<int> freeNodeIds() const;
    std::vector<int> supportedNodeIds() const;

    // Modification d'un niveau d'étage avec propagation instantanée aux objets attachés
    void onLevelElevationChanged(const std::string& levelId, double oldElevation, double newElevation);

    // Gestion des poutres et barres structurales (Bar)
    int addBeam(int startNodeId, int endNodeId, double width = 0.30, double height = 0.50, const std::string& name = "");
    bool addBeamWithId(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.50, const std::string& name = "");
    int addBar(int startNodeId, int endNodeId, const Section& section, const Material& material, BarRole role = BarRole::Beam, double rotation = 0.0, const std::string& name = "");
    int addBar(const BarProperties& props, int startNodeId, int endNodeId);
    bool removeBeam(int beamId);
    Beam* getBeam(int beamId);
    const Beam* getBeam(int beamId) const;
    const std::map<int, Beam>& beams() const { return m_beams; }

    // Alias Bar
    Bar* getBar(int barId) { return getBeam(barId); }
    const Bar* getBar(int barId) const { return getBeam(barId); }
    bool removeBar(int barId) { return removeBeam(barId); }
    const std::map<int, Bar>& bars() const { return m_beams; }

    // Gestion des poteaux
    int addColumn(int startNodeId, int endNodeId, double width = 0.30, double height = 0.30, const std::string& name = "");
    int addColumn(int startNodeId, int endNodeId, const Section& section, const Material& material, double rotation = 0.0, const std::string& name = "");
    bool addColumnWithId(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.30, const std::string& name = "");
    bool removeColumn(int columnId);
    Column* getColumn(int columnId);
    const Column* getColumn(int columnId) const;
    const std::map<int, Column>& columns() const { return m_columns; }

    // Gestion des dalles
    int addSlab(const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);
    bool addSlabWithId(int id, const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);
    bool removeSlab(int slabId);
    Slab* getSlab(int slabId);
    const Slab* getSlab(int slabId) const;
    const std::map<int, Slab>& slabs() const { return m_slabs; }

    // Gestion des voiles (Walls)
    int addWall(int startNodeId, int endNodeId, double height = 3.0, double thickness = 0.20, const std::string& name = "");
    bool addWallWithId(int id, int startNodeId, int endNodeId, double height = 3.0, double thickness = 0.20, const std::string& name = "");
    bool removeWall(int wallId);
    Wall* getWall(int wallId);
    const Wall* getWall(int wallId) const;
    const std::map<int, Wall>& walls() const { return m_walls; }

    // Gestion des fondations
    int addFoundation(int nodeId, double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50, const std::string& name = "", FoundationType type = FoundationType::IsolatedFooting);
    bool addFoundationWithId(int id, int nodeId, double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50, const std::string& name = "", FoundationType type = FoundationType::IsolatedFooting);
    bool removeFoundation(int foundationId);
    Foundation* getFoundation(int foundationId);
    const Foundation* getFoundation(int foundationId) const;
    const std::map<int, Foundation>& foundations() const { return m_foundations; }

    // Gestion des treillis & contreventements
    int addTrussMember(int startNodeId, int endNodeId, double diameterOrWidth = 0.10, const std::string& name = "", TrussMemberRole role = TrussMemberRole::Diagonal);
    bool addTrussMemberWithId(int id, int startNodeId, int endNodeId, double diameterOrWidth = 0.10, const std::string& name = "", TrussMemberRole role = TrussMemberRole::Diagonal);
    bool removeTrussMember(int memberId);
    TrussMember* getTrussMember(int memberId);
    const TrussMember* getTrussMember(int memberId) const;
    const std::map<int, TrussMember>& trussMembers() const { return m_trussMembers; }

    // Gestion des câbles & éléments tendus
    int addCable(int startNodeId, int endNodeId, double diameter = 0.020, const std::string& name = "", CableGeometryMode mode = CableGeometryMode::Straight, double sag = 0.0);
    int addCable(int startNodeId, int endNodeId, CableType type, const std::string& name = "", CableGeometryMode mode = CableGeometryMode::Straight, double sag = 0.0);
    int addCable(int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name = "", CableGeometryMode mode = CableGeometryMode::Straight, double sag = 0.0);
    bool addCableWithId(int id, int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name = "", CableGeometryMode mode = CableGeometryMode::Straight, double sag = 0.0);
    bool removeCable(int cableId);
    Cable* getCable(int cableId);
    const Cable* getCable(int cableId) const;
    const std::map<int, Cable>& cables() const { return m_cables; }

    // Transformations
    bool moveNodes(const std::set<int>& nodeIds, double dx, double dy, double dz);
    bool rotateNodes(const std::set<int>& nodeIds, const gp_Pnt& center, const gp_Dir& axis, double angleRad);
    std::vector<int> copyElements(const std::set<int>& nodeIds,
                                  const std::set<int>& beamIds,
                                  const std::set<int>& columnIds,
                                  const std::set<int>& slabIds,
                                  double dx, double dy, double dz, int repetitions = 1,
                                  const std::set<int>& cableIds = {},
                                  const std::set<int>& trussIds = {});
    std::vector<int> copyAndRotateElements(const std::set<int>& nodeIds,
                                          const std::set<int>& beamIds,
                                          const std::set<int>& columnIds,
                                          const std::set<int>& slabIds,
                                          const gp_Pnt& center, const gp_Dir& axis,
                                          double angleRad, int repetitions = 1,
                                          const std::set<int>& cableIds = {},
                                          const std::set<int>& trussIds = {});

    // Topologie : symétrie, division de barres, fusion de nœuds (Model_Topology.cpp)
    /// Symétrie par rapport au plan (planePoint, planeNormal).
    /// keepOriginal = true : copie miroir, les nœuds situés sur le plan (à planeTol près) sont
    /// partagés et un élément entièrement sur le plan n'est pas dupliqué ; retourne les ids créés.
    /// keepOriginal = false : les nœuds sont déplacés ; retourne les ids des nœuds déplacés.
    /// L'ordre des nœuds des dalles est inversé pour conserver leur orientation.
    std::vector<int> mirrorElements(const std::set<int>& nodeIds,
                                    const std::set<int>& beamIds,
                                    const std::set<int>& columnIds,
                                    const std::set<int>& slabIds,
                                    const gp_Pnt& planePoint, const gp_Dir& planeNormal,
                                    bool keepOriginal,
                                    const std::set<int>& cableIds = {},
                                    double planeTol = 1e-6);

    /// Divise une poutre en `segments` tronçons égaux. La poutre d'origine devient le premier
    /// tronçon (relâchement de fin reporté sur le dernier), les charges réparties uniformes sur
    /// toute la portée sont recopiées sur chaque tronçon. Retourne les ids de tous les tronçons
    /// (origine en tête), vide si refus : segments < 2, barre absente ou de longueur nulle, charge
    /// ponctuelle / partielle / trapézoïdale sur la barre (non redistribuable sans ambiguïté).
    std::vector<int> splitBeam(int beamId, int segments);
    /// Idem pour un poteau.
    std::vector<int> splitColumn(int columnId, int segments);

    /// Divise une barre (poutre, poteau ou treillis) au paramètre t ∈ ]0, 1[ (0 = nœud de début).
    /// nodeId > 0 : ce nœud existant devient le point de division (il doit être sur la barre,
    /// sinon il est utilisé tel quel) ; sinon un nœud est créé en p(t). La barre d'origine devient
    /// le tronçon de début, une nouvelle barre (mêmes attributs) le tronçon de fin. Charges : mêmes
    /// règles que splitBeam. Retourne le nœud de division (0 si refus) ; *newBarId = tronçon créé.
    int splitBarAt(ElementKind kind, int id, double t, int nodeId = 0, int* newBarId = nullptr);

    /// Applique une transformation géométrique quelconque aux nœuds (échelle, rotation, translation).
    bool transformNodes(const std::set<int>& nodeIds, const gp_Trsf& trsf);

    /// Nœuds géométriquement confondus à `tol` près : doublon → nœud conservé (plus petit id).
    std::map<int, int> findCoincidentNodes(double tol = 1e-3) const;
    /// Fusionne les nœuds confondus : éléments, fondations, charges nodales et appuis reportés sur
    /// le nœud conservé ; éléments devenus de longueur nulle (et dalles < 3 nœuds) supprimés.
    /// Retourne le nombre de nœuds supprimés.
    int mergeCoincidentNodes(double tol = 1e-3);

    struct ModelStateSnapshot
    {
        std::map<int, Node> nodes;
        std::map<int, Beam> beams;
        std::map<int, Column> columns;
        std::map<int, Slab> slabs;
        std::map<int, Wall> walls;
        std::map<int, Foundation> foundations;
        std::map<int, TrussMember> trussMembers;
        std::map<int, Cable> cables;
        LoadManager::LoadSnapshot loadSnapshot;
        std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot> calculationSnapshots;
        std::map<std::string, TSA::ExtensionSystem::DefinitionReference> definitionReferences;
        TSA::BIM::BimModel bim;   ///< couche BIM (produits physiques, GlobalId) synchronisée
        TSA::Annotation::DimensionSet dimensions; ///< cotations (annotations, hors calcul)
        // Données de définition hors éléments (BUG-003) : axes X/Y et niveaux (CoordinateSystem), grilles
        // (GridManager rattaché). Vide = non capturé : la restauration laisse alors l'état courant.
        // Les plans de travail sont un état d'affichage (comme la caméra) : ils ne sont pas annulés.
        std::string coordinatesJson;
        std::string gridsJson;
        int nextNodeId = 1;
        int nextBeamId = 1;
        int nextColumnId = 1;
        int nextSlabId = 1;
        int nextWallId = 1;
        int nextFoundationId = 1;
        int nextTrussMemberId = 1;
        int nextCableId = 1;
        std::string actionName;
    };

    // Gestionnaire de charges, cas de charges et combinaisons
    LoadManager& loadManager() noexcept { return m_loadManager; }
    const LoadManager& loadManager() const noexcept { return m_loadManager; }

    // Historique Undo / Redo (Ctrl+Z / Ctrl+Y)
    void pushUndoState(const std::string& actionName = "", const std::string& coalesceKey = "");
    /// Entrée Annuler pour un changement d'étiquettes seulement (renumérotation) : révision inchangée.
    void pushLabelUndoState(const std::string& actionName);
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void clearUndoRedo();
    std::string lastUndoActionName() const;
    std::string lastRedoActionName() const;

    TSA::UndoRedo::UndoManager* undoManager();
    const TSA::UndoRedo::UndoManager* undoManager() const;

    ModelStateSnapshot createSnapshot(const std::string& actionName = "") const;
    void restoreSnapshot(const ModelStateSnapshot& snapshot);
    void applySnapshotData(const ModelStateSnapshot& snapshot);
    void notifyModelDiffApplied(const ModelDiff& diff);

    // Notifications de modification
    void notifyNodeModified(int nodeId);
    void notifyBeamModified(int beamId);
    void notifyColumnModified(int columnId);
    void notifySlabModified(int slabId);
    void notifyWallModified(int wallId);
    void notifyFoundationModified(int foundationId);
    void notifyTrussMemberModified(int memberId);
    void notifyCableModified(int cableId);

    void notifyNodalLoadAdded(int loadId);
    void notifyNodalLoadModified(int loadId);
    void notifyNodalLoadRemoved(int loadId);

    void notifyMemberLoadAdded(int loadId);
    void notifyMemberLoadModified(int loadId);
    void notifyMemberLoadRemoved(int loadId);

    void notifyLoadAdded(int loadId);
    void notifyLoadModified(int loadId);
    void notifyLoadRemoved(int loadId);
    void notifyLoadCaseChanged(int caseId);

    // État de modification du document (Dirty state)
    /// Paramètres d'analyse du projet (AnalysisContext::toJson, schéma versionné), persistés dans le
    /// chunk SETT du .tsa. JSON opaque pour le modèle (pas de dépendance vers Analysis) ; état de
    /// configuration, hors historique Annuler. Vide = réglages par défaut.
    const std::string& analysisSettingsJson() const noexcept { return m_analysisSettingsJson; }
    void setAnalysisSettingsJson(const std::string& json) { m_analysisSettingsJson = json; }

    /// Paramètres de topologie et de numérotation (TSA::Topology::TopologySettings::toJson),
    /// persistés dans le chunk TOPO du .tsa (format ≥ 1.5). Vide = valeurs par défaut (anciens projets).
    const std::string& topologySettingsJson() const noexcept { return m_topologySettingsJson; }
    void setTopologySettingsJson(const std::string& json) { m_topologySettingsJson = json; }
    /// Données d'échange non interprétées par TSA (format TSA3D : champs et extensions inconnus d'un
    /// fichier tiers), conservées pour être restituées à l'export ; JSON opaque persisté (chunk XTND, 1.7).
    const std::string& exchangeExtensionsJson() const noexcept { return m_exchangeExtensionsJson; }
    void setExchangeExtensionsJson(const std::string& json) { m_exchangeExtensionsJson = json; }

    /// Étiquettes (noms visibles) modifiées sans autre changement : observateurs notifiés par
    /// onModelDiffApplied, document marqué modifié, mais révision inchangée — les résultats de calcul
    /// référencent les identifiants internes et restent valides après une renumérotation.
    void notifyLabelsChanged(const ModelDiff& diff);

    /// Cotations 3D (annotations : jamais transmises au calcul). Modifiées par src/Annotation/DimensionService,
    /// persistées dans le chunk DIMS du .tsa (format ≥ 1.6), comprises dans l'historique Annuler.
    const TSA::Annotation::DimensionSet& dimensions() const noexcept { return m_dimensions; }
    TSA::Annotation::DimensionSet& dimensionsForEdit() noexcept { return m_dimensions; }
    void setDimensions(const TSA::Annotation::DimensionSet& set) { m_dimensions = set; }
    /// Notifie les vues (ids vides = toutes) ; document modifié, révision inchangée (résultats conservés).
    void notifyDimensionsChanged(const std::vector<int>& ids);

    bool isModified() const { return m_isModified; }
    void setModified(bool modified)
    {
        m_isModified = modified;
        if (modified) bumpRevision();
    }

    /// Révision du modèle : augmente à chaque modification notifiée (jamais décrémentée, y compris
    /// par Undo). Permet de savoir si des résultats de calcul correspondent encore au modèle.
    std::uint64_t revision() const noexcept { return m_revision; }

    // Réinitialisation
    void clear();

    // Snapshots mécaniques de calcul & références d'extensions (Phase 8)
    const std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot>& calculationSnapshots() const { return m_calculationSnapshots; }
    const std::map<std::string, TSA::ExtensionSystem::DefinitionReference>& definitionReferences() const { return m_definitionReferences; }
    void setCalculationSnapshot(const std::string& key, const TSA::ExtensionSystem::MechanicalSnapshot& snapshot, const TSA::ExtensionSystem::DefinitionReference& ref = {});
    const TSA::ExtensionSystem::MechanicalSnapshot* getCalculationSnapshot(const std::string& key) const;
    const TSA::ExtensionSystem::DefinitionReference* getDefinitionReference(const std::string& key) const;
    bool hasCalculationSnapshot(const std::string& key) const;
    void removeCalculationSnapshot(const std::string& key);
    void clearCalculationSnapshots();

    // Couche BIM (docs/BIM_ARCHITECTURE.md) : produits physiques, structure spatiale, GlobalId.
    // Synchronisée paresseusement avec le modèle analytique (aucun produit n'est perdu ni
    // recréé : les identifiants restent stables à travers Annuler / Rétablir et la sauvegarde).
    const TSA::BIM::BimModel& bim() const;
    /// Accès en écriture (synchronisé). L'appelant encadre la modification par pushUndoState /
    /// EditTransaction puis setModified(true).
    TSA::BIM::BimModel& bimForEdit();
    /// Remplace la couche BIM (chargement .tsa) ; la synchronisation complète ce qui manque.
    void setBim(const TSA::BIM::BimModel& bim);

    // Générateurs d'identifiants
    int nextNodeId() const { return m_nextNodeId; }
    int nextBeamId() const { return m_nextBeamId; }
    int nextColumnId() const { return m_nextColumnId; }
    int nextSlabId() const { return m_nextSlabId; }
    int nextWallId() const { return m_nextWallId; }
    int nextFoundationId() const { return m_nextFoundationId; }
    int nextTrussMemberId() const { return m_nextTrussMemberId; }
    int nextCableId() const { return m_nextCableId; }

private:
    int m_nextNodeId = 1;
    int m_nextBeamId = 1;
    int m_nextColumnId = 1;
    int m_nextSlabId = 1;
    int m_nextWallId = 1;
    int m_nextFoundationId = 1;
    int m_nextTrussMemberId = 1;
    int m_nextCableId = 1;

    std::map<int, Node> m_nodes;
    std::map<int, Beam> m_beams;
    std::map<int, Column> m_columns;
    std::map<int, Slab> m_slabs;
    std::map<int, Wall> m_walls;
    std::map<int, Foundation> m_foundations;
    std::map<int, TrussMember> m_trussMembers;
    std::map<int, Cable> m_cables;
    std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot> m_calculationSnapshots;
    std::map<std::string, TSA::ExtensionSystem::DefinitionReference> m_definitionReferences;
    LoadManager m_loadManager;

    mutable TSA::BIM::BimModel m_bim;
    mutable std::uint64_t m_bimSignature = ~0ull;
    std::uint64_t bimSignature() const;
    void syncBim() const;

    std::vector<IModelObserver*> m_observers;
    std::uint64_t m_revision = 0;
    void bumpRevision();

    std::unique_ptr<TSA::UndoRedo::UndoManager> m_undoManager;

    std::shared_ptr<TSA::Coordinate::CoordinateSystem> m_coordinateSystem;
    std::unique_ptr<TSA::Coordinate::WorkPlaneManager> m_workPlaneManager;
    TSA::Grid::GridManager* m_gridManager = nullptr; ///< non possédé (voir setGridManager)
    bool m_isModified = false;
    std::string m_analysisSettingsJson;
    std::string m_topologySettingsJson;
    std::string m_exchangeExtensionsJson;
    TSA::Annotation::DimensionSet m_dimensions;
};

} // namespace TSA::Model
