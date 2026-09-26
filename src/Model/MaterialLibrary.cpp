#include "MaterialLibrary.h"
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
    for (auto& m : m_customMaterials)
    {
        if (m.id == material.id || m.name == material.name)
        {
            m = material;
            return true;
        }
    }
    m_customMaterials.push_back(material);
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

} // namespace TSA::Model
