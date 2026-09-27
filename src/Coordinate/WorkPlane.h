#pragma once

#include <gp_Pln.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <gp_Trsf.hxx>
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
    Custom          // Plan arbitraire défini par origine et orientation libre
};

/**
 * @brief Gestionnaire de plan de travail 3D (Work Plane) interactif pour la modélisation et le dessin CAO.
 * Fournit :
 * - Position 3D (Origine), orientation (Axe X local, Axe Y local, Normale)
 * - Dimensions visibles (Largeur, Hauteur)
 * - Grille locale paramétrable (espacement X/Y, subdivisions, visibilité)
 * - États de contrôle (Actif, Visible, Verrouillé, Isolé)
 * - Projections géométriques, conversion bidirectionnelle WCS <-> UCS
 * - Transformations 3D directes (Translation, Rotation, gp_Trsf)
 */
class WorkPlane
{
public:
    WorkPlane();
    explicit WorkPlane(WorkPlaneType type, const std::string& name = "Plan XY", double offset = 0.0);
    WorkPlane(const gp_Ax3& coordinateSystem, const std::string& name = "Plan Personnalisé", WorkPlaneType type = WorkPlaneType::Custom);

    // Identifiant & Nom
    int id() const noexcept { return m_id; }
    void setId(int id) noexcept { m_id = id; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Type de plan
    WorkPlaneType type() const noexcept { return m_type; }
    void setType(WorkPlaneType t) noexcept { m_type = t; }

    // Position & Repère géométrique
    double offset() const noexcept { return m_offset; }
    void setOffset(double off);

    const gp_Ax3& coordinateSystem() const noexcept { return m_cs; }
    void setCoordinateSystem(const gp_Ax3& cs);

    const gp_Pln& plane() const noexcept { return m_plane; }

    gp_Pnt origin() const noexcept { return m_cs.Location(); }
    void setOrigin(const gp_Pnt& orig);

    gp_Dir normal() const noexcept { return m_cs.Direction(); }
    gp_Dir xDirection() const noexcept { return m_cs.XDirection(); }
    gp_Dir yDirection() const noexcept { return m_cs.YDirection(); }

    // Angles d'orientation (en degrés)
    double rotationX() const noexcept;
    double rotationY() const noexcept;
    double rotationZ() const noexcept;
    void setRotation(double rxDeg, double ryDeg, double rzDeg);

    // Dimensions visibles dans le viewport 3D
    double width() const noexcept { return m_width; }
    double height() const noexcept { return m_height; }
    void setDimensions(double w, double h) noexcept;
    void setWidth(double w) noexcept { m_width = w; }
    void setHeight(double h) noexcept { m_height = h; }

    // Paramètres de grille locale propre au plan
    double gridSpacingX() const noexcept { return m_gridSpacingX; }
    double gridSpacingY() const noexcept { return m_gridSpacingY; }
    int gridSubdivisions() const noexcept { return m_gridSubdivisions; }
    bool isGridVisible() const noexcept { return m_isGridVisible; }
    void setGridSettings(double spX, double spY, int subdivisions, bool visible) noexcept;
    void setGridSpacingX(double s) noexcept { m_gridSpacingX = s; }
    void setGridSpacingY(double s) noexcept { m_gridSpacingY = s; }
    void setGridSubdivisions(int sub) noexcept { m_gridSubdivisions = sub; }
    void setIsGridVisible(bool v) noexcept { m_isGridVisible = v; }

    // États de contrôle
    bool isVisible() const noexcept { return m_isVisible; }
    void setVisible(bool v) noexcept { m_isVisible = v; }
    void setIsVisible(bool v) noexcept { m_isVisible = v; }

    bool isActive() const noexcept { return m_isActive; }
    void setActive(bool a) noexcept { m_isActive = a; }
    void setIsActive(bool a) noexcept { m_isActive = a; }

    bool isLocked() const noexcept { return m_isLocked; }
    void setLocked(bool l) noexcept { m_isLocked = l; }
    void setIsLocked(bool l) noexcept { m_isLocked = l; }

    bool isIsolated() const noexcept { return m_isIsolated; }
    void setIsolated(bool iso) noexcept { m_isIsolated = iso; }
    void setIsIsolated(bool iso) noexcept { m_isIsolated = iso; }

    double isolationDistance() const noexcept { return m_isolationDistance; }
    void setIsolationDistance(double d) noexcept { m_isolationDistance = d; }

    // Transformations 3D directes (pour Gizmo et manipulations temps réel)
    void translate(const gp_Vec& vec);
    void rotate(const gp_Pnt& center, const gp_Dir& axis, double angleRad);
    void rotate(const gp_Ax1& axis, double angleRad) { rotate(axis.Location(), axis.Direction(), angleRad); }
    void transform(const gp_Trsf& trsf);
    void setLocalAxes(const gp_Dir& xDir, const gp_Dir& yDir, const gp_Dir& zDir) {
        (void)yDir;
        m_cs = gp_Ax3(m_cs.Location(), zDir, xDir);
        m_plane = gp_Pln(m_cs);
        m_type = WorkPlaneType::Custom;
    }

    // Projections géométriques
    bool projectRay(const gp_Pnt& eye, const gp_Dir& rayDir, gp_Pnt& outPnt) const;
    gp_Pnt projectOrtho(const gp_Pnt& worldPoint) const;
    double distanceTo(const gp_Pnt& worldPoint) const;

    // Transformation WCS (Monde global) <-> UCS (Plan de travail local)
    gp_Pnt toUcs(const gp_Pnt& worldPoint) const;
    gp_Pnt toWorld(const gp_Pnt& ucsPoint) const;
    gp_Pnt toWorld(double u, double v) const { return toWorld(gp_Pnt(u, v, 0.0)); }
    void toLocal(const gp_Pnt& worldPoint, double& outU, double& outV) const {
        gp_Pnt p = toUcs(worldPoint);
        outU = p.X();
        outV = p.Y();
    }

    // Usines standard (Robot SA / AutoCAD style)
    static WorkPlane xy(double elevation = 0.0, const std::string& name = "Plan XY");
    static WorkPlane xz(double yOffset = 0.0, const std::string& name = "Plan XZ");
    static WorkPlane yz(double xOffset = 0.0, const std::string& name = "Plan YZ");
    static WorkPlane fromThreePoints(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3, const std::string& name = "Plan 3 Points");
    static WorkPlane fromOriginAndNormal(const gp_Pnt& origin, const gp_Dir& normal, const std::string& name = "Plan Personnalisé");

    // Sérialisation JSON
    std::string serializeToJson() const;
    static WorkPlane deserializeFromJson(const std::string& json);
    std::string toJson() const { return serializeToJson(); }
    static WorkPlane fromJson(const std::string& json) { return deserializeFromJson(json); }

private:
    void updatePlane();

private:
    int m_id = 1;
    WorkPlaneType m_type = WorkPlaneType::GlobalXY;
    std::string m_name = "Plan XY";
    double m_offset = 0.0;
    gp_Ax3 m_cs;
    gp_Pln m_plane;

    // Dimensions visibles
    double m_width = 20.0;
    double m_height = 20.0;

    // Grille locale
    double m_gridSpacingX = 1.0;
    double m_gridSpacingY = 1.0;
    int m_gridSubdivisions = 5;
    bool m_isGridVisible = true;

    // États
    bool m_isVisible = true;
    bool m_isActive = true;
    bool m_isLocked = false;
    bool m_isIsolated = false;
    double m_isolationDistance = 1.5;
};

} // namespace TSA::Coordinate
