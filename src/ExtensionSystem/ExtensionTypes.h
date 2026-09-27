#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace TSA::ExtensionSystem
{

/**
 * @brief Type d'extension supportée par le système TSA.
 * Séparation stricte entre les extensions de données (Data) et les extensions de code (Code).
 */
enum class ExtensionKind
{
    DataExtension, // TSALib, SteelProfiles, CableLibrary (JSON, textures, metadata - AUCUNE recompilation)
    CodeExtension  // Solvers avancés, modules non-linéaires, plugins DLL compilés
};

/**
 * @brief Catégories fonctionnelles des définitions de bibliothèques d'ingénierie.
 */
enum class DefinitionCategory
{
    Materials,      // Matériaux (Béton, Acier, Bois, Sol, Maçonnerie, Verre, Aluminium)
    Sections,       // Sections transversales de barres et éléments
    Profiles,       // Catalogues de profilés standardisés (IPE, HEA, HEB, UPN, Cornières, Tubes)
    Cables,         // Câbles, torons, haubans et suspentes
    Reinforcement,  // Armatures passives (Rebars)
    Prestressing,   // Câbles et torons de précontrainte
    Anchors,        // Ancrages et dispositifs d'extrémité
    Bearings,       // Appareils d'appui
    Connections,    // Assemblages métalliques et liaisons
    Loads,          // Cas et combinaisons de charges types
    Standards,      // Référentiels normatifs (Eurocodes EN 1990 à 1999, etc.)
    Components,     // Composants et sous-structures paramétriques
    Textures,       // Textures visuelles de rendu physique (Albedo, Normal, Roughness)
    Manufacturers   // Catalogues de fabricants certifiés
};

/**
 * @brief Gestionnaire de version sémantique (SemVer 2.0).
 */
struct SemanticVersion
{
    int major = 1;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    SemanticVersion() = default;
    SemanticVersion(int maj, int min, int pat, const std::string& pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre) {}

    static std::optional<SemanticVersion> fromString(const std::string& str);
    std::string toString() const;

    bool operator==(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const { return *this < other || *this == other; }
    bool operator>(const SemanticVersion& other) const { return !(*this <= other); }
    bool operator>=(const SemanticVersion& other) const { return !(*this < other); }
};

/**
 * @brief Valeur physique avec unité explicite pour garantir la traçabilité et la conversion sans ambiguïté.
 */
struct PhysicalValue
{
    double value = 0.0;
    std::string unit; // ex: "MPa", "GPa", "Pa", "kg/m3", "kN", "N", "m", "mm", "deg"

    PhysicalValue() = default;
    PhysicalValue(double v, const std::string& u) : value(v), unit(u) {}

    // Conversion normalisée vers le Système International (SI) de base de TSA (m, kg, s, N, Pa)
    double toBaseSI() const;

    static PhysicalValue fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;
};

/**
 * @brief Référence versionnée vers une définition de bibliothèque.
 * Stockée dans les éléments du modèle structural et les fichiers de projet (.tsa).
 */
struct DefinitionReference
{
    std::string libraryId;          // ex: "org.tsaraloha.tsalib"
    SemanticVersion libraryVersion; // ex: "1.0.0"
    std::string definitionId;       // ex: "concrete.c25_30" ou "steel.ipe200"
    SemanticVersion definitionVersion; // ex: "1.0"

    bool isValid() const { return !libraryId.empty() && !definitionId.empty(); }
    std::string toQualifiedKey() const { return libraryId + ":" + definitionId; }
};

/**
 * @brief Snapshot immuable des caractéristiques mécaniques au moment de l'affectation à un élément.
 * Garantit la reproductibilité absolue des calculs d'un ancien projet même si la bibliothèque externe évolue.
 */
struct MechanicalSnapshot
{
    double youngModulus = 0.0;     // Pa
    double poissonRatio = 0.0;     // sans unité
    double density = 0.0;          // kg/m³
    double characteristicStrength = 0.0; // Pa (fck, fpk, fk)
    double yieldStrength = 0.0;    // Pa (fy)
    double thermalCoeff = 0.0;     // 1/K

    bool operator==(const MechanicalSnapshot& other) const;
    bool operator!=(const MechanicalSnapshot& other) const { return !(*this == other); }
};

/**
 * @brief Dépendance déclarée par une extension.
 */
struct ExtensionDependency
{
    std::string id;                  // ex: "org.tsaraloha.standards"
    SemanticVersion minimumVersion;  // ex: "1.0.0"
    bool optional = false;
};

/**
 * @brief Manifeste officiel d'une extension TSA (manifest.json).
 */
struct ExtensionManifest
{
    std::string id;                     // Identifiant unique inverse (ex: "org.tsaraloha.tsalib")
    std::string name;                   // Nom lisible (ex: "TSA Engineering Library")
    SemanticVersion version;            // Version du package (ex: 1.0.0)
    std::string formatVersion = "1.0";  // Version du format de manifest TSA
    SemanticVersion minimumTsaVersion;  // Version minimale de TSA requise
    std::string author;                 // Auteur (ex: "Tsaraloha Christinot")
    std::string license = "Proprietary";// Licence
    std::string description;            // Description détaillée
    std::string website;                // Site web ou documentation
    ExtensionKind kind = ExtensionKind::DataExtension;

    std::vector<std::string> categories;// Catégories fournies
    std::vector<ExtensionDependency> dependencies; // Dépendances
    std::string checksum;               // Somme de contrôle SHA256 / CRC32 optionnelle

    static std::optional<ExtensionManifest> fromJson(const QJsonObject& json, std::string* outError = nullptr);
    QJsonObject toJson() const;
};

/**
 * @brief Résultat de validation d'une bibliothèque ou définition.
 */
struct ValidationResult
{
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void addError(const std::string& err)
    {
        valid = false;
        errors.push_back(err);
    }

    void addWarning(const std::string& warn)
    {
        warnings.push_back(warn);
    }
};

} // namespace TSA::ExtensionSystem
