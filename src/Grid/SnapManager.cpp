#include "SnapManager.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace TSA::Grid
{

SnapManager::SnapManager()
    : m_gridSnapManager(nullptr)
    , m_wpGridSnapEnabled(true)
    , m_snapTolerance(0.50)
{
}

SnapManager::SnapManager(GridSnapManager* gridSnapManager)
    : m_gridSnapManager(gridSnapManager)
    , m_wpGridSnapEnabled(true)
    , m_snapTolerance(0.50)
{
}

GridSnapResult SnapManager::snapToWorkPlaneGrid(const gp_Pnt& rawGlobalPoint,
                                               const TSA::Coordinate::WorkPlane& wp) const
{
    GridSnapResult res;
    res.snapped = false;
    res.point = rawGlobalPoint;
    res.type = GridSnapType::None;

    if (!wp.isGridVisible() || !m_wpGridSnapEnabled)
    {
        return res;
    }

    double spX = (wp.gridSpacingX() > 1e-4) ? wp.gridSpacingX() : 1.0;
    double spY = (wp.gridSpacingY() > 1e-4) ? wp.gridSpacingY() : 1.0;

    // Conversion en coordonnées locales (Xwp, Ywp, Zwp)
    double u = 0.0, v = 0.0, w = 0.0;
    wp.toLocal(rawGlobalPoint, u, v, w);

    // Arrondi aux multiples du pas de grille locale
    double uSnapped = std::round(u / spX) * spX;
    double vSnapped = std::round(v / spY) * spY;

    // Conversion retour vers le repère global avec Zwp = 0 sur le plan
    gp_Pnt snappedGlobal = wp.toGlobal(uSnapped, vSnapped, 0.0);
    double dist = rawGlobalPoint.Distance(snappedGlobal);

    if (dist <= m_snapTolerance)
    {
        res.snapped = true;
        res.point = snappedGlobal;
        res.distance = dist;
        res.type = GridSnapType::Intersection;

        std::ostringstream oss;
        oss << "Grille " << wp.name() << " ["
            << std::fixed << std::setprecision(2)
            << uSnapped << " m, " << vSnapped << " m]";
        res.description = oss.str();
    }

    return res;
}

GridSnapResult SnapManager::findWorkPlaneSnap(const gp_Pnt& rawGlobalPoint,
                                              const TSA::Coordinate::WorkPlane& wp,
                                              const TSA::Model::Model* model) const
{
    // 1. Priorité absolue : Object Snaps sur le modèle (Nœuds, extrémités, etc.)
    if (m_gridSnapManager && model)
    {
        GridSnapResult objSnap = m_gridSnapManager->findObjectSnap(rawGlobalPoint, model, m_snapTolerance);
        if (objSnap.snapped)
        {
            return objSnap;
        }
    }

    // 2. Priorité 2 : Accrochage sur la grille locale du WorkPlane
    GridSnapResult wpSnap = snapToWorkPlaneGrid(rawGlobalPoint, wp);
    if (wpSnap.snapped)
    {
        return wpSnap;
    }

    // Aucun accrochage : point brut
    return GridSnapResult{ false, rawGlobalPoint, GridSnapType::None, 0.0, "", -1 };
}

} // namespace TSA::Grid
