#include "AnalysisEngineOptions.h"

#include "Custom2DOptionsWidget.h"
#include "Fem3DOptionsWidget.h"
#include "OpenSeesOptionsWidget.h"
#include "../../Analysis/Engines/OpenSees/OpenSeesEngine.h"

namespace TSA::UI
{

void registerBuiltInEngineOptions(AnalysisEngineOptionsRegistry& registry)
{
    registry.registerFactory(TSA::Analysis::OpenSeesEngine::kId,
                             [](QWidget* parent) { return new OpenSeesOptionsWidget(parent); });
    registry.registerFactory("custom2d", [](QWidget* parent) { return new Custom2DOptionsWidget(parent); });
    // Sans effet si le moteur Fem3D n'est pas compilé (TSA_HAS_FEM3D) : aucun moteur « fem3d » n'est alors listé.
    registry.registerFactory("fem3d", [](QWidget* parent) { return new Fem3DOptionsWidget(parent); });
}

} // namespace TSA::UI
