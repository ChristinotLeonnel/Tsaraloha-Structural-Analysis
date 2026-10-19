#include "MockAIProvider.h"

#include <QCoreApplication>
#include <QTimer>

namespace TSA::AI
{

MockAIProvider::MockAIProvider(QObject* parent)
    : IAIProvider(parent)
{
    qRegisterMetaType<TSA::AI::ChatResult>("TSA::AI::ChatResult");
}

MockAIProvider::MockAIProvider(const QString& id, const QString& name, QObject* parent)
    : IAIProvider(parent)
    , m_id(id)
    , m_name(name)
{
    qRegisterMetaType<TSA::AI::ChatResult>("TSA::AI::ChatResult");
}

AICapabilities MockAIProvider::capabilities() const
{
    AICapabilities c;
    c.streaming = true;
    c.toolCalling = true;
    c.contextTokens = 131072;
    c.locality = m_locality;
    c.modelLabel = QStringLiteral("mock-model-v1");
    c.backendLabel = QStringLiteral("Simulation Déterministe");
    return c;
}

void MockAIProvider::enqueueScenario(const MockScenario& scenario)
{
    m_scenarios.enqueue(scenario);
}

void MockAIProvider::clearScenarios()
{
    m_scenarios.clear();
}

quint64 MockAIProvider::chat(const ChatRequest& request)
{
    if (!isAvailable()) return 0;

    const quint64 id = m_nextId++;
    m_lastRequest = request;
    m_requestCount++;

    MockScenario sc;
    if (!m_scenarios.isEmpty())
    {
        sc = m_scenarios.dequeue();
    }
    else if (m_responder)
    {
        sc = m_responder(request);
    }
    else
    {
        // Réponse par défaut simulant un avis d'ingénierie structurelle
        sc.content = QStringLiteral("[FAIT] Modèle analysé en mode simulation.\n"
                                    "[RECOMMANDATION] Structure vérifiée sans anomalie critique.\n"
                                    "Confiance : élevée.");
        sc.promptTokens = 120;
        sc.completionTokens = 25;
    }

    // Exécution différée au prochain tour d'événement pour permettre aux routeurs
    // et décorateurs d'enregistrer l'ID de requête renvoyé avant émission des signaux.
    QTimer::singleShot(0, this, [this, id, sc] {
        executeScenario(id, sc);
    });

    return id;
}

void MockAIProvider::cancel(quint64 requestId)
{
    // Dans le mock synchrone, la requête est déjà terminée. En asynchrone, émet failed
    emit failed(requestId, QStringLiteral("Requête simulée annulée."));
}

void MockAIProvider::executeScenario(quint64 id, const MockScenario& sc)
{
    if (sc.shouldFail)
    {
        emit failed(id, sc.errorMessage.isEmpty() ? QStringLiteral("Erreur simulée (Mock failure)") : sc.errorMessage);
        return;
    }

    if (sc.streamInChunks && !sc.chunks.isEmpty())
    {
        for (const QString& chunk : sc.chunks)
        {
            emit contentDelta(id, chunk);
        }
    }
    else if (!sc.content.isEmpty())
    {
        emit contentDelta(id, sc.content);
    }

    if (!sc.reasoning.isEmpty())
    {
        emit reasoningDelta(id, sc.reasoning);
    }

    ChatResult res;
    res.content = sc.content;
    res.reasoning = sc.reasoning;
    res.toolCalls = sc.toolCalls;
    res.promptTokens = sc.promptTokens;
    res.completionTokens = sc.completionTokens;
    res.serverTokensPerSecond = sc.tokensPerSecond;
    res.latencyMs = 15;
    res.firstTokenMs = 5;
    res.finishReason = sc.toolCalls.empty() ? QStringLiteral("stop") : QStringLiteral("tool_calls");

    emit finished(id, res);
}

} // namespace TSA::AI
