// Méthode des déplacements : assemblage, conditions d'appui, résolution, efforts et réactions.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace mdd
{

using namespace detail;

namespace
{
const char* dofName(int d) { return d == 0 ? "ux" : d == 1 ? "uy" : "θz"; }

/// Matrice de passage local → global pour 3 DDL : [c -s 0; s c 0; 0 0 1].
void toGlobal(const LocalElement& e, const Vec6& local, Vec6& global)
{
    for (int n = 0; n < 2; ++n)
    {
        const double a = local[3 * n], b = local[3 * n + 1];
        global[3 * n] = e.c * a - e.s * b;
        global[3 * n + 1] = e.s * a + e.c * b;
        global[3 * n + 2] = local[3 * n + 2];
    }
}

void toLocal(const LocalElement& e, const Vec6& global, Vec6& local)
{
    for (int n = 0; n < 2; ++n)
    {
        const double a = global[3 * n], b = global[3 * n + 1];
        local[3 * n] = e.c * a + e.s * b;
        local[3 * n + 1] = -e.s * a + e.c * b;
        local[3 * n + 2] = global[3 * n + 2];
    }
}

/// K_global = Tᵀ k T (6×6).
Mat6 globalStiffness(const LocalElement& e)
{
    Mat6 T {};
    for (int n = 0; n < 2; ++n)
    {
        T[3 * n][3 * n] = e.c;   T[3 * n][3 * n + 1] = e.s;
        T[3 * n + 1][3 * n] = -e.s; T[3 * n + 1][3 * n + 1] = e.c;
        T[3 * n + 2][3 * n + 2] = 1.0;
    }
    Mat6 kt {}, kg {};
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            for (int c = 0; c < 6; ++c) kt[a][b] += e.k[a][c] * T[c][b];
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            for (int c = 0; c < 6; ++c) kg[a][b] += T[c][a] * kt[c][b];
    return kg;
}
} // namespace

Result solve(const Model& model)
{
    Result r;
    const int nn = static_cast<int>(model.nodes.size());
    const int nm = static_cast<int>(model.members.size());
    auto fail = [&](const std::string& msg) { r.success = false; r.message = msg; return r; };

    if (nn == 0 || nm == 0) return fail("Modèle vide : aucun nœud ou aucune barre.");
    for (int m = 0; m < nm; ++m)
    {
        const auto& b = model.members[m];
        if (b.i < 0 || b.j < 0 || b.i >= nn || b.j >= nn || b.i == b.j)
            return fail("Barre " + std::to_string(m + 1) + " : nœuds invalides.");
        if (b.E <= 0 || b.A <= 0 || b.I < 0)
            return fail("Barre " + std::to_string(m + 1) + " : E et A doivent être positifs, I ≥ 0.");
    }

    // 1. Éléments (rigidité + encastrement parfait) — calculés une seule fois.
    std::vector<LocalElement> el(static_cast<std::size_t>(nm));
    for (int m = 0; m < nm; ++m)
    {
        el[m] = buildElement(model, m);
        if (el[m].L <= 1e-9) return fail("Barre " + std::to_string(m + 1) + " de longueur nulle.");
    }

    // 2. DDL bloqués ; rotation sans aucune rigidité (nœud entouré de rotules) bloquée d'office.
    std::vector<std::array<bool, 3>> fixed(static_cast<std::size_t>(nn));
    std::vector<double> rotStiffness(static_cast<std::size_t>(nn), 0.0);
    for (int m = 0; m < nm; ++m)
    {
        rotStiffness[model.members[m].i] += el[m].k[2][2];
        rotStiffness[model.members[m].j] += el[m].k[5][5];
    }
    for (int n = 0; n < nn; ++n)
    {
        const auto& nd = model.nodes[n];
        fixed[n] = { nd.fixX, nd.fixY, nd.fixRz };
        if (!nd.fixRz && rotStiffness[n] <= 0.0 && nd.kRz <= 0.0)
        {
            fixed[n][2] = true;
            r.log.push_back("Nœud " + std::to_string(n + 1) + " : rotation sans rigidité (barres articulées) — DDL θz éliminé.");
        }
    }

    // 3. Numérotation des équations dans l'ordre Cuthill–McKee inverse.
    const std::vector<int> order = reverseCuthillMcKee(nn, model.members);
    std::vector<std::array<int, 3>> eq(static_cast<std::size_t>(nn), { -1, -1, -1 });
    int neq = 0;
    for (int n : order)
        for (int d = 0; d < 3; ++d)
            if (!fixed[n][d]) eq[n][d] = neq++;
    r.equations = neq;
    if (neq == 0) return fail("Tous les degrés de liberté sont bloqués : rien à calculer.");

    int bw = 0;
    for (const auto& b : model.members)
    {
        int lo = neq, hi = -1;
        for (int n : { b.i, b.j })
            for (int d = 0; d < 3; ++d)
                if (eq[n][d] >= 0) { lo = std::min(lo, eq[n][d]); hi = std::max(hi, eq[n][d]); }
        if (hi >= 0) bw = std::max(bw, hi - lo);
    }
    r.halfBandwidth = bw;

    // 4. Assemblage K et second membre F = charges nodales - efforts d'encastrement.
    BandMatrix K(neq, bw);
    std::vector<double> F(static_cast<std::size_t>(neq), 0.0);
    std::vector<std::array<double, 3>> nodal(static_cast<std::size_t>(nn), { 0, 0, 0 });
    for (const auto& p : model.nodalLoads)
    {
        if (p.node < 0 || p.node >= nn) continue;
        nodal[p.node][0] += p.fx;
        nodal[p.node][1] += p.fy;
        nodal[p.node][2] += p.mz;
    }
    for (int n = 0; n < nn; ++n)
        for (int d = 0; d < 3; ++d)
            if (eq[n][d] >= 0) F[eq[n][d]] += nodal[n][d];

    for (int n = 0; n < nn; ++n)   // appuis élastiques
    {
        const auto& nd = model.nodes[n];
        const double kk[3] = { nd.kX, nd.kY, nd.kRz };
        for (int d = 0; d < 3; ++d)
            if (eq[n][d] >= 0 && kk[d] > 0) K.add(eq[n][d], eq[n][d], kk[d]);
    }

    for (int m = 0; m < nm; ++m)
    {
        const auto& b = model.members[m];
        const Mat6 kg = globalStiffness(el[m]);
        Vec6 fg {};
        toGlobal(el[m], el[m].fixedEnd, fg);
        int dofs[6];
        for (int d = 0; d < 3; ++d) { dofs[d] = eq[b.i][d]; dofs[3 + d] = eq[b.j][d]; }
        for (int a = 0; a < 6; ++a)
        {
            if (dofs[a] < 0) continue;
            F[dofs[a]] -= fg[a];
            for (int c = a; c < 6; ++c)
                if (dofs[c] >= 0) K.add(dofs[a], dofs[c], kg[a][c]);   // partie supérieure : chaque paire une fois
        }
    }

    // 5. Résolution.
    int bad = -1;
    if (!K.factorize(&bad))
    {
        for (int n = 0; n < nn; ++n)
            for (int d = 0; d < 3; ++d)
                if (eq[n][d] == bad)
                    return fail("Structure instable (mécanisme ou appuis insuffisants) : DDL " + std::string(dofName(d))
                                + " du nœud " + std::to_string(n + 1) + ".");
        return fail("Structure instable (mécanisme).");
    }
    K.solve(F);

    r.displacements.assign(static_cast<std::size_t>(nn), { 0, 0, 0 });
    for (int n = 0; n < nn; ++n)
        for (int d = 0; d < 3; ++d)
            if (eq[n][d] >= 0) r.displacements[n][d] = F[eq[n][d]];

    // 6. Efforts d'extrémité, réactions (Σ efforts sur les barres - charges nodales), courbes.
    std::vector<std::array<double, 3>> nodeSum(static_cast<std::size_t>(nn), { 0, 0, 0 });
    r.members.resize(static_cast<std::size_t>(nm));
    double loadX = 0.0, loadY = 0.0;
    for (int n = 0; n < nn; ++n) { loadX += nodal[n][0]; loadY += nodal[n][1]; }
    for (int m = 0; m < nm; ++m)
    {
        const auto& b = model.members[m];
        Vec6 dg {}, dl {}, fl {}, fg {};
        for (int d = 0; d < 3; ++d) { dg[d] = r.displacements[b.i][d]; dg[3 + d] = r.displacements[b.j][d]; }
        toLocal(el[m], dg, dl);
        for (int a = 0; a < 6; ++a)
        {
            fl[a] = el[m].fixedEnd[a];
            for (int c = 0; c < 6; ++c) fl[a] += el[m].k[a][c] * dl[c];
        }
        toGlobal(el[m], fl, fg);
        for (int d = 0; d < 3; ++d) { nodeSum[b.i][d] += fg[d]; nodeSum[b.j][d] += fg[3 + d]; }

        MemberEndForces& ef = r.members[m].end;
        ef = { fl[0], fl[1], fl[2], fl[3], fl[4], fl[5] };
        computeCurves(model, m, ef, { dl[0], dl[1], dl[3], dl[4] }, r.members[m]);

        const LoadSet loads(model, m, el[m].L);   // charges de barre, pour l'équilibre global
        const double qx = loads.Px(el[m].L), qy = loads.Py(el[m].L);
        loadX += el[m].c * qx - el[m].s * qy;
        loadY += el[m].s * qx + el[m].c * qy;
    }

    r.reactions.assign(static_cast<std::size_t>(nn), { 0, 0, 0 });
    r.supported.assign(static_cast<std::size_t>(nn), false);
    double reacX = 0.0, reacY = 0.0;
    for (int n = 0; n < nn; ++n)
    {
        const auto& nd = model.nodes[n];
        const bool sup = nd.fixX || nd.fixY || nd.fixRz || nd.kX > 0 || nd.kY > 0 || nd.kRz > 0;
        r.supported[n] = sup;
        if (!sup) continue;
        for (int d = 0; d < 3; ++d) r.reactions[n][d] = nodeSum[n][d] - nodal[n][d];
        reacX += r.reactions[n][0];
        reacY += r.reactions[n][1];
    }
    r.equilibriumResidual = std::hypot(loadX + reacX, loadY + reacY);

    std::ostringstream o;
    o << "Méthode des déplacements : " << neq << " équation(s), demi-largeur de bande " << bw
      << ", résidu d'équilibre " << r.equilibriumResidual << " kN.";
    r.log.push_back(o.str());
    r.success = true;
    r.message = "Calcul terminé.";
    return r;
}

} // namespace mdd
