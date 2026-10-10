#include "NumberingStrategies.h"

#include "../Model/Model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <numeric>

namespace TSA::Topology
{

// ─── Outils ──────────────────────────────────────────────────────────────────────────────────

namespace detail
{

std::map<int, int> clusterRanks(const std::vector<std::pair<int, double>>& values, double tol)
{
    std::vector<std::pair<double, int>> sorted;
    sorted.reserve(values.size());
    for (const auto& [id, v] : values) sorted.emplace_back(v, id);
    std::sort(sorted.begin(), sorted.end());
    std::map<int, int> rank;
    int r = 0;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i > 0 && sorted[i].first - sorted[i - 1].first > tol) ++r;
        rank[sorted[i].second] = r;
    }
    return rank;
}

namespace
{
double coord(const OrderItem& it, char axis)
{
    return axis == 'X' ? it.x : (axis == 'Y' ? it.y : it.z);
}
int axisIndex(char axis)
{
    return axis == 'X' ? 0 : (axis == 'Y' ? 1 : 2);
}
} // namespace

std::vector<int> sortByAxes(const std::vector<OrderItem>& items, const AxisOrder& order, double tol)
{
    std::array<std::map<int, int>, 3> ranks;
    for (int k = 0; k < 3; ++k)
    {
        std::vector<std::pair<int, double>> v;
        v.reserve(items.size());
        for (const auto& it : items) v.emplace_back(it.id, coord(it, order.axes[k]));
        ranks[k] = clusterRanks(v, tol);
    }
    std::vector<int> ids;
    ids.reserve(items.size());
    for (const auto& it : items) ids.push_back(it.id);
    std::sort(ids.begin(), ids.end(), [&](int a, int b) {
        for (int k = 0; k < 3; ++k)
        {
            const bool desc = order.descending[axisIndex(order.axes[k])];
            const int ra = ranks[k].at(a), rb = ranks[k].at(b);
            if (ra != rb) return desc ? ra > rb : ra < rb;
        }
        return a < b; // coordonnées confondues : ordre déterministe par identifiant interne
    });
    return ids;
}

std::map<int, std::vector<int>> adjacency(const TSA::Model::Model& model, const std::set<int>& scope)
{
    std::map<int, std::set<int>> adj;
    for (int id : scope) adj[id];
    auto link = [&](int a, int b) {
        if (a == b || !scope.count(a) || !scope.count(b)) return;
        adj[a].insert(b);
        adj[b].insert(a);
    };
    for (const auto& [id, b] : model.beams()) link(b.startNodeId(), b.endNodeId());
    for (const auto& [id, c] : model.columns()) link(c.startNodeId(), c.endNodeId());
    for (const auto& [id, t] : model.trussMembers()) link(t.startNodeId(), t.endNodeId());
    for (const auto& [id, c] : model.cables()) link(c.startNodeId(), c.endNodeId());
    for (const auto& [id, w] : model.walls()) link(w.startNodeId(), w.endNodeId());
    for (const auto& [id, s] : model.slabs())
    {
        const auto& ns = s.nodeIds();
        for (size_t i = 0; i < ns.size(); ++i) link(ns[i], ns[(i + 1) % ns.size()]);
    }
    std::map<int, std::vector<int>> out;
    for (auto& [id, set] : adj) out[id] = std::vector<int>(set.begin(), set.end());
    return out;
}

int modelDimension(const TSA::Model::Model& model, double tol)
{
    std::vector<std::array<double, 3>> p;
    for (const auto& [id, n] : model.nodes()) p.push_back({ n.x(), n.y(), n.z() });
    if (p.size() < 2) return 0;
    const auto& a = p.front();
    size_t ib = p.size();
    for (size_t i = 1; i < p.size(); ++i)
        if (std::hypot(p[i][0] - a[0], std::hypot(p[i][1] - a[1], p[i][2] - a[2])) > tol) { ib = i; break; }
    if (ib == p.size()) return 0;
    const std::array<double, 3> u{ p[ib][0] - a[0], p[ib][1] - a[1], p[ib][2] - a[2] };
    std::array<double, 3> normal{ 0, 0, 0 };
    bool planar = false;
    for (const auto& q : p)
    {
        const std::array<double, 3> v{ q[0] - a[0], q[1] - a[1], q[2] - a[2] };
        const std::array<double, 3> c{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
        const double len = std::hypot(c[0], std::hypot(c[1], c[2]));
        if (len > tol * std::hypot(u[0], std::hypot(u[1], u[2])))
        {
            normal = { c[0] / len, c[1] / len, c[2] / len };
            planar = true;
            break;
        }
    }
    if (!planar) return 1;
    for (const auto& q : p)
    {
        const double d = (q[0] - a[0]) * normal[0] + (q[1] - a[1]) * normal[1] + (q[2] - a[2]) * normal[2];
        if (std::abs(d) > tol) return 3;
    }
    return 2;
}

} // namespace detail

Availability NodeNumberingStrategy::availability(const TSA::Model::Model&, const NumberingInput&) const
{
    return {};
}

Availability ElementNumberingStrategy::availability(const TSA::Model::Model&, const NumberingInput&) const
{
    return {};
}

namespace
{

using detail::sortByAxes;

std::map<int, int> zLayers(const std::vector<OrderItem>& items, double tol)
{
    std::vector<std::pair<int, double>> v;
    for (const auto& it : items) v.emplace_back(it.id, it.z);
    std::map<int, int> r = detail::clusterRanks(v, tol);
    for (auto& [id, rank] : r) ++rank; // 1..n
    return r;
}

int distinctLayers(const std::map<int, int>& layers)
{
    std::set<int> s;
    for (const auto& [id, l] : layers) s.insert(l);
    return static_cast<int>(s.size());
}

bool hasConnectivity(const TSA::Model::Model& m)
{
    return !m.beams().empty() || !m.columns().empty() || !m.trussMembers().empty() || !m.cables().empty() ||
           !m.walls().empty() || !m.slabs().empty();
}

// ─── Stratégies des nœuds ────────────────────────────────────────────────────────────────────

class SequentialNodes final : public NodeNumberingStrategy
{
public:
    std::string id() const override { return "sequential"; }
    std::string name() const override { return "Séquentielle globale (ordre de création)"; }
    std::string description() const override
    {
        return "Numérote les nœuds dans l'ordre de leur identifiant interne, c'est-à-dire l'ordre de création.";
    }
    Ordering order(const NodeContext& ctx) const override
    {
        Ordering o;
        for (const auto& it : ctx.items) o.order.push_back(it.id);
        std::sort(o.order.begin(), o.order.end());
        return o;
    }
};

/// Tri par coordonnées avec ordre d'axes fixe ou choisi par l'utilisateur.
class AxisSortedNodes final : public NodeNumberingStrategy
{
public:
    AxisSortedNodes(std::string id, std::string name, std::string desc, std::optional<std::string> fixedAxes, bool needs3D)
        : m_id(std::move(id)), m_name(std::move(name)), m_desc(std::move(desc)), m_fixed(std::move(fixedAxes)), m_needs3D(needs3D)
    {
    }
    std::string id() const override { return m_id; }
    std::string name() const override { return m_name; }
    std::string description() const override { return m_desc; }
    Availability availability(const TSA::Model::Model& model, const NumberingInput&) const override
    {
        if (m_needs3D && detail::modelDimension(model, 1e-6) < 3)
        {
            // Couches horizontales : au moins deux cotes distinctes.
            double zmin = 1e300, zmax = -1e300;
            for (const auto& [id, n] : model.nodes())
            {
                zmin = std::min(zmin, n.z());
                zmax = std::max(zmax, n.z());
            }
            if (model.nodes().empty() || zmax - zmin < 1e-6)
                return { false, "Le modèle n'a qu'une seule cote : il n'y a pas de couches à parcourir." };
        }
        return {};
    }
    Ordering order(const NodeContext& ctx) const override
    {
        AxisOrder ord = ctx.settings.order;
        if (m_fixed) AxisOrder::fromCode(*m_fixed, ord);
        Ordering o;
        o.order = sortByAxes(ctx.items, ord, ctx.settings.tolerance);
        o.layer = zLayers(ctx.items, ctx.settings.tolerance);
        return o;
    }

private:
    std::string m_id, m_name, m_desc;
    std::optional<std::string> m_fixed;
    bool m_needs3D;
};

std::string defaultAxisLabel(size_t index, bool letters)
{
    if (!letters) return std::to_string(index + 1);
    std::string s;
    size_t n = index + 1;
    while (n > 0)
    {
        const size_t r = (n - 1) % 26;
        s.insert(s.begin(), static_cast<char>('A' + r));
        n = (n - 1) / 26;
    }
    return s;
}

bool isAlphaLabel(const std::string& s)
{
    return !s.empty() && std::isalpha(static_cast<unsigned char>(s.front()));
}

class GridNodes final : public NodeNumberingStrategy
{
public:
    std::string id() const override { return "grid"; }
    std::string name() const override { return "Grille d'axes (A1, A2, B1…)"; }
    std::string description() const override
    {
        return "Étiquette chaque nœud situé à une intersection de la grille active par le repère de ses axes "
               "(lettre + chiffre), avec la couche en 3D. Les nœuds hors intersection ne reçoivent pas de faux "
               "repère : ils sont numérotés à la suite avec le préfixe.";
    }
    std::string suggestedPattern() const override { return "{g}"; }
    Availability availability(const TSA::Model::Model&, const NumberingInput& input) const override
    {
        if (!input.grid || input.grid->xPositions.empty() || input.grid->yPositions.empty())
            return { false, "Aucune grille cartésienne active avec des axes dans les deux directions." };
        return {};
    }
    Ordering order(const NodeContext& ctx) const override
    {
        Ordering o;
        const GridAxes& g = *ctx.input.grid;
        const double tol = ctx.settings.tolerance;
        const double r = -g.rotationDeg * M_PI / 180.0;
        o.layer = zLayers(ctx.items, tol);
        const bool layered = distinctLayers(o.layer) > 1;

        auto nearestAxis = [tol](const std::vector<double>& pos, double v) -> int {
            int best = -1;
            double bestD = tol;
            for (size_t i = 0; i < pos.size(); ++i)
            {
                const double d = std::abs(pos[i] - v);
                if (d <= bestD)
                {
                    bestD = d;
                    best = static_cast<int>(i);
                }
            }
            return best;
        };
        const bool xLetters = !g.xLabels.empty() ? isAlphaLabel(g.xLabels.front()) : false;
        const bool yLetters = !g.yLabels.empty() ? isAlphaLabel(g.yLabels.front()) : true;
        auto label = [&](const std::vector<std::string>& labels, int i, bool letters) {
            return (i < static_cast<int>(labels.size()) && !labels[i].empty()) ? labels[i] : defaultAxisLabel(i, letters);
        };

        struct OnGrid { int layer, yi, xi, id; };
        std::vector<OnGrid> on;
        std::vector<OrderItem> off;
        for (const auto& it : ctx.items)
        {
            const double dx = it.x - g.originX, dy = it.y - g.originY;
            const double lx = dx * std::cos(r) - dy * std::sin(r);
            const double ly = dx * std::sin(r) + dy * std::cos(r);
            const int xi = nearestAxis(g.xPositions, lx), yi = nearestAxis(g.yPositions, ly);
            if (xi < 0 || yi < 0)
            {
                off.push_back(it);
                continue;
            }
            const std::string xl = label(g.xLabels, xi, xLetters), yl = label(g.yLabels, yi, yLetters);
            // Lettre d'abord (convention A1) : axe à libellé alphabétique en tête.
            std::string ref = (isAlphaLabel(yl) && !isAlphaLabel(xl)) ? yl + xl : xl + yl;
            if (layered) ref += "-" + std::to_string(o.layer.at(it.id));
            o.gridRef[it.id] = ref;
            on.push_back({ o.layer.at(it.id), yi, xi, it.id });
        }
        std::sort(on.begin(), on.end(), [](const OnGrid& a, const OnGrid& b) {
            return std::tie(a.layer, a.yi, a.xi, a.id) < std::tie(b.layer, b.yi, b.xi, b.id);
        });
        // Nœuds confondus sur la même intersection : suffixe .2, .3… (pas d'étiquette en double).
        std::map<std::string, int> seen;
        int coincident = 0;
        for (const auto& e : on)
        {
            o.order.push_back(e.id);
            const int k = ++seen[o.gridRef[e.id]];
            if (k > 1)
            {
                o.gridRef[e.id] += "." + std::to_string(k);
                ++coincident;
            }
        }
        if (coincident)
            o.warnings.push_back(std::to_string(coincident) +
                                 " nœud(s) confondu(s) avec un autre à la même intersection : suffixe .2, .3… ajouté. "
                                 "Envisagez « Fusionner les nœuds ».");
        AxisOrder zyx;
        for (int id : sortByAxes(off, zyx, tol)) o.order.push_back(id);
        if (!off.empty())
            o.warnings.push_back(std::to_string(off.size()) +
                                 " nœud(s) hors des intersections de la grille (tolérance " + std::to_string(tol) +
                                 " m) : numérotés à la suite avec le préfixe, sans repère de grille.");
        return o;
    }
};

class LevelNodes final : public NodeNumberingStrategy
{
public:
    std::string id() const override { return "level"; }
    std::string name() const override { return "Par niveau (plans de travail horizontaux)"; }
    std::string description() const override
    {
        return "Regroupe les nœuds par niveau du projet (ou par cote si aucun niveau n'est défini), puis "
               "numérote chaque niveau par rangées. Le jeton {l} donne le numéro de niveau.";
    }
    Availability availability(const TSA::Model::Model& model, const NumberingInput& input) const override
    {
        if (input.levelElevations.size() >= 2) return {};
        std::set<long long> z;
        for (const auto& [id, n] : model.nodes()) z.insert(std::llround(n.z() * 1000.0));
        if (z.size() >= 2) return {};
        return { false, "Un seul niveau (ou une seule cote) : la numérotation par niveau n'apporte rien." };
    }
    Ordering order(const NodeContext& ctx) const override
    {
        Ordering o;
        const double tol = ctx.settings.tolerance;
        const auto& levels = ctx.input.levelElevations;
        size_t offLevel = 0;
        if (levels.size() >= 2)
        {
            std::vector<double> lv = levels;
            std::sort(lv.begin(), lv.end());
            for (const auto& it : ctx.items)
            {
                size_t best = 0;
                for (size_t i = 1; i < lv.size(); ++i)
                    if (std::abs(lv[i] - it.z) < std::abs(lv[best] - it.z)) best = i;
                if (std::abs(lv[best] - it.z) > tol) ++offLevel;
                o.layer[it.id] = static_cast<int>(best) + 1;
            }
        }
        else
        {
            o.layer = zLayers(ctx.items, tol);
        }
        AxisOrder yx;
        AxisOrder::fromCode("ZYX", yx);
        const std::vector<int> planar = sortByAxes(ctx.items, yx, tol);
        std::map<int, size_t> pos;
        for (size_t i = 0; i < planar.size(); ++i) pos[planar[i]] = i;
        o.order = planar;
        std::stable_sort(o.order.begin(), o.order.end(), [&](int a, int b) { return o.layer.at(a) < o.layer.at(b); });
        if (offLevel)
            o.warnings.push_back(std::to_string(offLevel) + " nœud(s) entre deux niveaux : rattaché(s) au niveau le plus proche.");
        return o;
    }
};

/// Parcours de graphe : BFS, DFS, Cuthill-McKee inverse (réduction de la largeur de bande).
class GraphNodes final : public NodeNumberingStrategy
{
public:
    enum class Kind { Bfs, Dfs, Rcm };
    explicit GraphNodes(Kind k) : m_kind(k) {}
    std::string id() const override { return m_kind == Kind::Bfs ? "bfs" : (m_kind == Kind::Dfs ? "dfs" : "rcm"); }
    std::string name() const override
    {
        switch (m_kind)
        {
        case Kind::Bfs: return "Parcours en largeur (BFS)";
        case Kind::Dfs: return "Parcours en profondeur (DFS)";
        default: return "Connectivité topologique (Cuthill-McKee inverse)";
        }
    }
    std::string description() const override
    {
        switch (m_kind)
        {
        case Kind::Bfs:
            return "Suit les éléments à partir du nœud de départ, niveau de voisinage par niveau de voisinage. "
                   "Chaque partie non reliée est parcourue à son tour.";
        case Kind::Dfs:
            return "Suit les éléments à partir du nœud de départ en allant le plus loin possible avant de revenir. "
                   "Chaque partie non reliée est parcourue à son tour.";
        default:
            return "Ordre de Cuthill-McKee inverse : rapproche les numéros des nœuds reliés (largeur de bande "
                   "réduite), utile pour lire les matrices de rigidité.";
        }
    }
    Availability availability(const TSA::Model::Model& model, const NumberingInput&) const override
    {
        if (!hasConnectivity(model)) return { false, "Aucun élément ne relie les nœuds : pas de graphe à parcourir." };
        return {};
    }
    Ordering order(const NodeContext& ctx) const override
    {
        Ordering o;
        std::set<int> scope;
        for (const auto& it : ctx.items) scope.insert(it.id);
        const auto adj = detail::adjacency(ctx.model, scope);
        AxisOrder zyx;
        const std::vector<int> geo = sortByAxes(ctx.items, zyx, ctx.settings.tolerance);
        std::map<int, size_t> geoRank;
        for (size_t i = 0; i < geo.size(); ++i) geoRank[geo[i]] = i;
        auto degree = [&](int id) { return adj.at(id).size(); };
        auto neighbours = [&](int id) {
            std::vector<int> n = adj.at(id);
            if (m_kind == Kind::Rcm)
                std::sort(n.begin(), n.end(), [&](int a, int b) {
                    return std::make_pair(degree(a), geoRank.at(a)) < std::make_pair(degree(b), geoRank.at(b));
                });
            else
                std::sort(n.begin(), n.end(), [&](int a, int b) { return geoRank.at(a) < geoRank.at(b); });
            return n;
        };

        std::set<int> visited;
        const int userStart = scope.count(ctx.settings.startNodeId) ? ctx.settings.startNodeId : 0;
        if (ctx.settings.startNodeId > 0 && !userStart)
            o.warnings.push_back("Nœud de départ " + std::to_string(ctx.settings.startNodeId) +
                                 " hors de la portée : départ automatique.");
        int components = 0;
        for (size_t gi = 0; gi <= geo.size(); ++gi)
        {
            int start = 0;
            if (components == 0 && userStart) start = userStart;
            else
            {
                // Départ automatique déterministe : premier nœud non visité (ordre géométrique) ;
                // Cuthill-McKee : nœud de degré minimal de cette composante.
                for (int id : geo)
                    if (!visited.count(id)) { start = id; break; }
                if (!start) break;
                if (m_kind == Kind::Rcm)
                {
                    std::vector<int> comp;
                    std::deque<int> q{ start };
                    std::set<int> seen{ start };
                    while (!q.empty())
                    {
                        const int c = q.front();
                        q.pop_front();
                        comp.push_back(c);
                        for (int n : adj.at(c))
                            if (!visited.count(n) && seen.insert(n).second) q.push_back(n);
                    }
                    start = *std::min_element(comp.begin(), comp.end(), [&](int a, int b) {
                        return std::make_pair(degree(a), geoRank.at(a)) < std::make_pair(degree(b), geoRank.at(b));
                    });
                }
            }
            if (visited.count(start)) continue;
            ++components;
            std::vector<int> part;
            if (m_kind == Kind::Dfs)
            {
                std::vector<int> stack{ start };
                while (!stack.empty())
                {
                    const int c = stack.back();
                    stack.pop_back();
                    if (!visited.insert(c).second) continue;
                    part.push_back(c);
                    const auto n = neighbours(c);
                    for (auto it = n.rbegin(); it != n.rend(); ++it)
                        if (!visited.count(*it)) stack.push_back(*it);
                }
            }
            else
            {
                std::deque<int> q{ start };
                visited.insert(start);
                while (!q.empty())
                {
                    const int c = q.front();
                    q.pop_front();
                    part.push_back(c);
                    for (int n : neighbours(c))
                        if (visited.insert(n).second) q.push_back(n);
                }
                if (m_kind == Kind::Rcm) std::reverse(part.begin(), part.end());
            }
            o.order.insert(o.order.end(), part.begin(), part.end());
        }
        if (components > 1)
            o.warnings.push_back(std::to_string(components) + " parties non reliées : parcourues l'une après l'autre.");
        return o;
    }

private:
    Kind m_kind;
};

/// Stratégie dont les prérequis n'existent pas dans TSA : toujours indisponible, avec la raison.
class UnavailableNodes final : public NodeNumberingStrategy
{
public:
    UnavailableNodes(std::string id, std::string name, std::string reason)
        : m_id(std::move(id)), m_name(std::move(name)), m_reason(std::move(reason))
    {
    }
    std::string id() const override { return m_id; }
    std::string name() const override { return m_name; }
    std::string description() const override { return m_reason; }
    Availability availability(const TSA::Model::Model&, const NumberingInput&) const override { return { false, m_reason }; }
    Ordering order(const NodeContext&) const override { return {}; }

private:
    std::string m_id, m_name, m_reason;
};

class KeepNodes final : public NodeNumberingStrategy
{
public:
    std::string id() const override { return "keep"; }
    std::string name() const override { return "Personnalisée / manuelle (conserver les étiquettes)"; }
    std::string description() const override
    {
        return "Ne renumérote pas les nœuds : les étiquettes actuelles, éventuellement saisies à la main dans les "
               "propriétés, sont conservées.";
    }
    Ordering order(const NodeContext& ctx) const override
    {
        Ordering o;
        for (const auto& it : ctx.items) o.order.push_back(it.id);
        return o;
    }
};

const char* kNoGroups = "TSA ne gère pas encore de groupes ni de sous-modèles : rien à quoi rattacher la numérotation.";
const char* kNoMesh = "TSA ne génère pas encore de maillage éléments finis (la commande « Générer maillage » ne fait "
                      "qu'une estimation ; les barres sont calculées sans maillage) : aucun nœud ni élément de maillage "
                      "à numéroter.";

// ─── Stratégies des éléments ─────────────────────────────────────────────────────────────────

int familyRank(EntityFamily f)
{
    return static_cast<int>(f);
}

class SimpleElements final : public ElementNumberingStrategy
{
public:
    enum class Kind { Sequential, ByType, Proximity, Topological, Keep };
    explicit SimpleElements(Kind k) : m_kind(k) {}
    std::string id() const override
    {
        switch (m_kind)
        {
        case Kind::Sequential: return "sequential";
        case Kind::ByType: return "by_type";
        case Kind::Proximity: return "proximity";
        case Kind::Topological: return "topological";
        default: return "keep";
        }
    }
    std::string name() const override
    {
        switch (m_kind)
        {
        case Kind::Sequential: return "Séquentielle (ordre de création)";
        case Kind::ByType: return "Par type d'élément";
        case Kind::Proximity: return "Par proximité géométrique (centres des éléments)";
        case Kind::Topological: return "Par parcours topologique (suit la numérotation des nœuds)";
        default: return "Personnalisée / manuelle (conserver les étiquettes)";
        }
    }
    std::string description() const override
    {
        switch (m_kind)
        {
        case Kind::Sequential: return "Ordre de l'identifiant interne de chaque famille (ordre de création).";
        case Kind::ByType:
            return "Regroupe par famille (poutres, poteaux, treillis, câbles, dalles, voiles, fondations) ; utile avec "
                   "une suite de numéros commune.";
        case Kind::Proximity: return "Trie les éléments par la position de leur centre, selon l'ordre des axes choisi.";
        case Kind::Topological:
            return "Ordonne les éléments selon les nouveaux numéros de leurs nœuds : les éléments voisins ont des "
                   "numéros voisins. Les extrémités et l'orientation des barres ne sont jamais modifiées.";
        default: return "Ne renumérote pas les éléments : les étiquettes actuelles sont conservées.";
        }
    }
    ElementOrdering order(const ElementContext& ctx) const override
    {
        ElementOrdering o;
        std::vector<const ElementItem*> v;
        for (const auto& it : ctx.items) v.push_back(&it);
        auto byFamilyId = [](const ElementItem* a, const ElementItem* b) {
            return std::make_pair(familyRank(a->family), a->id) < std::make_pair(familyRank(b->family), b->id);
        };
        switch (m_kind)
        {
        case Kind::Sequential:
            std::sort(v.begin(), v.end(), [](const ElementItem* a, const ElementItem* b) {
                return std::make_pair(a->id, familyRank(a->family)) < std::make_pair(b->id, familyRank(b->family));
            });
            break;
        case Kind::ByType:
        case Kind::Keep:
            std::sort(v.begin(), v.end(), byFamilyId);
            break;
        case Kind::Proximity:
        {
            std::vector<OrderItem> pts;
            std::map<int, const ElementItem*> byIndex;
            for (size_t i = 0; i < v.size(); ++i)
            {
                pts.push_back({ static_cast<int>(i), v[i]->cx, v[i]->cy, v[i]->cz });
                byIndex[static_cast<int>(i)] = v[i];
            }
            std::vector<const ElementItem*> sorted;
            for (int i : sortByAxes(pts, ctx.settings.order, ctx.settings.tolerance)) sorted.push_back(byIndex[i]);
            v = sorted;
            break;
        }
        case Kind::Topological:
        {
            auto key = [&](const ElementItem* e) {
                int lo = INT32_MAX, hi = -1;
                for (int n : e->nodeIds)
                {
                    auto it = ctx.nodeRank.find(n);
                    const int r = it != ctx.nodeRank.end() ? it->second : INT32_MAX - 1;
                    lo = std::min(lo, r);
                    hi = std::max(hi, r);
                }
                return std::make_tuple(lo, hi, familyRank(e->family), e->id);
            };
            std::sort(v.begin(), v.end(), [&](const ElementItem* a, const ElementItem* b) { return key(a) < key(b); });
            break;
        }
        }
        for (const auto* e : v) o.order.emplace_back(e->family, e->id);
        return o;
    }

private:
    Kind m_kind;
};

class UnavailableElements final : public ElementNumberingStrategy
{
public:
    UnavailableElements(std::string id, std::string name, std::string reason)
        : m_id(std::move(id)), m_name(std::move(name)), m_reason(std::move(reason))
    {
    }
    std::string id() const override { return m_id; }
    std::string name() const override { return m_name; }
    std::string description() const override { return m_reason; }
    Availability availability(const TSA::Model::Model&, const NumberingInput&) const override { return { false, m_reason }; }
    ElementOrdering order(const ElementContext&) const override { return {}; }

private:
    std::string m_id, m_name, m_reason;
};

} // namespace

const std::vector<std::unique_ptr<NodeNumberingStrategy>>& nodeStrategies()
{
    static const std::vector<std::unique_ptr<NodeNumberingStrategy>> list = [] {
        std::vector<std::unique_ptr<NodeNumberingStrategy>> l;
        l.push_back(std::make_unique<SequentialNodes>());
        l.push_back(std::make_unique<AxisSortedNodes>(
            "coordinates", "Par coordonnées (ordre des axes au choix)",
            "Trie les nœuds selon les axes choisis (X-Y-Z, Z-Y-X…), dans le sens choisi pour chaque axe. Les "
            "coordonnées à moins de la tolérance sont regroupées sur une même rangée.",
            std::nullopt, false));
        l.push_back(std::make_unique<AxisSortedNodes>("xyz", "Par coordonnées X, puis Y, puis Z",
                                                      "Trie d'abord selon X, puis Y, puis Z.", std::string("XYZ"), false));
        l.push_back(std::make_unique<AxisSortedNodes>("zyx", "Par coordonnées Z, puis Y, puis X",
                                                      "Trie d'abord selon Z, puis Y, puis X.", std::string("ZYX"), false));
        l.push_back(std::make_unique<AxisSortedNodes>(
            "rows", "Par rangées", "Rangées parallèles à X : de rangée en rangée (Y), puis le long de chaque rangée (X), "
                                   "couche par couche.", std::string("ZYX"), false));
        l.push_back(std::make_unique<AxisSortedNodes>(
            "columns", "Par colonnes", "Colonnes parallèles à Y : de colonne en colonne (X), puis le long de chaque "
                                       "colonne (Y), couche par couche.", std::string("ZXY"), false));
        l.push_back(std::make_unique<AxisSortedNodes>(
            "layers", "Par couches (modèles 3D)",
            "Couche horizontale par couche horizontale (Z), puis par rangées. Avec « Recommencer à chaque couche », "
            "chaque couche repart du numéro de départ (jeton {l} pour distinguer les couches).",
            std::string("ZYX"), true));
        l.push_back(std::make_unique<GridNodes>());
        l.push_back(std::make_unique<LevelNodes>());
        l.push_back(std::make_unique<UnavailableNodes>("group", "Par sous-modèle ou groupe", kNoGroups));
        l.push_back(std::make_unique<GraphNodes>(GraphNodes::Kind::Rcm));
        l.push_back(std::make_unique<GraphNodes>(GraphNodes::Kind::Bfs));
        l.push_back(std::make_unique<GraphNodes>(GraphNodes::Kind::Dfs));
        l.push_back(std::make_unique<UnavailableNodes>("mesh_structured", "Orientée maillage structuré", kNoMesh));
        l.push_back(std::make_unique<UnavailableNodes>("mesh_unstructured", "Orientée maillage non structuré", kNoMesh));
        l.push_back(std::make_unique<KeepNodes>());
        return l;
    }();
    return list;
}

const std::vector<std::unique_ptr<ElementNumberingStrategy>>& elementStrategies()
{
    static const std::vector<std::unique_ptr<ElementNumberingStrategy>> list = [] {
        std::vector<std::unique_ptr<ElementNumberingStrategy>> l;
        l.push_back(std::make_unique<SimpleElements>(SimpleElements::Kind::Sequential));
        l.push_back(std::make_unique<SimpleElements>(SimpleElements::Kind::ByType));
        l.push_back(std::make_unique<SimpleElements>(SimpleElements::Kind::Proximity));
        l.push_back(std::make_unique<SimpleElements>(SimpleElements::Kind::Topological));
        l.push_back(std::make_unique<UnavailableElements>("group", "Par groupe", kNoGroups));
        l.push_back(std::make_unique<UnavailableElements>("mesh_structured", "Par maillage structuré", kNoMesh));
        l.push_back(std::make_unique<UnavailableElements>("mesh_unstructured", "Par maillage non structuré", kNoMesh));
        l.push_back(std::make_unique<SimpleElements>(SimpleElements::Kind::Keep));
        return l;
    }();
    return list;
}

const NodeNumberingStrategy* findNodeStrategy(const std::string& id)
{
    for (const auto& s : nodeStrategies())
        if (s->id() == id) return s.get();
    return nullptr;
}

const ElementNumberingStrategy* findElementStrategy(const std::string& id)
{
    for (const auto& s : elementStrategies())
        if (s->id() == id) return s.get();
    return nullptr;
}

} // namespace TSA::Topology
