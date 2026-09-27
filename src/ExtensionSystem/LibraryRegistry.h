#pragma once

#include "DefinitionModels.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

namespace TSA::ExtensionSystem
{

/**
 * @brief Registre logique central des définitions d'ingénierie disponibles dans TSA.
 * Indépendant des chemins physiques de stockage sur le disque.
 * Fournit un accès instantané par ID logique (ex: "concrete.c25_30", "steel.ipe200").
 */
class LibraryRegistry
{
public:
    static LibraryRegistry& instance();

    // Enregistrement
    bool registerMaterial(const MaterialDefinition& mat);
    bool registerSection(const SectionDefinition& sec);
    bool registerCable(const CableCatalogDefinition& cable);

    // Recherches par ID logique
    const MaterialDefinition* findMaterial(const std::string& id) const;
    const SectionDefinition* findSection(const std::string& id) const;
    const CableCatalogDefinition* findCable(const std::string& id) const;

    // Récupération globale
    std::vector<MaterialDefinition> allMaterials() const;
    std::vector<SectionDefinition> allSections() const;
    std::vector<CableCatalogDefinition> allCables() const;

    std::vector<MaterialDefinition> materials() const { return allMaterials(); }
    std::vector<SectionDefinition> sections() const { return allSections(); }
    std::vector<CableCatalogDefinition> cables() const { return allCables(); }

    // Filtrage par catégorie
    std::vector<MaterialDefinition> materialsByCategory(const std::string& category) const;
    std::vector<SectionDefinition> sectionsByCategory(const std::string& category) const;
    std::vector<CableCatalogDefinition> cablesByCategory(const std::string& category) const;

    // Recherche textuelle multi-champs (ID, Nom, Norme, Catégorie)
    std::vector<MaterialDefinition> searchMaterials(const std::string& query) const;
    std::vector<SectionDefinition> searchSections(const std::string& query) const;
    std::vector<CableCatalogDefinition> searchCables(const std::string& query) const;

    // Gestion du cycle de vie des extensions
    void removeDefinitionsByLibraryId(const std::string& libraryId);
    void clear();

    size_t materialCount() const;
    size_t sectionCount() const;
    size_t cableCount() const;

public:
    LibraryRegistry() = default;
    ~LibraryRegistry() = default;

    LibraryRegistry(const LibraryRegistry&) = delete;
    LibraryRegistry& operator=(const LibraryRegistry&) = delete;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, MaterialDefinition> m_materials;
    std::unordered_map<std::string, SectionDefinition> m_sections;
    std::unordered_map<std::string, CableCatalogDefinition> m_cables;
};

} // namespace TSA::ExtensionSystem
