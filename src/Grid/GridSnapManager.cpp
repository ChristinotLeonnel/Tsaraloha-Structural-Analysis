#include "GridSnapManager.h"
#include "../Model/Model.h"

#include <sstream>
#include <iomanip>

namespace TSA::Grid
{

GridSnapManager::GridSnapManager()
    : m_snapEnabled(true)
    , m_snapTolerance(0.50)
{
}

GridSnapResult GridSnapManager::findSnap(const gp_Pnt& rawPoint,
                                        const GridSystem* activeGrid,
                                        const TSA::Model::Model* model) const
{
    if (!m_snapEnabled)
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "" };
    }

    GridSnapResult bestResult;
    bestResult.snapped = false;
    bestResult.distance = m_snapTolerance;
    bestResult.point = rawPoint;

    // 1. Priorité N°1 absolue : Nœuds structuraux existants du modèle
    if (model)
    {
        for (const auto& [nodeId, node] : model->nodes())
        {
            gp_Pnt nodePnt(node.x(), node.y(), node.z());
            double dist = rawPoint.Distance(nodePnt);
            if (dist <= m_snapTolerance && dist < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = nodePnt;
                bestResult.type = GridSnapType::Node;
                bestResult.distance = dist;

                std::ostringstream oss;
                oss << "Nœud N" << nodeId << " ("
                    << std::fixed << std::setprecision(3)
                    << node.x() << ", " << node.y() << ", " << node.z() << " m)";
                bestResult.description = oss.str();
            }
        }
    }

    // Si un nœud a été détecté à proximité immédiate, on le retourne en priorité
    if (bestResult.snapped)
    {
        return bestResult;
    }

    // 2. Priorité N°2 : Grille active (Intersections, Axes, Cercles, Origine)
    if (activeGrid && activeGrid->isActive() && activeGrid->isVisible())
    {
        GridSnapResult gridSnap = activeGrid->findClosestSnap(rawPoint, m_snapTolerance);
        if (gridSnap.snapped)
        {
            return gridSnap;
        }
    }

    return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "" };
}

} // namespace TSA::Grid
