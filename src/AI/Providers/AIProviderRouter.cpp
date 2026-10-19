#include "AIProviderRouter.h"

namespace TSA::AI
{

AIProviderRouter::AIProviderRouter(IAIProvider* primary, IAIProvider* fallback, QObject* parent)
    : IAIProvider(parent)
    , m_primary(primary)
    , m_fallback(fallback)
{
    qRegisterMetaType<TSA::AI::ChatResult>("TSA::AI::ChatResult");
    if (m_primary) setupConnections(m_primary);
    if (m_fallback) setupConnections(m_fallback);
}

void AIProviderRouter::setPrimaryProvider(IAIProvider* primary)
{
    if (m_primary == primary) return;
    if (m_primary) disconnect(m_primary, nullptr, this, nullptr);
    m_primary = primary;
    if (m_primary) setupConnections(m_primary);
}

void AIProviderRouter::setFallbackProvider(IAIProvider* fallback)
{
    if (m_fallback == fallback) return;
    if (m_fallback) disconnect(m_fallback, nullptr, this, nullptr);
    m_fallback = fallback;
    if (m_fallback) setupConnections(m_fallback);
}

QString AIProviderRouter::providerId() const
{
    if (m_primary && m_primary->isAvailable()) return m_primary->providerId();
    if (m_fallback && m_fallback->isAvailable()) return m_fallback->providerId();
    return QStringLiteral("router-none");
}

QString AIProviderRouter::displayName() const
{
    if (m_primary && m_primary->isAvailable())
    {
        return m_fallback && m_fallback->isAvailable()
                   ? QStringLiteral("%1 (Secours : %2)").arg(m_primary->displayName(), m_fallback->displayName())
                   : m_primary->displayName();
    }
    if (m_fallback && m_fallback->isAvailable())
    {
        return QStringLiteral("%1 (Mode repli)").arg(m_fallback->displayName());
    }
    return QStringLiteral("Aucun fournisseur disponible");
}

ProviderLocality AIProviderRouter::locality() const
{
    if (m_primary && m_primary->isAvailable()) return m_primary->locality();
    if (m_fallback && m_fallback->isAvailable()) return m_fallback->locality();
    return ProviderLocality::Local;
}

bool AIProviderRouter::isAvailable() const
{
    return (m_primary && m_primary->isAvailable()) || (m_fallback && m_fallback->isAvailable());
}

AICapabilities AIProviderRouter::capabilities() const
{
    if (m_primary && m_primary->isAvailable()) return m_primary->capabilities();
    if (m_fallback && m_fallback->isAvailable()) return m_fallback->capabilities();
    AICapabilities c;
    c.streaming = false;
    c.toolCalling = false;
    return c;
}

void AIProviderRouter::setupConnections(IAIProvider* provider)
{
    if (!provider) return;

    connect(provider, &IAIProvider::contentDelta, this, [this](quint64 underlyingId, const QString& text) {
        const quint64 routerId = m_underlyingToRouter.value(underlyingId, 0);
        if (routerId > 0) emit contentDelta(routerId, text);
    });

    connect(provider, &IAIProvider::reasoningDelta, this, [this](quint64 underlyingId, const QString& text) {
        const quint64 routerId = m_underlyingToRouter.value(underlyingId, 0);
        if (routerId > 0) emit reasoningDelta(routerId, text);
    });

    connect(provider, &IAIProvider::finished, this, [this](quint64 underlyingId, const ChatResult& res) {
        const quint64 routerId = m_underlyingToRouter.take(underlyingId);
        if (routerId > 0)
        {
            m_activeRequests.remove(routerId);
            emit finished(routerId, res);
        }
    });

    connect(provider, &IAIProvider::failed, this, [this](quint64 underlyingId, const QString& error) {
        const quint64 routerId = m_underlyingToRouter.take(underlyingId);
        if (routerId == 0) return;

        auto it = m_activeRequests.find(routerId);
        if (it == m_activeRequests.end()) return;

        RoutedRequest& req = it.value();
        // Si la requête a échoué sur le primaire et qu'un fallback est disponible, on bascule
        if (!req.usingFallback && m_fallback && m_fallback->isAvailable())
        {
            req.usingFallback = true;
            m_fallbackCount++;
            const QString fromName = m_primary ? m_primary->displayName() : QStringLiteral("Primaire");
            const QString toName = m_fallback->displayName();
            m_lastActiveProviderId = m_fallback->providerId();

            emit fallbackTriggered(fromName, toName, error);

            const quint64 fallbackId = m_fallback->chat(req.originalRequest);
            if (fallbackId > 0)
            {
                req.underlyingId = fallbackId;
                m_underlyingToRouter.insert(fallbackId, routerId);
                return;
            }
        }

        // Échec définitif
        m_activeRequests.remove(routerId);
        emit failed(routerId, error);
    });
}

quint64 AIProviderRouter::chat(const ChatRequest& request)
{
    if (!isAvailable()) return 0;

    const quint64 routerId = m_nextRouterId++;
    RoutedRequest r;
    r.routerId = routerId;
    r.originalRequest = request;

    // Choix du point de départ : primaire si disponible, sinon repli direct
    if (m_primary && m_primary->isAvailable())
    {
        r.usingFallback = false;
        m_lastActiveProviderId = m_primary->providerId();
        const quint64 underlying = m_primary->chat(request);
        if (underlying > 0)
        {
            r.underlyingId = underlying;
            m_activeRequests.insert(routerId, r);
            m_underlyingToRouter.insert(underlying, routerId);
            return routerId;
        }
    }

    if (m_fallback && m_fallback->isAvailable())
    {
        r.usingFallback = true;
        m_lastActiveProviderId = m_fallback->providerId();
        const quint64 underlying = m_fallback->chat(request);
        if (underlying > 0)
        {
            r.underlyingId = underlying;
            m_activeRequests.insert(routerId, r);
            m_underlyingToRouter.insert(underlying, routerId);
            return routerId;
        }
    }

    return 0;
}

void AIProviderRouter::cancel(quint64 requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end()) return;

    const RoutedRequest req = it.value();
    m_underlyingToRouter.remove(req.underlyingId);
    m_activeRequests.erase(it);

    if (req.usingFallback && m_fallback)
    {
        m_fallback->cancel(req.underlyingId);
    }
    else if (!req.usingFallback && m_primary)
    {
        m_primary->cancel(req.underlyingId);
    }

    emit failed(requestId, QStringLiteral("Requête annulée par l'utilisateur."));
}

} // namespace TSA::AI
