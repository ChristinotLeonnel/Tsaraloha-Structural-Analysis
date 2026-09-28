#pragma once

#include <gp_Pnt.hxx>
#include <string>
#include "GridSnapManager.h"
#include "../Coordinate/WorkPlane.h"
#include "../Model/Model.h"

namespace TSA::Grid
{

/**
 * @brief Gestionnaire de Snapping unifié intégrant le repère local du WorkPlane (Section 11 & 13).
 * Assure le cycle cinématique complet :
 * Cursor -> Camera Ray -> WorkPlane Intersection -> Local Xwp/Ywp -> Snap -> Global Point
 */
class SnapManager
{
public:
    SnapManager();
    explicit SnapManager(GridSnapManager* gridSnapManager);

    void setGridSnapManager(GridSnapManager* gsm) noexcept { m_gridSnapManager = gsm; }
    GridSnapManager* gridSnapManager() const noexcept { return m_gridSnapManager; }

    bool isWorkPlaneGridSnapEnabled() const noexcept { return m_wpGridSnapEnabled; }
    void setWorkPlaneGridSnapEnabled(bool enabled) noexcept { m_wpGridSnapEnabled = enabled; }

    double snapTolerance() const noexcept { return m_snapTolerance; }
    void setSnapTolerance(double tol) noexcept { m_snapTolerance = tol; }

    // Accrochage sur le plan de travail actif
    GridSnapResult findWorkPlaneSnap(const gp_Pnt& rawGlobalPoint,
                                     const TSA::Coordinate::WorkPlane& wp,
                                     const TSA::Model::Model* model) const;

    // Accrochage direct sur la grille locale du WorkPlane (Xwp, Ywp)
    GridSnapResult snapToWorkPlaneGrid(const gp_Pnt& rawGlobalPoint,
                                      const TSA::Coordinate::WorkPlane& wp) const;

private:
    GridSnapManager* m_gridSnapManager = nullptr;
    bool m_wpGridSnapEnabled = true;
    double m_snapTolerance = 0.50;
};

} // namespace TSA::Grid
