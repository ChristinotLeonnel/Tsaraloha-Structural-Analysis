#pragma once

// Gestionnaire d'analyse (panneau) partagé par les applications de l'écosystème — ADR-024, phase 7.
//
// Vue du contrôleur d'analyse de la session (TSA::Analysis::AnalysisController) : moteurs enregistrés
// (disponibilité, capacités), réglages d'analyse du projet (fenêtre Analysis commune), validation du
// modèle d'analyse, calcul en tâche de fond annulable, progression, journal et synthèse des résultats.
// Aucun calcul ni aucune règle propre à un moteur ici : tout passe par le contrôleur et le registre.

#include "../../Model/SelectionQuery.h"

#include <QWidget>

#include <functional>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::UI
{

class AnalysisEngineOptionsRegistry;

class AnalysisManagerPanel : public QWidget
{
    Q_OBJECT

public:
    AnalysisManagerPanel(TSA::Project::ProjectSession* session, const AnalysisEngineOptionsRegistry* options,
                         QWidget* parent = nullptr);

    /// Sélection courante (portée « éléments sélectionnés » de la fenêtre Analysis).
    void setSelectionProvider(std::function<TSA::Model::ElementSet()> provider) { m_selection = std::move(provider); }

    /// Relit les réglages et l'état (après ouverture d'un projet, nouveau projet).
    void refresh();

    // --- Actions (menus de l'application, tests) ---
    void configure();
    /// Prépare et valide sans calculer ; vrai si le calcul peut être lancé.
    bool validate();
    /// Valide puis lance le calcul en tâche de fond (avertissements confirmés par l'utilisateur).
    bool run();
    void cancel();

signals:
    void logMessage(const QString& text, const QString& type);
    /// Les réglages d'analyse ont été modifiés (enregistrés avec le projet).
    void settingsChanged();
    void analysisFinished(bool success);

private:
    void buildUi();
    void fillEngines();
    void updateContextSummary();
    void updateResultsSummary();
    void setRunning(bool running);

    TSA::Project::ProjectSession* m_session = nullptr;
    const AnalysisEngineOptionsRegistry* m_options = nullptr;
    std::function<TSA::Model::ElementSet()> m_selection;

    QTableWidget* m_engines = nullptr;
    QLabel* m_context = nullptr;
    QListWidget* m_validation = nullptr;
    QPushButton* m_btnConfigure = nullptr;
    QPushButton* m_btnValidate = nullptr;
    QPushButton* m_btnRun = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_results = nullptr;
};

} // namespace TSA::UI
