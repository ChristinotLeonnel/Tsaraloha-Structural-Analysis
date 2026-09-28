#pragma once

#include "WorkPlane.h"
#include "WorkPlaneCoordinateSystem.h"
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace TSA::Coordinate
{

/**
 * @brief Composant spécialisé de transformation de coordonnées (Règle 1 & 11).
 * Fournit une passerelle découplée entre :
 * 1. Global <-> WorkPlane Local (Xwp, Ywp, Zwp)
 * 2. Global <-> Object Local (Élément structural longitudinal & inertiel)
 */
class CoordinateTransform
{
public:
    // Global -> WorkPlane Local
    static gp_Pnt globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp);
    static gp_Pnt globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlaneCoordinateSystem& cs);
    static gp_Pnt toWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlaneCoordinateSystem& cs);
    static gp_Pnt toWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp);
    static void globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp, double& outXwp, double& outYwp, double& outZwp);
    static void globalToWorkPlaneLocal2D(const gp_Pnt& globalPt, const WorkPlane& wp, double& outXwp, double& outYwp);

    // WorkPlane Local -> Global
    static gp_Pnt workPlaneLocalToGlobal(const gp_Pnt& localPt, const WorkPlane& wp);
    static gp_Pnt workPlaneLocalToGlobal(const gp_Pnt& localPt, const WorkPlaneCoordinateSystem& cs);
    static gp_Pnt toGlobalFromWorkPlane(const gp_Pnt& localPt, const WorkPlaneCoordinateSystem& cs);
    static gp_Pnt toGlobalFromWorkPlane(const gp_Pnt& localPt, const WorkPlane& wp);
    static gp_Pnt workPlaneLocalToGlobal(double xLocal, double yLocal, double zLocal, const WorkPlane& wp);
    static gp_Pnt workPlaneLocal2DToGlobal(double xLocal, double yLocal, const WorkPlane& wp);

    // Global -> Object Local
    static gp_Pnt globalToObjectLocal(const gp_Pnt& globalPt, const gp_Pnt& elemStart, const gp_Pnt& elemEnd, double betaAngleDeg = 0.0);

    // Object Local -> Global
    static gp_Pnt objectLocalToGlobal(const gp_Pnt& localPt, const gp_Pnt& elemStart, const gp_Pnt& elemEnd, double betaAngleDeg = 0.0);
};

} // namespace TSA::Coordinate
