#include "WorkPlaneCoordinateSystem.h"
#include <cmath>
#include <algorithm>

namespace TSA::Coordinate
{

WorkPlaneCoordinateSystem::WorkPlaneCoordinateSystem()
    : m_origin(0.0, 0.0, 0.0)
    , m_axisX(1.0, 0.0, 0.0)
    , m_axisY(0.0, 1.0, 0.0)
    , m_axisZ(0.0, 0.0, 1.0)
{
}

WorkPlaneCoordinateSystem::WorkPlaneCoordinateSystem(const gp_Pnt& origin,
                                                     const gp_Dir& normal,
                                                     const gp_Dir& axisX)
    : m_origin(origin)
{
    setAxesFromNormal(normal, axisX);
}

WorkPlaneCoordinateSystem::WorkPlaneCoordinateSystem(const gp_Ax3& cs)
    : m_origin(cs.Location())
    , m_axisX(cs.XDirection())
    , m_axisY(cs.YDirection())
    , m_axisZ(cs.Direction())
{
}

WorkPlaneCoordinateSystem WorkPlaneCoordinateSystem::fromOriginAndAxes(const gp_Pnt& origin,
                                                                     const gp_Dir& axisX,
                                                                     const gp_Dir& axisY)
{
    WorkPlaneCoordinateSystem cs;
    cs.m_origin = origin;
    cs.setAxes(axisX, axisY);
    return cs;
}

WorkPlaneCoordinateSystem WorkPlaneCoordinateSystem::xy(double elevation)
{
    return WorkPlaneCoordinateSystem(gp_Pnt(0.0, 0.0, elevation),
                                     gp_Dir(0.0, 0.0, 1.0),
                                     gp_Dir(1.0, 0.0, 0.0));
}

WorkPlaneCoordinateSystem WorkPlaneCoordinateSystem::xz(double yOffset)
{
    // Façade frontale XZ :
    // Axe horizontal = +X (1, 0, 0)
    // Axe vertical   = +Z (0, 0, 1)
    // Normale        = X ^ Y = (1, 0, 0) ^ (0, 0, 1) = (0, -1, 0) (face vers l'observateur en Y-)
    return fromOriginAndAxes(gp_Pnt(0.0, yOffset, 0.0),
                             gp_Dir(1.0, 0.0, 0.0),
                             gp_Dir(0.0, 0.0, 1.0));
}

WorkPlaneCoordinateSystem WorkPlaneCoordinateSystem::yz(double xOffset)
{
    // Façade latérale YZ :
    // Axe horizontal = +Y (0, 1, 0)
    // Axe vertical   = +Z (0, 0, 1)
    // Normale        = (0, 1, 0) ^ (0, 0, 1) = (1, 0, 0) (+X)
    return fromOriginAndAxes(gp_Pnt(xOffset, 0.0, 0.0),
                             gp_Dir(0.0, 1.0, 0.0),
                             gp_Dir(0.0, 0.0, 1.0));
}

void WorkPlaneCoordinateSystem::setOrigin(const gp_Pnt& orig) noexcept
{
    m_origin = orig;
}

void WorkPlaneCoordinateSystem::setAxes(const gp_Dir& axisX, const gp_Dir& axisY)
{
    orthonormalize(axisX, axisY);
}

void WorkPlaneCoordinateSystem::setAxesFromNormal(const gp_Dir& normal, const gp_Dir& preferredX)
{
    gp_Vec nVec(normal);
    gp_Vec xVec(preferredX);

    // Si preferredX est colinéaire à normal, choisir un axe de repli orthogonal
    if (std::abs(nVec.Dot(xVec)) > 0.999)
    {
        if (std::abs(normal.Z()) < 0.9)
        {
            xVec = gp_Vec(0.0, 0.0, 1.0);
        }
        else
        {
            xVec = gp_Vec(1.0, 0.0, 0.0);
        }
    }

    // Calculer Y = Normal ^ X
    gp_Vec yVec = nVec.Crossed(xVec);
    yVec.Normalize();

    // Recalculer X = Y ^ Normal pour garantir l'orthogonalité exacte
    xVec = yVec.Crossed(nVec);
    xVec.Normalize();

    m_axisX = gp_Dir(xVec);
    m_axisY = gp_Dir(yVec);
    m_axisZ = normal;
}

void WorkPlaneCoordinateSystem::orthonormalize(const gp_Dir& xDir, const gp_Dir& yDir)
{
    gp_Vec vx(xDir);
    gp_Vec vy(yDir);

    gp_Vec vz = vx.Crossed(vy);
    double lenZ = vz.Magnitude();
    if (lenZ < 1e-7)
    {
        // Axes quasi-colinéaires : repli sur XY standard
        m_axisX = gp_Dir(1.0, 0.0, 0.0);
        m_axisY = gp_Dir(0.0, 1.0, 0.0);
        m_axisZ = gp_Dir(0.0, 0.0, 1.0);
        return;
    }
    vz.Normalize();

    // Corriger vy pour qu'il soit parfaitement perpendiculaire à vx
    vy = vz.Crossed(vx);
    vy.Normalize();
    vx.Normalize();

    m_axisX = gp_Dir(vx);
    m_axisY = gp_Dir(vy);
    m_axisZ = gp_Dir(vz);
}

gp_Pnt WorkPlaneCoordinateSystem::toLocal(const gp_Pnt& globalPnt) const
{
    gp_Vec v(m_origin, globalPnt);
    double u = v.Dot(gp_Vec(m_axisX));
    double w = v.Dot(gp_Vec(m_axisY));
    double n = v.Dot(gp_Vec(m_axisZ));
    return gp_Pnt(u, w, n);
}

void WorkPlaneCoordinateSystem::toLocal(const gp_Pnt& globalPnt, double& outXwp, double& outYwp, double& outZwp) const
{
    gp_Vec v(m_origin, globalPnt);
    outXwp = v.Dot(gp_Vec(m_axisX));
    outYwp = v.Dot(gp_Vec(m_axisY));
    outZwp = v.Dot(gp_Vec(m_axisZ));
}

void WorkPlaneCoordinateSystem::toLocal2D(const gp_Pnt& globalPnt, double& outXwp, double& outYwp) const
{
    gp_Vec v(m_origin, globalPnt);
    outXwp = v.Dot(gp_Vec(m_axisX));
    outYwp = v.Dot(gp_Vec(m_axisY));
}

gp_Pnt WorkPlaneCoordinateSystem::toGlobal(const gp_Pnt& localPnt) const
{
    return toGlobal(localPnt.X(), localPnt.Y(), localPnt.Z());
}

gp_Pnt WorkPlaneCoordinateSystem::toGlobal(double xLocal, double yLocal, double zLocal) const
{
    gp_Vec vx = gp_Vec(m_axisX) * xLocal;
    gp_Vec vy = gp_Vec(m_axisY) * yLocal;
    gp_Vec vz = gp_Vec(m_axisZ) * zLocal;

    return m_origin.Translated(vx).Translated(vy).Translated(vz);
}

gp_Vec WorkPlaneCoordinateSystem::vectorToLocal(const gp_Vec& globalVec) const
{
    double u = globalVec.Dot(gp_Vec(m_axisX));
    double v = globalVec.Dot(gp_Vec(m_axisY));
    double w = globalVec.Dot(gp_Vec(m_axisZ));
    return gp_Vec(u, v, w);
}

gp_Vec WorkPlaneCoordinateSystem::vectorToGlobal(const gp_Vec& localVec) const
{
    gp_Vec vx = gp_Vec(m_axisX) * localVec.X();
    gp_Vec vy = gp_Vec(m_axisY) * localVec.Y();
    gp_Vec vz = gp_Vec(m_axisZ) * localVec.Z();
    return vx + vy + vz;
}

gp_Pnt WorkPlaneCoordinateSystem::projectPoint(const gp_Pnt& globalPnt) const
{
    gp_Vec v(m_origin, globalPnt);
    double distNorm = v.Dot(gp_Vec(m_axisZ));
    return globalPnt.Translated(gp_Vec(m_axisZ) * (-distNorm));
}

double WorkPlaneCoordinateSystem::distanceTo(const gp_Pnt& globalPnt) const
{
    gp_Vec v(m_origin, globalPnt);
    return std::abs(v.Dot(gp_Vec(m_axisZ)));
}

bool WorkPlaneCoordinateSystem::intersectRay(const gp_Pnt& rayOrigin,
                                            const gp_Dir& rayDir,
                                            gp_Pnt& outGlobalHit,
                                            double& outT) const
{
    gp_Vec norm(m_axisZ);
    double denom = rayDir.X() * norm.X() + rayDir.Y() * norm.Y() + rayDir.Z() * norm.Z();
    if (std::abs(denom) < 1e-7)
    {
        return false; // Rayon parallèle au plan
    }

    gp_Vec origDiff(rayOrigin, m_origin);
    double numer = origDiff.Dot(norm);
    double t = numer / denom;

    outT = t;
    outGlobalHit = gp_Pnt(rayOrigin.X() + t * rayDir.X(),
                          rayOrigin.Y() + t * rayDir.Y(),
                          rayOrigin.Z() + t * rayDir.Z());
    return true;
}

gp_Ax3 WorkPlaneCoordinateSystem::toAx3() const
{
    return gp_Ax3(m_origin, m_axisZ, m_axisX);
}

gp_Pln WorkPlaneCoordinateSystem::toPln() const
{
    return gp_Pln(toAx3());
}

} // namespace TSA::Coordinate
