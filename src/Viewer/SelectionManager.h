#pragma once

#include <QObject>
#include <map>
#include <set>
#include <AIS_InteractiveObject.hxx>

namespace TSA::Viewer
{

enum class SelectionType
{
    None,
    Node,
    Beam,
    Column,
    Slab
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

    void unregisterNode(int nodeId);
    void unregisterBeam(int beamId);
    void unregisterColumn(int columnId);
    void unregisterSlab(int slabId);

    void clearRegistry();

    // Recherche
    int getNodeId(const Handle(AIS_InteractiveObject)& obj) const;
    int getBeamId(const Handle(AIS_InteractiveObject)& obj) const;
    int getColumnId(const Handle(AIS_InteractiveObject)& obj) const;
    int getSlabId(const Handle(AIS_InteractiveObject)& obj) const;

    Handle(AIS_InteractiveObject) getNodeObject(int nodeId) const;
    Handle(AIS_InteractiveObject) getBeamObject(int beamId) const;
    Handle(AIS_InteractiveObject) getColumnObject(int columnId) const;
    Handle(AIS_InteractiveObject) getSlabObject(int slabId) const;

    // État de sélection
    SelectionType currentSelectionType() const { return m_selectionType; }
    int primarySelectedId() const { return m_primaryId; }
    const std::set<int>& selectedNodes() const { return m_selectedNodes; }
    const std::set<int>& selectedBeams() const { return m_selectedBeams; }
    const std::set<int>& selectedColumns() const { return m_selectedColumns; }
    const std::set<int>& selectedSlabs() const { return m_selectedSlabs; }

    bool hasSelection() const { return m_selectionType != SelectionType::None; }
    size_t totalSelectedCount() const { return m_selectedNodes.size() + m_selectedBeams.size() + m_selectedColumns.size() + m_selectedSlabs.size(); }

public slots:
    void selectNode(int nodeId, bool multiSelect = false);
    void selectBeam(int beamId, bool multiSelect = false);
    void selectColumn(int columnId, bool multiSelect = false);
    void selectSlab(int slabId, bool multiSelect = false);
    void selectObject(const Handle(AIS_InteractiveObject)& obj, bool multiSelect = false);
    void setMultipleObjectsSelected(const std::vector<Handle(AIS_InteractiveObject)>& objects, bool multiSelect = false);
    void clearSelection();

signals:
    void selectionChanged();
    void nodeSelected(int nodeId);
    void beamSelected(int beamId);
    void columnSelected(int columnId);
    void slabSelected(int slabId);
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

    SelectionType m_selectionType = SelectionType::None;
    int m_primaryId = -1;
    std::set<int> m_selectedNodes;
    std::set<int> m_selectedBeams;
    std::set<int> m_selectedColumns;
    std::set<int> m_selectedSlabs;
};

} // namespace TSA::Viewer

