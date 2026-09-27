#pragma once

#include "ExtensionTypes.h"
#include <string>
#include <vector>
#include <map>

namespace TSA::ExtensionSystem
{

/**
 * @brief Gestionnaire et résolveur de graphes de dépendances pour les extensions TSALib.
 */
class LibraryDependencyManager
{
public:
    LibraryDependencyManager() = default;

    // Enregistrement des manifestes d'extensions découvertes
    void registerManifest(const ExtensionManifest& manifest);
    void unregisterManifest(const std::string& extensionId);
    void clear();

    // Vérification globale de la résolution des dépendances
    ValidationResult validateDependencies() const;

    // Calcul de l'ordre de chargement topologique
    std::vector<std::string> computeLoadOrder(ValidationResult* outResult = nullptr) const;

    // Vérification des dépendances pour une extension donnée
    bool areDependenciesSatisfied(const std::string& extensionId, std::vector<std::string>* missingDeps = nullptr) const;

private:
    std::map<std::string, ExtensionManifest> m_manifests;
};

} // namespace TSA::ExtensionSystem
