#include "AnalysisEngineOptions.h"

#include "Custom2DOptionsWidget.h"
#include "OpenSeesOptionsWidget.h"
#include "../../Analysis/Engines/OpenSees/OpenSeesEngine.h"

namespace TSA::UI
{

void registerBuiltInEngineOptions(AnalysisEngineOptionsRegistry& registry)
{
    registry.registerFactory(TSA::Analysis::OpenSeesEngine::kId,
                             [](QWidget* parent) { return new OpenSeesOptionsWidget(parent); });
    registry.registerFactory("custom2d", [](QWidget* parent) { return new Custom2DOptionsWidget(parent); });
}

} // namespace TSA::UI
