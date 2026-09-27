#pragma once

#include "ExtensionTypes.h"
#include "DefinitionModels.h"
#include <QString>
#include <QJsonObject>
#include <vector>
#include <string>

namespace TSA::ExtensionSystem
{

/**
 * @brief Validateur d'intégrité, de syntaxe et de conformité physique pour TSALib.
 * Empêche tout crash ou instabilité en cas de fichier externe corrompu, incomplet ou incohérent.
 */
class LibraryValidator
{
public:
    LibraryValidator() = default;

    // Validation du manifest.json
    ValidationResult validateManifest(const QJsonObject& manifestJson) const;
    ValidationResult validateManifest(const ExtensionManifest& manifest) const;

    // Validation unitaire des définitions
    ValidationResult validateMaterial(const MaterialDefinition& mat, const QString& libraryBasePath = "") const;
    ValidationResult validateSection(const SectionDefinition& sec, const QString& libraryBasePath = "") const;
    ValidationResult validateCable(const CableCatalogDefinition& cable, const QString& libraryBasePath = "") const;

    // Validation complète d'un répertoire d'extension sur le disque
    ValidationResult validateExtensionDirectory(const QString& extensionDirPath) const;

    // Utilitaires de conformité de format
    static bool isValidId(const std::string& id);
    static bool isSupportedPhysicalUnit(const std::string& unit);
};

} // namespace TSA::ExtensionSystem
