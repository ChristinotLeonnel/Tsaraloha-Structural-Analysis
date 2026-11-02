#pragma once

// Moteur « fem3d » : moteur EF 3D de TSA (dépôt voisin TSA-3D-FEM-Engine), MOTEUR SECONDAIRE. OpenSees reste le moteur
// 3D par défaut tant que la qualification de Fem3D dans TSA n'est pas complète (docs/ANALYSIS_ENGINES.md).
//
// Modes (réglages du moteur) :
//  - « fem3d » (défaut) : Fem3D seul ; un modèle hors de son périmètre est REFUSÉ, avec la liste des raisons (choisir
//    alors le moteur OpenSees) ;
//  - « compare » : Fem3D ET OpenSees ; résultats affichés = Fem3D, écarts dans une table et les avertissements,
//    aucune fusion des deux jeux de résultats.
// Plus de secours OpenSees (BUG-144) : depuis Fem3D 0.3.0, les seuls modèles hors périmètre (treillis, câbles, coques)
// sont refusés avant par AnalysisManager ; l'option n'avait plus d'effet. La clé « fallback » des anciens projets est
// ignorée.

#include "../../Engine/AnalysisEngine.h"

#include <memory>

namespace TSA::Analysis
{

class OpenSeesEngine;

class Fem3DEngine final : public AnalysisEngine
{
public:
    static constexpr const char* kId = "fem3d";

    enum class Mode { Fem3D, Compare };
    struct Settings
    {
        Mode mode = Mode::Fem3D;
    };
    static Settings settingsFromContext(const AnalysisContext& context);
    static QJsonObject settingsToJson(const Settings& settings);

    /// Tolérance de la comparaison Fem3D / OpenSees, fixée avant les essais (mêmes critères que les benchmarks du
    /// moteur, TSA-3D-FEM-Engine/docs/validation/VALIDATION_OPENSEES.md §3) : |écart| ≤ max(1e-6·|OpenSees|, 1e-9·échelle).
    static constexpr double kCompareRelative = 1e-6;
    static constexpr double kCompareAbsoluteFactor = 1e-9;

    Fem3DEngine();
    ~Fem3DEngine() override;

    EngineInfo info() const override;
    AnalysisCapabilities capabilities() const override;
    EngineAvailability availability() const override;
    QJsonObject defaultSettings() const override { return settingsToJson({}); }
    ValidationResult validate(const AnalysisContext& context, const AnalysisModel& model) const override;
    AnalysisRunResult run(const AnalysisContext& context, const AnalysisModel& model,
                          const AnalysisRunCallbacks& callbacks) override;
    void cancel() override;

private:
    std::unique_ptr<OpenSeesEngine> m_openSees;   ///< comparaison (même exécutable que le moteur OpenSees)
};

/// Comparaison de deux jeux de résultats (déplacements, réactions, efforts aux extrémités des barres) : table ajoutée
/// à `reference` et avertissement en cas d'écart. Exposée pour les tests.
struct EngineComparison
{
    int compared = 0, failed = 0;
    double maxAbsolute = 0.0, maxRelative = 0.0;
};
EngineComparison compareResults(ResultsModel& fem3d, const ResultsModel& openSees);

} // namespace TSA::Analysis
