#pragma once

#include "AnalysisEngine.h"

#include <memory>
#include <vector>

namespace TSA::Analysis
{

/// Registre central des moteurs. Possède les moteurs ; l'ordre d'enregistrement est l'ordre
/// d'affichage. L'UI ne crée jamais de moteur : elle liste et interroge ce registre.
class AnalysisEngineRegistry
{
public:
    /// false si l'identifiant est vide ou déjà enregistré (le moteur est alors détruit).
    bool registerEngine(std::unique_ptr<AnalysisEngine> engine);

    std::vector<EngineInfo> engines() const;
    std::vector<EngineId> ids() const;
    AnalysisEngine* engine(const EngineId& id);
    const AnalysisEngine* engine(const EngineId& id) const;
    bool hasEngine(const EngineId& id) const { return engine(id) != nullptr; }
    std::size_t size() const { return m_engines.size(); }

private:
    struct Entry
    {
        EngineId id;   ///< figé à l'enregistrement : la recherche n'appelle pas info()
        std::unique_ptr<AnalysisEngine> engine;
    };
    std::vector<Entry> m_engines;
};

/// Enregistre les moteurs livrés avec TSA. SEUL endroit à modifier pour ajouter un moteur intégré.
void registerBuiltInEngines(AnalysisEngineRegistry& registry);

} // namespace TSA::Analysis
