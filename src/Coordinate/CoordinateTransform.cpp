#include "CoordinateTransform.h"
#include "CoordinateTransformationService.h"

namespace TSA::Coordinate
{

gp_Pnt CoordinateTransform::globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toLocal(globalPt);
}

gp_Pnt CoordinateTransform::globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlaneCoordinateSystem& cs)
{
    return cs.toLocal(globalPt);
}

gp_Pnt CoordinateTransform::toWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlaneCoordinateSystem& cs)
{
    return cs.toLocal(globalPt);
}

gp_Pnt CoordinateTransform::toWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toLocal(globalPt);
}

void CoordinateTransform::globalToWorkPlaneLocal(const gp_Pnt& globalPt, const WorkPlane& wp,
                                                 double& outXwp, double& outYwp, double& outZwp)
{
    wp.coordinateSystemLocal().toLocal(globalPt, outXwp, outYwp, outZwp);
}

void CoordinateTransform::globalToWorkPlaneLocal2D(const gp_Pnt& globalPt, const WorkPlane& wp,
                                                   double& outXwp, double& outYwp)
{
    wp.coordinateSystemLocal().toLocal2D(globalPt, outXwp, outYwp);
}

gp_Pnt CoordinateTransform::workPlaneLocalToGlobal(const gp_Pnt& localPt, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toGlobal(localPt);
}

gp_Pnt CoordinateTransform::workPlaneLocalToGlobal(const gp_Pnt& localPt, const WorkPlaneCoordinateSystem& cs)
{
    return cs.toGlobal(localPt);
}

gp_Pnt CoordinateTransform::toGlobalFromWorkPlane(const gp_Pnt& localPt, const WorkPlaneCoordinateSystem& cs)
{
    return cs.toGlobal(localPt);
}

gp_Pnt CoordinateTransform::toGlobalFromWorkPlane(const gp_Pnt& localPt, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toGlobal(localPt);
}

gp_Pnt CoordinateTransform::workPlaneLocalToGlobal(double xLocal, double yLocal, double zLocal, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toGlobal(xLocal, yLocal, zLocal);
}

gp_Pnt CoordinateTransform::workPlaneLocal2DToGlobal(double xLocal, double yLocal, const WorkPlane& wp)
{
    return wp.coordinateSystemLocal().toGlobal(xLocal, yLocal, 0.0);
}

gp_Pnt CoordinateTransform::globalToObjectLocal(const gp_Pnt& globalPt, const gp_Pnt& elemStart, const gp_Pnt& elemEnd, double betaAngleDeg)
{
    return CoordinateTransformationService::wcsToElementLocal(globalPt, elemStart, elemEnd, betaAngleDeg);
}

gp_Pnt CoordinateTransform::objectLocalToGlobal(const gp_Pnt& localPt, const gp_Pnt& elemStart, const gp_Pnt& elemEnd, double betaAngleDeg)
{
    return CoordinateTransformationService::elementLocalToWcs(localPt, elemStart, elemEnd, betaAngleDeg);
}

} // namespace TSA::Coordinate
