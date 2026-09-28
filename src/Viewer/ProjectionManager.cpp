#include "ProjectionManager.h"
#include <cmath>

namespace TSA::Viewer
{

ProjectionManager::ProjectionManager(QObject* parent)
    : QObject(parent)
    , m_mode(ProjectionMode::ThreeD)
    , m_direction(ProjectionDirection::Normal)
    , m_customDir(0.0, 0.0, 1.0)
{
}

void ProjectionManager::setMode(ProjectionMode mode)
{
    if (m_mode != mode)
    {
        m_mode = mode;
        emit projectionModeChanged(m_mode);
    }
}

void ProjectionManager::setDirection(ProjectionDirection dir)
{
    if (m_direction != dir)
    {
        m_direction = dir;
        emit projectionDirectionChanged(m_direction);
    }
}

void ProjectionManager::setCustomDirection(const gp_Dir& dir)
{
    m_customDir = dir;
    if (m_direction == ProjectionDirection::Custom)
    {
        emit projectionDirectionChanged(m_direction);
    }
}

gp_Dir ProjectionManager::effectiveDirection(const TSA::Coordinate::WorkPlane& wp, const gp_Dir& cameraDir) const
{
    switch (m_direction)
    {
    case ProjectionDirection::OppositeNormal:
        return gp_Dir(gp_Vec(wp.normal()) * -1.0);
    case ProjectionDirection::CameraRay:
        return cameraDir;
    case ProjectionDirection::Custom:
        return m_customDir;
    case ProjectionDirection::Normal:
    default:
        return wp.normal();
    }
}

gp_Pnt ProjectionManager::projectPoint(const gp_Pnt& worldPoint, const TSA::Coordinate::WorkPlane& wp) const
{
    if (m_mode == ProjectionMode::ThreeD)
    {
        // En mode 3D, le point conserve sa position spatiale complète
        return worldPoint;
    }

    // En mode 2D, projection sur le plan de référence
    gp_Dir projDir = effectiveDirection(wp);
    gp_Vec n(wp.normal());
    double denom = projDir.Dot(n);

    if (std::abs(denom) < 1e-6)
    {
        // Direction quasi-parallèle : projection orthogonale directe
        return wp.projectOrtho(worldPoint);
    }

    // Intersection de la droite (worldPoint + t * projDir) avec le plan (P - Origin).N = 0
    gp_Vec origDiff(worldPoint, wp.origin());
    double t = origDiff.Dot(n) / denom;

    return gp_Pnt(worldPoint.X() + t * projDir.X(),
                  worldPoint.Y() + t * projDir.Y(),
                  worldPoint.Z() + t * projDir.Z());
}

bool ProjectionManager::projectCursorRay(const gp_Pnt& eye,
                                         const gp_Dir& rayDir,
                                         const TSA::Coordinate::WorkPlane& wp,
                                         gp_Pnt& outGlobalHit,
                                         double& outXwp,
                                         double& outYwp) const
{
    double t = 0.0;
    if (!wp.coordinateSystemLocal().intersectRay(eye, rayDir, outGlobalHit, t))
    {
        return false;
    }

    wp.coordinateSystemLocal().toLocal2D(outGlobalHit, outXwp, outYwp);
    return true;
}

} // namespace TSA::Viewer
