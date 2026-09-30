#include "OpenSeesResultsReader.h"
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

std::vector<double> parseDoubles(const std::string& line)
{
    std::vector<double> vals;
    std::istringstream iss(line);
    double v = 0.0;
    while (iss >> v)
    {
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

    bool hasDisp = readDisplacements(dispPath, snapshot, outResults);
    bool hasReact = readReactions(reactPath, snapshot, outResults);
    bool hasForces = readElementForces(forcePath, snapshot, outResults);

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
                                             ResultsModel& outResults)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    // Dernier pas de calcul
    const auto& lastLine = lines.back();
    auto vals = parseDoubles(lastLine);

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
            auto stepVals = parseDoubles(lines[stepIdx]);
            TimeHistoryStep thStep;
            thStep.time = static_cast<double>(stepIdx);

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
                    sIdx += 6;
                }
            }
            outResults.addTimeHistoryStep(thStep);
        }
    }

    return true;
}

bool OpenSeesResultsReader::readReactions(const std::string& filePath,
                                         const CalculationSnapshot& snapshot,
                                         ResultsModel& outResults)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    auto vals = parseDoubles(lines.back());
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
                outResults.setNodeReaction(nodeId, react);
                idx += 6;
            }
        }
    }
    return true;
}

bool OpenSeesResultsReader::readElementForces(const std::string& filePath,
                                             const CalculationSnapshot& snapshot,
                                             ResultsModel& outResults)
{
    auto lines = readAllLines(filePath);
    if (lines.empty()) return false;

    auto vals = parseDoubles(lines.back());
    size_t idx = 0;

    for (const auto& [elemId, el] : snapshot.elements())
    {
        ElementResults res;
        res.elementId = elemId;
        res.length = el.length;

        if (el.type == SnapshotElement::ElementType::Truss)
        {
            if (idx < vals.size())
            {
                double axial = vals[idx++];
                res.startForces.position = 0.0;
                res.startForces.N = axial;
                res.endForces.position = el.length;
                res.endForces.N = axial;
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
                res.startForces.N = vals[idx];
                res.startForces.Vy = vals[idx + 1];
                res.startForces.Vz = vals[idx + 2];
                res.startForces.Mx = vals[idx + 3];
                res.startForces.My = vals[idx + 4];
                res.startForces.Mz = vals[idx + 5];

                res.endForces.position = el.length;
                res.endForces.N = vals[idx + 6];
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

        outResults.setElementResults(elemId, res);
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

    // Somme des charges nodales
    for (const auto& nl : snapshot.nodalLoads())
    {
        if (params.targetLoadCaseId > 0 && nl.loadCaseId() != params.targetLoadCaseId) continue;
        eq.appliedFx += nl.fx();
        eq.appliedFy += nl.fy();
        eq.appliedFz += nl.fz();
    }

    // Poids propre
    if (params.includeSelfWeight)
    {
        double g = 9.81;
        for (const auto& [_, el] : snapshot.elements())
        {
            double A = el.section.area();
            double rho = el.material.density;
            double W = A * rho * g * el.length * (params.useKiloNewtons ? 1e-3 : 1.0);
            eq.appliedFz -= W;
        }
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
