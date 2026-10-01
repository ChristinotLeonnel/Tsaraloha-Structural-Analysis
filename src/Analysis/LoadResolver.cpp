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

LocalMemberLoadComponents LoadResolver::resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                               const TSA::Model::Model& model)
{
    LocalMemberLoadComponents result;

    // Récupérer les coordonnées de début et fin de l'élément
    int elemId = load.elementId();
    int startNodeId = 0;
    int endNodeId = 0;
    double rotationDeg = 0.0;

    const auto* b = model.getBeam(elemId);
    if (b)
    {
        startNodeId = b->startNodeId();
        endNodeId = b->endNodeId();
        rotationDeg = b->rotation();
    }
    else
    {
        const auto* col = model.getColumn(elemId);
        if (col)
        {
            startNodeId = col->startNodeId();
            endNodeId = col->endNodeId();
            rotationDeg = col->rotation();
        }
        else
        {
            const auto* tr = model.getTrussMember(elemId);
            if (tr)
            {
                startNodeId = tr->startNodeId();
                endNodeId = tr->endNodeId();
            }
        }
    }

    const auto* n1 = model.getNode(startNodeId);
    const auto* n2 = model.getNode(endNodeId);
    if (!n1 || !n2)
    {
        // En cas d'élément introuvable, renvoie direct la valeur selon direction
        return result;
    }

    gp_Pnt p1(n1->x(), n1->y(), n1->z());
    gp_Pnt p2(n2->x(), n2->y(), n2->z());

    double qMag = load.q1(); // Intensité représentative

    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
    {
        // La charge est déjà exprimée dans le repère local
        switch (load.direction())
        {
        case TSA::Model::LoadDirection::LocalX:
            result.wx = qMag;
            break;
        case TSA::Model::LoadDirection::LocalY:
            result.wy = qMag;
            break;
        case TSA::Model::LoadDirection::LocalZ:
            result.wz = qMag;
            break;
        default:
            result.wz = -qMag; // Par défaut transversal
            break;
        }
        return result;
    }

    // Charge exprimée en repère global : construire le vecteur global
    gp_Vec globalVec(0.0, 0.0, 0.0);
    switch (load.direction())
    {
    case TSA::Model::LoadDirection::GlobalX:
        globalVec = gp_Vec(qMag, 0.0, 0.0);
        break;
    case TSA::Model::LoadDirection::GlobalY:
        globalVec = gp_Vec(0.0, qMag, 0.0);
        break;
    case TSA::Model::LoadDirection::GlobalZ:
    case TSA::Model::LoadDirection::Gravity:
        // Gravité / charge verticale descendante (-Z)
        globalVec = gp_Vec(0.0, 0.0, -std::abs(qMag));
        break;
    default:
        globalVec = gp_Vec(0.0, 0.0, -std::abs(qMag));
        break;
    }

    return decomposeGlobalVectorToLocal(globalVec, p1, p2, rotationDeg);
}

LocalMemberLoadComponents LoadResolver::resolveMemberLoadToLocal(const TSA::Model::MemberLoad& load,
                                                               const CalculationSnapshot& snapshot)
{
    LocalMemberLoadComponents result;

    const auto* el = snapshot.getElement(load.elementId());
    if (!el) return result;

    const auto* n1 = snapshot.getNode(el->startNodeId);
    const auto* n2 = snapshot.getNode(el->endNodeId);
    if (!n1 || !n2) return result;

    gp_Pnt p1(n1->x, n1->y, n1->z);
    gp_Pnt p2(n2->x, n2->y, n2->z);
    double rotationDeg = el->rotation;
    double qMag = load.q1();

    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
    {
        switch (load.direction())
        {
        case TSA::Model::LoadDirection::LocalX:
            result.wx = qMag;
            break;
        case TSA::Model::LoadDirection::LocalY:
            result.wy = qMag;
            break;
        case TSA::Model::LoadDirection::LocalZ:
            result.wz = qMag;
            break;
        default:
            result.wz = -std::abs(qMag);
            break;
        }
        return result;
    }

    gp_Vec globalVec(0.0, 0.0, 0.0);
    switch (load.direction())
    {
    case TSA::Model::LoadDirection::GlobalX:
        globalVec = gp_Vec(qMag, 0.0, 0.0);
        break;
    case TSA::Model::LoadDirection::GlobalY:
        globalVec = gp_Vec(0.0, qMag, 0.0);
        break;
    case TSA::Model::LoadDirection::GlobalZ:
    case TSA::Model::LoadDirection::Gravity:
        globalVec = gp_Vec(0.0, 0.0, -std::abs(qMag));
        break;
    default:
        globalVec = gp_Vec(0.0, 0.0, -std::abs(qMag));
        break;
    }

    return decomposeGlobalVectorToLocal(globalVec, p1, p2, rotationDeg);
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
