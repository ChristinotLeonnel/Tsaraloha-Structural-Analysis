#include "Fem3DAdapter.h"

#include "../../OpenSeesResultsReader.h"

#include "Fem3D/Core/Fem3DError.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Analysis::Fem3DIntegration
{

namespace
{
QString tr(const char* text) { return QCoreApplication::translate("TSA::Analysis::Fem3DAdapter", text); }

std::string elementLabel(const SnapshotElement& el) { return el.key().label(); }

constexpr double kFullLengthTolerance = 1e-9;   ///< a/L ≈ 0 et b/L ≈ 1 : charge sur toute la longueur
} // namespace

AnalysisParameters parametersFromContext(const AnalysisContext& context)
{
    AnalysisParameters p;
    p.type = AnalysisType::LinearStatic;
    p.useKiloNewtons = true;   // Fem3D calcule en kN, m (matériaux, sections et charges convertis ci-dessous)
    p.includeSelfWeight = context.common.includeSelfWeight;
    p.supportRestraints = context.common.supportRestraints;
    p.targetCombinationId = context.combinationId;
    p.targetLoadCaseId = (context.combinationId <= 0 && context.loadCaseIds.size() == 1) ? context.loadCaseIds.front() : 0;
    return p;
}

Conversion convert(const AnalysisContext& context, const AnalysisModel& analysisModel)
{
    Conversion c;
    const CalculationSnapshot& snapshot = analysisModel.snapshot;
    c.params = parametersFromContext(context);
    c.map = OpenSeesModelMap::build(snapshot, c.params);
    Fem3D::StructuralModel3D& m = c.model;

    if (context.combinationId <= 0 && context.loadCaseIds.size() > 1)
        c.errors.push_back(tr("Fem3D calcule un cas, tous les cas superposés ou une combinaison : créez une combinaison pour "
                              "calculer plusieurs cas ensemble.").toStdString());
    if (!snapshot.shells().empty())
        c.outOfScope.push_back(tr("éléments surfaciques (coques) : non calculés par Fem3D").toStdString());

    for (const auto& [id, n] : snapshot.nodes()) m.addNode(id, Fem3D::Vector3(n.x, n.y, n.z));

    // Appuis rigides et ressorts. Un ressort sur un DDL bloqué n'a aucun effet (déplacement nul) : il est ignoré
    // et signalé, ce qui donne le même résultat qu'OpenSees (ressort parallèle à un blocage).
    for (const auto& [id, n] : snapshot.nodes())
    {
        const std::array<bool, 6> fixed { n.fixTx, n.fixTy, n.fixTz, n.fixRx, n.fixRy, n.fixRz };
        if (std::any_of(fixed.begin(), fixed.end(), [](bool f) { return f; })) m.setRestraint(Fem3D::NodeRestraint::custom(id, fixed));
    }
    static const char* const kDofNames[6] = { "Tx", "Ty", "Tz", "Rx", "Ry", "Rz" };
    for (const auto& s : c.map.springs())
    {
        const SnapshotNode* n = snapshot.getNode(s.nodeId);
        if (!n) continue;
        const std::array<bool, 6> fixed { n->fixTx, n->fixTy, n->fixTz, n->fixRx, n->fixRy, n->fixRz };
        Fem3D::NodalSpring spring { s.nodeId, {} };
        for (std::size_t i = 0; i < s.dofs.size(); ++i)
        {
            const int d = s.dofs[i];
            if (fixed[static_cast<std::size_t>(d)])
                c.notes.push_back(tr("Ressort %1 du nœud N%2 sans effet : ce DDL est bloqué.").arg(QString::fromLatin1(kDofNames[d])).arg(s.nodeId).toStdString());
            else
                spring.stiffness[static_cast<std::size_t>(d)] = s.stiffness[i];   // kN/m, kN·m/rad (OpenSeesModelMap, kN)
        }
        if (std::any_of(spring.stiffness.begin(), spring.stiffness.end(), [](double k) { return k > 0.0; })) m.setSpring(spring);
    }

    // Barres : un élément Fem3D par barre, identifiant = tag du snapshot (= tag OpenSees), même v_xz que geomTransf.
    for (const auto& e : c.map.elements())
    {
        const SnapshotElement* el = snapshot.getElementByTag(e.tag);
        if (!el) continue;
        if (!e.isBeamColumn())
        {
            c.outOfScope.push_back(tr("%1 : barre articulée (treillis ou câble), non calculée par Fem3D").arg(QString::fromStdString(elementLabel(*el))).toStdString());
            continue;
        }
        const auto& r0 = el->startRelease;
        const auto& r1 = el->endRelease;
        if (r0.fx || r0.fy || r0.fz || r0.mx || r1.fx || r1.fy || r1.fz || r1.mx)
            c.errors.push_back(tr("%1 : relâchement d'effort normal, tranchant ou de torsion non pris en charge (seules les rotules "
                                  "de flexion My, Mz le sont)").arg(QString::fromStdString(elementLabel(*el))).toStdString());

        const double young = el->material.mechanical.youngModulus * 1e-3;   // Pa → kN/m² (même échelle que le script OpenSees)
        const double nu = el->material.mechanical.poissonRatio;
        const std::string tag = std::to_string(e.tag);
        try
        {
            m.addMaterial({ "M" + tag, young, young / (2.0 * (1.0 + nu)) });
            Fem3D::BeamSection3D s;
            s.name = "S" + tag;
            s.area = el->section.area();
            s.inertiaY = el->section.iy();
            s.inertiaZ = el->section.iz();
            s.torsionConstant = el->section.it();
            m.addSection(s);
        }
        catch (const Fem3D::Fem3DError& error)
        {
            c.errors.push_back(QString::fromStdString(elementLabel(*el) + " : " + error.what()).toStdString());
            continue;
        }
        Fem3D::ElementDefinition d { e.tag, e.nodeI, e.nodeJ, "M" + tag, "S" + tag, Fem3D::BeamFormulation::EulerBernoulli,
                                     Fem3D::Vector3(e.vecxz[0], e.vecxz[1], e.vecxz[2]) };
        d.releases.momentYStart = r0.my;
        d.releases.momentZStart = r0.mz;
        d.releases.momentYEnd = r1.my;
        d.releases.momentZEnd = r1.mz;
        m.addElement(d);

        // Garde-fou : les charges sur barre sont résolues dans les axes de TSA (= axes d'OpenSees) ; Fem3D doit
        // construire exactement les mêmes, sinon les charges seraient projetées sur d'autres axes.
        try
        {
            const auto element = m.createElement(d);
            const auto& ax = element->localAxes();
            const Fem3D::Vector3 tx(e.axes.x[0], e.axes.x[1], e.axes.x[2]), ty(e.axes.y[0], e.axes.y[1], e.axes.y[2]);
            if (!e.axes.valid || (ax.xAxis() - tx).norm() > 1e-9 || (ax.yAxis() - ty).norm() > 1e-9)
                c.errors.push_back(tr("%1 : axes locaux de Fem3D différents de ceux de TSA (calcul refusé)").arg(QString::fromStdString(elementLabel(*el))).toStdString());
        }
        catch (const Fem3D::Fem3DError& error)
        {
            c.errors.push_back(QString::fromStdString(elementLabel(*el) + " : " + error.what()).toStdString());
        }
    }

    // Charges : mêmes motifs (cas, combinaison, poids propre) et mêmes charges locales que le script OpenSees.
    Fem3D::LoadCase3D loadCase { 1, context.combinationId > 0 ? "Combinaison " + std::to_string(context.combinationId) : "Cas", {}, {} };
    for (const auto& pattern : OpenSeesAnalysisBuilder::loadPatterns(snapshot, c.params))
    {
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (pattern.caseId > 0 && nl.loadCaseId() != pattern.caseId) continue;
            if (!snapshot.hasNode(nl.nodeId())) continue;
            const double f = pattern.factor;   // charges TSA en kN
            loadCase.nodalLoads.push_back({ nl.nodeId(), Fem3D::Vector3(nl.fx() * f, nl.fy() * f, nl.fz() * f),
                                            Fem3D::Vector3(nl.mx() * f, nl.my() * f, nl.mz() * f) });
            if (nl.coordSystem() == TSA::Model::LoadCoordSystem::Local)
                c.notes.push_back(tr("Charge nodale « %1 » déclarée locale : appliquée dans les axes globaux (un nœud n'a pas de repère local).")
                                      .arg(QString::fromStdString(nl.name())).toStdString());
        }
        for (const auto& [tag, list] : OpenSeesAnalysisBuilder::beamElementLoads(snapshot, c.params, pattern))
        {
            const SnapshotElement* el = snapshot.getElementByTag(tag);
            for (const auto& l : list)
            {
                c.memberLoads[tag].push_back(l);
                // Positions relatives de TSA → abscisses depuis le nœud i (longueur de la barre du calcul) ; une charge
                // sur toute la longueur reste sans bornes (forme historique, mêmes résultats)
                const double length = el ? el->length : 0.0;
                const bool fullLength = l.relativePosition <= kFullLengthTolerance && l.relativeEnd >= 1.0 - kFullLengthTolerance;
                const double a = fullLength ? 0.0 : l.relativePosition * length;
                const std::optional<double> b = fullLength ? std::nullopt : std::optional<double>(l.relativeEnd * length);
                if (l.kind == BeamElementLoad::Kind::Uniform)
                    loadCase.distributedLoads.push_back({ tag, Fem3D::LoadFrame::Local, Fem3D::Vector3(l.wx, l.wy, l.wz), Fem3D::Vector3(l.wx, l.wy, l.wz) });
                else if (l.kind == BeamElementLoad::Kind::Linear)
                    loadCase.distributedLoads.push_back({ tag, Fem3D::LoadFrame::Local, Fem3D::Vector3(l.wx, l.wy, l.wz), Fem3D::Vector3(l.wxB, l.wyB, l.wzB), a, b });
                else if (l.kind == BeamElementLoad::Kind::Point)
                    loadCase.pointLoads.push_back({ tag, Fem3D::LoadFrame::Local, l.relativePosition * length, Fem3D::Vector3(l.wx, l.wy, l.wz) });
                else if (l.kind == BeamElementLoad::Kind::Couple)
                    loadCase.distributedMoments.push_back({ tag, Fem3D::LoadFrame::Local, Fem3D::Vector3(l.wx, l.wy, l.wz), Fem3D::Vector3(l.wxB, l.wyB, l.wzB), a, b });
            }
        }
        // Charges sur treillis / câbles : sans objet (barres articulées hors périmètre, déjà signalées).
    }
    m.addLoadCase(loadCase);

    // Doublons (plusieurs charges hors périmètre sur la même barre) : une ligne par motif suffit.
    std::sort(c.outOfScope.begin(), c.outOfScope.end());
    c.outOfScope.erase(std::unique(c.outOfScope.begin(), c.outOfScope.end()), c.outOfScope.end());
    std::sort(c.notes.begin(), c.notes.end());
    c.notes.erase(std::unique(c.notes.begin(), c.notes.end()), c.notes.end());

    if (c.computable())
    {
        try { m.validate(); }
        catch (const Fem3D::Fem3DError& error) { c.errors.push_back(std::string("Fem3D : ") + error.what()); }
    }
    return c;
}

ResultsModel mapResults(const Conversion& c, const AnalysisModel& analysisModel, const Fem3D::LoadCaseResults& fr)
{
    const CalculationSnapshot& snapshot = analysisModel.snapshot;
    ResultsModel out;
    out.setAnalysisType(AnalysisType::LinearStatic);
    out.setUnits(UnitSystem::fromKiloNewtons(true));
    out.updateTimestamp();

    for (const auto& n : fr.nodes)
    {
        const auto& v = n.displacement;
        out.setNodeDisplacement(n.nodeId, NodeDisplacement { v[0], v[1], v[2], v[3], v[4], v[5] });
    }
    // Réactions : appuis rigides (nœuds ayant au moins un DDL bloqué) + forces des ressorts, sommées sur le nœud,
    // comme OpenSees reporte la réaction du nœud auxiliaire de chaque ressort sur son nœud TSA.
    std::map<int, NodeReaction> reactions;
    auto add = [&](int nodeId, const std::array<double, 6>& f) {
        NodeReaction& r = reactions[nodeId];
        r.rx += f[0]; r.ry += f[1]; r.rz += f[2]; r.mx += f[3]; r.my += f[4]; r.mz += f[5];
    };
    for (const auto& r : fr.reactions) add(r.nodeId, r.force);
    for (const auto& s : fr.springs) add(s.nodeId, s.force);
    for (const auto& [id, r] : reactions) out.setNodeReaction(id, r);

    // Barres : efforts d'extrémité Fem3D (convention localForce d'OpenSees, validée) → stations par la même
    // reconstruction que pour OpenSees (convention de ResultsModel).
    std::map<int, const Fem3D::ElementResult*> byTag;
    for (const auto& e : fr.elements) byTag[e.elementId] = &e;
    std::vector<double> localForces;
    localForces.reserve(c.map.beamColumnTags().size() * 12);
    for (int tag : c.map.beamColumnTags())
    {
        const auto it = byTag.find(tag);
        for (int k = 0; k < 12; ++k) localForces.push_back(it != byTag.end() ? it->second->endForcesLocal(k) : 0.0);
    }
    for (const auto& [key, res] : OpenSeesResultsReader::beamResultsFromEndForces(snapshot, c.map, c.params, localForces,
                                                                                  out.allDisplacements(), c.memberLoads))
        out.setElementResults(res);

    OpenSeesResultsReader::computeGlobalEquilibrium(snapshot, c.params, out);
    OpenSeesResultsReader::checkAutomaticRestraints(snapshot, c.params, out);

    auto meta = out.executionMetadata();
    meta.solverEngine = "Fem3D";
    meta.solverVersion = Fem3D::kEngineVersion;
    meta.calculationMethod = tr("Méthode des déplacements (Fem3D) : éléments poutre 3D d'Euler–Bernoulli à rigidité exacte, "
                                "charges sur barre (réparties sur tout ou partie de la barre, ponctuelles, moments répartis) par forces nodales "
                                "équivalentes exactes, rotules par condensation statique, "
                                "appuis élastiques sur la diagonale de K, résolution LDLᵀ creuse (Eigen SimplicialLDLT).").toStdString();
    meta.modelBuilder = "Fem3D, 6 DDL par nœud";
    meta.systemSolver = "SimplicialLDLT (Eigen)";
    meta.constraintHandler = tr("Partition DDL libres / imposés").toStdString();
    meta.numberer = "AMD (Eigen)";
    meta.geomTransf = "Linear (convention geomTransf)";
    meta.loadCombinationType = c.params.targetCombinationId > 0 ? "Combinaison #" + std::to_string(c.params.targetCombinationId)
                             : c.params.targetLoadCaseId > 0   ? "Cas de charge #" + std::to_string(c.params.targetLoadCaseId)
                                                               : "Statique Linéaire";
    for (const auto& w : fr.warnings) meta.warnings.push_back("Fem3D : " + w);
    out.setExecutionMetadata(meta);

    std::ostringstream log;
    log << std::scientific << std::setprecision(2);
    log << "[Fem3D] " << Fem3D::kEngineVersion << " : " << fr.nodes.size() << " nœuds, " << fr.elements.size() << " barres ; résidu de résolution "
        << fr.equilibrium.solutionResidual << " ; équilibre |ΣF| = " << fr.equilibrium.forceResidual << ", |ΣM| = " << fr.equilibrium.momentResidual << "\n";
    for (const auto& n : c.notes) log << "[Fem3D] " << n << "\n";
    for (const auto& w : fr.warnings) log << "[AVERTISSEMENT] Fem3D : " << w << "\n";
    out.appendLog(log.str());

    out.computeSummary();
    out.setAvailability(out.availabilityFromData());
    out.setValid(true);
    return out;
}

} // namespace TSA::Analysis::Fem3DIntegration
