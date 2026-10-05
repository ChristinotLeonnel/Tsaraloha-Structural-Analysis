// Point d'enregistrement unique des moteurs d'analyse livrés avec TSA.
// Ajouter un moteur : écrire son adaptateur (AnalysisEngine), puis une ligne ici.

#include "../Engine/AnalysisEngineRegistry.h"
#include "Custom2D/Custom2DEngine.h"
#include "Custom2D/MetDeDeplacementSolver.h"
#include "OpenSees/OpenSeesEngine.h"

namespace TSA::Analysis
{

void registerBuiltInEngines(AnalysisEngineRegistry& registry)
{
    registry.registerEngine(std::make_unique<OpenSeesEngine>());
    // Solveur 2D personnalisé : méthode des déplacements (thirdparty/MetDeDeplacement).
    registry.registerEngine(std::make_unique<Custom2DEngine>(std::make_unique<Custom2D::MetDeDeplacementSolver>()));
}

} // namespace TSA::Analysis
