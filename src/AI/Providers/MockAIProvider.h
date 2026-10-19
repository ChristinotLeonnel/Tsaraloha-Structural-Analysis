#pragma once

// Fournisseur IA simulé (Mock) pour les tests automatisés, la CI/CD et le mode démonstration
// sans connexion Internet ni clé API. Permet d'injecter des scénarios prédéfinis, des flux de
// jetons simulés, des appels d'outils et des erreurs simulées (429, timeout, panne réseau).

#include "AIProvider.h"

#include <QQueue>
#include <QStringList>
#include <functional>

namespace TSA::AI
{

struct MockScenario
{
    QString content;
    QString reasoning;
    std::vector<ToolCall> toolCalls;
    bool shouldFail = false;
    QString errorMessage;
    int promptTokens = 42;
    int completionTokens = 24;
    double tokensPerSecond = 45.0;
    bool streamInChunks = false;
    QStringList chunks;
};

class MockAIProvider : public IAIProvider
{
    Q_OBJECT

public:
    explicit MockAIProvider(QObject* parent = nullptr);
    explicit MockAIProvider(const QString& id, const QString& name, QObject* parent = nullptr);
    ~MockAIProvider() override = default;

    QString providerId() const override { return m_id; }
    QString displayName() const override { return m_name; }
    ProviderLocality locality() const override { return m_locality; }
    bool isAvailable() const override { return m_available; }
    AICapabilities capabilities() const override;

    void setProviderId(const QString& id) { m_id = id; }
    void setDisplayName(const QString& name) { m_name = name; }
    void setLocality(ProviderLocality loc) { m_locality = loc; }
    void setAvailable(bool avail) { m_available = avail; }
    void setSyncExecution(bool sync) { m_sync = sync; }

    /// Ajoute un scénario à la file d'attente des réponses simulées
    void enqueueScenario(const MockScenario& scenario);
    void clearScenarios();

    /// Définit un générateur dynamique de réponse basé sur la requête
    using DynamicResponder = std::function<MockScenario(const ChatRequest&)>;
    void setDynamicResponder(DynamicResponder responder) { m_responder = std::move(responder); }

    quint64 chat(const ChatRequest& request) override;
    void cancel(quint64 requestId) override;

    int totalRequestsReceived() const { return m_requestCount; }
    const ChatRequest& lastRequest() const { return m_lastRequest; }

private:
    void executeScenario(quint64 id, const MockScenario& sc);

private:
    QString m_id = QStringLiteral("mock-ai");
    QString m_name = QStringLiteral("Fournisseur Simulé (Mock)");
    ProviderLocality m_locality = ProviderLocality::Local;
    bool m_available = true;
    bool m_sync = true; // Exécution synchrone par défaut pour la rapidité des tests unitaires
    quint64 m_nextId = 1;
    int m_requestCount = 0;
    ChatRequest m_lastRequest;
    QQueue<MockScenario> m_scenarios;
    DynamicResponder m_responder;
};

} // namespace TSA::AI
