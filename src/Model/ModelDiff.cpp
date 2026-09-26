#include "ModelDiff.h"
#include <cmath>
#include <set>

namespace TSA::Model
{

namespace
{

inline bool approxEq(double a, double b, double eps = 1e-5)
{
    return std::abs(a - b) <= eps;
}

bool isNodeDifferent(const Node& a, const Node& b)
{
    if (!approxEq(a.x(), b.x()) ||
        !approxEq(a.y(), b.y()) ||
        !approxEq(a.z(), b.z()))
    {
        return true;
    }
    if (a.name() != b.name()) return true;
    if (a.levelId() != b.levelId()) return true;
    if (a.supportType() != b.supportType()) return true;
    if (a.color() != b.color()) return true;
    return false;
}

bool isSectionDifferent(const Section& a, const Section& b)
{
    if (a.shape != b.shape) return true;
    if (!approxEq(a.width, b.width)) return true;
    if (!approxEq(a.height, b.height)) return true;
    if (!approxEq(a.diameter, b.diameter)) return true;
    if (!approxEq(a.tw, b.tw)) return true;
    if (!approxEq(a.tf, b.tf)) return true;
    if (a.name != b.name) return true;
    return false;
}

bool isMaterialDifferent(const Material& a, const Material& b)
{
    if (a.id != b.id && a.id != 0 && b.id != 0) return true;
    if (a.type != b.type) return true;
    if (a.name != b.name) return true;
    if (!approxEq(a.E, b.E, 1e-2)) return true;
    if (!approxEq(a.nu, b.nu, 1e-5)) return true;
    if (!approxEq(a.density, b.density, 1e-2)) return true;
    if (!approxEq(a.fk, b.fk, 1e-2)) return true;
    if (!approxEq(a.thermalCoeff, b.thermalCoeff, 1e-8)) return true;
    if (a.visual.baseColor != b.visual.baseColor) return true;
    if (!approxEq(a.visual.roughness, b.visual.roughness, 1e-4)) return true;
    if (!approxEq(a.visual.metallic, b.visual.metallic, 1e-4)) return true;
    if (!approxEq(a.visual.transparency, b.visual.transparency, 1e-4)) return true;
    if (a.visual.textureName != b.visual.textureName) return true;
    return false;
}

bool isReleaseDifferent(const EndRelease& a, const EndRelease& b)
{
    return a.fx != b.fx || a.fy != b.fy || a.fz != b.fz ||
           a.mx != b.mx || a.my != b.my || a.mz != b.mz;
}

bool isBeamDifferent(const Beam& a, const Beam& b, const std::set<int>& movedNodes)
{
    if (a.startNodeId() != b.startNodeId() || a.endNodeId() != b.endNodeId()) return true;
    if (!approxEq(a.rotation(), b.rotation(), 1e-4)) return true;
    if (a.eccentricity() != b.eccentricity()) return true;
    if (a.role() != b.role()) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isReleaseDifferent(a.startRelease(), b.startRelease())) return true;
    if (isReleaseDifferent(a.endRelease(), b.endRelease())) return true;
    if (isSectionDifferent(a.section(), b.section())) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    if (movedNodes.count(b.startNodeId()) > 0 || movedNodes.count(b.endNodeId()) > 0) return true;
    return false;
}

bool isColumnDifferent(const Column& a, const Column& b, const std::set<int>& movedNodes)
{
    if (a.startNodeId() != b.startNodeId() || a.endNodeId() != b.endNodeId()) return true;
    if (!approxEq(a.rotation(), b.rotation(), 1e-4)) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isSectionDifferent(a.section(), b.section())) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    if (movedNodes.count(b.startNodeId()) > 0 || movedNodes.count(b.endNodeId()) > 0) return true;
    return false;
}

bool isSlabDifferent(const Slab& a, const Slab& b, const std::set<int>& movedNodes)
{
    if (a.nodeIds() != b.nodeIds()) return true;
    if (!approxEq(a.thickness(), b.thickness())) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    for (int nid : b.nodeIds())
    {
        if (movedNodes.count(nid) > 0) return true;
    }
    return false;
}

bool isWallDifferent(const Wall& a, const Wall& b, const std::set<int>& movedNodes)
{
    if (a.startNodeId() != b.startNodeId() || a.endNodeId() != b.endNodeId()) return true;
    if (!approxEq(a.thickness(), b.thickness())) return true;
    if (!approxEq(a.height(), b.height())) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    if (movedNodes.count(b.startNodeId()) > 0 || movedNodes.count(b.endNodeId()) > 0) return true;
    return false;
}

bool isFoundationDifferent(const Foundation& a, const Foundation& b, const std::set<int>& movedNodes)
{
    if (a.nodeId() != b.nodeId()) return true;
    if (a.foundationType() != b.foundationType()) return true;
    if (!approxEq(a.widthA(), b.widthA())) return true;
    if (!approxEq(a.lengthB(), b.lengthB())) return true;
    if (!approxEq(a.heightH(), b.heightH())) return true;
    if (!approxEq(a.soilBearingCapacity(), b.soilBearingCapacity())) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    if (movedNodes.count(b.nodeId()) > 0) return true;
    return false;
}

bool isTrussDifferent(const TrussMember& a, const TrussMember& b, const std::set<int>& movedNodes)
{
    if (a.startNodeId() != b.startNodeId() || a.endNodeId() != b.endNodeId()) return true;
    if (a.role() != b.role()) return true;
    if (a.name() != b.name()) return true;
    if (a.color() != b.color()) return true;
    if (isSectionDifferent(a.section(), b.section())) return true;
    if (isMaterialDifferent(a.material(), b.material())) return true;
    if (movedNodes.count(b.startNodeId()) > 0 || movedNodes.count(b.endNodeId()) > 0) return true;
    return false;
}

} // anonymous namespace

ModelDiff ModelDiff::compute(const Model::ModelStateSnapshot& before, const Model::ModelStateSnapshot& after)
{
    ModelDiff diff;
    std::set<int> movedNodes;

    // 1. Nœuds
    for (const auto& [id, nAfter] : after.nodes)
    {
        auto it = before.nodes.find(id);
        if (it == before.nodes.end())
        {
            diff.nodes.created.push_back(id);
            movedNodes.insert(id);
        }
        else if (isNodeDifferent(it->second, nAfter))
        {
            diff.nodes.modified.push_back(id);
            movedNodes.insert(id);
        }
    }
    for (const auto& [id, _] : before.nodes)
    {
        if (after.nodes.find(id) == after.nodes.end())
        {
            diff.nodes.deleted.push_back(id);
            movedNodes.insert(id);
        }
    }

    // 2. Poutres
    for (const auto& [id, bAfter] : after.beams)
    {
        auto it = before.beams.find(id);
        if (it == before.beams.end())
        {
            diff.beams.created.push_back(id);
        }
        else if (isBeamDifferent(it->second, bAfter, movedNodes))
        {
            diff.beams.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.beams)
    {
        if (after.beams.find(id) == after.beams.end())
        {
            diff.beams.deleted.push_back(id);
        }
    }

    // 3. Poteaux
    for (const auto& [id, cAfter] : after.columns)
    {
        auto it = before.columns.find(id);
        if (it == before.columns.end())
        {
            diff.columns.created.push_back(id);
        }
        else if (isColumnDifferent(it->second, cAfter, movedNodes))
        {
            diff.columns.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.columns)
    {
        if (after.columns.find(id) == after.columns.end())
        {
            diff.columns.deleted.push_back(id);
        }
    }

    // 4. Dalles
    for (const auto& [id, sAfter] : after.slabs)
    {
        auto it = before.slabs.find(id);
        if (it == before.slabs.end())
        {
            diff.slabs.created.push_back(id);
        }
        else if (isSlabDifferent(it->second, sAfter, movedNodes))
        {
            diff.slabs.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.slabs)
    {
        if (after.slabs.find(id) == after.slabs.end())
        {
            diff.slabs.deleted.push_back(id);
        }
    }

    // 5. Voiles
    for (const auto& [id, wAfter] : after.walls)
    {
        auto it = before.walls.find(id);
        if (it == before.walls.end())
        {
            diff.walls.created.push_back(id);
        }
        else if (isWallDifferent(it->second, wAfter, movedNodes))
        {
            diff.walls.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.walls)
    {
        if (after.walls.find(id) == after.walls.end())
        {
            diff.walls.deleted.push_back(id);
        }
    }

    // 6. Fondations
    for (const auto& [id, fAfter] : after.foundations)
    {
        auto it = before.foundations.find(id);
        if (it == before.foundations.end())
        {
            diff.foundations.created.push_back(id);
        }
        else if (isFoundationDifferent(it->second, fAfter, movedNodes))
        {
            diff.foundations.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.foundations)
    {
        if (after.foundations.find(id) == after.foundations.end())
        {
            diff.foundations.deleted.push_back(id);
        }
    }

    // 7. Treillis
    for (const auto& [id, tAfter] : after.trussMembers)
    {
        auto it = before.trussMembers.find(id);
        if (it == before.trussMembers.end())
        {
            diff.trussMembers.created.push_back(id);
        }
        else if (isTrussDifferent(it->second, tAfter, movedNodes))
        {
            diff.trussMembers.modified.push_back(id);
        }
    }
    for (const auto& [id, _] : before.trussMembers)
    {
        if (after.trussMembers.find(id) == after.trussMembers.end())
        {
            diff.trussMembers.deleted.push_back(id);
        }
    }

    diff.createdNodeIds = diff.nodes.created;
    diff.modifiedNodeIds = diff.nodes.modified;
    diff.deletedNodeIds = diff.nodes.deleted;

    diff.createdBeamIds = diff.beams.created;
    diff.modifiedBeamIds = diff.beams.modified;
    diff.deletedBeamIds = diff.beams.deleted;

    diff.createdColumnIds = diff.columns.created;
    diff.modifiedColumnIds = diff.columns.modified;
    diff.deletedColumnIds = diff.columns.deleted;

    diff.createdSlabIds = diff.slabs.created;
    diff.modifiedSlabIds = diff.slabs.modified;
    diff.deletedSlabIds = diff.slabs.deleted;

    diff.createdWallIds = diff.walls.created;
    diff.modifiedWallIds = diff.walls.modified;
    diff.deletedWallIds = diff.walls.deleted;

    diff.createdFoundationIds = diff.foundations.created;
    diff.modifiedFoundationIds = diff.foundations.modified;
    diff.deletedFoundationIds = diff.foundations.deleted;

    diff.createdTrussMemberIds = diff.trussMembers.created;
    diff.modifiedTrussMemberIds = diff.trussMembers.modified;
    diff.deletedTrussMemberIds = diff.trussMembers.deleted;

    return diff;
}

} // namespace TSA::Model
