#include "GridSnapManager.h"
#include "GridManager.h"
#include "../Model/Model.h"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace TSA::Grid
{

GridSnapManager::GridSnapManager()
    : m_snapEnabled(true)
    , m_activeModes(SnapMode::All)
    , m_snapTolerance(0.50)
    , m_refPoint(0.0, 0.0, 0.0)
    , m_hasRefPoint(false)
{
}

void GridSnapManager::setModeActive(SnapMode mode, bool active)
{
    if (active)
        m_activeModes = m_activeModes | mode;
    else
        m_activeModes = m_activeModes & ~mode;
}

GridSnapResult GridSnapManager::findObjectSnap(const gp_Pnt& rawPoint, const TSA::Model::Model& model) const
{
    GridSnapResult bestResult;
    bestResult.snapped = false;
    bestResult.distance = m_snapTolerance;
    bestResult.point = rawPoint;

    // 1. Nœuds existants du modèle
    if (isModeActive(SnapMode::Node))
    {
        for (const auto& [nodeId, node] : model.nodes())
        {
            gp_Pnt nodePnt(node.x(), node.y(), node.z());
            double dist = rawPoint.Distance(nodePnt);
            if (dist <= m_snapTolerance && dist < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = nodePnt;
                bestResult.type = GridSnapType::Node;
                bestResult.distance = dist;
                bestResult.targetEntityId = nodeId;

                std::ostringstream oss;
                oss << "Nœud N" << nodeId << " ("
                    << std::fixed << std::setprecision(3)
                    << node.x() << ", " << node.y() << ", " << node.z() << " m)";
                bestResult.description = oss.str();
            }
        }
    }

    // Helper pour récupérer les coordonnées d'un nœud
    auto getNodePnt = [&](int nid) -> std::pair<bool, gp_Pnt> {
        const auto* n = model.getNode(nid);
        if (n) return { true, gp_Pnt(n->x(), n->y(), n->z()) };
        return { false, gp_Pnt(0.0, 0.0, 0.0) };
    };

    // Helper pour tester segments linéaires (Beams, Columns, Cables, TrussMembers, Walls)
    struct LinearSeg {
        std::string elemType;
        int elemId;
        gp_Pnt p1;
        gp_Pnt p2;
    };
    std::vector<LinearSeg> segments;
    segments.reserve(model.beams().size() + model.columns().size() + model.cables().size() + model.trussMembers().size() + model.walls().size());

    for (const auto& [bid, b] : model.beams()) {
        auto [ok1, p1] = getNodePnt(b.startNodeId());
        auto [ok2, p2] = getNodePnt(b.endNodeId());
        if (ok1 && ok2) segments.push_back({ "Poutre B", bid, p1, p2 });
    }
    for (const auto& [cid, c] : model.columns()) {
        auto [ok1, p1] = getNodePnt(c.startNodeId());
        auto [ok2, p2] = getNodePnt(c.endNodeId());
        if (ok1 && ok2) segments.push_back({ "Poteau C", cid, p1, p2 });
    }
    for (const auto& [cid, cab] : model.cables()) {
        auto [ok1, p1] = getNodePnt(cab.startNodeId());
        auto [ok2, p2] = getNodePnt(cab.endNodeId());
        if (ok1 && ok2) segments.push_back({ "Câble C", cid, p1, p2 });
    }
    for (const auto& [tid, tr] : model.trussMembers()) {
        auto [ok1, p1] = getNodePnt(tr.startNodeId());
        auto [ok2, p2] = getNodePnt(tr.endNodeId());
        if (ok1 && ok2) segments.push_back({ "Treillis T", tid, p1, p2 });
    }
    for (const auto& [wid, w] : model.walls()) {
        auto [ok1, p1] = getNodePnt(w.startNodeId());
        auto [ok2, p2] = getNodePnt(w.endNodeId());
        if (ok1 && ok2) segments.push_back({ "Voile W", wid, p1, p2 });
    }

    // 2. Extrémités (Endpoints)
    if (isModeActive(SnapMode::Endpoint))
    {
        for (const auto& seg : segments)
        {
            double d1 = rawPoint.Distance(seg.p1);
            if (d1 <= m_snapTolerance && d1 < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = seg.p1;
                bestResult.type = GridSnapType::Endpoint;
                bestResult.distance = d1;
                bestResult.targetEntityId = seg.elemId;
                bestResult.description = "Extrémité " + seg.elemType + std::to_string(seg.elemId);
            }
            double d2 = rawPoint.Distance(seg.p2);
            if (d2 <= m_snapTolerance && d2 < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = seg.p2;
                bestResult.type = GridSnapType::Endpoint;
                bestResult.distance = d2;
                bestResult.targetEntityId = seg.elemId;
                bestResult.description = "Extrémité " + seg.elemType + std::to_string(seg.elemId);
            }
        }
    }

    // 3. Milieux (Midpoints)
    if (isModeActive(SnapMode::Midpoint))
    {
        for (const auto& seg : segments)
        {
            gp_Pnt midPnt((seg.p1.X() + seg.p2.X()) * 0.5,
                          (seg.p1.Y() + seg.p2.Y()) * 0.5,
                          (seg.p1.Z() + seg.p2.Z()) * 0.5);
            double d = rawPoint.Distance(midPnt);
            if (d <= m_snapTolerance && d < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = midPnt;
                bestResult.type = GridSnapType::Midpoint;
                bestResult.distance = d;
                bestResult.targetEntityId = seg.elemId;
                bestResult.description = "Milieu " + seg.elemType + std::to_string(seg.elemId);
            }
        }
    }

    // 4. Centres (Centers) : Centroïdes de Dalles & Fondations
    if (isModeActive(SnapMode::Center))
    {
        for (const auto& [sid, slab] : model.slabs())
        {
            if (slab.nodeIds().size() >= 3)
            {
                double sx = 0.0, sy = 0.0, sz = 0.0;
                int validCount = 0;
                for (int nid : slab.nodeIds())
                {
                    auto [ok, p] = getNodePnt(nid);
                    if (ok) { sx += p.X(); sy += p.Y(); sz += p.Z(); validCount++; }
                }
                if (validCount >= 3)
                {
                    gp_Pnt centerPnt(sx / validCount, sy / validCount, sz / validCount);
                    double d = rawPoint.Distance(centerPnt);
                    if (d <= m_snapTolerance && d < bestResult.distance)
                    {
                        bestResult.snapped = true;
                        bestResult.point = centerPnt;
                        bestResult.type = GridSnapType::Center;
                        bestResult.distance = d;
                        bestResult.targetEntityId = sid;
                        bestResult.description = "Centre Dalle S" + std::to_string(sid);
                    }
                }
            }
        }

        for (const auto& [fid, fnd] : model.foundations())
        {
            auto [ok, p] = getNodePnt(fnd.nodeId());
            if (ok)
            {
                double d = rawPoint.Distance(p);
                if (d <= m_snapTolerance && d < bestResult.distance)
                {
                    bestResult.snapped = true;
                    bestResult.point = p;
                    bestResult.type = GridSnapType::Center;
                    bestResult.distance = d;
                    bestResult.targetEntityId = fid;
                    bestResult.description = "Centre Semelle F" + std::to_string(fid);
                }
            }
        }
    }

    // 5. Perpendiculaire (Perpendicular) : projection orthogonale depuis m_refPoint
    if (isModeActive(SnapMode::Perpendicular) && m_hasRefPoint)
    {
        for (const auto& seg : segments)
        {
            gp_Vec AB(seg.p1, seg.p2);
            double L2 = AB.SquareMagnitude();
            if (L2 > 1e-8)
            {
                gp_Vec AP(seg.p1, m_refPoint);
                double t = AP.Dot(AB) / L2;
                if (t >= 0.0 && t <= 1.0)
                {
                    gp_Pnt perpPnt = seg.p1.Translated(AB * t);
                    double d = rawPoint.Distance(perpPnt);
                    if (d <= m_snapTolerance && d < bestResult.distance)
                    {
                        bestResult.snapped = true;
                        bestResult.point = perpPnt;
                        bestResult.type = GridSnapType::Perpendicular;
                        bestResult.distance = d;
                        bestResult.targetEntityId = seg.elemId;
                        bestResult.description = "Perpendiculaire à " + seg.elemType + std::to_string(seg.elemId);
                    }
                }
            }
        }
    }

    // 6. Le plus proche (Nearest) : projection glissante sur l'axe (priorité secondaire après les accrochages discrets)
    if (isModeActive(SnapMode::Nearest) && !bestResult.snapped)
    {
        for (const auto& seg : segments)
        {
            gp_Vec AB(seg.p1, seg.p2);
            double L2 = AB.SquareMagnitude();
            if (L2 > 1e-8)
            {
                gp_Vec AP(seg.p1, rawPoint);
                double t = AP.Dot(AB) / L2;
                // Exclure les extrémités immédiates (déjà gérées par Endpoint)
                if (t >= 0.05 && t <= 0.95)
                {
                    gp_Pnt nearPnt = seg.p1.Translated(AB * t);
                    double d = rawPoint.Distance(nearPnt);
                    if (d <= m_snapTolerance && d < bestResult.distance)
                    {
                        bestResult.snapped = true;
                        bestResult.point = nearPnt;
                        bestResult.type = GridSnapType::Nearest;
                        bestResult.distance = d;
                        bestResult.targetEntityId = seg.elemId;
                        bestResult.description = "Sur axe de " + seg.elemType + std::to_string(seg.elemId);
                    }
                }
            }
        }
    }

    return bestResult;
}

GridSnapResult GridSnapManager::findSnap(const gp_Pnt& rawPoint,
                                        const GridManager* gridManager,
                                        const TSA::Model::Model* model) const
{
    if (!m_snapEnabled)
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };
    }

    // 1. Priorité N°1 : Object Snaps (Nœuds, extrémités, milieux, centres, etc.)
    if (model)
    {
        GridSnapResult objSnap = findObjectSnap(rawPoint, *model);
        if (objSnap.snapped)
        {
            return objSnap;
        }
    }

    if (!gridManager || !isModeActive(SnapMode::Grid))
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };
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

    // 3. Priorité N°3 : Autre grille visible
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

    return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };
}

GridSnapResult GridSnapManager::findSnap(const gp_Pnt& rawPoint,
                                        const GridSystem* activeGrid,
                                        const TSA::Model::Model* model) const
{
    if (!m_snapEnabled)
    {
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };
    }

    if (model)
    {
        GridSnapResult objSnap = findObjectSnap(rawPoint, *model);
        if (objSnap.snapped)
        {
            return objSnap;
        }
    }

    if (activeGrid && activeGrid->isVisible() && isModeActive(SnapMode::Grid))
    {
        return activeGrid->findClosestSnap(rawPoint, m_snapTolerance);
    }

    return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };
}

GridSnapResult GridSnapManager::findObjectSnap(const gp_Pnt& rawPoint, const TSA::Model::Model* model, double snapTol) const
{
    if (!model)
        return GridSnapResult{ false, rawPoint, GridSnapType::None, 0.0, "", -1 };

    double oldTol = m_snapTolerance;
    if (snapTol > 0.0)
    {
        const_cast<GridSnapManager*>(this)->m_snapTolerance = snapTol;
    }
    auto res = findObjectSnap(rawPoint, *model);
    if (snapTol > 0.0)
    {
        const_cast<GridSnapManager*>(this)->m_snapTolerance = oldTol;
    }
    return res;
}

} // namespace TSA::Grid
