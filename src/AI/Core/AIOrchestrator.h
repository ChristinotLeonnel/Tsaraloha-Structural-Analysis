#pragma once

// Chef d'orchestre de l'IA de co-ingénierie :
//   demande → choix du fournisseur (LOCAL / CLOUD / AUTO + confidentialité) → contexte d'ingénierie
//   → modèle → appels d'outils (liste blanche) → réponse, avec propositions soumises à l'ingénieur.
// Tout est asynchrone (réseau Qt + processus séparé) : l'interface, le viewport et le solveur
// ne sont jamais bloqués par une génération.

#include "AISettings.h"
#include "../Hardware/HardwareProfiler.h"
#include "../Models/ModelManager.h"
#include "../Models/ModelRegistry.h"
#include "../Providers/LocalLlamaServer.h"
#include "../Providers/OpenAICompatibleProvider.h"
#include "../RAG/EngineeringKnowledgeBase.h"
#include "../Tools/AIToolRegistry.h"

#include <QObject>
#include <memory>

namespace TSA::AI
{

enum class AITask { Chat, AnalyzeModel, CheckStructure, ExplainSelection };

enum class RuntimeState
{
    NotConfigured, // aucun moteur ni modèle utilisable
    Stopped,       // prêt à démarrer
    Starting,      // chargement du modèle
    Ready,
    Busy,          // génération en cours
    Error
};

class AIOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit AIOrchestrator(QObject* parent = nullptr);
    ~AIOrchestrator() override;

    void setSourcesProvider(AIToolRegistry::SourcesProvider provider);

    // --- Matériel & configuration ------------------------------------------------------------
    void probeHardwareAsync();
    bool hardwareProbed() const { return m_hardwareProbed; }
    const AIHardwareProfile& hardware() const { return m_hardware; }
    const ModelRecommendation& recommendation() const { return m_recommendation; }
    const ModelRegistry& registry() const { return m_registry; }
    ModelManager* modelManager() const { return m_modelManager; }
    EngineeringKnowledgeBase& knowledge() { return m_knowledge; }
    AISettings& settings() { return m_settings; }
    void saveSettings();
    void reloadCloudProvider();

    /// Applique la configuration recommandée (modèle installé correspondant, périphérique, contexte).
    void applyRecommendation();

    // --- Moteur local ------------------------------------------------------------------------
    void startLocal();
    void stopLocal();
    LocalLlamaServer* localServer() const { return m_server; }
    QString resolvedModelPath() const;      // modèle qui sera chargé
    RuntimeState state() const;
    QString runtimeLabel() const;           // « LOCAL GPU », « LOCAL CPU », « CLOUD »…
    QString runtimeDetails() const;         // modèle · backend · contexte
    bool cloudConfigured() const;

    // --- Conversation ------------------------------------------------------------------------
    void submit(AITask task, const QString& userText = QString());
    void cancel();
    bool isBusy() const { return m_busy; }
    void resetConversation();
    void answerCloudConsent(bool allowed, bool remember);

    // --- Propositions (human-in-the-loop) -----------------------------------------------------
    bool acceptProposal(const QString& proposalId, QString* error);
    void rejectProposal(const QString& proposalId);

    // --- Diagnostic & auto-benchmark ----------------------------------------------------------
    void runDiagnostics();
    /// Mesure réelle de la vitesse de génération sur CPU et sur chaque GPU dédié (petit modèle
    /// installé, ~1 min). Les mesures remplacent les estimations dans la recommandation.
    void runCalibration();
    bool isCalibrating() const { return m_calibrating; }
    bool hasCalibration() const { return !m_hardware.measuredBandwidthGBs.empty(); }

    static QString systemPrompt();
    static QString deterministicAnalysis(const EngineeringSources& src);

signals:
    void hardwareReady();
    void statusChanged();
    void assistantStarted(const QString& providerLabel);
    void assistantDelta(const QString& text);
    void assistantFinished(const QString& fullText, const QString& meta);
    void assistantFailed(const QString& error);
    void deterministicReport(const QString& title, const QString& markdown);
    void toolActivity(const QString& description);
    void proposalCreated(const TSA::AI::ActionProposal& proposal);
    void proposalResolved(const QString& proposalId, bool applied, const QString& message);
    void cloudConsentRequested(const QString& reason);
    void runAnalysisRequested();
    /// Blueprint proposé par l'IA et accepté : l'application l'exécute ou l'ouvre dans l'éditeur.
    void blueprintAccepted(const QString& title, const QString& script);
    void modelChanged(); // modèle appliqué modifié (résultats à invalider : géré par le modèle)
    void diagnosticsProgress(const QString& line);
    void diagnosticsFinished(bool ok, const QString& summary);
    void calibrationProgress(const QString& line);
    void calibrationFinished(bool ok, const QString& summary);

private:
    enum class Route { Local, Cloud, None };
    struct PendingRequest
    {
        AITask task = AITask::Chat;
        QString userText;
        bool active = false;
    };

    Route chooseRoute(AITask task, QString* reason, bool* needsConsent) const;
    void dispatch(const PendingRequest& request, Route route);
    void sendRound();
    void onProviderFinished(quint64 id, const ChatResult& result);
    void onProviderFailed(quint64 id, const QString& error);
    void finishTurn(const QString& text, const ChatResult& result);
    IAIProvider* activeProvider() const;
    QString buildContextBlock(AITask task) const;
    QString taskInstruction(AITask task, const QString& userText) const;
    void configureLocalProvider();
    void continueDiagnostics(int step);
    void calibrationNext();
    void calibrationRecord(double tokensPerSec, const QString& error);
    void loadCalibration();
    void saveCalibration() const;

private:
    ModelRegistry m_registry;
    ModelManager* m_modelManager = nullptr;
    LocalLlamaServer* m_server = nullptr;
    OpenAICompatibleProvider* m_localProvider = nullptr;
    OpenAICompatibleProvider* m_cloudProvider = nullptr;
    EngineeringKnowledgeBase m_knowledge;
    std::unique_ptr<AIToolRegistry> m_tools;
    AIToolRegistry::SourcesProvider m_sources;
    AISettings m_settings;

    AIHardwareProfile m_hardware;
    ModelRecommendation m_recommendation;
    bool m_hardwareProbed = false;
    bool m_probing = false;

    // Conversation
    QVector<ChatMessage> m_history;   // échanges utilisateur/assistant (sans contexte)
    QVector<ChatMessage> m_turn;      // messages de la requête en cours (contexte + outils)
    PendingRequest m_pending;         // en attente (démarrage du moteur ou accord Cloud)
    Route m_route = Route::None;
    Route m_pendingRoute = Route::None;
    quint64 m_requestId = 0;
    int m_toolRounds = 0;
    bool m_busy = false;
    AITask m_currentTask = AITask::Chat;
    std::vector<ActionProposal> m_proposals;
    qint64 m_turnStartMs = 0;

    // Diagnostic
    bool m_diagnosticsRunning = false;
    quint64 m_diagnosticsRequest = 0;
    QStringList m_diagnosticsLines;

    // Auto-benchmark
    bool m_calibrating = false;
    QStringList m_calibQueue;     // « CPU » puis identifiants GPU
    QString m_calibDevice;
    QString m_calibModelPath;
    const ModelSpec* m_calibSpec = nullptr;
    quint64 m_calibRequest = 0;
    bool m_calibRestart = false;
    QStringList m_calibLines;
};

} // namespace TSA::AI
