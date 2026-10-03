#include "Model.h"
#include "ModelDiff.h"
#include "../UndoRedo/UndoManager.h"
#include "../Diagnostics/Logger.h"
#include <algorithm>
#include <cmath>

namespace TSA::Model
{

Model::Model()
    : m_coordinateSystem(std::make_shared<TSA::Coordinate::CoordinateSystem>())
    , m_undoManager(std::make_unique<TSA::UndoRedo::UndoManager>())
    , m_workPlaneManager(std::make_unique<TSA::Coordinate::WorkPlaneManager>())
{
    m_coordinateSystem->setDefaultBuildingCoordinates();

    if (auto* lm = m_coordinateSystem->levelManager())
    {
        QObject::connect(lm, &TSA::Coordinate::LevelManager::levelElevationChanged, [this](const std::string& lvlId, double oldZ, double newZ) {
            onLevelElevationChanged(lvlId, oldZ, newZ);
        });
    }
}

Model::~Model()
{
    const auto observers = m_observers; // copie : un observateur peut modifier la liste
    for (auto* obs : observers)
    {
        obs->onModelDestroyed();
    }
}

void Model::bumpRevision()
{
    ++m_revision;
    for (auto* obs : m_observers)
    {
        obs->onModelEdited();
    }
}

TSA::Coordinate::LevelManager* Model::levelManager()
{
    return m_coordinateSystem ? m_coordinateSystem->levelManager() : nullptr;
}

const TSA::Coordinate::LevelManager* Model::levelManager() const
{
    return m_coordinateSystem ? m_coordinateSystem->levelManager() : nullptr;
}

std::vector<TSA::Coordinate::DetectedPlaneInfo> Model::detectStructuralPlanes(TSA::Coordinate::WorkPlaneAxis axis) const
{
    if (m_coordinateSystem)
        return m_coordinateSystem->detectStructuralPlanes(axis, this);
    return {};
}

void Model::addObserver(IModelObserver* observer)
{
    if (observer && std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end())
    {
        m_observers.push_back(observer);
    }
}

void Model::removeObserver(IModelObserver* observer)
{
    m_observers.erase(std::remove(m_observers.begin(), m_observers.end(), observer), m_observers.end());
}

int Model::addNode(double x, double y, double z, const std::string& levelId, const std::string& name)
{
    int id = m_nextNodeId++;
    while (m_nodes.find(id) != m_nodes.end())
    {
        id = m_nextNodeId++;
    }

    std::string actualLvlId = levelId;
    if (actualLvlId.empty() && levelManager())
    {
        const auto* lvl = levelManager()->findLevelAtElevation(z);
        if (lvl)
            actualLvlId = lvl->id;
    }

    Node node(id, x, y, z, actualLvlId, name);
    auto it = m_nodes.emplace(id, node).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodeAdded(it->second);
    }

    return id;
}

bool Model::addNodeWithId(int id, double x, double y, double z, const std::string& levelId, const std::string& name)
{
    if (m_nodes.find(id) != m_nodes.end())
    {
        return false;
    }

    std::string actualLvlId = levelId;
    if (actualLvlId.empty() && levelManager())
    {
        const auto* lvl = levelManager()->findLevelAtElevation(z);
        if (lvl)
            actualLvlId = lvl->id;
    }

    Node node(id, x, y, z, actualLvlId, name);
    auto it = m_nodes.emplace(id, node).first;
    if (id >= m_nextNodeId)
    {
        m_nextNodeId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodeAdded(it->second);
    }

    return true;
}

int Model::addNodeAtGridIntersection(int ix, int iy, int iz)
{
    if (!m_coordinateSystem)
        return -1;

    TSA::Coordinate::Point3D p = m_coordinateSystem->gridPoint(ix, iy, iz);
    std::string lvlId;
    if (auto* lm = levelManager())
    {
        const auto* lvl = lm->getLevelByIndex(iz);
        if (lvl)
            lvlId = lvl->id;
    }
    return addNode(p.x, p.y, p.z, lvlId);
}

int Model::addColumnBetweenLevels(int levelStartIndex, int levelEndIndex, double x, double y, double width, double height)
{
    auto* lm = levelManager();
    if (!lm)
        return -1;

    const auto* lStart = lm->getLevelByIndex(levelStartIndex);
    const auto* lEnd = lm->getLevelByIndex(levelEndIndex);
    if (!lStart || !lEnd)
        return -1;

    // Rechercher un nœud existant à la base ou le créer
    int nStart = -1;
    for (const auto& [nId, n] : m_nodes)
    {
        if (std::abs(n.x() - x) < 1e-3 && std::abs(n.y() - y) < 1e-3 && std::abs(n.z() - lStart->elevation) < 1e-3)
        {
            nStart = nId;
            break;
        }
    }
    if (nStart == -1)
    {
        nStart = addNode(x, y, lStart->elevation, lStart->id);
    }

    // Rechercher un nœud existant au sommet ou le créer
    int nEnd = -1;
    for (const auto& [nId, n] : m_nodes)
    {
        if (std::abs(n.x() - x) < 1e-3 && std::abs(n.y() - y) < 1e-3 && std::abs(n.z() - lEnd->elevation) < 1e-3)
        {
            nEnd = nId;
            break;
        }
    }
    if (nEnd == -1)
    {
        nEnd = addNode(x, y, lEnd->elevation, lEnd->id);
    }

    return addColumn(nStart, nEnd, width, height);
}

void Model::onLevelElevationChanged(const std::string& levelId, double oldElevation, double newElevation)
{
    std::set<int> modifiedNodeIds;

    for (auto& [id, node] : m_nodes)
    {
        if (node.levelId() == levelId || (node.levelId().empty() && std::abs(node.z() - oldElevation) < 1e-3))
        {
            node.setZ(newElevation);
            node.setLevelId(levelId);
            modifiedNodeIds.insert(id);
            notifyNodeModified(id);
        }
    }

    for (const auto& [bId, beam] : m_beams)
    {
        if (modifiedNodeIds.count(beam.startNodeId()) || modifiedNodeIds.count(beam.endNodeId()))
        {
            notifyBeamModified(bId);
        }
    }

    for (const auto& [cId, col] : m_columns)
    {
        if (modifiedNodeIds.count(col.startNodeId()) || modifiedNodeIds.count(col.endNodeId()))
        {
            notifyColumnModified(cId);
        }
    }

    for (const auto& [sId, slab] : m_slabs)
    {
        for (int nId : slab.nodeIds())
        {
            if (modifiedNodeIds.count(nId))
            {
                notifySlabModified(sId);
                break;
            }
        }
    }
}

bool Model::removeNode(int nodeId)
{
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end())
    {
        return false;
    }

    // Supprimer d'abord les poutres, poteaux et dalles connectés à ce nœud
    std::vector<int> connectedBeams;
    for (const auto& [beamId, beam] : m_beams)
    {
        if (beam.startNodeId() == nodeId || beam.endNodeId() == nodeId)
        {
            connectedBeams.push_back(beamId);
        }
    }
    for (int beamId : connectedBeams)
    {
        removeBeam(beamId);
    }

    std::vector<int> connectedColumns;
    for (const auto& [colId, col] : m_columns)
    {
        if (col.startNodeId() == nodeId || col.endNodeId() == nodeId)
        {
            connectedColumns.push_back(colId);
        }
    }
    for (int colId : connectedColumns)
    {
        removeColumn(colId);
    }

    std::vector<int> connectedSlabs;
    for (const auto& [slabId, slab] : m_slabs)
    {
        const auto& nids = slab.nodeIds();
        if (std::find(nids.begin(), nids.end(), nodeId) != nids.end())
        {
            connectedSlabs.push_back(slabId);
        }
    }
    for (int slabId : connectedSlabs)
    {
        removeSlab(slabId);
    }

    std::vector<int> connectedWalls;
    for (const auto& [wallId, wall] : m_walls)
    {
        if (wall.startNodeId() == nodeId || wall.endNodeId() == nodeId)
        {
            connectedWalls.push_back(wallId);
        }
    }
    for (int wallId : connectedWalls)
    {
        removeWall(wallId);
    }

    std::vector<int> connectedFoundations;
    for (const auto& [fId, f] : m_foundations)
    {
        if (f.nodeId() == nodeId)
        {
            connectedFoundations.push_back(fId);
        }
    }
    for (int fId : connectedFoundations)
    {
        removeFoundation(fId);
    }

    std::vector<int> connectedTruss;
    for (const auto& [trId, tr] : m_trussMembers)
    {
        if (tr.startNodeId() == nodeId || tr.endNodeId() == nodeId)
        {
            connectedTruss.push_back(trId);
        }
    }
    for (int trId : connectedTruss)
    {
        removeTrussMember(trId);
    }

    std::vector<int> connectedCables;
    for (const auto& [cId, c] : m_cables)
    {
        if (c.startNodeId() == nodeId || c.endNodeId() == nodeId)
        {
            connectedCables.push_back(cId);
        }
    }
    for (int cId : connectedCables)
    {
        removeCable(cId);
    }

    std::vector<int> removedNodalLoads = m_loadManager.removeNodalLoadsForNode(nodeId);
    for (int loadId : removedNodalLoads)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onNodalLoadRemoved(loadId);
            obs->onLoadRemoved(loadId);
        }
    }

    m_nodes.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodeRemoved(nodeId);
    }

    return true;
}

Node* Model::getNode(int nodeId)
{
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

const Node* Model::getNode(int nodeId) const
{
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

bool Model::isNodeFree(int nodeId) const
{
    if (m_nodes.find(nodeId) == m_nodes.end())
        return false;

    for (const auto& [id, beam] : m_beams)
    {
        if (beam.startNodeId() == nodeId || beam.endNodeId() == nodeId) return false;
    }
    for (const auto& [id, col] : m_columns)
    {
        if (col.startNodeId() == nodeId || col.endNodeId() == nodeId) return false;
    }
    for (const auto& [id, tm] : m_trussMembers)
    {
        if (tm.startNodeId() == nodeId || tm.endNodeId() == nodeId) return false;
    }
    for (const auto& [id, cb] : m_cables)
    {
        if (cb.startNodeId() == nodeId || cb.endNodeId() == nodeId) return false;
    }
    for (const auto& [id, wall] : m_walls)
    {
        if (wall.startNodeId() == nodeId || wall.endNodeId() == nodeId) return false;
    }
    for (const auto& [id, slab] : m_slabs)
    {
        for (int nid : slab.nodeIds())
        {
            if (nid == nodeId) return false;
        }
    }
    for (const auto& [id, f] : m_foundations)
    {
        if (f.nodeId() == nodeId) return false;
    }

    return true;
}

std::vector<int> Model::freeNodeIds() const
{
    std::set<int> connectedNodes;
    for (const auto& [id, b] : m_beams) { connectedNodes.insert(b.startNodeId()); connectedNodes.insert(b.endNodeId()); }
    for (const auto& [id, c] : m_columns) { connectedNodes.insert(c.startNodeId()); connectedNodes.insert(c.endNodeId()); }
    for (const auto& [id, tm] : m_trussMembers) { connectedNodes.insert(tm.startNodeId()); connectedNodes.insert(tm.endNodeId()); }
    for (const auto& [id, cb] : m_cables) { connectedNodes.insert(cb.startNodeId()); connectedNodes.insert(cb.endNodeId()); }
    for (const auto& [id, w] : m_walls) { connectedNodes.insert(w.startNodeId()); connectedNodes.insert(w.endNodeId()); }
    for (const auto& [id, s] : m_slabs) { for (int nid : s.nodeIds()) connectedNodes.insert(nid); }
    for (const auto& [id, f] : m_foundations) { connectedNodes.insert(f.nodeId()); }

    std::vector<int> freeIds;
    for (const auto& [nid, node] : m_nodes)
    {
        if (connectedNodes.find(nid) == connectedNodes.end())
        {
            freeIds.push_back(nid);
        }
    }
    return freeIds;
}

std::vector<int> Model::supportedNodeIds() const
{
    std::vector<int> suppIds;
    for (const auto& [nid, node] : m_nodes)
    {
        if (node.support().isSupported())
        {
            suppIds.push_back(nid);
        }
    }
    return suppIds;
}

int Model::addBeam(int startNodeId, int endNodeId, double width, double height, const std::string& name)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextBeamId++;
    while (m_beams.find(id) != m_beams.end())
    {
        id = m_nextBeamId++;
    }

    Beam beam(id, startNodeId, endNodeId, width, height, name);
    auto it = m_beams.emplace(id, beam).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onBeamAdded(it->second);
    }

    return id;
}

bool Model::addBeamWithId(int id, int startNodeId, int endNodeId, double width, double height, const std::string& name)
{
    if (m_beams.find(id) != m_beams.end() ||
        m_nodes.find(startNodeId) == m_nodes.end() ||
        m_nodes.find(endNodeId) == m_nodes.end())
    {
        return false;
    }

    Beam beam(id, startNodeId, endNodeId, width, height, name);
    auto it = m_beams.emplace(id, beam).first;
    if (id >= m_nextBeamId)
    {
        m_nextBeamId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onBeamAdded(it->second);
    }

    return true;
}

int Model::addBar(int startNodeId, int endNodeId, const Section& section, const Material& material, BarRole role, double rotation, const std::string& name)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextBeamId++;
    while (m_beams.find(id) != m_beams.end())
    {
        id = m_nextBeamId++;
    }

    Beam bar(id, startNodeId, endNodeId, section, material, role, rotation, name);
    auto it = m_beams.emplace(id, bar).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onBeamAdded(it->second);
    }

    return id;
}

int Model::addBar(const BarProperties& props, int startNodeId, int endNodeId)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = props.id > 0 && m_beams.find(props.id) == m_beams.end() ? props.id : m_nextBeamId++;
    while (m_beams.find(id) != m_beams.end())
    {
        id = m_nextBeamId++;
    }
    if (id >= m_nextBeamId)
    {
        m_nextBeamId = id + 1;
    }

    Beam bar(id, startNodeId, endNodeId, props.section, props.material, props.role, props.rotation, props.name);
    bar.setEccentricity(props.eccentricity);
    bar.setStartRelease(props.startRelease);
    bar.setEndRelease(props.endRelease);
    if (!props.color.empty()) bar.setColor(props.color);

    auto it = m_beams.emplace(id, bar).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onBeamAdded(it->second);
    }

    return id;
}

bool Model::removeBeam(int beamId)
{
    auto it = m_beams.find(beamId);
    if (it == m_beams.end())
    {
        return false;
    }

    std::vector<int> removedMemberLoads = m_loadManager.removeMemberLoadsForElement(beamId, MemberTargetType::Beam);
    for (int loadId : removedMemberLoads)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onMemberLoadRemoved(loadId);
            obs->onLoadRemoved(loadId);
        }
    }

    m_beams.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onBeamRemoved(beamId);
    }

    return true;
}

Beam* Model::getBeam(int beamId)
{
    auto it = m_beams.find(beamId);
    return (it != m_beams.end()) ? &it->second : nullptr;
}

const Beam* Model::getBeam(int beamId) const
{
    auto it = m_beams.find(beamId);
    return (it != m_beams.end()) ? &it->second : nullptr;
}

void Model::notifyNodeModified(int nodeId)
{
    const Node* node = getNode(nodeId);
    if (node)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onNodeModified(*node);
        }
    }
}

void Model::notifyBeamModified(int beamId)
{
    const Beam* beam = getBeam(beamId);
    if (beam)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onBeamModified(*beam);
        }
    }
}

void Model::notifyColumnModified(int columnId)
{
    const Column* col = getColumn(columnId);
    if (col)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onColumnModified(*col);
        }
    }
}

void Model::notifySlabModified(int slabId)
{
    const Slab* slab = getSlab(slabId);
    if (slab)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onSlabModified(*slab);
        }
    }
}

int Model::addColumn(int startNodeId, int endNodeId, double width, double height, const std::string& name)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextColumnId++;
    while (m_columns.find(id) != m_columns.end())
    {
        id = m_nextColumnId++;
    }

    Column col(id, startNodeId, endNodeId, width, height, name);
    auto it = m_columns.emplace(id, col).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onColumnAdded(it->second);
    }

    return id;
}

int Model::addColumn(int startNodeId, int endNodeId, const Section& section, const Material& material, double rotation, const std::string& name)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextColumnId++;
    while (m_columns.find(id) != m_columns.end())
    {
        id = m_nextColumnId++;
    }

    Column col(id, startNodeId, endNodeId, section, material, rotation, name);
    auto it = m_columns.emplace(id, col).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onColumnAdded(it->second);
    }

    return id;
}

bool Model::addColumnWithId(int id, int startNodeId, int endNodeId, double width, double height, const std::string& name)
{
    if (m_columns.find(id) != m_columns.end() ||
        m_nodes.find(startNodeId) == m_nodes.end() ||
        m_nodes.find(endNodeId) == m_nodes.end())
    {
        return false;
    }

    Column col(id, startNodeId, endNodeId, width, height, name);
    auto it = m_columns.emplace(id, col).first;
    if (id >= m_nextColumnId)
    {
        m_nextColumnId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onColumnAdded(it->second);
    }

    return true;
}

bool Model::removeColumn(int columnId)
{
    auto it = m_columns.find(columnId);
    if (it == m_columns.end())
    {
        return false;
    }

    std::vector<int> removedMemberLoads = m_loadManager.removeMemberLoadsForElement(columnId, MemberTargetType::Column);
    for (int loadId : removedMemberLoads)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onMemberLoadRemoved(loadId);
            obs->onLoadRemoved(loadId);
        }
    }

    m_columns.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onColumnRemoved(columnId);
    }

    return true;
}

Column* Model::getColumn(int columnId)
{
    auto it = m_columns.find(columnId);
    return (it != m_columns.end()) ? &it->second : nullptr;
}

const Column* Model::getColumn(int columnId) const
{
    auto it = m_columns.find(columnId);
    return (it != m_columns.end()) ? &it->second : nullptr;
}

int Model::addSlab(const std::vector<int>& nodeIds, double thickness, const std::string& name, SlabType type)
{
    if (nodeIds.size() < 3)
    {
        return -1;
    }

    for (int nid : nodeIds)
    {
        if (m_nodes.find(nid) == m_nodes.end())
        {
            return -1;
        }
    }

    int id = m_nextSlabId++;
    while (m_slabs.find(id) != m_slabs.end())
    {
        id = m_nextSlabId++;
    }

    Slab slab(id, nodeIds, thickness, name, type);
    auto it = m_slabs.emplace(id, slab).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onSlabAdded(it->second);
    }

    return id;
}

bool Model::addSlabWithId(int id, const std::vector<int>& nodeIds, double thickness, const std::string& name, SlabType type)
{
    if (m_slabs.find(id) != m_slabs.end() || nodeIds.size() < 3)
    {
        return false;
    }

    for (int nid : nodeIds)
    {
        if (m_nodes.find(nid) == m_nodes.end())
        {
            return false;
        }
    }

    Slab slab(id, nodeIds, thickness, name, type);
    auto it = m_slabs.emplace(id, slab).first;
    if (id >= m_nextSlabId)
    {
        m_nextSlabId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onSlabAdded(it->second);
    }

    return true;
}

bool Model::removeSlab(int slabId)
{
    auto it = m_slabs.find(slabId);
    if (it == m_slabs.end())
    {
        return false;
    }

    m_slabs.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onSlabRemoved(slabId);
    }

    return true;
}

Slab* Model::getSlab(int slabId)
{
    auto it = m_slabs.find(slabId);
    return (it != m_slabs.end()) ? &it->second : nullptr;
}

const Slab* Model::getSlab(int slabId) const
{
    auto it = m_slabs.find(slabId);
    return (it != m_slabs.end()) ? &it->second : nullptr;
}

void Model::notifyWallModified(int wallId)
{
    const Wall* w = getWall(wallId);
    if (w)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onWallModified(*w);
        }
    }
}

int Model::addWall(int startNodeId, int endNodeId, double height, double thickness, const std::string& name)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextWallId++;
    while (m_walls.find(id) != m_walls.end())
    {
        id = m_nextWallId++;
    }

    Wall wall(id, startNodeId, endNodeId, height, thickness, name);
    auto it = m_walls.emplace(id, wall).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onWallAdded(it->second);
    }

    return id;
}

bool Model::addWallWithId(int id, int startNodeId, int endNodeId, double height, double thickness, const std::string& name)
{
    if (m_walls.find(id) != m_walls.end() ||
        m_nodes.find(startNodeId) == m_nodes.end() ||
        m_nodes.find(endNodeId) == m_nodes.end())
    {
        return false;
    }

    Wall wall(id, startNodeId, endNodeId, height, thickness, name);
    auto it = m_walls.emplace(id, wall).first;
    if (id >= m_nextWallId)
    {
        m_nextWallId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onWallAdded(it->second);
    }

    return true;
}

bool Model::removeWall(int wallId)
{
    auto it = m_walls.find(wallId);
    if (it == m_walls.end())
    {
        return false;
    }

    m_walls.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onWallRemoved(wallId);
    }

    return true;
}

Wall* Model::getWall(int wallId)
{
    auto it = m_walls.find(wallId);
    return (it != m_walls.end()) ? &it->second : nullptr;
}

const Wall* Model::getWall(int wallId) const
{
    auto it = m_walls.find(wallId);
    return (it != m_walls.end()) ? &it->second : nullptr;
}

void Model::notifyFoundationModified(int foundationId)
{
    const Foundation* f = getFoundation(foundationId);
    if (f)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onFoundationModified(*f);
        }
    }
}

int Model::addFoundation(int nodeId, double widthA, double lengthB, double heightH, const std::string& name, FoundationType type)
{
    if (m_nodes.find(nodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextFoundationId++;
    while (m_foundations.find(id) != m_foundations.end())
    {
        id = m_nextFoundationId++;
    }

    Foundation f(id, nodeId, widthA, lengthB, heightH, name, type);
    auto it = m_foundations.emplace(id, f).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onFoundationAdded(it->second);
    }

    return id;
}

bool Model::addFoundationWithId(int id, int nodeId, double widthA, double lengthB, double heightH, const std::string& name, FoundationType type)
{
    if (m_foundations.find(id) != m_foundations.end() || m_nodes.find(nodeId) == m_nodes.end())
    {
        return false;
    }

    Foundation f(id, nodeId, widthA, lengthB, heightH, name, type);
    auto it = m_foundations.emplace(id, f).first;
    if (id >= m_nextFoundationId)
    {
        m_nextFoundationId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onFoundationAdded(it->second);
    }

    return true;
}

bool Model::removeFoundation(int foundationId)
{
    auto it = m_foundations.find(foundationId);
    if (it == m_foundations.end())
    {
        return false;
    }

    m_foundations.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onFoundationRemoved(foundationId);
    }

    return true;
}

Foundation* Model::getFoundation(int foundationId)
{
    auto it = m_foundations.find(foundationId);
    return (it != m_foundations.end()) ? &it->second : nullptr;
}

const Foundation* Model::getFoundation(int foundationId) const
{
    auto it = m_foundations.find(foundationId);
    return (it != m_foundations.end()) ? &it->second : nullptr;
}

void Model::notifyTrussMemberModified(int memberId)
{
    const TrussMember* tr = getTrussMember(memberId);
    if (tr)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onTrussMemberModified(*tr);
        }
    }
}

int Model::addTrussMember(int startNodeId, int endNodeId, double diameterOrWidth, const std::string& name, TrussMemberRole role)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextTrussMemberId++;
    while (m_trussMembers.find(id) != m_trussMembers.end())
    {
        id = m_nextTrussMemberId++;
    }

    TrussMember member(id, startNodeId, endNodeId, diameterOrWidth, name, role);
    auto it = m_trussMembers.emplace(id, member).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onTrussMemberAdded(it->second);
    }

    return id;
}

bool Model::addTrussMemberWithId(int id, int startNodeId, int endNodeId, double diameterOrWidth, const std::string& name, TrussMemberRole role)
{
    if (m_trussMembers.find(id) != m_trussMembers.end() ||
        m_nodes.find(startNodeId) == m_nodes.end() ||
        m_nodes.find(endNodeId) == m_nodes.end())
    {
        return false;
    }

    TrussMember member(id, startNodeId, endNodeId, diameterOrWidth, name, role);
    auto it = m_trussMembers.emplace(id, member).first;
    if (id >= m_nextTrussMemberId)
    {
        m_nextTrussMemberId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onTrussMemberAdded(it->second);
    }

    return true;
}

bool Model::removeTrussMember(int memberId)
{
    auto it = m_trussMembers.find(memberId);
    if (it == m_trussMembers.end())
    {
        return false;
    }

    std::vector<int> removedMemberLoads = m_loadManager.removeMemberLoadsForElement(memberId, MemberTargetType::Truss);
    for (int loadId : removedMemberLoads)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onMemberLoadRemoved(loadId);
            obs->onLoadRemoved(loadId);
        }
    }

    m_trussMembers.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onTrussMemberRemoved(memberId);
    }

    return true;
}

TrussMember* Model::getTrussMember(int memberId)
{
    auto it = m_trussMembers.find(memberId);
    return (it != m_trussMembers.end()) ? &it->second : nullptr;
}

const TrussMember* Model::getTrussMember(int memberId) const
{
    auto it = m_trussMembers.find(memberId);
    return (it != m_trussMembers.end()) ? &it->second : nullptr;
}

void Model::notifyCableModified(int cableId)
{
    const Cable* c = getCable(cableId);
    if (c)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onCableModified(*c);
        }
    }
}

void Model::notifyNodalLoadAdded(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodalLoadAdded(loadId);
        obs->onLoadAdded(loadId);
    }
}

void Model::notifyNodalLoadModified(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodalLoadModified(loadId);
        obs->onLoadModified(loadId);
    }
}

void Model::notifyNodalLoadRemoved(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onNodalLoadRemoved(loadId);
        obs->onLoadRemoved(loadId);
    }
}

void Model::notifyMemberLoadAdded(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onMemberLoadAdded(loadId);
        obs->onLoadAdded(loadId);
    }
}

void Model::notifyMemberLoadModified(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onMemberLoadModified(loadId);
        obs->onLoadModified(loadId);
    }
}

void Model::notifyMemberLoadRemoved(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onMemberLoadRemoved(loadId);
        obs->onLoadRemoved(loadId);
    }
}

void Model::notifyLoadAdded(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onLoadAdded(loadId);
    }
}

void Model::notifyLoadModified(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onLoadModified(loadId);
    }
}

void Model::notifyLoadRemoved(int loadId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onLoadRemoved(loadId);
    }
}

void Model::notifyLoadCaseChanged(int caseId)
{
    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onLoadCaseChanged(caseId);
    }
}

int Model::addCable(int startNodeId, int endNodeId, double diameter, const std::string& name, CableGeometryMode mode, double sag)
{
    CableDefinition def;
    def.setNominalDiameter(diameter);
    def.setName(name.empty() ? ("Cable_" + std::to_string(m_nextCableId)) : name);
    return addCable(startNodeId, endNodeId, def, name, mode, sag);
}

int Model::addCable(int startNodeId, int endNodeId, CableType type, const std::string& name, CableGeometryMode mode, double sag)
{
    CableDefinition def;
    def.setType(type);
    def.setName(name.empty() ? ("Cable_" + std::to_string(m_nextCableId)) : name);
    return addCable(startNodeId, endNodeId, def, name, mode, sag);
}

int Model::addCable(int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name, CableGeometryMode mode, double sag)
{
    if (m_nodes.find(startNodeId) == m_nodes.end() || m_nodes.find(endNodeId) == m_nodes.end())
    {
        return -1;
    }

    int id = m_nextCableId++;
    while (m_cables.find(id) != m_cables.end())
    {
        id = m_nextCableId++;
    }

    Cable cable(id, startNodeId, endNodeId, definition, name);
    cable.setGeometryMode(mode);
    cable.setSag(sag);

    auto it = m_cables.emplace(id, cable).first;

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onCableAdded(it->second);
    }

    return id;
}

bool Model::addCableWithId(int id, int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name, CableGeometryMode mode, double sag)
{
    if (m_cables.find(id) != m_cables.end() ||
        m_nodes.find(startNodeId) == m_nodes.end() ||
        m_nodes.find(endNodeId) == m_nodes.end())
    {
        return false;
    }

    Cable cable(id, startNodeId, endNodeId, definition, name);
    cable.setGeometryMode(mode);
    cable.setSag(sag);

    auto it = m_cables.emplace(id, cable).first;
    if (id >= m_nextCableId)
    {
        m_nextCableId = id + 1;
    }

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onCableAdded(it->second);
    }

    return true;
}

bool Model::removeCable(int cableId)
{
    auto it = m_cables.find(cableId);
    if (it == m_cables.end())
    {
        return false;
    }

    std::vector<int> removedMemberLoads = m_loadManager.removeMemberLoadsForElement(cableId, MemberTargetType::Cable);
    for (int loadId : removedMemberLoads)
    {
        bumpRevision();
        for (auto* obs : m_observers)
        {
            obs->onMemberLoadRemoved(loadId);
            obs->onLoadRemoved(loadId);
        }
    }

    m_cables.erase(it);

    bumpRevision();
    for (auto* obs : m_observers)
    {
        obs->onCableRemoved(cableId);
    }

    return true;
}

Cable* Model::getCable(int cableId)
{
    auto it = m_cables.find(cableId);
    return (it != m_cables.end()) ? &it->second : nullptr;
}

const Cable* Model::getCable(int cableId) const
{
    auto it = m_cables.find(cableId);
    return (it != m_cables.end()) ? &it->second : nullptr;
}

} // namespace TSA::Model
