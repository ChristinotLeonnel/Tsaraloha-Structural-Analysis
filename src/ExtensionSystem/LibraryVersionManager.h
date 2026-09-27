#pragma once

#include "ExtensionTypes.h"
#include "DefinitionModels.h"
#include <string>
#include <vector>

namespace TSA::ExtensionSystem
{

/**
 * @brief Différence détectée sur une propriété physique entre deux versions d'une définition.
 */
struct PropertyDiff
{
    std::string propertyName;
    double oldValue = 0.0;
    double newValue = 0.0;
    std::string unit;
    bool isModified = false;
};

/**
 * @brief Rapport complet de comparaison de versions pour l'arbitrage utilisateur (Conserver / Mettre à jour / Comparer).
 */
struct VersionComparisonResult
{
    std::string definitionId;
    SemanticVersion oldVersion;
    SemanticVersion newVersion;

    std::vector<PropertyDiff> modifiedProperties;
    std::vector<std::string> unchangedProperties;

    bool hasMechanicalChanges() const { return !modifiedProperties.empty(); }
};

/**
 * @brief Gestionnaire de versions et comparateur sémantique pour TSALib.
 * Empêche toute altération silencieuse des résultats de calcul d'un ancien projet.
 */
class LibraryVersionManager
{
public:
    LibraryVersionManager() = default;

    // Comparaison détaillée entre un snapshot de projet et une nouvelle définition
    VersionComparisonResult compare(const MechanicalSnapshot& projectSnapshot,
                                    const SemanticVersion& projectVersion,
                                    const MaterialDefinition& libraryDefinition) const;

    VersionComparisonResult compareCable(const MechanicalSnapshot& projectSnapshot,
                                         const SemanticVersion& projectVersion,
                                         const CableCatalogDefinition& libraryDefinition) const;

    // Vérification de compatibilité de version (Majeure / Mineure)
    bool isCompatible(const SemanticVersion& requiredVersion, const SemanticVersion& availableVersion) const;
};

} // namespace TSA::ExtensionSystem
