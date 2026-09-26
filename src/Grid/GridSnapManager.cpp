#include "GridSnapManager.h"
#include "GridManager.h"
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
                                        const GridManager* gridManager,
                                        const TSA::Model::Model* model) const
{
    if (!m_snapEnabled)
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "" };
    }

    // 1. Priorité N°1 : Nœud du modèle structural
    GridSnapResult nodeSnap = findSnap(rawPoint, static_cast<const GridSystem*>(nullptr), model);
    if (nodeSnap.snapped)
    {
        return nodeSnap;
    }

    if (!gridManager)
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "" };
    }

    // 2. Priorité N°2 : Grille active
    const GridSystem* activeG = gridManager->activeGrid();
    if (activeG && activeG->isVisible())
    {
        GridSnapResult snap = activeG->findClosestSnap(rawPoint, m_snapTolerance);
        if (snap.snapped)
        {
            return snap;
        }
    }

    // 3. Priorité N°3 : Autre grille visible (s'il y en a)
    GridSnapResult bestOtherSnap;
    bestOtherSnap.snapped = false;
    bestOtherSnap.distance = m_snapTolerance;

    for (const auto& g : gridManager->grids())
    {
        if (g && g.get() != activeG && g->isVisible())
        {
            GridSnapResult snap = g->findClosestSnap(rawPoint, m_snapTolerance);
            if (snap.snapped && snap.distance < bestOtherSnap.distance)
            {
                bestOtherSnap = snap;
            }
        }
    }

    if (bestOtherSnap.snapped)
    {
        return bestOtherSnap;
    }

    return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "" };
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

    if (bestResult.snapped)
    {
        return bestResult;
    }

    // 2. Priorité N°2 : Grille active
    if (activeGrid && activeGrid->isVisible())
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
