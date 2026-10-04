#pragma once

// Interface commune des fournisseurs d'inférence. Les fournisseurs ne connaissent rien de TSA :
// ils reçoivent des messages et des définitions d'outils, et renvoient texte et appels d'outils.

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>
#include <vector>

namespace TSA::AI
{

struct ToolCall
{
    QString id;
    QString name;
    QString argumentsJson;
};

struct ChatMessage
{
    QString role;          // system, user, assistant, tool
    QString content;
    QString toolCallId;    // rôle « tool »
    std::vector<ToolCall> toolCalls; // rôle « assistant » ayant demandé des outils

    static ChatMessage system(const QString& c) { return { "system", c, {}, {} }; }
    static ChatMessage user(const QString& c) { return { "user", c, {}, {} }; }
    static ChatMessage assistant(const QString& c) { return { "assistant", c, {}, {} }; }
    static ChatMessage tool(const QString& callId, const QString& c) { return { "tool", c, callId, {} }; }
    QJsonObject toJson() const;
};

struct ChatRequest
{
    QVector<ChatMessage> messages;
    QJsonArray tools;            // définitions au format « function » OpenAI
    double temperature = 0.3;
    int maxTokens = 1536;
    bool stream = true;
    bool disableThinking = true; // modèles « thinking » (Qwen3) : réponse directe par défaut
};

struct ChatResult
{
    QString content;
    QString reasoning;
    std::vector<ToolCall> toolCalls;
    QString finishReason;
    int promptTokens = -1;
    int completionTokens = -1;
    qint64 latencyMs = 0;
    qint64 firstTokenMs = -1;
    double serverTokensPerSecond = 0.0; // mesure du moteur (llama.cpp « timings »), si fournie
    double tokensPerSecond() const
    {
        if (serverTokensPerSecond > 0.0) return serverTokensPerSecond;
        const qint64 genMs = latencyMs - (firstTokenMs > 0 ? firstTokenMs : 0);
        return (completionTokens > 0 && genMs > 0) ? completionTokens * 1000.0 / genMs : 0.0;
    }
};

enum class ProviderLocality { Local, Cloud };

struct AICapabilities
{
    bool streaming = true;
    bool toolCalling = false;
    int contextTokens = 0;
    ProviderLocality locality = ProviderLocality::Local;
    QString modelLabel;
    QString backendLabel; // « Vulkan · RTX 3050 », « CPU », « Gemini »…
};

class IAIProvider : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~IAIProvider() override = default;

    virtual QString providerId() const = 0;
    virtual QString displayName() const = 0;
    virtual ProviderLocality locality() const = 0;
    virtual bool isAvailable() const = 0;
    virtual AICapabilities capabilities() const = 0;

    /// Lance une requête asynchrone ; renvoie son identifiant (0 en cas de refus immédiat).
    virtual quint64 chat(const ChatRequest& request) = 0;
    virtual void cancel(quint64 requestId) = 0;

signals:
    void contentDelta(quint64 requestId, const QString& text);
    void reasoningDelta(quint64 requestId, const QString& text);
    void finished(quint64 requestId, const TSA::AI::ChatResult& result);
    void failed(quint64 requestId, const QString& error);
};

} // namespace TSA::AI

Q_DECLARE_METATYPE(TSA::AI::ChatResult)
