#pragma once

#include <QString>
#include <QJsonObject>
#include <string>
#include <vector>
#include "ExtensionTypes.h"
#include "DefinitionModels.h"

namespace TSA::ExtensionSystem
{

/**
 * @brief Options de configuration pour l'échafaudage d'une nouvelle extension TSALib.
 */
struct ExtensionTemplateOptions
{
    std::string id;                          ///< Identifiant unique (ex: "org.eurocode.timber")
    std::string name;                        ///< Nom complet (ex: "Eurocode 5 - Bois et Lamellé-Collé")
    SemanticVersion version{ 1, 0, 0 };      ///< Version sémantique initiale
    std::string author;                      ///< Auteur ou organisation
    std::string description;                 ///< Description détaillée
    std::string license = "MIT";             ///< Licence (MIT, Proprietary, etc.)
    std::string website = "";                ///< Site web ou documentation
    ExtensionKind kind = ExtensionKind::DataExtension;

    // Catégories à initialiser dans l'arborescence
    bool includeMaterials = true;
    bool includeSections = true;
    bool includeProfiles = true;
    bool includeCables = false;
    bool includeTextures = true;
    bool includeStandards = true;

    // Répertoire cible (si vide, calculé automatiquement dans le répertoire Extensions)
    QString targetDirectory;

    // Packaging automatique
    bool createPackage = false;
    QString packageOutputPath;
};

/**
 * @brief Moteur d'échafaudage et de génération de bibliothèques TSALib.
 * Permet la création en 1 clic de bibliothèques complètes, de manifests et de fiches types.
 */
class ExtensionScaffolder
{
public:
    /**
     * @brief Génère l'arborescence complète et les fiches types d'une nouvelle bibliothèque TSALib.
     * @param options Options de génération
     * @param outDirectory Chemin du répertoire créé (en sortie)
     * @param outError Message d'erreur détaillé en cas d'échec
     * @return true si la création a réussi avec succès
     */
    static bool scaffold(const ExtensionTemplateOptions& options,
                         QString* outDirectory = nullptr,
                         QString* outError = nullptr);

    /**
     * @brief Ajoute une fiche de matériau au format JSON dans une extension.
     */
    static bool createMaterialFile(const QString& extensionDir,
                                  const QString& filename,
                                  const MaterialDefinition& material,
                                  QString* outError = nullptr);

    /**
     * @brief Ajoute une fiche de section ou profilé au format JSON dans une extension.
     */
    static bool createSectionFile(const QString& extensionDir,
                                 const QString& filename,
                                 const SectionDefinition& section,
                                 bool isProfile = false,
                                 QString* outError = nullptr);

    /**
     * @brief Ajoute une fiche de câble au format JSON dans une extension.
     */
    static bool createCableFile(const QString& extensionDir,
                               const QString& filename,
                               const CableCatalogDefinition& cable,
                               QString* outError = nullptr);

    /**
     * @brief Ajoute une référence normative au format JSON dans une extension.
     */
    static bool createStandardFile(const QString& extensionDir,
                                  const QString& filename,
                                  const QString& stdId,
                                  const QString& stdName,
                                  const QString& edition,
                                  const QString& scope,
                                  QString* outError = nullptr);
};

} // namespace TSA::ExtensionSystem
