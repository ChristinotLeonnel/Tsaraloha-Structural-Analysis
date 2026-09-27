#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <string>
#include <memory>

#include "ExtensionTypes.h"
#include "LibraryManager.h"

namespace TSA::ExtensionSystem
{

/**
 * @brief Gestionnaire de plus haut niveau pour l'ensemble des extensions TSA.
 * Assure la séparation étanche entre Data Extensions (TSALib) et Code Extensions (Plugins DLL).
 */
class ExtensionManager : public QObject
{
    Q_OBJECT

public:
    static ExtensionManager& instance();

    // Initialisation
    void initialize(const QString& appDirPath = "");

    // Découverte globale
    void scanExtensions();

    // Accès aux sous-systèmes
    LibraryManager& libraryManager() { return LibraryManager::instance(); }

    // Listes d'extensions découvertes
    std::vector<ExtensionManifest> allExtensions() const;
    std::vector<ExtensionManifest> dataExtensions() const;
    std::vector<ExtensionManifest> codeExtensions() const;

    // Statut
    bool isExtensionInstalled(const std::string& id) const;

signals:
    void extensionsScanned(int totalCount);
    void extensionLoaded(const QString& id, ExtensionKind kind);
    void extensionError(const QString& id, const QString& error);

public:
    explicit ExtensionManager(QObject* parent = nullptr);
    ~ExtensionManager() override = default;

    ExtensionManager(const ExtensionManager&) = delete;
    ExtensionManager& operator=(const ExtensionManager&) = delete;

private:
    std::vector<ExtensionManifest> m_discoveredExtensions;
};

} // namespace TSA::ExtensionSystem
