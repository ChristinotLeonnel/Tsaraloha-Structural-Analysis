#pragma once

// Conversion AnalysisModel (portée plane) ↔ données du solveur 2D, dans les deux sens :
//  - buildInput   : nœuds projetés dans le plan, appuis / ressorts projetés, inertie de flexion
//                   dans le plan, charges projetées (composantes hors plan signalées) ;
//  - mapResults   : résultats 2D (indices) → ResultsModel indexé par nœud TSA / ElementKey, en
//                   3D global et dans les axes locaux utilisés par les résultats OpenSees.

#include "Custom2DSolver.h"
#include "../../Engine/AnalysisEngine.h"

namespace TSA::Analysis::Custom2D
{

/// Construit l'entrée du solveur. Les pertes d'information (composantes hors plan, section non
/// alignée, type de charge non transmis) sont ajoutées à diagnostics.
Input buildInput(const AnalysisContext& context, const AnalysisModel& model, ValidationResult* diagnostics);

/// Remappe la sortie du solveur vers les objets TSA (via AnalysisMapping et le plan d'analyse).
/// Les indices inconnus sont ignorés et signalés dans le journal des résultats.
ResultsModel mapResults(const AnalysisContext& context, const AnalysisModel& model, const Output& output);

/// Maillage de l'ossature plane réellement résolue (nœuds et éléments de l'entrée du solveur), placé
/// aux coordonnées 3D des nœuds TSA (nœud sans origine TSA : repositionné depuis le plan).
SolverMesh solverMesh(const AnalysisModel& model, const Input& input);

} // namespace TSA::Analysis::Custom2D
