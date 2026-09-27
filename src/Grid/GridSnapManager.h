#pragma once

#include "GridType.h"
#include "GridSystem.h"
#include <gp_Pnt.hxx>

namespace TSA::Model
{
    class Model;
}

namespace TSA::Grid
{

class GridManager;

class GridSnapManager
{
public:
    GridSnapManager();
    ~GridSnapManager() = default;

    bool isSnapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }

    SnapMode activeModes() const { return m_activeModes; }
    void setActiveModes(SnapMode modes) { m_activeModes = modes; }
    bool isModeActive(SnapMode mode) const { return hasSnapMode(m_activeModes, mode); }
    void setModeActive(SnapMode mode, bool active);

    double snapTolerance() const { return m_snapTolerance; }
    void setSnapTolerance(double tolerance) { m_snapTolerance = tolerance; }

    void setReferencePoint(const gp_Pnt& p, bool hasRef = true) {
        m_refPoint = p;
        m_hasRefPoint = hasRef;
    }
    void clearReferencePoint() { m_hasRefPoint = false; }

    // Recherche du point d'accrochage optimal
    GridSnapResult findSnap(const gp_Pnt& rawPoint,
                            const GridManager* gridManager,
                            const TSA::Model::Model* model = nullptr) const;

    GridSnapResult findSnap(const gp_Pnt& rawPoint,
                            const GridSystem* activeGrid,
                            const TSA::Model::Model* model = nullptr) const;

    // Accrochages spécialisés
    GridSnapResult findObjectSnap(const gp_Pnt& rawPoint, const TSA::Model::Model& model) const;
    GridSnapResult findObjectSnap(const gp_Pnt& rawPoint, const TSA::Model::Model* model, double snapTol = -1.0) const;

private:
    bool m_snapEnabled = true;
    SnapMode m_activeModes = SnapMode::All;
    double m_snapTolerance = 0.50; // Tolérance d'accrochage en mètres
    gp_Pnt m_refPoint = gp_Pnt(0.0, 0.0, 0.0);
    bool m_hasRefPoint = false;
};

} // namespace TSA::Grid
