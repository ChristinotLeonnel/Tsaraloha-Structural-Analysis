#include "GridLabelLayout.h"

#include <algorithm>
#include <cmath>

namespace TSA::Grid
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

/// Indice du niveau retenu : le plus proche du niveau actif (plan), sinon le plus bas.
std::size_t chooseLevel(const std::vector<double>& globalZ, const GridLabelView& view)
{
    std::size_t best = 0;
    for (std::size_t k = 1; k < globalZ.size(); ++k)
    {
        const bool better = (view.kind == GridViewKind::Plan && view.hasActiveLevel)
                                ? std::abs(globalZ[k] - view.activeLevelZ) < std::abs(globalZ[best] - view.activeLevelZ)
                                : globalZ[k] < globalZ[best];
        if (better) best = k;
    }
    return best;
}
} // namespace

GridViewKind classifyGridView(const gp_Dir& viewDirection, double gridRotationDeg, double toleranceDeg)
{
    const double c = std::cos(toleranceDeg * kPi / 180.0);
    const double r = gridRotationDeg * kPi / 180.0;
    const gp_Dir lx(std::cos(r), std::sin(r), 0.0);
    const gp_Dir ly(-std::sin(r), std::cos(r), 0.0);
    if (std::abs(viewDirection.Z()) >= c) return GridViewKind::Plan;
    if (std::abs(viewDirection.Dot(lx)) >= c) return GridViewKind::ElevationAlongX;
    if (std::abs(viewDirection.Dot(ly)) >= c) return GridViewKind::ElevationAlongY;
    return GridViewKind::Axonometric;
}

std::vector<PlacedGridLabel> layoutCartesianLabels(const CartesianGrid& grid, const GridLabelView& view)
{
    std::vector<PlacedGridLabel> out;
    const GridDefinition& def = grid.definition();
    const auto& xs = def.xPositions();
    const auto& ys = def.yPositions();
    if (xs.empty() || ys.empty()) return out; // même condition que CartesianGrid::computeGeometry

    std::vector<double> levels = def.zLevels();
    if (levels.empty()) levels.push_back(0.0);
    std::vector<double> globalZ;
    for (double z : levels) globalZ.push_back(def.origin().Z() + z);

    const double ext = grid.effectiveExtension();
    const auto [minX, maxX] = std::minmax_element(xs.begin(), xs.end());
    const auto [minY, maxY] = std::minmax_element(ys.begin(), ys.end());
    const auto [minZ, maxZ] = std::minmax_element(levels.begin(), levels.end());
    const bool both = def.displaySettings().labelsBothEnds;

    auto addX = [&](std::size_t i, double lx, double ly, double lz, const gp_Dir& n) {
        out.push_back({ grid.localToWorld(lx, ly, lz), def.getXLabel(i), n, def.xIsBold(i), false, 'X', static_cast<int>(i) });
    };
    auto addY = [&](std::size_t j, double lx, double ly, double lz, const gp_Dir& n) {
        out.push_back({ grid.localToWorld(lx, ly, lz), def.getYLabel(j), n, def.yIsBold(j), false, 'Y', static_cast<int>(j) });
    };
    const gp_Dir up(0.0, 0.0, 1.0);

    switch (view.kind)
    {
    case GridViewKind::Plan:
    {
        const double z = levels[chooseLevel(globalZ, view)];
        for (std::size_t i = 0; i < xs.size(); ++i)
        {
            addX(i, xs[i], *minY - ext, z, up);
            if (both) addX(i, xs[i], *maxY + ext, z, up);
        }
        for (std::size_t j = 0; j < ys.size(); ++j)
        {
            addY(j, *minX - ext, ys[j], z, up);
            if (both) addY(j, *maxX + ext, ys[j], z, up);
        }
        break;
    }
    case GridViewKind::ElevationAlongY:
    {
        // Les axes X se projettent en verticales : repère au-dessus du dernier niveau.
        const gp_Dir n = grid.localDirToWorld(0.0, 1.0);
        for (std::size_t i = 0; i < xs.size(); ++i)
        {
            addX(i, xs[i], *minY - ext, *maxZ + ext, n);
            if (both) addX(i, xs[i], *minY - ext, *minZ - ext, n);
        }
        break;
    }
    case GridViewKind::ElevationAlongX:
    {
        const gp_Dir n = grid.localDirToWorld(1.0, 0.0);
        for (std::size_t j = 0; j < ys.size(); ++j)
        {
            addY(j, *minX - ext, ys[j], *maxZ + ext, n);
            if (both) addY(j, *minX - ext, ys[j], *minZ - ext, n);
        }
        break;
    }
    case GridViewKind::Axonometric:
    {
        // 3D : une seule occurrence, au niveau le plus bas (pas de copie par étage).
        for (std::size_t i = 0; i < xs.size(); ++i) addX(i, xs[i], *minY - ext, *minZ, up);
        for (std::size_t j = 0; j < ys.size(); ++j) addY(j, *minX - ext, ys[j], *minZ, up);
        break;
    }
    }

    // Colonne des niveaux : une étiquette par niveau, inutile (et empilée) en vue en plan.
    if (view.kind != GridViewKind::Plan)
    {
        int k = 0;
        for (const auto& a : grid.levelLabelAnchors())
            out.push_back({ a.position, a.text, up, a.isBold, true, 'Z', k++ });
    }
    return out;
}

std::vector<PlacedGridLabel> layoutCylindricalLabels(const CylindricalGrid& grid, const GridLabelView& view)
{
    std::vector<PlacedGridLabel> out;
    const auto& anchors = grid.labelAnchors();
    if (anchors.empty()) return out;

    // Les ancres sont générées à chaque niveau : on n'en garde qu'un (actif en plan, sinon le plus bas).
    std::vector<double> zs;
    for (const auto& a : anchors)
        if (std::none_of(zs.begin(), zs.end(), [&](double z) { return std::abs(z - a.position.Z()) < 1e-6; }))
            zs.push_back(a.position.Z());
    const double z = zs[chooseLevel(zs, view)];

    int index = 0;
    for (const auto& a : anchors)
    {
        if (std::abs(a.position.Z() - z) >= 1e-6) continue;
        out.push_back({ a.position, a.text, gp_Dir(0.0, 0.0, 1.0), false, false, a.isAngle ? 'A' : 'R', index++ });
    }
    return out;
}

} // namespace TSA::Grid
