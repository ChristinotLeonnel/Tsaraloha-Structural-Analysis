#include "SurfaceLoads.h"

#include <array>
#include <cmath>
#include <map>

namespace TSA::Mesh
{

namespace
{
struct Gauss
{
    double xi, eta, w;
};

// Fonctions de forme (ordre Gmsh) et leurs dérivées
void shape(FeType t, double r, double s, std::vector<double>& N, std::vector<double>& dr, std::vector<double>& ds)
{
    switch (t)
    {
    case FeType::Tri3:
        N = { 1 - r - s, r, s };
        dr = { -1, 1, 0 };
        ds = { -1, 0, 1 };
        break;
    case FeType::Tri6:
    {
        const double l = 1 - r - s;
        N = { l * (2 * l - 1), r * (2 * r - 1), s * (2 * s - 1), 4 * l * r, 4 * r * s, 4 * s * l };
        dr = { -(4 * l - 1), 4 * r - 1, 0, 4 * (l - r), 4 * s, -4 * s };
        ds = { -(4 * l - 1), 0, 4 * s - 1, -4 * r, 4 * r, 4 * (l - s) };
        break;
    }
    case FeType::Quad4:
        N = { 0.25 * (1 - r) * (1 - s), 0.25 * (1 + r) * (1 - s), 0.25 * (1 + r) * (1 + s), 0.25 * (1 - r) * (1 + s) };
        dr = { -0.25 * (1 - s), 0.25 * (1 - s), 0.25 * (1 + s), -0.25 * (1 + s) };
        ds = { -0.25 * (1 - r), -0.25 * (1 + r), 0.25 * (1 + r), 0.25 * (1 - r) };
        break;
    case FeType::Quad8:
    {
        static const double cr[4] = { -1, 1, 1, -1 }, cs[4] = { -1, -1, 1, 1 };
        N.assign(8, 0), dr.assign(8, 0), ds.assign(8, 0);
        for (int i = 0; i < 4; ++i)
        {
            const double a = cr[i], b = cs[i];
            N[i] = 0.25 * (1 + a * r) * (1 + b * s) * (a * r + b * s - 1);
            dr[i] = 0.25 * a * (1 + b * s) * (2 * a * r + b * s);
            ds[i] = 0.25 * b * (1 + a * r) * (a * r + 2 * b * s);
        }
        // milieux : arêtes (0,1) s = −1, (1,2) r = 1, (2,3) s = 1, (3,0) r = −1
        N[4] = 0.5 * (1 - r * r) * (1 - s), dr[4] = -r * (1 - s), ds[4] = -0.5 * (1 - r * r);
        N[5] = 0.5 * (1 + r) * (1 - s * s), dr[5] = 0.5 * (1 - s * s), ds[5] = -(1 + r) * s;
        N[6] = 0.5 * (1 - r * r) * (1 + s), dr[6] = -r * (1 + s), ds[6] = 0.5 * (1 - r * r);
        N[7] = 0.5 * (1 - r) * (1 - s * s), dr[7] = -0.5 * (1 - s * s), ds[7] = -(1 - r) * s;
        break;
    }
    default: N.clear(); break;
    }
}

std::vector<Gauss> gauss(FeType t)
{
    if (t == FeType::Tri3 || t == FeType::Tri6)
    {
        // 7 points, degré 5
        const double a = 0.0597158717, b = 0.4701420641, c = 0.7974269853, d = 0.1012865073;
        const double w0 = 0.225 / 2, w1 = 0.1323941527 / 2, w2 = 0.1259391805 / 2;
        return { { 1.0 / 3, 1.0 / 3, w0 }, { a, b, w1 }, { b, a, w1 }, { b, b, w1 }, { c, d, w2 }, { d, c, w2 }, { d, d, w2 } };
    }
    const double g = std::sqrt(0.6);
    const double p[3] = { -g, 0, g }, w[3] = { 5.0 / 9, 8.0 / 9, 5.0 / 9 };
    std::vector<Gauss> out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out.push_back({ p[i], p[j], w[i] * w[j] });
    return out;
}

double polygonArea(const std::vector<P3>& poly, const PlaneFrame& f, P3* centroidTimesArea)
{
    double a = 0, cu = 0, cv = 0;
    std::vector<std::pair<double, double>> p;
    for (const auto& q : poly)
    {
        double u = 0, v = 0;
        f.toLocal(q, &u, &v);
        p.emplace_back(u, v);
    }
    for (std::size_t i = 0; i < p.size(); ++i)
    {
        const auto& [x1, y1] = p[i];
        const auto& [x2, y2] = p[(i + 1) % p.size()];
        const double cr = x1 * y2 - x2 * y1;
        a += cr;
        cu += (x1 + x2) * cr;
        cv += (y1 + y2) * cr;
    }
    a *= 0.5;
    if (centroidTimesArea && std::abs(a) > 0) *centroidTimesArea = f.toGlobal(cu / (6 * a), cv / (6 * a)) * std::abs(a);
    return std::abs(a);
}

double relErr(const P3& a, const P3& b, double scale)
{
    return (a - b).norm() / std::max(scale, 1e-12);
}
} // namespace

SurfaceLoadResult distributeSurfaceLoad(const FeMesh& mesh, const MeshGeometry& geo, const SurfacePressure& load, double tol)
{
    SurfaceLoadResult r;
    const SurfaceGeom* s = geo.surface(load.surface);
    if (!s)
    {
        r.errors.push_back("surface « " + load.surface + " » inconnue du modèle : charge non convertie");
        return r;
    }
    P3 dir;
    bool normal = false;
    switch (load.direction)
    {
    case SurfacePressure::Direction::GlobalX: dir = { 1, 0, 0 }; break;
    case SurfacePressure::Direction::GlobalY: dir = { 0, 1, 0 }; break;
    case SurfacePressure::Direction::GlobalZ: dir = { 0, 0, 1 }; break;
    case SurfacePressure::Direction::Gravity: dir = { 0, 0, -1 }; break;
    case SurfacePressure::Direction::LocalNormal: dir = s->frame.N * -1.0, normal = true; break;
    }
    if (normal && load.projected) r.warnings.push_back("surface projetée sans objet pour une pression normale : surface réelle utilisée");
    const double projFactor = (!normal && load.projected) ? std::abs(s->frame.N.dot(dir)) : 1.0;

    std::map<int, P3> forces;
    std::vector<double> N, dr, ds;
    for (const auto& e : mesh.elements)
    {
        if (e.boundary || e.source != load.surface || feTypeDimension(e.type) != 2) continue;
        shape(e.type, 0, 0, N, dr, ds);
        if (N.empty() || N.size() != e.nodes.size())
        {
            r.errors.push_back(std::string("élément ") + feTypeName(e.type) + " non pris en charge pour la répartition : charge non convertie");
            return r;
        }
        ++r.elements;
        for (const auto& g : gauss(e.type))
        {
            shape(e.type, g.xi, g.eta, N, dr, ds);
            P3 Xr, Xs;
            for (std::size_t i = 0; i < N.size(); ++i)
            {
                const auto& n = mesh.node(e.nodes[i]);
                const P3 X { n.x, n.y, n.z };
                Xr = Xr + X * dr[i];
                Xs = Xs + X * ds[i];
            }
            const double dA = Xr.cross(Xs).norm() * g.w;
            for (std::size_t i = 0; i < N.size(); ++i) forces[e.nodes[i]] = forces[e.nodes[i]] + dir * (load.value * projFactor * N[i] * dA);
        }
    }
    if (r.elements == 0)
    {
        r.errors.push_back("surface « " + load.surface + " » non maillée (aucun élément 2D) : charge non convertie");
        return r;
    }
    for (const auto& [n, f] : forces)
    {
        r.forces.push_back({ n, f.x, f.y, f.z });
        const auto& fn = mesh.node(n);
        r.resultant = r.resultant + f;
        r.moment = r.moment + P3 { fn.x, fn.y, fn.z }.cross(f);
    }
    // Valeurs analytiques : aire nette (ouvertures déduites) et centre de surface
    P3 ca, cah;
    double area = polygonArea(s->outer, s->frame, &ca);
    P3 c = ca;
    for (const auto& h : s->holes)
    {
        const double ah = polygonArea(h, s->frame, &cah);
        area -= ah;
        c = c - cah;
    }
    if (area <= 0)
    {
        r.errors.push_back("aire nette nulle : charge non convertie");
        return r;
    }
    const P3 centroid = c * (1.0 / area);
    r.expectedResultant = dir * (load.value * projFactor * area);
    r.expectedMoment = centroid.cross(r.expectedResultant);
    const double scale = r.expectedResultant.norm();
    r.resultantError = relErr(r.resultant, r.expectedResultant, scale);
    const double armScale = scale * std::max(1.0, centroid.norm() + std::sqrt(area));
    r.momentError = relErr(r.moment, r.expectedMoment, armScale);
    if (r.resultantError > tol || r.momentError > tol)
    {
        r.errors.push_back("résultante ou moment des charges nodales différents de la valeur analytique (écarts " + std::to_string(r.resultantError) + " / " +
                           std::to_string(r.momentError) + ") : maillage ne couvrant pas exactement la surface, charge non convertie");
        return r;
    }
    r.ok = true;
    return r;
}

SurfaceLoadResult distributeSurfacePointLoad(const FeMesh& mesh, const MeshGeometry& geo, const SurfacePointLoad& load, double tol)
{
    SurfaceLoadResult r;
    const SurfaceGeom* s = geo.surface(load.surface);
    if (!s)
    {
        r.errors.push_back("surface « " + load.surface + " » inconnue du modèle : charge non convertie");
        return r;
    }
    // Le point doit être dans la surface nette (repère local)
    auto local = [&](const std::vector<P3>& poly) {
        std::vector<std::pair<double, double>> out;
        for (const auto& q : poly)
        {
            double u = 0, v = 0;
            s->frame.toLocal(q, &u, &v);
            out.emplace_back(u, v);
        }
        return out;
    };
    if (!pointInPolygon(load.u, load.v, local(s->outer)))
    {
        r.errors.push_back("point (" + std::to_string(load.u) + " ; " + std::to_string(load.v) + ") hors de la surface " + load.surface + " : charge non convertie");
        return r;
    }
    for (const auto& h : s->holes)
        if (pointInPolygon(load.u, load.v, local(h)))
        {
            r.errors.push_back("point dans une ouverture de " + load.surface + " : aucune matière porteuse, charge non convertie");
            return r;
        }
    P3 dir;
    switch (load.direction)
    {
    case SurfacePressure::Direction::GlobalX: dir = { 1, 0, 0 }; break;
    case SurfacePressure::Direction::GlobalY: dir = { 0, 1, 0 }; break;
    case SurfacePressure::Direction::GlobalZ: dir = { 0, 0, 1 }; break;
    case SurfacePressure::Direction::Gravity: dir = { 0, 0, -1 }; break;
    case SurfacePressure::Direction::LocalNormal: dir = s->frame.N * -1.0; break;
    }
    const P3 P = s->frame.toGlobal(load.u, load.v);
    std::vector<double> N, dr, ds;
    for (const auto& e : mesh.elements)
    {
        if (e.boundary || e.source != load.surface || (e.type != FeType::Tri3 && e.type != FeType::Quad4)) continue;
        // Coordonnées naturelles du point dans l'élément (plan de la surface) : Newton sur x(ξ, η) = P
        double xi = e.type == FeType::Tri3 ? 1.0 / 3 : 0.0, eta = xi;
        bool converged = false;
        for (int it = 0; it < 25; ++it)
        {
            shape(e.type, xi, eta, N, dr, ds);
            P3 X, Xr, Xs;
            for (std::size_t i = 0; i < N.size(); ++i)
            {
                const auto& n = mesh.node(e.nodes[i]);
                const P3 Q { n.x, n.y, n.z };
                X = X + Q * N[i];
                Xr = Xr + Q * dr[i];
                Xs = Xs + Q * ds[i];
            }
            const P3 res = P - X;
            // Moindres carrés 2×2 : [Xr·Xr Xr·Xs ; Xs·Xr Xs·Xs] d = [Xr·res ; Xs·res]
            const double a11 = Xr.dot(Xr), a12 = Xr.dot(Xs), a22 = Xs.dot(Xs), b1 = Xr.dot(res), b2 = Xs.dot(res);
            const double det = a11 * a22 - a12 * a12;
            if (std::abs(det) < 1e-30) break;
            const double dxi = (b1 * a22 - b2 * a12) / det, deta = (a11 * b2 - a12 * b1) / det;
            xi += dxi;
            eta += deta;
            if (std::abs(dxi) + std::abs(deta) < 1e-12)
            {
                converged = res.norm() < 1e-6;
                break;
            }
        }
        if (!converged) continue;
        const double e1 = 1e-9;
        const bool inside = e.type == FeType::Tri3 ? (xi >= -e1 && eta >= -e1 && xi + eta <= 1 + e1) : (std::abs(xi) <= 1 + e1 && std::abs(eta) <= 1 + e1);
        if (!inside) continue;
        shape(e.type, xi, eta, N, dr, ds);
        ++r.elements;
        for (std::size_t i = 0; i < N.size(); ++i)
        {
            const P3 f = dir * (load.value * N[i]);
            r.forces.push_back({ e.nodes[i], f.x, f.y, f.z });
            const auto& n = mesh.node(e.nodes[i]);
            r.resultant = r.resultant + f;
            r.moment = r.moment + P3 { n.x, n.y, n.z }.cross(f);
        }
        r.expectedResultant = dir * load.value;
        r.expectedMoment = P.cross(r.expectedResultant);
        const double scale = std::abs(load.value);
        r.resultantError = relErr(r.resultant, r.expectedResultant, scale);
        r.momentError = relErr(r.moment, r.expectedMoment, scale * std::max(1.0, P.norm()));
        if (r.resultantError > tol || r.momentError > tol)
        {
            r.errors.push_back("charge ponctuelle : résultante ou moment des forces nodales différents de la valeur exacte, charge non convertie");
            r.forces.clear();
            return r;
        }
        r.ok = true;
        return r;
    }
    r.errors.push_back("aucun élément du premier ordre (tri3, quad4) de " + load.surface + " ne contient le point : surface non maillée ou maillage du second ordre");
    return r;
}

} // namespace TSA::Mesh
