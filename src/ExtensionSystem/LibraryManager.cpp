#include "LibraryManager.h"
#include "../Model/MaterialLibrary.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>

namespace TSA::ExtensionSystem
{

LibraryManager& LibraryManager::instance()
{
    static LibraryManager inst;
    return inst;
}

LibraryManager::LibraryManager(QObject* parent)
    : QObject(parent)
    , m_loader(&LibraryRegistry::instance())
{
}

void LibraryManager::initialize(const QString& applicationDirPath)
{
    m_searchPaths.clear();

    QString appDir = applicationDirPath;
    if (appDir.isEmpty() && QCoreApplication::instance())
    {
        appDir = QCoreApplication::applicationDirPath();
    }
    if (appDir.isEmpty())
    {
        appDir = QDir::currentPath();
    }

    // 1. Répertoire d'extensions de l'application : ./Extensions/
    addSearchPath(QDir(appDir).filePath("Extensions"));

    // 2. Répertoire d'extensions utilisateur : %APPDATA%/TSA/Extensions
    QString userDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!userDir.isEmpty())
    {
        addSearchPath(QDir(userDir).filePath("Extensions"));
    }

    // 3. Répertoire racine du projet de développement (si présent)
    addSearchPath(QDir::currentPath() + "/Extensions");

    discover();
}

void LibraryManager::addSearchPath(const QString& path)
{
    if (path.isEmpty()) return;
    QDir dir(path);
    QString cleanPath = dir.cleanPath(path);
    if (!m_searchPaths.contains(cleanPath))
    {
        m_searchPaths.append(cleanPath);
    }
}

std::vector<ExtensionManifest> LibraryManager::discover()
{
    std::vector<ExtensionManifest> discovered;

    for (const QString& basePath : m_searchPaths)
    {
        QDir rootDir(basePath);
        if (!rootDir.exists()) continue;

        // Chaque sous-dossier contenant un manifest.json est une extension candidate
        QStringList subDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subDir : subDirs)
        {
            QString extPath = rootDir.filePath(subDir);
            QString manifestFile = QDir(extPath).filePath("manifest.json");
            if (QFile::exists(manifestFile))
            {
                ExtensionManifest manifest;
                ValidationResult res;
                if (m_loader.indexExtension(extPath, &manifest))
                {
                    m_manifests[manifest.id] = manifest;
                    m_extensionPaths[manifest.id] = extPath;
                    m_dependencyManager.registerManifest(manifest);
                    discovered.push_back(manifest);
                }
            }
        }
    }

    emit librariesDiscovered(static_cast<int>(discovered.size()));
    return discovered;
}

bool LibraryManager::load(const std::string& extensionId)
{
    auto it = m_extensionPaths.find(extensionId);
    if (it == m_extensionPaths.end()) return false;

    ExtensionManifest manifest;
    ValidationResult val;
    bool ok = m_loader.loadExtension(it->second, &manifest, &val);
    if (ok)
    {
        m_manifests[extensionId] = manifest;
        TSA::Model::MaterialLibrary::instance().reloadFromRegistry();
        emit libraryLoaded(QString::fromStdString(extensionId));
        emit definitionsChanged();
    }
    return ok;
}

bool LibraryManager::unload(const std::string& extensionId)
{
    bool ok = m_loader.unloadExtension(extensionId);
    if (ok)
    {
        TSA::Model::MaterialLibrary::instance().reloadFromRegistry();
        emit libraryUnloaded(QString::fromStdString(extensionId));
        emit definitionsChanged();
    }
    return ok;
}

bool LibraryManager::reload(const std::string& extensionId)
{
    unload(extensionId);
    bool ok = load(extensionId);
    if (ok)
    {
        emit libraryReloaded(QString::fromStdString(extensionId));
    }
    return ok;
}

void LibraryManager::reloadAll()
{
    for (const auto& [id, _] : m_extensionPaths)
    {
        reload(id);
    }
    emit definitionsChanged();
}

bool LibraryManager::reloadFile(const QString& filePath)
{
    bool ok = m_loader.reloadFile(filePath);
    if (ok)
    {
        emit definitionsChanged();
    }
    return ok;
}

ValidationResult LibraryManager::validate(const std::string& extensionId) const
{
    auto it = m_extensionPaths.find(extensionId);
    if (it == m_extensionPaths.end())
    {
        ValidationResult r;
        r.addError("Extension non trouvée : " + extensionId);
        return r;
    }
    return m_validator.validateExtensionDirectory(it->second);
}

ValidationResult LibraryManager::validateAll() const
{
    ValidationResult total;
    for (const auto& [id, path] : m_extensionPaths)
    {
        ValidationResult r = m_validator.validateExtensionDirectory(path);
        for (const auto& err : r.errors) total.addError("[" + id + "] " + err);
        for (const auto& warn : r.warnings) total.addWarning("[" + id + "] " + warn);
    }

    ValidationResult depRes = m_dependencyManager.validateDependencies();
    for (const auto& err : depRes.errors) total.addError(err);
    for (const auto& warn : depRes.warnings) total.addWarning(warn);

    return total;
}

std::vector<ExtensionManifest> LibraryManager::installedExtensions() const
{
    std::vector<ExtensionManifest> res;
    res.reserve(m_manifests.size());
    for (const auto& [_, man] : m_manifests)
    {
        res.push_back(man);
    }
    return res;
}

} // namespace TSA::ExtensionSystem
