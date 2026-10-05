#include "Custom2DAdapter.h"

#include "../../ElementTransformation.h"
#include "../../LoadResolver.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace TSA::Analysis::Custom2D
{

namespace
{
constexpr double kEps = 1e-9;
constexpr double kAligned = 1e-6;     ///< |cos| proche de 1 : axe de section aligné sur la normale
constexpr double kGravity = 9.81;     ///< même valeur que OpenSeesAnalysisBuilder (poids propre)

gp_Vec vec(const gp_Dir& d) { return gp_Vec(d); }

/// Axes locaux 3D identiques à ceux des résultats OpenSees (OpenSeesModelMap).
LocalAxes resultAxes(const SnapshotElement& el, const SnapshotNode& n1, const SnapshotNode& n2)
{
    const gp_Pnt p1(n1.x, n1.y, n1.z), p2(n2.x, n2.y, n2.z);
    const bool bending = el.type == StructuralElementKind::Beam || el.type == StructuralElementKind::Column;
    const gp_Dir z = LoadResolver::computeElementLocalAxes(p1, p2, bending ? el.rotation : 0.0).Direction();
    return ElementTransformation::openSeesAxes({ n1.x, n1.y, n1.z }, { n2.x, n2.y, n2.z }, { z.X(), z.Y(), z.Z() });
}

double dot(const Vec3& a, const gp_Dir& b) { return a[0] * b.X() + a[1] * b.Y() + a[2] * b.Z(); }
double dot(const Vec3& a, const gp_Vec& b) { return a[0] * b.X() + a[1] * b.Y() + a[2] * b.Z(); }

/// Projection d'appuis définis par axe global sur une direction d : bloqué si tous les axes
/// globaux qui y contribuent sont bloqués ; raideur = Σ k_i d_i² (projection exacte d'une
/// raideur diagonale).
void projectSupport(const std::array<bool, 3>& fixed, const std::array<double, 3>& k, const gp_Dir& d,
                    bool& outFixed, double& outK)
{
    const double c[3] = { d.X(), d.Y(), d.Z() };
    bool contributes = false, allFixed = true;
    outK = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(c[i]) <= kAligned) continue;
        contributes = true;
        allFixed &= fixed[i];
        outK += k[i] * c[i] * c[i];
    }
    outFixed = contributes && allFixed;
    if (outFixed) outK = 0.0;
}

/// Direction globale unitaire d'une charge sur barre (même sémantique que LoadResolver) et
/// signe de l'intensité (la gravité est toujours descendante).
gp_Vec memberLoadDirection(const TSA::Model::MemberLoad& load, const gp_Pnt& p1, const gp_Pnt& p2, double rotation)
{
    using TSA::Model::LoadDirection;
    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
    {
        switch (load.direction())
        {
        case LoadDirection::LocalX: return LoadResolver::localVectorToGlobal(1, 0, 0, p1, p2, rotation);
        case LoadDirection::LocalY: return LoadResolver::localVectorToGlobal(0, 1, 0, p1, p2, rotation);
        case LoadDirection::LocalZ: return LoadResolver::localVectorToGlobal(0, 0, 1, p1, p2, rotation);
        default: return LoadResolver::localVectorToGlobal(0, 0, -1, p1, p2, rotation);
        }
    }
    switch (load.direction())
    {
    case LoadDirection::GlobalX: return gp_Vec(1, 0, 0);
    case LoadDirection::GlobalY: return gp_Vec(0, 1, 0);
    case LoadDirection::GlobalZ: return gp_Vec(0, 0, 1);
    default: return gp_Vec(0, 0, -1);   // Gravity
    }
}

/// LoadResolver applique |q| (charge descendante / transversale par défaut) quand la direction
/// n'est pas l'un des axes explicites du repère choisi.
bool usesAbsoluteIntensity(const TSA::Model::MemberLoad& load)
{
    using TSA::Model::LoadDirection;
    const auto d = load.direction();
    if (load.coordSystem() == TSA::Model::LoadCoordSystem::Local)
        return d != LoadDirection::LocalX && d != LoadDirection::LocalY && d != LoadDirection::LocalZ;
    return d != LoadDirection::GlobalX && d != LoadDirection::GlobalY && d != LoadDirection::GlobalZ;
}
} // namespace

Input buildInput(const AnalysisContext& context, const AnalysisModel& model, ValidationResult* diag)
{
    Input in;
    if (!model.plane) return in;
    const AnalysisPlane& pl = *model.plane;
    const auto& snap = model.snapshot;
    auto warn = [&](const std::string& t) { if (diag) diag->addWarning("Custom2D", t); };

    // Nœuds (indices = AnalysisMapping)
    bool coupledSprings = false;
    for (std::size_t i = 0; i < model.mapping.nodeCount(); ++i)
    {
        const int tsaId = model.mapping.nodesByIndex()[i];
        const auto* sn = snap.getNode(tsaId);
        if (!sn) continue;
        Node n;
        n.index = static_cast<int>(i) + 1;
        const auto uv = pl.toPlane(sn->x, sn->y, sn->z);
        n.x = uv.first;
        n.y = uv.second;
        const auto& f = sn->definedFix;
        projectSupport({ f[0], f[1], f[2] }, { sn->kTx, sn->kTy, sn->kTz }, pl.u, n.fixX, n.kX);
        projectSupport({ f[0], f[1], f[2] }, { sn->kTx, sn->kTy, sn->kTz }, pl.v, n.fixY, n.kY);
        projectSupport({ f[3], f[4], f[5] }, { sn->kRx, sn->kRy, sn->kRz }, pl.n, n.fixRz, n.kRz);
        // Ressorts en translation sur un axe oblique au plan : couplage u-v non représentable.
        const double c[3] = { pl.u.X() * pl.v.X(), pl.u.Y() * pl.v.Y(), pl.u.Z() * pl.v.Z() };
        const double coupling = sn->kTx * c[0] + sn->kTy * c[1] + sn->kTz * c[2];
        coupledSprings |= std::abs(coupling) > kEps;
        in.nodes.push_back(n);
    }
    if (coupledSprings)
        warn("Ressorts d'appui obliques au plan : le couplage entre les deux translations du plan est ignoré.");

    // Barres
    std::vector<std::string> unaligned;
    for (const auto& [tag, el] : snap.elements())
    {
        const auto* n1 = snap.getNode(el.startNodeId);
        const auto* n2 = snap.getNode(el.endNodeId);
        if (!n1 || !n2) continue;
        Element e;
        e.index = tag;
        e.nodeI = model.mapping.analysisNode(el.startNodeId);
        e.nodeJ = model.mapping.analysisNode(el.endNodeId);
        const bool bending = el.type == StructuralElementKind::Beam || el.type == StructuralElementKind::Column;
        e.type = bending ? ElementType::Frame : ElementType::Truss;
        e.E = el.material.mechanical.youngModulus * 1e-3;   // Pa → kPa (kN, m)
        e.A = el.section.area();
        e.length = el.length;
        e.weightPerLength = e.A * el.material.density * kGravity * 1e-3;
        if (bending)
        {
            const LocalAxes ax = resultAxes(el, *n1, *n2);
            const double cy = dot(ax.y, pl.n), cz = dot(ax.z, pl.n);
            e.I = el.section.iy() * cy * cy + el.section.iz() * cz * cz;
            if (std::abs(std::abs(cy) - 1.0) > kAligned && std::abs(std::abs(cz) - 1.0) > kAligned)
                unaligned.push_back(el.key().label());
            // Rotule dans le plan : relâchement du moment autour de l'axe local parallèle à n.
            const bool aboutY = std::abs(cy) >= std::abs(cz);
            e.releaseI = aboutY ? el.startRelease.my : el.startRelease.mz;
            e.releaseJ = aboutY ? el.endRelease.my : el.endRelease.mz;
        }
        in.elements.push_back(e);
    }
    if (!unaligned.empty())
    {
        std::string s;
        for (std::size_t i = 0; i < unaligned.size() && i < 6; ++i) s += (i ? ", " : "") + unaligned[i];
        warn("Axes de section non alignés sur la normale au plan (" + s + ") : inertie de flexion projetée.");
    }

    // Charges nodales
    double outOfPlane = 0.0;
    for (const auto& nl : snap.nodalLoads())
    {
        const int idx = model.mapping.analysisNode(nl.nodeId());
        if (!idx) continue;
        const gp_Vec F(nl.fx(), nl.fy(), nl.fz()), M(nl.mx(), nl.my(), nl.mz());
        NodalLoad l;
        l.loadCaseId = nl.loadCaseId();
        l.node = idx;
        l.fx = F.Dot(vec(pl.u));
        l.fy = F.Dot(vec(pl.v));
        l.mz = M.Dot(vec(pl.n));
        outOfPlane = std::max({ outOfPlane, std::abs(F.Dot(vec(pl.n))), std::abs(M.Dot(vec(pl.u))), std::abs(M.Dot(vec(pl.v))) });
        in.nodalLoads.push_back(l);
    }

    // Charges sur barres
    std::size_t skipped = 0;
    for (const auto& ml : snap.memberLoads())
    {
        const auto* el = snap.findElementForLoad(ml);
        if (!el) continue;
        const auto* n1 = snap.getNode(el->startNodeId);
        const auto* n2 = snap.getNode(el->endNodeId);
        if (!n1 || !n2) continue;

        using TSA::Model::LoadType;
        MemberLoad l;
        if (ml.type() == LoadType::MemberUniform || ml.type() == LoadType::MemberLinear)
            l.kind = MemberLoadKind::Distributed;
        else if (ml.type() == LoadType::MemberPoint)
            l.kind = MemberLoadKind::Point;
        else
        {
            ++skipped;
            continue;
        }

        const gp_Pnt p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
        const double L = el->length;
        const gp_Vec d = memberLoadDirection(ml, p1, p2, el->rotation);
        const bool grav = usesAbsoluteIntensity(ml);
        const double q1 = grav ? std::abs(ml.q1()) : ml.q1();
        const double q2 = grav ? std::abs(ml.q2()) : ml.q2();

        // Repère local 2D : x' de i vers j dans le plan, y' = x' tourné de +90°.
        const auto a = pl.toPlane(n1->x, n1->y, n1->z), b = pl.toPlane(n2->x, n2->y, n2->z);
        const double lx = (b.first - a.first) / L, ly = (b.second - a.second) / L;
        const double du = d.Dot(vec(pl.u)), dv = d.Dot(vec(pl.v));
        const double ex = du * lx + dv * ly, ey = -du * ly + dv * lx;
        outOfPlane = std::max(outOfPlane, std::abs(d.Dot(vec(pl.n))) * std::max(std::abs(q1), std::abs(q2)));

        auto pos = [&](double x) { return ml.isRelativePosition() ? x * L : x; };
        l.loadCaseId = ml.loadCaseId();
        l.element = el->tag;
        if (ml.type() == LoadType::MemberUniform)
        {
            l.a = 0.0;
            l.b = L;
            l.px1 = l.px2 = ex * q1;
            l.py1 = l.py2 = ey * q1;
        }
        else if (ml.type() == LoadType::MemberLinear)
        {
            l.a = pos(ml.x1());
            l.b = pos(ml.x2());
            if (l.b <= l.a) l.b = L;
            l.px1 = ex * q1; l.py1 = ey * q1;
            l.px2 = ex * q2; l.py2 = ey * q2;
        }
        else
        {
            l.a = l.b = pos(ml.x1());
            l.px1 = ex * q1;
            l.py1 = ey * q1;
        }
        in.memberLoads.push_back(l);
    }
    if (skipped)
        warn(std::to_string(skipped) + " charge(s) sur barre de type moment réparti ou poids propre explicite ne sont "
                                       "pas transmises au solveur 2D.");
    if (outOfPlane > kEps)
        warn("Composantes de charge hors du plan ignorées (max " + std::to_string(outOfPlane) + ").");

    // Cas, combinaisons, demande
    for (const auto& [id, lc] : snap.loadCases())
        in.loadCases.push_back({ id, lc.name(), lc.isSelfWeightIncluded(), lc.selfWeightFactor() });
    for (const auto& [id, c] : snap.combinations())
        in.combinations.push_back({ id, c.name(), c.caseFactors() });

    // Réglages propres au solveur 2D (fenêtre Analysis, bloc « custom2d »)
    const QJsonObject opts = context.settingsFor("custom2d");
    in.options.axialStiffnessFactor = opts.value("inextensible").toBool(false) ? 1.0e4 : 1.0;
    in.options.curvePoints = std::clamp(opts.value("curvePoints").toInt(41), 3, 2001);

    in.request.type = context.type;
    in.request.loadCaseIds = context.loadCaseIds;
    in.request.combinationId = context.combinationId;
    in.request.includeSelfWeight = context.common.includeSelfWeight;
    const gp_Vec g(0, 0, -1);
    in.request.gravityX = g.Dot(vec(pl.u));
    in.request.gravityY = g.Dot(vec(pl.v));
    if (context.common.includeSelfWeight && std::abs(g.Dot(vec(pl.n))) > kAligned)
        warn("La pesanteur n'est pas contenue dans le plan : seule sa projection dans le plan est appliquée.");
    return in;
}

ResultsModel mapResults(const AnalysisContext& context, const AnalysisModel& model, const Output& out)
{
    ResultsModel r;
    r.setAnalysisType(context.type);
    r.setUnits(UnitSystem::fromKiloNewtons(true));
    for (const auto& line : out.log) r.appendLog(line);
    if (!out.success || !model.plane)
    {
        r.setValid(false);
        return r;
    }
    const AnalysisPlane& pl = *model.plane;
    const auto& snap = model.snapshot;
    std::size_t unknown = 0;

    // Déplacements et réactions : (x, y, θz) du plan → 3D global.
    for (const auto& d : out.displacements)
    {
        const int id = model.mapping.tsaNode(d.node);
        if (!id) { ++unknown; continue; }
        const gp_Vec U = vec(pl.u) * d.ux + vec(pl.v) * d.uy;
        const gp_Vec R = vec(pl.n) * d.rz;
        r.setNodeDisplacement(id, { U.X(), U.Y(), U.Z(), R.X(), R.Y(), R.Z() });
    }
    for (const auto& re : out.reactions)
    {
        const int id = model.mapping.tsaNode(re.node);
        if (!id) { ++unknown; continue; }
        const gp_Vec F = vec(pl.u) * re.fx + vec(pl.v) * re.fy;
        const gp_Vec M = vec(pl.n) * re.mz;
        r.setNodeReaction(id, { F.X(), F.Y(), F.Z(), M.X(), M.Y(), M.Z() });
    }

    // Efforts : repère local 2D → axes locaux 3D des résultats TSA (ceux d'OpenSees).
    for (const auto& ef : out.elementForces)
    {
        const auto key = model.mapping.tsaElement(ef.element);
        const auto* el = key ? snap.findElement(*key) : nullptr;
        const auto* n1 = el ? snap.getNode(el->startNodeId) : nullptr;
        const auto* n2 = el ? snap.getNode(el->endNodeId) : nullptr;
        if (!n1 || !n2) { ++unknown; continue; }

        const LocalAxes ax = resultAxes(*el, *n1, *n2);
        const gp_Vec x3(ax.x[0], ax.x[1], ax.x[2]);
        const gp_Vec t = vec(pl.n).Crossed(x3);    // y' en 3D
        const double ty = dot(ax.y, t), tz = dot(ax.z, t);
        const double ny = dot(ax.y, pl.n), nz = dot(ax.z, pl.n);

        ElementResults er;
        er.elementId = el->id;
        er.kind = el->type;
        er.opsTag = 0;
        er.length = el->length;

        // Convention ResultsModel (identique au lecteur OpenSees) : N intérieur (traction > 0),
        // tranchants et moments = forces d'extrémité exercées sur la barre, axes locaux.
        auto fill = [&](StationForces& s, double pos, double N, double V, double M) {
            s.position = pos;
            s.N = N;
            s.Vy = V * ty;
            s.Vz = V * tz;
            s.My = M * ny;
            s.Mz = M * nz;
        };
        fill(er.startForces, 0.0, -ef.fxI, ef.fyI, ef.mzI);
        fill(er.endForces, el->length, ef.fxJ, ef.fyJ, ef.mzJ);

        // Déplacements locaux aux extrémités, déduits des déplacements nodaux calculés.
        auto localDisp = [&](StationForces& s, int nodeId) {
            const auto* d = r.getNodeDisplacement(nodeId);
            if (!d) return;
            const Vec3 U { d->ux, d->uy, d->uz }, R { d->rx, d->ry, d->rz };
            auto proj = [](const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
            s.ux = proj(U, ax.x); s.uy = proj(U, ax.y); s.uz = proj(U, ax.z);
            s.rx = proj(R, ax.x); s.ry = proj(R, ax.y); s.rz = proj(R, ax.z);
        };
        localDisp(er.startForces, el->startNodeId);
        localDisp(er.endForces, el->endNodeId);

        // Stations fournies par le solveur (efforts intérieurs) ; aucune n'est interpolée ici.
        PlanarMemberCurves curves;
        curves.key = *key;
        curves.length = el->length;
        for (const auto& st : ef.stations)
        {
            StationForces s;
            fill(s, st.x, st.N, st.V, st.M);
            // Déplacements locaux 3D : u selon x', v selon y' (= n × x' en 3D)
            s.ux = st.u;
            s.uy = st.v * ty;
            s.uz = st.v * tz;
            er.intermediateStations.push_back(s);
            // Convention RDM des courbes planes : V = dM/dx = -V(face) du contrat solveur
            curves.points.push_back({ st.x, st.N, -st.V, st.M, st.u, st.v });
        }
        r.setElementResults(er);
        if (ef.hasSummary)
        {
            const auto& sm = ef.summary;
            curves.Mi = sm.Mi; curves.Mj = sm.Mj;
            curves.hasSpanExtremum = sm.hasSpanExtremum;
            curves.xSpanExtremum = sm.xSpanExtremum; curves.MSpanExtremum = sm.MSpanExtremum;
            curves.Mmax = sm.Mmax; curves.xMmax = sm.xMmax; curves.Mmin = sm.Mmin; curves.xMmin = sm.xMmin;
            curves.momentZeros = sm.momentZeros;
            curves.Vi = sm.Vi; curves.Vj = sm.Vj; curves.Nmin = sm.Nmin; curves.Nmax = sm.Nmax;
            curves.deflectionMax = sm.deflectionMax; curves.xDeflectionMax = sm.xDeflectionMax;
            curves.rotationI = sm.rotationI; curves.rotationJ = sm.rotationJ;
        }
        if (!curves.points.empty() || ef.hasSummary) r.setPlanarCurves(curves);
    }

    // Tables propres au solveur : indices 2D → identifiants TSA.
    for (const auto& t : out.customTables)
    {
        EngineResultTable et;
        et.engineId = context.engineId;
        et.title = t.title;
        et.columns = t.columns;
        for (const auto& row : t.rows)
        {
            std::vector<std::string> cells;
            for (std::size_t c = 0; c < row.size(); ++c)
            {
                const int idx = static_cast<int>(row[c]);
                if (static_cast<int>(c) == t.nodeColumn)
                    cells.push_back("N" + std::to_string(model.mapping.tsaNode(idx)));
                else if (static_cast<int>(c) == t.elementColumn)
                {
                    const auto k = model.mapping.tsaElement(idx);
                    cells.push_back(k ? k->label() : "?");
                }
                else
                {
                    std::ostringstream o;
                    o.precision(10);
                    o << row[c];
                    cells.push_back(o.str());
                }
            }
            et.rows.push_back(std::move(cells));
        }
        r.addEngineTable(et);
    }

    if (unknown)
        r.appendLog("[Custom2D] " + std::to_string(unknown) + " résultat(s) d'indice inconnu ignoré(s).");

    auto& meta = r.executionMetadata();
    meta.modelBuilder = "Ossature plane 2D (ux, uy, θz)";
    meta.calculationMethod = out.method;
    if (out.equilibriumResidual >= 0.0)
    {
        meta.maxResidualForce = out.equilibriumResidual;
        meta.isEquilibriumVerified = out.equilibriumResidual <= meta.globalEquilibriumTolerance;
    }
    meta.totalNodes = static_cast<int>(model.mapping.nodeCount());
    meta.totalElements = static_cast<int>(model.mapping.elementCount());
    meta.systemSolver.clear();
    meta.constraintHandler.clear();
    meta.numberer.clear();
    meta.geomTransf.clear();
    r.updateTimestamp();
    r.computeSummary();
    r.setValid(true);
    return r;
}

} // namespace TSA::Analysis::Custom2D
