#pragma once

#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

namespace TSA::Coordinate
{

/**
 * @brief Repère cartésien 3D orthonormé local d'un plan de travail (WorkPlane).
 * Stocke mathématiquement :
 * - Origin (Owp) : Point d'origine 3D dans le repère global
 * - AxisX  (Xwp) : Axe horizontal du plan (vecteur unitaire)
 * - AxisY  (Ywp) : Axe vertical du plan (vecteur unitaire)
 * - AxisZ / Normal (Zwp) : Normale au plan (vecteur unitaire, Zwp = Xwp ^ Ywp)
 *
 * Assure les transformations directes et inverses :
 * Pglobal = Origin + Xlocal * AxisX + Ylocal * AxisY + Zlocal * AxisZ
 * Xlocal  = (Pglobal - Origin) . AxisX
 * Ylocal  = (Pglobal - Origin) . AxisY
 * Zlocal  = (Pglobal - Origin) . AxisZ
 */
class WorkPlaneCoordinateSystem
{
public:
    WorkPlaneCoordinateSystem();
    explicit WorkPlaneCoordinateSystem(const gp_Pnt& origin,
                                       const gp_Dir& normal = gp_Dir(0, 0, 1),
                                       const gp_Dir& axisX  = gp_Dir(1, 0, 0));
    explicit WorkPlaneCoordinateSystem(const gp_Ax3& cs);

    // Définition par origine et deux axes (X et Y direct)
    static WorkPlaneCoordinateSystem fromOriginAndAxes(const gp_Pnt& origin,
                                                       const gp_Dir& axisX,
                                                       const gp_Dir& axisY);

    // Repères standard
    static WorkPlaneCoordinateSystem xy(double elevation = 0.0);
    static WorkPlaneCoordinateSystem xz(double yOffset = 0.0);
    static WorkPlaneCoordinateSystem yz(double xOffset = 0.0);

    // Accesseurs du repère orthonormé
    const gp_Pnt& origin() const noexcept { return m_origin; }
    void setOrigin(const gp_Pnt& orig) noexcept;

    const gp_Dir& axisX() const noexcept { return m_axisX; }
    const gp_Dir& axisY() const noexcept { return m_axisY; }
    const gp_Dir& axisZ() const noexcept { return m_axisZ; }
    const gp_Dir& normal() const noexcept { return m_axisZ; }

    void setAxes(const gp_Dir& axisX, const gp_Dir& axisY);
    void setAxesFromNormal(const gp_Dir& normal, const gp_Dir& preferredX = gp_Dir(1, 0, 0));

    // Transformations de coordonnées
    // Global -> Local (Xwp, Ywp, Zwp)
    gp_Pnt toLocal(const gp_Pnt& globalPnt) const;
    void toLocal(const gp_Pnt& globalPnt, double& outXwp, double& outYwp, double& outZwp) const;
    void toLocal2D(const gp_Pnt& globalPnt, double& outXwp, double& outYwp) const;

    // Local (Xwp, Ywp, Zwp) -> Global (X, Y, Z)
    gp_Pnt toGlobal(const gp_Pnt& localPnt) const;
    gp_Pnt toGlobal(double xLocal, double yLocal, double zLocal = 0.0) const;

    // Vecteurs Global <-> Local
    gp_Vec vectorToLocal(const gp_Vec& globalVec) const;
    gp_Vec vectorToGlobal(const gp_Vec& localVec) const;

    // Projections géométriques
    gp_Pnt projectPoint(const gp_Pnt& globalPnt) const;
    double distanceTo(const gp_Pnt& globalPnt) const;

    // Rayon caméra -> intersection avec le plan
    bool intersectRay(const gp_Pnt& rayOrigin, const gp_Dir& rayDir, gp_Pnt& outGlobalHit, double& outT) const;
    bool intersectRay(const gp_Pnt& rayOrigin, const gp_Dir& rayDir, gp_Pnt& outGlobalHit) const
    {
        double t = 0.0;
        return intersectRay(rayOrigin, rayDir, outGlobalHit, t);
    }

    // Interopérabilité OCCT
    gp_Ax3 toAx3() const;
    gp_Pln toPln() const;

private:
    void orthonormalize(const gp_Dir& xDir, const gp_Dir& yDir);

private:
    gp_Pnt m_origin;
    gp_Dir m_axisX;
    gp_Dir m_axisY;
    gp_Dir m_axisZ; // Normal
};

} // namespace TSA::Coordinate
