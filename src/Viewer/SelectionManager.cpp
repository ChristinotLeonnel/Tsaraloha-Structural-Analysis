#include "SelectionManager.h"

namespace TSA::Viewer
{

SelectionManager::SelectionManager(QObject* parent)
    : QObject(parent)
{
}

void SelectionManager::registerNode(int nodeId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull())
        return;

    m_nodeToObj[nodeId] = obj;
    m_objToNode[obj] = nodeId;
}

void SelectionManager::registerBeam(int beamId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull())
        return;

    m_beamToObj[beamId] = obj;
    m_objToBeam[obj] = beamId;
}

void SelectionManager::registerColumn(int columnId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull())
        return;

    m_columnToObj[columnId] = obj;
    m_objToColumn[obj] = columnId;
}

void SelectionManager::registerSlab(int slabId, const Handle(AIS_InteractiveObject)& obj)
{
    if (obj.IsNull())
        return;

    m_slabToObj[slabId] = obj;
    m_objToSlab[obj] = slabId;
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
    clearSelection();
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

void SelectionManager::selectNode(int nodeId, bool multiSelect)
{
    if (!multiSelect)
    {
        m_selectedNodes.clear();
        m_selectedBeams.clear();
        m_selectedColumns.clear();
        m_selectedSlabs.clear();
    }

    m_selectedNodes.insert(nodeId);
    m_primaryId = nodeId;
    m_selectionType = SelectionType::Node;

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
    }

    m_selectedBeams.insert(beamId);
    m_primaryId = beamId;
    m_selectionType = SelectionType::Beam;

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
    }

    m_selectedColumns.insert(columnId);
    m_primaryId = columnId;
    m_selectionType = SelectionType::Column;

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
    }

    m_selectedSlabs.insert(slabId);
    m_primaryId = slabId;
    m_selectionType = SelectionType::Slab;

    emit slabSelected(slabId);
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
    if (beamId > 0)
    {
        selectBeam(beamId, multiSelect);
        return;
    }

    int colId = getColumnId(obj);
    if (colId > 0)
    {
        selectColumn(colId, multiSelect);
        return;
    }

    int slabId = getSlabId(obj);
    if (slabId > 0)
    {
        selectSlab(slabId, multiSelect);
        return;
    }

    int nodeId = getNodeId(obj);
    if (nodeId > 0)
    {
        selectNode(nodeId, multiSelect);
        return;
    }

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
        m_primaryId = -1;
        m_selectionType = SelectionType::None;
    }

    for (const auto& obj : objects)
    {
        if (obj.IsNull())
            continue;

        int beamId = getBeamId(obj);
        if (beamId > 0)
        {
            m_selectedBeams.insert(beamId);
            if (m_primaryId < 0)
            {
                m_primaryId = beamId;
                m_selectionType = SelectionType::Beam;
            }
            continue;
        }

        int colId = getColumnId(obj);
        if (colId > 0)
        {
            m_selectedColumns.insert(colId);
            if (m_primaryId < 0)
            {
                m_primaryId = colId;
                m_selectionType = SelectionType::Column;
            }
            continue;
        }

        int slabId = getSlabId(obj);
        if (slabId > 0)
        {
            m_selectedSlabs.insert(slabId);
            if (m_primaryId < 0)
            {
                m_primaryId = slabId;
                m_selectionType = SelectionType::Slab;
            }
            continue;
        }

        int nodeId = getNodeId(obj);
        if (nodeId > 0)
        {
            m_selectedNodes.insert(nodeId);
            if (m_primaryId < 0)
            {
                m_primaryId = nodeId;
                m_selectionType = SelectionType::Node;
            }
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
    }

    emit selectionChanged();
}

void SelectionManager::clearSelection()
{
    if (m_selectionType == SelectionType::None &&
        m_selectedNodes.empty() &&
        m_selectedBeams.empty() &&
        m_selectedColumns.empty() &&
        m_selectedSlabs.empty())
    {
        return;
    }

    m_selectedNodes.clear();
    m_selectedBeams.clear();
    m_selectedColumns.clear();
    m_selectedSlabs.clear();
    m_primaryId = -1;
    m_selectionType = SelectionType::None;

    emit selectionCleared();
    emit selectionChanged();
}

} // namespace TSA::Viewer
