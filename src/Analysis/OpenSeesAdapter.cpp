#include "OpenSeesAdapter.h"
#include "CalculationSnapshot.h"
#include "OpenSeesAnalysisBuilder.h"
#include "OpenSeesModelMap.h"
#include "LoadResolver.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"
#include "../Coordinate/CoordinateTransformationService.h"
#include "../Core/Utf8Path.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace TSA::Analysis
{

std::string OpenSeesAdapter::generateTclScript(const TSA::Model::Model& model,
                                               const OpenSeesOptions& options)
{
    // Export Tcl : même générateur que le calcul (OpenSeesAnalysisBuilder + OpenSeesModelMap).
    // L'ancien générateur parallèle confondait les ids poutre/poteau, écrivait « element truss
    // $A $E » (refusé par OpenSees 3.8.0) et tronquait les valeurs à 6 décimales.
    const CalculationSnapshot snapshot = CalculationSnapshot::capture(model);
    AnalysisParameters params;
    params.type = AnalysisType::LinearStatic;
    params.includeSelfWeight = options.includeSelfWeight;
    params.targetLoadCaseId = options.targetLoadCaseId;
    params.targetCombinationId = options.targetCombinationId;
    params.useKiloNewtons = options.useKiloNewtons;
    const OpenSeesModelMap map = OpenSeesModelMap::build(snapshot, params);

    if (options.includeAnalysisCommands)
        return OpenSeesAnalysisBuilder::buildScript(snapshot, map, params);

    std::string tcl;
    tcl += "# TSA (Tsaraloha Structural Analysis) -> OpenSees : modèle sans commandes d'analyse\n";
    tcl += std::string("# Unités : ") + (options.useKiloNewtons ? "kN, m, kPa, kNm" : "N, m, Pa, Nm") + "\n\n";
    tcl += "wipe\nmodel BasicBuilder -ndm 3 -ndf 6\n\n";
    tcl += OpenSeesAnalysisBuilder::buildNodes(snapshot);
    tcl += OpenSeesAnalysisBuilder::buildBoundaryConditions(snapshot, map);
    tcl += OpenSeesAnalysisBuilder::buildElements(snapshot, map, params);
    tcl += OpenSeesAnalysisBuilder::buildLoads(snapshot, params);
    return tcl;
}

bool OpenSeesAdapter::exportToFile(const std::string& filePath,
                                   const TSA::Model::Model& model,
                                   const OpenSeesOptions& options)
{
    std::string script = generateTclScript(model, options);
    std::ofstream ofs(TSA::Core::utf8Path(filePath));
    if (!ofs.is_open())
    {
        return false;
    }
    ofs << script;
    return true;
}

} // namespace TSA::Analysis
