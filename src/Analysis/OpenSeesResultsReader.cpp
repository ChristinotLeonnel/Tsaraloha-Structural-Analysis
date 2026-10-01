#include "OpenSeesResultsReader.h"
#include "LoadResolver.h"
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
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

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
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lower.find("nan") != std::string::npos || lower.find("inf") != std::string::npos || lower.find("ind") != std::string::npos)
            {
                if (hasNonFinite) *hasNonFinite = true;
                v = 0.0;
            }
        }
        else
        {
            if (std::isnan(v) || std::isinf(v))
            {
                if (hasNonFinite) *hasNonFinite = true;
                v = 0.0;
            }
        }
        vals.push_back(v);
    }
    return vals;
}
} // namespace

bool OpenSeesResultsReader::readResults(const std::string& workingDirectory,
                                       const CalculationSnapshot& snapshot,
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
    outResults.updateTimestamp();

    std::string dispPath = workingDirectory + "/" + params.dispOutputFile;
    std::string reactPath = workingDirectory + "/" + params.reactOutputFile;
    std::string forcePath = workingDirectory + "/" + params.forceOutputFile;

    if (params.type == AnalysisType::Modal)
    {
        parseModalOutput(solverStdOut, outResults);
        outResults.setValid(!outResults.modalModes().empty());
        outResults.computeSummary();
        return outResults.isValid();
    }

    bool nonFiniteFound = false;
    bool hasDisp = readDisplacements(dispPath, snapshot, outResults, &nonFiniteFound);
    bool hasReact = readReactions(reactPath, snapshot, outResults, &nonFiniteFound);
    bool hasForces = readElementForces(forcePath, snapshot, outResults, &nonFiniteFound);

    if (nonFiniteFound)
    {
        if (errorMessage)
        {
            *errorMessage = "Instabilité numérique ou matrice de rigidité singulière : des valeurs infinies ou indéterminées (NaN/Inf) ont été détectées dans la réponse structurale OpenSees.";
        }
        outResults.appendLog("\n[ERREUR NORMATIVE] Instabilité numérique détectée : les déplacements ou réactions contiennent des valeurs non finies (NaN/Inf).\n");
        outResults.setValid(false);
        return false;
    }

    computeGlobalEquilibrium(snapshot, params, outResults);
    outResults.computeSummary();

    if (!hasDisp && !hasForces && !hasReact)
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

bool OpenSeesResultsReader::readDisplacements(const std::string& filePath,
                                             const CalculationSnapshot& snapshot,
                                             ResultsModel& outResults,
                                             bool* hasNonFinite)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    // Dernier pas de calcul
    const auto& lastLine = lines.back();
    auto vals = parseDoubles(lastLine, hasNonFinite);

    size_t idx = 0;
    for (const auto& [nodeId, _] : snapshot.nodes())
    {
        if (idx + 5 < vals.size())
        {
            NodeDisplacement disp;
            disp.ux = vals[idx];
            disp.uy = vals[idx + 1];
            disp.uz = vals[idx + 2];
            disp.rx = vals[idx + 3];
            disp.ry = vals[idx + 4];
            disp.rz = vals[idx + 5];
            outResults.setNodeDisplacement(nodeId, disp);
            idx += 6;
        }
    }

    // Sauvegarde des étapes si plusieurs pas (Non linéaire / Dynamique)
    if (lines.size() > 1)
    {
        for (size_t stepIdx = 0; stepIdx < lines.size(); ++stepIdx)
        {
            auto stepVals = parseDoubles(lines[stepIdx], hasNonFinite);
            TimeHistoryStep thStep;
            thStep.time = static_cast<double>(stepIdx);

            StepResults stepRes;
            stepRes.stepNumber = static_cast<int>(stepIdx + 1);
            stepRes.factorOrTime = static_cast<double>(stepIdx + 1) / static_cast<double>(lines.size());

            size_t sIdx = 0;
            for (const auto& [nodeId, _] : snapshot.nodes())
            {
                if (sIdx + 5 < stepVals.size())
                {
                    NodeDisplacement d;
                    d.ux = stepVals[sIdx];
                    d.uy = stepVals[sIdx + 1];
                    d.uz = stepVals[sIdx + 2];
                    d.rx = stepVals[sIdx + 3];
                    d.ry = stepVals[sIdx + 4];
                    d.rz = stepVals[sIdx + 5];
                    thStep.displacements[nodeId] = d;
                    stepRes.displacements[nodeId] = d;
                    sIdx += 6;
                }
            }
            outResults.addTimeHistoryStep(thStep);
            outResults.addStepResults(stepRes);
        }
    }

    return true;
}

bool OpenSeesResultsReader::readReactions(const std::string& filePath,
                                         const CalculationSnapshot& snapshot,
                                         ResultsModel& outResults,
                                         bool* hasNonFinite)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    auto parseReactionsLine = [&](const std::vector<double>& vals) {
        std::map<int, NodeReaction> reactions;
        size_t idx = 0;
        for (const auto& [nodeId, n] : snapshot.nodes())
        {
            if (n.fixTx || n.fixTy || n.fixTz || n.fixRx || n.fixRy || n.fixRz)
            {
                if (idx + 5 < vals.size())
                {
                    NodeReaction react;
                    react.rx = vals[idx];
                    react.ry = vals[idx + 1];
                    react.rz = vals[idx + 2];
                    react.mx = vals[idx + 3];
                    react.my = vals[idx + 4];
                    react.mz = vals[idx + 5];
                    reactions[nodeId] = react;
                    idx += 6;
                }
            }
        }
        return reactions;
    };

    auto lastReactions = parseReactionsLine(parseDoubles(lines.back(), hasNonFinite));
    for (const auto& [nodeId, react] : lastReactions)
    {
        outResults.setNodeReaction(nodeId, react);
    }

    auto& steps = outResults.allStepResults();
    if (!steps.empty())
    {
        for (size_t stepIdx = 0; stepIdx < steps.size() && stepIdx < lines.size(); ++stepIdx)
        {
            steps[stepIdx].reactions = parseReactionsLine(parseDoubles(lines[stepIdx], hasNonFinite));
        }
    }

    return true;
}

bool OpenSeesResultsReader::readElementForces(const std::string& filePath,
                                             const CalculationSnapshot& snapshot,
                                             ResultsModel& outResults,
                                             bool* hasNonFinite)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    auto parseElementsLine = [&](const std::vector<double>& vals) {
        std::map<int, ElementResults> elementRes;
        size_t idx = 0;

        for (const auto& [elemId, el] : snapshot.elements())
        {
            ElementResults res;
            res.elementId = elemId;
            res.length = el.length;

            // Récupération des déplacements nodaux aux extrémités
            const auto* n1 = snapshot.getNode(el.startNodeId);
            const auto* n2 = snapshot.getNode(el.endNodeId);
            if (n1 && n2)
            {
                gp_Pnt p1(n1->x, n1->y, n1->z);
                gp_Pnt p2(n2->x, n2->y, n2->z);
                const auto* d1 = outResults.getNodeDisplacement(el.startNodeId);
                const auto* d2 = outResults.getNodeDisplacement(el.endNodeId);
                if (d1)
                {
                    auto locD1 = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d1->ux, d1->uy, d1->uz), p1, p2, el.rotation);
                    res.startForces.ux = locD1.wx;
                    res.startForces.uy = locD1.wy;
                    res.startForces.uz = locD1.wz;
                    auto locR1 = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d1->rx, d1->ry, d1->rz), p1, p2, el.rotation);
                    res.startForces.rx = locR1.wx;
                    res.startForces.ry = locR1.wy;
                    res.startForces.rz = locR1.wz;
                }
                if (d2)
                {
                    auto locD2 = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d2->ux, d2->uy, d2->uz), p1, p2, el.rotation);
                    res.endForces.ux = locD2.wx;
                    res.endForces.uy = locD2.wy;
                    res.endForces.uz = locD2.wz;
                    auto locR2 = LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(d2->rx, d2->ry, d2->rz), p1, p2, el.rotation);
                    res.endForces.rx = locR2.wx;
                    res.endForces.ry = locR2.wy;
                    res.endForces.rz = locR2.wz;
                }
            }

            if (el.type == SnapshotElement::ElementType::Truss || el.type == SnapshotElement::ElementType::Cable)
            {
                if (idx < vals.size())
                {
                    double axial = vals[idx++];
                    res.startForces.position = 0.0;
                    res.startForces.N = axial;
                    res.endForces.position = el.length;
                    res.endForces.N = axial;

                    const int numStations = 5;
                    for (int s = 1; s < numStations; ++s)
                    {
                        double t = static_cast<double>(s) / numStations;
                        StationForces sf;
                        sf.position = t * el.length;
                        sf.N = axial;
                        sf.ux = (1.0 - t) * res.startForces.ux + t * res.endForces.ux;
                        sf.uy = (1.0 - t) * res.startForces.uy + t * res.endForces.uy;
                        sf.uz = (1.0 - t) * res.startForces.uz + t * res.endForces.uz;
                        res.intermediateStations.push_back(sf);
                    }
                }
            }
            else
            {
                // elasticBeamColumn 3D localForce fournit 12 composantes :
                // End 1 : N1, Vy1, Vz1, T1, My1, Mz1
                // End 2 : N2, Vy2, Vz2, T2, My2, Mz2
                if (idx + 11 < vals.size())
                {
                    res.startForces.position = 0.0;
                    res.startForces.N = -vals[idx]; // Convention RDM : traction > 0
                    res.startForces.Vy = vals[idx + 1];
                    res.startForces.Vz = vals[idx + 2];
                    res.startForces.Mx = vals[idx + 3];
                    res.startForces.My = vals[idx + 4];
                    res.startForces.Mz = vals[idx + 5];

                    res.endForces.position = el.length;
                    res.endForces.N = vals[idx + 6]; // Convention RDM : traction > 0
                    res.endForces.Vy = vals[idx + 7];
                    res.endForces.Vz = vals[idx + 8];
                    res.endForces.Mx = vals[idx + 9];
                    res.endForces.My = vals[idx + 10];
                    res.endForces.Mz = vals[idx + 11];

                    // Interpolation et superposition des charges sur barre (M0, V0)
                    const int numStations = 20;
                    double L = el.length;
                    for (int s = 1; s < numStations; ++s)
                    {
                        double t = static_cast<double>(s) / numStations;
                        double x = t * L;
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

                        // Superposition isostatique des charges sur barres
                        for (const auto& ml : snapshot.memberLoads())
                        {
                            if (ml.elementId() != elemId) continue;
                            if (ml.type() == TSA::Model::LoadType::MemberPoint)
                            {
                                double p = ml.q1();
                                double a = ml.isRelativePosition() ? (ml.x1() * L) : ml.x1();
                                if (a < 0.0) a = 0.0;
                                if (a > L) a = L;

                                double m0 = 0.0;
                                if (x <= a)
                                {
                                    m0 = p * (1.0 - a / L) * x;
                                }
                                else
                                {
                                    m0 = p * a * (1.0 - x / L);
                                }
                                sf.My += m0;
                            }
                            else
                            {
                                // Charge uniforme
                                double q = ml.q1();
                                double m0 = (q * x * (L - x)) * 0.5;
                                sf.My += m0;
                                sf.Vz += q * (0.5 * L - x);
                            }
                        }

                        res.intermediateStations.push_back(sf);
                    }

                    idx += 12;
                }
            }

            elementRes[elemId] = res;
        }
        return elementRes;
    };

    auto lastElements = parseElementsLine(parseDoubles(lines.back(), hasNonFinite));
    for (const auto& [elemId, res] : lastElements)
    {
        outResults.setElementResults(elemId, res);
    }

    auto& steps = outResults.allStepResults();
    if (!steps.empty())
    {
        for (size_t stepIdx = 0; stepIdx < steps.size() && stepIdx < lines.size(); ++stepIdx)
        {
            steps[stepIdx].elementResults = parseElementsLine(parseDoubles(lines[stepIdx], hasNonFinite));
        }
    }

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

    auto accumulateLoads = [&](int targetCaseId, double factor, bool includeSW) {
        // 1. Charges nodales
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (targetCaseId > 0 && nl.loadCaseId() != targetCaseId) continue;
            eq.appliedFx += nl.fx() * factor;
            eq.appliedFy += nl.fy() * factor;
            eq.appliedFz += nl.fz() * factor;
        }

        // 2. Charges sur barres
        for (const auto& ml : snapshot.memberLoads())
        {
            if (targetCaseId > 0 && ml.loadCaseId() != targetCaseId) continue;

            const auto* el = snapshot.getElement(ml.elementId());
            if (!el) continue;

            LocalMemberLoadComponents comp = LoadResolver::resolveMemberLoadToLocal(ml, snapshot);
            const auto* n1 = snapshot.getNode(el->startNodeId);
            const auto* n2 = snapshot.getNode(el->endNodeId);
            if (!n1 || !n2) continue;

            gp_Pnt p1(n1->x, n1->y, n1->z);
            gp_Pnt p2(n2->x, n2->y, n2->z);
            gp_Vec gVec = LoadResolver::localVectorToGlobal(comp.wx, comp.wy, comp.wz, p1, p2, el->rotation);

            double mult = (ml.type() == TSA::Model::LoadType::MemberPoint) ? 1.0 : el->length;
            eq.appliedFx += gVec.X() * mult * factor;
            eq.appliedFy += gVec.Y() * mult * factor;
            eq.appliedFz += gVec.Z() * mult * factor;
        }

        // 3. Poids propre
        if (includeSW)
        {
            double g = 9.81;
            for (const auto& [_, el] : snapshot.elements())
            {
                double A = el.section.area();
                double rho = el.material.density;
                double W = A * rho * g * el.length * (params.useKiloNewtons ? 1e-3 : 1.0);
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

    // Somme des réactions aux appuis
    for (const auto& [_, r] : outResults.allReactions())
    {
        eq.reactionFx += r.rx;
        eq.reactionFy += r.ry;
        eq.reactionFz += r.rz;
    }

    outResults.setEquilibrium(eq);
}

} // namespace TSA::Analysis
