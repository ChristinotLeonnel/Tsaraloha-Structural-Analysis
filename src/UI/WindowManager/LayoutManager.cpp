#include "LayoutManager.h"
#include <QSettings>
#include <QDockWidget>

namespace TSA::UI
{

LayoutManager::LayoutManager(QMainWindow* mainWindow, WindowRegistry* registry, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_registry(registry)
{
}

void LayoutManager::resetLayout()
{
    if (!m_mainWindow || !m_registry)
    {
        return;
    }

    const auto windows = m_registry->allWindows();

    // 1. Replacer chaque dock dans sa zone par défaut et réinitialiser son état flottant et visible
    for (const auto& info : windows)
    {
        if (info.dockWidget)
        {
            info.dockWidget->setFloating(info.defaultFloating);
            if (info.defaultArea != Qt::NoDockWidgetArea)
            {
                m_mainWindow->addDockWidget(info.defaultArea, info.dockWidget);
            }
            info.dockWidget->setVisible(info.defaultVisible);
        }
        else if (info.widget && info.id != "viewport")
        {
            info.widget->setVisible(info.defaultVisible);
        }
    }

    // 2. Ongletiser les panneaux gauches : ModelTree + Visibility + Elements
    auto* modelTree = m_registry->findWindow("model_browser");
    auto* visibility = m_registry->findWindow("visibility");
    auto* elements = m_registry->findWindow("elements");

    if (modelTree && modelTree->dockWidget && visibility && visibility->dockWidget)
    {
        m_mainWindow->tabifyDockWidget(modelTree->dockWidget, visibility->dockWidget);
    }
    if (modelTree && modelTree->dockWidget && elements && elements->dockWidget)
    {
        m_mainWindow->tabifyDockWidget(modelTree->dockWidget, elements->dockWidget);
    }
    if (modelTree && modelTree->dockWidget)
    {
        modelTree->dockWidget->raise();
    }

    // 3. Ongletiser les panneaux droits : Properties + WorkPlanes (ProjectionView)
    auto* properties = m_registry->findWindow("properties");
    auto* workPlanes = m_registry->findWindow("work_planes");

    if (properties && properties->dockWidget && workPlanes && workPlanes->dockWidget)
    {
        m_mainWindow->tabifyDockWidget(properties->dockWidget, workPlanes->dockWidget);
    }
    if (properties && properties->dockWidget)
    {
        properties->dockWidget->raise();
    }

    // 4. Panneau inférieur : Console
    auto* console = m_registry->findWindow("console");
    if (console && console->dockWidget)
    {
        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, console->dockWidget);
        console->dockWidget->setVisible(console->defaultVisible);
    }

    // 5. Mettre l'accent sur le viewport 3D central
    auto* viewport = m_registry->findWindow("viewport");
    if (viewport && viewport->widget)
    {
        viewport->widget->setFocus();
    }

    m_currentProfile = "Modélisation";
    syncAllActionStates();

    emit layoutReset();
}

QByteArray LayoutManager::saveState() const
{
    if (!m_mainWindow)
    {
        return QByteArray();
    }
    return m_mainWindow->saveState(LayoutVersion);
}

bool LayoutManager::restoreState(const QByteArray& state)
{
    if (!m_mainWindow || state.isEmpty())
    {
        return false;
    }

    const bool ok = m_mainWindow->restoreState(state, LayoutVersion);
    if (ok)
    {
        syncAllActionStates();
        emit layoutRestored();
    }
    return ok;
}

void LayoutManager::saveToSettings(const QString& group)
{
    if (!m_mainWindow)
    {
        return;
    }

    QSettings settings("Tsaraloha", "TSA");
    settings.beginGroup(group);
    settings.setValue("version", LayoutVersion);
    settings.setValue("geometry", m_mainWindow->saveGeometry());
    settings.setValue("windowState", m_mainWindow->saveState(LayoutVersion));
    settings.setValue("currentProfile", m_currentProfile);

    settings.beginGroup("profiles");
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it)
    {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    settings.endGroup();
    emit layoutSaved();
}

bool LayoutManager::restoreFromSettings(const QString& group)
{
    if (!m_mainWindow)
    {
        return false;
    }

    QSettings settings("Tsaraloha", "TSA");
    settings.beginGroup(group);

    const int version = settings.value("version", 0).toInt();
    if (version != LayoutVersion)
    {
        settings.endGroup();
        resetLayout();
        return false;
    }

    const QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty())
    {
        m_mainWindow->restoreGeometry(geometry);
    }

    const QByteArray windowState = settings.value("windowState").toByteArray();
    bool stateRestored = false;
    if (!windowState.isEmpty())
    {
        stateRestored = m_mainWindow->restoreState(windowState, LayoutVersion);
    }

    m_currentProfile = settings.value("currentProfile", "Modélisation").toString();

    settings.beginGroup("profiles");
    const QStringList keys = settings.childKeys();
    for (const auto& key : keys)
    {
        m_profiles.insert(key, settings.value(key).toByteArray());
    }
    settings.endGroup();

    settings.endGroup();

    if (!stateRestored)
    {
        resetLayout();
        return false;
    }

    syncAllActionStates();
    emit layoutRestored();
    return true;
}

bool LayoutManager::saveProfile(const QString& name)
{
    if (!m_mainWindow || name.trimmed().isEmpty())
    {
        return false;
    }

    const QByteArray state = m_mainWindow->saveState(LayoutVersion);
    m_profiles.insert(name, state);
    m_currentProfile = name;
    emit profileSaved(name);
    return true;
}

bool LayoutManager::applyProfile(const QString& name)
{
    if (!m_mainWindow)
    {
        return false;
    }

    if (m_profiles.contains(name))
    {
        const bool ok = m_mainWindow->restoreState(m_profiles.value(name), LayoutVersion);
        if (ok)
        {
            m_currentProfile = name;
            syncAllActionStates();
            emit profileApplied(name);
            return true;
        }
    }

    // Si le profil n'a pas encore été sauvegardé explicitement, appliquer le profil prédéfini
    applyPresetProfile(name);
    m_currentProfile = name;
    syncAllActionStates();
    emit profileApplied(name);
    return true;
}

void LayoutManager::applyPresetProfile(const QString& name)
{
    if (!m_mainWindow || !m_registry)
    {
        return;
    }

    if (name == "Modélisation")
    {
        resetLayout();
        return;
    }

    // Réinitialiser d'abord la base
    resetLayout();

    auto* modelTree = m_registry->findWindow("model_browser");
    auto* visibility = m_registry->findWindow("visibility");
    auto* elements = m_registry->findWindow("elements");
    auto* properties = m_registry->findWindow("properties");
    auto* workPlanes = m_registry->findWindow("work_planes");
    auto* console = m_registry->findWindow("console");

    if (name == "Analyse")
    {
        // Focus sur l'arbre, les propriétés et la console de calculs
        if (elements && elements->dockWidget) elements->dockWidget->hide();
        if (visibility && visibility->dockWidget) visibility->dockWidget->hide();
        if (modelTree && modelTree->dockWidget) modelTree->dockWidget->show();
        if (properties && properties->dockWidget) properties->dockWidget->show();
        if (console && console->dockWidget)
        {
            console->dockWidget->show();
            console->dockWidget->raise();
        }
    }
    else if (name == "Résultats")
    {
        // Focus sur la vue 3D, les propriétés, les plans de travail et la console
        if (elements && elements->dockWidget) elements->dockWidget->hide();
        if (modelTree && modelTree->dockWidget) modelTree->dockWidget->show();
        if (workPlanes && workPlanes->dockWidget)
        {
            workPlanes->dockWidget->show();
            workPlanes->dockWidget->raise();
        }
        if (properties && properties->dockWidget) properties->dockWidget->show();
        if (console && console->dockWidget) console->dockWidget->show();
    }
    else if (name == "Détaillage")
    {
        // Focus sur le volet de dessin d'éléments et les propriétés détaillées
        if (console && console->dockWidget) console->dockWidget->hide();
        if (elements && elements->dockWidget)
        {
            elements->dockWidget->show();
            elements->dockWidget->raise();
        }
        if (properties && properties->dockWidget) properties->dockWidget->show();
        if (modelTree && modelTree->dockWidget) modelTree->dockWidget->show();
    }

    // Mémoriser cet état par défaut pour ce profil
    m_profiles.insert(name, m_mainWindow->saveState(LayoutVersion));
}

bool LayoutManager::hasProfile(const QString& name) const
{
    return m_profiles.contains(name) || availableProfiles().contains(name);
}

QStringList LayoutManager::availableProfiles() const
{
    return {"Modélisation", "Analyse", "Résultats", "Détaillage", "Personnalisée"};
}

QString LayoutManager::currentProfile() const
{
    return m_currentProfile;
}

void LayoutManager::syncAllActionStates()
{
    if (!m_registry)
    {
        return;
    }

    for (const auto& info : m_registry->allWindows())
    {
        if (info.action)
        {
            const bool visible = info.isVisible();
            if (info.action->isChecked() != visible)
            {
                const QSignalBlocker blocker(info.action);
                info.action->setChecked(visible);
            }
        }
    }
}

} // namespace TSA::UI
