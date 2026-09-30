#pragma once

#include "CalculationSnapshot.h"
#include "ResultsModel.h"
#include <string>

namespace TSA::Analysis
{

struct AnalysisParameters
{
    AnalysisType type = AnalysisType::LinearStatic;
    int targetLoadCaseId = 0;       ///< 0 = tous les cas actifs
    int targetCombinationId = 0;    ///< 0 = cas individuels
    bool useKiloNewtons = true;     ///< true: kN, m, kPa, kNm | false: N, m, Pa, Nm
    bool includeSelfWeight = true;

    // Non-linéaire / Itératif
    int maxIterations = 50;
    double tolerance = 1e-6;
    int numSteps = 10;
    std::string algorithm = "Newton"; // Newton, ModifiedNewton, Linear, NewtonLineSearch

    // Modal
    int numEigenmodes = 3;

    // Fichiers recorders
    std::string workingDir = ".";
    std::string dispOutputFile = "node_disp.out";
    std::string reactOutputFile = "node_react.out";
    std::string forceOutputFile = "ele_forces.out";
};

/**
 * @brief Constructeur de scripts OpenSees modulaire et multi-analyses.
 * Traduit le snapshot calculatoire figé en code Tcl vérifiable avec recorders et diagnostics.
 */
class OpenSeesAnalysisBuilder
{
public:
    static std::string buildScript(const CalculationSnapshot& snapshot,
                                   const AnalysisParameters& params);

    static std::string buildNodes(const CalculationSnapshot& snapshot);
    static std::string buildBoundaryConditions(const CalculationSnapshot& snapshot);
    static std::string buildElements(const CalculationSnapshot& snapshot, bool useKiloNewtons);
    static std::string buildRecorders(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildLoads(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildAnalysisCommands(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
};

} // namespace TSA::Analysis
