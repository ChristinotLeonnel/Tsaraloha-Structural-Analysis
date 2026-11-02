#include "OpenFileQueue.h"

namespace TSA::Platform
{

OpenFileQueue::OpenFileQueue(std::function<void(const QString&)> open, std::function<bool()> busy, QObject* parent)
    : QObject(parent), m_open(std::move(open)), m_busy(std::move(busy))
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this] { process(); });
}

void OpenFileQueue::enqueue(const QStringList& files)
{
    for (const QString& file : files)
        if (!m_pending.contains(file, Qt::CaseInsensitive)) m_pending << file;
    process();
}

void OpenFileQueue::clear()
{
    m_pending.clear();
    m_retry.stop();
}

void OpenFileQueue::process()
{
    if (m_running || m_pending.isEmpty()) return;   // appelé pendant l'ouverture en cours : la suite est programmée à sa fin
    if (m_busy && m_busy())
    {
        m_retry.start(200);
        return;
    }
    m_running = true;
    const QString file = m_pending.takeFirst();
    m_open(file);
    m_running = false;
    // Un fichier à la fois : l'interface reprend la main entre deux (onglet affiché, fermeture et clics traités)
    if (!m_pending.isEmpty()) m_retry.start(0);
}

} // namespace TSA::Platform
