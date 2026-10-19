#include "GridCoordinateSync.h"

#include "GridDefinition.h"
#include "GridManager.h"
#include "GridSystem.h"
#include "../Coordinate/CoordinateSystem.h"
#include "../Coordinate/LevelManager.h"
#include "../Model/Model.h"

#include <cmath>

namespace TSA::Grid
{

namespace
{
std::vector<double> shifted(const std::vector<double>& values, double origin)
{
    std::vector<double> out;
    out.reserve(values.size());
    for (double v : values) out.push_back(origin + v);
    return out;
}

bool isUnrotated(const GridDefinition& grid)
{
    return std::abs(std::remainder(grid.rotationDeg(), 360.0)) < 1e-9;
}
} // namespace

void syncCoordinateSystem(const GridDefinition* grid, TSA::Coordinate::CoordinateSystem& cs)
{
    if (!grid)
    {
        cs.setXAxes({}, {});
        cs.setYAxes({}, {});
        cs.setGridElevations({}, {});
        return;
    }
    const gp_Pnt& o = grid->origin();
    if (grid->type() == GridType::Cartesian && isUnrotated(*grid))
    {
        cs.setXAxes(shifted(grid->xPositions(), o.X()), grid->xLabels());
        cs.setYAxes(shifted(grid->yPositions(), o.Y()), grid->yLabels());
    }
    else
    {
        cs.setXAxes({}, {});
        cs.setYAxes({}, {});
    }
    cs.setGridElevations(shifted(grid->zLevels(), o.Z()), grid->zLabels());
}

void applyGridLevels(const GridDefinition& grid, TSA::Coordinate::LevelManager& levels)
{
    levels.setFromElevations(shifted(grid.zLevels(), grid.origin().Z()), grid.zLabels());
}

bool hasLegacyDefaultLevels(const TSA::Coordinate::LevelManager& levels)
{
    static const double kElevations[] = { 0.0, 3.0, 6.5, 10.0 };
    static const char* kNames[] = { "RDC (Z=0.00)", "Étage 1 (Z=3.00)", "Étage 2 (Z=6.50)", "Toiture (Z=10.00)" };
    const auto& list = levels.levels();
    if (list.size() != 4) return false;
    for (std::size_t i = 0; i < 4; ++i)
        if (std::abs(list[i].elevation - kElevations[i]) > 1e-9 || list[i].name != kNames[i]) return false;
    return true;
}

void reconcileProject(TSA::Model::Model& model, const GridManager* grids)
{
    auto* cs = model.coordinateSystem();
    if (!cs) return;
    const GridSystem* active = grids ? grids->activeGrid() : nullptr;
    syncCoordinateSystem(active ? &active->definition() : nullptr, *cs);
    if (auto* lm = model.levelManager(); lm && hasLegacyDefaultLevels(*lm))
    {
        if (active) applyGridLevels(active->definition(), *lm);
        else lm->clear();
    }
}

} // namespace TSA::Grid
