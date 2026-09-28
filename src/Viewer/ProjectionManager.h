#pragma once

#include <QObject>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include "../Coordinate/WorkPlane.h"

namespace TSA::Viewer
{

/**
 * @brief Mode de projection de TSA (Section 5 & 6).
 */
enum class ProjectionMode
{
    ThreeD, // Mode A : Projection 3D / Vue 3D dans l'espace
    TwoD    // Mode B : Projection 2D plane sur le WorkPlane actif
};

/**
 * @brief Direction de projection sur le plan de travail.
 */
enum class ProjectionDirection
{
    Normal,         // Direction normale (+Zwp)
    OppositeNormal, // Direction opposée à la normale (-Zwp)
    CameraRay,      // Suivant le rayon caméra
    Custom          // Direction libre personnalisée
};

/**
 * @brief Gestionnaire de projection spécialisé (Règle 3, 6 & 11).
 * Responsable de la logique de projection mathématique et cinématique :
 * - Mode 3D vs Mode 2D
 * - Direction de projection
 * - Projection des coordonnées WCS <-> WorkPlane Local (Xwp, Ywp)
 * - Non destructif pour le modèle structural
 */
class ProjectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectionManager(QObject* parent = nullptr);
    ~ProjectionManager() override = default;

    // Mode de projection (3D vs 2D)
    ProjectionMode mode() const noexcept { return m_mode; }
    void setMode(ProjectionMode mode);

    bool is2D() const noexcept { return m_mode == ProjectionMode::TwoD; }
    bool is3D() const noexcept { return m_mode == ProjectionMode::ThreeD; }

    // Direction de projection
    ProjectionDirection direction() const noexcept { return m_direction; }
    void setDirection(ProjectionDirection dir);

    const gp_Dir& customDirection() const noexcept { return m_customDir; }
    void setCustomDirection(const gp_Dir& dir);

    // Calcul de la direction effective de projection pour un WorkPlane donné
    gp_Dir effectiveDirection(const TSA::Coordinate::WorkPlane& wp, const gp_Dir& cameraDir = gp_Dir(0, 0, -1)) const;

    // Projection d'un point 3D global sur le WorkPlane
    // En Mode 2D : projette orthogonalement ou selon la direction choisie
    // En Mode 3D : conserve la géométrie 3D
    gp_Pnt projectPoint(const gp_Pnt& worldPoint, const TSA::Coordinate::WorkPlane& wp) const;

    // Intersection rayon caméra -> point sur le plan de travail et extraction (Xwp, Ywp)
    bool projectCursorRay(const gp_Pnt& eye,
                          const gp_Dir& rayDir,
                          const TSA::Coordinate::WorkPlane& wp,
                          gp_Pnt& outGlobalHit,
                          double& outXwp,
                          double& outYwp) const;

    bool projectCursorRay(const gp_Pnt& eye,
                          const gp_Dir& rayDir,
                          const TSA::Coordinate::WorkPlane& wp,
                          gp_Pnt& outGlobalHit) const
    {
        double u = 0.0, v = 0.0;
        return projectCursorRay(eye, rayDir, wp, outGlobalHit, u, v);
    }

signals:
    void projectionModeChanged(ProjectionMode mode);
    void projectionDirectionChanged(ProjectionDirection dir);

private:
    ProjectionMode m_mode = ProjectionMode::ThreeD;
    ProjectionDirection m_direction = ProjectionDirection::Normal;
    gp_Dir m_customDir = gp_Dir(0, 0, 1);
};

} // namespace TSA::Viewer
