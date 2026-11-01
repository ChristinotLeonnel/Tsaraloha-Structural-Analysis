#include "LegacyBarFrames.h"

#include "CopyTransform.h"
#include "Load/LoadManager.h"
#include "Model.h"
#include "../Coordinate/CoordinateTransformationService.h"

#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace TSA::Model
{

namespace
{
using TSA::Coordinate::CoordinateTransformationService;

constexpr double kLegacyVerticalDot = 0.999;

bool legacyVertical(const gp_Pnt& a, const gp_Pnt& b)
{
    return std::abs(gp_Dir(gp_Vec(a, b)).Z()) > kLegacyVerticalDot;
}

/// Repère local de la convention d'avant le format 3, pour une barre « verticale » : référence Y global.
gp_Ax3 legacyVerticalFrame(const gp_Pnt& a, const gp_Pnt& b, double rotationDeg)
{
    const gp_Dir x(gp_Vec(a, b));
    gp_Vec y = gp_Vec(0.0, 1.0, 0.0).Crossed(gp_Vec(x));
    y.Normalize();
    gp_Vec z = gp_Vec(x).Crossed(y);
    z.Normalize();
    const double rad = rotationDeg * std::numbers::pi / 180.0;
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(a, x), rad);
    y.Transform(rotation);
    z.Transform(rotation);
    return gp_Ax3(a, gp_Dir(z), x);
}

/// Angle, dans la convention actuelle, de la section dont l'axe y local est celui de `frame`.
double currentRotation(const gp_Ax3& frame, const gp_Pnt& a, const gp_Pnt& b)
{
    const gp_Ax3 base = CoordinateTransformationService::computeElementLocalFrame(a, b, 0.0);
    const gp_Vec y(frame.YDirection());
    const double c = y.Dot(gp_Vec(base.YDirection())), s = y.Dot(gp_Vec(base.Direction()));
    return std::atan2(std::abs(s) < 1e-12 ? 0.0 : s, std::abs(c) < 1e-12 ? 0.0 : c) * 180.0 / std::numbers::pi;
}

gp_Pnt pointOf(const Model& model, int nodeId)
{
    const Node* n = model.getNode(nodeId);
    return n ? gp_Pnt(n->x(), n->y(), n->z()) : gp_Pnt();
}

template <typename Bar>
bool migrateRotation(const Model& model, Bar& bar)
{
    const gp_Pnt a = pointOf(model, bar.startNodeId()), b = pointOf(model, bar.endNodeId());
    if (a.Distance(b) < 1e-6 || !legacyVertical(a, b)) return false;
    bar.setRotation(currentRotation(legacyVerticalFrame(a, b, bar.rotation()), a, b));
    return true;
}

/// Barre sans angle de section : son repère local change, ses charges locales sont réexprimées.
int migrateLoads(Model& model, MemberTargetType target, int elementId, const gp_Pnt& a, const gp_Pnt& b)
{
    if (a.Distance(b) < 1e-6 || !legacyVertical(a, b)) return 0;
    auto& loads = model.loadManager();
    const gp_Ax3 from = legacyVerticalFrame(a, b, 0.0);
    const gp_Ax3 to = CoordinateTransformationService::computeElementLocalFrame(a, b, 0.0);
    std::vector<MemberLoad> local;
    for (const auto& [id, load] : loads.memberLoads())
        if (load.targetType() == target && load.elementId() == elementId && load.coordSystem() == LoadCoordSystem::Local) local.push_back(load);
    for (const MemberLoad& load : local)
    {
        loads.removeMemberLoad(load.id());
        for (MemberLoad part : memberLoadInFrame(load, from, to))
        {
            part.setId(0);
            loads.addMemberLoad(part);
        }
    }
    return local.empty() ? 0 : 1;
}
} // namespace

int migrateLegacyBarFrames(Model& model)
{
    int changed = 0;
    for (const int id : [&] { std::vector<int> ids; for (const auto& [k, e] : model.beams()) ids.push_back(k); return ids; }())
        changed += migrateRotation(model, *model.getBeam(id)) ? 1 : 0;
    for (const int id : [&] { std::vector<int> ids; for (const auto& [k, e] : model.columns()) ids.push_back(k); return ids; }())
        changed += migrateRotation(model, *model.getColumn(id)) ? 1 : 0;
    for (const auto& [id, t] : model.trussMembers())
        changed += migrateLoads(model, MemberTargetType::Truss, id, pointOf(model, t.startNodeId()), pointOf(model, t.endNodeId()));
    for (const auto& [id, c] : model.cables())
        changed += migrateLoads(model, MemberTargetType::Cable, id, pointOf(model, c.startNodeId()), pointOf(model, c.endNodeId()));
    return changed;
}

} // namespace TSA::Model
