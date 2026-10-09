#include "DeformedGeometry.h"
#include "BeamGeometry.h"
#include "../Analysis/LoadResolver.h"

#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_XY.hxx>

#include <algorithm>
#include <array>
#include <cmath>

namespace TSA::Geometry
{

namespace
{
bool finite(double v) { return std::isfinite(v); }

bool finiteDisplacement(const TSA::Analysis::NodeDisplacement& d)
{
    return finite(d.ux) && finite(d.uy) && finite(d.uz) && finite(d.rx) && finite(d.ry) && finite(d.rz);
}

/// Stations du moteur exploitables : même longueur de barre (résultat non périmé) et valeurs finies.
bool stationsUsable(const TSA::Analysis::ElementResults& er, double L)
{
    if (er.intermediateStations.empty()) return false;
    if (std::abs(er.length - L) > 1e-6 * std::max(1.0, L)) return false;
    return std::all_of(er.intermediateStations.begin(), er.intermediateStations.end(), [](const auto& s) {
        return finite(s.position) && finite(s.ux) && finite(s.uy) && finite(s.uz);
    });
}

/// Rotation minimale amenant `from` sur `to` (vecteurs unitaires), appliquée à v (formule de Rodrigues).
gp_Vec rotateMinimal(const gp_Vec& from, const gp_Vec& to, const gp_Vec& v, const gp_Vec& fallbackAxis)
{
    const gp_Vec k = from.Crossed(to);
    const double s = k.Magnitude();
    const double c = from.Dot(to);
    if (s < 1e-12)
    {
        if (c > 0.0) return v;
        // Demi-tour : autour d'un axe perpendiculaire quelconque
        const gp_Vec a = fallbackAxis.Normalized();
        return a * (2.0 * a.Dot(v)) - v;
    }
    const gp_Vec kn = k / s;
    return v * c + kn.Crossed(v) * s + kn * (kn.Dot(v) * (1.0 - c));
}

bool pointInTriangle(const gp_XY& p, const gp_XY& a, const gp_XY& b, const gp_XY& c)
{
    const double d1 = (b - a) ^ (p - a);
    const double d2 = (c - b) ^ (p - b);
    const double d3 = (a - c) ^ (p - c);
    return d1 >= -1e-14 && d2 >= -1e-14 && d3 >= -1e-14;
}

/// Triangulation par oreilles d'un polygone simple en sens trigonométrique.
std::vector<std::array<int, 3>> earClip(const std::vector<gp_XY>& poly)
{
    std::vector<std::array<int, 3>> tris;
    std::vector<int> idx(poly.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
    while (idx.size() > 3)
    {
        bool clipped = false;
        const std::size_t n = idx.size();
        for (std::size_t i = 0; i < n && !clipped; ++i)
        {
            const int a = idx[(i + n - 1) % n], b = idx[i], c = idx[(i + 1) % n];
            if (((poly[b] - poly[a]) ^ (poly[c] - poly[a])) <= 1e-14) continue; // sommet rentrant ou aligné
            bool inside = false;
            for (int j : idx)
                if (j != a && j != b && j != c && pointInTriangle(poly[j], poly[a], poly[b], poly[c]))
                {
                    inside = true;
                    break;
                }
            if (inside) continue;
            tris.push_back({ a, b, c });
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
        }
        if (clipped) continue;
        // Aucune oreille : retirer un sommet aligné (aire nulle), sinon abandon (polygone dégénéré).
        bool removed = false;
        for (std::size_t i = 0; i < n && !removed; ++i)
        {
            const int a = idx[(i + n - 1) % n], b = idx[i], c = idx[(i + 1) % n];
            if (std::abs((poly[b] - poly[a]) ^ (poly[c] - poly[a])) <= 1e-14)
            {
                idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
                removed = true;
            }
        }
        if (!removed) break;
    }
    if (idx.size() == 3) tris.push_back({ idx[0], idx[1], idx[2] });
    return tris;
}
} // namespace

gp_Pnt DeformedGeometry::computeDeformedPoint(
    const gp_Pnt& orig,
    const TSA::Analysis::NodeDisplacement& disp,
    double scaleFactor)
{
    double sf = (std::isnan(scaleFactor) || std::isinf(scaleFactor)) ? 0.0 : scaleFactor;
    double ux = (std::isnan(disp.ux) || std::isinf(disp.ux)) ? 0.0 : disp.ux;
    double uy = (std::isnan(disp.uy) || std::isinf(disp.uy)) ? 0.0 : disp.uy;
    double uz = (std::isnan(disp.uz) || std::isinf(disp.uz)) ? 0.0 : disp.uz;
    gp_Vec d(ux * sf, uy * sf, uz * sf);
    return orig.Translated(d);
}

TopoDS_Shape DeformedGeometry::createDeformedNodeSphere(
    const gp_Pnt& orig,
    const TSA::Analysis::NodeDisplacement& disp,
    double scaleFactor,
    double radius)
{
    if (std::isnan(radius) || std::isinf(radius) || radius <= 1e-6)
    {
        radius = 0.08;
    }
    gp_Pnt p = computeDeformedPoint(orig, disp, scaleFactor);
    try
    {
        return BRepPrimAPI_MakeSphere(p, radius).Shape();
    }
    catch (...)
    {
        return TopoDS_Shape();
    }
}

DeformedAxis DeformedGeometry::computeMemberAxis(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    double betaAngleDeg,
    const TSA::Analysis::NodeDisplacement* d1,
    const TSA::Analysis::NodeDisplacement* d2,
    const TSA::Analysis::ElementResults* elementResults,
    bool flexural,
    int hermiteSegments)
{
    DeformedAxis axis;
    const double L = p1.Distance(p2);
    if (!(L > 1e-9))
    {
        axis.problem = "barre de longueur nulle";
        return axis;
    }
    if (!d1 || !d2)
    {
        axis.problem = "déplacement nodal absent des résultats";
        return axis;
    }
    if (!finiteDisplacement(*d1) || !finiteDisplacement(*d2))
    {
        axis.problem = "déplacement ou rotation nodale non fini";
        return axis;
    }

    // Repère local du solveur (le même que celui des résultats de barre : x de i vers j, y, z).
    const gp_Ax3 frame = TSA::Analysis::LoadResolver::computeElementLocalAxes(p1, p2, betaAngleDeg);
    const gp_Vec ex(frame.XDirection()), ey(frame.YDirection()), ez(frame.Direction());
    const gp_Vec chord(p1, p2);
    const gp_Vec U1(d1->ux, d1->uy, d1->uz), U2(d2->ux, d2->uy, d2->uz);

    auto add = [&](double x, const gp_Vec& u) {
        axis.x.push_back(x);
        axis.initial.push_back(p1.Translated(chord * (x / L)));
        axis.displacement.push_back(u);
    };

    if (flexural && elementResults && stationsUsable(*elementResults, L))
    {
        // Déplacements du moteur le long de la barre (repère local → global), calés sur les nœuds.
        axis.source = DeformedAxisSource::SolverStations;
        std::vector<const TSA::Analysis::StationForces*> st;
        for (const auto& s : elementResults->intermediateStations)
            if (s.position > 1e-9 * L && s.position < L * (1.0 - 1e-9)) st.push_back(&s);
        std::sort(st.begin(), st.end(), [](const auto* a, const auto* b) { return a->position < b->position; });
        add(0.0, U1);
        for (const auto* s : st) add(s->position, ex * s->ux + ey * s->uy + ez * s->uz);
        add(L, U2);
    }
    else if (flexural)
    {
        // Euler-Bernoulli (élément ElasticBeam3d) : u axial linéaire ; v (y local) et w (z local)
        // cubiques d'Hermite, avec θz = v' et θy = −w' (convention des résultats de barre).
        axis.source = DeformedAxisSource::CubicHermite;
        const gp_Vec R1(d1->rx, d1->ry, d1->rz), R2(d2->rx, d2->ry, d2->rz);
        const double u1 = U1.Dot(ex), v1 = U1.Dot(ey), w1 = U1.Dot(ez);
        const double u2 = U2.Dot(ex), v2 = U2.Dot(ey), w2 = U2.Dot(ez);
        const double ry1 = R1.Dot(ey), rz1 = R1.Dot(ez), ry2 = R2.Dot(ey), rz2 = R2.Dot(ez);
        const int n = std::clamp(hermiteSegments, 2, 200);
        for (int k = 0; k <= n; ++k)
        {
            const double s = static_cast<double>(k) / n, s2 = s * s, s3 = s2 * s;
            const double h1 = 2.0 * s3 - 3.0 * s2 + 1.0;
            const double h2 = -2.0 * s3 + 3.0 * s2;
            const double h3 = (s3 - 2.0 * s2 + s) * L;
            const double h4 = (s3 - s2) * L;
            const double u = (1.0 - s) * u1 + s * u2;
            const double v = h1 * v1 + h2 * v2 + h3 * rz1 + h4 * rz2;
            const double w = h1 * w1 + h2 * w2 - (h3 * ry1 + h4 * ry2);
            // Extrémités : déplacements nodaux exacts (pas d'arrondi de projection)
            add(s * L, k == 0 ? U1 : (k == n ? U2 : ex * u + ey * v + ez * w));
        }
    }
    else
    {
        axis.source = DeformedAxisSource::Linear;
        add(0.0, U1);
        add(L, U2);
    }
    axis.valid = true;
    return axis;
}

TopoDS_Shape DeformedGeometry::createAxisWire(const DeformedAxis& axis, double scale)
{
    if (!axis.valid || axis.size() < 2 || !std::isfinite(scale)) return TopoDS_Shape();
    try
    {
        if (axis.size() == 2)
        {
            const gp_Pnt a = axis.pointAt(0, scale), b = axis.pointAt(1, scale);
            if (a.Distance(b) < 1e-9) return TopoDS_Shape();
            return BRepBuilderAPI_MakeEdge(a, b).Shape();
        }
        BRepBuilderAPI_MakePolygon poly;
        for (std::size_t i = 0; i < axis.size(); ++i) poly.Add(axis.pointAt(i, scale));
        if (poly.IsDone()) return poly.Wire();
    }
    catch (...)
    {
    }
    return TopoDS_Shape();
}

TopoDS_Shape DeformedGeometry::createMemberSolid(const DeformedAxis& axis, const TSA::Model::Section& section,
                                                 double rotationDeg, double scale)
{
    if (!axis.valid || axis.size() < 2 || !std::isfinite(scale)) return TopoDS_Shape();
    const auto frame = BeamGeometry::sectionFrame(axis.initial.front(), axis.initial.back(), rotationDeg);
    if (!frame.valid) return TopoDS_Shape();
    const auto outline = BeamGeometry::sectionOutline(section);
    if (outline.outer.size() < 3) return TopoDS_Shape();

    // 1. Points amplifiés et repère de section à chaque station : tangente de l'axe déformé, section
    //    tournée par la rotation minimale depuis la barre droite (aucune torsion artificielle).
    const std::size_t n = axis.size();
    std::vector<gp_Pnt> P(n);
    for (std::size_t i = 0; i < n; ++i) P[i] = axis.pointAt(i, scale);
    std::vector<gp_Vec> X(n), Y(n), T(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        gp_Vec t = (i == 0) ? gp_Vec(P[0], P[1]) : (i == n - 1) ? gp_Vec(P[n - 2], P[n - 1]) : gp_Vec(P[i - 1], P[i + 1]);
        t = t.Magnitude() > 1e-12 ? t.Normalized() : frame.z;
        T[i] = t;
        X[i] = rotateMinimal(frame.z, t, frame.x, frame.x);
        Y[i] = rotateMinimal(frame.z, t, frame.y, frame.x);
    }

    std::vector<gp_Pnt> nodes;
    std::vector<gp_Dir> normals;
    std::vector<std::array<int, 3>> tris; // indices 0-based
    auto vertex = [&](std::size_t station, const gp_XY& q, const gp_Vec& normal) {
        nodes.push_back(P[station].Translated(X[station] * q.X() + Y[station] * q.Y()));
        normals.push_back(normal.Magnitude() > 1e-12 ? gp_Dir(normal) : gp_Dir(T[station]));
        return static_cast<int>(nodes.size()) - 1;
    };

    // 2. Faces latérales d'un contour : normales par arête (arêtes vives) ou radiales (section ronde).
    auto sweepLoop = [&](const std::vector<gp_XY>& loop, bool hole) {
        const std::size_t m = loop.size();
        const double sign = hole ? -1.0 : 1.0;
        for (std::size_t e = 0; e < m; ++e)
        {
            const gp_XY& qa = loop[e];
            const gp_XY& qb = loop[(e + 1) % m];
            const gp_XY d = qb - qa;
            const gp_XY edgeNormal = gp_XY(d.Y(), -d.X()) * sign; // extérieure pour un contour trigonométrique
            std::vector<int> ia(n), ib(n);
            for (std::size_t k = 0; k < n; ++k)
            {
                const gp_XY na = outline.smooth ? qa * sign : edgeNormal;
                const gp_XY nb = outline.smooth ? qb * sign : edgeNormal;
                ia[k] = vertex(k, qa, X[k] * na.X() + Y[k] * na.Y());
                ib[k] = vertex(k, qb, X[k] * nb.X() + Y[k] * nb.Y());
            }
            for (std::size_t k = 0; k + 1 < n; ++k)
            {
                if (!hole)
                {
                    tris.push_back({ ia[k], ib[k], ib[k + 1] });
                    tris.push_back({ ia[k], ib[k + 1], ia[k + 1] });
                }
                else
                {
                    tris.push_back({ ia[k], ib[k + 1], ib[k] });
                    tris.push_back({ ia[k], ia[k + 1], ib[k + 1] });
                }
            }
        }
    };
    sweepLoop(outline.outer, false);
    if (outline.inner.size() >= 3) sweepLoop(outline.inner, true);

    // 3. Faces d'extrémité (contour plein : oreilles ; contour creux : anneau en correspondance).
    std::vector<std::array<int, 3>> capLocal; // indices dans la liste [outer..., inner...]
    std::vector<gp_XY> capPoints = outline.outer;
    if (outline.inner.empty())
    {
        capLocal = earClip(outline.outer);
    }
    else if (outline.inner.size() == outline.outer.size())
    {
        const int m = static_cast<int>(outline.outer.size());
        capPoints.insert(capPoints.end(), outline.inner.begin(), outline.inner.end());
        for (int i = 0; i < m; ++i)
        {
            const int o0 = i, o1 = (i + 1) % m, i0 = m + i, i1 = m + (i + 1) % m;
            capLocal.push_back({ o0, o1, i1 });
            capLocal.push_back({ o0, i1, i0 });
        }
    }
    auto cap = [&](std::size_t station, bool end) {
        const gp_Vec normal = end ? T[station] : -T[station];
        std::vector<int> ids;
        for (const auto& q : capPoints) ids.push_back(vertex(station, q, normal));
        for (const auto& t : capLocal)
        {
            if (end) tris.push_back({ ids[t[0]], ids[t[1]], ids[t[2]] });
            else tris.push_back({ ids[t[0]], ids[t[2]], ids[t[1]] });
        }
    };
    cap(0, false);
    cap(n - 1, true);

    // 4. Un seul maillage OCCT (face triangulée) : la barre déformée est un objet continu.
    try
    {
        Handle(Poly_Triangulation) mesh = new Poly_Triangulation(static_cast<int>(nodes.size()), static_cast<int>(tris.size()),
                                                                 false, true);
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            mesh->SetNode(static_cast<int>(i) + 1, nodes[i]);
            mesh->SetNormal(static_cast<int>(i) + 1, normals[i]);
        }
        for (std::size_t i = 0; i < tris.size(); ++i)
            mesh->SetTriangle(static_cast<int>(i) + 1, Poly_Triangle(tris[i][0] + 1, tris[i][1] + 1, tris[i][2] + 1));
        TopoDS_Face face;
        BRep_Builder builder;
        builder.MakeFace(face, mesh);
        return face;
    }
    catch (...)
    {
        return TopoDS_Shape();
    }
}

TopoDS_Shape DeformedGeometry::createDeformedCenterline(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    const TSA::Analysis::NodeDisplacement& d1,
    const TSA::Analysis::NodeDisplacement& d2,
    double scaleFactor,
    int numSegments)
{
    const DeformedAxis axis = computeMemberAxis(p1, p2, 0.0, &d1, &d2, nullptr, true, numSegments);
    return createAxisWire(axis, scaleFactor);
}

TopoDS_Shape DeformedGeometry::createDeformedBeamShape(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    const TSA::Analysis::NodeDisplacement& d1,
    const TSA::Analysis::NodeDisplacement& d2,
    const TSA::Model::Section& section,
    double scaleFactor,
    double rotationDeg)
{
    const DeformedAxis axis = computeMemberAxis(p1, p2, rotationDeg, &d1, &d2, nullptr, true);
    return createMemberSolid(axis, section, rotationDeg, scaleFactor);
}

} // namespace TSA::Geometry
