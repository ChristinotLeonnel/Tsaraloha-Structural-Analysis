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

class GridSnapManager
{
public:
    GridSnapManager();
    ~GridSnapManager() = default;

    bool isSnapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }

    double snapTolerance() const { return m_snapTolerance; }
    void setSnapTolerance(double tolerance) { m_snapTolerance = tolerance; }

    // Recherche du point d'accrochage optimal
    GridSnapResult findSnap(const gp_Pnt& rawPoint,
                            const GridSystem* activeGrid,
                            const TSA::Model::Model* model = nullptr) const;

private:
    bool m_snapEnabled = true;
    double m_snapTolerance = 0.50; // Tolérance d'accrochage en mètres (0.5m par défaut)
};

} // namespace TSA::Grid
