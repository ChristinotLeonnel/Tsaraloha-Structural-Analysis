#include "MeshQuality.h"

#include "MeshGeometry.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>

namespace TSA::Mesh
{

const char* severityName(QualityIssue::Severity s)
{
    switch (s)
    {
    case QualityIssue::Severity::Error: return "erreur";
    case QualityIssue::Severity::Warning: return "avertissement";
    case QualityIssue::Severity::Info: return "information";
    }
    return "";
}

namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Acc
{
    MetricStats s;
    double sum = 0;
    void add(double v)
    {
        if (s.count == 0) s.min = s.max = v;
        s.min = std::min(s.min, v);
        s.max = std::max(s.max, v);
        sum += v;
        ++s.count;
        s.mean = sum / double(s.count);
    }
};

double angleDeg(const P3& a, const P3& b)
{
    const double c = a.dot(b) / (a.norm() * b.norm() + 1e-300);
    return std::acos(std::clamp(c, -1.0, 1.0)) * 180.0 / kPi;
}
} // namespace

QualityReport analyzeMesh(const FeMesh& mesh, const QualityCriteria& q, const std::vector<MeshInterface>& interfaces)
{
    QualityReport r;
    r.nodes = mesh.nodes.size();
    r.perElementQuality.assign(mesh.elements.size(), 1.0);
    Acc aspect, minAng, maxAng, jac, growth;
    const int N = int(mesh.nodes.size());
    auto P = [&](int id) {
        const auto& n = mesh.node(id);
        return P3 { n.x, n.y, n.z };
    };
    auto issue = [&](QualityIssue::Severity sev, const std::string& kind, const std::string& msg, const FeElement* e, int node, double value) {
        QualityIssue i;
        i.severity = sev;
        i.kind = kind;
        i.message = msg;
        i.value = value;
        if (e)
        {
            i.element = e->id;
            i.source = e->source;
            P3 c;
            const int nc = feTypeCornerCount(e->type);
            for (int k = 0; k < nc && k < int(e->nodes.size()); ++k)
                if (e->nodes[k] >= 1 && e->nodes[k] <= N) c = c + P(e->nodes[k]);
            c = c * (1.0 / nc);
            i.x = c.x, i.y = c.y, i.z = c.z;
        }
        if (node >= 1 && node <= N)
        {
            i.node = node;
            const auto& n = mesh.node(node);
            i.x = n.x, i.y = n.y, i.z = n.z;
            if (i.source.empty()) i.source = n.source;
        }
        (sev == QualityIssue::Severity::Error ? r.errors : sev == QualityIssue::Severity::Warning ? r.warnings : r.warnings) +=
            sev == QualityIssue::Severity::Info ? 0 : 1;
        r.issues.push_back(i);
    };
    auto fmt = [](double v) { return std::to_string(std::round(v * 100) / 100).substr(0, std::to_string(std::round(v * 100) / 100).find('.') + 3); };

    // Orientation des éléments 2D : cohérence entre voisins (une arête commune doit être parcourue en sens
    // opposés). Propagation depuis un élément par surface ; les éléments minoritaires sont « inversés ».
    std::vector<char> flipped(mesh.elements.size(), 0);
    {
        std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> edgeDir;   // arête → (élément, +1/−1)
        for (const auto& e : mesh.elements)
        {
            if (e.boundary || feTypeDimension(e.type) != 2) continue;
            const int nc = feTypeCornerCount(e.type);
            if (int(e.nodes.size()) < nc) continue;
            for (int k = 0; k < nc; ++k)
            {
                const int a = e.nodes[k], b = e.nodes[(k + 1) % nc];
                edgeDir[{ std::min(a, b), std::max(a, b) }].push_back({ e.id, a < b ? 1 : -1 });
            }
        }
        std::map<int, std::vector<std::pair<int, bool>>> adj;   // élément → (voisin, même sens ?)
        for (const auto& [ed, list] : edgeDir)
            for (std::size_t i = 0; i < list.size(); ++i)
                for (std::size_t j = i + 1; j < list.size(); ++j)
                {
                    const auto& ei = mesh.elements[std::size_t(list[i].first - 1)];
                    const auto& ej = mesh.elements[std::size_t(list[j].first - 1)];
                    if (ei.source != ej.source) continue;
                    const bool same = list[i].second == list[j].second;
                    adj[ei.id].push_back({ ej.id, same });
                    adj[ej.id].push_back({ ei.id, same });
                }
        std::vector<int> state(mesh.elements.size() + 1, 0);   // 0 inconnu, 1 sens de référence, 2 sens opposé
        for (const auto& e : mesh.elements)
        {
            if (e.boundary || feTypeDimension(e.type) != 2 || state[e.id]) continue;
            std::vector<int> comp { e.id };
            state[e.id] = 1;
            for (std::size_t c = 0; c < comp.size(); ++c)
                for (const auto& [nb, same] : adj[comp[c]])
                    if (!state[nb])
                    {
                        state[nb] = same ? 3 - state[comp[c]] : state[comp[c]];
                        comp.push_back(nb);
                    }
            std::size_t ones = 0;
            for (int id : comp) ones += state[id] == 1;
            const int minority = ones * 2 >= comp.size() ? 2 : 1;
            for (int id : comp)
                if (state[id] == minority) flipped[std::size_t(id - 1)] = 1;
        }
    }

    std::vector<int> used(std::size_t(N + 1), 0);
    std::vector<int> usedBy3D(std::size_t(N + 1), 0), usedByOther(std::size_t(N + 1), 0);
    std::map<std::pair<int, int>, std::vector<int>> lineAdj;   // 1D : nœud → éléments (croissance)
    for (const auto& e : mesh.elements)
    {
        const int dim = feTypeDimension(e.type);
        const int nc = feTypeCornerCount(e.type);
        bool valid = int(e.nodes.size()) >= nc;
        for (int id : e.nodes) valid &= id >= 1 && id <= N;
        if (!valid)
        {
            issue(QualityIssue::Severity::Error, "connexion incorrecte", "élément référant un nœud inexistant", &e, 0, 0);
            r.perElementQuality[e.id - 1] = 0;
            continue;
        }
        std::set<int> distinct(e.nodes.begin(), e.nodes.end());
        if (distinct.size() != e.nodes.size())
        {
            issue(QualityIssue::Severity::Error, "connexion incorrecte", "nœud répété dans l'élément (élément dégénéré)", &e, 0, 0);
            r.perElementQuality[e.id - 1] = 0;
        }
        for (int id : e.nodes) (dim == 3 ? usedBy3D : usedByOther)[id] = 1, used[id] = 1;
        if (e.boundary) continue;
        ++r.elements;
        (dim == 1 ? r.elements1D : dim == 2 ? r.elements2D : r.elements3D)++;
        std::vector<P3> c;
        for (int k = 0; k < nc; ++k) c.push_back(P(e.nodes[k]));
        double quality = 1.0;
        if (dim == 1)
        {
            const double L = distance(c[0], c[1]);
            if (L < 1e-9) issue(QualityIssue::Severity::Error, "élément dégénéré", "élément 1D de longueur nulle", &e, 0, L), quality = 0;
            lineAdj[{ std::min(e.nodes[0], e.nodes[1]), 0 }].push_back(e.id);
        }
        else if (dim == 2)
        {
            double area = 0;
            P3 nrm;
            for (int k = 1; k + 1 < nc; ++k) nrm = nrm + (c[k] - c[0]).cross(c[k + 1] - c[0]);
            area = 0.5 * nrm.norm();
            double lmin = 1e300, lmax = 0, per = 0, amin = 360, amax = 0;
            for (int k = 0; k < nc; ++k)
            {
                const double l = distance(c[k], c[(k + 1) % nc]);
                lmin = std::min(lmin, l), lmax = std::max(lmax, l), per += l;
                const double a = angleDeg(c[(k + nc - 1) % nc] - c[k], c[(k + 1) % nc] - c[k]);
                amin = std::min(amin, a), amax = std::max(amax, a);
            }
            if (area < 1e-12 * std::max(1.0, lmax * lmax))
            {
                issue(QualityIssue::Severity::Error, "élément dégénéré", "élément d'aire nulle", &e, 0, area);
                r.perElementQuality[e.id - 1] = 0;
                continue;
            }
            if (flipped[std::size_t(e.id - 1)])
                issue(QualityIssue::Severity::Error, "élément inversé", "orientation opposée à celle des éléments voisins de la surface", &e, 0, 0), quality = 0;
            double ar;
            if (nc == 3) ar = lmax * per / (4 * std::sqrt(3.0) * area);
            else
            {
                ar = lmax / std::max(lmin, 1e-300);
                const P3 n = nrm.normalized();
                double sj = 1;
                for (int k = 0; k < 4; ++k)
                {
                    const P3 e1 = c[(k + 1) % 4] - c[k], e2 = c[(k + 3) % 4] - c[k];
                    sj = std::min(sj, e1.cross(e2).dot(n) / (e1.norm() * e2.norm() + 1e-300));
                }
                jac.add(sj);
                if (sj <= 0) issue(QualityIssue::Severity::Error, "élément inversé", "jacobien normalisé ≤ 0 (quadrangle non convexe ou retourné)", &e, 0, sj), quality = 0;
                else if (sj < q.minScaledJacobian)
                    issue(QualityIssue::Severity::Warning, "distorsion", "jacobien normalisé " + fmt(sj) + " < " + fmt(q.minScaledJacobian), &e, 0, sj);
                quality = std::min(quality, std::max(0.0, sj));
            }
            aspect.add(ar), minAng.add(amin), maxAng.add(amax);
            if (ar > q.maxAspectRatio)
                issue(QualityIssue::Severity::Warning, "rapport d'aspect", "rapport d'aspect " + fmt(ar) + " > " + fmt(q.maxAspectRatio), &e, 0, ar);
            if (amin < 5) issue(QualityIssue::Severity::Error, "angle", "angle de " + fmt(amin) + "° : élément quasi dégénéré", &e, 0, amin);
            else if (amin < q.minAngleDeg)
                issue(QualityIssue::Severity::Warning, "angle", "angle minimal " + fmt(amin) + "° < " + fmt(q.minAngleDeg) + "°", &e, 0, amin);
            if (amax > 175) issue(QualityIssue::Severity::Error, "angle", "angle de " + fmt(amax) + "° : élément quasi dégénéré", &e, 0, amax);
            else if (amax > q.maxAngleDeg)
                issue(QualityIssue::Severity::Warning, "angle", "angle maximal " + fmt(amax) + "° > " + fmt(q.maxAngleDeg) + "°", &e, 0, amax);
            quality = std::min({ quality, 1.0 / ar, nc == 3 ? amin / 60.0 : amin / 90.0 });
        }
        else if (nc == 4)
        {
            // Tétraèdre : volume signé, arêtes ; la qualité normalisée est celle de Gmsh (minSICN)
            const double vol = (c[1] - c[0]).dot((c[2] - c[0]).cross(c[3] - c[0])) / 6.0;
            static const int edges[6][2] = { { 0, 1 }, { 1, 2 }, { 2, 0 }, { 0, 3 }, { 1, 3 }, { 2, 3 } };
            double lmin = 1e300, lmax = 0;
            for (const auto& ed : edges)
            {
                const double l = distance(c[ed[0]], c[ed[1]]);
                lmin = std::min(lmin, l), lmax = std::max(lmax, l);
            }
            const double ar = lmax / std::max(lmin, 1e-300);
            aspect.add(ar);
            if (std::abs(vol) < 1e-12 * std::max(1.0, lmax * lmax * lmax))
                issue(QualityIssue::Severity::Error, "élément dégénéré", "tétraèdre de volume nul", &e, 0, vol), quality = 0;
            if (ar > q.maxAspectRatio)
                issue(QualityIssue::Severity::Warning, "rapport d'aspect", "rapport d'aspect " + fmt(ar) + " > " + fmt(q.maxAspectRatio), &e, 0, ar);
            quality = std::min(quality, 1.0 / ar);
        }
        else
        {
            static const int corner[8][3] = { { 1, 3, 4 }, { 2, 0, 5 }, { 3, 1, 6 }, { 0, 2, 7 }, { 7, 5, 0 }, { 4, 6, 1 }, { 5, 7, 2 }, { 6, 4, 3 } };
            static const int edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
            double sj = 1, lmin = 1e300, lmax = 0;
            for (int k = 0; k < 8; ++k)
            {
                const P3 e1 = c[corner[k][0]] - c[k], e2 = c[corner[k][1]] - c[k], e3 = c[corner[k][2]] - c[k];
                sj = std::min(sj, e1.dot(e2.cross(e3)) / (e1.norm() * e2.norm() * e3.norm() + 1e-300));
            }
            for (const auto& ed : edges)
            {
                const double l = distance(c[ed[0]], c[ed[1]]);
                lmin = std::min(lmin, l), lmax = std::max(lmax, l);
            }
            const double ar = lmax / std::max(lmin, 1e-300);
            aspect.add(ar), jac.add(sj);
            if (lmin < 1e-9) issue(QualityIssue::Severity::Error, "élément dégénéré", "hexaèdre à arête nulle", &e, 0, lmin), quality = 0;
            else if (sj <= 0) issue(QualityIssue::Severity::Error, "élément inversé", "jacobien normalisé ≤ 0 (hexaèdre retourné)", &e, 0, sj), quality = 0;
            else if (sj < q.minScaledJacobian)
                issue(QualityIssue::Severity::Warning, "distorsion", "jacobien normalisé " + fmt(sj) + " < " + fmt(q.minScaledJacobian), &e, 0, sj);
            if (ar > q.maxAspectRatio)
                issue(QualityIssue::Severity::Warning, "rapport d'aspect", "rapport d'aspect " + fmt(ar) + " > " + fmt(q.maxAspectRatio), &e, 0, ar);
            quality = std::min({ quality, std::max(0.0, sj), 1.0 / ar });
        }
        // Qualité Gmsh « minSICN » (inverse du conditionnement signé) : < 0 = élément inversé
        if (dim >= 2 && e.quality <= 1.0)
        {
            jac.add(e.quality);
            if (e.quality < 0)
                issue(QualityIssue::Severity::Error, "élément inversé", "qualité Gmsh minSICN " + fmt(e.quality) + " < 0 (jacobien négatif)", &e, 0, e.quality), quality = 0;
            else if (e.quality < q.minSICN)
                issue(QualityIssue::Severity::Warning, "qualité Gmsh", "minSICN " + fmt(e.quality) + " < " + fmt(q.minSICN), &e, 0, e.quality);
            quality = std::min(quality, std::max(0.0, e.quality));
        }
        r.perElementQuality[e.id - 1] = std::min(r.perElementQuality[e.id - 1], std::clamp(quality, 0.0, 1.0));
    }

    // Croissance des tailles 1D (éléments voisins d'une même barre)
    {
        std::map<int, std::vector<const FeElement*>> byNode;
        for (const auto& e : mesh.elements)
            if (!e.boundary && feTypeDimension(e.type) == 1) byNode[e.nodes[0]].push_back(&e), byNode[e.nodes[1]].push_back(&e);
        for (const auto& [n, els] : byNode)
            for (std::size_t i = 0; i < els.size(); ++i)
                for (std::size_t j = i + 1; j < els.size(); ++j)
                {
                    if (els[i]->source != els[j]->source) continue;
                    const double a = distance(P(els[i]->nodes[0]), P(els[i]->nodes[1])), b = distance(P(els[j]->nodes[0]), P(els[j]->nodes[1]));
                    const double g = std::max(a, b) / std::max(std::min(a, b), 1e-300);
                    growth.add(g);
                    if (g > q.maxGrowthRatio)
                        issue(QualityIssue::Severity::Warning, "transition de taille", "rapport " + fmt(g) + " entre éléments voisins > " + fmt(q.maxGrowthRatio),
                              els[i], n, g);
                }
    }

    // Nœuds isolés et nœuds confondus non fusionnés
    std::unordered_map<long long, std::vector<int>> grid;
    auto cellKey = [](const FeNode& n) {
        const long long x = std::llround(n.x * 1e6), y = std::llround(n.y * 1e6), z = std::llround(n.z * 1e6);
        return x * 73856093LL ^ y * 19349663LL ^ z * 83492791LL;
    };
    for (const auto& n : mesh.nodes)
    {
        if (!used[n.id]) issue(QualityIssue::Severity::Warning, "nœud isolé", "nœud relié à aucun élément", nullptr, n.id, 0);
        grid[cellKey(n)].push_back(n.id);
    }
    for (const auto& [k, ids] : grid)
        for (std::size_t i = 0; i < ids.size(); ++i)
            for (std::size_t j = i + 1; j < ids.size(); ++j)
                if (distance(P(ids[i]), P(ids[j])) < 1e-6)
                    issue(QualityIssue::Severity::Warning, "nœuds confondus",
                          "nœuds " + std::to_string(ids[i]) + " et " + std::to_string(ids[j]) + " confondus mais distincts (" + mesh.node(ids[i]).source + " / " +
                              mesh.node(ids[j]).source + ") : régions non connectées à cet endroit",
                          nullptr, ids[i], 0);

    // Arêtes : non-variétés (plus de deux éléments d'une même surface) et arêtes libres
    std::map<std::pair<int, int>, std::vector<const FeElement*>> edgeUse;
    for (const auto& e : mesh.elements)
    {
        if (e.boundary || feTypeDimension(e.type) != 2) continue;
        const int nc = feTypeCornerCount(e.type);
        for (int k = 0; k < nc; ++k)
        {
            const int a = e.nodes[k], b = e.nodes[(k + 1) % nc];
            edgeUse[{ std::min(a, b), std::max(a, b) }].push_back(&e);
        }
    }
    std::vector<std::pair<std::pair<int, int>, const FeElement*>> freeEdges;
    for (const auto& [ed, els] : edgeUse)
    {
        std::map<std::string, int> perSource;
        for (const auto* e : els) ++perSource[e->source];
        for (const auto& [src, count] : perSource)
            if (count > 2)
                issue(QualityIssue::Severity::Error, "connexion incorrecte", "arête partagée par " + std::to_string(count) + " éléments de " + src, els.front(), ed.first, count);
        for (const auto* e : els)
            if (perSource[e->source] == 1) freeEdges.push_back({ ed, e });
    }
    // Faces libres des hexaèdres (arêtes de contour des volumes)
    {
        static const int faces[6][4] = { { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 4, 7, 3 }, { 1, 2, 6, 5 } };
        std::map<std::vector<int>, std::vector<const FeElement*>> faceUse;
        for (const auto& e : mesh.elements)
        {
            if (e.boundary || feTypeDimension(e.type) != 3 || feTypeCornerCount(e.type) != 8) continue;
            for (const auto& f : faces)
            {
                std::vector<int> key { e.nodes[f[0]], e.nodes[f[1]], e.nodes[f[2]], e.nodes[f[3]] };
                std::sort(key.begin(), key.end());
                faceUse[key].push_back(&e);
            }
        }
        for (const auto& [face, els] : faceUse)
            if (els.size() == 1)
            {
                const auto* e = els.front();
                for (const auto& f : faces)
                {
                    std::vector<int> key { e->nodes[f[0]], e->nodes[f[1]], e->nodes[f[2]], e->nodes[f[3]] };
                    std::sort(key.begin(), key.end());
                    if (key != face) continue;
                    for (int k = 0; k < 4; ++k)
                    {
                        const int a = e->nodes[f[k]], b = e->nodes[f[(k + 1) % 4]];
                        freeEdges.push_back({ { std::min(a, b), std::max(a, b) }, e });
                    }
                }
            }
    }
    // Incompatibilités entre régions : nœud d'une autre région au milieu d'une arête libre
    {
        double avg = 0;
        for (const auto& [ed, e] : freeEdges) avg += distance(P(ed.first), P(ed.second));
        const double cell = freeEdges.empty() ? 1.0 : std::max(1e-3, avg / double(freeEdges.size()));
        std::unordered_map<long long, std::vector<int>> ng;
        auto ck = [&](long x, long y, long z) { return (long long)x * 73856093LL ^ (long long)y * 19349663LL ^ (long long)z * 83492791LL; };
        for (const auto& n : mesh.nodes)
            if (used[n.id]) ng[ck(long(std::floor(n.x / cell)), long(std::floor(n.y / cell)), long(std::floor(n.z / cell)))].push_back(n.id);
        std::set<std::pair<int, int>> reported;
        for (const auto& [ed, e] : freeEdges)
        {
            const P3 a = P(ed.first), b = P(ed.second);
            const long x0 = long(std::floor(std::min(a.x, b.x) / cell)), x1 = long(std::floor(std::max(a.x, b.x) / cell));
            const long y0 = long(std::floor(std::min(a.y, b.y) / cell)), y1 = long(std::floor(std::max(a.y, b.y) / cell));
            const long z0 = long(std::floor(std::min(a.z, b.z) / cell)), z1 = long(std::floor(std::max(a.z, b.z) / cell));
            if ((x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1) > 4096) continue;
            for (long x = x0; x <= x1; ++x)
                for (long y = y0; y <= y1; ++y)
                    for (long z = z0; z <= z1; ++z)
                    {
                        auto it = ng.find(ck(x, y, z));
                        if (it == ng.end()) continue;
                        for (int nid : it->second)
                        {
                            if (nid == ed.first || nid == ed.second) continue;
                            const P3 p = P(nid);
                            if (distanceToSegment(p, a, b) > 1e-6 || distance(p, a) < 1e-6 || distance(p, b) < 1e-6) continue;
                            if (mesh.node(nid).source == e->source) continue;
                            if (!reported.insert({ nid, e->id }).second) continue;
                            issue(QualityIssue::Severity::Warning, "incompatibilité entre régions",
                                  "nœud de " + mesh.node(nid).source + " au milieu d'une arête de " + e->source + " (nœud non partagé)", e, nid, 0);
                        }
                    }
        }
    }
    for (const auto& i : interfaces)
        if (!i.conforming)
        {
            QualityIssue qi;
            qi.severity = QualityIssue::Severity::Warning;
            qi.kind = "interface non conforme";
            qi.message = i.a + " / " + i.b + " : " + i.description;
            qi.source = i.a;
            r.issues.push_back(qi);
            ++r.warnings;
        }

    for (int id = 1; id <= N; ++id)
        if (used[id]) r.estimatedDofs += (usedBy3D[id] && !usedByOther[id]) ? 3 : 6;
    r.aspect = aspect.s, r.minAngle = minAng.s, r.maxAngle = maxAng.s, r.jacobian = jac.s, r.growth = growth.s;
    std::stable_sort(r.issues.begin(), r.issues.end(), [](const QualityIssue& a, const QualityIssue& b) { return int(a.severity) < int(b.severity); });
    return r;
}

} // namespace TSA::Mesh
