#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace TSA::Analysis
{
struct AnalysisContext;
class ResultsModel;
}

namespace TSA::Standards
{
class ModelValidationReport;
}

namespace TSA::Project
{

/// Un calcul lancé sur le projet (réussi, en échec ou annulé) : traçabilité (BUG-111) et historique du bloc DIAG.
struct CalculationRecord
{
    std::string timestamp;          ///< ISO 8601, UTC
    std::string engineId;           ///< moteur choisi ("opensees", "custom2d"…)
    std::string engine;             ///< moteur qui a produit les résultats et sa version (vide en cas d'échec)
    std::string analysisType;       ///< "linear_static", "modal"
    std::string loadSelection;      ///< cas ou combinaison calculés
    std::string scope;              ///< portée (vide : modèle complet)
    bool success = false;
    std::string message;            ///< message du moteur
    double durationMs = 0.0;
    std::size_t warnings = 0;       ///< avertissements et messages de préparation
    std::string modelFingerprint;   ///< empreinte du modèle calculé (TSAProjectIO::modelFingerprint)

    bool operator==(const CalculationRecord&) const = default;
};

/// Anomalie du rapport de validation relevé à l'enregistrement.
struct ValidationEntry
{
    std::string severity;   ///< "info", "warning", "error"
    std::string category;
    std::string message;
    int entityId = 0;

    bool operator==(const ValidationEntry&) const = default;
};

/// Messages, erreurs et historique des calculs du projet (bloc DIAG du .tsa, docs/PROJECT_STATE_PLAN.md §3.4).
/// N'enregistre rien du journal général de l'application (crashs, session : chemins personnels).
class ProjectDiagnostics
{
public:
    static constexpr std::size_t kMaxRecords = 20;      ///< calculs conservés (les plus récents)
    static constexpr std::size_t kMaxLogLines = 500;    ///< lignes du journal du moteur conservées (les dernières)

    /// Ajoute un calcul ; ses messages (avertissements, préparation) et le journal du moteur deviennent ceux du dernier
    /// calcul. results : résultats du calcul s'il a réussi (moteur, version, avertissements), sinon nul.
    void recordCalculation(const TSA::Analysis::AnalysisContext& context, bool success, const std::string& message,
                           const TSA::Analysis::ResultsModel* results, const std::string& engineLog, double durationMs,
                           const std::string& modelFingerprint, const std::string& timestamp);

    /// Rapport de validation du modèle au moment de l'enregistrement.
    void setValidation(const TSA::Standards::ModelValidationReport& report);

    const std::deque<CalculationRecord>& history() const noexcept { return m_history; }
    const std::vector<std::string>& lastMessages() const noexcept { return m_lastMessages; }
    const std::string& lastEngineLog() const noexcept { return m_lastEngineLog; }
    const std::vector<ValidationEntry>& validation() const noexcept { return m_validation; }
    bool empty() const noexcept { return m_history.empty() && m_validation.empty(); }
    void clear();

    /// JSON UTF-8 « schema » 1 ; fromJson : faux (objet inchangé, *error renseigné) si illisible ou d'une version inconnue.
    std::string toJson() const;
    bool fromJson(const std::string& json, std::string* error = nullptr);

    bool operator==(const ProjectDiagnostics&) const = default;

private:
    std::deque<CalculationRecord> m_history;   ///< du plus ancien au plus récent
    std::vector<std::string> m_lastMessages;
    std::string m_lastEngineLog;
    std::vector<ValidationEntry> m_validation;
};

} // namespace TSA::Project
