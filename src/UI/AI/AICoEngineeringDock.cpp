#include "AICoEngineeringDock.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::AI::AITask;
using TSA::AI::RuntimeState;

namespace
{
const char* kDisclaimer = "Les recommandations de l'IA doivent être vérifiées par un ingénieur qualifié avant toute utilisation dans un projet réel.";
}

AICoEngineeringDock::AICoEngineeringDock(TSA::AI::AIOrchestrator* orchestrator, QWidget* parent)
    : QDockWidget(tr("IA CO-ENGINEERING"), parent)
    , m_ai(orchestrator)
{
    setObjectName("AICoEngineeringDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(90); // le flux de jetons ne redessine pas plus de ~11 fois/s
    connect(m_renderTimer, &QTimer::timeout, this, &AICoEngineeringDock::render);
    setupUi();

    connect(m_ai, &TSA::AI::AIOrchestrator::statusChanged, this, &AICoEngineeringDock::refreshStatus);
    connect(m_ai, &TSA::AI::AIOrchestrator::assistantStarted, this, [this](const QString& provider) {
        m_streaming = true;
        appendEntry({ "assistant", provider, QString(), QString() });
        setBusyUi(true);
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::assistantDelta, this, [this](const QString& t) {
        if (!m_entries.isEmpty() && m_entries.last().role == "assistant") m_entries.last().markdown += t;
        scheduleRender();
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::assistantFinished, this, [this](const QString& full, const QString& meta) {
        if (!m_entries.isEmpty() && m_entries.last().role == "assistant")
        {
            m_entries.last().markdown = full;
            m_entries.last().meta = meta;
        }
        m_streaming = false;
        m_activity->clear();
        setBusyUi(false);
        render();
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::assistantFailed, this, [this](const QString& error) {
        if (m_streaming && !m_entries.isEmpty() && m_entries.last().role == "assistant" && m_entries.last().markdown.isEmpty())
            m_entries.removeLast();
        m_streaming = false;
        m_activity->clear();
        appendEntry({ "error", tr("IA indisponible"), error, QString() });
        setBusyUi(false);
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::deterministicReport, this, [this](const QString& title, const QString& md) {
        appendEntry({ "report", title, md, tr("Calculé par TSA — sans IA") });
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::toolActivity, this, [this](const QString& a) { m_activity->setText("⋯ " + a); });
    connect(m_ai, &TSA::AI::AIOrchestrator::proposalCreated, this, &AICoEngineeringDock::addProposalCard);
    connect(m_ai, &TSA::AI::AIOrchestrator::proposalResolved, this, [this](const QString& id, bool applied, const QString& msg) {
        if (QWidget* card = m_proposalCards.take(id)) card->deleteLater();
        m_proposalsBox->setVisible(!m_proposalCards.isEmpty());
        appendEntry({ applied ? "info" : "error", applied ? tr("Proposition appliquée") : tr("Proposition non appliquée"), msg, QString() });
    });
    connect(m_ai, &TSA::AI::AIOrchestrator::cloudConsentRequested, this, [this](const QString& reason) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Envoi vers un service Cloud"));
        box.setText(tr("<b>%1</b><br><br>Utiliser le fournisseur Cloud configuré ?").arg(reason.toHtmlEscaped()));
        box.setInformativeText(tr("⚠ Les données structurées du projet (géométrie, sections, charges, résultats) seront "
                                  "envoyées au fournisseur sélectionné. Les fichiers .tsa ne sont jamais envoyés."));
        auto* remember = new QCheckBox(tr("Ne plus demander pour ce fournisseur"), &box);
        box.setCheckBox(remember);
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.button(QMessageBox::Yes)->setText(tr("Autoriser l'envoi"));
        box.button(QMessageBox::No)->setText(tr("Refuser (rester local)"));
        box.setDefaultButton(QMessageBox::No);
        const bool ok = box.exec() == QMessageBox::Yes;
        m_ai->answerCloudConsent(ok, remember->isChecked());
    });
    refreshStatus();
}

void AICoEngineeringDock::setupUi()
{
    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // État du moteur
    auto* statusRow = new QHBoxLayout();
    m_statusChip = new QLabel(root);
    m_statusChip->setTextFormat(Qt::RichText);
    m_statusDetails = new QLabel(root);
    m_statusDetails->setStyleSheet("color: palette(mid); font-size: 11px;");
    m_statusDetails->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_btnEngine = new QToolButton(root);
    m_btnEngine->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_btnSettings = new QToolButton(root);
    m_btnSettings->setText(QStringLiteral("⚙"));
    m_btnSettings->setToolTip(tr("Configuration IA : matériel, modèles, mode, confidentialité, diagnostic"));
    statusRow->addWidget(m_statusChip);
    statusRow->addWidget(m_statusDetails, 1);
    statusRow->addWidget(m_btnEngine);
    statusRow->addWidget(m_btnSettings);
    layout->addLayout(statusRow);
    connect(m_btnSettings, &QToolButton::clicked, this, &AICoEngineeringDock::configureRequested);
    connect(m_btnEngine, &QToolButton::clicked, this, [this] {
        const auto s = m_ai->state();
        if (s == RuntimeState::Ready || s == RuntimeState::Starting || s == RuntimeState::Busy) m_ai->stopLocal();
        else if (s == RuntimeState::NotConfigured) emit configureRequested();
        else m_ai->startLocal();
    });

    // Actions d'ingénierie
    auto* actions = new QHBoxLayout();
    actions->setSpacing(4);
    m_btnAnalyze = new QPushButton(tr("Analyser le modèle"), root);
    m_btnAnalyze->setToolTip(tr("Synthèse TSA du modèle puis analyse par l'IA (système porteur, cohérence, points à vérifier)"));
    m_btnCheck = new QPushButton(tr("Vérifier la structure"), root);
    m_btnCheck->setToolTip(tr("Contrôles automatiques TSA (topologie, stabilité, propriétés, charges, résultats), expliqués par l'IA"));
    m_btnExplain = new QPushButton(tr("Expliquer la sélection"), root);
    m_btnExplain->setToolTip(tr("Explique le comportement de l'élément sélectionné à partir de ses données et résultats réels"));
    m_btnNew = new QToolButton(root);
    m_btnNew->setText(tr("Nouvelle"));
    m_btnNew->setToolTip(tr("Nouvelle conversation"));
    actions->addWidget(m_btnAnalyze);
    actions->addWidget(m_btnCheck);
    actions->addWidget(m_btnExplain);
    actions->addStretch();
    actions->addWidget(m_btnNew);
    layout->addLayout(actions);
    connect(m_btnAnalyze, &QPushButton::clicked, this, [this] { runTask(AITask::AnalyzeModel); });
    connect(m_btnCheck, &QPushButton::clicked, this, [this] { runTask(AITask::CheckStructure); });
    connect(m_btnExplain, &QPushButton::clicked, this, [this] { runTask(AITask::ExplainSelection, m_input->toPlainText().trimmed()); });
    connect(m_btnNew, &QToolButton::clicked, this, [this] {
        m_ai->resetConversation();
        m_entries.clear();
        for (QWidget* c : m_proposalCards) c->deleteLater();
        m_proposalCards.clear();
        m_proposalsBox->hide();
        render();
    });

    // Conversation
    m_view = new QTextBrowser(root);
    m_view->setOpenExternalLinks(false);
    m_view->setMinimumHeight(160);
    layout->addWidget(m_view, 1);

    // Propositions à valider
    m_proposalsBox = new QWidget(root);
    m_proposalsLayout = new QVBoxLayout(m_proposalsBox);
    m_proposalsLayout->setContentsMargins(0, 0, 0, 0);
    m_proposalsLayout->setSpacing(4);
    m_proposalsBox->hide();
    layout->addWidget(m_proposalsBox);

    m_activity = new QLabel(root);
    m_activity->setStyleSheet("color: palette(mid); font-style: italic;");
    layout->addWidget(m_activity);

    // Saisie
    m_input = new QPlainTextEdit(root);
    m_input->setPlaceholderText(tr("Question d'ingénierie… (Ctrl+Entrée pour envoyer)\nex. « Pourquoi la poutre B3 a-t-elle un moment élevé ? »"));
    m_input->setMaximumHeight(72);
    m_input->installEventFilter(this);
    layout->addWidget(m_input);
    auto* sendRow = new QHBoxLayout();
    m_privacy = new QLabel(root);
    m_privacy->setTextFormat(Qt::RichText);
    m_btnStop = new QPushButton(tr("Arrêter"), root);
    m_btnSend = new QPushButton(tr("Envoyer"), root);
    m_btnSend->setDefault(true);
    sendRow->addWidget(m_privacy, 1);
    sendRow->addWidget(m_btnStop);
    sendRow->addWidget(m_btnSend);
    layout->addLayout(sendRow);
    connect(m_btnSend, &QPushButton::clicked, this, &AICoEngineeringDock::send);
    connect(m_btnStop, &QPushButton::clicked, m_ai, &TSA::AI::AIOrchestrator::cancel);

    auto* disclaimer = new QLabel(tr(kDisclaimer), root);
    disclaimer->setWordWrap(true);
    disclaimer->setStyleSheet("color: #D29922; font-size: 11px;");
    layout->addWidget(disclaimer);

    setWidget(root);
    setMinimumWidth(320);
    setBusyUi(false);
    render();
}

void AICoEngineeringDock::showEvent(QShowEvent* event)
{
    QDockWidget::showEvent(event);
    if (!m_probeRequested && !m_ai->hardwareProbed())
    {
        m_probeRequested = true;
        m_ai->probeHardwareAsync();
    }
}

bool AICoEngineeringDock::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress)
    {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && (ke->modifiers() & Qt::ControlModifier))
        {
            send();
            return true;
        }
    }
    return QDockWidget::eventFilter(watched, event);
}

void AICoEngineeringDock::runTask(AITask task, const QString& text)
{
    if (!isVisible()) show();
    raise();
    if (m_ai->isBusy()) return;
    static const QMap<int, QString> titles = {
        { static_cast<int>(AITask::AnalyzeModel), tr("Analyser le modèle") },
        { static_cast<int>(AITask::CheckStructure), tr("Vérifier la structure") },
        { static_cast<int>(AITask::ExplainSelection), tr("Expliquer la sélection") } };
    if (task == AITask::Chat)
        appendEntry({ "user", tr("Vous"), text, QString() });
    else
        appendEntry({ "user", tr("Vous"), QStringLiteral("**%1**%2").arg(titles.value(static_cast<int>(task)), text.isEmpty() ? QString() : QStringLiteral(" — ") + text), QString() });
    if (task != AITask::Chat) m_input->clear();
    m_ai->submit(task, text);
}

void AICoEngineeringDock::send()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_ai->isBusy()) return;
    m_input->clear();
    runTask(AITask::Chat, text);
}

void AICoEngineeringDock::setBusyUi(bool busy)
{
    m_btnSend->setEnabled(!busy);
    m_btnStop->setVisible(busy);
    m_btnAnalyze->setEnabled(!busy);
    m_btnCheck->setEnabled(!busy);
    m_btnExplain->setEnabled(!busy);
}

void AICoEngineeringDock::refreshStatus()
{
    const auto s = m_ai->state();
    const QString label = m_ai->runtimeLabel();
    QString color = "#8B949E";
    QString engine = tr("Démarrer");
    switch (s)
    {
    case RuntimeState::Ready: color = "#3FB950"; engine = tr("Arrêter"); break;
    case RuntimeState::Busy: color = "#58A6FF"; engine = tr("Arrêter"); break;
    case RuntimeState::Starting: color = "#D29922"; engine = tr("Arrêter"); break;
    case RuntimeState::Error: color = "#F85149"; engine = tr("Redémarrer"); break;
    case RuntimeState::NotConfigured: engine = tr("Configurer…"); break;
    case RuntimeState::Stopped: break;
    }
    const QString suffix = s == RuntimeState::Starting ? tr(" (chargement…)") : (s == RuntimeState::Error ? tr(" (erreur)") : QString());
    m_statusChip->setText(QStringLiteral("<span style='color:%1;font-size:14px'>●</span> <b>%2</b>%3").arg(color, label.toHtmlEscaped(), suffix));
    m_statusDetails->setText(m_ai->runtimeDetails());
    m_statusDetails->setToolTip(s == RuntimeState::Error && m_ai->localServer() ? m_ai->localServer()->lastError() : m_ai->runtimeDetails());
    m_btnEngine->setText(engine);
    m_btnEngine->setVisible(m_ai->settings().mode != TSA::AI::AIMode::Cloud);

    const bool cloud = m_ai->settings().mode == TSA::AI::AIMode::Cloud && !label.startsWith("LOCAL");
    m_privacy->setText(cloud ? tr("<span style='color:#D29922'>☁ CLOUD — données du projet envoyées au fournisseur</span>")
                             : (m_ai->settings().mode == TSA::AI::AIMode::Auto
                                    ? tr("🔒 LOCAL · AUTO (Cloud uniquement avec votre accord)")
                                    : tr("🔒 LOCAL — aucune donnée ne quitte ce poste")));
}

void AICoEngineeringDock::appendEntry(const Entry& e)
{
    m_entries.append(e);
    render();
}

void AICoEngineeringDock::scheduleRender()
{
    if (!m_renderTimer->isActive()) m_renderTimer->start();
}

void AICoEngineeringDock::render()
{
    if (m_entries.isEmpty())
    {
        m_view->setHtml(tr("<div style='color:gray'><h3>Assistant de co-ingénierie TSA</h3>"
                           "<p>Il lit le modèle structural réel (géométrie, sections, matériaux, appuis, charges, résultats) "
                           "et utilise les outils de TSA. Il ne modifie jamais le modèle sans votre accord.</p>"
                           "<ul><li><b>Analyser le modèle</b> : synthèse et points à vérifier</li>"
                           "<li><b>Vérifier la structure</b> : contrôles automatiques expliqués</li>"
                           "<li><b>Expliquer la sélection</b> : pourquoi cet élément se comporte ainsi</li></ul>"
                           "<p>Les contrôles automatiques fonctionnent même sans modèle IA installé.</p></div>"));
        return;
    }
    QString md;
    for (const Entry& e : m_entries)
    {
        QString header;
        if (e.role == "user") header = QStringLiteral("##### 🧑 %1").arg(e.title);
        else if (e.role == "assistant") header = QStringLiteral("##### 🤖 IA%1").arg(e.title.isEmpty() ? QString() : QStringLiteral(" · ") + e.title);
        else if (e.role == "report") header = QStringLiteral("##### 📋 %1").arg(e.title);
        else if (e.role == "error") header = QStringLiteral("##### ⚠ %1").arg(e.title);
        else header = QStringLiteral("##### ✓ %1").arg(e.title);
        QString body = e.markdown;
        if (e.role == "assistant" && body.isEmpty()) body = QStringLiteral("_…_");
        md += header + "\n\n" + body + "\n\n";
        if (!e.meta.isEmpty()) md += QStringLiteral("_%1_\n\n").arg(e.meta);
        md += "---\n\n";
    }
    QScrollBar* bar = m_view->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 24;
    m_view->setMarkdown(md);
    if (atBottom) bar->setValue(bar->maximum());
}

void AICoEngineeringDock::addProposalCard(const TSA::AI::ActionProposal& p)
{
    auto* card = new QFrame(m_proposalsBox);
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet("QFrame { border: 1px solid #388BFD; border-radius: 4px; }");
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(8, 6, 8, 6);
    auto* title = new QLabel(QStringLiteral("<b>Proposition de l'IA</b><br>%1").arg(p.title.toHtmlEscaped()), card);
    title->setWordWrap(true);
    title->setStyleSheet("border: none;");
    l->addWidget(title);
    if (!p.rationale.isEmpty())
    {
        auto* why = new QLabel(tr("Justification : %1").arg(p.rationale), card);
        why->setWordWrap(true);
        why->setStyleSheet("border: none; color: palette(mid);");
        l->addWidget(why);
    }
    if (!p.impacts.isEmpty())
    {
        auto* impacts = new QLabel("• " + p.impacts.join("\n• "), card);
        impacts->setWordWrap(true);
        impacts->setStyleSheet("border: none; font-size: 11px;");
        l->addWidget(impacts);
    }
    auto* row = new QHBoxLayout();
    auto* apply = new QPushButton(p.tool == "propose_run_analysis" ? tr("Lancer le calcul") : tr("Appliquer"), card);
    auto* refuse = new QPushButton(tr("Refuser"), card);
    row->addStretch();
    row->addWidget(refuse);
    row->addWidget(apply);
    l->addLayout(row);
    const QString id = p.id;
    connect(apply, &QPushButton::clicked, this, [this, id] {
        QString err;
        m_ai->acceptProposal(id, &err);
    });
    connect(refuse, &QPushButton::clicked, this, [this, id] { m_ai->rejectProposal(id); });
    m_proposalsLayout->addWidget(card);
    m_proposalCards.insert(id, card);
    m_proposalsBox->show();
}

} // namespace TSA::UI
