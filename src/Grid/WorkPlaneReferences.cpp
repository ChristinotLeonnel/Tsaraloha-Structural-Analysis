#include "WorkPlaneReferences.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <set>

namespace TSA::Grid
{

namespace
{
struct Local
{
    InPlaneIntersection inter;
    double w = 0.0;
    std::string labelA, labelB, labelZ;   // libellés par famille d'axes (X / rayon, Y / angle, niveau)
};

std::vector<Local> localIntersections(const std::vector<const GridSystem*>& grids, const TSA::Coordinate::WorkPlane& wp)
{
    const gp_Pnt o = wp.origin();
    const gp_Vec U(wp.xDirection()), V(wp.yDirection()), N(wp.normal());
    std::vector<Local> out;
    auto add = [&](const gp_Pnt& p, std::string a, std::string b, std::string z) {
        Local l;
        const gp_Vec d(o, p);
        l.inter.point = p;
        l.inter.u = d.Dot(U);
        l.inter.v = d.Dot(V);
        l.w = d.Dot(N);
        std::string label = a;
        for (const std::string* s : { &b, &z })
            if (!s->empty()) label += (label.empty() ? "" : " / ") + *s;
        l.inter.label = label;
        l.labelA = std::move(a);
        l.labelB = std::move(b);
        l.labelZ = std::move(z);
        out.push_back(std::move(l));
    };
    for (const GridSystem* g : grids)
    {
        if (!g) continue;
        if (const CartesianGrid* c = g->cartesian())
            for (const auto& i : c->intersections()) add(i.point, i.labelX, i.labelY, i.labelZ);
        if (const CylindricalGrid* c = g->cylindrical())
            for (const auto& i : c->intersections()) add(i.point, i.labelRadius, i.labelAngle, i.labelZ);
        if (const ArbitraryGrid* a = g->arbitrary())
            for (const auto& p : a->intersections()) add(p, {}, {}, {});
    }
    return out;
}

/// Libellé du groupe d'intersections d'un plan : famille d'axes (niveau, X / rayon, Y / angle) constante dans le groupe
/// et qui le distingue du reste de la grille (une grille à un seul niveau ne nomme pas une coupe verticale « niveau »).
std::string commonLabel(const std::vector<const Local*>& group, const std::vector<Local>& all)
{
    auto discriminating = [&](auto member) -> std::string {
        const std::string& first = group.front()->*member;
        if (first.empty()) return {};
        for (const Local* l : group)
            if (l->*member != first) return {};
        for (const Local& l : all)
            if (l.*member != first) return first;
        return {};
    };
    if (std::string s = discriminating(&Local::labelZ); !s.empty()) return "niveau " + s;
    if (std::string s = discriminating(&Local::labelA); !s.empty()) return "axe " + s;
    if (std::string s = discriminating(&Local::labelB); !s.empty()) return "axe " + s;
    return {};
}
} // namespace

std::vector<InPlaneIntersection> WorkPlaneReferences::allIntersections(const std::vector<const GridSystem*>& grids,
                                                                       const TSA::Coordinate::WorkPlane& wp)
{
    std::vector<InPlaneIntersection> out;
    for (const Local& l : localIntersections(grids, wp))
        out.push_back(l.inter);
    return out;
}

std::vector<PlaneReference> WorkPlaneReferences::parallelPlanes(const std::vector<const GridSystem*>& grids,
                                                                const TSA::Coordinate::WorkPlane& wp, double tol)
{
    std::vector<Local> all = localIntersections(grids, wp);
    std::sort(all.begin(), all.end(), [](const Local& a, const Local& b) { return a.w < b.w; });
    std::vector<PlaneReference> planes;
    size_t i = 0;
    while (i < all.size())
    {
        // Regroupement des intersections de même décalage (à la tolérance près)
        std::vector<const Local*> group { &all[i] };
        size_t j = i + 1;
        while (j < all.size() && all[j].w - all[i].w <= tol)
            group.push_back(&all[j++]);
        double sum = 0.0;
        for (const Local* l : group) sum += l->w;
        planes.push_back({ sum / static_cast<double>(group.size()), commonLabel(group, all) });
        i = j;
    }
    return planes;
}

std::vector<InPlaneIntersection> WorkPlaneReferences::intersectionsInPlane(const std::vector<const GridSystem*>& grids,
                                                                           const TSA::Coordinate::WorkPlane& wp, double tol)
{
    std::vector<InPlaneIntersection> out;
    for (const Local& l : localIntersections(grids, wp))
    {
        if (std::abs(l.w) > tol) continue;
        // Une même position dans plusieurs grilles : une seule intersection
        const bool duplicate = std::any_of(out.begin(), out.end(), [&](const InPlaneIntersection& e) {
            return std::abs(e.u - l.inter.u) <= tol && std::abs(e.v - l.inter.v) <= tol;
        });
        if (!duplicate) out.push_back(l.inter);
    }
    std::sort(out.begin(), out.end(), [tol](const InPlaneIntersection& a, const InPlaneIntersection& b) {
        if (std::abs(a.v - b.v) > tol) return a.v < b.v;
        return a.u < b.u;
    });
    return out;
}

std::optional<PlaneReference> WorkPlaneReferences::nextPlane(const std::vector<const GridSystem*>& grids,
                                                             const TSA::Coordinate::WorkPlane& wp, int direction, double tol)
{
    const auto planes = parallelPlanes(grids, wp, tol);
    if (direction > 0)
    {
        for (const auto& p : planes)
            if (p.offset > tol) return p;
    }
    else
    {
        for (auto it = planes.rbegin(); it != planes.rend(); ++it)
            if (it->offset < -tol) return *it;
    }
    return std::nullopt;
}

std::optional<InPlaneIntersection> WorkPlaneReferences::nextIntersection(const std::vector<const GridSystem*>& grids,
                                                                         const TSA::Coordinate::WorkPlane& wp, int direction, double tol)
{
    const auto list = intersectionsInPlane(grids, wp, tol);
    // Ordre (v, u) : « après l'origine » = v > 0, ou v ≈ 0 et u > 0 (l'origine elle-même est en (0, 0))
    auto after = [tol](const InPlaneIntersection& i) { return i.v > tol || (std::abs(i.v) <= tol && i.u > tol); };
    auto before = [tol](const InPlaneIntersection& i) { return i.v < -tol || (std::abs(i.v) <= tol && i.u < -tol); };
    if (direction > 0)
    {
        for (const auto& i : list)
            if (after(i)) return i;
    }
    else
    {
        for (auto it = list.rbegin(); it != list.rend(); ++it)
            if (before(*it)) return *it;
    }
    return std::nullopt;
}

std::optional<InPlaneIntersection> WorkPlaneReferences::nearestIntersection(const std::vector<const GridSystem*>& grids,
                                                                            const TSA::Coordinate::WorkPlane& wp, const gp_Pnt& target,
                                                                            double tol)
{
    const gp_Vec d(wp.origin(), target);
    const double u = d.Dot(gp_Vec(wp.xDirection())), v = d.Dot(gp_Vec(wp.yDirection()));
    std::optional<InPlaneIntersection> best;
    double bestDist = 0.0;
    for (const auto& i : intersectionsInPlane(grids, wp, tol))
    {
        const double dist = std::hypot(i.u - u, i.v - v);
        if (!best || dist < bestDist)
        {
            best = i;
            bestDist = dist;
        }
    }
    return best;
}

} // namespace TSA::Grid
