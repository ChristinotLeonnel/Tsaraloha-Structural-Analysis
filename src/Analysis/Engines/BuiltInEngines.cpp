// Point d'enregistrement unique des moteurs d'analyse livrés avec TSA.
// Ajouter un moteur : écrire son adaptateur (AnalysisEngine), puis une ligne ici.

#include "../Engine/AnalysisEngineRegistry.h"
#include "Custom2D/Custom2DEngine.h"
#include "tsalab/planar/PlanarSolvers.h"
#include "OpenSees/OpenSeesEngine.h"

namespace TSA::Analysis
{

void registerBuiltInEngines(AnalysisEngineRegistry& registry)
{
    registry.registerEngine(std::make_unique<OpenSeesEngine>());
    // Solveur d'ossatures planes du cœur scientifique TSALab (tsalab::planar) : le premier des
    // solveurs intégrés (méthode des déplacements, MetDeDeplacement).
    auto solvers = tsalab::planar::createBuiltInSolvers();
    if (!solvers.empty())
        registry.registerEngine(std::make_unique<Custom2DEngine>(std::move(solvers.front())));
    else
        registry.registerEngine(std::make_unique<Custom2DEngine>());
}

} // namespace TSA::Analysis
