#include "ModelCleanup.h"

#include "Model.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace TSA::Model
{

namespace
{
using BarId = std::pair<ElementKind, int>;   // (famille, id) — TSA::Model::Bar existe déjà

const char* prefix(ElementKind k)
{
    switch (k)
    {
    case ElementKind::Beam: return "B";
    case ElementKind::Column: return "C";
    case ElementKind::TrussMember: return "T";
    default: return "E";
    }
}
std::string label(const BarId& b) { return prefix(b.first) + std::to_string(b.second); }
std::string nodeLabel(int id) { return "N" + std::to_string(id); }

MemberTargetType targetOf(ElementKind k)
{
    return k == ElementKind::Column ? MemberTargetType::Column
         : k == ElementKind::TrussMember ? MemberTargetType::Truss : MemberTargetType::Beam;
}

bool ends(const Model& m, const BarId& b, int& s, int& e, gp_Pnt& a, gp_Pnt& c)
{
    switch (b.first)
    {
    case ElementKind::Beam: if (const auto* x = m.getBeam(b.second)) { s = x->startNodeId(); e = x->endNodeId(); } else return false; break;
    case ElementKind::Column: if (const auto* x = m.getColumn(b.second)) { s = x->startNodeId(); e = x->endNodeId(); } else return false; break;
    case ElementKind::TrussMember: if (const auto* x = m.getTrussMember(b.second)) { s = x->startNodeId(); e = x->endNodeId(); } else return false; break;
    default: return false;
    }
    const auto* ns = m.getNode(s);
    const auto* ne = m.getNode(e);
    if (!ns || !ne) return false;
    a = gp_Pnt(ns->x(), ns->y(), ns->z());
    c = gp_Pnt(ne->x(), ne->y(), ne->z());
    return true;
}

std::vector<BarId> allBars(const Model& m)
{
    std::vector<BarId> bars;
    for (const auto& [id, b] : m.beams()) bars.push_back({ ElementKind::Beam, id });
    for (const auto& [id, c] : m.columns()) bars.push_back({ ElementKind::Column, id });
    for (const auto& [id, t] : m.trussMembers()) bars.push_back({ ElementKind::TrussMember, id });
    return bars;
}

bool removeBar(Model& m, const BarId& b)
{
    switch (b.first)
    {
    case ElementKind::Beam: return m.removeBeam(b.second);
    case ElementKind::Column: return m.removeColumn(b.second);
    case ElementKind::TrussMember: return m.removeTrussMember(b.second);
    default: return false;
    }
}

/// Paramètres des points les plus proches de deux droites ; false si parallèles.
bool closest(const gp_Pnt& a1, const gp_Pnt& a2, const gp_Pnt& b1, const gp_Pnt& b2, double& ta, double& tb)
{
    const gp_Vec u(a1, a2), v(b1, b2), w(b1, a1);
    const double a = u.Dot(u), b = u.Dot(v), c = v.Dot(v), d = u.Dot(w), e = v.Dot(w);
    const double den = a * c - b * b;
    if (a < 1e-12 || c < 1e-12 || std::abs(den) < 1e-12 * a * c) return false;
    ta = (b * e - c * d) / den;
    tb = (a * e - b * d) / den;
    return true;
}

constexpr std::size_t kMaxDetails = 200;
void detail(CleanupReport& r, const std::string& s)
{
    if (r.details.size() < kMaxDetails) r.details.push_back(s);
    else if (r.details.size() == kMaxDetails) r.details.push_back("…");
}

// ---------------------------------------------------------------------------------------------

void removeDuplicateBars(Model& m, CleanupReport& r)
{
    std::map<std::tuple<int, int, int>, int> keeper;   // (famille, nœud min, nœud max) → barre conservée
    std::map<std::pair<int, int>, BarId> anyFamily;
    std::vector<std::pair<BarId, int>> duplicates;
    for (const BarId& b : allBars(m))
    {
        int s = 0, e = 0;
        gp_Pnt a, c;
        if (!ends(m, b, s, e, a, c)) continue;
        const int lo = std::min(s, e), hi = std::max(s, e);
        const auto key = std::make_tuple(static_cast<int>(b.first), lo, hi);
        auto it = keeper.find(key);
        if (it == keeper.end())
        {
            keeper[key] = b.second;
            auto other = anyFamily.find({ lo, hi });
            if (other != anyFamily.end())
                r.warnings.push_back(label(b) + " et " + label(other->second) + " relient les mêmes nœuds (" + nodeLabel(lo) + ", "
                                     + nodeLabel(hi) + ") : familles différentes, à vérifier.");
            else
                anyFamily[{ lo, hi }] = b;
        }
        else
            duplicates.push_back({ b, it->second });
    }
    for (const auto& [dup, keep] : duplicates)
    {
        // Charges de la barre supprimée reportées sur la barre conservée.
        const MemberTargetType t = targetOf(dup.first);
        std::vector<int> moved;
        for (const auto& [lid, ml] : m.loadManager().memberLoads())
            if (ml.elementId() == dup.second && ml.targetType() == t) moved.push_back(lid);
        for (int lid : moved)
            if (auto* ml = m.loadManager().getMemberLoad(lid))
            {
                ml->setElementId(keep);
                m.notifyMemberLoadModified(lid);
            }
        if (removeBar(m, dup))
        {
            ++r.duplicateBarsRemoved;
            detail(r, "Barre en double supprimée : " + label(dup) + " (doublon de " + prefix(dup.first) + std::to_string(keep) + ")"
                          + (moved.empty() ? "" : ", " + std::to_string(moved.size()) + " charge(s) reportée(s)"));
        }
    }
}

void connectNodesOnBars(Model& m, double tol, CleanupReport& r)
{
    // Nœuds « utiles » (élément, appui ou charge) : un nœud parasite n'est pas raccordé, il sera supprimé.
    std::set<int> used;
    for (const auto& [id, b] : m.beams()) { used.insert(b.startNodeId()); used.insert(b.endNodeId()); }
    for (const auto& [id, c] : m.columns()) { used.insert(c.startNodeId()); used.insert(c.endNodeId()); }
    for (const auto& [id, t] : m.trussMembers()) { used.insert(t.startNodeId()); used.insert(t.endNodeId()); }
    for (const auto& [id, c] : m.cables()) { used.insert(c.startNodeId()); used.insert(c.endNodeId()); }
    for (const auto& [id, w] : m.walls()) { used.insert(w.startNodeId()); used.insert(w.endNodeId()); }
    for (const auto& [id, s] : m.slabs()) used.insert(s.nodeIds().begin(), s.nodeIds().end());
    for (const auto& [id, f] : m.foundations()) used.insert(f.nodeId());
    for (const auto& [id, nl] : m.loadManager().nodalLoads()) used.insert(nl.nodeId());
    for (const auto& [id, n] : m.nodes())
        if (n.support().isSupported()) used.insert(id);

    // Nœuds triés en x : recherche des candidats dans la boîte englobante de chaque barre.
    std::vector<std::pair<double, int>> byX;
    for (int id : used)
        if (const auto* n = m.getNode(id)) byX.push_back({ n->x(), id });
    std::sort(byX.begin(), byX.end());

    for (const BarId& bar : allBars(m))
    {
        int s = 0, e = 0;
        gp_Pnt a, b;
        if (!ends(m, bar, s, e, a, b)) continue;
        const double L = a.Distance(b);
        if (L <= 2 * tol) continue;
        const gp_Vec ab(a, b);
        std::vector<std::pair<double, int>> onBar;   // (t, nœud)
        auto lo = std::lower_bound(byX.begin(), byX.end(), std::make_pair(std::min(a.X(), b.X()) - tol, -1));
        for (auto it = lo; it != byX.end() && it->first <= std::max(a.X(), b.X()) + tol; ++it)
        {
            const int nid = it->second;
            if (nid == s || nid == e) continue;
            const auto* n = m.getNode(nid);
            const gp_Pnt p(n->x(), n->y(), n->z());
            const double t = gp_Vec(a, p).Dot(ab) / (L * L);
            if (t * L <= tol || (1 - t) * L <= tol) continue;   // confondu avec une extrémité : fusion
            if (p.Distance(a.Translated(ab * t)) <= tol) onBar.push_back({ t, nid });
        }
        if (onBar.empty()) continue;
        std::sort(onBar.begin(), onBar.end());

        BarId current = bar;
        for (const auto& [t0, nid] : onBar)
        {
            int cs = 0, ce = 0;
            gp_Pnt ca, cb;
            if (!ends(m, current, cs, ce, ca, cb)) break;
            const auto* n = m.getNode(nid);
            const gp_Vec cv(ca, cb);
            const double t = gp_Vec(ca, gp_Pnt(n->x(), n->y(), n->z())).Dot(cv) / cv.SquareMagnitude();
            int created = 0;
            if (m.splitBarAt(current.first, current.second, t, nid, &created))
            {
                ++r.nodesConnectedOnBars;
                detail(r, label(current) + " divisée sur " + nodeLabel(nid) + " (nœud posé sur la barre sans lui être relié)");
                current = { current.first, created };
            }
            else
            {
                r.refused.push_back(label(current) + " : division sur " + nodeLabel(nid)
                                    + " refusée (charge ponctuelle ou partielle sur la barre) — à traiter manuellement.");
            }
        }
    }
}

void splitCrossings(Model& m, double tol, CleanupReport& r)
{
    std::vector<BarId> bars = allBars(m);
    for (std::size_t i = 0; i < bars.size(); ++i)
    {
        for (std::size_t j = i + 1; j < bars.size(); ++j)
        {
            int s1, e1, s2, e2;
            gp_Pnt a1, a2, b1, b2;
            if (!ends(m, bars[i], s1, e1, a1, a2) || !ends(m, bars[j], s2, e2, b1, b2)) continue;
            // Boîtes englobantes disjointes : pas de croisement possible.
            if (std::max(a1.X(), a2.X()) + tol < std::min(b1.X(), b2.X()) || std::max(b1.X(), b2.X()) + tol < std::min(a1.X(), a2.X())
                || std::max(a1.Y(), a2.Y()) + tol < std::min(b1.Y(), b2.Y()) || std::max(b1.Y(), b2.Y()) + tol < std::min(a1.Y(), a2.Y())
                || std::max(a1.Z(), a2.Z()) + tol < std::min(b1.Z(), b2.Z()) || std::max(b1.Z(), b2.Z()) + tol < std::min(a1.Z(), a2.Z()))
                continue;
            std::vector<BarId> created;
            const std::size_t before = m.nodes().size();
            const int node = ModelCleanup::connectCrossingBars(m, bars[i].first, bars[i].second, bars[j].first, bars[j].second, tol, &created);
            if (!node) continue;
            if (m.nodes().size() > before)
            {
                ++r.crossingNodesCreated;
                detail(r, "Nœud " + nodeLabel(node) + " créé au croisement de " + label(bars[i]) + " et " + label(bars[j]));
            }
            bars.insert(bars.end(), created.begin(), created.end());
        }
    }
}

void removeOrphanNodes(Model& m, CleanupReport& r)
{
    std::map<int, int> uses;
    auto use = [&](int id) { ++uses[id]; };
    for (const auto& [id, b] : m.beams()) { use(b.startNodeId()); use(b.endNodeId()); }
    for (const auto& [id, c] : m.columns()) { use(c.startNodeId()); use(c.endNodeId()); }
    for (const auto& [id, t] : m.trussMembers()) { use(t.startNodeId()); use(t.endNodeId()); }
    for (const auto& [id, c] : m.cables()) { use(c.startNodeId()); use(c.endNodeId()); }
    for (const auto& [id, w] : m.walls()) { use(w.startNodeId()); use(w.endNodeId()); }
    for (const auto& [id, s] : m.slabs()) for (int n : s.nodeIds()) use(n);
    for (const auto& [id, f] : m.foundations()) use(f.nodeId());
    std::set<int> loaded;
    for (const auto& [id, nl] : m.loadManager().nodalLoads()) loaded.insert(nl.nodeId());

    std::vector<int> orphans;
    for (const auto& [id, n] : m.nodes())
    {
        if (uses.count(id)) continue;
        const bool supported = n.support().isSupported();
        if (supported || loaded.count(id))
        {
            r.warnings.push_back(nodeLabel(id) + " : " + (supported ? "appui" : "charge nodale")
                                 + " sur un nœud relié à aucun élément — conservé, à vérifier.");
            continue;
        }
        orphans.push_back(id);
    }
    for (int id : orphans)
        if (m.removeNode(id))
        {
            ++r.orphanNodesRemoved;
            detail(r, "Nœud parasite supprimé : " + nodeLabel(id));
        }
}
} // namespace

std::string CleanupReport::summary() const
{
    std::ostringstream o;
    o << mergedNodes << " nœud(s) confondu(s) fusionné(s), " << duplicateBarsRemoved << " barre(s) en double supprimée(s), "
      << nodesConnectedOnBars << " nœud(s) raccordé(s) sur des barres, " << crossingNodesCreated << " nœud(s) de croisement, "
      << orphanNodesRemoved << " nœud(s) parasite(s) supprimé(s)";
    if (!refused.empty()) o << " ; " << refused.size() << " opération(s) refusée(s)";
    if (!warnings.empty()) o << " ; " << warnings.size() << " point(s) à vérifier";
    o << ".";
    return o.str();
}

namespace ModelCleanup
{

int connectCrossingBars(Model& m, ElementKind kindA, int idA, ElementKind kindB, int idB, double tol, std::vector<BarId>* created)
{
    int as = 0, ae = 0, bs = 0, be = 0;
    gp_Pnt a1, a2, b1, b2;
    if (!ends(m, { kindA, idA }, as, ae, a1, a2) || !ends(m, { kindB, idB }, bs, be, b1, b2)) return 0;
    double ta = 0, tb = 0;
    if (!closest(a1, a2, b1, b2, ta, tb)) return 0;
    const double ea = tol / std::max(a1.Distance(a2), 1e-9), eb = tol / std::max(b1.Distance(b2), 1e-9);
    if (ta < -ea || ta > 1 + ea || tb < -eb || tb > 1 + eb) return 0;
    const gp_Pnt pa = a1.Translated(gp_Vec(a1, a2) * ta), pb = b1.Translated(gp_Vec(b1, b2) * tb);
    if (pa.Distance(pb) > tol) return 0;
    const bool aEnd = ta <= ea || ta >= 1 - ea, bEnd = tb <= eb || tb >= 1 - eb;
    if (aEnd && bEnd) return 0;   // déjà reliées par une extrémité (ou simple contact)

    // Croisement : nœud créé sur a puis b divisée sur ce nœud. Jonction en T : la barre traversée
    // est divisée sur le nœud d'extrémité existant de l'autre (ses autres éléments restent reliés).
    int node = 0, newBar = 0;
    if (aEnd) node = ta <= ea ? as : ae;
    else if (bEnd) node = tb <= eb ? bs : be;
    if (!aEnd)
    {
        node = m.splitBarAt(kindA, idA, ta, node, &newBar);
        if (!node) return 0;
        if (created) created->push_back({ kindA, newBar });
    }
    if (!bEnd)
    {
        if (!m.splitBarAt(kindB, idB, tb, node, &newBar)) return 0;
        if (created) created->push_back({ kindB, newBar });
    }
    return node;
}

CleanupReport clean(Model& m, const CleanupOptions& o)
{
    CleanupReport r;
    const double tol = std::max(o.tolerance, 1e-9);
    if (o.mergeCoincidentNodes)
    {
        const auto dup = m.findCoincidentNodes(tol);
        for (const auto& [d, k] : dup) detail(r, "Nœuds confondus : " + nodeLabel(d) + " fusionné dans " + nodeLabel(k));
        r.mergedNodes = m.mergeCoincidentNodes(tol);
    }
    if (o.removeDuplicateBars) removeDuplicateBars(m, r);
    if (o.connectNodesOnBars) connectNodesOnBars(m, tol, r);
    if (o.splitCrossingBars) splitCrossings(m, tol, r);
    if (o.removeOrphanNodes) removeOrphanNodes(m, r);
    return r;
}

CleanupReport analyze(const Model& model, const CleanupOptions& o)
{
    Model copy;
    copy.restoreSnapshot(model.createSnapshot());
    return clean(copy, o);
}

} // namespace ModelCleanup

} // namespace TSA::Model
