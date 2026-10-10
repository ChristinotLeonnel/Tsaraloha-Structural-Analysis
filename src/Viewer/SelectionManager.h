#pragma once

#include <QObject>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include <AIS_InteractiveObject.hxx>
#include "../Model/SelectionQuery.h"

namespace TSA::Model { class Model; }

namespace TSA::Viewer
{

enum class SelectionType
{
    None,
    Node,
    Beam,
    Column,
    Slab,
    Wall,
    Foundation,
    TrussMember,
    Cable,
    WorkPlane,
    NodalLoad,
    MemberLoad,
    Dimension   ///< cotation 3D (annotation)
};

class SelectionManager : public QObject
{
    Q_OBJECT

public:
    explicit SelectionManager(QObject* parent = nullptr);
    ~SelectionManager() override = default;

    // Association ID métier <-> Objet 3D OpenCASCADE
    void registerNode(int nodeId, const Handle(AIS_InteractiveObject)& obj);
    void registerSupport(int nodeId, const Handle(AIS_InteractiveObject)& obj);
    void registerBeam(int beamId, const Handle(AIS_InteractiveObject)& obj);
    void registerColumn(int columnId, const Handle(AIS_InteractiveObject)& obj);
    void registerSlab(int slabId, const Handle(AIS_InteractiveObject)& obj);
    void registerWall(int wallId, const Handle(AIS_InteractiveObject)& obj);
    void registerFoundation(int foundationId, const Handle(AIS_InteractiveObject)& obj);
    void registerTrussMember(int memberId, const Handle(AIS_InteractiveObject)& obj);
    void registerCable(int cableId, const Handle(AIS_InteractiveObject)& obj);
    void registerWorkPlane(int workPlaneId, const Handle(AIS_InteractiveObject)& obj);
    void registerNodalLoad(int loadId, const Handle(AIS_InteractiveObject)& obj);
    void registerMemberLoad(int loadId, const Handle(AIS_InteractiveObject)& obj);
    /// Cotation : plusieurs objets graphiques (lignes, texte) pour un même identifiant.
    void registerDimension(int dimensionId, const Handle(AIS_InteractiveObject)& obj);

    void unregisterNode(int nodeId);
    void unregisterSupport(int nodeId);
    void unregisterBeam(int beamId);
    void unregisterColumn(int columnId);
    void unregisterSlab(int slabId);
    void unregisterWall(int wallId);
    void unregisterFoundation(int foundationId);
    void unregisterTrussMember(int memberId);
    void unregisterCable(int cableId);
    void unregisterWorkPlane(int workPlaneId);
    void unregisterNodalLoad(int loadId);
    void unregisterMemberLoad(int loadId);
    void unregisterDimension(int dimensionId);

    void clearRegistry();

    // Recherche
    int getNodeId(const Handle(AIS_InteractiveObject)& obj) const;
    int getSupportNodeId(const Handle(AIS_InteractiveObject)& obj) const;
    int getBeamId(const Handle(AIS_InteractiveObject)& obj) const;
    int getColumnId(const Handle(AIS_InteractiveObject)& obj) const;
    int getSlabId(const Handle(AIS_InteractiveObject)& obj) const;
    int getWallId(const Handle(AIS_InteractiveObject)& obj) const;
    int getFoundationId(const Handle(AIS_InteractiveObject)& obj) const;
    int getTrussMemberId(const Handle(AIS_InteractiveObject)& obj) const;
    int getCableId(const Handle(AIS_InteractiveObject)& obj) const;
    int getWorkPlaneId(const Handle(AIS_InteractiveObject)& obj) const;
    int getNodalLoadId(const Handle(AIS_InteractiveObject)& obj) const;
    int getMemberLoadId(const Handle(AIS_InteractiveObject)& obj) const;
    int getDimensionId(const Handle(AIS_InteractiveObject)& obj) const;
    /// Objet d'un élément du modèle (nœud, barre, surface, charge...), cotations exclues.
    bool isModelObject(const Handle(AIS_InteractiveObject)& obj) const;

    Handle(AIS_InteractiveObject) getNodeObject(int nodeId) const;
    Handle(AIS_InteractiveObject) getSupportObject(int nodeId) const;
    Handle(AIS_InteractiveObject) getBeamObject(int beamId) const;
    Handle(AIS_InteractiveObject) getColumnObject(int columnId) const;
    Handle(AIS_InteractiveObject) getSlabObject(int slabId) const;
    Handle(AIS_InteractiveObject) getWallObject(int wallId) const;
    Handle(AIS_InteractiveObject) getFoundationObject(int foundationId) const;
    Handle(AIS_InteractiveObject) getTrussMemberObject(int memberId) const;
    Handle(AIS_InteractiveObject) getCableObject(int cableId) const;
    Handle(AIS_InteractiveObject) getWorkPlaneObject(int workPlaneId) const;
    Handle(AIS_InteractiveObject) getNodalLoadObject(int loadId) const;
    Handle(AIS_InteractiveObject) getMemberLoadObject(int loadId) const;

    // État de sélection
    SelectionType currentSelectionType() const { return m_selectionType; }
    int primarySelectedId() const { return m_primaryId; }
    const std::set<int>& selectedNodes() const { return m_selectedNodes; }
    const std::set<int>& selectedBeams() const { return m_selectedBeams; }
    const std::set<int>& selectedColumns() const { return m_selectedColumns; }
    const std::set<int>& selectedSlabs() const { return m_selectedSlabs; }
    const std::set<int>& selectedWalls() const { return m_selectedWalls; }
    const std::set<int>& selectedFoundations() const { return m_selectedFoundations; }
    const std::set<int>& selectedTrussMembers() const { return m_selectedTrussMembers; }
    const std::set<int>& selectedCables() const { return m_selectedCables; }
    const std::set<int>& selectedNodalLoads() const { return m_selectedNodalLoads; }
    const std::set<int>& selectedMemberLoads() const { return m_selectedMemberLoads; }
    const std::set<int>& selectedDimensions() const { return m_selectedDimensions; }
    bool isWorkPlaneSelected() const { return m_selectionType == SelectionType::WorkPlane; }
    int selectedWorkPlaneId() const { return (m_selectionType == SelectionType::WorkPlane) ? m_primaryId : -1; }

    bool hasSelection() const { return m_selectionType != SelectionType::None; }

    /// Éléments structuraux sélectionnés (hors charges et plan de travail).
    TSA::Model::ElementSet selectedElements() const;
    size_t totalSelectedCount() const {
        return m_selectedNodes.size() + m_selectedBeams.size() + m_selectedColumns.size() +
               m_selectedSlabs.size() + m_selectedWalls.size() + m_selectedFoundations.size() +
               m_selectedTrussMembers.size() + m_selectedCables.size() +
               m_selectedNodalLoads.size() + m_selectedMemberLoads.size() + m_selectedDimensions.size() +
               (isWorkPlaneSelected() ? 1 : 0);
    }

public slots:
    void selectNode(int nodeId, bool multiSelect = false);
    void selectBeam(int beamId, bool multiSelect = false);
    void selectColumn(int columnId, bool multiSelect = false);
    void selectSlab(int slabId, bool multiSelect = false);
    void selectWall(int wallId, bool multiSelect = false);
    void selectFoundation(int foundationId, bool multiSelect = false);
    void selectTrussMember(int memberId, bool multiSelect = false);
    void selectCable(int cableId, bool multiSelect = false);
    void selectWorkPlane(int workPlaneId, bool multiSelect = false);
    void selectNodalLoad(int loadId, bool multiSelect = false);
    void selectMemberLoad(int loadId, bool multiSelect = false);
    void selectDimension(int dimensionId, bool multiSelect = false);
    void selectObject(const Handle(AIS_InteractiveObject)& obj, bool multiSelect = false);
    void setMultipleObjectsSelected(const std::vector<Handle(AIS_InteractiveObject)>& objects, bool multiSelect = false);
    /// Sélection ensembliste (tout sélectionner, inverser, par type/section/matériau/niveau/plan).
    /// Un seul selectionChanged() ; multipleSelectionChanged() si plus d'un élément. Remplace les
    /// boucles de selectX() qui émettaient un signal (et un redraw du viewport) par élément.
    void selectElements(const TSA::Model::ElementSet& elements, bool addToSelection = false);
    void clearSelection();
    /// Ctrl + clic : retire l'élément s'il est sélectionné, l'ajoute sinon ; le reste de la sélection
    /// est conservé. Faux si l'objet n'est pas un élément du modèle (grille, repère…) : rien ne change.
    bool toggleObject(const Handle(AIS_InteractiveObject)& obj);
    /// Même règle par identifiant stable (nœud, barre, poteau, dalle, voile, fondation, treillis,
    /// câble, charge nodale, charge sur barre). Sans effet pour un plan de travail.
    void toggleElement(SelectionType type, int id);

    /// Remet un élément dans la sélection sans signal ni changement d'élément principal : la vue
    /// recrée la forme d'un élément modifié (unregister retire l'élément de la sélection). Émettre
    /// une sélection ici rouvrait le panneau sur ce seul élément et interrompait l'édition groupée
    /// (BUG-070 : appui appliqué à un seul des nœuds sélectionnés).
    void restoreSelected(SelectionType type, int id);
    /// Retire les éléments qui n'existent plus dans le modèle (suppression, Annuler, nettoyage) :
    /// aucune commande ne reçoit d'identifiant périmé. Vrai si la sélection a changé.
    bool pruneMissing(const TSA::Model::Model& model);
    /// pruneMissing différé et regroupé (une seule passe après une rafale de notifications du modèle).
    void schedulePrune(const TSA::Model::Model* model);

signals:
    void selectionChanged();
    void nodeSelected(int nodeId);
    void beamSelected(int beamId);
    void columnSelected(int columnId);
    void slabSelected(int slabId);
    void wallSelected(int wallId);
    void foundationSelected(int foundationId);
    void trussMemberSelected(int memberId);
    void cableSelected(int cableId);
    void workPlaneSelected(int workPlaneId);
    void nodalLoadSelected(int loadId);
    void memberLoadSelected(int loadId);
    void dimensionSelected(int dimensionId);
    void selectionCleared();
    /// Émis après une sélection ensembliste de plusieurs éléments (selectElements).
    void multipleSelectionChanged();

private:
    /// Ensemble des identifiants sélectionnés de ce type (nullptr : type non sélectionnable ainsi).
    std::set<int>* selectionSet(SelectionType type);
    /// Type et identifiant de l'élément représenté par un objet graphique ({None, -1} sinon).
    std::pair<SelectionType, int> elementOf(const Handle(AIS_InteractiveObject)& obj) const;
    void clearElementSets();
    /// Élément principal (propriétés affichées) : premier élément restant, ordre de priorité habituel.
    void choosePrimaryIfNeeded();
    /// Signaux selon la taille de la sélection : aucun élément → selectionCleared ; un seul → signal
    /// de son type (comme un clic simple) ; plusieurs → multipleSelectionChanged (surbrillance de tout
    /// l'ensemble, édition groupée) ; puis selectionChanged.
    void notifySelectionSetChanged();
    bool m_prunePending = false;

    std::map<int, Handle(AIS_InteractiveObject)> m_nodeToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToNode;

    std::map<int, Handle(AIS_InteractiveObject)> m_supportToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToSupport;

    std::map<int, Handle(AIS_InteractiveObject)> m_beamToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToBeam;

    std::map<int, Handle(AIS_InteractiveObject)> m_columnToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToColumn;

    std::map<int, Handle(AIS_InteractiveObject)> m_slabToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToSlab;

    std::map<int, Handle(AIS_InteractiveObject)> m_wallToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToWall;

    std::map<int, Handle(AIS_InteractiveObject)> m_foundationToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToFoundation;

    std::map<int, Handle(AIS_InteractiveObject)> m_trussToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToTruss;

    std::map<int, Handle(AIS_InteractiveObject)> m_cableToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToCable;

    std::map<int, Handle(AIS_InteractiveObject)> m_workPlaneToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToWorkPlane;

    std::map<int, std::vector<Handle(AIS_InteractiveObject)>> m_nodalLoadToObjs;
    std::map<Handle(AIS_InteractiveObject), int> m_objToNodalLoad;

    std::map<int, std::vector<Handle(AIS_InteractiveObject)>> m_memberLoadToObjs;
    std::map<Handle(AIS_InteractiveObject), int> m_objToMemberLoad;

    std::map<int, std::vector<Handle(AIS_InteractiveObject)>> m_dimensionToObjs;
    std::map<Handle(AIS_InteractiveObject), int> m_objToDimension;

    SelectionType m_selectionType = SelectionType::None;
    int m_primaryId = -1;
    std::set<int> m_selectedNodes;
    std::set<int> m_selectedBeams;
    std::set<int> m_selectedColumns;
    std::set<int> m_selectedSlabs;
    std::set<int> m_selectedWalls;
    std::set<int> m_selectedFoundations;
    std::set<int> m_selectedTrussMembers;
    std::set<int> m_selectedCables;
    std::set<int> m_selectedNodalLoads;
    std::set<int> m_selectedMemberLoads;
    std::set<int> m_selectedDimensions;
};

} // namespace TSA::Viewer

