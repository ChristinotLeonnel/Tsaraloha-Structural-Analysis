#include "SnapEngine.h"

#include "GridSystem.h"
#include "../Model/Model.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace TSA::Grid
{

using TSA::Model::ElementKind;

namespace
{
constexpr double kTieTolerancePx = 0.75;    ///< écart de score considéré comme une égalité
constexpr std::size_t kMaxNearSegments = 64; ///< segments testés pour les intersections

double dot(const gp_XYZ& a, const gp_XYZ& b) { return a.Dot(b); }

std::string coords(const gp_Pnt& p)
{
    char buf[96];
    std::snprintf(buf, sizeof(buf), "(%.3f ; %.3f ; %.3f m)", p.X(), p.Y(), p.Z());
    return buf;
}

std::string label(const char* what, int id) { return std::string(what) + std::to_string(id); }

/// Points les plus proches de deux segments [p1,q1] et [p2,q2] (paramètres s, t ∈ [0, 1]).
void closestSegmentSegment(const gp_XYZ& p1, const gp_XYZ& q1, const gp_XYZ& p2, const gp_XYZ& q2, double& s, double& t)
{
    const gp_XYZ d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    s = t = 0.0;
    if (a <= 1e-18 && e <= 1e-18) return;
    if (a <= 1e-18) { t = std::clamp(f / e, 0.0, 1.0); return; }
    const double c = dot(d1, r);
    if (e <= 1e-18) { s = std::clamp(-c / a, 0.0, 1.0); return; }
    const double b = dot(d1, d2), denom = a * e - b * b;
    s = denom > 1e-18 ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
    t = (b * s + f) / e;
    if (t < 0.0) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
    else if (t > 1.0) { t = 1.0; s = std::clamp((b - c) / a, 0.0, 1.0); }
}
} // namespace

SnapEngine::SnapEngine(const SnapQuery& query) : m_q(query), m_radius(std::max(1.0, query.radiusPx))
{
    m_nearSegments.reserve(kMaxNearSegments);
}

double SnapEngine::priorityBias(GridSnapType type, SnapSource source)
{
    switch (type)
    {
    case GridSnapType::Node:
    case GridSnapType::Endpoint: return 0.0;
    case GridSnapType::Intersection: return source == SnapSource::Model ? 1.0 : 4.0;
    case GridSnapType::Midpoint: return 2.0;
    case GridSnapType::Center:
    case GridSnapType::Perpendicular: return 3.0;
    case GridSnapType::Origin: return 4.0;
    // Suivis (seulement en l'absence de point discret dans l'ouverture)
    case GridSnapType::Nearest: return 0.0;
    case GridSnapType::AxisLine:
    case GridSnapType::RadialLine:
    case GridSnapType::Circle: return 3.0;
    case GridSnapType::Face: return 10.0;
    default: return 6.0;
    }
}

bool SnapEngine::isDiscrete(GridSnapType type)
{
    switch (type)
    {
    case GridSnapType::Node:
    case GridSnapType::Endpoint:
    case GridSnapType::Midpoint:
    case GridSnapType::Center:
    case GridSnapType::Intersection:
    case GridSnapType::Perpendicular:
    case GridSnapType::Origin: return true;
    default: return false;
    }
}

const char* SnapEngine::typeLabel(GridSnapType type, SnapSource source)
{
    switch (type)
    {
    case GridSnapType::Node: return "Nœud";
    case GridSnapType::Endpoint: return "Extrémité";
    case GridSnapType::Midpoint: return "Milieu";
    case GridSnapType::Center: return "Centre";
    case GridSnapType::Intersection: return source == SnapSource::Model ? "Intersection" : "Grille";
    case GridSnapType::Perpendicular: return "Perpendiculaire";
    case GridSnapType::Nearest: return "Proche";
    case GridSnapType::AxisLine: return "Axe de grille";
    case GridSnapType::RadialLine: return "Rayon de grille";
    case GridSnapType::Circle: return "Arc de grille";
    case GridSnapType::Origin: return "Origine";
    case GridSnapType::Face: return "Face";
    case GridSnapType::LevelPlane: return "Plan";
    default: return "Accrochage";
    }
}

std::string SnapEngine::displayLabel(const GridSnapResult& snap)
{
    const std::string type = typeLabel(snap.type, snap.source);
    std::string what = snap.description;
    if (const auto p = what.find(" ("); p != std::string::npos) what.resize(p);   // sans coordonnées
    if (what.empty() || what == type) return type;
    if (what.rfind(type, 0) == 0) return what;   // « Nœud N3 », « Grille A-2 » contiennent déjà le type
    return type + " · " + what;
}

bool SnapEngine::screenDistance(const gp_Pnt& p, double& dist) const
{
    if (m_q.acceptPoint && !m_q.acceptPoint(p)) return false;
    double sx = 0.0, sy = 0.0;
    if (!m_q.project || !m_q.project(p, sx, sy)) return false;
    dist = std::hypot(sx - m_q.cursorX, sy - m_q.cursorY);
    return dist <= m_radius;
}

void SnapEngine::consider(GridSnapResult& best, double& bestScore, GridSnapType type, SnapSource source, const gp_Pnt& p,
                          double dist, int kind, int id, const std::string& desc) const
{
    const double score = dist + priorityBias(type, source);
    const double depth = dot(p.XYZ() - m_q.rayOrigin.XYZ(), m_q.rayDir.XYZ());
    // Égalité de score : le candidat le plus proche de l'observateur l'emporte (structure dense)
    const bool better = score < bestScore - kTieTolerancePx
        || (std::abs(score - bestScore) <= kTieTolerancePx && (!best.snapped || depth < best.depth - 1e-9));
    if (!better) return;
    bestScore = score;
    best.snapped = true;
    best.point = p;
    best.type = type;
    best.source = source;
    best.targetKind = kind;
    best.targetEntityId = id;
    best.screenDistance = dist;
    best.distance = dist;
    best.depth = depth;
    best.description = desc;
}

void SnapEngine::offerPoint(GridSnapType type, SnapSource source, const gp_Pnt& p, int kind, int id, const std::string& desc)
{
    double dist = 0.0;
    if (!screenDistance(p, dist)) return;
    if (isDiscrete(type)) consider(m_bestPoint, m_bestPointScore, type, source, p, dist, kind, id, desc);
    else consider(m_bestTrack, m_bestTrackScore, type, source, p, dist, kind, id, desc);
}

bool SnapEngine::closestOnRay(const gp_Pnt& a, const gp_Pnt& b, gp_Pnt& out) const
{
    // Point du segment le plus proche de la droite de visée (exact en perspective comme en ortho)
    const gp_XYZ d1 = b.XYZ() - a.XYZ();
    const gp_XYZ d2 = m_q.rayDir.XYZ();
    const gp_XYZ r = a.XYZ() - m_q.rayOrigin.XYZ();
    const double aa = dot(d1, d1);
    if (aa < 1e-18) return false;
    const double bb = dot(d1, d2), c = dot(d1, r), f = dot(d2, r);
    const double denom = aa - bb * bb;   // |d2| = 1
    const double s = denom > 1e-12 * aa ? std::clamp((bb * f - c) / denom, 0.0, 1.0) : 0.0;
    out = gp_Pnt(a.XYZ() + d1 * s);
    return true;
}

void SnapEngine::offerSegment(GridSnapType type, SnapSource source, const gp_Pnt& a, const gp_Pnt& b, int kind, int id,
                              const std::string& desc)
{
    gp_Pnt q;
    if (!closestOnRay(a, b, q)) return;
    double dist = 0.0;
    if (!screenDistance(q, dist)) return;
    if (source == SnapSource::Model && m_nearSegments.size() < kMaxNearSegments) m_nearSegments.push_back({ a, b, kind, id });
    consider(m_bestTrack, m_bestTrackScore, type, source, q, dist, kind, id, desc);
}

void SnapEngine::collectModel(const TSA::Model::Model& m)
{
    if (!m_q.objects) return;
    const SnapMode modes = m_q.modes;
    auto accept = [this](ElementKind k, int id) { return !m_q.acceptElement || m_q.acceptElement(static_cast<int>(k), id); };
    auto nodePnt = [&m](int nid, gp_Pnt& p) {
        const auto* n = m.getNode(nid);
        if (!n) return false;
        p.SetCoord(n->x(), n->y(), n->z());
        return true;
    };

    // Nœuds
    if (hasSnapMode(modes, SnapMode::Node))
        for (const auto& [id, n] : m.nodes())
        {
            if (!accept(ElementKind::Node, id)) continue;
            const gp_Pnt p(n.x(), n.y(), n.z());
            double d = 0.0;
            if (!screenDistance(p, d)) continue;   // filtre avant toute chaîne de caractères
            consider(m_bestPoint, m_bestPointScore, GridSnapType::Node, SnapSource::Model, p, d, static_cast<int>(ElementKind::Node),
                     id, label("Nœud N", id) + " " + coords(p));
        }

    // Éléments linéaires : milieu, proche, perpendiculaire (les extrémités sont des nœuds)
    auto linear = [&](ElementKind kind, int id, int na, int nb, const char* name) {
        gp_Pnt a, b;
        if (!accept(kind, id) || !nodePnt(na, a) || !nodePnt(nb, b) || a.SquareDistance(b) < 1e-18) return;
        const int k = static_cast<int>(kind);
        double d = 0.0;
        if (hasSnapMode(modes, SnapMode::Midpoint))
        {
            const gp_Pnt mid((a.XYZ() + b.XYZ()) * 0.5);
            if (screenDistance(mid, d))
                consider(m_bestPoint, m_bestPointScore, GridSnapType::Midpoint, SnapSource::Model, mid, d, k, id, label(name, id));
        }
        if (hasSnapMode(modes, SnapMode::Perpendicular) && m_q.hasReference)
        {
            const gp_XYZ ab = b.XYZ() - a.XYZ();
            const double t = dot(m_q.reference.XYZ() - a.XYZ(), ab) / dot(ab, ab);
            if (t > 1e-6 && t < 1.0 - 1e-6)
            {
                const gp_Pnt foot(a.XYZ() + ab * t);
                if (foot.SquareDistance(m_q.reference) > 1e-12 && screenDistance(foot, d))
                    consider(m_bestPoint, m_bestPointScore, GridSnapType::Perpendicular, SnapSource::Model, foot, d, k, id, label(name, id));
            }
        }
        gp_Pnt q;
        if (closestOnRay(a, b, q) && screenDistance(q, d))
        {
            if (m_nearSegments.size() < kMaxNearSegments) m_nearSegments.push_back({ a, b, k, id });
            if (hasSnapMode(modes, SnapMode::Nearest))
                consider(m_bestTrack, m_bestTrackScore, GridSnapType::Nearest, SnapSource::Model, q, d, k, id, label(name, id));
        }
    };
    for (const auto& [id, e] : m.beams()) linear(ElementKind::Beam, id, e.startNodeId(), e.endNodeId(), "Poutre B");
    for (const auto& [id, e] : m.columns()) linear(ElementKind::Column, id, e.startNodeId(), e.endNodeId(), "Poteau C");
    for (const auto& [id, e] : m.trussMembers()) linear(ElementKind::TrussMember, id, e.startNodeId(), e.endNodeId(), "Treillis T");
    for (const auto& [id, e] : m.cables()) linear(ElementKind::Cable, id, e.startNodeId(), e.endNodeId(), "Câble K");

    // Faces (dalles, voiles) : arêtes, coins, centre, point de la face sous le curseur
    auto surface = [&](ElementKind kind, int id, const std::vector<gp_Pnt>& poly, bool cornersAreNodes, const char* name) {
        if (poly.size() < 3 || !accept(kind, id)) return;
        const int k = static_cast<int>(kind);
        double d = 0.0;
        gp_XYZ centre(0, 0, 0), normal(0, 0, 0);
        for (std::size_t i = 0; i < poly.size(); ++i)
        {
            const gp_XYZ& p = poly[i].XYZ();
            const gp_XYZ& q = poly[(i + 1) % poly.size()].XYZ();
            centre += p;
            normal += gp_XYZ((p.Y() - q.Y()) * (p.Z() + q.Z()), (p.Z() - q.Z()) * (p.X() + q.X()), (p.X() - q.X()) * (p.Y() + q.Y()));
            if (!cornersAreNodes && hasSnapMode(modes, SnapMode::Endpoint) && screenDistance(poly[i], d))
                consider(m_bestPoint, m_bestPointScore, GridSnapType::Endpoint, SnapSource::Model, poly[i], d, k, id, label(name, id));
            const gp_Pnt mid((p + q) * 0.5);
            if (hasSnapMode(modes, SnapMode::Midpoint) && screenDistance(mid, d))
                consider(m_bestPoint, m_bestPointScore, GridSnapType::Midpoint, SnapSource::Model, mid, d, k, id, label(name, id));
            gp_Pnt on;
            if (hasSnapMode(modes, SnapMode::Nearest) && closestOnRay(poly[i], poly[(i + 1) % poly.size()], on) && screenDistance(on, d))
                consider(m_bestTrack, m_bestTrackScore, GridSnapType::Nearest, SnapSource::Model, on, d, k, id, label(name, id));
        }
        centre /= static_cast<double>(poly.size());
        if (hasSnapMode(modes, SnapMode::Center) && screenDistance(gp_Pnt(centre), d))
            consider(m_bestPoint, m_bestPointScore, GridSnapType::Center, SnapSource::Model, gp_Pnt(centre), d, k, id, label(name, id));

        // Face : intersection rayon / plan, puis test d'appartenance au polygone (plan dominant)
        const double nn = normal.Modulus();
        if (!hasSnapMode(modes, SnapMode::Face) || nn < 1e-12) return;
        normal /= nn;
        const double denom = dot(normal, m_q.rayDir.XYZ());
        if (std::abs(denom) < 1e-9) return;
        const double t = dot(normal, poly[0].XYZ() - m_q.rayOrigin.XYZ()) / denom;
        const gp_XYZ hit = m_q.rayOrigin.XYZ() + m_q.rayDir.XYZ() * t;
        const int drop = std::abs(normal.X()) > std::abs(normal.Y()) ? (std::abs(normal.X()) > std::abs(normal.Z()) ? 0 : 2)
                                                                     : (std::abs(normal.Y()) > std::abs(normal.Z()) ? 1 : 2);
        auto u = [drop](const gp_XYZ& v) { return drop == 0 ? v.Y() : v.X(); };
        auto w = [drop](const gp_XYZ& v) { return drop == 2 ? v.Y() : v.Z(); };
        bool inside = false;
        for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        {
            const gp_XYZ& pi = poly[i].XYZ();
            const gp_XYZ& pj = poly[j].XYZ();
            if ((w(pi) > w(hit)) != (w(pj) > w(hit)) && u(hit) < (u(pj) - u(pi)) * (w(hit) - w(pi)) / (w(pj) - w(pi)) + u(pi))
                inside = !inside;
        }
        if (inside && screenDistance(gp_Pnt(hit), d))
            consider(m_bestTrack, m_bestTrackScore, GridSnapType::Face, SnapSource::Model, gp_Pnt(hit), d, k, id, label(name, id));
    };
    std::vector<gp_Pnt> poly;
    poly.reserve(8);
    for (const auto& [id, s] : m.slabs())
    {
        poly.clear();
        gp_Pnt p;
        for (int nid : s.nodeIds())
            if (nodePnt(nid, p)) poly.push_back(p);
        surface(ElementKind::Slab, id, poly, true, "Dalle S");
    }
    for (const auto& [id, wall] : m.walls())
    {
        gp_Pnt a, b;
        if (!nodePnt(wall.startNodeId(), a) || !nodePnt(wall.endNodeId(), b)) continue;
        // Plan moyen du voile tel qu'il est dessiné (WallGeometry : décalage selon la normale horizontale)
        const gp_XYZ along(b.X() - a.X(), b.Y() - a.Y(), 0.0);
        if (along.Modulus() > 1e-9 && std::abs(wall.offset()) > 1e-12)
        {
            const gp_XYZ shift = gp_XYZ(-along.Y(), along.X(), 0.0) / along.Modulus() * wall.offset();
            a.SetXYZ(a.XYZ() + shift);
            b.SetXYZ(b.XYZ() + shift);
        }
        const gp_XYZ up(0.0, 0.0, wall.height());
        poly.assign({ a, b, gp_Pnt(b.XYZ() + up), gp_Pnt(a.XYZ() + up) });
        // Les coins bas sont des nœuds (déjà proposés) ; les coins hauts sont des extrémités
        surface(ElementKind::Wall, id, poly, false, "Voile W");
    }

    if (hasSnapMode(modes, SnapMode::Intersection)) collectIntersections();
}

void SnapEngine::collectIntersections()
{
    // Intersections réelles (3D) entre éléments proches du curseur, hors nœud commun
    for (std::size_t i = 0; i < m_nearSegments.size(); ++i)
        for (std::size_t j = i + 1; j < m_nearSegments.size(); ++j)
        {
            const auto& s1 = m_nearSegments[i];
            const auto& s2 = m_nearSegments[j];
            if (s1.kind == s2.kind && s1.id == s2.id) continue;
            double s = 0.0, t = 0.0;
            closestSegmentSegment(s1.a.XYZ(), s1.b.XYZ(), s2.a.XYZ(), s2.b.XYZ(), s, t);
            const gp_XYZ p1 = s1.a.XYZ() + (s1.b.XYZ() - s1.a.XYZ()) * s;
            const gp_XYZ p2 = s2.a.XYZ() + (s2.b.XYZ() - s2.a.XYZ()) * t;
            const double scale = std::max({ 1.0, (s1.b.XYZ() - s1.a.XYZ()).Modulus(), (s2.b.XYZ() - s2.a.XYZ()).Modulus() });
            if ((p1 - p2).Modulus() > 1e-6 * scale) continue;
            const bool end1 = s < 1e-9 || s > 1.0 - 1e-9;
            const bool end2 = t < 1e-9 || t > 1.0 - 1e-9;
            if (end1 && end2) continue;   // nœud partagé : déjà proposé comme nœud
            const gp_Pnt x((p1 + p2) * 0.5);
            double d = 0.0;
            if (screenDistance(x, d))
                consider(m_bestPoint, m_bestPointScore, GridSnapType::Intersection, SnapSource::Model, x, d, s1.kind, s1.id,
                         "Intersection " + std::to_string(s1.id) + " × " + std::to_string(s2.id) + " " + coords(x));
        }
}

void SnapEngine::collectGrids(const std::vector<const GridSystem*>& grids)
{
    if (!m_q.grids || !hasSnapMode(m_q.modes, SnapMode::Grid)) return;
    double d = 0.0;
    for (const auto* grid : grids)
    {
        if (!grid) continue;
        if (const auto* cart = grid->cartesian())
        {
            for (const auto& inter : cart->intersections())
                if (screenDistance(inter.point, d))
                    consider(m_bestPoint, m_bestPointScore, GridSnapType::Intersection, SnapSource::Grid, inter.point, d, -1, -1,
                             "Grille " + inter.labelX + "-" + inter.labelY + " " + coords(inter.point));
            for (const auto& line : cart->allLines())
            {
                gp_Pnt q;
                if (closestOnRay(line.start, line.end, q) && screenDistance(q, d))
                    consider(m_bestTrack, m_bestTrackScore, GridSnapType::AxisLine, SnapSource::Grid, q, d, -1, -1,
                             std::string("Axe ") + (line.isXAxis ? "X " : "Y ") + line.label);
            }
        }
        else if (const auto* cyl = grid->cylindrical())
        {
            const gp_Pnt& orig = grid->definition().origin();
            const auto& zLevels = grid->definition().zLevels();
            const std::vector<double> levels = zLevels.empty() ? std::vector<double> { 0.0 } : zLevels;
            for (double z : levels)
            {
                const gp_Pnt c(orig.X(), orig.Y(), orig.Z() + z);
                if (screenDistance(c, d))
                    consider(m_bestPoint, m_bestPointScore, GridSnapType::Origin, SnapSource::Grid, c, d, -1, -1, "Centre de grille " + coords(c));
            }
            for (const auto& inter : cyl->intersections())
                if (screenDistance(inter.point, d))
                {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "Grille R=%.2f m, %.1f°", inter.radius, inter.angleDeg);
                    consider(m_bestPoint, m_bestPointScore, GridSnapType::Intersection, SnapSource::Grid, inter.point, d, -1, -1, buf);
                }
            for (const auto& rad : cyl->radialLines())
            {
                gp_Pnt q;
                if (closestOnRay(rad.start, rad.end, q) && screenDistance(q, d))
                    consider(m_bestTrack, m_bestTrackScore, GridSnapType::RadialLine, SnapSource::Grid, q, d, -1, -1, "Rayon " + rad.label);
            }
            // Arcs : intersection du rayon de visée avec le plan horizontal de l'arc
            const double rot = grid->definition().rotationDeg();
            for (const auto& circ : cyl->circles())
            {
                if (circ.radius <= 1e-4 || std::abs(m_q.rayDir.Z()) < 1e-6) continue;
                const double t = (circ.zLevel - m_q.rayOrigin.Z()) / m_q.rayDir.Z();
                const gp_XYZ h = m_q.rayOrigin.XYZ() + m_q.rayDir.XYZ() * t;
                const double dx = h.X() - circ.center.X(), dy = h.Y() - circ.center.Y();
                if (std::hypot(dx, dy) < 1e-4) continue;
                double ang = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846 - rot;
                ang = std::fmod(std::fmod(ang, 360.0) + 360.0, 360.0);
                if (!circ.isFullCircle())
                {
                    const double start = std::fmod(std::fmod(circ.startAngleDeg, 360.0) + 360.0, 360.0);
                    const double delta = std::fmod(ang - start + 360.0, 360.0);
                    if (delta > circ.totalAngleDeg + 1e-4)
                        ang = (360.0 - delta < delta - circ.totalAngleDeg) ? circ.startAngleDeg : circ.startAngleDeg + circ.totalAngleDeg;
                }
                const gp_Pnt p = cyl->polarToWorld(circ.radius, ang, circ.zLevel - orig.Z());
                if (screenDistance(p, d))
                    consider(m_bestTrack, m_bestTrackScore, GridSnapType::Circle, SnapSource::Grid, p, d, -1, -1, "Arc " + circ.label);
            }
        }
        else if (const auto* arb = grid->arbitrary())
        {
            for (const auto& p : arb->intersections())
                if (screenDistance(p, d))
                    consider(m_bestPoint, m_bestPointScore, GridSnapType::Intersection, SnapSource::Grid, p, d, -1, -1, "Grille " + coords(p));
        }
    }
}

GridSnapResult SnapEngine::result() const
{
    // Un point discret dans l'ouverture l'emporte toujours sur un suivi (axe, face, arc)
    if (m_bestPoint.snapped) return m_bestPoint;
    return m_bestTrack;
}

} // namespace TSA::Grid
