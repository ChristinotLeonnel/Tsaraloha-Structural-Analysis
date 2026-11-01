#include "CoordinateTransformationService.h"
#include <gp_Trsf.hxx>
#include <cmath>

namespace TSA::Coordinate
{

CoordinateTransformationService& CoordinateTransformationService::instance()
{
    static CoordinateTransformationService s_instance;
    return s_instance;
}

CoordinateTransformationService::CoordinateTransformationService()
    : m_activeWorkPlane(WorkPlane::xy(0.0))
{
}

void CoordinateTransformationService::setActiveWorkPlane(const WorkPlane& wp)
{
    m_activeWorkPlane = wp;
}

gp_Pnt CoordinateTransformationService::wcsToUcs(const gp_Pnt& worldPoint) const
{
    return m_activeWorkPlane.toUcs(worldPoint);
}

gp_Pnt CoordinateTransformationService::ucsToWcs(const gp_Pnt& ucsPoint) const
{
    return m_activeWorkPlane.toWorld(ucsPoint);
}

gp_Vec CoordinateTransformationService::wcsVectorToUcs(const gp_Vec& worldVec) const
{
    gp_Dir Xd = m_activeWorkPlane.xDirection();
    gp_Dir Yd = m_activeWorkPlane.yDirection();
    gp_Dir Zd = m_activeWorkPlane.normal();

    double u = worldVec.Dot(gp_Vec(Xd));
    double v = worldVec.Dot(gp_Vec(Yd));
    double w = worldVec.Dot(gp_Vec(Zd));
    return gp_Vec(u, v, w);
}

gp_Vec CoordinateTransformationService::ucsVectorToWcs(const gp_Vec& ucsVec) const
{
    gp_Dir Xd = m_activeWorkPlane.xDirection();
    gp_Dir Yd = m_activeWorkPlane.yDirection();
    gp_Dir Zd = m_activeWorkPlane.normal();

    gp_Vec Vu = gp_Vec(Xd) * ucsVec.X();
    gp_Vec Vv = gp_Vec(Yd) * ucsVec.Y();
    gp_Vec Vw = gp_Vec(Zd) * ucsVec.Z();

    return Vu + Vv + Vw;
}

bool CoordinateTransformationService::isVertical(const gp_Dir& direction)
{
    return std::hypot(direction.X(), direction.Y()) < kVerticalTolerance;
}

gp_Ax3 CoordinateTransformationService::computeElementLocalFrame(const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg)
{
    gp_Vec vX(startPt, endPt);
    double len = vX.Magnitude();
    if (len < 1e-6)
    {
        return gp_Ax3(startPt, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    }

    gp_Dir dirX(vX);

    // Convention de Robot Structural Analysis, mesurée (docs/audit/2026-10-31/COORDINATE_SYSTEM_AND_SECTION_ROTATION_REPORT.md) :
    // barre verticale, y = +Y global, quel que soit le sens de x ; sinon y = Z global × x. z = x × y.
    gp_Vec vY;
    if (isVertical(dirX))
    {
        vY = gp_Vec(0.0, 1.0, 0.0) - gp_Vec(dirX) * dirX.Y();
    }
    else
    {
        vY = gp_Vec(0.0, 0.0, 1.0).Crossed(gp_Vec(dirX));
    }
    vY.Normalize();

    gp_Vec vZ = gp_Vec(dirX).Crossed(vY);
    vZ.Normalize();

    // Rotation d'angle beta autour de x ; composantes exactes aux multiples de 90°
    if (std::abs(betaAngleDeg) > 1e-4)
    {
        const double rad = betaAngleDeg * M_PI / 180.0;
        const double raw = std::cos(rad), ras = std::sin(rad);
        const double c = std::abs(raw) < 1e-14 ? 0.0 : raw, s = std::abs(ras) < 1e-14 ? 0.0 : ras;
        const gp_Vec y = vY * c + vZ * s;
        vZ = vZ * c - vY * s;
        vY = y;
    }

    return gp_Ax3(startPt, gp_Dir(vZ), gp_Dir(dirX));
}

gp_Pnt CoordinateTransformationService::wcsToElementLocal(const gp_Pnt& worldPoint, const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg)
{
    gp_Ax3 frame = computeElementLocalFrame(startPt, endPt, betaAngleDeg);
    gp_Vec v(startPt, worldPoint);

    gp_Dir dX = frame.XDirection();
    gp_Dir dZ = frame.Direction();
    gp_Dir dY = frame.YDirection();

    double lx = v.Dot(gp_Vec(dX));
    double ly = v.Dot(gp_Vec(dY));
    double lz = v.Dot(gp_Vec(dZ));

    return gp_Pnt(lx, ly, lz);
}

gp_Pnt CoordinateTransformationService::elementLocalToWcs(const gp_Pnt& localPoint, const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg)
{
    gp_Ax3 frame = computeElementLocalFrame(startPt, endPt, betaAngleDeg);
    gp_Dir dX = frame.XDirection();
    gp_Dir dZ = frame.Direction();
    gp_Dir dY = frame.YDirection();

    gp_Vec vx = gp_Vec(dX) * localPoint.X();
    gp_Vec vy = gp_Vec(dY) * localPoint.Y();
    gp_Vec vz = gp_Vec(dZ) * localPoint.Z();

    return startPt.Translated(vx).Translated(vy).Translated(vz);
}

} // namespace TSA::Coordinate
