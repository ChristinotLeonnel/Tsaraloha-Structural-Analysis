#include "WindowManager.h"
#include <QActionGroup>
#include <QInputDialog>
#include <QMessageBox>

namespace TSA::UI
{

WindowManager::WindowManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_registry(std::make_unique<WindowRegistry>(this))
    , m_layoutManager(std::make_unique<LayoutManager>(mainWindow, m_registry.get(), this))
{
    // Relayer les signaux du registre
    connect(m_registry.get(), &WindowRegistry::windowRegistered, this, &WindowManager::windowRegistered);
    connect(m_registry.get(), &WindowRegistry::windowUnregistered, this, &WindowManager::windowUnregistered);
    connect(m_registry.get(), &WindowRegistry::windowVisibilityChanged, this, &WindowManager::windowVisibilityChanged);
    connect(m_registry.get(), &WindowRegistry::windowFloatingChanged, this, &WindowManager::windowFloatingChanged);

    // Relayer les signaux du layout manager
    connect(m_layoutManager.get(), &LayoutManager::layoutReset, this, &WindowManager::layoutReset);
    connect(m_layoutManager.get(), &LayoutManager::layoutRestored, this, &WindowManager::layoutRestored);
    connect(m_layoutManager.get(), &LayoutManager::layoutSaved, this, &WindowManager::layoutSaved);
    connect(m_layoutManager.get(), &LayoutManager::profileApplied, this, &WindowManager::profileApplied);
}

bool WindowManager::registerWindow(const QString& id,
                                   const QString& title,
                                   const QString& category,
                                   QWidget* widget,
                                   Qt::DockWidgetArea defaultArea,
                                   bool defaultVisible,
                                   const QKeySequence& shortcut,
                                   const QIcon& icon)
{
    if (id.trimmed().isEmpty())
    {
        return false;
    }

    WindowInfo info;
    info.id = id;
    info.title = title;
    info.category = category;
    info.widget = widget;
    info.dockWidget = qobject_cast<QDockWidget*>(widget);
    info.defaultArea = defaultArea;
    info.defaultVisible = defaultVisible;
    info.shortcut = shortcut;
    info.icon = icon;

    auto* act = new QAction(title, this);
    act->setCheckable(true);
    act->setChecked(info.isVisible());
    if (!shortcut.isEmpty())
    {
        act->setShortcut(shortcut);
    }
    if (!icon.isNull())
    {
        act->setIcon(icon);
    }

    if (m_mainWindow)
    {
        m_mainWindow->addAction(act);
    }

    connect(act, &QAction::triggered, this, [this, id](bool checked) {
        if (id == "viewport")
        {
            focusWindow(id);
            if (auto* it = findWindow(id); it && it->action)
            {
                const QSignalBlocker blocker(it->action);
                it->action->setChecked(true);
            }
            return;
        }
        if (checked)
        {
            showWindow(id);
        }
        else
        {
            hideWindow(id);
        }
    });

    info.action = act;

    return m_registry->registerWindow(info);
}

bool WindowManager::registerDock(const QString& id,
                                 const QString& title,
                                 const QString& category,
                                 QDockWidget* dock,
                                 Qt::DockWidgetArea defaultArea,
                                 bool defaultVisible,
                                 const QKeySequence& shortcut,
                                 const QIcon& icon)
{
    if (id.trimmed().isEmpty() || !dock)
    {
        return false;
    }

    WindowInfo info;
    info.id = id;
    info.title = title;
    info.category = category;
    info.widget = dock;
    info.dockWidget = dock;
    info.defaultArea = defaultArea;
    info.defaultVisible = defaultVisible;
    info.shortcut = shortcut;
    info.icon = icon;

    auto* act = new QAction(title, this);
    act->setCheckable(true);
    act->setChecked(dock->isVisible());
    if (!shortcut.isEmpty())
    {
        act->setShortcut(shortcut);
    }
    if (!icon.isNull())
    {
        act->setIcon(icon);
    }

    if (m_mainWindow)
    {
        m_mainWindow->addAction(act);
    }

    connect(act, &QAction::triggered, this, [this, id](bool checked) {
        if (checked)
        {
            showWindow(id);
        }
        else
        {
            hideWindow(id);
        }
    });

    info.action = act;

    return m_registry->registerWindow(info);
}

bool WindowManager::unregisterWindow(const QString& id)
{
    if (auto* item = m_registry->findWindow(id))
    {
        if (item->action && m_mainWindow)
        {
            m_mainWindow->removeAction(item->action);
        }
    }
    return m_registry->unregisterWindow(id);
}

bool WindowManager::showWindow(const QString& id)
{
    auto* item = m_registry->findWindow(id);
    if (!item)
    {
        return false;
    }

    if (item->dockWidget)
    {
        item->dockWidget->show();
        item->dockWidget->raise();
        if (item->dockWidget->widget())
        {
            item->dockWidget->widget()->setFocus();
        }
    }
    else if (item->widget)
    {
        item->widget->show();
        item->widget->raise();
        item->widget->setFocus();
    }

    if (item->action && !item->action->isChecked())
    {
        const QSignalBlocker blocker(item->action);
        item->action->setChecked(true);
    }

    emit windowVisibilityChanged(id, true);
    return true;
}

bool WindowManager::hideWindow(const QString& id)
{
    auto* item = m_registry->findWindow(id);
    if (!item)
    {
        return false;
    }

    if (item->dockWidget)
    {
        item->dockWidget->hide();
    }
    else if (item->widget)
    {
        item->widget->hide();
    }

    if (item->action && item->action->isChecked())
    {
        const QSignalBlocker blocker(item->action);
        item->action->setChecked(false);
    }

    emit windowVisibilityChanged(id, false);
    return true;
}

bool WindowManager::toggleWindow(const QString& id)
{
    if (isWindowVisible(id))
    {
        return hideWindow(id);
    }
    return showWindow(id);
}

bool WindowManager::isWindowVisible(const QString& id) const
{
    const auto* item = m_registry->findWindow(id);
    return item ? item->isVisible() : false;
}

bool WindowManager::isWindowFloating(const QString& id) const
{
    const auto* item = m_registry->findWindow(id);
    return item ? item->isFloating() : false;
}

bool WindowManager::setWindowFloating(const QString& id, bool floating)
{
    auto* item = m_registry->findWindow(id);
    if (!item || !item->dockWidget)
    {
        return false;
    }

    item->dockWidget->setFloating(floating);
    return true;
}

bool WindowManager::focusWindow(const QString& id)
{
    auto* item = m_registry->findWindow(id);
    if (!item)
    {
        return false;
    }

    showWindow(id);
    if (item->dockWidget && item->dockWidget->widget())
    {
        item->dockWidget->widget()->setFocus();
    }
    else if (item->widget)
    {
        item->widget->setFocus();
    }
    return true;
}

WindowInfo* WindowManager::findWindow(const QString& id)
{
    return m_registry->findWindow(id);
}

const WindowInfo* WindowManager::findWindow(const QString& id) const
{
    return m_registry->findWindow(id);
}

bool WindowManager::hasWindow(const QString& id) const
{
    return m_registry->hasWindow(id);
}

QList<WindowInfo> WindowManager::allWindows() const
{
    return m_registry->allWindows();
}

QStringList WindowManager::categories() const
{
    return m_registry->categories();
}

void WindowManager::resetLayout()
{
    m_layoutManager->resetLayout();
}

void WindowManager::saveLayout()
{
    m_layoutManager->saveToSettings();
}

bool WindowManager::restoreLayout()
{
    return m_layoutManager->restoreFromSettings();
}

bool WindowManager::applyProfile(const QString& name)
{
    return m_layoutManager->applyProfile(name);
}

bool WindowManager::saveProfile(const QString& name)
{
    return m_layoutManager->saveProfile(name);
}

QStringList WindowManager::availableProfiles() const
{
    return m_layoutManager->availableProfiles();
}

QString WindowManager::currentProfile() const
{
    return m_layoutManager->currentProfile();
}

void WindowManager::populateWindowsMenu(QMenu* menu)
{
    if (!menu)
    {
        return;
    }

    menu->clear();
    m_layoutManager->syncAllActionStates();

    // 1. Ajouter les actions de chaque fenêtre enregistrée
    const auto windows = m_registry->allWindows();
    for (const auto& w : windows)
    {
        if (w.action)
        {
            menu->addAction(w.action);
        }
    }

    menu->addSeparator();

    // 2. Sous-menu "Disposition"
    QMenu* layoutSubMenu = menu->addMenu(tr("&Disposition"));
    auto* profileGroup = new QActionGroup(layoutSubMenu);
    profileGroup->setExclusive(true);

    const auto profiles = availableProfiles();
    const QString activeProfile = currentProfile();

    for (const auto& profName : profiles)
    {
        auto* act = layoutSubMenu->addAction(profName);
        act->setCheckable(true);
        act->setChecked(profName == activeProfile);
        profileGroup->addAction(act);

        connect(act, &QAction::triggered, this, [this, profName]() {
            applyProfile(profName);
        });
    }

    layoutSubMenu->addSeparator();

    auto* actSaveProfile = layoutSubMenu->addAction(tr("Enregistrer sous 'Personnalisée'"));
    connect(actSaveProfile, &QAction::triggered, this, [this]() {
        saveProfile("Personnalisée");
    });

    menu->addSeparator();

    // 3. Action "Réinitialiser la disposition"
    auto* actReset = menu->addAction(QIcon(":/icons/edit/undo.svg"), tr("&Réinitialiser la disposition"));
    connect(actReset, &QAction::triggered, this, &WindowManager::resetLayout);

    // Mettre à jour l'état coché à chaque ouverture du menu
    connect(menu, &QMenu::aboutToShow, this, &WindowManager::onMenuAboutToShow, Qt::UniqueConnection);
}

void WindowManager::onMenuAboutToShow()
{
    m_layoutManager->syncAllActionStates();
}

QMenu* WindowManager::createWindowsMenu(QMenuBar* menuBar)
{
    if (!menuBar)
    {
        return nullptr;
    }

    QMenu* menu = menuBar->addMenu(tr("&Fenêtres"));
    populateWindowsMenu(menu);
    return menu;
}

} // namespace TSA::UI
