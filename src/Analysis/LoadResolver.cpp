#include "LoadResolver.h"
#include "CalculationSnapshot.h"
#include "../Model/Model.h"
#include "../Coordinate/CoordinateTransformationService.h"

namespace TSA::Analysis
{

gp_Ax3 LoadResolver::computeElementLocalAxes(const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg)
{
    return TSA::Coordinate::CoordinateTransformationService::computeElementLocalFrame(p1, p2, betaAngleDeg);
}

LocalMemberLoadComponents LoadResolver::decomposeGlobalVectorToLocal(const gp_Vec& globalVec,
                                                                   const gp_Pnt& p1,
                                                                   const gp_Pnt& p2,
                                                                   double betaAngleDeg)
{
    gp_Ax3 frame = computeElementLocalAxes(p1, p2, betaAngleDeg);

    // Axe X longitudinal : XDirection() de gp_Ax3
    gp_Dir dirX = frame.XDirection();
    // Axe Z local : Direction() normale principale de gp_Ax3
    gp_Dir dirZ = frame.Direction();
    // Axe Y local : YDirection() de gp_Ax3
    gp_Dir dirY = frame.YDirection();

    LocalMemberLoadComponents comp;
    comp.wx = globalVec.Dot(gp_Vec(dirX));
    comp.wy = globalVec.Dot(gp_Vec(dirY));
    comp.wz = globalVec.Dot(gp_Vec(dirZ));
    return comp;
}

namespace
{
bool isLocalAxis(TSA::Model::LoadDirection d)
{
    using TSA::Model::LoadDirection;
    return d == LoadDirection::LocalX || d == LoadDirection::LocalY || d == LoadDirection::LocalZ;
}

bool isGlobalAxis(TSA::Model::LoadDirection d)
{
    using TSA::Model::LoadDirection;
    return d == LoadDirection::GlobalX || d == LoadDirection::GlobalY || d == LoadDirection::GlobalZ;
}
} // namespace

bool LoadResolver::usesMagnitudeOnly(const TSA::Model::MemberLoad& load)
{
    return load.coordSystem() == TSA::Model::LoadCoordSystem::Local ? !isLocalAxis(load.direction())
                                                                     : !isGlobalAxis(load.direction());
}

LocalMemberLoadComponents LoadResolver::memberLoadLocalComponents(const TSA::Model::MemberLoad& load, double q,
                                                                  const gp_Pnt& p1, const gp_Pnt& p2,
                                                                  double betaAngleDeg)
{
    using TSA::Model::LoadDirection;
    LocalMemberLoadComponents c;
    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
    {
        // Composantes exactes : aucune projection (résultats identiques à l'historique).
        switch (load.direction())
        {
        case LoadDirection::LocalX: c.wx = q; break;
        case LoadDirection::LocalY: c.wy = q; break;
        case LoadDirection::LocalZ: c.wz = q; break;
        default: c.wz = -std::abs(q); break;
        }
        return c;
    }
    gp_Vec g(0.0, 0.0, -std::abs(q));
    switch (load.direction())
    {
    case LoadDirection::GlobalX: g = gp_Vec(q, 0.0, 0.0); break;
    case LoadDirection::GlobalY: g = gp_Vec(0.0, q, 0.0); break;
    case LoadDirection::GlobalZ: g = gp_Vec(0.0, 0.0, q); break;
    default: break;   // Gravité, ou axe local en repère global : descendante |q|
    }
    return decomposeGlobalVectorToLocal(g, p1, p2, betaAngleDeg);
}

gp_Vec LoadResolver::memberLoadVector(const TSA::Model::MemberLoad& load, double q,
                                      const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg)
{
    using TSA::Model::LoadDirection;
    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
    {
        const LocalMemberLoadComponents c = memberLoadLocalComponents(load, q, p1, p2, betaAngleDeg);
        return localVectorToGlobal(c.wx, c.wy, c.wz, p1, p2, betaAngleDeg);
    }
    switch (load.direction())
    {
    case LoadDirection::GlobalX: return gp_Vec(q, 0.0, 0.0);
    case LoadDirection::GlobalY: return gp_Vec(0.0, q, 0.0);
    case LoadDirection::GlobalZ: return gp_Vec(0.0, 0.0, q);
    default: return gp_Vec(0.0, 0.0, -std::abs(q));
    }
}

LocalMemberLoadComponents LoadResolver::resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                               const TSA::Model::Model& model)
{
    // Extrémités de l'élément porteur (poutre, poteau, treillis) ; même recherche qu'auparavant.
    int startNodeId = 0;
    int endNodeId = 0;
    double rotationDeg = 0.0;
    const int elemId = load.elementId();
    auto take = [&](const auto* e) {
        if (!e) return false;
        startNodeId = e->startNodeId();
        endNodeId = e->endNodeId();
        return true;
    };
    using TSA::Model::MemberTargetType;
    if (load.targetType() == MemberTargetType::Column)
    {
        if (const auto* col = model.getColumn(elemId); take(col)) rotationDeg = col->rotation();
    }
    else if (load.targetType() == MemberTargetType::Truss)
    {
        take(model.getTrussMember(elemId));
    }
    else if (const auto* b = model.getBeam(elemId); take(b))
    {
        rotationDeg = b->rotation();
    }
    else if (const auto* col = model.getColumn(elemId); take(col))
    {
        rotationDeg = col->rotation();
    }
    else
    {
        take(model.getTrussMember(elemId));
    }

    const auto* n1 = model.getNode(startNodeId);
    const auto* n2 = model.getNode(endNodeId);
    if (!n1 || !n2) return {};
    return memberLoadLocalComponents(load, load.q1(), gp_Pnt(n1->x(), n1->y(), n1->z()),
                                     gp_Pnt(n2->x(), n2->y(), n2->z()), rotationDeg);
}

LocalMemberLoadComponents LoadResolver::resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                               const CalculationSnapshot& snapshot)
{
    const auto* el = snapshot.findElementForLoad(load);
    if (!el) return {};
    const auto* n1 = snapshot.getNode(el->startNodeId);
    const auto* n2 = snapshot.getNode(el->endNodeId);
    if (!n1 || !n2) return {};
    return memberLoadLocalComponents(load, load.q1(), gp_Pnt(n1->x, n1->y, n1->z), gp_Pnt(n2->x, n2->y, n2->z),
                                     el->rotation);
}

gp_Vec LoadResolver::localVectorToGlobal(double lx, double ly, double lz,
                                        const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg)
{
    gp_Ax3 frame = computeElementLocalAxes(p1, p2, betaAngleDeg);
    gp_Dir dirX = frame.XDirection();
    gp_Dir dirY = frame.YDirection();
    gp_Dir dirZ = frame.Direction();

    return gp_Vec(dirX) * lx + gp_Vec(dirY) * ly + gp_Vec(dirZ) * lz;
}

} // namespace TSA::Analysis
