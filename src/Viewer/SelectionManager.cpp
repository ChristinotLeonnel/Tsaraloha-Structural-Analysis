#include "SelectionManager.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"

#include <QTimer>

namespace TSA::Viewer
{

SelectionManager::SelectionManager(QObject* parent)
    : QObject(parent)
{
}

void SelectionManager::registerNode(int nodeId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_nodeToObj[nodeId] = obj;
    m_objToNode[obj] = nodeId;
}

void SelectionManager::registerSupport(int nodeId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_supportToObj[nodeId] = obj;
    m_objToSupport[obj] = nodeId;
}

void SelectionManager::registerBeam(int beamId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_beamToObj[beamId] = obj;
    m_objToBeam[obj] = beamId;
}

void SelectionManager::registerColumn(int columnId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_columnToObj[columnId] = obj;
    m_objToColumn[obj] = columnId;
}

void SelectionManager::registerSlab(int slabId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_slabToObj[slabId] = obj;
    m_objToSlab[obj] = slabId;
}

void SelectionManager::registerWall(int wallId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_wallToObj[wallId] = obj;
    m_objToWall[obj] = wallId;
}

void SelectionManager::registerFoundation(int foundationId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_foundationToObj[foundationId] = obj;
    m_objToFoundation[obj] = foundationId;
}

void SelectionManager::registerTrussMember(int memberId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_trussToObj[memberId] = obj;
    m_objToTruss[obj] = memberId;
}

void SelectionManager::registerCable(int cableId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_cableToObj[cableId] = obj;
    m_objToCable[obj] = cableId;
}

void SelectionManager::registerWorkPlane(int workPlaneId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_workPlaneToObj[workPlaneId] = obj;
    m_objToWorkPlane[obj] = workPlaneId;
}

void SelectionManager::registerNodalLoad(int loadId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_nodalLoadToObjs[loadId].push_back(obj);
    m_objToNodalLoad[obj] = loadId;
}

void SelectionManager::registerMemberLoad(int loadId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull()) return;
    m_memberLoadToObjs[loadId].push_back(obj);
    m_objToMemberLoad[obj] = loadId;
}

void SelectionManager::unregisterWorkPlane(int workPlaneId)
{
    auto it = m_workPlaneToObj.find(workPlaneId);
    if (it != m_workPlaneToObj.end())
    {
        m_objToWorkPlane.erase(it->second);
        m_workPlaneToObj.erase(it);
    }
    if (m_primaryId == workPlaneId && m_selectionType == SelectionType::WorkPlane)
    {
        clearSelection();
    }
}

void SelectionManager::unregisterNodalLoad(int loadId)
{
    auto it = m_nodalLoadToObjs.find(loadId);
    if (it != m_nodalLoadToObjs.end())
    {
        for (const auto& obj : it->second)
        {
            m_objToNodalLoad.erase(obj);
        }
        m_nodalLoadToObjs.erase(it);
    }
    m_selectedNodalLoads.erase(loadId);
}

void SelectionManager::unregisterMemberLoad(int loadId)
{
    auto it = m_memberLoadToObjs.find(loadId);
    if (it != m_memberLoadToObjs.end())
    {
        for (const auto& obj : it->second)
        {
            m_objToMemberLoad.erase(obj);
        }
        m_memberLoadToObjs.erase(it);
    }
    m_selectedMemberLoads.erase(loadId);
}

void SelectionManager::unregisterNode(int nodeId)
{
    auto it = m_nodeToObj.find(nodeId);
    if (it != m_nodeToObj.end())
    {
        m_objToNode.erase(it->second);
        m_nodeToObj.erase(it);
    }
    m_selectedNodes.erase(nodeId);
}

void SelectionManager::unregisterSupport(int nodeId)
{
    auto it = m_supportToObj.find(nodeId);
    if (it != m_supportToObj.end())
    {
        m_objToSupport.erase(it->second);
        m_supportToObj.erase(it);
    }
}

void SelectionManager::unregisterBeam(int beamId)
{
    auto it = m_beamToObj.find(beamId);
    if (it != m_beamToObj.end())
    {
        m_objToBeam.erase(it->second);
        m_beamToObj.erase(it);
    }
    m_selectedBeams.erase(beamId);
}

void SelectionManager::unregisterColumn(int columnId)
{
    auto it = m_columnToObj.find(columnId);
    if (it != m_columnToObj.end())
    {
        m_objToColumn.erase(it->second);
        m_columnToObj.erase(it);
    }
    m_selectedColumns.erase(columnId);
}

void SelectionManager::unregisterSlab(int slabId)
{
    auto it = m_slabToObj.find(slabId);
    if (it != m_slabToObj.end())
    {
        m_objToSlab.erase(it->second);
        m_slabToObj.erase(it);
    }
    m_selectedSlabs.erase(slabId);
}

void SelectionManager::unregisterWall(int wallId)
{
    auto it = m_wallToObj.find(wallId);
    if (it != m_wallToObj.end())
    {
        m_objToWall.erase(it->second);
        m_wallToObj.erase(it);
    }
    m_selectedWalls.erase(wallId);
}

void SelectionManager::unregisterFoundation(int foundationId)
{
    auto it = m_foundationToObj.find(foundationId);
    if (it != m_foundationToObj.end())
    {
        m_objToFoundation.erase(it->second);
        m_foundationToObj.erase(it);
    }
    m_selectedFoundations.erase(foundationId);
}

void SelectionManager::unregisterTrussMember(int memberId)
{
    auto it = m_trussToObj.find(memberId);
    if (it != m_trussToObj.end())
    {
        m_objToTruss.erase(it->second);
        m_trussToObj.erase(it);
    }
    m_selectedTrussMembers.erase(memberId);
}

void SelectionManager::unregisterCable(int cableId)
{
    auto it = m_cableToObj.find(cableId);
    if (it != m_cableToObj.end())
    {
        m_objToCable.erase(it->second);
        m_cableToObj.erase(it);
    }
    m_selectedCables.erase(cableId);
}

void SelectionManager::clearRegistry()
{
    m_nodeToObj.clear();
    m_objToNode.clear();
    m_supportToObj.clear();
    m_objToSupport.clear();
    m_beamToObj.clear();
    m_objToBeam.clear();
    m_columnToObj.clear();
    m_objToColumn.clear();
    m_slabToObj.clear();
    m_objToSlab.clear();
    m_wallToObj.clear();
    m_objToWall.clear();
    m_foundationToObj.clear();
    m_objToFoundation.clear();
    m_trussToObj.clear();
    m_objToTruss.clear();
    m_cableToObj.clear();
    m_objToCable.clear();
    m_workPlaneToObj.clear();
    m_objToWorkPlane.clear();
    m_nodalLoadToObjs.clear();
    m_objToNodalLoad.clear();
    m_memberLoadToObjs.clear();
    m_objToMemberLoad.clear();
}

int SelectionManager::getNodeId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToNode.find(obj);
    if (it != m_objToNode.end()) return it->second;
    auto itSupp = m_objToSupport.find(obj);
    if (itSupp != m_objToSupport.end()) return itSupp->second;
    return -1;
}

int SelectionManager::getSupportNodeId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto itSupp = m_objToSupport.find(obj);
    return (itSupp != m_objToSupport.end()) ? itSupp->second : -1;
}

int SelectionManager::getNodalLoadId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToNodalLoad.find(obj);
    return (it != m_objToNodalLoad.end()) ? it->second : -1;
}

int SelectionManager::getMemberLoadId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToMemberLoad.find(obj);
    return (it != m_objToMemberLoad.end()) ? it->second : -1;
}

int SelectionManager::getBeamId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToBeam.find(obj);
    return (it != m_objToBeam.end()) ? it->second : -1;
}

int SelectionManager::getColumnId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToColumn.find(obj);
    return (it != m_objToColumn.end()) ? it->second : -1;
}

int SelectionManager::getSlabId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToSlab.find(obj);
    return (it != m_objToSlab.end()) ? it->second : -1;
}

int SelectionManager::getWallId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToWall.find(obj);
    return (it != m_objToWall.end()) ? it->second : -1;
}

int SelectionManager::getFoundationId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToFoundation.find(obj);
    return (it != m_objToFoundation.end()) ? it->second : -1;
}

int SelectionManager::getTrussMemberId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToTruss.find(obj);
    return (it != m_objToTruss.end()) ? it->second : -1;
}

int SelectionManager::getCableId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToCable.find(obj);
    return (it != m_objToCable.end()) ? it->second : -1;
}

int SelectionManager::getWorkPlaneId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToWorkPlane.find(obj);
    return (it != m_objToWorkPlane.end()) ? it->second : -1;
}

Handle(AIS_InteractiveObject) SelectionManager::getNodeObject(int nodeId) const
{
    auto it = m_nodeToObj.find(nodeId);
    return (it != m_nodeToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getSupportObject(int nodeId) const
{
    auto it = m_supportToObj.find(nodeId);
    return (it != m_supportToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getBeamObject(int beamId) const
{
    auto it = m_beamToObj.find(beamId);
    return (it != m_beamToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getColumnObject(int columnId) const
{
    auto it = m_columnToObj.find(columnId);
    return (it != m_columnToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getSlabObject(int slabId) const
{
    auto it = m_slabToObj.find(slabId);
    return (it != m_slabToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getWallObject(int wallId) const
{
    auto it = m_wallToObj.find(wallId);
    return (it != m_wallToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getFoundationObject(int foundationId) const
{
    auto it = m_foundationToObj.find(foundationId);
    return (it != m_foundationToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getTrussMemberObject(int memberId) const
{
    auto it = m_trussToObj.find(memberId);
    return (it != m_trussToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getCableObject(int cableId) const
{
    auto it = m_cableToObj.find(cableId);
    return (it != m_cableToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getWorkPlaneObject(int workPlaneId) const
{
    auto it = m_workPlaneToObj.find(workPlaneId);
    return (it != m_workPlaneToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getNodalLoadObject(int loadId) const
{
    auto it = m_nodalLoadToObjs.find(loadId);
    if (it != m_nodalLoadToObjs.end() && !it->second.empty())
    {
        return it->second.front();
    }
    return Handle(AIS_InteractiveObject)();
}

Handle(AIS_InteractiveObject) SelectionManager::getMemberLoadObject(int loadId) const
{
    auto it = m_memberLoadToObjs.find(loadId);
    if (it != m_memberLoadToObjs.end() && !it->second.empty())
    {
        return it->second.front();
    }
    return Handle(AIS_InteractiveObject)();
}

void SelectionManager::selectNode(int nodeId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedNodes.insert(nodeId);
    m_selectionType = SelectionType::Node;
    m_primaryId = nodeId;

    emit nodeSelected(nodeId);
    emit selectionChanged();
}

void SelectionManager::selectBeam(int beamId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedBeams.insert(beamId);
    m_selectionType = SelectionType::Beam;
    m_primaryId = beamId;

    emit beamSelected(beamId);
    emit selectionChanged();
}

void SelectionManager::selectColumn(int columnId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedColumns.insert(columnId);
    m_selectionType = SelectionType::Column;
    m_primaryId = columnId;

    emit columnSelected(columnId);
    emit selectionChanged();
}

void SelectionManager::selectSlab(int slabId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedSlabs.insert(slabId);
    m_selectionType = SelectionType::Slab;
    m_primaryId = slabId;

    emit slabSelected(slabId);
    emit selectionChanged();
}

void SelectionManager::selectWall(int wallId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedWalls.insert(wallId);
    m_selectionType = SelectionType::Wall;
    m_primaryId = wallId;

    emit wallSelected(wallId);
    emit selectionChanged();
}

void SelectionManager::selectFoundation(int foundationId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedFoundations.insert(foundationId);
    m_selectionType = SelectionType::Foundation;
    m_primaryId = foundationId;

    emit foundationSelected(foundationId);
    emit selectionChanged();
}

void SelectionManager::selectTrussMember(int memberId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedTrussMembers.insert(memberId);
    m_selectionType = SelectionType::TrussMember;
    m_primaryId = memberId;

    emit trussMemberSelected(memberId);
    emit selectionChanged();
}

void SelectionManager::selectCable(int cableId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectedCables.insert(cableId);
    m_selectionType = SelectionType::Cable;
    m_primaryId = cableId;

    emit cableSelected(cableId);
    emit selectionChanged();
}

void SelectionManager::selectWorkPlane(int workPlaneId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
    }
    m_selectionType = SelectionType::WorkPlane;
    m_primaryId = workPlaneId;

    emit workPlaneSelected(workPlaneId);
    emit selectionChanged();
}

void SelectionManager::selectNodalLoad(int loadId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
        m_selectedNodalLoads.clear();
        m_selectedMemberLoads.clear();
    }
    m_selectedNodalLoads.insert(loadId);
    m_selectionType = SelectionType::NodalLoad;
    m_primaryId = loadId;

    emit nodalLoadSelected(loadId);
    emit selectionChanged();
}

void SelectionManager::selectMemberLoad(int loadId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
        m_selectedWalls.clear();
        m_selectedFoundations.clear();
        m_selectedTrussMembers.clear();
        m_selectedCables.clear();
        m_selectedNodalLoads.clear();
        m_selectedMemberLoads.clear();
    }
    m_selectedMemberLoads.insert(loadId);
    m_selectionType = SelectionType::MemberLoad;
    m_primaryId = loadId;

    emit memberLoadSelected(loadId);
    emit selectionChanged();
}

void SelectionManager::selectObject(const Handle(AIS_InteractiveObject)& obj, bool multiSelect)
{
    if (obj.IsNull())
    {
        clearSelection();
        return;
    }

    int wpId = getWorkPlaneId(obj);
    if (wpId > 0) { selectWorkPlane(wpId, multiSelect); return; }

    int beamId = getBeamId(obj);
    if (beamId > 0) { selectBeam(beamId, multiSelect); return; }

    int colId = getColumnId(obj);
    if (colId > 0) { selectColumn(colId, multiSelect); return; }

    int slabId = getSlabId(obj);
    if (slabId > 0) { selectSlab(slabId, multiSelect); return; }

    int wallId = getWallId(obj);
    if (wallId > 0) { selectWall(wallId, multiSelect); return; }

    int fId = getFoundationId(obj);
    if (fId > 0) { selectFoundation(fId, multiSelect); return; }

    int trId = getTrussMemberId(obj);
    if (trId > 0) { selectTrussMember(trId, multiSelect); return; }

    int cableId = getCableId(obj);
    if (cableId > 0) { selectCable(cableId, multiSelect); return; }

    int nlId = getNodalLoadId(obj);
    if (nlId > 0) { selectNodalLoad(nlId, multiSelect); return; }

    int mlId = getMemberLoadId(obj);
    if (mlId > 0) { selectMemberLoad(mlId, multiSelect); return; }

    int nodeId = getNodeId(obj);
    if (nodeId > 0) { selectNode(nodeId, multiSelect); return; }

    clearSelection();
}

void SelectionManager::setMultipleObjectsSelected(const std::vector<Handle(AIS_InteractiveObject)>& objects, bool multiSelect)
{
    // Auparavant : charges sélectionnées jamais vidées et aucun signal d'ensemble au-delà d'un élément
    // (pas d'édition groupée après une sélection par rectangle).
    if (!multiSelect)
        clearElementSets();
    for (const auto& obj : objects)
    {
        const auto [type, id] = elementOf(obj);
        if (std::set<int>* ids = selectionSet(type))
        {
            ids->insert(id);
            if (m_primaryId < 0) { m_primaryId = id; m_selectionType = type; }
        }
    }
    choosePrimaryIfNeeded();
    notifySelectionSetChanged();
}

void SelectionManager::clearSelection()
{
    if (m_selectionType == SelectionType::None && totalSelectedCount() == 0)
    {
        return;
    }

    m_selectedNodes.clear();
    m_selectedBeams.clear();
    m_selectedColumns.clear();
    m_selectedSlabs.clear();
    m_selectedWalls.clear();
    m_selectedFoundations.clear();
    m_selectedTrussMembers.clear();
    m_selectedCables.clear();
    m_selectedNodalLoads.clear();
    m_selectedMemberLoads.clear();
    m_primaryId = -1;
    m_selectionType = SelectionType::None;

    emit selectionCleared();
    emit selectionChanged();
}

TSA::Model::ElementSet SelectionManager::selectedElements() const
{
    TSA::Model::ElementSet e;
    e.nodes = m_selectedNodes;
    e.beams = m_selectedBeams;
    e.columns = m_selectedColumns;
    e.slabs = m_selectedSlabs;
    e.walls = m_selectedWalls;
    e.foundations = m_selectedFoundations;
    e.trussMembers = m_selectedTrussMembers;
    e.cables = m_selectedCables;
    return e;
}

void SelectionManager::selectElements(const TSA::Model::ElementSet& elements, bool addToSelection)
{
    if (!addToSelection)
        clearElementSets();

    m_selectedNodes.insert(elements.nodes.begin(), elements.nodes.end());
    m_selectedBeams.insert(elements.beams.begin(), elements.beams.end());
    m_selectedColumns.insert(elements.columns.begin(), elements.columns.end());
    m_selectedSlabs.insert(elements.slabs.begin(), elements.slabs.end());
    m_selectedWalls.insert(elements.walls.begin(), elements.walls.end());
    m_selectedFoundations.insert(elements.foundations.begin(), elements.foundations.end());
    m_selectedTrussMembers.insert(elements.trussMembers.begin(), elements.trussMembers.end());
    m_selectedCables.insert(elements.cables.begin(), elements.cables.end());

    choosePrimaryIfNeeded();
    notifySelectionSetChanged();
}

bool SelectionManager::toggleObject(const Handle(AIS_InteractiveObject)& obj)
{
    const auto [type, id] = elementOf(obj);
    if (!selectionSet(type)) return false;
    toggleElement(type, id);
    return true;
}

void SelectionManager::restoreSelected(SelectionType type, int id)
{
    if (std::set<int>* ids = selectionSet(type); ids && id > 0) ids->insert(id);
}

void SelectionManager::toggleElement(SelectionType type, int id)
{
    std::set<int>* ids = selectionSet(type);
    if (!ids || id <= 0) return;
    if (ids->erase(id) > 0)
    {
        if (m_selectionType == type && m_primaryId == id)
        {
            m_primaryId = -1;
            m_selectionType = SelectionType::None;
        }
    }
    else
    {
        ids->insert(id);
        m_primaryId = id;   // le dernier élément ajouté devient l'élément principal (propriétés)
        m_selectionType = type;
    }
    choosePrimaryIfNeeded();
    notifySelectionSetChanged();
}

bool SelectionManager::pruneMissing(const TSA::Model::Model& model)
{
    bool changed = false;
    auto prune = [&](std::set<int>& ids, auto exists) {
        for (auto it = ids.begin(); it != ids.end();)
        {
            if (exists(*it)) { ++it; continue; }
            if (m_primaryId == *it) m_primaryId = -1;
            it = ids.erase(it);
            changed = true;
        }
    };
    prune(m_selectedNodes, [&](int id) { return model.getNode(id) != nullptr; });
    prune(m_selectedBeams, [&](int id) { return model.getBeam(id) != nullptr; });
    prune(m_selectedColumns, [&](int id) { return model.getColumn(id) != nullptr; });
    prune(m_selectedSlabs, [&](int id) { return model.getSlab(id) != nullptr; });
    prune(m_selectedWalls, [&](int id) { return model.getWall(id) != nullptr; });
    prune(m_selectedFoundations, [&](int id) { return model.getFoundation(id) != nullptr; });
    prune(m_selectedTrussMembers, [&](int id) { return model.getTrussMember(id) != nullptr; });
    prune(m_selectedCables, [&](int id) { return model.getCable(id) != nullptr; });
    prune(m_selectedNodalLoads, [&](int id) { return model.loadManager().getNodalLoad(id) != nullptr; });
    prune(m_selectedMemberLoads, [&](int id) { return model.loadManager().getMemberLoad(id) != nullptr; });
    if (!changed) return false;
    if (m_primaryId < 0) m_selectionType = SelectionType::None;
    choosePrimaryIfNeeded();
    notifySelectionSetChanged();
    return true;
}

void SelectionManager::schedulePrune(const TSA::Model::Model* model)
{
    if (!model || m_prunePending || totalSelectedCount() == 0) return;
    m_prunePending = true;
    QTimer::singleShot(0, this, [this, model] {
        m_prunePending = false;
        pruneMissing(*model);
    });
}

std::set<int>* SelectionManager::selectionSet(SelectionType type)
{
    switch (type)
    {
    case SelectionType::Node: return &m_selectedNodes;
    case SelectionType::Beam: return &m_selectedBeams;
    case SelectionType::Column: return &m_selectedColumns;
    case SelectionType::Slab: return &m_selectedSlabs;
    case SelectionType::Wall: return &m_selectedWalls;
    case SelectionType::Foundation: return &m_selectedFoundations;
    case SelectionType::TrussMember: return &m_selectedTrussMembers;
    case SelectionType::Cable: return &m_selectedCables;
    case SelectionType::NodalLoad: return &m_selectedNodalLoads;
    case SelectionType::MemberLoad: return &m_selectedMemberLoads;
    default: return nullptr;
    }
}

std::pair<SelectionType, int> SelectionManager::elementOf(const Handle(AIS_InteractiveObject)& obj) const
{
    if (obj.IsNull()) return { SelectionType::None, -1 };
    // Même ordre de recherche que selectObject.
    if (int id = getBeamId(obj); id > 0) return { SelectionType::Beam, id };
    if (int id = getColumnId(obj); id > 0) return { SelectionType::Column, id };
    if (int id = getSlabId(obj); id > 0) return { SelectionType::Slab, id };
    if (int id = getWallId(obj); id > 0) return { SelectionType::Wall, id };
    if (int id = getFoundationId(obj); id > 0) return { SelectionType::Foundation, id };
    if (int id = getTrussMemberId(obj); id > 0) return { SelectionType::TrussMember, id };
    if (int id = getCableId(obj); id > 0) return { SelectionType::Cable, id };
    if (int id = getNodalLoadId(obj); id > 0) return { SelectionType::NodalLoad, id };
    if (int id = getMemberLoadId(obj); id > 0) return { SelectionType::MemberLoad, id };
    if (int id = getNodeId(obj); id > 0) return { SelectionType::Node, id };
    return { SelectionType::None, -1 };
}

void SelectionManager::clearElementSets()
{
    m_selectedNodes.clear();
    m_selectedBeams.clear();
    m_selectedColumns.clear();
    m_selectedSlabs.clear();
    m_selectedWalls.clear();
    m_selectedFoundations.clear();
    m_selectedTrussMembers.clear();
    m_selectedCables.clear();
    m_selectedNodalLoads.clear();
    m_selectedMemberLoads.clear();
    m_primaryId = -1;
    m_selectionType = SelectionType::None;
}

void SelectionManager::choosePrimaryIfNeeded()
{
    if (m_primaryId >= 0) return;
    auto pick = [this](const std::set<int>& ids, SelectionType type) {
        if (m_primaryId < 0 && !ids.empty())
        {
            m_primaryId = *ids.begin();
            m_selectionType = type;
        }
    };
    pick(m_selectedBeams, SelectionType::Beam);
    pick(m_selectedColumns, SelectionType::Column);
    pick(m_selectedSlabs, SelectionType::Slab);
    pick(m_selectedWalls, SelectionType::Wall);
    pick(m_selectedFoundations, SelectionType::Foundation);
    pick(m_selectedTrussMembers, SelectionType::TrussMember);
    pick(m_selectedCables, SelectionType::Cable);
    pick(m_selectedNodes, SelectionType::Node);
    pick(m_selectedNodalLoads, SelectionType::NodalLoad);
    pick(m_selectedMemberLoads, SelectionType::MemberLoad);
}

void SelectionManager::notifySelectionSetChanged()
{
    const size_t total = totalSelectedCount();
    if (total == 0)
    {
        m_selectionType = SelectionType::None;
        m_primaryId = -1;
        emit selectionCleared();
    }
    else if (total == 1)
    {
        // Comportement identique à un clic simple (propriétés, arbre, surbrillance).
        switch (m_selectionType)
        {
        case SelectionType::Node: emit nodeSelected(m_primaryId); break;
        case SelectionType::Beam: emit beamSelected(m_primaryId); break;
        case SelectionType::Column: emit columnSelected(m_primaryId); break;
        case SelectionType::Slab: emit slabSelected(m_primaryId); break;
        case SelectionType::Wall: emit wallSelected(m_primaryId); break;
        case SelectionType::Foundation: emit foundationSelected(m_primaryId); break;
        case SelectionType::TrussMember: emit trussMemberSelected(m_primaryId); break;
        case SelectionType::Cable: emit cableSelected(m_primaryId); break;
        case SelectionType::NodalLoad: emit nodalLoadSelected(m_primaryId); break;
        case SelectionType::MemberLoad: emit memberLoadSelected(m_primaryId); break;
        default: break;
        }
    }
    else
    {
        emit multipleSelectionChanged();
    }
    emit selectionChanged();
}

} // namespace TSA::Viewer
