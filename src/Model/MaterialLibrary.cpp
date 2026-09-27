#include "MaterialLibrary.h"
#include "../ExtensionSystem/LibraryRegistry.h"
#include "../ExtensionSystem/DefinitionModels.h"
#include <algorithm>

namespace TSA::Model
{

MaterialLibrary& MaterialLibrary::instance()
{
    static MaterialLibrary inst;
    return inst;
}

MaterialLibrary::MaterialLibrary()
{
    initializeStandards();
}

void MaterialLibrary::initializeStandards()
{
    m_standards = Material::defaultLibrary();
    reloadFromRegistry();
}

const std::vector<Material>& MaterialLibrary::standardMaterials() const
{
    return m_standards;
}

std::vector<Material> MaterialLibrary::allMaterials() const
{
    std::vector<Material> result = m_standards;
    result.insert(result.end(), m_customMaterials.begin(), m_customMaterials.end());
    return result;
}

const Material* MaterialLibrary::findById(int id) const
{
    for (const auto& m : m_customMaterials)
    {
        if (m.id == id) return &m;
    }
    for (const auto& m : m_standards)
    {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const Material* MaterialLibrary::findByName(const std::string& name) const
{
    for (const auto& m : m_customMaterials)
    {
        if (m.name == name) return &m;
    }
    for (const auto& m : m_standards)
    {
        if (m.name == name) return &m;
    }
    return nullptr;
}

const Material* MaterialLibrary::findByType(MaterialType type) const
{
    for (const auto& m : m_customMaterials)
    {
        if (m.type == type) return &m;
    }
    for (const auto& m : m_standards)
    {
        if (m.type == type) return &m;
    }
    return nullptr;
}

bool MaterialLibrary::registerCustomMaterial(const Material& material)
{
    bool updated = false;
    for (auto& m : m_customMaterials)
    {
        if (m.id == material.id || m.name == material.name)
        {
            m = material;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        m_customMaterials.push_back(material);
    }

    // Synchronisation automatique avec LibraryRegistry de l'ExtensionSystem
    auto def = TSA::ExtensionSystem::MaterialDefinition::fromModelMaterial(material, "user.custom");
    TSA::ExtensionSystem::LibraryRegistry::instance().registerMaterial(def);
    return true;
}

bool MaterialLibrary::removeCustomMaterial(int id)
{
    for (auto it = m_customMaterials.begin(); it != m_customMaterials.end(); ++it)
    {
        if (it->id == id)
        {
            m_customMaterials.erase(it);
            return true;
        }
    }
    return false;
}

bool MaterialLibrary::removeCustomMaterialByName(const std::string& name)
{
    for (auto it = m_customMaterials.begin(); it != m_customMaterials.end(); ++it)
    {
        if (it->name == name)
        {
            m_customMaterials.erase(it);
            return true;
        }
    }
    return false;
}

void MaterialLibrary::clearCustomMaterials()
{
    m_customMaterials.clear();
}

void MaterialLibrary::reloadFromRegistry()
{
    auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
    auto extMaterials = registry.allMaterials();
    if (!extMaterials.empty())
    {
        m_standards.clear();
        int nextId = 1;
        for (const auto& matDef : extMaterials)
        {
            m_standards.push_back(matDef.toModelMaterial(nextId++));
        }
    }
}

bool MaterialLibrary::synchronizeToRegistry()
{
    auto& registry = TSA::ExtensionSystem::LibraryRegistry::instance();
    for (const auto& m : m_customMaterials)
    {
        auto def = TSA::ExtensionSystem::MaterialDefinition::fromModelMaterial(m, "user.custom");
        registry.registerMaterial(def);
    }
    return true;
}

} // namespace TSA::Model
