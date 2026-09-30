#include "WindowRegistry.h"
#include <QSet>

namespace TSA::UI
{

WindowRegistry::WindowRegistry(QObject* parent)
    : QObject(parent)
{
}

bool WindowRegistry::registerWindow(const WindowInfo& info)
{
    if (info.id.trimmed().isEmpty())
    {
        return false;
    }

    if (m_windows.contains(info.id))
    {
        // Déjà présent : on met à jour la structure sans dupliquer l'ordre
        m_windows[info.id] = info;
        if (info.dockWidget)
        {
            setupDockConnections(info.id, info.dockWidget);
        }
        emit windowRegistered(info.id);
        return true;
    }

    m_windows.insert(info.id, info);
    m_order.append(info.id);

    if (info.dockWidget)
    {
        setupDockConnections(info.id, info.dockWidget);
    }

    emit windowRegistered(info.id);
    return true;
}

bool WindowRegistry::unregisterWindow(const QString& id)
{
    if (!m_windows.contains(id))
    {
        return false;
    }

    m_windows.remove(id);
    m_order.removeAll(id);

    emit windowUnregistered(id);
    return true;
}

WindowInfo* WindowRegistry::findWindow(const QString& id)
{
    auto it = m_windows.find(id);
    if (it != m_windows.end())
    {
        return &it.value();
    }
    return nullptr;
}

const WindowInfo* WindowRegistry::findWindow(const QString& id) const
{
    auto it = m_windows.constFind(id);
    if (it != m_windows.constEnd())
    {
        return &it.value();
    }
    return nullptr;
}

bool WindowRegistry::hasWindow(const QString& id) const
{
    return m_windows.contains(id);
}

QList<WindowInfo> WindowRegistry::allWindows() const
{
    QList<WindowInfo> list;
    list.reserve(m_order.size());
    for (const auto& id : m_order)
    {
        if (auto it = m_windows.constFind(id); it != m_windows.constEnd())
        {
            list.append(it.value());
        }
    }
    return list;
}

QStringList WindowRegistry::categories() const
{
    QStringList result;
    QSet<QString> seen;
    for (const auto& id : m_order)
    {
        const auto& w = m_windows[id];
        if (!w.category.isEmpty() && !seen.contains(w.category))
        {
            seen.insert(w.category);
            result.append(w.category);
        }
    }
    return result;
}

QList<WindowInfo> WindowRegistry::windowsByCategory(const QString& category) const
{
    QList<WindowInfo> list;
    for (const auto& id : m_order)
    {
        const auto& w = m_windows[id];
        if (w.category == category)
        {
            list.append(w);
        }
    }
    return list;
}

void WindowRegistry::clear()
{
    m_windows.clear();
    m_order.clear();
}

int WindowRegistry::count() const
{
    return static_cast<int>(m_windows.size());
}

void WindowRegistry::setupDockConnections(const QString& id, QDockWidget* dock)
{
    if (!dock)
    {
        return;
    }

    connect(dock, &QDockWidget::visibilityChanged, this, [this, id](bool visible) {
        if (auto* item = findWindow(id))
        {
            if (item->action && item->action->isChecked() != visible)
            {
                const QSignalBlocker blocker(item->action);
                item->action->setChecked(visible);
            }
        }
        emit windowVisibilityChanged(id, visible);
    });

    connect(dock, &QDockWidget::topLevelChanged, this, [this, id](bool topLevel) {
        emit windowFloatingChanged(id, topLevel);
    });
}

} // namespace TSA::UI
