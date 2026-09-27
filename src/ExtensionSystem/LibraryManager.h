#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <string>
#include <memory>

#include "ExtensionTypes.h"
#include "DefinitionModels.h"
#include "LibraryRegistry.h"
#include "LibraryLoader.h"
#include "LibraryValidator.h"
#include "LibraryCache.h"
#include "LibraryVersionManager.h"
#include "LibraryDependencyManager.h"

namespace TSA::ExtensionSystem
{

/**
 * @brief Contrôleur central du système de bibliothèques d'ingénierie TSALib.
 * Gère la découverte automatique, le chargement, la validation, le cache,
 * et le rechargement à chaud sans redémarrage ni recompilation de TSA.
 */
class LibraryManager : public QObject
{
    Q_OBJECT

public:
    static LibraryManager& instance();

    // Initialisation générale
    void initialize(const QString& applicationDirPath = "");

    // Chemins de recherche d'extensions
    void addSearchPath(const QString& path);
    QStringList searchPaths() const { return m_searchPaths; }

    // Découverte et cycle de vie
    std::vector<ExtensionManifest> discover();
    bool load(const std::string& extensionId);
    bool unload(const std::string& extensionId);
    bool reload(const std::string& extensionId);
    void reloadAll();
    bool reloadFile(const QString& filePath);

    // Validation
    ValidationResult validate(const std::string& extensionId) const;
    ValidationResult validateAll() const;

    // Accès aux sous-systèmes
    LibraryRegistry& registry() { return LibraryRegistry::instance(); }
    const LibraryRegistry& registry() const { return LibraryRegistry::instance(); }

    LibraryLoader& loader() { return m_loader; }
    LibraryValidator& validator() { return m_validator; }
    LibraryCache& cache() { return m_cache; }
    LibraryVersionManager& versionManager() { return m_versionManager; }
    LibraryDependencyManager& dependencyManager() { return m_dependencyManager; }

    // Liste des manifests installés
    std::vector<ExtensionManifest> installedExtensions() const;

    // Mode de chargement paresseux (Lazy Loading On-Demand)
    void setLazyLoadingEnabled(bool enabled) { m_lazyLoadingEnabled = enabled; }
    bool isLazyLoadingEnabled() const { return m_lazyLoadingEnabled; }

    bool loadOnDemand(const std::string& id);
    bool isIndexed(const std::string& id) const;
    bool isLoaded(const std::string& id) const;
    size_t indexedDefinitionsCount() const;

    // Recherches rapides déléguées (avec chargement à la demande si lazy loading actif)
    const MaterialDefinition* findMaterial(const std::string& id) const;
    const SectionDefinition* findSection(const std::string& id) const;
    const CableCatalogDefinition* findCable(const std::string& id) const;

    // Gestion des packages .tsalib
    QString extensionPath(const std::string& extensionId) const;
    bool installPackage(const QString& packagePath, QString* outError = nullptr);
    bool exportPackage(const std::string& extensionId, const QString& outputPackagePath, QString* outError = nullptr);

signals:
    void librariesDiscovered(int count);
    void libraryLoaded(const QString& extensionId);
    void libraryUnloaded(const QString& extensionId);
    void libraryReloaded(const QString& extensionId);
    void definitionsChanged();

public:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override = default;

    LibraryManager(const LibraryManager&) = delete;
    LibraryManager& operator=(const LibraryManager&) = delete;

private:
    QStringList m_searchPaths;
    LibraryLoader m_loader;
    LibraryValidator m_validator;
    LibraryCache m_cache;
    LibraryVersionManager m_versionManager;
    LibraryDependencyManager m_dependencyManager;

    std::map<std::string, ExtensionManifest> m_manifests;
    std::map<std::string, QString> m_extensionPaths; // id -> directoryPath
    bool m_lazyLoadingEnabled = true;
};

} // namespace TSA::ExtensionSystem
