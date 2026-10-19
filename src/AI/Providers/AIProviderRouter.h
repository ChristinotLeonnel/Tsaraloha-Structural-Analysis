#pragma once

// Routeur multi-fournisseur avec basculement automatique (Fallback) :
//  - Tente d'abord le fournisseur primaire (ex. Gemini 2.0 Flash gratuit ou DeepSeek-R1)
//  - En cas d'erreur réseau, de code HTTP 429/503 ou de timeout, bascule immédiatement
//    sur le fournisseur de secours (ex. DeepSeek-V3 ou moteur local Qwen)
//  - Garantit zéro interruption pour l'ingénieur sans blocage de l'interface TSA.

#include "AIProvider.h"

#include <QPointer>

namespace TSA::AI
{

class AIProviderRouter : public IAIProvider
{
    Q_OBJECT

public:
    explicit AIProviderRouter(IAIProvider* primary = nullptr, IAIProvider* fallback = nullptr, QObject* parent = nullptr);
    ~AIProviderRouter() override = default;

    void setPrimaryProvider(IAIProvider* primary);
    void setFallbackProvider(IAIProvider* fallback);
    IAIProvider* primaryProvider() const { return m_primary; }
    IAIProvider* fallbackProvider() const { return m_fallback; }

    QString providerId() const override;
    QString displayName() const override;
    ProviderLocality locality() const override;
    bool isAvailable() const override;
    AICapabilities capabilities() const override;

    quint64 chat(const ChatRequest& request) override;
    void cancel(quint64 requestId) override;

    int totalFallbacks() const { return m_fallbackCount; }
    QString lastActiveProviderId() const { return m_lastActiveProviderId; }

signals:
    void fallbackTriggered(const QString& fromProvider, const QString& toProvider, const QString& reason);

private:
    void setupConnections(IAIProvider* provider);
    void onPrimaryFailed(quint64 primaryId, const QString& error);

private:
    struct RoutedRequest
    {
        quint64 routerId = 0;
        quint64 underlyingId = 0;
        ChatRequest originalRequest;
        bool usingFallback = false;
    };

    QPointer<IAIProvider> m_primary;
    QPointer<IAIProvider> m_fallback;
    QHash<quint64, RoutedRequest> m_activeRequests; // clé = routerId
    QHash<quint64, quint64> m_underlyingToRouter;   // underlyingId -> routerId
    quint64 m_nextRouterId = 1;
    int m_fallbackCount = 0;
    QString m_lastActiveProviderId;
};

} // namespace TSA::AI
