#include "AIOrchestrator.h"

#include "AILog.h"
#include "../../Analysis/ResultsModel.h"
#include "../../Model/Model.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QCoreApplication>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <QSettings>

namespace TSA::AI
{

namespace
{
constexpr int kMaxToolRounds = 6;
constexpr int kHistoryMessages = 8;

QString jsonCompact(const QJsonObject& o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
} // namespace

AIOrchestrator::AIOrchestrator(QObject* parent)
    : QObject(parent)
    , m_registry(ModelRegistry::loadDefault())
    , m_settings(AISettings::load())
{
    qRegisterMetaType<TSA::AI::ChatResult>("TSA::AI::ChatResult");
    m_modelManager = new ModelManager(&m_registry, this);
    m_server = new LocalLlamaServer(this);

    OpenAICompatibleConfig local;
    local.providerId = "local-llama";
    local.displayName = "TSA Local";
    local.locality = ProviderLocality::Local;
    local.llamaCppExtensions = true;
    local.model = "tsa-local";
    m_localProvider = new OpenAICompatibleProvider(local, this);
    m_localProvider->setAvailable(false);

    m_knowledge.indexDirectories(EngineeringKnowledgeBase::defaultDirectories());
    m_tools = std::make_unique<AIToolRegistry>([this] { return m_sources ? m_sources() : EngineeringSources{}; }, &m_knowledge);

    for (auto* p : { static_cast<IAIProvider*>(m_localProvider) })
    {
        connect(p, &IAIProvider::contentDelta, this, [this](quint64 id, const QString& t) {
            if (id == m_requestId) emit assistantDelta(t);
        });
        connect(p, &IAIProvider::finished, this, &AIOrchestrator::onProviderFinished);
        connect(p, &IAIProvider::failed, this, &AIOrchestrator::onProviderFailed);
    }

    connect(m_server, &LocalLlamaServer::stateChanged, this, [this](LocalLlamaServer::State s) {
        configureLocalProvider(); // URL, contexte et backend du serveur effectivement démarré
        m_localProvider->setAvailable(s == LocalLlamaServer::State::Ready);
        if (m_calibrating)
        {
            if (s == LocalLlamaServer::State::Ready)
            {
                ChatRequest req;
                req.messages = { ChatMessage::user(QStringLiteral("Décris en quatre phrases le rôle d'une poutre continue en acier dans un bâtiment.")) };
                req.maxTokens = 64;
                req.temperature = 0.0;
                m_calibRequest = m_localProvider->chat(req);
                if (m_calibRequest == 0) calibrationRecord(0.0, QStringLiteral("requête refusée"));
            }
            else if (s == LocalLlamaServer::State::Failed)
                calibrationRecord(0.0, m_server->lastError());
            emit statusChanged();
            return;
        }
        AILogEntry e;
        e.event = "server";
        e.provider = "local-llama";
        e.model = QFileInfo(m_server->config().modelPath).fileName();
        e.backend = m_server->config().deviceId.isEmpty() ? "CPU" : m_server->config().deviceId;
        e.detail = s == LocalLlamaServer::State::Failed ? m_server->lastError()
                 : QString::fromLatin1(QMetaEnum::fromType<LocalLlamaServer::State>().valueToKey(static_cast<int>(s)));
        AILog::append(e);

        if (s == LocalLlamaServer::State::Ready && m_pending.active && m_pendingRoute == Route::Local)
        {
            PendingRequest r = m_pending;
            m_pending = {};
            dispatch(r, Route::Local);
        }
        else if (s == LocalLlamaServer::State::Failed && m_pending.active && m_pendingRoute == Route::Local)
        {
            m_pending = {};
            emit assistantFailed(QStringLiteral("Le moteur IA local n'a pas pu démarrer : %1").arg(m_server->lastError()));
        }
        if (m_diagnosticsRunning && (s == LocalLlamaServer::State::Ready || s == LocalLlamaServer::State::Failed))
            continueDiagnostics(s == LocalLlamaServer::State::Ready ? 3 : -1);
        emit statusChanged();
    });
    connect(m_modelManager, &ModelManager::inventoryChanged, this, &AIOrchestrator::statusChanged);

    reloadCloudProvider();
}

AIOrchestrator::~AIOrchestrator()
{
    m_server->stop();
}

void AIOrchestrator::setSourcesProvider(AIToolRegistry::SourcesProvider provider)
{
    m_sources = std::move(provider);
}

void AIOrchestrator::saveSettings()
{
    m_settings.save();
    emit statusChanged();
}

// -----------------------------------------------------------------------------
// Matériel et configuration
// -----------------------------------------------------------------------------

void AIOrchestrator::probeHardwareAsync()
{
    if (m_probing) return;
    m_probing = true;
    const QString exe = HardwareProfiler::findLlamaServer(m_settings.llamaServerPath);
    QPointer<AIOrchestrator> self(this);
    QThread* worker = QThread::create([self, exe] {
        AIHardwareProfile profile = HardwareProfiler::probe(exe);
        QMetaObject::invokeMethod(qApp, [self, profile] {
            if (!self) return;
            self->m_hardware = profile;
            self->loadCalibration();
            self->m_recommendation = ModelSelector::recommend(profile, self->m_registry);
            self->m_hardwareProbed = true;
            self->m_probing = false;
            self->configureLocalProvider();
            emit self->hardwareReady();
            emit self->statusChanged();
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowPriority);
}

void AIOrchestrator::applyRecommendation()
{
    const auto& r = m_recommendation;
    if (!r.model) return;
    m_settings.modelId = r.model->id;
    const auto installed = m_modelManager->scanInstalled();
    if (const InstalledModel* m = m_modelManager->findInstalled(installed, r.model->id))
        m_settings.modelPath = m->path;
    m_settings.deviceId = r.deviceId.isEmpty() ? QStringLiteral("none") : r.deviceId;
    m_settings.contextSize = r.contextSize;
    m_settings.setupCompleted = true;
    saveSettings();
    configureLocalProvider();
}

QString AIOrchestrator::resolvedModelPath() const
{
    if (!m_settings.modelPath.isEmpty() && QFileInfo::exists(m_settings.modelPath)) return m_settings.modelPath;
    const auto installed = m_modelManager->scanInstalled();
    // 1. Modèle choisi par identifiant, 2. modèle recommandé, 3. meilleur modèle installé qui tient en mémoire.
    for (const QString& id : { m_settings.modelId, m_recommendation.model ? m_recommendation.model->id : QString() })
        if (!id.isEmpty())
            if (const auto* m = m_modelManager->findInstalled(installed, id)) return m->path;
    const InstalledModel* best = nullptr;
    double bestSpeed = 0.0;
    for (const auto& m : installed)
    {
        if (!m.spec || !m.sizeMatchesRegistry) continue;
        const auto eval = ModelSelector::evaluate(m_hardware, *m.spec);
        if (!eval || eval->estimatedTokensPerSec < 3.0) continue;
        if (!best || m.spec->tierRank() > best->spec->tierRank()
            || (m.spec->tierRank() == best->spec->tierRank() && eval->estimatedTokensPerSec > bestSpeed))
        {
            best = &m;
            bestSpeed = eval->estimatedTokensPerSec;
        }
    }
    return best ? best->path : QString();
}

void AIOrchestrator::configureLocalProvider()
{
    auto cfg = m_localProvider->config();
    if (m_server->isReady()) cfg.baseUrl = m_server->baseUrl();
    const QString path = m_server->isReady() ? m_server->config().modelPath : resolvedModelPath();
    const ModelSpec* spec = m_registry.findByFileName(QFileInfo(path).fileName());
    cfg.contextTokens = m_server->isReady() ? m_server->config().contextSize
                                            : (m_settings.contextSize > 0 ? m_settings.contextSize : m_recommendation.contextSize);
    cfg.toolCalling = !spec || spec->toolCalling;
    QString device = m_server->isReady() ? m_server->config().deviceId : m_settings.deviceId;
    if (device == "none") device.clear();
    QString gpuName;
    for (const auto& g : m_hardware.gpus)
        if (g.deviceId == device) gpuName = g.name;
    cfg.backendLabel = device.isEmpty() ? QStringLiteral("CPU") : QStringLiteral("%1 · %2").arg(device.left(device.size() - 1), gpuName);
    cfg.displayName = spec ? spec->label() : QFileInfo(path).completeBaseName();
    m_localProvider->setConfig(cfg);
}

bool AIOrchestrator::cloudConfigured() const
{
    return m_cloudProvider && m_cloudProvider->config().baseUrl.isValid() && !m_cloudProvider->config().baseUrl.isEmpty();
}

void AIOrchestrator::reloadCloudProvider()
{
    if (m_cloudProvider)
    {
        m_cloudProvider->deleteLater();
        m_cloudProvider = nullptr;
    }
    const auto& c = m_settings.cloud;
    QString base = c.baseUrl;
    if (base.isEmpty() && c.preset == "gemini") base = QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai");
    if (base.isEmpty() && c.preset == "ollama") base = QStringLiteral("http://127.0.0.1:11434/v1");
    if (base.isEmpty() || c.model.isEmpty()) { emit statusChanged(); return; }

    OpenAICompatibleConfig cfg;
    cfg.providerId = c.preset;
    cfg.displayName = c.preset == "ollama" ? QStringLiteral("Ollama (local)") : QStringLiteral("Cloud · %1").arg(c.model);
    cfg.baseUrl = QUrl(base);
    cfg.model = c.model;
    cfg.apiKey = AISettings::loadApiKey();
    // Ollama tourne sur la machine : traité comme local (aucune donnée ne sort du poste).
    cfg.locality = c.preset == "ollama" ? ProviderLocality::Local : ProviderLocality::Cloud;
    cfg.backendLabel = c.preset == "ollama" ? QStringLiteral("Ollama") : c.preset;
    cfg.contextTokens = 32768;
    m_cloudProvider = new OpenAICompatibleProvider(cfg, this);
    connect(m_cloudProvider, &IAIProvider::contentDelta, this, [this](quint64 id, const QString& t) {
        if (id == m_requestId) emit assistantDelta(t);
    });
    connect(m_cloudProvider, &IAIProvider::finished, this, &AIOrchestrator::onProviderFinished);
    connect(m_cloudProvider, &IAIProvider::failed, this, &AIOrchestrator::onProviderFailed);
    emit statusChanged();
}

// -----------------------------------------------------------------------------
// Moteur local
// -----------------------------------------------------------------------------

void AIOrchestrator::startLocal()
{
    if (m_server->state() == LocalLlamaServer::State::Starting || m_server->isReady()) return;
    LlamaServerConfig cfg;
    cfg.executable = HardwareProfiler::findLlamaServer(m_settings.llamaServerPath);
    cfg.modelPath = resolvedModelPath();
    const ModelSpec* spec = m_registry.findByFileName(QFileInfo(cfg.modelPath).fileName());

    QString device = m_settings.deviceId;
    if (device.isEmpty()) device = m_recommendation.deviceId;
    // Le modèle effectivement chargé peut différer de la recommandation : réévaluation.
    if (spec && m_hardwareProbed && (m_settings.deviceId.isEmpty() || m_settings.contextSize <= 0))
    {
        if (auto eval = ModelSelector::evaluate(m_hardware, *spec))
        {
            if (m_settings.deviceId.isEmpty()) device = eval->deviceId;
            cfg.contextSize = eval->contextSize;
        }
    }
    if (device == "none") device.clear();
    cfg.deviceId = device;
    if (m_settings.contextSize > 0) cfg.contextSize = m_settings.contextSize;
    if (cfg.contextSize <= 0) cfg.contextSize = spec ? spec->contextDefault : 8192;
    cfg.threads = m_hardware.physicalCores > 0 ? m_hardware.physicalCores : 0;
    m_server->start(cfg);
    configureLocalProvider();
    emit statusChanged();
}

void AIOrchestrator::stopLocal()
{
    if (m_busy && m_route == Route::Local) cancel();
    m_server->stop();
    emit statusChanged();
}

RuntimeState AIOrchestrator::state() const
{
    if (m_busy) return RuntimeState::Busy;
    switch (m_server->state())
    {
    case LocalLlamaServer::State::Ready: return RuntimeState::Ready;
    case LocalLlamaServer::State::Starting: return RuntimeState::Starting;
    case LocalLlamaServer::State::Failed: return RuntimeState::Error;
    case LocalLlamaServer::State::Stopped: break;
    }
    if (m_settings.mode == AIMode::Cloud && cloudConfigured()) return RuntimeState::Ready;
    const bool engine = !HardwareProfiler::findLlamaServer(m_settings.llamaServerPath).isEmpty();
    if (!engine || resolvedModelPath().isEmpty()) return cloudConfigured() ? RuntimeState::Stopped : RuntimeState::NotConfigured;
    return RuntimeState::Stopped;
}

QString AIOrchestrator::runtimeLabel() const
{
    if (m_settings.mode == AIMode::Cloud)
        return cloudConfigured() && m_cloudProvider->locality() == ProviderLocality::Local ? QStringLiteral("LOCAL (Ollama)") : QStringLiteral("CLOUD");
    if (m_server->state() == LocalLlamaServer::State::Stopped || m_server->state() == LocalLlamaServer::State::Failed)
    {
        if (resolvedModelPath().isEmpty()) return QStringLiteral("IA non configurée");
        return QStringLiteral("LOCAL (arrêtée)");
    }
    const QString dev = m_server->config().deviceId;
    if (dev.isEmpty() || m_server->config().gpuLayers == 0) return QStringLiteral("LOCAL CPU");
    // Répartition automatique (--fit) : hybride si le modèle chargé ne tient pas entièrement en VRAM.
    if (const ModelSpec* spec = m_registry.findByFileName(QFileInfo(m_server->config().modelPath).fileName()); spec && m_hardwareProbed)
        if (const auto eval = ModelSelector::evaluate(m_hardware, *spec); eval && eval->mode == ExecutionMode::LocalHybrid)
            return QStringLiteral("LOCAL CPU+GPU");
    return QStringLiteral("LOCAL GPU");
}

QString AIOrchestrator::runtimeDetails() const
{
    const IAIProvider* p = m_settings.mode == AIMode::Cloud ? static_cast<IAIProvider*>(m_cloudProvider)
                                                            : static_cast<IAIProvider*>(m_localProvider);
    if (!p) return QStringLiteral("Aucun fournisseur configuré");
    const auto caps = p->capabilities();
    QString d = p->displayName();
    if (!caps.backendLabel.isEmpty()) d += QStringLiteral(" · %1").arg(caps.backendLabel);
    if (caps.contextTokens > 0) d += QStringLiteral(" · ctx %1").arg(caps.contextTokens);
    return d;
}

// -----------------------------------------------------------------------------
// Conversation
// -----------------------------------------------------------------------------

QString AIOrchestrator::systemPrompt()
{
    return QStringLiteral(
        "Tu es l'assistant de co-ingénierie intégré à TSA (Tsaraloha Structural Analysis), logiciel de modélisation et "
        "d'analyse de structures (calcul OpenSees). Tu aides un ingénieur structure ; tu n'es pas une autorité.\n"
        "Règles impératives :\n"
        "1. N'utilise que les données fournies par TSA (bloc CONTEXTE TSA et résultats d'outils). Si une donnée manque, "
        "dis-le ou appelle un outil. N'invente jamais une valeur, une section, une charge, un résultat ou un identifiant.\n"
        "2. Étiquette les informations : [FAIT] donnée du modèle, [RÉSULTAT] valeur issue du calcul TSA, [HYPOTHÈSE], "
        "[RECOMMANDATION], [AVERTISSEMENT], [INCERTITUDE].\n"
        "3. Cite les objets TSA (B12, C3, N5, cas de charge) et donne toujours les unités.\n"
        "4. Tu ne peux rien modifier toi-même. Pour proposer un changement, appelle un outil propose_* ; l'ingénieur "
        "décide. Ne prétends jamais qu'une modification a été appliquée.\n"
        "5. Normes et règles de calcul : ne cite une norme que si search_knowledge renvoie une source, en la citant. "
        "Sinon indique que la vérification normative reste à faire par l'ingénieur.\n"
        "6. Si les résultats sont absents ou obsolètes (upToDate=false), signale-le avant toute interprétation.\n"
        "7. Réponds en français, de façon concise et structurée (titres courts, listes). Termine par « Confiance : "
        "élevée / moyenne / faible » avec une justification d'une ligne.");
}

QString AIOrchestrator::deterministicAnalysis(const EngineeringSources& src)
{
    if (!src.model) return QStringLiteral("Aucun modèle ouvert.");
    const auto info = EngineeringContextBuilder::projectInfo(src);
    const auto counts = info["counts"].toObject();
    QString t = QStringLiteral("**Modèle** : %1 nœuds · %2 poutres · %3 poteaux · %4 dalles · %5 voiles · %6 fondations · %7 treillis · %8 câbles\n")
                    .arg(counts["nodes"].toInt()).arg(counts["beams"].toInt()).arg(counts["columns"].toInt())
                    .arg(counts["slabs"].toInt()).arg(counts["walls"].toInt()).arg(counts["foundations"].toInt())
                    .arg(counts["trussMembers"].toInt()).arg(counts["cables"].toInt());
    t += QStringLiteral("**Appuis** : %1 nœud(s) · **Charges** : %2 cas, %3 combinaisons, %4 nodales, %5 sur barres\n")
             .arg(counts["supportedNodes"].toInt()).arg(counts["loadCases"].toInt()).arg(counts["loadCombinations"].toInt())
             .arg(counts["nodalLoads"].toInt()).arg(counts["memberLoads"].toInt());
    QStringList secs, mats;
    for (const auto& v : EngineeringContextBuilder::sections(*src.model))
        secs << QStringLiteral("%1 (×%2)").arg(v.toObject()["name"].toString()).arg(v.toObject()["usedByMembers"].toInt());
    for (const auto& v : EngineeringContextBuilder::materials(*src.model))
        mats << QStringLiteral("%1 (×%2)").arg(v.toObject()["name"].toString()).arg(v.toObject()["usedByMembers"].toInt());
    if (!secs.isEmpty()) t += QStringLiteral("**Sections** : %1\n").arg(secs.join(", "));
    if (!mats.isEmpty()) t += QStringLiteral("**Matériaux** : %1\n").arg(mats.join(", "));
    const auto res = EngineeringContextBuilder::resultsSummary(src);
    if (res["available"].toBool())
    {
        const QString u = res["units"].toObject()["length"].toString();
        const QString um = res["units"].toObject()["moment"].toString();
        t += QStringLiteral("**Résultats** (%1%2) : déplacement max %3 %4 (%5) · moment max %6 %7 (%8)\n")
                 .arg(res["loadCaseOrCombination"].toString(), res["upToDate"].toBool() ? QString() : QStringLiteral(", OBSOLÈTES"))
                 .arg(res["maxDisplacement"].toObject()["value"].toDouble()).arg(u, res["maxDisplacement"].toObject()["node"].toString())
                 .arg(res["maxBendingMoment"].toObject()["value"].toDouble()).arg(um, res["maxBendingMoment"].toObject()["element"].toString());
    }
    else
        t += QStringLiteral("**Résultats** : aucun calcul disponible.\n");
    return t;
}

QString AIOrchestrator::taskInstruction(AITask task, const QString& userText) const
{
    switch (task)
    {
    case AITask::AnalyzeModel:
        return QStringLiteral("Analyse ce modèle structural : système porteur probable, cohérence des sections, matériaux, "
                              "appuis et charges, résultats s'ils existent, points faibles et points à vérifier en priorité. %1").arg(userText);
    case AITask::CheckStructure:
        return QStringLiteral("Les contrôles automatiques de TSA ont produit le rapport « checks » du contexte. Explique chaque "
                              "problème à l'ingénieur, classe-les par gravité, indique la cause probable et l'action corrective "
                              "dans TSA. N'ajoute pas de problème qui ne figure pas dans le rapport, sauf en [HYPOTHÈSE]. %1").arg(userText);
    case AITask::ExplainSelection:
        return userText.isEmpty()
                   ? QStringLiteral("Explique le comportement de l'objet sélectionné (bloc « selection ») : rôle structural, charges, "
                                    "appuis, efforts et déplacements, et pourquoi ces valeurs sont élevées ou faibles.")
                   : QStringLiteral("À propos de l'objet sélectionné (bloc « selection ») : %1").arg(userText);
    case AITask::Chat:
        return userText;
    }
    return userText;
}

QString AIOrchestrator::buildContextBlock(AITask task) const
{
    EngineeringSources src = m_sources ? m_sources() : EngineeringSources{};
    if (!src.model) return QStringLiteral("CONTEXTE TSA : aucun modèle ouvert.");
    ContextOptions opt;
    opt.includeCheckReport = (task == AITask::CheckStructure || task == AITask::AnalyzeModel);
    if (task != AITask::ExplainSelection) src.selection.clear();
    QString json = jsonCompact(EngineeringContextBuilder::summary(src, opt));

    // Budget de contexte : ≈ 3 caractères par jeton, 45 % du contexte pour les données.
    const IAIProvider* p = activeProvider();
    const int ctxTokens = p ? std::max(4096, p->capabilities().contextTokens) : 8192;
    const int budget = static_cast<int>(ctxTokens * 3 * 0.30); // ≈30 % du contexte pour les données
    if (json.size() > budget)
    {
        opt.maxListedMembers = 0;
        opt.maxListedNodes = 10;
        json = jsonCompact(EngineeringContextBuilder::summary(src, opt));
        if (json.size() > budget) json = json.left(budget) + QStringLiteral("…[tronqué : utiliser les outils pour le détail]");
    }
    return QStringLiteral("CONTEXTE TSA (JSON, lecture seule) :\n") + json;
}

IAIProvider* AIOrchestrator::activeProvider() const
{
    switch (m_route)
    {
    case Route::Local: return m_localProvider;
    case Route::Cloud: return m_cloudProvider;
    case Route::None: break;
    }
    return nullptr;
}

AIOrchestrator::Route AIOrchestrator::chooseRoute(AITask task, QString* reason, bool* needsConsent) const
{
    *needsConsent = false;
    const bool localPossible = !HardwareProfiler::findLlamaServer(m_settings.llamaServerPath).isEmpty() && !resolvedModelPath().isEmpty();
    const bool cloudIsRemote = cloudConfigured() && m_cloudProvider->locality() == ProviderLocality::Cloud;
    auto consentNeeded = [&] {
        if (!cloudIsRemote) return false;
        if (m_settings.autoAllowCloud) return false;
        return m_settings.alwaysAskBeforeCloud || !m_settings.cloudConsentGiven;
    };

    switch (m_settings.mode)
    {
    case AIMode::Local:
        if (localPossible || m_server->isReady()) return Route::Local;
        *reason = QStringLiteral("Aucun modèle local installé. Ouvrez « IA › Configuration » pour installer la configuration recommandée.");
        return Route::None;
    case AIMode::Cloud:
        if (!cloudConfigured()) { *reason = QStringLiteral("Aucun fournisseur Cloud configuré."); return Route::None; }
        *needsConsent = consentNeeded();
        *reason = QStringLiteral("Mode CLOUD sélectionné.");
        return Route::Cloud;
    case AIMode::Auto:
    {
        const ModelSpec* spec = m_registry.findByFileName(QFileInfo(m_server->isReady() ? m_server->config().modelPath : resolvedModelPath()).fileName());
        const bool complex = task == AITask::AnalyzeModel || task == AITask::ExplainSelection;
        const bool localWeak = !spec || spec->tierRank() < 2; // en-dessous de « standard »
        if ((localPossible || m_server->isReady()) && !(complex && localWeak && cloudConfigured())) return Route::Local;
        if (cloudConfigured())
        {
            *needsConsent = consentNeeded();
            *reason = (localPossible || m_server->isReady())
                          ? QStringLiteral("Cette tâche nécessite davantage de puissance que le modèle local installé.")
                          : QStringLiteral("Aucun modèle local n'est disponible.");
            return Route::Cloud;
        }
        if (localPossible || m_server->isReady()) return Route::Local;
        *reason = QStringLiteral("Aucun modèle local ni fournisseur Cloud configuré.");
        return Route::None;
    }
    }
    return Route::None;
}

void AIOrchestrator::submit(AITask task, const QString& userText)
{
    if (m_busy || m_pending.active) return;

    const EngineeringSources src = m_sources ? m_sources() : EngineeringSources{};
    if (task == AITask::ExplainSelection && src.selection.empty())
    {
        emit assistantFailed(QStringLiteral("Sélectionnez d'abord un élément (poutre, poteau, nœud…) dans le viewport ou l'arbre."));
        return;
    }
    // Partie déterministe : disponible même sans aucun modèle de langage.
    if (task == AITask::CheckStructure && src.model)
    {
        CheckOptions opt;
        opt.resultsUpToDate = src.resultsUpToDate;
        emit deterministicReport(QStringLiteral("Contrôles automatiques TSA"), StructuralChecker::check(*src.model, src.results, opt).toText());
    }
    else if (task == AITask::AnalyzeModel)
        emit deterministicReport(QStringLiteral("Synthèse du modèle (données TSA)"), deterministicAnalysis(src));

    QString reason;
    bool needsConsent = false;
    const Route route = chooseRoute(task, &reason, &needsConsent);
    PendingRequest req{ task, userText, true };
    if (route == Route::None)
    {
        if (task == AITask::Chat || task == AITask::ExplainSelection) emit assistantFailed(reason);
        return;
    }
    if (route == Route::Cloud && needsConsent)
    {
        m_pending = req;
        m_pendingRoute = Route::Cloud;
        emit cloudConsentRequested(reason);
        return;
    }
    if (route == Route::Local && !m_server->isReady())
    {
        m_pending = req;
        m_pendingRoute = Route::Local;
        emit toolActivity(QStringLiteral("Démarrage du moteur IA local (chargement du modèle)…"));
        startLocal();
        return;
    }
    dispatch(req, route);
}

void AIOrchestrator::answerCloudConsent(bool allowed, bool remember)
{
    if (!m_pending.active || m_pendingRoute != Route::Cloud) return;
    PendingRequest r = m_pending;
    m_pending = {};
    AILogEntry e;
    e.event = "consent";
    e.provider = m_cloudProvider ? m_cloudProvider->providerId() : QString();
    e.detail = allowed ? QStringLiteral("accordé") : QStringLiteral("refusé");
    AILog::append(e);
    if (allowed)
    {
        if (remember)
        {
            m_settings.cloudConsentGiven = true;
            m_settings.alwaysAskBeforeCloud = false;
            m_settings.save();
        }
        dispatch(r, Route::Cloud);
        return;
    }
    // Refus : repli sur le local si possible, sinon abandon. Rien n'est envoyé.
    if (!HardwareProfiler::findLlamaServer(m_settings.llamaServerPath).isEmpty() && !resolvedModelPath().isEmpty())
    {
        emit toolActivity(QStringLiteral("Cloud refusé : traitement local."));
        if (m_server->isReady()) dispatch(r, Route::Local);
        else { m_pending = r; m_pendingRoute = Route::Local; startLocal(); }
    }
    else
        emit assistantFailed(QStringLiteral("Cloud refusé et aucun modèle local disponible : aucune donnée n'a été envoyée."));
}

void AIOrchestrator::dispatch(const PendingRequest& request, Route route)
{
    m_route = route;
    m_currentTask = request.task;
    m_toolRounds = 0;
    m_busy = true;
    m_turnStartMs = QDateTime::currentMSecsSinceEpoch();
    m_turn.clear();
    m_turn << ChatMessage::system(systemPrompt());
    m_turn << ChatMessage::system(buildContextBlock(request.task));
    const int from = std::max(0, static_cast<int>(m_history.size()) - kHistoryMessages);
    for (int i = from; i < m_history.size(); ++i) m_turn << m_history[i];
    const QString instruction = taskInstruction(request.task, request.userText);
    m_turn << ChatMessage::user(instruction);
    m_history << ChatMessage::user(request.task == AITask::Chat ? request.userText : instruction);

    IAIProvider* p = activeProvider();
    emit assistantStarted(p ? p->displayName() : QString());
    emit statusChanged();
    sendRound();
}

void AIOrchestrator::sendRound()
{
    IAIProvider* p = activeProvider();
    if (!p || !p->isAvailable())
    {
        m_busy = false;
        emit assistantFailed(QStringLiteral("Fournisseur IA indisponible."));
        emit statusChanged();
        return;
    }
    ChatRequest req;
    req.messages = m_turn;
    if (p->capabilities().toolCalling) req.tools = m_tools->toolDefinitions();
    req.temperature = 0.2;
    const int ctx = p->capabilities().contextTokens > 0 ? p->capabilities().contextTokens : 8192;
    req.maxTokens = std::clamp(ctx / 5, 512, 2048);
    m_requestId = p->chat(req);
    if (m_requestId == 0)
    {
        m_busy = false;
        emit assistantFailed(QStringLiteral("Requête refusée par le fournisseur."));
        emit statusChanged();
    }
}

void AIOrchestrator::onProviderFinished(quint64 id, const ChatResult& result)
{
    if (m_calibrating && id == m_calibRequest)
    {
        calibrationRecord(result.tokensPerSecond(), QString());
        return;
    }
    if (m_diagnosticsRunning && id == m_diagnosticsRequest)
    {
        auto line = [this](const QString& l) { m_diagnosticsLines << l; emit diagnosticsProgress(l); };
        line(QStringLiteral("Inférence ........ OK (%1 jetons, %2 ms)").arg(result.completionTokens).arg(result.latencyMs));
        const double measured = result.tokensPerSecond();
        double estimated = 0.0;
        if (const ModelSpec* spec = m_registry.findByFileName(QFileInfo(m_server->config().modelPath).fileName()))
            if (const auto eval = ModelSelector::evaluate(m_hardware, *spec)) estimated = eval->estimatedTokensPerSec;
        line(estimated > 0 ? QStringLiteral("Performance ...... %1 jetons/s mesurés (≈%2 estimés)").arg(measured, 0, 'f', 1).arg(estimated, 0, 'f', 0)
                           : QStringLiteral("Performance ...... %1 jetons/s mesurés").arg(measured, 0, 'f', 1));
        if (estimated > 0 && measured > 0 && measured < estimated / 3.0)
            line(QStringLiteral("  ⚠ Vitesse très inférieure à l'estimation. Cause la plus fréquente : mémoire libre insuffisante "
                                "(%1 Go libres) — fermer les applications lourdes, ou choisir un modèle plus léger (onglet Modèles).")
                     .arg(m_hardware.ramAvailableGB, 0, 'f', 1));
        AILogEntry e;
        e.event = "benchmark";
        e.provider = "local-llama";
        e.model = m_localProvider->displayName();
        e.backend = m_localProvider->capabilities().backendLabel;
        e.latencyMs = result.latencyMs;
        e.completionTokens = result.completionTokens;
        e.tokensPerSecond = result.tokensPerSecond();
        AILog::append(e);
        continueDiagnostics(4);
        return;
    }
    if (id != m_requestId || !m_busy) return;

    if (!result.toolCalls.empty() && m_toolRounds < kMaxToolRounds)
    {
        ++m_toolRounds;
        ChatMessage assistant = ChatMessage::assistant(result.content);
        assistant.toolCalls = result.toolCalls;
        m_turn << assistant;
        for (const auto& call : result.toolCalls)
        {
            emit toolActivity(QStringLiteral("Outil TSA : %1").arg(call.name));
            ToolOutcome out = m_tools->execute(call.name, call.argumentsJson);
            if (out.proposal)
            {
                m_proposals.push_back(*out.proposal);
                emit proposalCreated(*out.proposal);
            }
            QString payload = jsonCompact(out.toModelJson());
            if (payload.size() > 6000) // garde le contexte dans le budget du modèle local
                payload = payload.left(6000) + QStringLiteral("…[tronqué : affiner la requête (limit, type, id)]");
            m_turn << ChatMessage::tool(call.id, payload);
        }
        sendRound();
        return;
    }
    finishTurn(result.content, result);
}

void AIOrchestrator::finishTurn(const QString& text, const ChatResult& result)
{
    m_busy = false;
    QString answer = text;
    // Modèles « thinking » sans extraction côté serveur : le raisonnement n'est pas une réponse.
    static const QRegularExpression think(QStringLiteral(R"(<think>[\s\S]*?</think>)"));
    answer.remove(think);
    answer = answer.trimmed();
    if (answer.isEmpty())
        answer = QStringLiteral("(Le modèle n'a pas produit de réponse textuelle.)");
    m_history << ChatMessage::assistant(answer);

    IAIProvider* p = activeProvider();
    const QString meta = QStringLiteral("%1 · %2 s · %3 jetons%4")
                             .arg(p ? p->displayName() : QString())
                             .arg((QDateTime::currentMSecsSinceEpoch() - m_turnStartMs) / 1000.0, 0, 'f', 1)
                             .arg(result.completionTokens)
                             .arg(result.tokensPerSecond() > 0 ? QStringLiteral(" · %1 j/s").arg(result.tokensPerSecond(), 0, 'f', 1) : QString());
    AILogEntry e;
    e.event = "request";
    e.provider = p ? p->providerId() : QString();
    e.model = p ? p->displayName() : QString();
    e.backend = p ? p->capabilities().backendLabel : QString();
    e.latencyMs = QDateTime::currentMSecsSinceEpoch() - m_turnStartMs;
    e.promptTokens = result.promptTokens;
    e.completionTokens = result.completionTokens;
    e.tokensPerSecond = result.tokensPerSecond();
    e.detail = QStringLiteral("tâche %1, %2 tour(s) d'outils").arg(static_cast<int>(m_currentTask)).arg(m_toolRounds);
    AILog::append(e);

    emit assistantFinished(answer, meta);
    emit statusChanged();
}

void AIOrchestrator::onProviderFailed(quint64 id, const QString& error)
{
    if (m_calibrating && id == m_calibRequest)
    {
        calibrationRecord(0.0, error);
        return;
    }
    if (m_diagnosticsRunning && id == m_diagnosticsRequest)
    {
        m_diagnosticsLines << QStringLiteral("Inférence ........ ÉCHEC (%1)").arg(error);
        emit diagnosticsProgress(m_diagnosticsLines.last());
        continueDiagnostics(-1);
        return;
    }
    if (id != m_requestId || !m_busy) return;
    m_busy = false;
    AILogEntry e;
    e.event = "error";
    e.provider = activeProvider() ? activeProvider()->providerId() : QString();
    e.detail = error;
    AILog::append(e);
    emit assistantFailed(error);
    emit statusChanged();
}

void AIOrchestrator::cancel()
{
    if (m_pending.active)
    {
        m_pending = {};
        emit assistantFailed(QStringLiteral("Demande annulée."));
    }
    if (m_busy)
    {
        if (IAIProvider* p = activeProvider()) p->cancel(m_requestId);
    }
}

void AIOrchestrator::resetConversation()
{
    cancel();
    m_history.clear();
    m_proposals.clear();
}

// -----------------------------------------------------------------------------
// Propositions
// -----------------------------------------------------------------------------

bool AIOrchestrator::acceptProposal(const QString& proposalId, QString* error)
{
    auto it = std::find_if(m_proposals.begin(), m_proposals.end(), [&](const ActionProposal& p) { return p.id == proposalId; });
    if (it == m_proposals.end())
    {
        if (error) *error = QStringLiteral("Proposition expirée.");
        return false;
    }
    const ActionProposal p = *it;
    m_proposals.erase(it);
    if (p.tool == "propose_run_analysis")
    {
        emit proposalResolved(p.id, true, QStringLiteral("Calcul lancé par l'ingénieur."));
        emit runAnalysisRequested();
        return true;
    }
    EngineeringSources src = m_sources ? m_sources() : EngineeringSources{};
    auto* model = const_cast<TSA::Model::Model*>(src.model); // modification explicitement acceptée
    QString err;
    if (!model || !AIToolRegistry::applyProposal(p, *model, &err))
    {
        if (error) *error = err;
        emit proposalResolved(p.id, false, err);
        return false;
    }
    emit proposalResolved(p.id, true, QStringLiteral("Appliqué : %1 (annulable par Ctrl+Z)").arg(p.title));
    emit modelChanged();
    return true;
}

void AIOrchestrator::rejectProposal(const QString& proposalId)
{
    m_proposals.erase(std::remove_if(m_proposals.begin(), m_proposals.end(), [&](const ActionProposal& p) { return p.id == proposalId; }),
                      m_proposals.end());
    emit proposalResolved(proposalId, false, QStringLiteral("Proposition refusée par l'ingénieur."));
}

// -----------------------------------------------------------------------------
// Diagnostic
// -----------------------------------------------------------------------------

void AIOrchestrator::runDiagnostics()
{
    if (m_diagnosticsRunning) return;
    m_diagnosticsRunning = true;
    m_diagnosticsLines.clear();
    continueDiagnostics(0);
}

void AIOrchestrator::continueDiagnostics(int step)
{
    auto line = [&](const QString& l) { m_diagnosticsLines << l; emit diagnosticsProgress(l); };
    auto finish = [&](bool ok) {
        m_diagnosticsRunning = false;
        line(ok ? QStringLiteral("\nÉtat : PRÊT") : QStringLiteral("\nÉtat : NON OPÉRATIONNEL"));
        emit diagnosticsFinished(ok, m_diagnosticsLines.join('\n'));
    };
    if (step < 0) { finish(false); return; }

    if (step == 0)
    {
        if (!m_hardwareProbed)
        {
            line(QStringLiteral("Analyse du matériel…"));
            connect(this, &AIOrchestrator::hardwareReady, this, [this] { continueDiagnostics(1); }, Qt::SingleShotConnection);
            probeHardwareAsync();
            return;
        }
        step = 1;
    }
    if (step == 1)
    {
        const auto& h = m_hardware;
        line(QStringLiteral("CPU .............. OK — %1 (%2 cœurs%3%4)").arg(h.cpuName).arg(h.physicalCores)
                 .arg(h.avx2 ? ", AVX2" : "").arg(h.avx512 ? ", AVX-512" : ""));
        line(QStringLiteral("RAM .............. %1 — %2 Go (%3 Go libres)").arg(h.ramGB >= 8 ? "OK" : "LIMITÉE")
                 .arg(h.ramGB, 0, 'f', 1).arg(h.ramAvailableGB, 0, 'f', 1));
        const GpuDevice* g = h.bestDiscreteGpu();
        line(g ? QStringLiteral("GPU .............. OK — %1 (%2, %3 Go libres)").arg(g->name, g->backend).arg(g->freeGB, 0, 'f', 1)
               : QStringLiteral("GPU .............. aucun GPU dédié utilisable (CPU)"));
        line(QStringLiteral("CUDA ............. %1").arg(h.cudaDriver ? (h.llamaBackends.contains("CUDA") ? "OK" : "pilote présent, moteur sans CUDA") : "non"));
        line(QStringLiteral("Vulkan ........... %1").arg(h.vulkanLoader ? (h.llamaBackends.contains("Vulkan") ? "OK" : "chargeur présent, moteur sans Vulkan") : "non"));
        if (h.llamaServerPath.isEmpty())
        {
            line(QStringLiteral("Moteur ........... ÉCHEC — llama-server introuvable"));
            finish(false);
            return;
        }
        line(QStringLiteral("Moteur ........... OK — llama.cpp %1").arg(h.llamaVersion));
        const QString model = resolvedModelPath();
        if (model.isEmpty())
        {
            line(QStringLiteral("Modèle ........... ÉCHEC — aucun modèle installé"));
            finish(false);
            return;
        }
        line(QStringLiteral("Modèle ........... OK — %1").arg(QFileInfo(model).fileName()));
        step = 2;
    }
    if (step == 2)
    {
        if (!m_server->isReady())
        {
            line(QStringLiteral("Chargement du modèle…"));
            startLocal();
            if (m_server->state() == LocalLlamaServer::State::Failed) { line(m_server->lastError()); finish(false); }
            return; // reprise sur stateChanged
        }
        step = 3;
    }
    if (step == 3)
    {
        if (!m_server->isReady())
        {
            line(QStringLiteral("Serveur .......... ÉCHEC — %1").arg(m_server->lastError()));
            finish(false);
            return;
        }
        line(QStringLiteral("Serveur .......... OK — %1").arg(runtimeDetails()));
        ChatRequest req;
        req.messages = { ChatMessage::user(QStringLiteral("Donne en une phrase la définition du moment fléchissant.")) };
        req.maxTokens = 96;
        req.temperature = 0.0;
        m_diagnosticsRequest = m_localProvider->chat(req);
        if (m_diagnosticsRequest == 0) { line(QStringLiteral("Inférence ........ ÉCHEC")); finish(false); }
        return;
    }
    if (step == 4) finish(true);
}

// -----------------------------------------------------------------------------
// Auto-benchmark
// -----------------------------------------------------------------------------

void AIOrchestrator::loadCalibration()
{
    // Les mesures ne valent que pour le matériel mesuré : clé = périphérique + nom.
    QSettings s;
    const QJsonObject root = QJsonDocument::fromJson(s.value("AI/calibration").toByteArray()).object();
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const QJsonObject o = it.value().toObject();
        const QString device = it.key();
        bool present = device == "CPU" && o["name"].toString() == m_hardware.cpuName;
        for (const auto& g : m_hardware.gpus)
            if (g.deviceId == device && g.name == o["name"].toString()) present = true;
        if (present && o["bandwidthGBs"].toDouble() > 0) m_hardware.measuredBandwidthGBs[device] = o["bandwidthGBs"].toDouble();
    }
}

void AIOrchestrator::saveCalibration() const
{
    QJsonObject root;
    for (const auto& [device, bw] : m_hardware.measuredBandwidthGBs)
    {
        QString name = device == "CPU" ? m_hardware.cpuName : QString();
        for (const auto& g : m_hardware.gpus)
            if (g.deviceId == device) name = g.name;
        root[device] = QJsonObject{ { "name", name }, { "bandwidthGBs", bw },
                                    { "date", QDateTime::currentDateTime().toString(Qt::ISODate) } };
    }
    QSettings s;
    s.setValue("AI/calibration", QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void AIOrchestrator::runCalibration()
{
    if (m_calibrating || m_busy) return;
    auto line = [this](const QString& l) { m_calibLines << l; emit calibrationProgress(l); };
    m_calibLines.clear();
    if (!m_hardwareProbed || m_hardware.llamaServerPath.isEmpty())
    {
        line(QStringLiteral("Moteur d'inférence local introuvable : mesure impossible."));
        emit calibrationFinished(false, m_calibLines.join('\n'));
        return;
    }
    // Plus petit modèle installé du registre : mesure rapide et peu gourmande en mémoire.
    m_calibSpec = nullptr;
    m_calibModelPath.clear();
    for (const auto& m : m_modelManager->scanInstalled())
        if (m.spec && m.sizeMatchesRegistry && (!m_calibSpec || m.spec->fileSizeBytes < m_calibSpec->fileSizeBytes))
        {
            m_calibSpec = m.spec;
            m_calibModelPath = m.path;
        }
    if (!m_calibSpec)
    {
        line(QStringLiteral("Aucun modèle installé : installez d'abord un modèle (le plus léger suffit)."));
        emit calibrationFinished(false, m_calibLines.join('\n'));
        return;
    }
    m_calibRestart = m_server->isReady() || m_server->state() == LocalLlamaServer::State::Starting;
    m_server->stop();
    m_calibQueue = { QStringLiteral("CPU") };
    for (const auto& g : m_hardware.gpus)
        if (!g.integrated) m_calibQueue << g.deviceId;
    m_calibrating = true;
    line(QStringLiteral("Mesure avec %1 sur %2 périphérique(s)…").arg(m_calibSpec->label()).arg(m_calibQueue.size()));
    calibrationNext();
}

void AIOrchestrator::calibrationNext()
{
    if (m_calibQueue.isEmpty())
    {
        m_calibrating = false;
        saveCalibration();
        m_recommendation = ModelSelector::recommend(m_hardware, m_registry);
        auto line = [this](const QString& l) { m_calibLines << l; emit calibrationProgress(l); };
        if (m_recommendation.model)
            line(QStringLiteral("Recommandation mise à jour : %1 · %2%3")
                     .arg(m_recommendation.model->label(), executionModeLabel(m_recommendation.mode),
                          m_recommendation.deviceId.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(m_recommendation.deviceId)));
        AILogEntry e;
        e.event = "benchmark";
        e.detail = m_calibLines.join(" | ");
        AILog::append(e);
        if (m_calibRestart) startLocal();
        emit calibrationFinished(true, m_calibLines.join('\n'));
        emit hardwareReady(); // les vues de configuration se rafraîchissent
        emit statusChanged();
        return;
    }
    m_calibDevice = m_calibQueue.takeFirst();
    LlamaServerConfig cfg;
    cfg.executable = m_hardware.llamaServerPath;
    cfg.modelPath = m_calibModelPath;
    cfg.contextSize = 2048;
    cfg.alias = QStringLiteral("tsa-benchmark");
    cfg.threads = m_hardware.physicalCores;
    cfg.startupTimeoutMs = 120000;
    if (m_calibDevice == "CPU") cfg.gpuLayers = 0;
    else { cfg.deviceId = m_calibDevice; cfg.gpuLayers = 999; } // tout le petit modèle en VRAM
    m_server->start(cfg);
}

void AIOrchestrator::calibrationRecord(double tokensPerSec, const QString& error)
{
    QString name = m_calibDevice;
    for (const auto& g : m_hardware.gpus)
        if (g.deviceId == m_calibDevice) name = QStringLiteral("%1 (%2)").arg(g.name, g.deviceId);
    QString l;
    if (tokensPerSec > 0 && m_calibSpec)
    {
        m_hardware.measuredBandwidthGBs[m_calibDevice] = ModelSelector::bandwidthFromMeasurement(tokensPerSec, *m_calibSpec);
        l = QStringLiteral("%1 : %2 jetons/s mesurés").arg(name).arg(tokensPerSec, 0, 'f', 1);
    }
    else
        l = QStringLiteral("%1 : échec de la mesure (%2)").arg(name, error);
    m_calibLines << l;
    emit calibrationProgress(l);
    m_server->stop();
    QMetaObject::invokeMethod(this, &AIOrchestrator::calibrationNext, Qt::QueuedConnection);
}

} // namespace TSA::AI
