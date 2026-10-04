#pragma once

// Panneau « IA CO-ENGINEERING » : conversation en flux, actions d'ingénierie (analyser, vérifier,
// expliquer la sélection), propositions à valider, indicateur LOCAL / CLOUD et avertissement
// permanent. Toute la logique est dans TSA::AI::AIOrchestrator ; ce panneau ne fait qu'afficher.

#include "../../AI/Core/AIOrchestrator.h"

#include <QDockWidget>
#include <QMap>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace TSA::UI
{

class AICoEngineeringDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AICoEngineeringDock(TSA::AI::AIOrchestrator* orchestrator, QWidget* parent = nullptr);

    /// Lance une tâche (depuis le ruban, le menu contextuel…) et affiche le panneau.
    void runTask(TSA::AI::AITask task, const QString& text = QString());

signals:
    void configureRequested();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Entry
    {
        QString role;   // user, assistant, report, info, error
        QString title;
        QString markdown;
        QString meta;
    };

    void setupUi();
    void refreshStatus();
    void appendEntry(const Entry& e);
    void scheduleRender();
    void render();
    void send();
    void addProposalCard(const TSA::AI::ActionProposal& p);
    void setBusyUi(bool busy);

private:
    TSA::AI::AIOrchestrator* m_ai = nullptr;

    QLabel* m_statusChip = nullptr;
    QLabel* m_statusDetails = nullptr;
    QToolButton* m_btnEngine = nullptr;
    QToolButton* m_btnSettings = nullptr;
    QPushButton* m_btnAnalyze = nullptr;
    QPushButton* m_btnCheck = nullptr;
    QPushButton* m_btnExplain = nullptr;
    QToolButton* m_btnNew = nullptr;
    QTextBrowser* m_view = nullptr;
    QWidget* m_proposalsBox = nullptr;
    QVBoxLayout* m_proposalsLayout = nullptr;
    QMap<QString, QWidget*> m_proposalCards;
    QLabel* m_activity = nullptr;
    QPlainTextEdit* m_input = nullptr;
    QPushButton* m_btnSend = nullptr;
    QPushButton* m_btnStop = nullptr;
    QLabel* m_privacy = nullptr;

    QList<Entry> m_entries;
    QTimer* m_renderTimer = nullptr;
    bool m_streaming = false;
    bool m_probeRequested = false;
};

} // namespace TSA::UI
