#pragma once

// Contrôleur d'analyse partagé par les applications de l'écosystème (TSA, TSALab) — ADR-024, phase 7.
//
// Regroupe ce qui était câblé dans MainWindow : registre des moteurs, orchestration (AnalysisManager),
// réglages d'analyse du projet (AnalysisContext, enregistrés dans le modèle), calcul dans un thread de
// travail, publication des résultats et invalidation quand le modèle change (ResultsValidityGuard).
//
// Couche modèle : Qt Core seulement (QObject, signaux, QThread), aucun widget. Les questions posées à
// l'utilisateur (installation d'un moteur, nettoyage, avertissements) restent dans l'application ;
// le contrôleur ne fait que préparer, calculer et publier. Les signaux de journal et de progression sont
// émis depuis le thread de travail : connectés à un objet du thread de l'interface, ils y sont mis en file.

#include "Engine/AnalysisManager.h"

#include <QObject>
#include <QString>

#include <memory>

class QThread;

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

class ResultsModel;
class ResultsValidityGuard;

class AnalysisController : public QObject
{
    Q_OBJECT

public:
    /// Les moteurs intégrés sont enregistrés (registerBuiltInEngines).
    AnalysisController(TSA::Model::Model* model, const TSA::Grid::GridManager* grids, QObject* parent = nullptr);
    ~AnalysisController() override;

    AnalysisEngineRegistry& registry() { return *m_registry; }
    const AnalysisEngineRegistry& registry() const { return *m_registry; }
    AnalysisManager& manager() { return *m_manager; }
    const AnalysisManager& manager() const { return *m_manager; }

    // --- Réglages d'analyse du projet ---------------------------------------------------------
    const AnalysisContext& context() const { return m_context; }
    void setContext(const AnalysisContext& context) { m_context = context; }
    /// Relit les réglages enregistrés avec le projet (moteur absent → premier moteur disponible).
    /// Faux si des réglages existaient mais étaient illisibles (réglages par défaut appliqués).
    bool restoreContextFromModel();
    /// Enregistre le contexte courant dans le modèle ; vrai si le modèle a changé.
    bool storeContextInModel();

    // --- Calcul -------------------------------------------------------------------------------
    /// Résout la portée, extrait et valide le modèle d'analyse (aucun calcul).
    PreparedAnalysis prepare(const AnalysisContext& context) const;
    PreparedAnalysis prepare() const { return prepare(m_context); }

    bool isRunning() const { return m_worker != nullptr; }
    /// Lance le calcul dans un thread de travail et rend la main immédiatement. Faux si un calcul est
    /// déjà en cours ou si le modèle préparé est invalide. Fin : signal finished() (résultats publiés).
    bool start(const AnalysisContext& context, const PreparedAnalysis& prepared);
    /// Prépare puis lance avec le contexte courant ; *error reçoit la raison d'un refus.
    bool start(QString* error = nullptr);
    /// Demande l'annulation du calcul en cours (le moteur s'arrête au prochain point de contrôle).
    void cancel();
    /// Calcul synchrone dans le thread appelant (tests, scripts, Blueprint) ; publie si succès.
    /// Rend succès et message (et, en cas d'échec, le journal du moteur) ; résultats : results().
    AnalysisRunResult runBlocking(const AnalysisContext& context, const PreparedAnalysis& prepared);
    /// Dernier résultat d'exécution : succès, message ; ses résultats sont transférés à results() en cas
    /// de succès (journal du moteur conservé en cas d'échec).
    const AnalysisRunResult& lastRun() const { return *m_lastRun; }

    // --- Résultats ----------------------------------------------------------------------------
    std::shared_ptr<ResultsModel> results() const { return m_results; }
    /// Vrai si des résultats existent et correspondent encore au modèle.
    bool resultsUpToDate() const;
    /// Rend des résultats courants (suivis par le garde de validité) et émet resultsChanged().
    void publishResults(const std::shared_ptr<ResultsModel>& results);
    /// Oublie les résultats (nouveau projet, projet fermé) et émet resultsChanged().
    void clearResults();

signals:
    void started(const QString& engineName);
    void logMessage(const QString& line);
    void progressChanged(int percent, const QString& status);
    /// Fin du calcul, après publication des résultats en cas de succès.
    void finished(bool success, const QString& message);
    void resultsChanged();
    /// Le modèle a changé depuis le calcul : les résultats ont été invalidés (émis une fois, différé).
    void resultsBecameStale();

private:
    AnalysisRunCallbacks makeCallbacks();
    void onWorkerFinished();

    TSA::Model::Model* m_model = nullptr;
    const TSA::Grid::GridManager* m_grids = nullptr;
    std::unique_ptr<AnalysisEngineRegistry> m_registry;
    std::unique_ptr<AnalysisManager> m_manager;
    std::unique_ptr<ResultsValidityGuard> m_guard;
    AnalysisContext m_context;
    std::shared_ptr<ResultsModel> m_results;

    QThread* m_worker = nullptr;
    EngineId m_runningEngine;
    std::shared_ptr<AnalysisRunResult> m_lastRun;
};

} // namespace TSA::Analysis
