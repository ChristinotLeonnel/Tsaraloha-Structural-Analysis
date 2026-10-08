#include "AnalysisManagerPanel.h"

#include "AnalysisDialog.h"
#include "AnalysisEngineOptions.h"
#include "../../Analysis/AnalysisController.h"
#include "../../Analysis/Engine/AnalysisModel.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Model/Model.h"
#include "../../Project/ProjectSession.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{

using namespace TSA::Analysis;

namespace
{
QString yesNo(bool v) { return v ? QStringLiteral("✔") : QStringLiteral("—"); }

QString typeLabel(AnalysisType t)
{
    return t == AnalysisType::NonLinearStatic ? QObject::tr("statique non linéaire") : QObject::tr("statique linéaire");
}
} // namespace

AnalysisManagerPanel::AnalysisManagerPanel(TSA::Project::ProjectSession* session, const AnalysisEngineOptionsRegistry* options,
                                           QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_options(options)
{
    setObjectName("AnalysisManagerPanel");
    buildUi();

    AnalysisController& ac = m_session->analysis();
    connect(&ac, &AnalysisController::started, this, [this](const QString& engine) {
        setRunning(true);
        m_status->setText(tr("Calcul %1 en cours…").arg(engine));
        emit logMessage(tr("--- CALCUL %1 ---").arg(engine.toUpper()), QStringLiteral("SYS"));
    });
    connect(&ac, &AnalysisController::progressChanged, this, [this](int pct, const QString& status) {
        m_progress->setValue(std::clamp(pct, 0, 100));
        m_status->setText(status);
    });
    connect(&ac, &AnalysisController::logMessage, this,
            [this](const QString& line) { emit logMessage(line, QStringLiteral("INFO")); });
    connect(&ac, &AnalysisController::finished, this, [this](bool ok, const QString& message) {
        setRunning(false);
        m_progress->setValue(ok ? 100 : 0);
        m_status->setText(ok ? tr("Calcul terminé.") : tr("Échec : %1").arg(message));
        emit logMessage(ok ? message : tr("Échec du calcul : %1").arg(message), ok ? QStringLiteral("SUCCESS") : QStringLiteral("ERROR"));
        if (!ok)
        {
            const std::string& journal = m_session->analysis().lastRun().results.journalLog();
            if (!journal.empty()) emit logMessage(QString::fromStdString(journal), QStringLiteral("ERROR"));
        }
        emit analysisFinished(ok);
    });
    connect(&ac, &AnalysisController::resultsChanged, this, &AnalysisManagerPanel::updateResultsSummary);
    connect(&ac, &AnalysisController::resultsBecameStale, this, &AnalysisManagerPanel::updateResultsSummary);
    refresh();
}

void AnalysisManagerPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* title = new QLabel(tr("ANALYSIS MANAGER — moteurs, réglages, calcul"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700;"));
    root->addWidget(title);

    auto* split = new QSplitter(Qt::Vertical, this);
    root->addWidget(split, 1);

    // Moteurs enregistrés : capacités et disponibilité (aucune liste écrite en dur).
    auto* enginesBox = new QGroupBox(tr("Moteurs de calcul"), split);
    auto* enginesLay = new QVBoxLayout(enginesBox);
    m_engines = new QTableWidget(enginesBox);
    m_engines->setColumnCount(8);
    m_engines->setHorizontalHeaderLabels({ tr("Moteur"), tr("Version"), tr("Disponible"), "2D", "3D", tr("Treillis"),
                                           tr("Ressorts"), tr("Matrice K") });
    m_engines->verticalHeader()->setVisible(false);
    m_engines->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_engines->setSelectionMode(QAbstractItemView::NoSelection);
    m_engines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    enginesLay->addWidget(m_engines);

    // Réglages du projet + actions
    auto* ctxBox = new QGroupBox(tr("Réglages d'analyse du projet"), split);
    auto* ctxLay = new QVBoxLayout(ctxBox);
    m_context = new QLabel(ctxBox);
    m_context->setWordWrap(true);
    m_context->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ctxLay->addWidget(m_context);
    auto* buttons = new QHBoxLayout();
    m_btnConfigure = new QPushButton(tr("Configurer…"), ctxBox);
    m_btnValidate = new QPushButton(tr("Valider"), ctxBox);
    m_btnRun = new QPushButton(tr("▶ Calculer"), ctxBox);
    m_btnRun->setDefault(true);
    m_btnCancel = new QPushButton(tr("Annuler le calcul"), ctxBox);
    for (QPushButton* b : { m_btnConfigure, m_btnValidate, m_btnRun, m_btnCancel }) buttons->addWidget(b);
    buttons->addStretch(1);
    ctxLay->addLayout(buttons);
    m_progress = new QProgressBar(ctxBox);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    ctxLay->addWidget(m_progress);
    m_status = new QLabel(tr("Prêt."), ctxBox);
    ctxLay->addWidget(m_status);
    connect(m_btnConfigure, &QPushButton::clicked, this, &AnalysisManagerPanel::configure);
    connect(m_btnValidate, &QPushButton::clicked, this, &AnalysisManagerPanel::validate);
    connect(m_btnRun, &QPushButton::clicked, this, &AnalysisManagerPanel::run);
    connect(m_btnCancel, &QPushButton::clicked, this, &AnalysisManagerPanel::cancel);

    // Bilan de validation
    auto* valBox = new QGroupBox(tr("Validation du modèle d'analyse"), split);
    auto* valLay = new QVBoxLayout(valBox);
    m_validation = new QListWidget(valBox);
    valLay->addWidget(m_validation);

    // Synthèse des résultats
    auto* resBox = new QGroupBox(tr("Résultats"), split);
    auto* resLay = new QVBoxLayout(resBox);
    m_results = new QLabel(resBox);
    m_results->setWordWrap(true);
    m_results->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resLay->addWidget(m_results);
    resLay->addStretch(1);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setStretchFactor(2, 1);
    split->setStretchFactor(3, 1);
    setRunning(false);
}

void AnalysisManagerPanel::refresh()
{
    fillEngines();
    updateContextSummary();
    updateResultsSummary();
    m_validation->clear();
}

void AnalysisManagerPanel::fillEngines()
{
    AnalysisEngineRegistry& reg = m_session->analysis().registry();
    const auto ids = reg.ids();
    m_engines->setRowCount(static_cast<int>(ids.size()));
    int row = 0;
    for (const auto& id : ids)
    {
        const AnalysisEngine* e = reg.engine(id);
        const EngineInfo info = e->info();
        const AnalysisCapabilities caps = e->capabilities();
        const EngineAvailability av = e->availability();
        const QString cells[] = { QString::fromStdString(info.name), QString::fromStdString(info.version),
                                  av.available ? tr("oui") : tr("non"), yesNo(caps.supports2D), yesNo(caps.supports3D),
                                  yesNo(caps.supportsTruss), yesNo(caps.supportsSprings), yesNo(caps.providesGlobalStiffness) };
        for (int c = 0; c < 8; ++c)
        {
            auto* item = new QTableWidgetItem(cells[c]);
            item->setToolTip(c == 2 && !av.available ? QString::fromStdString(av.message) : QString::fromStdString(info.description));
            if (c > 1) item->setTextAlignment(Qt::AlignCenter);
            m_engines->setItem(row, c, item);
        }
        ++row;
    }
    m_engines->resizeColumnsToContents();
    m_engines->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Tous les moteurs visibles sans défilement (la liste est courte).
    int h = m_engines->horizontalHeader()->height() + 2 * m_engines->frameWidth();
    for (int r = 0; r < m_engines->rowCount(); ++r) h += m_engines->rowHeight(r);
    m_engines->setMinimumHeight(h);
    m_engines->setMaximumHeight(h);
}

void AnalysisManagerPanel::updateContextSummary()
{
    const AnalysisController& ac = m_session->analysis();
    const AnalysisContext& c = ac.context();
    const AnalysisEngine* engine = ac.registry().engine(c.engineId);

    QString scope = tr("modèle complet");
    if (c.scope.type != ScopeType::EntireModel)
    {
        scope = tr("portée personnalisée");
        for (const auto& o : AnalysisScopeResolver::availableScopes(m_session->model(), &m_session->grids()))
            if (o.scope == c.scope) scope = QString::fromStdString(o.label);
        if (c.scope.type == ScopeType::SelectedElements) scope = tr("éléments sélectionnés");
        if (c.scope.type == ScopeType::ModelPlane && scope == tr("portée personnalisée"))
            scope = tr("plan du modèle (détecté au calcul)");
    }
    QString loads;
    const auto& lm = m_session->model().loadManager();
    if (c.combinationId > 0)
    {
        const auto* combo = lm.getCombination(c.combinationId);
        loads = tr("combinaison %1").arg(combo ? QString::fromStdString(combo->name()) : QString::number(c.combinationId));
    }
    else if (c.loadCaseIds.empty())
        loads = tr("tous les cas de charge");
    else
        loads = tr("%n cas de charge", nullptr, static_cast<int>(c.loadCaseIds.size()));

    const bool exportSystem = c.settingsFor(c.engineId).value(QStringLiteral("exportSystem")).toBool(false);
    m_context->setText(tr("<b>Moteur</b> : %1 &nbsp;·&nbsp; <b>Calcul</b> : %2 %3 &nbsp;·&nbsp; <b>Portée</b> : %4<br>"
                          "<b>Chargement</b> : %5%6%7")
                           .arg(engine ? QString::fromStdString(engine->info().name).toHtmlEscaped() : tr("aucun"))
                           .arg(typeLabel(c.type), c.dimension == AnalysisDimension::Plane2D ? tr("plan (2D)") : tr("spatial (3D)"))
                           .arg(scope.toHtmlEscaped(), loads.toHtmlEscaped())
                           .arg(c.common.includeSelfWeight ? tr(" + poids propre") : QString())
                           .arg(exportSystem ? tr(" &nbsp;·&nbsp; système K·U = F exporté") : QString()));
}

void AnalysisManagerPanel::updateResultsSummary()
{
    const AnalysisController& ac = m_session->analysis();
    const auto r = ac.results();
    if (!r)
    {
        m_results->setText(tr("Aucun résultat : configurez puis lancez le calcul."));
        return;
    }
    const auto& meta = r->executionMetadata();
    const auto sum = r->summary();
    const auto eq = r->equilibrium();
    const auto& adv = r->advanced();
    QString text = tr("<b>%1</b>%2 — %3<br>").arg(QString::fromStdString(meta.solverEngine).toHtmlEscaped(),
                                                   meta.solverVersion.empty() ? QString() : " " + QString::fromStdString(meta.solverVersion).toHtmlEscaped(),
                                                   meta.analysisScope.empty() ? tr("modèle complet") : QString::fromStdString(meta.analysisScope).toHtmlEscaped());
    text += tr("Déplacement max : <b>%1 mm</b> (N%2) · Moment max : <b>%3 %4</b> · Réaction verticale totale : %5 %6")
                .arg(sum.maxDisplacement * 1000.0, 0, 'f', 3).arg(sum.maxDisplacementNodeId)
                .arg(sum.maxBendingMoment, 0, 'f', 2).arg(QString::fromStdString(r->units().moment))
                .arg(eq.reactionFz, 0, 'f', 2).arg(QString::fromStdString(r->units().force));
    if (adv.available && adv.hasGlobalStiffness)
        text += tr("<br>Système : %1 équation(s), %2 coefficient(s) non nul(s) de K (dock « Données d'analyse », espace Recherche).")
                    .arg(adv.kGlobal.rows).arg(adv.kGlobal.nonZeros());
    if (!ac.resultsUpToDate())
        text += tr("<br><span style='color:#d9822b'><b>Résultats obsolètes</b> : le modèle a changé depuis le calcul.</span>");
    m_results->setText(text);
}

void AnalysisManagerPanel::setRunning(bool running)
{
    m_btnConfigure->setEnabled(!running);
    m_btnValidate->setEnabled(!running);
    m_btnRun->setEnabled(!running);
    m_btnCancel->setEnabled(running);
}

void AnalysisManagerPanel::configure()
{
    AnalysisController& ac = m_session->analysis();
    if (ac.isRunning() || !m_options) return;
    const TSA::Model::ElementSet selection = m_selection ? m_selection() : TSA::Model::ElementSet {};
    AnalysisDialog dlg(ac.manager(), *m_options, &m_session->model(), &m_session->grids(), selection, this);
    dlg.setContext(ac.context());
    if (dlg.exec() != QDialog::Accepted) return;
    ac.setContext(dlg.context());
    if (ac.storeContextInModel()) emit settingsChanged();
    updateContextSummary();
    emit logMessage(tr("Réglages d'analyse enregistrés avec le projet."), QStringLiteral("INFO"));
    if (dlg.runRequested()) run();
}

bool AnalysisManagerPanel::validate()
{
    AnalysisController& ac = m_session->analysis();
    m_validation->clear();
    const PreparedAnalysis prepared = ac.prepare();
    if (!prepared.extracted)
        m_validation->addItem(new QListWidgetItem(tr("Modèle d'analyse non extrait (moteur ou portée invalide).")));
    else
        m_validation->addItem(new QListWidgetItem(tr("Portée « %1 » : %2 nœud(s), %3 barre(s).")
                                                      .arg(QString::fromStdString(prepared.model.scopeLabel))
                                                      .arg(prepared.model.mapping.nodeCount())
                                                      .arg(prepared.model.mapping.elementCount())));
    for (const auto& m : prepared.validation.messages())
    {
        const QString prefix = m.severity == ValidationSeverity::Error ? tr("Erreur") :
                               m.severity == ValidationSeverity::Warning ? tr("Avertissement") : tr("Info");
        auto* item = new QListWidgetItem(QStringLiteral("%1 — %2 : %3").arg(prefix, QString::fromStdString(m.category),
                                                                            QString::fromStdString(m.text)));
        if (m.severity == ValidationSeverity::Error) item->setForeground(QColor(220, 70, 70));
        else if (m.severity == ValidationSeverity::Warning) item->setForeground(QColor(217, 130, 43));
        m_validation->addItem(item);
    }
    const bool ok = prepared.canRun();
    m_status->setText(ok ? tr("Modèle d'analyse valide.") : tr("Le modèle d'analyse n'est pas valide."));
    return ok;
}

bool AnalysisManagerPanel::run()
{
    AnalysisController& ac = m_session->analysis();
    if (ac.isRunning()) return false;
    const AnalysisEngine* engine = ac.registry().engine(ac.context().engineId);
    if (!engine)
    {
        m_status->setText(tr("Aucun moteur d'analyse sélectionné."));
        return false;
    }
    const EngineAvailability av = engine->availability();
    if (!av.available)
    {
        m_status->setText(tr("Moteur %1 indisponible : %2").arg(QString::fromStdString(engine->info().name),
                                                                  QString::fromStdString(av.message)));
        return false;
    }
    if (!validate()) return false;
    const PreparedAnalysis prepared = ac.prepare();
    if (prepared.validation.hasWarnings())
    {
        const auto reply = QMessageBox::warning(this, tr("Avertissements avant calcul"),
                                                tr("Le modèle d'analyse présente des avertissements (voir la liste). Lancer quand même le calcul ?"),
                                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return false;
    }
    m_progress->setValue(0);
    return ac.start(ac.context(), prepared);
}

void AnalysisManagerPanel::cancel()
{
    m_session->analysis().cancel();
    m_status->setText(tr("Annulation du calcul…"));
}

} // namespace TSA::UI
