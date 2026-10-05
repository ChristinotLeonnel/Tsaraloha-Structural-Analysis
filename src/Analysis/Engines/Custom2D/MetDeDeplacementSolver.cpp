#include "MetDeDeplacementSolver.h"

#include <mdd/MetDeDeplacement.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace TSA::Analysis::Custom2D
{

std::string MetDeDeplacementSolver::name() const
{
    return "MetDeDeplacement — méthode des déplacements";
}

std::string MetDeDeplacementSolver::version() const
{
    return mdd::kVersion;
}

Output MetDeDeplacementSolver::solve(const Input& in)
{
    Output out;
    const auto& rq = in.request;

    // Coefficient de chaque cas : combinaison, sinon cas demandés (aucun = tous, superposés).
    auto caseFactor = [&](int caseId) -> double {
        if (rq.combinationId > 0)
        {
            for (const auto& c : in.combinations)
                if (c.id == rq.combinationId)
                {
                    auto it = c.factors.find(caseId);
                    return it != c.factors.end() ? it->second : 0.0;
                }
            return 0.0;
        }
        if (rq.loadCaseIds.empty()) return 1.0;
        return std::find(rq.loadCaseIds.begin(), rq.loadCaseIds.end(), caseId) != rq.loadCaseIds.end() ? 1.0 : 0.0;
    };
    double selfWeight = 0.0;
    if (rq.combinationId > 0)
    {
        for (const auto& lc : in.loadCases)
            if (lc.includeSelfWeight) selfWeight += caseFactor(lc.id) * lc.selfWeightFactor;
    }
    else if (rq.includeSelfWeight)
    {
        selfWeight = 1.0;
    }

    // Modèle de la bibliothèque (indices 0-based)
    mdd::Model m;
    m.options.axialStiffnessFactor = in.options.axialStiffnessFactor;
    m.options.curvePoints = in.options.curvePoints;
    std::vector<int> nodeIndex(in.nodes.size() + 1, -1);
    for (const auto& n : in.nodes)
    {
        if (n.index < 1 || n.index > static_cast<int>(in.nodes.size())) continue;
        nodeIndex[n.index] = static_cast<int>(m.nodes.size());
        m.nodes.push_back({ n.x, n.y, n.fixX, n.fixY, n.fixRz, n.kX, n.kY, n.kRz });
    }
    std::vector<int> memberOf;   // indice bibliothèque → indice Custom2D
    std::vector<int> memberIndex(1, -1);
    for (const auto& e : in.elements)
    {
        if (e.index >= static_cast<int>(memberIndex.size())) memberIndex.resize(e.index + 1, -1);
        const int i = (e.nodeI >= 1 && e.nodeI < static_cast<int>(nodeIndex.size())) ? nodeIndex[e.nodeI] : -1;
        const int j = (e.nodeJ >= 1 && e.nodeJ < static_cast<int>(nodeIndex.size())) ? nodeIndex[e.nodeJ] : -1;
        const bool truss = e.type == ElementType::Truss;
        memberIndex[e.index] = static_cast<int>(m.members.size());
        memberOf.push_back(e.index);
        m.members.push_back({ i, j, e.E, e.A, e.I, truss || e.releaseI, truss || e.releaseJ });

        if (selfWeight != 0.0 && e.weightPerLength > 0.0 && i >= 0 && j >= 0)
        {
            const double dx = m.nodes[j].x - m.nodes[i].x, dy = m.nodes[j].y - m.nodes[i].y;
            const double L = std::hypot(dx, dy);
            if (L > 0)
            {
                const double c = dx / L, s = dy / L, w = e.weightPerLength * selfWeight;
                const double px = w * (rq.gravityX * c + rq.gravityY * s);
                const double py = w * (-rq.gravityX * s + rq.gravityY * c);
                m.distributedLoads.push_back({ memberIndex[e.index], 0.0, L, px, py, px, py });
            }
        }
    }
    auto member = [&](int idx) { return idx >= 0 && idx < static_cast<int>(memberIndex.size()) ? memberIndex[idx] : -1; };

    for (const auto& l : in.nodalLoads)
    {
        const double f = caseFactor(l.loadCaseId);
        if (f == 0.0 || l.node < 1 || l.node >= static_cast<int>(nodeIndex.size()) || nodeIndex[l.node] < 0) continue;
        m.nodalLoads.push_back({ nodeIndex[l.node], f * l.fx, f * l.fy, f * l.mz });
    }
    for (const auto& l : in.memberLoads)
    {
        const double f = caseFactor(l.loadCaseId);
        const int mi = member(l.element);
        if (f == 0.0 || mi < 0) continue;
        if (l.kind == MemberLoadKind::Point)
            m.pointLoads.push_back({ mi, l.a, f * l.px1, f * l.py1 });
        else
            m.distributedLoads.push_back({ mi, l.a, l.b, f * l.px1, f * l.py1, f * l.px2, f * l.py2 });
    }

    const mdd::Result r = mdd::solve(m);
    out.log = r.log;
    out.success = r.success;
    out.message = r.message;
    out.method = "Méthode des déplacements (forme matricielle de la méthode des rotations) — MetDeDeplacement "
                 + std::string(mdd::kVersion)
                 + (in.options.axialStiffnessFactor > 1.0 ? " ; barres quasi inextensibles (EA × 10⁴)" : " ; EA réel");
    if (!r.success) return out;
    out.equilibriumResidual = r.equilibriumResidual;

    for (const auto& n : in.nodes)
    {
        const int k = nodeIndex[n.index];
        if (k < 0) continue;
        const auto& d = r.displacements[k];
        out.displacements.push_back({ n.index, d[0], d[1], d[2] });
        if (r.supported[k])
        {
            const auto& re = r.reactions[k];
            out.reactions.push_back({ n.index, re[0], re[1], re[2] });
        }
    }
    for (std::size_t k = 0; k < r.members.size(); ++k)
    {
        const auto& mr = r.members[k];
        ElementForces ef;
        ef.element = memberOf[k];
        ef.fxI = mr.end.fxI; ef.fyI = mr.end.fyI; ef.mzI = mr.end.mzI;
        ef.fxJ = mr.end.fxJ; ef.fyJ = mr.end.fyJ; ef.mzJ = mr.end.mzJ;
        // Contrat Custom2D : V de station = composante y' de l'effort du tronçon droit (= -dM/dx).
        for (const auto& p : mr.curve) ef.stations.push_back({ p.x, p.N, -p.V, p.M, p.u, p.v });
        const auto& s = mr.summary;
        ef.hasSummary = true;
        ef.summary = { s.length, s.Mi, s.Mj, s.hasSpanExtremum, s.xSpanExtremum, s.MSpanExtremum,
                       s.Mmax, s.xMmax, s.Mmin, s.xMmin, s.momentZeros, s.Vi, s.Vj, s.Nmin, s.Nmax,
                       s.deflectionMax, s.xDeflectionMax, s.rotationI, s.rotationJ };
        out.elementForces.push_back(std::move(ef));
    }
    std::ostringstream o;
    o << "MetDeDeplacement : " << r.equations << " équation(s), demi-bande " << r.halfBandwidth << ".";
    out.log.push_back(o.str());
    return out;
}

} // namespace TSA::Analysis::Custom2D
