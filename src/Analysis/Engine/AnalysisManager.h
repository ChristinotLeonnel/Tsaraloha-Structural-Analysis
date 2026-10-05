#pragma once

#include "AnalysisEngineRegistry.h"

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Analysis
{

/// Modèle d'analyse prêt à calculer + bilan de validation (générique + moteur).
struct PreparedAnalysis
{
    bool extracted = false;    ///< false : contexte invalide (moteur inconnu, portée introuvable…)
    AnalysisModel model;
    ValidationResult validation;

    bool canRun() const { return extracted && validation.isValid(); }
};

/// Orchestration commune à tous les moteurs :
///   contexte → portée résolue → AnalysisModel → validation (générique + moteur) → run → résultats
/// Ne contient aucune logique propre à un moteur : tout passe par AnalysisEngine et ses capacités.
class AnalysisManager
{
public:
    explicit AnalysisManager(AnalysisEngineRegistry& registry) : m_registry(registry) {}

    AnalysisEngineRegistry& registry() { return m_registry; }
    const AnalysisEngineRegistry& registry() const { return m_registry; }

    /// Résout la portée, extrait le modèle d'analyse et le valide. N'exécute aucun calcul.
    PreparedAnalysis prepare(const TSA::Model::Model& model,
                             const TSA::Grid::GridManager* grids,
                             const AnalysisContext& context) const;

    /// Calcule un modèle préparé. Refuse un modèle invalide ou un moteur indisponible ; renseigne
    /// la traçabilité des résultats (moteur, version, portée, dimension, catégories disponibles).
    AnalysisRunResult run(const AnalysisContext& context,
                          const PreparedAnalysis& prepared,
                          const AnalysisRunCallbacks& callbacks = {});

    void cancel(const EngineId& id);

    /// Contrôles communs déduits des capacités du moteur (public pour les tests).
    static ValidationResult validateGeneric(const AnalysisContext& context,
                                            const AnalysisModel& model,
                                            const EngineInfo& info,
                                            const AnalysisCapabilities& caps);

private:
    AnalysisEngineRegistry& m_registry;
};

} // namespace TSA::Analysis
