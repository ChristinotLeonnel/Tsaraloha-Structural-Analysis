#include "MeshResultField.h"

#include "Solvers/OpenSeesMeshAdapter.h"
#include "../Analysis/LoadResolver.h"
#include "../Analysis/ResultsModel.h"
#include "../Model/Model.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace TSA::Mesh
{

bool MeshResultField::scalar(int id, double* out) const
{
    const auto it = values.find(id);
    if (it == values.end()) return false;
    const auto& v = it->second;
    *out = components == 1 ? v[0] : std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return std::isfinite(*out);
}

std::pair<double, double> MeshResultField::range() const
{
    double lo = std::numeric_limits<double>::max(), hi = -std::numeric_limits<double>::max();
    for (const auto& [id, v] : values)
    {
        double s = 0.0;
        if (!scalar(id, &s)) continue;
        lo = std::min(lo, s);
        hi = std::max(hi, s);
    }
    return lo <= hi ? std::make_pair(lo, hi) : std::make_pair(0.0, 0.0);
}

const MeshResultField* MeshResultSet::find(const std::string& name) const
{
    for (const auto& f : fields)
        if (f.name == name) return &f;
    return nullptr;
}

MeshResultSet ResultsModelMeshAdapter::extract(const FeMesh& mesh) const
{
    using TSA::Analysis::StructuralElementKind;
    MeshResultSet set;
    if (!m_results.isValid())
    {
        set.notes.push_back("résultats absents ou obsolètes : aucun champ");
        return set;
    }
    const auto& meta = m_results.executionMetadata();
    MeshResultField u;
    u.name = "Déplacement";
    u.unit = "m";
    u.source = meta.solverEngine + (meta.solverVersion.empty() ? std::string() : " " + meta.solverVersion);
    u.components = 3;
    std::size_t exact = 0, interpolated = 0;

    // 1. Nœuds confondus avec un nœud TSA calculé : valeurs exactes du solveur.
    for (const auto& n : mesh.nodes)
        if (n.tsaNode > 0)
            if (const auto* d = m_results.getNodeDisplacement(n.tsaNode); d && std::isfinite(d->ux) && std::isfinite(d->uy) && std::isfinite(d->uz))
            {
                u.values[n.id] = { d->ux, d->uy, d->uz };
                ++exact;
            }

    // 1 bis. Nœuds des surfaces calculées en coques : valeurs exactes du solveur (même maillage que le calcul).
    const bool sameMesh = !m_results.surfaceNodeDisplacements().empty() && !m_meshStamp.empty() && m_meshStamp == m_results.surfaceMeshStamp();
    std::size_t surfaceExact = 0;
    if (!m_results.surfaceNodeDisplacements().empty() && !sameMesh)
        set.notes.push_back("maillage affiché différent de celui du calcul : résultats des surfaces non reportés (relancer le calcul)");
    if (sameMesh)
        for (const auto& [id, d] : m_results.surfaceNodeDisplacements())
            if (!u.values.count(id) && id >= 1 && id <= int(mesh.nodes.size()) && std::isfinite(d.ux) && std::isfinite(d.uy) && std::isfinite(d.uz))
            {
                u.values[id] = { d.ux, d.uy, d.uz };
                ++surfaceExact;
            }

    // 2. Nœuds des barres maillées : stations du solveur le long de la barre (repère local → global).
    std::map<std::string, std::vector<int>> memberNodes;
    for (const auto& e : mesh.elements)
        if (feTypeDimension(e.type) == 1 && !e.boundary)
            for (int n : e.nodes) memberNodes[e.source].push_back(n);
    std::unordered_map<int, int> nodeIndex;
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i) nodeIndex[mesh.nodes[i].id] = int(i);
    for (auto& [ref, ids] : memberNodes)
    {
        const auto key = memberKeyFromRef(ref);
        if (!key) continue;
        int a = 0, b = 0;
        double beta = 0.0;
        if (key->kind == StructuralElementKind::Beam) { if (const auto* m = m_model.getBeam(key->id)) { a = m->startNodeId(); b = m->endNodeId(); beta = m->rotation(); } }
        else if (key->kind == StructuralElementKind::Column) { if (const auto* m = m_model.getColumn(key->id)) { a = m->startNodeId(); b = m->endNodeId(); beta = m->rotation(); } }
        else if (key->kind == StructuralElementKind::Truss) { if (const auto* m = m_model.getTrussMember(key->id)) { a = m->startNodeId(); b = m->endNodeId(); } }
        else if (const auto* m = m_model.getCable(key->id)) { a = m->startNodeId(); b = m->endNodeId(); }
        const auto* na = m_model.getNode(a);
        const auto* nb = m_model.getNode(b);
        const auto* er = m_results.getElementResults(*key);
        if (!na || !nb || !er) continue;
        const gp_Pnt p1(na->x(), na->y(), na->z()), p2(nb->x(), nb->y(), nb->z());
        const double L = p1.Distance(p2);
        if (!(L > 1e-9)) continue;
        // Stations triées : extrémités (déplacements nodaux en repère local) et stations intermédiaires.
        std::vector<const TSA::Analysis::StationForces*> st { &er->startForces };
        for (const auto& s : er->intermediateStations) st.push_back(&s);
        st.push_back(&er->endForces);
        std::sort(st.begin() + 1, st.end() - 1, [](const auto* x, const auto* y) { return x->position < y->position; });
        std::vector<double> pos(st.size());
        for (std::size_t k = 0; k < st.size(); ++k) pos[k] = k == 0 ? 0.0 : (k + 1 == st.size() ? L : st[k]->position);
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        const gp_Vec axis = gp_Vec(p1, p2) / L;
        for (int id : ids)
        {
            if (u.values.count(id)) continue; // valeur exacte déjà présente
            const auto it = nodeIndex.find(id);
            if (it == nodeIndex.end()) continue;
            const auto& fn = mesh.nodes[std::size_t(it->second)];
            const gp_Vec rel(p1, gp_Pnt(fn.x, fn.y, fn.z));
            if (rel.Crossed(axis).Magnitude() > 1e-6 * std::max(1.0, L)) continue; // nœud hors de l'axe : jamais deviné
            const double s = std::clamp(rel.Dot(axis), 0.0, L);
            std::size_t k = 1;
            while (k + 1 < pos.size() && pos[k] < s) ++k;
            const double s0 = pos[k - 1], s1 = pos[k], t = s1 > s0 ? (s - s0) / (s1 - s0) : 0.0;
            const auto* A = st[k - 1];
            const auto* B = st[k];
            const gp_Vec g = TSA::Analysis::LoadResolver::localVectorToGlobal(A->ux + (B->ux - A->ux) * t, A->uy + (B->uy - A->uy) * t,
                                                                              A->uz + (B->uz - A->uz) * t, p1, p2, beta);
            if (!std::isfinite(g.X()) || !std::isfinite(g.Y()) || !std::isfinite(g.Z())) continue;
            u.values[id] = { g.X(), g.Y(), g.Z() };
            ++interpolated;
        }
    }
    u.method = std::to_string(exact) + " nœud(s) aux valeurs exactes du solveur (nœuds TSA), " + std::to_string(surfaceExact) +
               " nœud(s) de surfaces aux valeurs exactes du solveur (coques), " + std::to_string(interpolated) +
               " nœud(s) de barres interpolés linéairement entre deux stations du solveur";
    const std::size_t missing = mesh.nodes.size() - u.values.size();
    if (missing > 0)
        set.notes.push_back(std::to_string(missing) + " nœud(s) du maillage sans résultat (surfaces, volumes ou barres non calculés) : "
                                                      "aucune valeur n'est inventée");
    if (!u.values.empty()) set.fields.push_back(std::move(u));

    // 3. Éléments de coque : résultantes par unité de longueur (moyenne des 4 points d'intégration)
    // Efforts des coques : enregistrés au dernier pas seulement ; pour un pas intermédiaire, aucun champ (pas de valeur d'un autre pas)
    const bool intermediateStep = m_results.activeStep() >= 0 && m_results.activeStep() + 1 < m_results.stepCount();
    if (sameMesh && intermediateStep && !m_results.shellResults().empty())
        set.notes.push_back("pas intermédiaire : efforts des coques disponibles au dernier pas seulement (déplacements du pas affichés)");
    if (sameMesh && !intermediateStep && !m_results.shellResults().empty())
    {
        const std::string src = meta.solverEngine + (meta.solverVersion.empty() ? std::string() : " " + meta.solverVersion);
        const char* method = "moyenne des 4 points d'intégration de l'élément, repère local de l'élément (axe x : nœud 1 → nœud 2)";
        struct Def
        {
            const char* name;
            const char* unit;
            int comp;   ///< composante, ou −1 / −2 : moment / effort principal de plus grande valeur absolue
        };
        const Def defs[] = { { "Moment principal (coque)", "kN·m/m", -1 }, { "Effort principal dans le plan (coque)", "kN/m", -2 }, { "Mxx (coque)", "kN·m/m", 3 },
                             { "Myy (coque)", "kN·m/m", 4 },              { "Mxy (coque)", "kN·m/m", 5 },                         { "Nxx (coque)", "kN/m", 0 },
                             { "Nyy (coque)", "kN/m", 1 },                { "Nxy (coque)", "kN/m", 2 },                           { "Vxz (coque)", "kN/m", 6 },
                             { "Vyz (coque)", "kN/m", 7 } };
        auto principal = [](double a, double b, double c) {
            const double m = 0.5 * (a + b), r = std::sqrt(0.25 * (a - b) * (a - b) + c * c);
            return std::abs(m + r) >= std::abs(m - r) ? m + r : m - r;
        };
        for (const auto& d : defs)
        {
            MeshResultField f;
            f.name = d.name;
            f.unit = d.unit;
            f.source = src;
            f.location = FieldLocation::Element;
            f.method = d.comp < 0 ? std::string("valeur principale de plus grande valeur absolue, indépendante du repère local ; ") + method : method;
            for (const auto& [eid, r] : m_results.shellResults())
            {
                if (eid < 1 || eid > int(mesh.elements.size())) continue;
                const auto& m = r.mean;
                const double v = d.comp == -1 ? principal(m[3], m[4], m[5]) : d.comp == -2 ? principal(m[0], m[1], m[2]) : m[std::size_t(d.comp)];
                if (std::isfinite(v)) f.values[eid] = { v, 0.0, 0.0 };
            }
            if (!f.values.empty()) set.fields.push_back(std::move(f));
        }
    }
    return set;
}

} // namespace TSA::Mesh
