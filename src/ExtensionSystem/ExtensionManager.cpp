#include "ExtensionManager.h"
#include <algorithm>

namespace TSA::ExtensionSystem
{

ExtensionManager& ExtensionManager::instance()
{
    static ExtensionManager inst;
    return inst;
}

ExtensionManager::ExtensionManager(QObject* parent)
    : QObject(parent)
{
}

void ExtensionManager::initialize(const QString& appDirPath)
{
    LibraryManager::instance().initialize(appDirPath);
    scanExtensions();
}

void ExtensionManager::scanExtensions()
{
    m_discoveredExtensions = LibraryManager::instance().discover();

    for (const auto& ext : m_discoveredExtensions)
    {
        if (ext.kind == ExtensionKind::DataExtension)
        {
            LibraryManager::instance().load(ext.id);
            emit extensionLoaded(QString::fromStdString(ext.id), ExtensionKind::DataExtension);
        }
        else
        {
            // Code Extension (réservé pour les futures DLL de solveurs non-linéaires / plugins)
            emit extensionLoaded(QString::fromStdString(ext.id), ExtensionKind::CodeExtension);
        }
    }

    emit extensionsScanned(static_cast<int>(m_discoveredExtensions.size()));
}

std::vector<ExtensionManifest> ExtensionManager::allExtensions() const
{
    return m_discoveredExtensions;
}

std::vector<ExtensionManifest> ExtensionManager::dataExtensions() const
{
    std::vector<ExtensionManifest> res;
    for (const auto& e : m_discoveredExtensions)
    {
        if (e.kind == ExtensionKind::DataExtension) res.push_back(e);
    }
    return res;
}

std::vector<ExtensionManifest> ExtensionManager::codeExtensions() const
{
    std::vector<ExtensionManifest> res;
    for (const auto& e : m_discoveredExtensions)
    {
        if (e.kind == ExtensionKind::CodeExtension) res.push_back(e);
    }
    return res;
}

bool ExtensionManager::isExtensionInstalled(const std::string& id) const
{
    for (const auto& e : m_discoveredExtensions)
    {
        if (e.id == id) return true;
    }
    return false;
}

} // namespace TSA::ExtensionSystem
