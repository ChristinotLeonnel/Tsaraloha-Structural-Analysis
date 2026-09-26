#pragma once

#include "Material.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace TSA::Model
{

/**
 * @brief Bibliothèque centrale de gestion des matériaux de TSA.
 * Fournit un registre partagé, les matériaux standards Eurocodes/Génie Civil,
 * et le support des matériaux personnalisés utilisateurs.
 */
class MaterialLibrary
{
public:
    static MaterialLibrary& instance();

    // Matériaux standards intégrés
    const std::vector<Material>& standardMaterials() const;

    // Matériaux personnalisés enregistrés à l'exécution
    const std::vector<Material>& customMaterials() const { return m_customMaterials; }

    // Tous les matériaux (standards + personnalisés)
    std::vector<Material> allMaterials() const;

    // Recherches
    const Material* findById(int id) const;
    const Material* findByName(const std::string& name) const;
    const Material* findByType(MaterialType type) const;

    // Enregistrement et gestion des matériaux personnalisés
    bool registerCustomMaterial(const Material& material);
    bool removeCustomMaterial(int id);
    bool removeCustomMaterialByName(const std::string& name);
    void clearCustomMaterials();

private:
    MaterialLibrary();
    ~MaterialLibrary() = default;
    MaterialLibrary(const MaterialLibrary&) = delete;
    MaterialLibrary& operator=(const MaterialLibrary&) = delete;

    void initializeStandards();

private:
    std::vector<Material> m_standards;
    std::vector<Material> m_customMaterials;
};

} // namespace TSA::Model
