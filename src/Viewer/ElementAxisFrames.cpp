#include "ElementAxisFrames.h"

#include "../Coordinate/CoordinateTransformationService.h"
#include "../Mesh/MeshGeometry.h"
#include "../Model/Model.h"

#include <gp_Ax3.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>

namespace TSA::Viewer
{

namespace
{
gp_Dir toDir(const TSA::Mesh::P3& p)
{
    return gp_Dir(p.x, p.y, p.z);
}

/// Repère de surface : axes du calcul, origine au centre du rectangle englobant dans le plan.
bool surfaceFrame(const TSA::Mesh::PlaneFrame& plane, const std::vector<TSA::Mesh::P3>& outer, ElementAxisFrame* out)
{
    if (outer.empty() || !std::isfinite(plane.N.x + plane.N.y + plane.N.z)) return false;
    const auto d = plane.N.dot(plane.N) * plane.U.dot(plane.U) * plane.V.dot(plane.V);
    if (d < 1e-12) return false;
    double uMin = std::numeric_limits<double>::max(), vMin = uMin, uMax = -uMin, vMax = -uMin;
    for (const auto& p : outer)
    {
        double u = 0.0, v = 0.0;
        plane.toLocal(p, &u, &v);
        uMin = std::min(uMin, u), uMax = std::max(uMax, u);
        vMin = std::min(vMin, v), vMax = std::max(vMax, v);
    }
    const TSA::Mesh::P3 c = plane.toGlobal(0.5 * (uMin + uMax), 0.5 * (vMin + vMax));
    out->origin = out->start = out->end = gp_Pnt(c.x, c.y, c.z);
    out->x = toDir(plane.U);
    out->y = toDir(plane.V);
    out->z = toDir(plane.N);
    return true;
}
} // namespace

bool barAxisFrame(const gp_Pnt& start, const gp_Pnt& end, double rotationDeg, ElementAxisFrame* out)
{
    if (start.Distance(end) <= 1e-4) return false;
    const gp_Ax3 frame = TSA::Coordinate::CoordinateTransformationService::computeElementLocalFrame(start, end, rotationDeg);
    out->start = start;
    out->end = end;
    out->origin = gp_Pnt(0.5 * (start.XYZ() + end.XYZ()));
    out->x = frame.XDirection();
    out->y = frame.YDirection();
    out->z = frame.Direction();
    return true;
}

std::vector<ElementAxisFrame> elementAxisFrames(const TSA::Model::Model& model, const TSA::Model::ElementSet& selection, bool* tooMany)
{
    using Kind = ElementAxisFrame::Kind;
    std::vector<ElementAxisFrame> frames;
    const std::size_t requested = selection.beams.size() + selection.columns.size() + selection.trussMembers.size() + selection.cables.size() +
                                  selection.slabs.size() + selection.walls.size();
    if (tooMany) *tooMany = requested > kMaxElementAxisFrames;
    if (requested > kMaxElementAxisFrames) return frames;
    frames.reserve(requested);

    auto addBar = [&](Kind kind, int id, int startNode, int endNode, double rotationDeg, bool transverse) {
        const auto* a = model.getNode(startNode);
        const auto* b = model.getNode(endNode);
        if (!a || !b) return;
        ElementAxisFrame f;
        f.kind = kind;
        f.id = id;
        f.transverseAxes = transverse;
        if (barAxisFrame(gp_Pnt(a->x(), a->y(), a->z()), gp_Pnt(b->x(), b->y(), b->z()), rotationDeg, &f)) frames.push_back(f);
    };

    for (int id : selection.beams)
        if (const auto* e = model.getBeam(id)) addBar(Kind::Beam, id, e->startNodeId(), e->endNodeId(), e->rotation(), true);
    for (int id : selection.columns)
        if (const auto* e = model.getColumn(id)) addBar(Kind::Column, id, e->startNodeId(), e->endNodeId(), e->rotation(), true);
    // Rotation nulle : même convention que OpenSeesModelMap pour les barres qui n'ont pas de section orientée.
    for (int id : selection.trussMembers)
        if (const auto* e = model.getTrussMember(id)) addBar(Kind::Truss, id, e->startNodeId(), e->endNodeId(), 0.0, false);
    for (int id : selection.cables)
        if (const auto* e = model.getCable(id)) addBar(Kind::Cable, id, e->startNodeId(), e->endNodeId(), 0.0, false);

    for (int id : selection.slabs)
    {
        const auto* s = model.getSlab(id);
        if (!s) continue;
        std::vector<TSA::Mesh::P3> corners;
        for (int nodeId : s->nodeIds())
            if (const auto* n = model.getNode(nodeId)) corners.push_back({ n->x(), n->y(), n->z() });
        if (corners.size() < 3) continue;
        ElementAxisFrame f;
        f.kind = Kind::Slab;
        f.id = id;
        if (surfaceFrame(TSA::Mesh::slabFrame(corners), corners, &f)) frames.push_back(f);
    }
    for (int id : selection.walls)
    {
        const auto* w = model.getWall(id);
        TSA::Mesh::SurfaceGeom geometry;
        if (!w || !TSA::Mesh::wallSurface(model, *w, &geometry)) continue;
        ElementAxisFrame f;
        f.kind = Kind::Wall;
        f.id = id;
        if (surfaceFrame(geometry.frame, geometry.outer, &f)) frames.push_back(f);
    }
    return frames;
}

gp_Pnt axisAnchorOnElement(const ElementAxisFrame& frame, const gp_Lin& ray)
{
    const gp_Vec u(ray.Direction());
    if (frame.surface())
    {
        const gp_Vec n(frame.z);
        const double denominator = u.Dot(n);
        if (std::abs(denominator) < 1e-9) return frame.origin;
        const double t = gp_Vec(ray.Location(), frame.origin).Dot(n) / denominator;
        return ray.Location().Translated(u * t);
    }
    // Point de la barre le plus proche du rayon : deux droites, paramètre s de la barre borné à [0, 1].
    const gp_Vec d(frame.start, frame.end);
    const gp_Vec w(ray.Location(), frame.start);
    const double a = d.Dot(d), b = d.Dot(u);
    const double denominator = a - b * b;
    if (denominator < 1e-9 * a) return frame.origin;
    const double s = std::clamp((b * u.Dot(w) - d.Dot(w)) / denominator, 0.0, 1.0);
    return frame.start.Translated(d * s);
}

} // namespace TSA::Viewer
