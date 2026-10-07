#pragma once

// Adaptateur OpenSees derrière AnalysisEngine. Il réutilise tel quel le chemin OpenSees existant
// (OpenSeesModelMap → OpenSeesAnalysisBuilder → OpenSees.exe → OpenSeesResultsReader) via
// OpenSeesSolver::solveSnapshot ; il ne fait que traduire AnalysisContext en AnalysisParameters.

#include "../../Engine/AnalysisEngine.h"
#include "../../OpenSeesAnalysisBuilder.h"   // AnalysisParameters

#include <memory>
#include <mutex>
#include <optional>

namespace TSA::Analysis
{

class OpenSeesSolver;

class OpenSeesEngine final : public AnalysisEngine
{
public:
    static constexpr const char* kId = "opensees";

    OpenSeesEngine();
    ~OpenSeesEngine() override;

    EngineInfo info() const override;
    AnalysisCapabilities capabilities() const override;
    EngineAvailability availability() const override;
    bool provision(std::string* error) override;
    QJsonObject defaultSettings() const override;
    ValidationResult validate(const AnalysisContext& context, const AnalysisModel& model) const override;
    AnalysisRunResult run(const AnalysisContext& context, const AnalysisModel& model,
                          const AnalysisRunCallbacks& callbacks) override;
    void cancel() override;

    /// Réglages OpenSees (bloc JSON) ↔ AnalysisParameters. Seuls les champs propres à OpenSees sont
    /// dans le JSON ; type, chargement et poids propre viennent de la partie commune du contexte.
    static QJsonObject settingsFromParameters(const AnalysisParameters& params);
    static AnalysisParameters parametersFromContext(const AnalysisContext& context);

private:
    std::mutex m_solverMutex;                ///< run() (thread de calcul) et cancel() (thread UI)
    std::unique_ptr<OpenSeesSolver> m_solver;
    mutable std::optional<std::string> m_version;   ///< lue une fois sur l'exécutable
};

} // namespace TSA::Analysis
