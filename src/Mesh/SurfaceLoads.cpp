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

/// Intégrales d'un polygone plan (u, v) : aire et moments d'ordre 1 et 2 (signe de l'orientation retiré).
struct PolyIntegrals
{
    double A = 0, Su = 0, Sv = 0, Suu = 0, Suv = 0, Svv = 0;
};
PolyIntegrals polyIntegrals(const std::vector<std::pair<double, double>>& p)
{
    PolyIntegrals r;
    for (std::size_t i = 0; i < p.size(); ++i)
    {
        const auto& [x1, y1] = p[i];
        const auto& [x2, y2] = p[(i + 1) % p.size()];
        const double c = x1 * y2 - x2 * y1;
        r.A += c / 2;
        r.Su += (x1 + x2) * c / 6;
        r.Sv += (y1 + y2) * c / 6;
        r.Suu += (x1 * x1 + x1 * x2 + x2 * x2) * c / 12;
        r.Svv += (y1 * y1 + y1 * y2 + y2 * y2) * c / 12;
        r.Suv += (x1 * y2 + 2 * x1 * y1 + 2 * x2 * y2 + x2 * y1) * c / 24;
    }
    if (r.A < 0) r.A = -r.A, r.Su = -r.Su, r.Sv = -r.Sv, r.Suu = -r.Suu, r.Suv = -r.Suv, r.Svv = -r.Svv;
    return r;
}

/// Découpe (Sutherland-Hodgman) d'un polygone quelconque par le rectangle [u0, u1] × [v0, v1].
std::vector<std::pair<double, double>> clipRect(std::vector<std::pair<double, double>> poly, double u0, double v0, double u1, double v1)
{
    auto clip = [&](auto inside, auto cut) {
        std::vector<std::pair<double, double>> out;
        for (std::size_t i = 0; i < poly.size(); ++i)
        {
            const auto a = poly[i], b = poly[(i + 1) % poly.size()];
            const bool ia = inside(a), ib = inside(b);
            if (ia) out.push_back(a);
            if (ia != ib) out.push_back(cut(a, b));
        }
        poly = out;
    };
    auto lerpU = [](double c) {
        return [c](std::pair<double, double> a, std::pair<double, double> b) {
            const double t = (c - a.first) / (b.first - a.first);
            return std::pair<double, double> { c, a.second + t * (b.second - a.second) };
        };
    };
    auto lerpV = [](double c) {
        return [c](std::pair<double, double> a, std::pair<double, double> b) {
            const double t = (c - a.second) / (b.second - a.second);
            return std::pair<double, double> { a.first + t * (b.first - a.first), c };
        };
    };
    clip([&](auto p) { return p.first >= u0; }, lerpU(u0));
    if (!poly.empty()) clip([&](auto p) { return p.first <= u1; }, lerpU(u1));
    if (!poly.empty()) clip([&](auto p) { return p.second >= v0; }, lerpV(v0));
    if (!poly.empty()) clip([&](auto p) { return p.second <= v1; }, lerpV(v1));
    return poly;
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

    if (load.zone && (load.zoneU1 <= load.zoneU0 || load.zoneV1 <= load.zoneV0))
    {
        r.errors.push_back("zone chargée vide (u₁ ≤ u₀ ou v₁ ≤ v₀) : charge non convertie");
        return r;
    }
    auto pressure = [&](double u, double v) { return load.value + load.gradU * u + load.gradV * v; };
    std::map<int, P3> forces;
    std::vector<double> N, dr, ds;
    // Repère local d'un nœud du maillage
    auto local = [&](int node) {
        const auto& n = mesh.node(node);
        double u = 0, v = 0;
        s->frame.toLocal(P3 { n.x, n.y, n.z }, &u, &v);
        return std::pair<double, double> { u, v };
    };
    // Coordonnées naturelles (r, s) d'un point (u, v) de l'élément : Newton sur la transformation géométrique
    auto natural = [&](const FeElement& e, double u, double v, double* rr, double* ss) {
        double a = (e.type == FeType::Tri3 || e.type == FeType::Tri6) ? 1.0 / 3 : 0.0, b = a;
        for (int it = 0; it < 30; ++it)
        {
            shape(e.type, a, b, N, dr, ds);
            double fu = -u, fv = -v, j11 = 0, j12 = 0, j21 = 0, j22 = 0;
            for (std::size_t i = 0; i < N.size(); ++i)
            {
                const auto [ui, vi] = local(e.nodes[i]);
                fu += N[i] * ui, fv += N[i] * vi;
                j11 += dr[i] * ui, j12 += ds[i] * ui, j21 += dr[i] * vi, j22 += ds[i] * vi;
            }
            const double det = j11 * j22 - j12 * j21;
            if (std::abs(det) < 1e-300) break;
            const double da = (j22 * fu - j12 * fv) / det, db = (-j21 * fu + j11 * fv) / det;
            a -= da, b -= db;
            if (std::abs(da) + std::abs(db) < 1e-14) break;
        }
        *rr = a, *ss = b;
    };
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
        // Zone : élément entièrement dans la zone → intégration de Gauss sur l'élément ; à cheval → intégration sur la partie
        // découpée (triangles en éventail, règle à 7 points, coordonnées naturelles retrouvées par Newton) ; hors zone → rien.
        bool whole = true;
        std::vector<std::pair<double, double>> part;
        if (load.zone)
        {
            const std::size_t corners = (e.type == FeType::Tri3 || e.type == FeType::Tri6) ? 3 : 4;
            std::vector<std::pair<double, double>> poly;
            bool inside = true;
            for (std::size_t i = 0; i < corners; ++i)
            {
                poly.push_back(local(e.nodes[i]));
                const auto& [u, v] = poly.back();
                inside &= u >= load.zoneU0 - 1e-12 && u <= load.zoneU1 + 1e-12 && v >= load.zoneV0 - 1e-12 && v <= load.zoneV1 + 1e-12;
            }
            if (!inside)
            {
                whole = false;
                part = clipRect(poly, load.zoneU0, load.zoneV0, load.zoneU1, load.zoneV1);
                if (part.size() < 3 || polyIntegrals(part).A < 1e-14) continue;
            }
        }
        if (whole)
        {
            for (const auto& g : gauss(e.type))
            {
                shape(e.type, g.xi, g.eta, N, dr, ds);
                P3 Xr, Xs, X;
                for (std::size_t i = 0; i < N.size(); ++i)
                {
                    const auto& n = mesh.node(e.nodes[i]);
                    const P3 Xi { n.x, n.y, n.z };
                    X = X + Xi * N[i];
                    Xr = Xr + Xi * dr[i];
                    Xs = Xs + Xi * ds[i];
                }
                double u = 0, v = 0;
                s->frame.toLocal(X, &u, &v);
                const double dA = Xr.cross(Xs).norm() * g.w;
                const double p = pressure(u, v);
                for (std::size_t i = 0; i < N.size(); ++i) forces[e.nodes[i]] = forces[e.nodes[i]] + dir * (p * projFactor * N[i] * dA);
            }
            continue;
        }
        const auto tri = gauss(FeType::Tri3);
        for (std::size_t k = 1; k + 1 < part.size(); ++k)
        {
            const auto a = part[0], b = part[k], c = part[k + 1];
            const double area2 = std::abs((b.first - a.first) * (c.second - a.second) - (c.first - a.first) * (b.second - a.second));
            for (const auto& g : tri)
            {
                const double u = a.first + g.xi * (b.first - a.first) + g.eta * (c.first - a.first);
                const double v = a.second + g.xi * (b.second - a.second) + g.eta * (c.second - a.second);
                double rr = 0, ss = 0;
                natural(e, u, v, &rr, &ss);
                shape(e.type, rr, ss, N, dr, ds);
                const double w = pressure(u, v) * projFactor * area2 * g.w;
                for (std::size_t i = 0; i < N.size(); ++i) forces[e.nodes[i]] = forces[e.nodes[i]] + dir * (w * N[i]);
            }
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
    // Valeurs analytiques sur l'aire nette (ouvertures déduites), limitée à la zone : ∫ p dA et ∫ X p dA par les intégrales
    // exactes des polygones (p linéaire en u, v).
    auto toUV = [&](const std::vector<P3>& poly) {
        std::vector<std::pair<double, double>> out;
        for (const auto& q : poly)
        {
            double u = 0, v = 0;
            s->frame.toLocal(q, &u, &v);
            out.emplace_back(u, v);
        }
        return load.zone ? clipRect(out, load.zoneU0, load.zoneV0, load.zoneU1, load.zoneV1) : out;
    };
    PolyIntegrals I;
    auto accumulate = [&](const std::vector<P3>& poly, double sign) {
        const auto uv = toUV(poly);
        if (uv.size() < 3) return;
        const auto J = polyIntegrals(uv);
        I.A += sign * J.A, I.Su += sign * J.Su, I.Sv += sign * J.Sv, I.Suu += sign * J.Suu, I.Suv += sign * J.Suv, I.Svv += sign * J.Svv;
    };
    accumulate(s->outer, 1.0);
    for (const auto& h : s->holes) accumulate(h, -1.0);
    const double area = I.A;
    if (area <= 1e-12)
    {
        r.errors.push_back(load.zone ? "zone chargée hors de la surface nette : charge non convertie" : "aire nette nulle : charge non convertie");
        return r;
    }
    const double P = load.value * I.A + load.gradU * I.Su + load.gradV * I.Sv;   // ∫ p dA
    const double Pu = load.value * I.Su + load.gradU * I.Suu + load.gradV * I.Suv;   // ∫ u p dA
    const double Pv = load.value * I.Sv + load.gradU * I.Suv + load.gradV * I.Svv;   // ∫ v p dA
    const P3 firstMoment = s->frame.origin * P + s->frame.U * Pu + s->frame.V * Pv;   // ∫ X p dA
    r.expectedResultant = dir * (P * projFactor);
    r.expectedMoment = firstMoment.cross(dir) * projFactor;
    const P3 centroid = std::abs(P) > 1e-300 ? firstMoment * (1.0 / P) : s->frame.origin;
    const double scale = std::max(r.expectedResultant.norm(), (std::abs(load.gradU) + std::abs(load.gradV)) * area * std::sqrt(area) * 1e-3 + 1e-12);
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
