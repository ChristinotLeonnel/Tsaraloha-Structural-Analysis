#include "OpenAICompatibleProvider.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace TSA::AI
{

QJsonObject ChatMessage::toJson() const
{
    QJsonObject o{ { "role", role } };
    if (role == "tool")
    {
        o["tool_call_id"] = toolCallId;
        o["content"] = content;
        return o;
    }
    o["content"] = content;
    if (!toolCalls.empty())
    {
        QJsonArray calls;
        for (const auto& c : toolCalls)
        {
            calls.append(QJsonObject{
                { "id", c.id },
                { "type", "function" },
                { "function", QJsonObject{ { "name", c.name }, { "arguments", c.argumentsJson } } } });
        }
        o["tool_calls"] = calls;
    }
    return o;
}

OpenAICompatibleProvider::OpenAICompatibleProvider(const OpenAICompatibleConfig& config, QObject* parent)
    : IAIProvider(parent)
    , m_config(config)
    , m_network(new QNetworkAccessManager(this))
{
    qRegisterMetaType<TSA::AI::ChatResult>("TSA::AI::ChatResult");
}

OpenAICompatibleProvider::~OpenAICompatibleProvider()
{
    const auto ids = m_pending.keys();
    for (quint64 id : ids) cancel(id);
}

AICapabilities OpenAICompatibleProvider::capabilities() const
{
    AICapabilities c;
    c.streaming = true;
    c.toolCalling = m_config.toolCalling;
    c.contextTokens = m_config.contextTokens;
    c.locality = m_config.locality;
    c.modelLabel = m_config.model;
    c.backendLabel = m_config.backendLabel;
    return c;
}

QJsonObject OpenAICompatibleProvider::buildRequestBody(const ChatRequest& request) const
{
    QJsonArray messages;
    for (const auto& m : request.messages) messages.append(m.toJson());

    QJsonObject body{
        { "model", m_config.model.isEmpty() ? QStringLiteral("default") : m_config.model },
        { "messages", messages },
        { "temperature", request.temperature },
        { "max_tokens", request.maxTokens },
        { "stream", request.stream },
    };
    if (request.stream) body["stream_options"] = QJsonObject{ { "include_usage", true } };
    if (!request.tools.isEmpty() && m_config.toolCalling)
    {
        body["tools"] = request.tools;
        body["tool_choice"] = "auto";
    }
    // Paramètre propre à llama.cpp : les fournisseurs Cloud refusent les champs inconnus.
    if (m_config.llamaCppExtensions && request.disableThinking)
        body["chat_template_kwargs"] = QJsonObject{ { "enable_thinking", false } };
    return body;
}

quint64 OpenAICompatibleProvider::chat(const ChatRequest& request)
{
    if (!isAvailable())
        return 0;

    const quint64 id = m_nextId++;
    auto* p = new Pending;
    p->stream = request.stream;
    p->clock.start();

    QUrl url = m_config.baseUrl;
    url.setPath(url.path().endsWith('/') ? url.path() + "chat/completions" : url.path() + "/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", request.stream ? "text/event-stream" : "application/json");
    if (!m_config.apiKey.isEmpty()) req.setRawHeader("Authorization", "Bearer " + m_config.apiKey.toUtf8());
    req.setTransferTimeout(0); // délai géré par le minuteur d'inactivité

    p->reply = m_network->post(req, QJsonDocument(buildRequestBody(request)).toJson(QJsonDocument::Compact));
    p->idleTimer = new QTimer(this);
    p->idleTimer->setSingleShot(true);
    p->idleTimer->setInterval(m_config.inactivityTimeoutMs);
    connect(p->idleTimer, &QTimer::timeout, this, [this, id] {
        fail(id, QStringLiteral("Délai dépassé : aucune réponse du modèle depuis %1 s.").arg(m_config.inactivityTimeoutMs / 1000));
    });
    p->idleTimer->start();
    connect(p->reply, &QNetworkReply::readyRead, this, [this, id] { onReadyRead(id); });
    connect(p->reply, &QNetworkReply::finished, this, [this, id] { onFinished(id); });
    m_pending.insert(id, p);
    return id;
}

void OpenAICompatibleProvider::consumeSse(StreamState& state, const QByteArray& chunk, QString* contentOut, QString* reasoningOut)
{
    state.buffer += chunk;
    int nl;
    while ((nl = state.buffer.indexOf('\n')) >= 0)
    {
        QByteArray line = state.buffer.left(nl).trimmed();
        state.buffer.remove(0, nl + 1);
        if (!line.startsWith("data:")) continue;
        line = line.mid(5).trimmed();
        if (line == "[DONE]") { state.done = true; continue; }

        const auto doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        const QJsonObject obj = doc.object();
        if (obj.contains("usage") && obj["usage"].isObject())
        {
            const auto u = obj["usage"].toObject();
            state.result.promptTokens = u.value("prompt_tokens").toInt(-1);
            state.result.completionTokens = u.value("completion_tokens").toInt(-1);
        }
        if (obj.value("timings").isObject())
        {
            const auto t = obj["timings"].toObject();
            state.result.serverTokensPerSecond = t.value("predicted_per_second").toDouble();
            if (state.result.completionTokens < 0) state.result.completionTokens = t.value("predicted_n").toInt(-1);
            if (state.result.promptTokens < 0) state.result.promptTokens = t.value("prompt_n").toInt(-1);
        }
        for (const auto& cv : obj.value("choices").toArray())
        {
            const QJsonObject choice = cv.toObject();
            if (choice.contains("finish_reason") && !choice["finish_reason"].isNull())
                state.result.finishReason = choice["finish_reason"].toString();
            const QJsonObject delta = choice.value("delta").toObject();
            const QString content = delta.value("content").toString();
            if (!content.isEmpty())
            {
                state.result.content += content;
                if (contentOut) *contentOut += content;
            }
            const QString reasoning = delta.value("reasoning_content").toString();
            if (!reasoning.isEmpty())
            {
                state.result.reasoning += reasoning;
                if (reasoningOut) *reasoningOut += reasoning;
            }
            for (const auto& tv : delta.value("tool_calls").toArray())
            {
                const QJsonObject t = tv.toObject();
                ToolCall& acc = state.toolAccumulator[t.value("index").toInt(0)];
                if (t.contains("id") && !t["id"].toString().isEmpty()) acc.id = t["id"].toString();
                const QJsonObject fn = t.value("function").toObject();
                if (fn.contains("name") && !fn["name"].toString().isEmpty()) acc.name = fn["name"].toString();
                acc.argumentsJson += fn.value("arguments").toString();
            }
        }
    }
}

void OpenAICompatibleProvider::finalizeToolCalls(StreamState& state)
{
    state.result.toolCalls.clear();
    int n = 0;
    for (auto& [index, call] : state.toolAccumulator)
    {
        if (call.name.isEmpty()) continue;
        if (call.id.isEmpty()) call.id = QStringLiteral("call_%1").arg(n);
        if (call.argumentsJson.trimmed().isEmpty()) call.argumentsJson = "{}";
        state.result.toolCalls.push_back(call);
        ++n;
    }
}

void OpenAICompatibleProvider::onReadyRead(quint64 id)
{
    Pending* p = m_pending.value(id, nullptr);
    if (!p || !p->reply) return;
    p->idleTimer->start();
    if (!p->stream) return; // lu en bloc à la fin

    const QByteArray chunk = p->reply->readAll();
    QString content, reasoning;
    consumeSse(p->state, chunk, &content, &reasoning);
    if ((!content.isEmpty() || !reasoning.isEmpty()) && p->state.result.firstTokenMs < 0)
        p->state.result.firstTokenMs = p->clock.elapsed();
    if (!reasoning.isEmpty()) emit reasoningDelta(id, reasoning);
    if (!content.isEmpty()) emit contentDelta(id, content);
}

void OpenAICompatibleProvider::onFinished(quint64 id)
{
    Pending* p = m_pending.value(id, nullptr);
    if (!p || !p->reply) return;

    const int http = p->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (p->reply->error() != QNetworkReply::NoError || (http != 0 && http >= 400))
    {
        QString detail = QString::fromUtf8(p->reply->readAll()).left(400);
        const auto doc = QJsonDocument::fromJson(detail.toUtf8());
        if (doc.isObject() && doc.object().value("error").isObject())
            detail = doc.object()["error"].toObject().value("message").toString(detail);
        fail(id, QStringLiteral("HTTP %1 — %2 %3").arg(http).arg(p->reply->errorString(), detail).trimmed());
        return;
    }

    if (p->stream)
    {
        onReadyRead(id);
        consumeSse(p->state, "\n", nullptr, nullptr);
    }
    else
    {
        const auto doc = QJsonDocument::fromJson(p->reply->readAll());
        const auto choice = doc.object().value("choices").toArray().first().toObject();
        const auto msg = choice.value("message").toObject();
        p->state.result.content = msg.value("content").toString();
        p->state.result.reasoning = msg.value("reasoning_content").toString();
        p->state.result.finishReason = choice.value("finish_reason").toString();
        int i = 0;
        for (const auto& tv : msg.value("tool_calls").toArray())
        {
            const auto t = tv.toObject();
            ToolCall& acc = p->state.toolAccumulator[i++];
            acc.id = t.value("id").toString();
            acc.name = t.value("function").toObject().value("name").toString();
            acc.argumentsJson = t.value("function").toObject().value("arguments").toString();
        }
        const auto u = doc.object().value("usage").toObject();
        p->state.result.promptTokens = u.value("prompt_tokens").toInt(-1);
        p->state.result.completionTokens = u.value("completion_tokens").toInt(-1);
    }
    finalizeToolCalls(p->state);
    p->state.result.latencyMs = p->clock.elapsed();

    ChatResult result = p->state.result;
    p->idleTimer->stop();
    p->idleTimer->deleteLater();
    p->reply->deleteLater();
    m_pending.remove(id);
    delete p;
    emit finished(id, result);
}

void OpenAICompatibleProvider::fail(quint64 id, const QString& error)
{
    Pending* p = m_pending.take(id);
    if (!p) return;
    if (p->idleTimer)
    {
        p->idleTimer->stop();
        p->idleTimer->deleteLater();
    }
    if (p->reply)
    {
        p->reply->disconnect(this);
        p->reply->abort();
        p->reply->deleteLater();
    }
    delete p;
    emit failed(id, error);
}

void OpenAICompatibleProvider::cancel(quint64 requestId)
{
    fail(requestId, QStringLiteral("Requête annulée."));
}

} // namespace TSA::AI
