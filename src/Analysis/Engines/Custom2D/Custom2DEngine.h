#pragma once

// Moteur « Custom2D » : emplacement d'intégration du solveur 2D personnalisé.
// Tant qu'aucun Custom2D::ISolver n'est connecté, le moteur est listé mais déclaré indisponible :
// la validation (extraction de la portée plane, conversion) fonctionne, le calcul est refusé
// explicitement et AUCUN résultat n'est produit.

#include "Custom2DSolver.h"
#include "../../Engine/AnalysisEngine.h"

#include <memory>

namespace TSA::Analysis
{

class Custom2DEngine final : public AnalysisEngine
{
public:
    static constexpr const char* kId = "custom2d";

    Custom2DEngine() = default;
    explicit Custom2DEngine(std::unique_ptr<Custom2D::ISolver> solver) : m_solver(std::move(solver)) {}

    /// Point d'intégration du solveur réel.
    void connectSolver(std::unique_ptr<Custom2D::ISolver> solver) { m_solver = std::move(solver); }
    bool isSolverConnected() const { return m_solver != nullptr; }

    EngineInfo info() const override;
    AnalysisCapabilities capabilities() const override;
    EngineAvailability availability() const override;
    ValidationResult validate(const AnalysisContext& context, const AnalysisModel& model) const override;
    AnalysisRunResult run(const AnalysisContext& context, const AnalysisModel& model,
                          const AnalysisRunCallbacks& callbacks) override;
    void cancel() override;

private:
    std::unique_ptr<Custom2D::ISolver> m_solver;
};

} // namespace TSA::Analysis
