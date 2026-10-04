#include "AIRuntimeDialog.h"

#include "../../AI/Core/AILog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::AI;

namespace
{
QString gb(double v) { return QString::number(v, 'f', 1) + " Go"; }
}

AIRuntimeDialog::AIRuntimeDialog(AIOrchestrator* orchestrator, QWidget* parent)
    : QDialog(parent)
    , m_ai(orchestrator)
{
    setWindowTitle(tr("Configuration IA — TSA Co-Engineering"));
    resize(760, 600);
    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildSetupPage(), tr("Configuration"));
    m_tabs->addTab(buildModelsPage(), tr("Modèles"));
    m_tabs->addTab(buildPrivacyPage(), tr("Mode et confidentialité"));
    m_tabs->addTab(buildDiagnosticsPage(), tr("Diagnostic"));
    m_tabs->addTab(buildLogPage(), tr("Journal"));
    layout->addWidget(m_tabs, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto* mm = m_ai->modelManager();
    connect(mm, &ModelManager::downloadProgress, this, [this](const QString& id, qint64 rec, qint64 total) {
        m_progress->setVisible(true);
        m_progress->setRange(0, 1000);
        m_progress->setValue(total > 0 ? static_cast<int>(1000.0 * rec / total) : 0);
        m_progressLabel->setText(tr("Téléchargement de %1 : %2 / %3").arg(id, gb(rec / 1073741824.0), gb(total / 1073741824.0)));
        m_btnCancelDownload->setVisible(true);
    });
    connect(mm, &ModelManager::downloadFinished, this, [this](const QString& id, const QString&) {
        m_progress->setVisible(false);
        m_btnCancelDownload->setVisible(false);
        m_progressLabel->setText(tr("✓ %1 installé et vérifié (SHA-256).").arg(id));
        refreshModels();
        if (m_installAfterDownload)
        {
            m_installAfterDownload = false;
            installRecommended();
        }
    });
    connect(mm, &ModelManager::downloadFailed, this, [this](const QString& id, const QString& err) {
        m_progress->setVisible(false);
        m_btnCancelDownload->setVisible(false);
        m_installAfterDownload = false;
        m_progressLabel->setText(tr("✗ Échec pour %1 : %2").arg(id, err));
        refreshModels();
    });
    connect(m_ai, &AIOrchestrator::hardwareReady, this, [this] {
        // Les GPU ne sont connus qu'après la détection : liste des périphériques complétée ici.
        for (const auto& g : m_ai->hardware().gpus)
            if (m_device->findData(g.deviceId) < 0) m_device->addItem(QStringLiteral("%1 — %2").arg(g.deviceId, g.name), g.deviceId);
        m_device->setCurrentIndex(std::max(0, m_device->findData(m_ai->settings().deviceId)));
        refreshSetup();
        refreshModels();
    });
    connect(m_ai, &AIOrchestrator::statusChanged, this, &AIRuntimeDialog::refreshSetup);
    connect(m_ai, &AIOrchestrator::diagnosticsProgress, this, [this](const QString& l) { m_diag->appendPlainText(l); });
    connect(m_ai, &AIOrchestrator::diagnosticsFinished, this, [this](bool, const QString&) { m_btnDiag->setEnabled(true); refreshLog(); });

    if (!m_ai->hardwareProbed()) m_ai->probeHardwareAsync();
    refreshSetup();
    refreshModels();
    refreshLog();
}

void AIRuntimeDialog::showPage(Page page)
{
    m_tabs->setCurrentIndex(static_cast<int>(page));
}

// -----------------------------------------------------------------------------
// Configuration recommandée
// -----------------------------------------------------------------------------

QWidget* AIRuntimeDialog::buildSetupPage()
{
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    auto* sys = new QGroupBox(tr("Analyse du système"), page);
    auto* sl = new QVBoxLayout(sys);
    m_systemLabel = new QLabel(tr("Analyse du système…"), sys);
    m_systemLabel->setTextFormat(Qt::RichText);
    m_systemLabel->setWordWrap(true);
    m_systemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(m_systemLabel);
    l->addWidget(sys);

    auto* rec = new QGroupBox(tr("Configuration recommandée"), page);
    auto* rl = new QVBoxLayout(rec);
    m_recommendLabel = new QLabel(rec);
    m_recommendLabel->setTextFormat(Qt::RichText);
    m_recommendLabel->setWordWrap(true);
    rl->addWidget(m_recommendLabel);
    auto* row = new QHBoxLayout();
    m_btnInstall = new QPushButton(tr("Installer la configuration recommandée"), rec);
    m_btnInstall->setDefault(true);
    m_btnCalibrate = new QPushButton(tr("Mesurer les performances (≈1 min)"), rec);
    m_btnCalibrate->setToolTip(tr("Mesure réelle CPU et GPU avec le plus petit modèle installé ; remplace les estimations"));
    row->addWidget(m_btnCalibrate);
    row->addStretch();
    row->addWidget(m_btnInstall);
    rl->addLayout(row);
    m_progress = new QProgressBar(rec);
    m_progress->setVisible(false);
    rl->addWidget(m_progress);
    m_progressLabel = new QLabel(rec);
    m_progressLabel->setWordWrap(true);
    rl->addWidget(m_progressLabel);
    l->addWidget(rec);
    l->addStretch();
    connect(m_btnInstall, &QPushButton::clicked, this, &AIRuntimeDialog::installRecommended);
    connect(m_btnCalibrate, &QPushButton::clicked, this, [this] {
        m_btnCalibrate->setEnabled(false);
        m_progressLabel->setText(tr("Mesure en cours…"));
        m_ai->runCalibration();
    });
    connect(m_ai, &AIOrchestrator::calibrationProgress, this, [this](const QString& l) { m_progressLabel->setText(l); });
    connect(m_ai, &AIOrchestrator::calibrationFinished, this, [this](bool, const QString& summary) {
        m_btnCalibrate->setEnabled(true);
        m_progressLabel->setText(summary);
        refreshSetup();
        refreshModels();
    });
    return page;
}

void AIRuntimeDialog::refreshSetup()
{
    if (!m_ai->hardwareProbed())
    {
        m_systemLabel->setText(tr("Analyse du système (CPU, RAM, GPU, backends)…"));
        m_recommendLabel->setText(QString());
        m_btnInstall->setEnabled(false);
        return;
    }
    const auto& h = m_ai->hardware();
    QString gpus;
    for (const auto& g : h.gpus)
        gpus += QStringLiteral("<br>&nbsp;&nbsp;• %1 — %2, %3 libres / %4 (%5)%6")
                    .arg(g.name.toHtmlEscaped(), g.deviceId, gb(g.freeGB), gb(g.totalGB), g.vendor,
                         g.integrated ? tr(" — intégré, non utilisé") : QString());
    if (h.gpus.empty()) gpus = tr(" aucun GPU utilisable par le moteur");
    m_systemLabel->setText(
        tr("<b>CPU</b> : %1 — %2 cœurs / %3 threads%4%5<br><b>RAM</b> : %6 (%7 libres)<br><b>GPU</b> :%8"
           "<br><b>Runtimes</b> : CUDA %9 · Vulkan %10 · HIP %11"
           "<br><b>Moteur d'inférence</b> : %12")
            .arg(h.cpuName.toHtmlEscaped()).arg(h.physicalCores).arg(h.logicalCores)
            .arg(h.avx2 ? ", AVX2" : "", h.avx512 ? ", AVX-512" : "")
            .arg(gb(h.ramGB), gb(h.ramAvailableGB), gpus)
            .arg(h.cudaDriver ? "✓" : "—", h.vulkanLoader ? "✓" : "—", h.hipRuntime ? "✓" : "—")
            .arg(h.llamaServerPath.isEmpty() ? tr("<span style='color:#F85149'>llama.cpp introuvable</span>")
                                             : tr("llama.cpp %1 (%2)").arg(h.llamaVersion, h.llamaBackends.join(", "))));

    const auto& r = m_ai->recommendation();
    if (!r.model)
    {
        m_recommendLabel->setText(tr("<b>Mode</b> : Cloud proposé<br>%1").arg(r.reasons.join("<br>")));
        m_btnInstall->setEnabled(false);
        return;
    }
    const auto installed = m_ai->modelManager()->scanInstalled();
    const bool have = m_ai->modelManager()->findInstalled(installed, r.model->id) != nullptr;
    m_recommendLabel->setText(
        tr("✓ <b>Mode</b> : %1<br>✓ <b>Modèle</b> : %2 (%3)<br>✓ <b>Quantification</b> : %4<br>✓ <b>Backend</b> : %5%6"
           "<br>✓ <b>Contexte</b> : %7 jetons · mémoire ≈ %8 · vitesse ≈ %9"
           "<br><span style='color:gray'>%10</span>")
            .arg(executionModeLabel(r.mode), r.model->displayName, gb(r.model->fileSizeGB()), r.model->quantization, r.backend,
                 r.deviceId.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(r.deviceId))
            .arg(r.contextSize).arg(gb(r.memoryRequiredGB)).arg(QStringLiteral("%1 jetons/s%2").arg(r.estimatedTokensPerSec, 0, 'f', 0)
                 .arg(m_ai->hasCalibration() ? tr(" (d'après mesure)") : tr(" (estimation — « Mesurer les performances » pour affiner)")))
            .arg(r.reasons.join("<br>")));
    m_btnInstall->setEnabled(!h.llamaServerPath.isEmpty() && !m_ai->modelManager()->isDownloading());
    m_btnInstall->setText(have ? tr("Appliquer la configuration recommandée") : tr("Installer la configuration recommandée (%1)").arg(gb(r.model->fileSizeGB())));
}

void AIRuntimeDialog::installRecommended()
{
    const auto& r = m_ai->recommendation();
    if (!r.model) return;
    const auto installed = m_ai->modelManager()->scanInstalled();
    if (!m_ai->modelManager()->findInstalled(installed, r.model->id))
    {
        const auto answer = QMessageBox::question(this, tr("Téléchargement du modèle"),
            tr("Télécharger %1 %2 (%3) depuis Hugging Face (dépôt officiel %4) ?\n\nLe fichier est vérifié par SHA-256. "
               "Une fois installé, l'IA fonctionne hors ligne.")
                .arg(r.model->displayName, r.model->quantization, gb(r.model->fileSizeGB()), r.model->hfRepo));
        if (answer != QMessageBox::Yes) return;
        QString err;
        if (!m_ai->modelManager()->startDownload(r.model->id, &err))
        {
            m_progressLabel->setText(err);
            return;
        }
        m_installAfterDownload = true;
        m_btnInstall->setEnabled(false);
        return;
    }
    m_ai->stopLocal();
    m_ai->applyRecommendation();
    m_ai->startLocal();
    m_progressLabel->setText(tr("✓ Configuration appliquée. Chargement du modèle en cours…"));
}

// -----------------------------------------------------------------------------
// Modèles
// -----------------------------------------------------------------------------

QWidget* AIRuntimeDialog::buildModelsPage()
{
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    m_models = new QTableWidget(page);
    m_models->setColumnCount(6);
    m_models->setHorizontalHeaderLabels({ tr("Modèle"), tr("Quantif."), tr("Taille"), tr("État"), tr("Sur cette machine"), tr("Source") });
    m_models->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_models->horizontalHeader()->setStretchLastSection(true);
    m_models->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_models->setSelectionMode(QAbstractItemView::SingleSelection);
    m_models->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_models->verticalHeader()->hide();
    l->addWidget(m_models, 1);
    auto* row = new QHBoxLayout();
    m_btnDownload = new QPushButton(tr("Télécharger"), page);
    m_btnRemove = new QPushButton(tr("Supprimer"), page);
    m_btnDefault = new QPushButton(tr("Utiliser par défaut"), page);
    auto* btnTest = new QPushButton(tr("Tester"), page);
    m_btnCancelDownload = new QPushButton(tr("Annuler le téléchargement"), page);
    m_btnCancelDownload->setVisible(false);
    row->addWidget(m_btnDownload);
    row->addWidget(m_btnRemove);
    row->addWidget(m_btnDefault);
    row->addWidget(btnTest);
    row->addStretch();
    row->addWidget(m_btnCancelDownload);
    l->addLayout(row);
    auto* note = new QLabel(tr("Dossier TSA : %1. Les modèles GGUF déjà présents (cache Hugging Face, LM Studio) sont réutilisés sans copie.")
                                .arg(ModelManager::modelsDirectory()), page);
    note->setWordWrap(true);
    note->setStyleSheet("color: gray;");
    l->addWidget(note);

    connect(m_btnDownload, &QPushButton::clicked, this, [this] {
        QString err;
        if (!m_ai->modelManager()->startDownload(selectedModelId(), &err)) QMessageBox::warning(this, tr("Téléchargement"), err);
    });
    connect(m_btnCancelDownload, &QPushButton::clicked, m_ai->modelManager(), &ModelManager::cancelDownload);
    connect(m_btnRemove, &QPushButton::clicked, this, [this] {
        const int row = m_models->currentRow();
        if (row < 0) return;
        const QString path = m_models->item(row, 0)->data(Qt::UserRole + 1).toString();
        for (const auto& m : m_ai->modelManager()->scanInstalled())
        {
            if (m.path != path) continue;
            if (QMessageBox::question(this, tr("Supprimer"), tr("Supprimer %1 ?").arg(m.fileName)) != QMessageBox::Yes) return;
            if (m_ai->resolvedModelPath() == m.path) m_ai->stopLocal();
            QString err;
            if (!m_ai->modelManager()->removeModel(m, &err)) QMessageBox::warning(this, tr("Supprimer"), err);
        }
        refreshModels();
    });
    connect(m_btnDefault, &QPushButton::clicked, this, [this] {
        const int row = m_models->currentRow();
        if (row < 0) return;
        const QString path = m_models->item(row, 0)->data(Qt::UserRole + 1).toString();
        if (path.isEmpty()) return;
        m_ai->stopLocal();
        m_ai->settings().modelPath = path;
        m_ai->settings().modelId = selectedModelId();
        m_ai->settings().contextSize = 0; // réévalué pour ce modèle
        m_ai->settings().deviceId.clear();
        m_ai->saveSettings();
        refreshModels();
    });
    connect(btnTest, &QPushButton::clicked, this, [this] {
        m_tabs->setCurrentIndex(DiagnosticsPage);
        m_btnDiag->click();
    });
    connect(m_models, &QTableWidget::itemSelectionChanged, this, [this] {
        const int row = m_models->currentRow();
        const bool installed = row >= 0 && !m_models->item(row, 0)->data(Qt::UserRole + 1).toString().isEmpty();
        const bool inRegistry = row >= 0 && !m_models->item(row, 0)->data(Qt::UserRole).toString().isEmpty();
        m_btnDownload->setEnabled(row >= 0 && !installed && inRegistry && !m_ai->modelManager()->isDownloading());
        m_btnRemove->setEnabled(installed && m_models->item(row, 5)->text() == "TSA");
        m_btnDefault->setEnabled(installed);
    });
    return page;
}

QString AIRuntimeDialog::selectedModelId() const
{
    const int row = m_models->currentRow();
    return row >= 0 ? m_models->item(row, 0)->data(Qt::UserRole).toString() : QString();
}

void AIRuntimeDialog::refreshModels()
{
    const auto installed = m_ai->modelManager()->scanInstalled();
    const QString current = m_ai->resolvedModelPath();
    m_models->setRowCount(0);
    auto addRow = [&](const QString& id, const QString& name, const QString& quant, double sizeGB, const QString& state,
                      const QString& fit, const QString& source, const QString& path) {
        const int r = m_models->rowCount();
        m_models->insertRow(r);
        auto* item = new QTableWidgetItem(name);
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, path);
        if (!path.isEmpty() && path == current)
        {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        m_models->setItem(r, 0, item);
        m_models->setItem(r, 1, new QTableWidgetItem(quant));
        m_models->setItem(r, 2, new QTableWidgetItem(gb(sizeGB)));
        m_models->setItem(r, 3, new QTableWidgetItem(state));
        m_models->setItem(r, 4, new QTableWidgetItem(fit));
        m_models->setItem(r, 5, new QTableWidgetItem(source));
    };
    for (const auto& spec : m_ai->registry().models())
    {
        const InstalledModel* inst = m_ai->modelManager()->findInstalled(installed, spec.id);
        QString fit = tr("—");
        if (m_ai->hardwareProbed())
        {
            if (auto e = ModelSelector::evaluate(m_ai->hardware(), spec))
                fit = tr("%1 · ≈%2 j/s").arg(executionModeLabel(e->mode)).arg(e->estimatedTokensPerSec, 0, 'f', 0);
            else
                fit = tr("Trop lourd");
        }
        QString state = inst ? (inst->path == current ? tr("● Par défaut") : tr("Installé")) : tr("Non installé");
        if (m_ai->modelManager()->downloadingModelId() == spec.id) state = tr("Téléchargement…");
        addRow(spec.id, spec.displayName, spec.quantization, spec.fileSizeGB(), state, fit, inst ? inst->source : QStringLiteral("Hugging Face"),
               inst ? inst->path : QString());
    }
    for (const auto& m : installed)
    {
        if (m.spec && m.sizeMatchesRegistry) continue;
        addRow(QString(), m.fileName, tr("?"), m.sizeBytes / 1073741824.0, m.path == current ? tr("● Par défaut") : tr("Installé (hors registre)"),
               tr("besoins inconnus"), m.source, m.path);
    }
}

// -----------------------------------------------------------------------------
// Mode et confidentialité
// -----------------------------------------------------------------------------

QWidget* AIRuntimeDialog::buildPrivacyPage()
{
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    const auto& s = m_ai->settings();

    auto* modeBox = new QGroupBox(tr("Mode IA"), page);
    auto* ml = new QVBoxLayout(modeBox);
    m_modeGroup = new QButtonGroup(this);
    const QList<QPair<AIMode, QString>> modes = {
        { AIMode::Local, tr("LOCAL — tout reste sur ce poste (fonctionne sans Internet)") },
        { AIMode::Auto, tr("AUTO — local d'abord ; Cloud proposé pour les tâches lourdes, avec votre accord") },
        { AIMode::Cloud, tr("CLOUD — fournisseur distant configuré ci-dessous") } };
    for (const auto& [mode, label] : modes)
    {
        auto* rb = new QRadioButton(label, modeBox);
        rb->setChecked(s.mode == mode);
        m_modeGroup->addButton(rb, static_cast<int>(mode));
        ml->addWidget(rb);
    }
    l->addWidget(modeBox);

    auto* localBox = new QGroupBox(tr("Moteur local"), page);
    auto* lf = new QFormLayout(localBox);
    m_device = new QComboBox(localBox);
    m_device->addItem(tr("Automatique (recommandé)"), QString());
    m_device->addItem(tr("CPU uniquement"), QStringLiteral("none"));
    for (const auto& g : m_ai->hardware().gpus)
        m_device->addItem(QStringLiteral("%1 — %2").arg(g.deviceId, g.name), g.deviceId);
    m_device->setCurrentIndex(std::max(0, m_device->findData(s.deviceId)));
    lf->addRow(tr("Périphérique :"), m_device);
    m_chkAutoStart = new QCheckBox(tr("Démarrer le moteur IA local au lancement de TSA"), localBox);
    m_chkAutoStart->setChecked(s.autoStartLocal);
    lf->addRow(m_chkAutoStart);
    l->addWidget(localBox);

    auto* privacy = new QGroupBox(tr("Confidentialité"), page);
    auto* pl = new QVBoxLayout(privacy);
    m_chkAlwaysAsk = new QCheckBox(tr("Toujours demander avant d'utiliser le Cloud"), privacy);
    m_chkAlwaysAsk->setChecked(s.alwaysAskBeforeCloud);
    m_chkNeverFiles = new QCheckBox(tr("Ne jamais envoyer les fichiers projet (.tsa)"), privacy);
    m_chkNeverFiles->setChecked(true);
    m_chkNeverFiles->setEnabled(false);
    m_chkNeverFiles->setToolTip(tr("Garanti par conception : seules des données structurées choisies par TSA peuvent être envoyées."));
    m_chkAutoAllow = new QCheckBox(tr("Autoriser automatiquement le Cloud (déconseillé)"), privacy);
    m_chkAutoAllow->setChecked(s.autoAllowCloud);
    pl->addWidget(m_chkAlwaysAsk);
    pl->addWidget(m_chkNeverFiles);
    pl->addWidget(m_chkAutoAllow);
    l->addWidget(privacy);

    auto* cloud = new QGroupBox(tr("Fournisseur Cloud (optionnel)"), page);
    auto* cf = new QFormLayout(cloud);
    m_cloudPreset = new QComboBox(cloud);
    m_cloudPreset->addItem(tr("Compatible OpenAI (URL personnalisée)"), "openai-compatible");
    m_cloudPreset->addItem(tr("Google Gemini (point d'accès compatible OpenAI)"), "gemini");
    m_cloudPreset->addItem(tr("Ollama (local, sur ce poste)"), "ollama");
    m_cloudPreset->setCurrentIndex(std::max(0, m_cloudPreset->findData(s.cloud.preset)));
    m_cloudUrl = new QLineEdit(s.cloud.baseUrl, cloud);
    m_cloudUrl->setPlaceholderText("https://…/v1");
    m_cloudModel = new QLineEdit(s.cloud.model, cloud);
    m_cloudModel->setPlaceholderText(tr("nom du modèle chez le fournisseur"));
    m_cloudKey = new QLineEdit(cloud);
    m_cloudKey->setEchoMode(QLineEdit::Password);
    m_cloudKey->setPlaceholderText(s.cloud.hasApiKey ? tr("•••••• (enregistrée, chiffrée) — laisser vide pour conserver") : tr("clé API"));
    cf->addRow(tr("Fournisseur :"), m_cloudPreset);
    cf->addRow(tr("URL de base :"), m_cloudUrl);
    cf->addRow(tr("Modèle :"), m_cloudModel);
    cf->addRow(tr("Clé API :"), m_cloudKey);
    auto* keyNote = new QLabel(SecretStore::isProtected() ? tr("La clé est chiffrée avec votre compte Windows (DPAPI).")
                                                          : tr("Attention : la clé est stockée sans chiffrement sur ce système."), cloud);
    keyNote->setStyleSheet("color: gray;");
    cf->addRow(keyNote);
    l->addWidget(cloud);

    auto* save = new QPushButton(tr("Enregistrer"), page);
    auto* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(save);
    l->addLayout(row);
    l->addStretch();
    connect(save, &QPushButton::clicked, this, &AIRuntimeDialog::savePrivacy);
    return page;
}

void AIRuntimeDialog::savePrivacy()
{
    auto& s = m_ai->settings();
    const QString oldDevice = s.deviceId;
    s.mode = static_cast<AIMode>(m_modeGroup->checkedId());
    s.alwaysAskBeforeCloud = m_chkAlwaysAsk->isChecked();
    s.autoAllowCloud = m_chkAutoAllow->isChecked();
    s.autoStartLocal = m_chkAutoStart->isChecked();
    s.deviceId = m_device->currentData().toString();
    s.cloud.preset = m_cloudPreset->currentData().toString();
    s.cloud.baseUrl = m_cloudUrl->text().trimmed();
    s.cloud.model = m_cloudModel->text().trimmed();
    if (!m_cloudKey->text().isEmpty())
    {
        AISettings::storeApiKey(m_cloudKey->text());
        s.cloud.hasApiKey = true;
        m_cloudKey->clear();
    }
    if (s.alwaysAskBeforeCloud) s.cloudConsentGiven = false;
    m_ai->saveSettings();
    m_ai->reloadCloudProvider();
    if (oldDevice != s.deviceId && m_ai->localServer()->isReady())
    {
        m_ai->stopLocal();
        m_ai->startLocal();
    }
    QMessageBox::information(this, tr("Configuration IA"), tr("Réglages enregistrés."));
}

// -----------------------------------------------------------------------------
// Diagnostic et journal
// -----------------------------------------------------------------------------

QWidget* AIRuntimeDialog::buildDiagnosticsPage()
{
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    m_diag = new QPlainTextEdit(page);
    m_diag->setReadOnly(true);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    m_diag->setFont(mono);
    l->addWidget(m_diag, 1);
    m_btnDiag = new QPushButton(tr("Tester l'IA"), page);
    auto* row = new QHBoxLayout();
    row->addStretch();
    row->addWidget(m_btnDiag);
    l->addLayout(row);
    connect(m_btnDiag, &QPushButton::clicked, this, [this] {
        m_diag->clear();
        m_btnDiag->setEnabled(false);
        m_ai->runDiagnostics();
    });
    return page;
}

QWidget* AIRuntimeDialog::buildLogPage()
{
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    m_log = new QTableWidget(page);
    m_log->setColumnCount(8);
    m_log->setHorizontalHeaderLabels({ tr("Date"), tr("Événement"), tr("Fournisseur"), tr("Modèle"), tr("Backend"),
                                       tr("Latence (ms)"), tr("Jetons"), tr("Détail") });
    m_log->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_log->verticalHeader()->hide();
    l->addWidget(m_log, 1);
    auto* note = new QLabel(tr("Métadonnées uniquement : le contenu des échanges et les données du projet ne sont pas journalisés. Dossier : %1")
                                .arg(AILog::directory()), page);
    note->setWordWrap(true);
    note->setStyleSheet("color: gray;");
    l->addWidget(note);
    return page;
}

void AIRuntimeDialog::refreshLog()
{
    const auto entries = AILog::recent(200);
    m_log->setRowCount(0);
    for (const auto& e : entries)
    {
        const int r = m_log->rowCount();
        m_log->insertRow(r);
        const QStringList cells = { e.timestamp, e.event, e.provider, e.model, e.backend,
                                    e.latencyMs >= 0 ? QString::number(e.latencyMs) : QString(),
                                    e.completionTokens >= 0 ? QString::number(e.completionTokens) : QString(), e.detail };
        for (int c = 0; c < cells.size(); ++c) m_log->setItem(r, c, new QTableWidgetItem(cells[c]));
    }
}

} // namespace TSA::UI
