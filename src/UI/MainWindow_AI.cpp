// Intégration de l'IA de co-ingénierie dans la fenêtre principale : orchestrateur, panneau,
// actions (ruban, menu contextuel de l'arbre), indicateur de barre d'état et configuration.

#include "MainWindow.h"

#include "AI/AICoEngineeringDock.h"
#include "AI/AIRuntimeDialog.h"
#include "ModelTree/ModelTreeWidget.h"
#include "WindowManager/WindowManager.h"
#include "../AI/Core/AIOrchestrator.h"
#include "../Analysis/AnalysisController.h"
#include "../Automation/CommandRegistry.h"
#include "../Blueprint/BlueprintScript.h"
#include "../Viewer/OccView.h"
#include "Dock/LogConsoleDock.h"
#include "../Project/ProjectManager.h"
#include "../Viewer/SelectionManager.h"

#include <QAction>
#include <QFileInfo>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

using TSA::AI::AITask;

void MainWindow::createAIComponents()
{
    m_aiOrchestrator = new TSA::AI::AIOrchestrator(this);

    // Source unique des données : le modèle TSA, ses résultats, la sélection courante.
    m_aiOrchestrator->setSourcesProvider([this] {
        TSA::AI::EngineeringSources src;
        src.model = m_model;
        src.results = m_resultsModel.get();
        src.resultsUpToDate = m_analysis ? m_analysis->resultsUpToDate() : true;
        if (m_projectManager)
        {
            src.projectName = m_projectManager->projectName();
            if (src.projectName.isEmpty() && m_projectManager->hasFilePath())
                src.projectName = QFileInfo(m_projectManager->currentFilePath()).completeBaseName();
        }
        if (m_selectionManager)
        {
            const auto sel = m_selectionManager->selectedElements();
            auto add = [&](const QString& type, const std::set<int>& ids) {
                for (int id : ids)
                    if (src.selection.size() < 20) src.selection.push_back({ type, id });
            };
            add("beam", sel.beams);
            add("column", sel.columns);
            add("truss", sel.trussMembers);
            add("cable", sel.cables);
            add("node", sel.nodes);
            add("slab", sel.slabs);
            add("wall", sel.walls);
            add("foundation", sel.foundations);
        }
        return src;
    });

    m_aiDock = new TSA::UI::AICoEngineeringDock(m_aiOrchestrator, this);
    m_aiDock->toggleViewAction()->setIcon(QIcon(":/icons/analysis_run.svg"));
    // Zone droite, non tabifié : l'assistant se consulte à côté du viewport, sans masquer
    // l'arbre ni les propriétés.
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
    m_aiDock->hide();
    connect(m_aiDock, &TSA::UI::AICoEngineeringDock::configureRequested, this, [this] { openAIConfig(0); });

    // Actions (catalogue : ruban, menu contextuel de l'arbre)
    m_actionAIAssistant = m_aiDock->toggleViewAction();
    m_actionAIAssistant->setText(tr("Assistant IA"));
    m_actionAIAssistant->setToolTip(tr("Assistant de co-ingénierie : lit le modèle réel, explique, vérifie et propose (Ctrl+Maj+I)"));
    m_actionAIAssistant->setShortcut(QKeySequence("Ctrl+Shift+I"));

    m_actionAIConfig = new QAction(QIcon(":/icons/settings.svg"), tr("Configuration IA..."), this);
    m_actionAIConfig->setToolTip(tr("Matériel détecté, modèle recommandé, mode LOCAL/CLOUD/AUTO, confidentialité, diagnostic"));
    connect(m_actionAIConfig, &QAction::triggered, this, [this] { openAIConfig(0); });

    m_actionAICheck = new QAction(QIcon(":/icons/measure.svg"), tr("Vérifier la structure (IA)"), this);
    m_actionAICheck->setToolTip(tr("Contrôles automatiques TSA (fonctionnent sans IA) puis explication par l'assistant"));
    connect(m_actionAICheck, &QAction::triggered, this, [this] { m_aiDock->runTask(AITask::CheckStructure); });

    m_actionAIAnalyze = new QAction(QIcon(":/icons/model_tree.svg"), tr("Analyser le modèle (IA)"), this);
    m_actionAIAnalyze->setToolTip(tr("Synthèse du modèle et points à vérifier en priorité"));
    connect(m_actionAIAnalyze, &QAction::triggered, this, [this] { m_aiDock->runTask(AITask::AnalyzeModel); });

    m_actionAIExplain = new QAction(QIcon(":/icons/results_forces.svg"), tr("Expliquer avec l'IA"), this);
    m_actionAIExplain->setToolTip(tr("Explique le comportement de l'élément sélectionné à partir de ses données et résultats"));
    connect(m_actionAIExplain, &QAction::triggered, this, [this] { m_aiDock->runTask(AITask::ExplainSelection); });

    if (m_modelTree)
        m_modelTree->setContextActions({ m_actionFitSelection, m_actionMove3D, m_actionCopy3D, m_actionDelete, nullptr, m_actionAIExplain });

    // « Lancer le calcul » proposé par l'IA et accepté : la commande existante est déclenchée.
    connect(m_aiOrchestrator, &TSA::AI::AIOrchestrator::runAnalysisRequested, this, [this] {
        if (m_actionRunSolve) m_actionRunSolve->trigger();
    });
    // Blueprint proposé par l'IA et accepté par l'ingénieur : exécuté sur le projet (une entrée Annuler par commande).
    connect(m_aiOrchestrator, &TSA::AI::AIOrchestrator::blueprintAccepted, this, [this](const QString& title, const QString& script) {
        TSA::Blueprint::Graph graph;
        std::string error;
        const auto& library = TSA::Blueprint::NodeLibrary::standard();
        if (!TSA::Blueprint::fromCommandScript(script.toStdString(), library, TSA::Automation::CommandRegistry::builtIn(), graph, &error))
        {
            if (m_consoleDock) m_consoleDock->appendLog(tr("Blueprint « %1 » refusé : %2").arg(title, QString::fromStdString(error)), "ERROR");
            return;
        }
        const auto report = TSA::Blueprint::Runner(library, graph, m_session.get()).run();
        if (m_consoleDock)
        {
            for (const auto& line : report.log) m_consoleDock->appendLog(QString::fromStdString(line), "INFO");
            m_consoleDock->appendLog(tr("Blueprint « %1 » : %2").arg(title, QString::fromStdString(report.message)), report.ok ? "SUCCESS" : "ERROR");
        }
        updateUndoRedoActions();
        if (m_occView) m_occView->update();
    });
    connect(m_aiOrchestrator, &TSA::AI::AIOrchestrator::statusChanged, this, &MainWindow::updateAIStatusWidget);
    connect(m_aiOrchestrator, &TSA::AI::AIOrchestrator::hardwareReady, this, [this] {
        if (m_aiOrchestrator->settings().autoStartLocal) m_aiOrchestrator->startLocal();
    });
    // Détection matérielle différée, en tâche de fond (processus séparé, thread dédié).
    QTimer::singleShot(4000, m_aiOrchestrator, [this] { m_aiOrchestrator->probeHardwareAsync(); });

    if (m_windowManager)
        m_windowManager->registerDock("ai_coengineering", tr("IA Co-Engineering"), tr("Outils"), m_aiDock,
                                      Qt::RightDockWidgetArea, false, QKeySequence(), QIcon(":/icons/analysis_run.svg"));
}

void MainWindow::createAIStatusWidget(QStatusBar* bar)
{
    m_statusAI = new QToolButton(this);
    m_statusAI->setAutoRaise(true);
    m_statusAI->setCursor(Qt::PointingHandCursor);
    m_statusAI->setMaximumWidth(190);
    m_statusAI->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_statusAI->setToolTip(tr("État de l'IA — cliquer pour la configuration"));
    connect(m_statusAI, &QToolButton::clicked, this, [this] { openAIConfig(0); });
    bar->addPermanentWidget(m_statusAI);
    updateAIStatusWidget();
}

void MainWindow::updateAIStatusWidget()
{
    if (!m_statusAI || !m_aiOrchestrator) return;
    using TSA::AI::RuntimeState;
    const auto s = m_aiOrchestrator->state();
    const char* dot = "○";
    switch (s)
    {
    case RuntimeState::Ready:
    case RuntimeState::Busy: dot = "●"; break;
    case RuntimeState::Starting: dot = "◐"; break;
    case RuntimeState::Error: dot = "✕"; break;
    default: break;
    }
    m_statusAI->setText(QStringLiteral("IA %1 %2").arg(QString::fromUtf8(dot), m_aiOrchestrator->runtimeLabel()));
    m_statusAI->setToolTip(tr("%1\n%2\nCliquer pour la configuration IA").arg(m_aiOrchestrator->runtimeLabel(), m_aiOrchestrator->runtimeDetails()));
}

void MainWindow::openAIConfig(int page)
{
    if (!m_aiOrchestrator) return;
    // Non modal : un téléchargement de modèle ou un diagnostic ne bloque pas le travail dans TSA.
    if (!m_aiDialog)
    {
        m_aiDialog = new TSA::UI::AIRuntimeDialog(m_aiOrchestrator, this);
        m_aiDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_aiDialog->showPage(static_cast<TSA::UI::AIRuntimeDialog::Page>(page));
    m_aiDialog->show();
    m_aiDialog->raise();
    m_aiDialog->activateWindow();
}
