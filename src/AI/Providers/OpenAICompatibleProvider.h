#pragma once

// Client du protocole « Chat Completions » compatible OpenAI, partagé par :
//  - llama-server (local, TSA le démarre lui-même),
//  - Ollama (local, optionnel),
//  - fournisseurs Cloud exposant ce protocole (OpenAI, Gemini via son point d'accès compatible, etc.).
// Streaming SSE, appels d'outils, annulation et délai d'inactivité. Réseau asynchrone : jamais bloquant.

#include "AIProvider.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QUrl>
#include <map>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace TSA::AI
{

struct OpenAICompatibleConfig
{
    QString providerId;          // « local-llama », « ollama », « gemini », « openai-compatible »
    QString displayName;
    QUrl baseUrl;                // se termine par /v1 (ou /v1beta/openai pour Gemini)
    QString apiKey;              // vide pour le local
    QString model;               // nom de modèle envoyé dans la requête
    ProviderLocality locality = ProviderLocality::Local;
    bool llamaCppExtensions = false; // chat_template_kwargs (désactivation du « thinking » Qwen3)
    int inactivityTimeoutMs = 180000;
    int contextTokens = 0;
    QString backendLabel;
    bool toolCalling = true;
};

class OpenAICompatibleProvider : public IAIProvider
{
    Q_OBJECT

public:
    explicit OpenAICompatibleProvider(const OpenAICompatibleConfig& config, QObject* parent = nullptr);
    ~OpenAICompatibleProvider() override;

    void setConfig(const OpenAICompatibleConfig& config) { m_config = config; }
    const OpenAICompatibleConfig& config() const { return m_config; }
    void setAvailable(bool available) { m_available = available; }

    QString providerId() const override { return m_config.providerId; }
    QString displayName() const override { return m_config.displayName; }
    ProviderLocality locality() const override { return m_config.locality; }
    bool isAvailable() const override { return m_available && m_config.baseUrl.isValid(); }
    AICapabilities capabilities() const override;

    quint64 chat(const ChatRequest& request) override;
    void cancel(quint64 requestId) override;

    /// Construit le corps JSON (exposé pour les tests).
    QJsonObject buildRequestBody(const ChatRequest& request) const;

    /// Analyse un bloc d'événements SSE ; renvoie les lignes « data: » complètes consommées.
    /// Exposé pour les tests : accumule dans result/toolAccumulator.
    struct StreamState
    {
        QByteArray buffer;
        ChatResult result;
        std::map<int, ToolCall> toolAccumulator;
        bool done = false;
    };
    static void consumeSse(StreamState& state, const QByteArray& chunk, QString* contentOut, QString* reasoningOut);
    static void finalizeToolCalls(StreamState& state);

private:
    struct Pending
    {
        QPointer<QNetworkReply> reply;
        StreamState state;
        QElapsedTimer clock;
        QTimer* idleTimer = nullptr;
        bool stream = true;
    };

    void onReadyRead(quint64 id);
    void onFinished(quint64 id);
    void fail(quint64 id, const QString& error);

private:
    OpenAICompatibleConfig m_config;
    QNetworkAccessManager* m_network = nullptr;
    QHash<quint64, Pending*> m_pending;
    quint64 m_nextId = 1;
    bool m_available = true;
};

} // namespace TSA::AI
