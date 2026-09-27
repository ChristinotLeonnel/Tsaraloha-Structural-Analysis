#pragma once

#include <gp_Pln.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <string>

namespace TSA::Coordinate
{

enum class WorkPlaneType
{
    GlobalXY,       // Plan horizontal standard XY
    GlobalXZ,       // Plan vertical frontal XZ
    GlobalYZ,       // Plan vertical latéral YZ
    ElevationZ,     // Plan horizontal décalé à une altitude Z (étage)
    ThreePoints,    // Plan défini par 3 points de référence
    ParallelToFace, // Plan parallèle à une face/surface
    Custom          // Plan arbitraire défini par origine et normale
};

/**
 * @brief Gestionnaire de plan de travail 3D (Work Plane) pour la modélisation et le dessin CAO.
 * Fournit la projection exacte des rayons caméra, l'accrochage sur le plan et la conversion WCS <-> UCS.
 */
class WorkPlane
{
public:
    WorkPlane();
    explicit WorkPlane(WorkPlaneType type, const std::string& name = "Plan XY", double offset = 0.0);
    WorkPlane(const gp_Ax3& coordinateSystem, const std::string& name = "Plan Personnalisé", WorkPlaneType type = WorkPlaneType::Custom);

    // Propriétés
    WorkPlaneType type() const noexcept { return m_type; }
    void setType(WorkPlaneType t) noexcept { m_type = t; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    double offset() const noexcept { return m_offset; }
    void setOffset(double off);

    const gp_Ax3& coordinateSystem() const noexcept { return m_cs; }
    void setCoordinateSystem(const gp_Ax3& cs);

    const gp_Pln& plane() const noexcept { return m_plane; }

    gp_Pnt origin() const noexcept { return m_cs.Location(); }
    gp_Dir normal() const noexcept { return m_cs.Direction(); }
    gp_Dir xDirection() const noexcept { return m_cs.XDirection(); }
    gp_Dir yDirection() const noexcept { return m_cs.YDirection(); }

    // Projections géométriques
    bool projectRay(const gp_Pnt& eye, const gp_Dir& rayDir, gp_Pnt& outPnt) const;
    gp_Pnt projectOrtho(const gp_Pnt& worldPoint) const;
    double distanceTo(const gp_Pnt& worldPoint) const;

    // Transformation WCS (Monde) <-> UCS (Plan de travail)
    gp_Pnt toUcs(const gp_Pnt& worldPoint) const;
    gp_Pnt toWorld(const gp_Pnt& ucsPoint) const;

    // Usines standard (Robot SA / AutoCAD style)
    static WorkPlane xy(double elevation = 0.0, const std::string& name = "Plan XY");
    static WorkPlane xz(double yOffset = 0.0, const std::string& name = "Plan XZ");
    static WorkPlane yz(double xOffset = 0.0, const std::string& name = "Plan YZ");
    static WorkPlane fromThreePoints(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3, const std::string& name = "Plan 3 Points");
    static WorkPlane fromOriginAndNormal(const gp_Pnt& origin, const gp_Dir& normal, const std::string& name = "Plan Personnalisé");

    // Sérialisation
    std::string serializeToJson() const;
    static WorkPlane deserializeFromJson(const std::string& json);
    std::string toJson() const { return serializeToJson(); }
    static WorkPlane fromJson(const std::string& json) { return deserializeFromJson(json); }

private:
    void updatePlane();

private:
    WorkPlaneType m_type = WorkPlaneType::GlobalXY;
    std::string m_name = "Plan XY";
    double m_offset = 0.0;
    gp_Ax3 m_cs;
    gp_Pln m_plane;
};

} // namespace TSA::Coordinate
