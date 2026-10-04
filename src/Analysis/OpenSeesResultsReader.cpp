#include "OpenSeesResultsReader.h"
#include "OpenSeesModelMap.h"
#include "ElementTransformation.h"
#include "LoadResolver.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <regex>

namespace TSA::Analysis
{

namespace
{
std::vector<std::string> readAllLines(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return lines;
    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.find_first_not_of(" \t\r") != std::string::npos) lines.push_back(line);
    }
    return lines;
}

/// Valeurs d'une ligne. Toute valeur non finie (NaN/Inf/IND) est signalée et remplacée par 0 :
/// elle ne doit jamais atteindre le ResultsModel (l'appelant invalide alors les résultats).
std::vector<double> parseDoubles(const std::string& line, bool* hasNonFinite = nullptr)
{
    std::vector<double> vals;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token)
    {
        char* endPtr = nullptr;
        double v = std::strtod(token.c_str(), &endPtr);
        if (endPtr == token.c_str())
        {
            std::string lower = token;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find("nan") != std::string::npos || lower.find("inf") != std::string::npos || lower.find("ind") != std::string::npos)
            {
                if (hasNonFinite) *hasNonFinite = true;
                v = 0.0;
            }
            else
            {
                continue; // jeton non numérique : ignoré
            }
        }
        else if (std::isnan(v) || std::isinf(v))
        {
            if (hasNonFinite) *hasNonFinite = true;
            v = 0.0;
        }
        vals.push_back(v);
    }
    return vals;
}

struct ReadContext
{
    bool nonFinite = false;
    std::vector<std::string> layoutErrors;

    /// Lignes d'un recorder ; chaque ligne doit contenir exactement `expected` valeurs.
    std::vector<std::vector<double>> table(const std::string& path, std::size_t expected, const std::string& what)
    {
        std::vector<std::vector<double>> rows;
        if (expected == 0) return rows;
        for (const auto& line : readAllLines(path))
        {
            auto vals = parseDoubles(line, &nonFinite);
            if (vals.size() != expected)
            {
                layoutErrors.push_back(what + " : " + std::to_string(vals.size()) + " valeurs lues, "
                                       + std::to_string(expected) + " attendues (mapping TSA/OpenSees incohérent).");
                return {};
            }
            rows.push_back(std::move(vals));
        }
        return rows;
    }
};

std::map<int, NodeDisplacement> parseDisplacements(const std::vector<double>& v, const OpenSeesModelMap& map)
{
    std::map<int, NodeDisplacement> out;
    std::size_t idx = 0;
    for (int tag : map.structuralNodeTags())
    {
        NodeDisplacement d;
        d.ux = v[idx]; d.uy = v[idx + 1]; d.uz = v[idx + 2];
        d.rx = v[idx + 3]; d.ry = v[idx + 4]; d.rz = v[idx + 5];
        out[tag] = d;
        idx += 6;
    }
    return out;
}

/// Réactions : les nœuds auxiliaires des ressorts sont reportés (sommés) sur leur nœud TSA.
std::map<int, NodeReaction> parseReactions(const std::vector<double>& v, const OpenSeesModelMap& map)
{
    std::map<int, NodeReaction> out;
    std::size_t idx = 0;
    for (int tag : map.reactionNodeTags())
    {
        const int aux = map.tsaNodeOfAux(tag);
        NodeReaction& r = out[aux ? aux : tag];
        r.rx += v[idx]; r.ry += v[idx + 1]; r.rz += v[idx + 2];
        r.mx += v[idx + 3]; r.my += v[idx + 4]; r.mz += v[idx + 5];
        idx += 6;
    }
    return out;
}

void fillLocalDisplacements(ElementResults& res, const SnapshotElement& el, const CalculationSnapshot& snapshot,
                            const std::map<int, NodeDisplacement>& disps)
{
    const auto* n1 = snapshot.getNode(el.startNodeId);
    const auto* n2 = snapshot.getNode(el.endNodeId);
    if (!n1 || !n2) return;
    const gp_Pnt p1(n1->x, n1->y, n1->z);
    const gp_Pnt p2(n2->x, n2->y, n2->z);
    auto it1 = disps.find(el.startNodeId);
    auto it2 = disps.find(el.endNodeId);
    if (it1 != disps.end())
    {
        const auto& d1 = it1->second;
        auto t = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d1.ux, d1.uy, d1.uz), p1, p2, el.rotation);
        auto r = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d1.rx, d1.ry, d1.rz), p1, p2, el.rotation);
        res.startForces.ux = t.wx; res.startForces.uy = t.wy; res.startForces.uz = t.wz;
        res.startForces.rx = r.wx; res.startForces.ry = r.wy; res.startForces.rz = r.wz;
    }
    if (it2 != disps.end())
    {
        const auto& d2 = it2->second;
        auto t = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d2.ux, d2.uy, d2.uz), p1, p2, el.rotation);
        auto r = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d2.rx, d2.ry, d2.rz), p1, p2, el.rotation);
        res.endForces.ux = t.wx; res.endForces.uy = t.wy; res.endForces.uz = t.wz;
        res.endForces.rx = r.wx; res.endForces.ry = r.wy; res.endForces.rz = r.wz;
    }
}

/// Efforts d'éléments d'un pas : poutres/poteaux depuis localForce (12), treillis/câbles depuis
/// basicForce (1). Les stations intermédiaires conservent le post-traitement historique de TSA.
std::map<ElementKey, ElementResults> buildElementResults(const CalculationSnapshot& snapshot,
                                                         const OpenSeesModelMap& map,
                                                         const std::vector<double>* beamRow,
                                                         const std::vector<double>* axialRow,
                                                         const std::map<int, NodeDisplacement>& disps)
{
    std::map<ElementKey, ElementResults> out;
    std::size_t beamIdx = 0;
    std::size_t axialIdx = 0;

    for (const auto& e : map.elements())
    {
        const auto* el = snapshot.getElementByTag(e.tag);
        if (!el) continue;

        ElementResults res;
        res.elementId = el->id;
        res.kind = el->type;
        res.opsTag = e.tag;
        res.length = el->length;
        fillLocalDisplacements(res, *el, snapshot, disps);

        if (!e.isBeamColumn())
        {
            if (!axialRow) continue;
            const double axial = (*axialRow)[axialIdx++]; // basicForce : traction > 0
            res.startForces.position = 0.0;
            res.startForces.N = axial;
            res.endForces.position = el->length;
            res.endForces.N = axial;

            const int numStations = 5;
            for (int s = 1; s < numStations; ++s)
            {
                const double t = static_cast<double>(s) / numStations;
                StationForces sf;
                sf.position = t * el->length;
                sf.N = axial;
                sf.ux = (1.0 - t) * res.startForces.ux + t * res.endForces.ux;
                sf.uy = (1.0 - t) * res.startForces.uy + t * res.endForces.uy;
                sf.uz = (1.0 - t) * res.startForces.uz + t * res.endForces.uz;
                res.intermediateStations.push_back(sf);
            }
        }
        else
        {
            if (!beamRow) continue;
            const auto& v = *beamRow;
            const std::size_t idx = beamIdx;
            beamIdx += 12;
            // ElasticBeam3d localForce : [N Vy Vz T My Mz]_i puis _j (forces sur l'élément).
            res.startForces.position = 0.0;
            res.startForces.N = -v[idx]; // Convention RDM : traction > 0
            res.startForces.Vy = v[idx + 1];
            res.startForces.Vz = v[idx + 2];
            res.startForces.Mx = v[idx + 3];
            res.startForces.My = v[idx + 4];
            res.startForces.Mz = v[idx + 5];

            res.endForces.position = el->length;
            res.endForces.N = v[idx + 6];
            res.endForces.Vy = v[idx + 7];
            res.endForces.Vz = v[idx + 8];
            res.endForces.Mx = v[idx + 9];
            res.endForces.My = v[idx + 10];
            res.endForces.Mz = v[idx + 11];

            // Interpolation et superposition des charges sur barre (M0, V0) — post-traitement
            // historique, voir known-issues (BUG-016) : non utilisé par l'extraction avancée.
            const int numStations = 20;
            const double L = el->length;
            for (int s = 1; s < numStations; ++s)
            {
                const double t = static_cast<double>(s) / numStations;
                const double x = t * L;
                StationForces sf;
                sf.position = x;
                sf.N = (1.0 - t) * res.startForces.N + t * res.endForces.N;
                sf.Vy = (1.0 - t) * res.startForces.Vy + t * res.endForces.Vy;
                sf.Vz = (1.0 - t) * res.startForces.Vz + t * res.endForces.Vz;
                sf.Mx = (1.0 - t) * res.startForces.Mx + t * res.endForces.Mx;
                sf.My = (1.0 - t) * res.startForces.My + t * res.endForces.My;
                sf.Mz = (1.0 - t) * res.startForces.Mz + t * res.endForces.Mz;

                sf.ux = (1.0 - t) * res.startForces.ux + t * res.endForces.ux;
                sf.uy = (1.0 - t) * res.startForces.uy + t * res.endForces.uy;
                sf.uz = (1.0 - t) * res.startForces.uz + t * res.endForces.uz;
                sf.rx = (1.0 - t) * res.startForces.rx + t * res.endForces.rx;
                sf.ry = (1.0 - t) * res.startForces.ry + t * res.endForces.ry;
                sf.rz = (1.0 - t) * res.startForces.rz + t * res.endForces.rz;

                for (const auto& ml : snapshot.memberLoads())
                {
                    if (snapshot.findElementForLoad(ml) != el) continue;
                    if (ml.type() == TSA::Model::LoadType::MemberPoint)
                    {
                        const double p = ml.q1();
                        double a = ml.isRelativePosition() ? (ml.x1() * L) : ml.x1();
                        a = std::clamp(a, 0.0, L);
                        sf.My += (x <= a) ? p * (1.0 - a / L) * x : p * a * (1.0 - x / L);
                    }
                    else
                    {
                        const double q = ml.q1();
                        sf.My += (q * x * (L - x)) * 0.5;
                        sf.Vz += q * (0.5 * L - x);
                    }
                }
                res.intermediateStations.push_back(sf);
            }
        }
        out[res.key()] = res;
    }
    return out;
}

MatrixMetadata elementMeta(const std::string& name, const std::string& source, const std::string& type,
                           const std::string& cs, const std::string& ordering, const UnitSystem& u,
                           bool exact, const std::string& notes)
{
    MatrixMetadata m;
    m.name = name;
    m.source = source;
    m.matrixType = type;
    m.coordinateSystem = cs;
    m.dofOrdering = ordering;
    m.constraints = "Aucune (matrice élémentaire, avant assemblage)";
    m.solver = "—";
    m.storage = "dense";
    m.units = u.translationalStiffness + " / " + u.rotationalStiffness + " (et termes croisés en " + u.force + "/rad)";
    m.symmetric = true;
    m.exact = exact;
    m.significantDigits = kRecorderPrecision;
    m.notes = notes;
    return m;
}

const char* kElementDofOrdering = "[UX UY UZ RX RY RZ]_i puis [UX UY UZ RX RY RZ]_j";
} // namespace

bool OpenSeesResultsReader::readResults(const std::string& workingDirectory,
                                       const CalculationSnapshot& snapshot,
                                       const AnalysisParameters& params,
                                       const std::string& solverStdOut,
                                       ResultsModel& outResults,
                                       std::string* errorMessage)
{
    return readResults(workingDirectory, snapshot, OpenSeesModelMap::build(snapshot, params), params,
                       solverStdOut, outResults, errorMessage);
}

bool OpenSeesResultsReader::readResults(const std::string& workingDirectory,
                                       const CalculationSnapshot& snapshot,
                                       const OpenSeesModelMap& map,
                                       const AnalysisParameters& params,
                                       const std::string& solverStdOut,
                                       ResultsModel& outResults,
                                       std::string* errorMessage)
{
    std::string savedLog = outResults.journalLog();
    outResults.clear();
    if (!savedLog.empty())
    {
        outResults.appendLog(savedLog);
    }
    outResults.setAnalysisType(params.type);
    outResults.setUnits(UnitSystem::fromKiloNewtons(params.useKiloNewtons));
    outResults.updateTimestamp();

    if (params.type == AnalysisType::Modal)
    {
        parseModalOutput(solverStdOut, outResults);
        outResults.setValid(!outResults.modalModes().empty());
        outResults.computeSummary();
        return outResults.isValid();
    }

    const std::string dir = workingDirectory + "/";
    ReadContext ctx;
    const auto dispRows = ctx.table(dir + params.dispOutputFile, map.structuralNodeTags().size() * 6, "Déplacements");
    const auto reactRows = ctx.table(dir + params.reactOutputFile, map.reactionNodeTags().size() * 6, "Réactions");
    const auto beamRows = ctx.table(dir + params.forceOutputFile, map.beamColumnTags().size() * 12, "Efforts locaux poutres/poteaux");
    const auto axialRows = ctx.table(dir + params.axialOutputFile, map.axialTags().size(), "Efforts normaux treillis/câbles");

    if (ctx.nonFinite)
    {
        if (errorMessage)
        {
            *errorMessage = "Instabilité numérique ou matrice de rigidité singulière : des valeurs infinies ou indéterminées (NaN/Inf) ont été détectées dans la réponse structurale OpenSees.";
        }
        outResults.appendLog("\n[ERREUR NORMATIVE] Instabilité numérique détectée : les déplacements ou réactions contiennent des valeurs non finies (NaN/Inf).\n");
        outResults.setValid(false);
        return false;
    }
    if (!ctx.layoutErrors.empty())
    {
        std::string msg = "Résultats OpenSees incohérents avec le modèle de calcul :";
        for (const auto& e : ctx.layoutErrors) msg += "\n  • " + e;
        if (errorMessage) *errorMessage = msg;
        outResults.appendLog("\n[ERREUR] " + msg + "\n");
        outResults.setValid(false);
        return false;
    }

    if (!dispRows.empty())
    {
        for (const auto& [id, d] : parseDisplacements(dispRows.back(), map))
            outResults.setNodeDisplacement(id, d);
    }
    if (!reactRows.empty())
    {
        for (const auto& [id, r] : parseReactions(reactRows.back(), map))
            outResults.setNodeReaction(id, r);
    }

    const bool hasBeams = map.beamColumnTags().empty() || !beamRows.empty();
    const bool hasAxial = map.axialTags().empty() || !axialRows.empty();
    if (!map.elements().empty() && hasBeams && hasAxial)
    {
        const auto finalElements = buildElementResults(snapshot, map,
                                                       beamRows.empty() ? nullptr : &beamRows.back(),
                                                       axialRows.empty() ? nullptr : &axialRows.back(),
                                                       outResults.allDisplacements());
        for (const auto& [key, res] : finalElements)
            outResults.setElementResults(res);
    }

    // Historique des pas (non linéaire) : toutes les tables ont une ligne par pas enregistré.
    if (dispRows.size() > 1)
    {
        for (std::size_t stepIdx = 0; stepIdx < dispRows.size(); ++stepIdx)
        {
            TimeHistoryStep thStep;
            thStep.time = static_cast<double>(stepIdx);
            StepResults stepRes;
            stepRes.stepNumber = static_cast<int>(stepIdx + 1);
            stepRes.factorOrTime = static_cast<double>(stepIdx + 1) / static_cast<double>(dispRows.size());
            stepRes.displacements = parseDisplacements(dispRows[stepIdx], map);
            thStep.displacements = stepRes.displacements;
            if (stepIdx < reactRows.size())
                stepRes.reactions = parseReactions(reactRows[stepIdx], map);
            const bool beamOk = map.beamColumnTags().empty() || stepIdx < beamRows.size();
            const bool axialOk = map.axialTags().empty() || stepIdx < axialRows.size();
            if (beamOk && axialOk)
            {
                stepRes.elementResults = buildElementResults(snapshot, map,
                    stepIdx < beamRows.size() ? &beamRows[stepIdx] : nullptr,
                    stepIdx < axialRows.size() ? &axialRows[stepIdx] : nullptr,
                    stepRes.displacements);
            }
            outResults.addTimeHistoryStep(thStep);
            outResults.addStepResults(stepRes);
        }
    }

    // Efforts bruts (Advanced) : globalForce (tous éléments) et basicForce (poutres/poteaux).
    if (params.extractionLevel == ExtractionLevel::Advanced)
    {
        const auto globalRows = ctx.table(dir + params.globalForceOutputFile, map.allElementTags().size() * 12, "Forces globales");
        const auto basicRows = ctx.table(dir + params.basicForceOutputFile, map.beamColumnTags().size() * 6, "Forces basiques");
        auto& adv = outResults.advanced();
        std::size_t gIdx = 0, bIdx = 0, beamIdx = 0, axialIdx = 0;
        for (const auto& e : map.elements())
        {
            ElementForceSet f;
            f.key = e.key;
            f.opsTag = e.tag;
            if (!globalRows.empty())
            {
                f.global.assign(globalRows.back().begin() + gIdx, globalRows.back().begin() + gIdx + 12);
            }
            gIdx += 12;
            if (e.isBeamColumn())
            {
                if (!beamRows.empty())
                    f.local.assign(beamRows.back().begin() + beamIdx, beamRows.back().begin() + beamIdx + 12);
                f.localSource = "OpenSees API: localForce (ElasticBeam3d)";
                beamIdx += 12;
                if (!basicRows.empty())
                    f.basic.assign(basicRows.back().begin() + bIdx, basicRows.back().begin() + bIdx + 6);
                f.basicLabels = { "N", "Mz_i", "Mz_j", "My_i", "My_j", "T" };
                bIdx += 6;
            }
            else
            {
                if (!f.global.empty() && e.axes.valid)
                {
                    Vec12 g {};
                    std::copy(f.global.begin(), f.global.end(), g.begin());
                    const Vec12 l = ElementTransformation::globalToLocal(e.axes, g);
                    f.local.assign(l.begin(), l.end());
                    f.localSource = "Reconstructed (exact): T · globalForce OpenSees";
                }
                if (!axialRows.empty())
                    f.basic = { axialRows.back()[axialIdx] };
                f.basicLabels = { "N" };
                ++axialIdx;
            }
            adv.elementForces[e.key] = std::move(f);
        }
        adv.springs = map.springs();
        if (ctx.nonFinite || !ctx.layoutErrors.empty())
        {
            adv.elementForces.clear();
            adv.warnings.push_back("Forces brutes ignorées : valeurs non finies ou disposition incohérente.");
            ctx.layoutErrors.clear();
            ctx.nonFinite = false;
        }
    }

    computeGlobalEquilibrium(snapshot, params, outResults);
    outResults.computeSummary();

    if (dispRows.empty() && reactRows.empty() && outResults.allElementResults().empty())
    {
        if (errorMessage)
        {
            *errorMessage = "Les fichiers de résultats d'OpenSees n'ont pas pu être lus ou sont vides.";
        }
        outResults.setValid(false);
        return false;
    }

    outResults.setValid(true);
    return true;
}

bool OpenSeesResultsReader::readMatrixResults(const std::string& workingDirectory,
                                             const CalculationSnapshot& snapshot,
                                             const OpenSeesModelMap& map,
                                             const AnalysisParameters& params,
                                             bool globalStiffnessRequested,
                                             ResultsModel& outResults,
                                             std::string* errorMessage)
{
    (void)snapshot;
    const std::string dir = workingDirectory + "/";
    auto& adv = outResults.advanced();
    const UnitSystem& units = outResults.units();
    const std::string handler = toTclString(params.constraintHandler);

    // 1. Mapping des DDL (nodeDOFs) — indispensable : sans lui aucune matrice n'est interprétable.
    DofMap dm;
    dm.numberer = "RCM";
    dm.constraintHandler = handler;
    dm.source = "OpenSees API: nodeDOFs (passage matrices, même handler et numberer que l'analyse)";
    std::map<int, DofEquation> byEquation;
    bool bad = false;
    for (const auto& line : readAllLines(dir + params.dofMapOutputFile))
    {
        auto v = parseDoubles(line, &bad);
        if (v.size() != 7) { bad = true; break; }
        const int node = static_cast<int>(v[0]);
        std::vector<int> eqs;
        for (int d = 0; d < 6; ++d)
        {
            const int eq = static_cast<int>(v[1 + d]);
            eqs.push_back(eq);
            if (eq >= 0)
            {
                if (byEquation.count(eq)) { bad = true; break; }
                byEquation[eq] = DofEquation{ eq, node, d };
            }
        }
        dm.nodeEquations[node] = eqs;
    }
    // Équations attendues : 0..n-1 sans trou (les nœuds auxiliaires sont entièrement fixés).
    int expected = 0;
    for (const auto& [eq, d] : byEquation)
    {
        if (eq != expected++) { bad = true; break; }
        dm.equations.push_back(d);
    }
    if (bad || dm.nodeEquations.size() != map.structuralNodeTags().size())
    {
        if (errorMessage) *errorMessage = "Mapping des DDL OpenSees absent ou incohérent (nodeDOFs).";
        adv.warnings.push_back("Mapping des DDL OpenSees absent ou incohérent : matrices ignorées.");
        return false;
    }
    adv.dofMap = dm;

    // 2. Rigidités élémentaires : k_basic d'OpenSees, puis transformations exactes.
    ReadContext ctx;
    const auto kbBeam = ctx.table(dir + params.beamBasicStiffnessOutputFile, map.basicStiffnessBeamTags().size() * 36, "basicStiffness poutres");
    const auto kbTruss = ctx.table(dir + params.trussBasicStiffnessOutputFile, map.basicStiffnessTrussTags().size(), "basicStiffness treillis");
    const bool linearTransf = params.geomTransf == GeomTransfType::Linear;
    std::size_t bIdx = 0, tIdx = 0;
    for (const auto& e : map.elements())
    {
        ElementMatrices m;
        m.key = e.key;
        m.opsTag = e.tag;
        m.opsClass = e.opsClass;
        m.nodeI = e.nodeI;
        m.nodeJ = e.nodeJ;
        m.length = e.axes.length;
        for (int c = 0; c < 3; ++c)
        {
            m.rotation[c] = e.axes.x[c];
            m.rotation[3 + c] = e.axes.y[c];
            m.rotation[6 + c] = e.axes.z[c];
        }

        const std::string label = e.key.label() + " (tag OpenSees " + std::to_string(e.tag) + ")";
        if (!e.axes.valid)
        {
            m.unavailableReason = "Repère local indéfini (longueur nulle).";
        }
        else if (e.isBeamColumn() && !kbBeam.empty())
        {
            m.kBasic = DenseMatrix(6, 6);
            for (int i = 0; i < 36; ++i) m.kBasic.data[i] = kbBeam.back()[bIdx + i];
            m.available = true;
        }
        else if (e.opsClass == "truss" && !kbTruss.empty())
        {
            m.kBasic = DenseMatrix(1, 1);
            m.kBasic.data[0] = kbTruss.back()[tIdx];
            m.available = true;
        }
        else if (e.opsClass == "corotTruss")
        {
            m.unavailableReason = "CorotTruss n'expose pas de réponse basicStiffness dans OpenSees 3.8.0 "
                                  "(sa contribution figure dans K_global).";
        }
        else
        {
            m.unavailableReason = "Rigidité basique non lue (fichier absent ou incohérent).";
        }
        if (e.isBeamColumn()) bIdx += 36;
        if (e.opsClass == "truss") ++tIdx;

        if (m.available)
        {
            const bool beam = e.isBeamColumn();
            m.kBasicMeta = elementMeta("k_basic " + label, "OpenSees API: basicStiffness (recorder Element)",
                "Initial tangent (état de référence)", "Basic",
                beam ? "[N, Mz_i, Mz_j, My_i, My_j, T]" : "[N]", units, true,
                beam ? "ElasticBeam3d : déformations basiques [allongement, θz_i, θz_j, θy_i, θy_j, torsion]."
                     : "Truss : EA/L.");
            m.kLocal = beam ? ElementTransformation::beamBasicToLocalStiffness(m.kBasic, e.axes.length)
                            : ElementTransformation::trussBasicToLocalStiffness(m.kBasic.data[0]);
            m.kGlobal = ElementTransformation::localToGlobalStiffness(e.axes, m.kLocal);
            const std::string transfNote = linearTransf
                ? "Transformation algébrique exacte de k_basic (LinearCrdTransf3d, sans excentricité)."
                : "geomTransf non linéaire : rigidité géométrique non incluse (valide à l'état de référence non chargé seulement).";
            m.kLocalMeta = elementMeta("k_local " + label, "Reconstructed: T_blᵀ · k_basic · T_bl (k_basic OpenSees)",
                "Initial tangent (état de référence)", "Local", kElementDofOrdering, units,
                beam ? linearTransf : true, transfNote);
            m.kGlobalMeta = elementMeta("K_global " + label, "Reconstructed: Tᵀ · k_local · T (k_basic OpenSees)",
                "Initial tangent (état de référence)", "Global", kElementDofOrdering, units,
                beam ? linearTransf : true, transfNote);
        }
        adv.elementMatrices[e.key] = std::move(m);
    }
    if (!ctx.layoutErrors.empty() || ctx.nonFinite)
    {
        for (auto& [k, m] : adv.elementMatrices)
        {
            m.available = false;
            m.unavailableReason = "Rigidités basiques incohérentes ou non finies.";
        }
        adv.warnings.push_back("Rigidités élémentaires ignorées (fichier incohérent ou valeurs non finies).");
    }

    // 3. K_global du système d'équations (printA -ret, system FullGeneral).
    const int n = dm.equationCount();
    if (!globalStiffnessRequested)
    {
        adv.kGlobalUnavailableReason = "Non extraite : " + std::to_string(map.estimatedFreeDofs())
            + " DDL libres estimés > plafond (" + std::to_string(params.maxGlobalStiffnessDofs)
            + "). OpenSees 3.8.0 n'expose la matrice que pour system FullGeneral (dense n×n).";
    }
    else
    {
        std::vector<double> values;
        bool nonFinite = false;
        for (const auto& line : readAllLines(dir + params.globalStiffnessOutputFile))
        {
            auto v = parseDoubles(line, &nonFinite);
            values.insert(values.end(), v.begin(), v.end());
        }
        if (nonFinite)
        {
            adv.kGlobalUnavailableReason = "K_global contient des valeurs non finies : ignorée.";
        }
        else if (values.size() != static_cast<std::size_t>(n) * n || n == 0)
        {
            adv.kGlobalUnavailableReason = "K_global : " + std::to_string(values.size()) + " coefficients lus, "
                + std::to_string(static_cast<long long>(n) * n) + " attendus (n = " + std::to_string(n) + ").";
        }
        else
        {
            SparseMatrix k;
            k.rows = n;
            k.cols = n;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                {
                    const double v = values[static_cast<std::size_t>(i) * n + j];
                    if (v != 0.0) { k.rowIndex.push_back(i); k.colIndex.push_back(j); k.values.push_back(v); }
                }
            bool symmetric = true;
            for (std::size_t q = 0; q < k.values.size() && symmetric; ++q)
            {
                const double a = k.values[q];
                const double b = k.at(k.colIndex[q], k.rowIndex[q]);
                symmetric = std::abs(a - b) <= 1e-9 * std::max(std::abs(a), std::abs(b)) + 1e-300;
            }
            adv.kGlobal = std::move(k);
            adv.hasGlobalStiffness = true;
            MatrixMetadata& m = adv.kGlobalMeta;
            m.name = "K_global";
            m.source = "OpenSees API: printA -ret (system FullGeneral, passage matrices séparé)";
            m.matrixType = "Initial tangent (formTangent à l'état de référence non déformé)";
            m.coordinateSystem = "Reduced global (equations)";
            m.dofOrdering = "Équations OpenSees 0..n-1 (numberer RCM) ; correspondance nœud/DDL : DofMap";
            m.constraints = "Après constraints " + handler + " : DDL fixés éliminés (non présents)";
            m.solver = "FullGeneral (extraction) ; l'analyse principale utilise " + std::string(toTclString(params.systemSolver));
            m.storage = "sparse COO (coefficients non nuls de la matrice dense OpenSees)";
            m.units = units.translationalStiffness + " / " + units.rotationalStiffness;
            m.symmetric = symmetric;
            m.exact = true;
            m.significantDigits = 11;
            m.notes = "Valeurs transmises par OpenSees au format %.10e (11 chiffres significatifs). "
                      "Inclut ressorts (zeroLength) et câbles. Pour une analyse linéaire élastique en "
                      "transformation Linear, c'est la matrice résolue par l'analyse principale.";
        }
    }

    adv.available = true;
    return true;
}

void OpenSeesResultsReader::parseModalOutput(const std::string& solverStdOut,
                                            ResultsModel& outResults)
{
    std::regex rx("MODE\\s+(\\d+)\\s+LAMBDA\\s+([0-9.eE+-]+)");
    auto words_begin = std::sregex_iterator(solverStdOut.begin(), solverStdOut.end(), rx);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
    {
        std::smatch match = *i;
        int modeNum = std::stoi(match[1].str());
        double lambda = std::stod(match[2].str());

        ModalMode mode;
        mode.modeNumber = modeNum;
        mode.eigenvalue = lambda;
        mode.omega = (lambda > 0.0) ? std::sqrt(lambda) : 0.0;
        mode.frequency = mode.omega / (2.0 * 3.141592653589793);
        mode.period = (mode.frequency > 1e-6) ? (1.0 / mode.frequency) : 0.0;

        outResults.addModalMode(mode);
    }
}

void OpenSeesResultsReader::computeGlobalEquilibrium(const CalculationSnapshot& snapshot,
                                                    const AnalysisParameters& params,
                                                    ResultsModel& outResults)
{
    GlobalEquilibrium eq;
    // Mêmes facteurs d'échelle que buildLoads : charges TSA en kN → unités du script.
    const double forceScale = params.useKiloNewtons ? 1.0 : 1000.0;

    auto accumulateLoads = [&](int targetCaseId, double factor, bool includeSW) {
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (targetCaseId > 0 && nl.loadCaseId() != targetCaseId) continue;
            eq.appliedFx += nl.fx() * factor * forceScale;
            eq.appliedFy += nl.fy() * factor * forceScale;
            eq.appliedFz += nl.fz() * factor * forceScale;
        }

        for (const auto& ml : snapshot.memberLoads())
        {
            if (targetCaseId > 0 && ml.loadCaseId() != targetCaseId) continue;

            const auto* el = snapshot.findElementForLoad(ml);
            if (!el) continue;

            LocalMemberLoadComponents comp = LoadResolver::resolveMemberLoadToLocal(ml, snapshot);
            const auto* n1 = snapshot.getNode(el->startNodeId);
            const auto* n2 = snapshot.getNode(el->endNodeId);
            if (!n1 || !n2) continue;

            gp_Pnt p1(n1->x, n1->y, n1->z);
            gp_Pnt p2(n2->x, n2->y, n2->z);
            gp_Vec gVec = LoadResolver::localVectorToGlobal(comp.wx, comp.wy, comp.wz, p1, p2, el->rotation);

            double mult = (ml.type() == TSA::Model::LoadType::MemberPoint) ? 1.0 : el->length;
            eq.appliedFx += gVec.X() * mult * factor * forceScale;
            eq.appliedFy += gVec.Y() * mult * factor * forceScale;
            eq.appliedFz += gVec.Z() * mult * factor * forceScale;
        }

        if (includeSW)
        {
            const double g = 9.81;
            for (const auto& [_, el] : snapshot.elements())
            {
                const double W = el.section.area() * el.material.density * g * el.length
                               * (params.useKiloNewtons ? 1e-3 : 1.0);
                eq.appliedFz -= W * factor;
            }
        }
    };

    if (params.targetCombinationId > 0)
    {
        auto it = snapshot.combinations().find(params.targetCombinationId);
        if (it != snapshot.combinations().end())
        {
            for (const auto& [caseId, factor] : it->second.caseFactors())
            {
                bool includeSW = false;
                auto lcIt = snapshot.loadCases().find(caseId);
                if (lcIt != snapshot.loadCases().end())
                {
                    includeSW = lcIt->second.isSelfWeightIncluded();
                }
                accumulateLoads(caseId, factor, includeSW);
            }
        }
    }
    else
    {
        int filterCaseId = (params.targetLoadCaseId > 0) ? params.targetLoadCaseId : 0;
        accumulateLoads(filterCaseId, 1.0, params.includeSelfWeight);
    }

    // Somme des réactions (appuis fixes + ressorts reportés sur leur nœud TSA)
    for (const auto& [_, r] : outResults.allReactions())
    {
        eq.reactionFx += r.rx;
        eq.reactionFy += r.ry;
        eq.reactionFz += r.rz;
    }

    outResults.setEquilibrium(eq);

    auto meta = outResults.executionMetadata();
    meta.maxResidualForce = eq.maxError();
    meta.isEquilibriumVerified = eq.isBalanced(meta.globalEquilibriumTolerance);
    const double applied = std::sqrt(eq.appliedFx * eq.appliedFx + eq.appliedFy * eq.appliedFy + eq.appliedFz * eq.appliedFz);
    const double err = std::sqrt(eq.errorFx() * eq.errorFx() + eq.errorFy() * eq.errorFy() + eq.errorFz() * eq.errorFz());
    meta.relativeEquilibriumResidual = applied > 0.0 ? err / applied : err;
    meta.totalNodes = static_cast<int>(snapshot.nodeCount());
    meta.totalElements = static_cast<int>(snapshot.elementCount());
    meta.systemSolver = toTclString(params.systemSolver);
    meta.constraintHandler = toTclString(params.constraintHandler);
    meta.geomTransf = toTclString(params.geomTransf);
    meta.extractionLevel = params.extractionLevel;
    if (meta.executionTimestamp.empty())
    {
        meta.executionTimestamp = outResults.timestamp();
    }
    outResults.setExecutionMetadata(meta);
}

} // namespace TSA::Analysis
