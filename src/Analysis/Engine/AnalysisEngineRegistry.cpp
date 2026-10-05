#include "AnalysisEngineRegistry.h"

namespace TSA::Analysis
{

bool AnalysisEngineRegistry::registerEngine(std::unique_ptr<AnalysisEngine> engine)
{
    if (!engine) return false;
    EngineId id = engine->info().id;
    if (id.empty() || hasEngine(id)) return false;
    m_engines.push_back({ std::move(id), std::move(engine) });
    return true;
}

std::vector<EngineInfo> AnalysisEngineRegistry::engines() const
{
    std::vector<EngineInfo> out;
    out.reserve(m_engines.size());
    for (const auto& e : m_engines) out.push_back(e.engine->info());
    return out;
}

std::vector<EngineId> AnalysisEngineRegistry::ids() const
{
    std::vector<EngineId> out;
    out.reserve(m_engines.size());
    for (const auto& e : m_engines) out.push_back(e.id);
    return out;
}

AnalysisEngine* AnalysisEngineRegistry::engine(const EngineId& id)
{
    for (auto& e : m_engines)
        if (e.id == id) return e.engine.get();
    return nullptr;
}

const AnalysisEngine* AnalysisEngineRegistry::engine(const EngineId& id) const
{
    for (const auto& e : m_engines)
        if (e.id == id) return e.engine.get();
    return nullptr;
}

} // namespace TSA::Analysis
