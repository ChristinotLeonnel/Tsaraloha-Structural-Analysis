#include "Model.h"
#include "ModelDiff.h"
#include "../UndoRedo/UndoManager.h"
#include <algorithm>
#include <cmath>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>

namespace TSA::Model
{

Model::Model()
    : m_coordinateSystem(std::make_shared<TSA::Coordinate::CoordinateSystem>())
    , m_undoManager(std::make_unique<TSA::UndoRedo::UndoManager>())
{
    m_coordinateSystem->setDefaultBuildingCoordinates();

    if (auto* lm = m_coordinateSystem->levelManager())
    {
        QObject::connect(lm, &TSA::Coordinate::LevelManager::levelElevationChanged, [this](const std::string& lvlId, double oldZ, double newZ) {
            onLevelElevationChanged(lvlId, oldZ, newZ);
        });
    }
}

Model::~Model() = default;

TSA::Coordinate::LevelManager* Model::levelManager()
{
    return m_coordinateSystem ? m_coordinateSystem->levelManager() : nullptr;
}

const TSA::Coordinate::LevelManager* Model::levelManager() const
{
    return m_coordinateSystem ? m_coordinateSystem->levelManager() : nullptr;
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

    m_nodes.erase(it);

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

    m_beams.erase(it);

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

    m_columns.erase(it);

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

    m_trussMembers.erase(it);

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
        for (auto* obs : m_observers)
        {
            obs->onCableModified(*c);
        }
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

    m_cables.erase(it);

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

bool Model::moveNodes(const std::set<int>& nodeIds, double dx, double dy, double dz)
{
    if (nodeIds.empty())
        return false;

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            n->setCoordinates(n->x() + dx, n->y() + dy, n->z() + dz);
            notifyNodeModified(nid);
        }
    }

    return true;
}

std::vector<int> Model::copyElements(const std::set<int>& nodeIds,
                                     const std::set<int>& beamIds,
                                     const std::set<int>& columnIds,
                                     const std::set<int>& slabIds,
                                     double dx, double dy, double dz, int repetitions)
{
    std::vector<int> newElementIds;
    if (repetitions < 1)
        return newElementIds;

    // Déterminer l'ensemble de tous les nœuds impliqués (explicites + connectés aux barres/dalles)
    std::set<int> allNodeIds = nodeIds;
    for (int bId : beamIds)
    {
        const auto* b = getBeam(bId);
        if (b) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    }
    for (int cId : columnIds)
    {
        const auto* c = getColumn(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }
    for (int sId : slabIds)
    {
        const auto* s = getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) allNodeIds.insert(nid);
        }
    }

    for (int step = 1; step <= repetitions; ++step)
    {
        double curDx = dx * step;
        double curDy = dy * step;
        double curDz = dz * step;

        std::map<int, int> oldToNewNodes;
        for (int oldNid : allNodeIds)
        {
            const auto* origNode = getNode(oldNid);
            if (origNode)
            {
                int newNid = addNode(origNode->x() + curDx, origNode->y() + curDy, origNode->z() + curDz);
                oldToNewNodes[oldNid] = newNid;
                newElementIds.push_back(newNid);
            }
        }

        for (int bId : beamIds)
        {
            const auto* origBeam = getBeam(bId);
            if (origBeam)
            {
                int newStart = oldToNewNodes[origBeam->startNodeId()];
                int newEnd = oldToNewNodes[origBeam->endNodeId()];
                int newBId = addBeam(newStart, newEnd, origBeam->width(), origBeam->height());
                if (auto* nb = getBeam(newBId))
                {
                    nb->setSection(origBeam->section());
                    nb->setMaterial(origBeam->material());
                    nb->setRotation(origBeam->rotation());
                    nb->setEccentricity(origBeam->eccentricity());
                    nb->setStartRelease(origBeam->startRelease());
                    nb->setEndRelease(origBeam->endRelease());
                    nb->setColor(origBeam->color());
                }
                newElementIds.push_back(newBId);
            }
        }

        for (int cId : columnIds)
        {
            const auto* origCol = getColumn(cId);
            if (origCol)
            {
                int newStart = oldToNewNodes[origCol->startNodeId()];
                int newEnd = oldToNewNodes[origCol->endNodeId()];
                int newCId = addColumn(newStart, newEnd, origCol->width(), origCol->height());
                if (auto* nc = getColumn(newCId))
                {
                    nc->setSection(origCol->section());
                    nc->setMaterial(origCol->material());
                    nc->setRotation(origCol->rotation());
                    nc->setColor(origCol->color());
                }
                newElementIds.push_back(newCId);
            }
        }

        for (int sId : slabIds)
        {
            const auto* origSlab = getSlab(sId);
            if (origSlab)
            {
                std::vector<int> newSlabNodes;
                for (int nid : origSlab->nodeIds())
                {
                    newSlabNodes.push_back(oldToNewNodes[nid]);
                }
                int newSId = addSlab(newSlabNodes, origSlab->thickness());
                if (auto* ns = getSlab(newSId))
                {
                    ns->setMaterial(origSlab->material());
                    ns->setSlabType(origSlab->slabType());
                    ns->setColor(origSlab->color());
                }
                newElementIds.push_back(newSId);
            }
        }
    }

    return newElementIds;
}

bool Model::rotateNodes(const std::set<int>& nodeIds, const gp_Pnt& center, const gp_Dir& axis, double angleRad)
{
    if (nodeIds.empty() || std::abs(angleRad) < 1e-7)
        return false;

    gp_Trsf trsf;
    trsf.SetRotation(gp_Ax1(center, axis), angleRad);

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            gp_Pnt p(n->x(), n->y(), n->z());
            p.Transform(trsf);
            n->setCoordinates(p.X(), p.Y(), p.Z());
            notifyNodeModified(nid);
        }
    }
    return true;
}

std::vector<int> Model::copyAndRotateElements(const std::set<int>& nodeIds,
                                              const std::set<int>& beamIds,
                                              const std::set<int>& columnIds,
                                              const std::set<int>& slabIds,
                                              const gp_Pnt& center, const gp_Dir& axis,
                                              double angleRad, int repetitions)
{
    std::vector<int> newElementIds;
    if (repetitions < 1 || std::abs(angleRad) < 1e-7)
        return newElementIds;

    std::set<int> allNodeIds = nodeIds;
    for (int bId : beamIds)
    {
        const auto* b = getBeam(bId);
        if (b) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    }
    for (int cId : columnIds)
    {
        const auto* c = getColumn(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }
    for (int sId : slabIds)
    {
        const auto* s = getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) allNodeIds.insert(nid);
        }
    }

    for (int step = 1; step <= repetitions; ++step)
    {
        double curAngle = angleRad * step;
        gp_Trsf trsf;
        trsf.SetRotation(gp_Ax1(center, axis), curAngle);

        std::map<int, int> oldToNewNodes;
        for (int oldNid : allNodeIds)
        {
            const auto* origNode = getNode(oldNid);
            if (origNode)
            {
                gp_Pnt p(origNode->x(), origNode->y(), origNode->z());
                p.Transform(trsf);
                int newNid = addNode(p.X(), p.Y(), p.Z());
                oldToNewNodes[oldNid] = newNid;
                newElementIds.push_back(newNid);
            }
        }

        for (int bId : beamIds)
        {
            const auto* origBeam = getBeam(bId);
            if (origBeam)
            {
                int newStart = oldToNewNodes[origBeam->startNodeId()];
                int newEnd = oldToNewNodes[origBeam->endNodeId()];
                int newBId = addBeam(newStart, newEnd, origBeam->width(), origBeam->height());
                if (auto* nb = getBeam(newBId))
                {
                    nb->setSection(origBeam->section());
                    nb->setMaterial(origBeam->material());
                    nb->setRotation(origBeam->rotation());
                    nb->setEccentricity(origBeam->eccentricity());
                    nb->setStartRelease(origBeam->startRelease());
                    nb->setEndRelease(origBeam->endRelease());
                    nb->setColor(origBeam->color());
                }
                newElementIds.push_back(newBId);
            }
        }

        for (int cId : columnIds)
        {
            const auto* origCol = getColumn(cId);
            if (origCol)
            {
                int newStart = oldToNewNodes[origCol->startNodeId()];
                int newEnd = oldToNewNodes[origCol->endNodeId()];
                int newCId = addColumn(newStart, newEnd, origCol->width(), origCol->height());
                if (auto* nc = getColumn(newCId))
                {
                    nc->setSection(origCol->section());
                    nc->setMaterial(origCol->material());
                    nc->setRotation(origCol->rotation());
                    nc->setColor(origCol->color());
                }
                newElementIds.push_back(newCId);
            }
        }

        for (int sId : slabIds)
        {
            const auto* origSlab = getSlab(sId);
            if (origSlab)
            {
                std::vector<int> newSlabNodes;
                for (int nid : origSlab->nodeIds())
                {
                    newSlabNodes.push_back(oldToNewNodes[nid]);
                }
                int newSId = addSlab(newSlabNodes, origSlab->thickness());
                if (auto* ns = getSlab(newSId))
                {
                    ns->setMaterial(origSlab->material());
                    ns->setSlabType(origSlab->slabType());
                    ns->setColor(origSlab->color());
                }
                newElementIds.push_back(newSId);
            }
        }
    }

    return newElementIds;
}

void Model::pushUndoState(const std::string& actionName)
{
    if (m_undoManager)
    {
        m_undoManager->pushState(*this, actionName);
    }
}

bool Model::canUndo() const
{
    return m_undoManager ? m_undoManager->canUndo() : false;
}

bool Model::canRedo() const
{
    return m_undoManager ? m_undoManager->canRedo() : false;
}

bool Model::undo()
{
    return m_undoManager ? m_undoManager->undo(*this) : false;
}

bool Model::redo()
{
    return m_undoManager ? m_undoManager->redo(*this) : false;
}

void Model::clearUndoRedo()
{
    if (m_undoManager)
    {
        m_undoManager->clear();
    }
}

std::string Model::lastUndoActionName() const
{
    return m_undoManager ? m_undoManager->lastUndoActionName() : "";
}

std::string Model::lastRedoActionName() const
{
    return m_undoManager ? m_undoManager->lastRedoActionName() : "";
}

TSA::UndoRedo::UndoManager* Model::undoManager()
{
    return m_undoManager.get();
}

const TSA::UndoRedo::UndoManager* Model::undoManager() const
{
    return m_undoManager.get();
}

Model::ModelStateSnapshot Model::createSnapshot(const std::string& actionName) const
{
    ModelStateSnapshot snap;
    snap.nodes = m_nodes;
    snap.beams = m_beams;
    snap.columns = m_columns;
    snap.slabs = m_slabs;
    snap.walls = m_walls;
    snap.foundations = m_foundations;
    snap.trussMembers = m_trussMembers;
    snap.cables = m_cables;
    snap.nextNodeId = m_nextNodeId;
    snap.nextBeamId = m_nextBeamId;
    snap.nextColumnId = m_nextColumnId;
    snap.nextSlabId = m_nextSlabId;
    snap.nextWallId = m_nextWallId;
    snap.nextFoundationId = m_nextFoundationId;
    snap.nextTrussMemberId = m_nextTrussMemberId;
    snap.nextCableId = m_nextCableId;
    snap.actionName = actionName;
    return snap;
}

void Model::applySnapshotData(const Model::ModelStateSnapshot& snapshot)
{
    m_nodes = snapshot.nodes;
    m_beams = snapshot.beams;
    m_columns = snapshot.columns;
    m_slabs = snapshot.slabs;
    m_walls = snapshot.walls;
    m_foundations = snapshot.foundations;
    m_trussMembers = snapshot.trussMembers;
    m_cables = snapshot.cables;
    m_nextNodeId = snapshot.nextNodeId;
    m_nextBeamId = snapshot.nextBeamId;
    m_nextColumnId = snapshot.nextColumnId;
    m_nextSlabId = snapshot.nextSlabId;
    m_nextWallId = snapshot.nextWallId;
    m_nextFoundationId = snapshot.nextFoundationId;
    m_nextTrussMemberId = snapshot.nextTrussMemberId;
    m_nextCableId = snapshot.nextCableId;
    m_isModified = true;
}

void Model::notifyModelDiffApplied(const ModelDiff& diff)
{
    for (auto* obs : m_observers)
    {
        obs->onModelDiffApplied(diff);
    }
}

void Model::restoreSnapshot(const Model::ModelStateSnapshot& snapshot)
{
    applySnapshotData(snapshot);

    for (auto* obs : m_observers)
    {
        obs->onModelCleared();
    }
}

void Model::clear()
{
    m_cables.clear();
    m_trussMembers.clear();
    m_foundations.clear();
    m_walls.clear();
    m_slabs.clear();
    m_columns.clear();
    m_beams.clear();
    m_nodes.clear();
    m_nextNodeId = 1;
    m_nextBeamId = 1;
    m_nextColumnId = 1;
    m_nextSlabId = 1;
    m_nextWallId = 1;
    m_nextFoundationId = 1;
    m_nextTrussMemberId = 1;
    m_nextCableId = 1;
    m_isModified = false;
    clearUndoRedo();

    for (auto* obs : m_observers)
    {
        obs->onModelCleared();
    }
}

} // namespace TSA::Model
