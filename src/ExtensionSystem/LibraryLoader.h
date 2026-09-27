#pragma once

#include "ExtensionTypes.h"
#include "DefinitionModels.h"
#include "LibraryValidator.h"
#include <QString>
#include <vector>
#include <string>
#include <map>

namespace TSA::ExtensionSystem
{

class LibraryRegistry;

/**
 * @brief Index d'un fichier de définition présent sur le disque pour le Lazy Loading.
 */
struct DefinitionFileIndex
{
    std::string id;
    std::string category;
    QString filePath;
    bool isLoaded = false;
};

/**
 * @brief Moteur de chargement et d'indexation des bibliothèques JSON de TSALib.
 * Gère le chargement paresseux (lazy loading), le rechargement à chaud et la validation.
 */
class LibraryLoader
{
public:
    explicit LibraryLoader(LibraryRegistry* registry = nullptr);

    void setRegistry(LibraryRegistry* registry) { m_registry = registry; }

    // Chargement complet d'une extension depuis son répertoire
    bool loadExtension(const QString& extensionDirPath, ExtensionManifest* outManifest = nullptr, ValidationResult* outResult = nullptr);

    // Indexation rapide des métadonnées (démarrage ultra-rapide sans charger tout le contenu)
    bool indexExtension(const QString& extensionDirPath, ExtensionManifest* outManifest = nullptr);

    // Chargement ciblé à la demande (On-demand / Lazy loading)
    bool loadDefinitionById(const std::string& id);

    // Rechargement d'un fichier spécifique modifié à chaud
    bool reloadFile(const QString& filePath);

    // Déchargement d'une extension par son ID
    bool unloadExtension(const std::string& extensionId);

    // Chargement unitaire depuis fichier JSON
    std::optional<MaterialDefinition> loadMaterialFile(const QString& filePath, ValidationResult* outResult = nullptr);
    std::optional<SectionDefinition> loadSectionFile(const QString& filePath, ValidationResult* outResult = nullptr);
    std::optional<CableCatalogDefinition> loadCableFile(const QString& filePath, ValidationResult* outResult = nullptr);

    const std::map<std::string, DefinitionFileIndex>& index() const { return m_fileIndex; }

private:
    void scanCategoryDirectory(const QString& dirPath, const std::string& category, const std::string& libraryId);

private:
    LibraryRegistry* m_registry = nullptr;
    LibraryValidator m_validator;
    std::map<std::string, DefinitionFileIndex> m_fileIndex; // definitionId -> DefinitionFileIndex
    std::map<std::string, QString> m_loadedExtensions;     // libraryId -> extensionDirPath
};

} // namespace TSA::ExtensionSystem
