// Point d'enregistrement unique des moteurs d'analyse livrés avec TSA.
// Ajouter un moteur : écrire son adaptateur (AnalysisEngine), puis une ligne ici.

#include "../Engine/AnalysisEngineRegistry.h"
#include "Custom2D/Custom2DEngine.h"
#include "tsalab/planar/PlanarSolvers.h"
#include "OpenSees/OpenSeesEngine.h"
#include "../OpenSeesManager.h"

#include <QDir>

namespace TSA::Analysis
{

void registerBuiltInEngines(AnalysisEngineRegistry& registry)
{
    registry.registerEngine(std::make_unique<OpenSeesEngine>());
    // Le solveur OpenSees du cœur scientifique (banc de validation) utilise l'exécutable configuré dans TSA.
    if (const QString exe = OpenSeesManager::instance().executablePath(); !exe.isEmpty())
        tsalab::planar::setOpenSeesExecutable(QDir::fromNativeSeparators(exe).toStdString());
    // Solveur d'ossatures planes du cœur scientifique TSALab (tsalab::planar) : le premier des
    // solveurs intégrés (méthode des déplacements, MetDeDeplacement).
    auto solvers = tsalab::planar::createBuiltInSolvers();
    if (!solvers.empty())
        registry.registerEngine(std::make_unique<Custom2DEngine>(std::move(solvers.front())));
    else
        registry.registerEngine(std::make_unique<Custom2DEngine>());
}

} // namespace TSA::Analysis
