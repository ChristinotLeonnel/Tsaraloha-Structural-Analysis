#include "SelectionManager.h"

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
}

int SelectionManager::getNodeId(const Handle(AIS_InteractiveObject)& obj) const
{
    auto it = m_objToNode.find(obj);
    return (it != m_objToNode.end()) ? it->second : -1;
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

Handle(AIS_InteractiveObject) SelectionManager::getNodeObject(int nodeId) const
{
    auto it = m_nodeToObj.find(nodeId);
    return (it != m_nodeToObj.end()) ? it->second : Handle(AIS_InteractiveObject)();
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

void SelectionManager::selectObject(const Handle(AIS_InteractiveObject)& obj, bool multiSelect)
{
    if (obj.IsNull())
    {
        clearSelection();
        return;
    }

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

    int nodeId = getNodeId(obj);
    if (nodeId > 0) { selectNode(nodeId, multiSelect); return; }

    clearSelection();
}

void SelectionManager::setMultipleObjectsSelected(const std::vector<Handle(AIS_InteractiveObject)>& objects, bool multiSelect)
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
        m_primaryId = -1;
        m_selectionType = SelectionType::None;
    }

    for (const auto& obj : objects)
    {
        if (obj.IsNull()) continue;

        int beamId = getBeamId(obj);
        if (beamId > 0)
        {
            m_selectedBeams.insert(beamId);
            if (m_primaryId < 0) { m_primaryId = beamId; m_selectionType = SelectionType::Beam; }
            continue;
        }

        int colId = getColumnId(obj);
        if (colId > 0)
        {
            m_selectedColumns.insert(colId);
            if (m_primaryId < 0) { m_primaryId = colId; m_selectionType = SelectionType::Column; }
            continue;
        }

        int slabId = getSlabId(obj);
        if (slabId > 0)
        {
            m_selectedSlabs.insert(slabId);
            if (m_primaryId < 0) { m_primaryId = slabId; m_selectionType = SelectionType::Slab; }
            continue;
        }

        int wallId = getWallId(obj);
        if (wallId > 0)
        {
            m_selectedWalls.insert(wallId);
            if (m_primaryId < 0) { m_primaryId = wallId; m_selectionType = SelectionType::Wall; }
            continue;
        }

        int fId = getFoundationId(obj);
        if (fId > 0)
        {
            m_selectedFoundations.insert(fId);
            if (m_primaryId < 0) { m_primaryId = fId; m_selectionType = SelectionType::Foundation; }
            continue;
        }

        int trId = getTrussMemberId(obj);
        if (trId > 0)
        {
            m_selectedTrussMembers.insert(trId);
            if (m_primaryId < 0) { m_primaryId = trId; m_selectionType = SelectionType::TrussMember; }
            continue;
        }

        int cableId = getCableId(obj);
        if (cableId > 0)
        {
            m_selectedCables.insert(cableId);
            if (m_primaryId < 0) { m_primaryId = cableId; m_selectionType = SelectionType::Cable; }
            continue;
        }

        int nodeId = getNodeId(obj);
        if (nodeId > 0)
        {
            m_selectedNodes.insert(nodeId);
            if (m_primaryId < 0) { m_primaryId = nodeId; m_selectionType = SelectionType::Node; }
            continue;
        }
    }

    if (totalSelectedCount() == 0)
    {
        m_selectionType = SelectionType::None;
        m_primaryId = -1;
        emit selectionCleared();
    }
    else if (totalSelectedCount() == 1)
    {
        if (!m_selectedNodes.empty()) emit nodeSelected(*m_selectedNodes.begin());
        else if (!m_selectedBeams.empty()) emit beamSelected(*m_selectedBeams.begin());
        else if (!m_selectedColumns.empty()) emit columnSelected(*m_selectedColumns.begin());
        else if (!m_selectedSlabs.empty()) emit slabSelected(*m_selectedSlabs.begin());
        else if (!m_selectedWalls.empty()) emit wallSelected(*m_selectedWalls.begin());
        else if (!m_selectedFoundations.empty()) emit foundationSelected(*m_selectedFoundations.begin());
        else if (!m_selectedTrussMembers.empty()) emit trussMemberSelected(*m_selectedTrussMembers.begin());
        else if (!m_selectedCables.empty()) emit cableSelected(*m_selectedCables.begin());
    }

    emit selectionChanged();
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
    m_primaryId = -1;
    m_selectionType = SelectionType::None;

    emit selectionCleared();
    emit selectionChanged();
}

} // namespace TSA::Viewer
