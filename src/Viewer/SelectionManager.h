#pragma once

#include <QObject>
#include <map>
#include <set>
#include <vector>
#include <AIS_InteractiveObject.hxx>

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
    TrussMember
};

class SelectionManager : public QObject
{
    Q_OBJECT

public:
    explicit SelectionManager(QObject* parent = nullptr);
    ~SelectionManager() override = default;

    // Association ID métier <-> Objet 3D OpenCASCADE
    void registerNode(int nodeId, const Handle(AIS_InteractiveObject)& obj);
    void registerBeam(int beamId, const Handle(AIS_InteractiveObject)& obj);
    void registerColumn(int columnId, const Handle(AIS_InteractiveObject)& obj);
    void registerSlab(int slabId, const Handle(AIS_InteractiveObject)& obj);
    void registerWall(int wallId, const Handle(AIS_InteractiveObject)& obj);
    void registerFoundation(int foundationId, const Handle(AIS_InteractiveObject)& obj);
    void registerTrussMember(int memberId, const Handle(AIS_InteractiveObject)& obj);

    void unregisterNode(int nodeId);
    void unregisterBeam(int beamId);
    void unregisterColumn(int columnId);
    void unregisterSlab(int slabId);
    void unregisterWall(int wallId);
    void unregisterFoundation(int foundationId);
    void unregisterTrussMember(int memberId);

    void clearRegistry();

    // Recherche
    int getNodeId(const Handle(AIS_InteractiveObject)& obj) const;
    int getBeamId(const Handle(AIS_InteractiveObject)& obj) const;
    int getColumnId(const Handle(AIS_InteractiveObject)& obj) const;
    int getSlabId(const Handle(AIS_InteractiveObject)& obj) const;
    int getWallId(const Handle(AIS_InteractiveObject)& obj) const;
    int getFoundationId(const Handle(AIS_InteractiveObject)& obj) const;
    int getTrussMemberId(const Handle(AIS_InteractiveObject)& obj) const;

    Handle(AIS_InteractiveObject) getNodeObject(int nodeId) const;
    Handle(AIS_InteractiveObject) getBeamObject(int beamId) const;
    Handle(AIS_InteractiveObject) getColumnObject(int columnId) const;
    Handle(AIS_InteractiveObject) getSlabObject(int slabId) const;
    Handle(AIS_InteractiveObject) getWallObject(int wallId) const;
    Handle(AIS_InteractiveObject) getFoundationObject(int foundationId) const;
    Handle(AIS_InteractiveObject) getTrussMemberObject(int memberId) const;

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

    bool hasSelection() const { return m_selectionType != SelectionType::None; }
    size_t totalSelectedCount() const {
        return m_selectedNodes.size() + m_selectedBeams.size() + m_selectedColumns.size() +
               m_selectedSlabs.size() + m_selectedWalls.size() + m_selectedFoundations.size() +
               m_selectedTrussMembers.size();
    }

public slots:
    void selectNode(int nodeId, bool multiSelect = false);
    void selectBeam(int beamId, bool multiSelect = false);
    void selectColumn(int columnId, bool multiSelect = false);
    void selectSlab(int slabId, bool multiSelect = false);
    void selectWall(int wallId, bool multiSelect = false);
    void selectFoundation(int foundationId, bool multiSelect = false);
    void selectTrussMember(int memberId, bool multiSelect = false);
    void selectObject(const Handle(AIS_InteractiveObject)& obj, bool multiSelect = false);
    void setMultipleObjectsSelected(const std::vector<Handle(AIS_InteractiveObject)>& objects, bool multiSelect = false);
    void clearSelection();

signals:
    void selectionChanged();
    void nodeSelected(int nodeId);
    void beamSelected(int beamId);
    void columnSelected(int columnId);
    void slabSelected(int slabId);
    void wallSelected(int wallId);
    void foundationSelected(int foundationId);
    void trussMemberSelected(int memberId);
    void selectionCleared();

private:
    std::map<int, Handle(AIS_InteractiveObject)> m_nodeToObj;
    std::map<Handle(AIS_InteractiveObject), int> m_objToNode;

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

    SelectionType m_selectionType = SelectionType::None;
    int m_primaryId = -1;
    std::set<int> m_selectedNodes;
    std::set<int> m_selectedBeams;
    std::set<int> m_selectedColumns;
    std::set<int> m_selectedSlabs;
    std::set<int> m_selectedWalls;
    std::set<int> m_selectedFoundations;
    std::set<int> m_selectedTrussMembers;
};

} // namespace TSA::Viewer
