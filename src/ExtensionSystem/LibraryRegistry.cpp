#include "LibraryRegistry.h"
#include <algorithm>
#include <cctype>

namespace TSA::ExtensionSystem
{

static std::string toLower(const std::string& str)
{
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

LibraryRegistry& LibraryRegistry::instance()
{
    static LibraryRegistry inst;
    return inst;
}

bool LibraryRegistry::registerMaterial(const MaterialDefinition& mat)
{
    if (mat.id.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_materials[mat.id] = mat;
    return true;
}

bool LibraryRegistry::registerSection(const SectionDefinition& sec)
{
    if (sec.id.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sections[sec.id] = sec;
    return true;
}

bool LibraryRegistry::registerCable(const CableCatalogDefinition& cable)
{
    if (cable.id.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cables[cable.id] = cable;
    return true;
}

const MaterialDefinition* LibraryRegistry::findMaterial(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_materials.find(id);
    if (it != m_materials.end()) return &it->second;

    std::string lId = toLower(id);
    for (const auto& [k, mat] : m_materials)
    {
        if (toLower(k) == lId || toLower(mat.id) == lId || toLower(mat.name) == lId)
        {
            return &mat;
        }
    }
    return nullptr;
}

const SectionDefinition* LibraryRegistry::findSection(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sections.find(id);
    if (it != m_sections.end()) return &it->second;
    return nullptr;
}

const CableCatalogDefinition* LibraryRegistry::findCable(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cables.find(id);
    if (it != m_cables.end()) return &it->second;
    return nullptr;
}

std::vector<MaterialDefinition> LibraryRegistry::allMaterials() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MaterialDefinition> result;
    result.reserve(m_materials.size());
    for (const auto& [_, mat] : m_materials)
    {
        result.push_back(mat);
    }
    return result;
}

std::vector<SectionDefinition> LibraryRegistry::allSections() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SectionDefinition> result;
    result.reserve(m_sections.size());
    for (const auto& [_, sec] : m_sections)
    {
        result.push_back(sec);
    }
    return result;
}

std::vector<CableCatalogDefinition> LibraryRegistry::allCables() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CableCatalogDefinition> result;
    result.reserve(m_cables.size());
    for (const auto& [_, cab] : m_cables)
    {
        result.push_back(cab);
    }
    return result;
}

std::vector<MaterialDefinition> LibraryRegistry::materialsByCategory(const std::string& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MaterialDefinition> result;
    std::string lCat = toLower(category);
    for (const auto& [_, mat] : m_materials)
    {
        if (toLower(mat.category) == lCat)
        {
            result.push_back(mat);
        }
    }
    return result;
}

std::vector<SectionDefinition> LibraryRegistry::sectionsByCategory(const std::string& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SectionDefinition> result;
    std::string lCat = toLower(category);
    for (const auto& [_, sec] : m_sections)
    {
        if (toLower(sec.category) == lCat)
        {
            result.push_back(sec);
        }
    }
    return result;
}

std::vector<CableCatalogDefinition> LibraryRegistry::cablesByCategory(const std::string& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CableCatalogDefinition> result;
    std::string lCat = toLower(category);
    for (const auto& [_, cab] : m_cables)
    {
        if (toLower(cab.category) == lCat)
        {
            result.push_back(cab);
        }
    }
    return result;
}

std::vector<MaterialDefinition> LibraryRegistry::searchMaterials(const std::string& query) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MaterialDefinition> result;
    std::string q = toLower(query);
    for (const auto& [_, mat] : m_materials)
    {
        if (toLower(mat.id).find(q) != std::string::npos ||
            toLower(mat.name).find(q) != std::string::npos ||
            toLower(mat.category).find(q) != std::string::npos ||
            toLower(mat.standard.name).find(q) != std::string::npos)
        {
            result.push_back(mat);
        }
    }
    return result;
}

std::vector<SectionDefinition> LibraryRegistry::searchSections(const std::string& query) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SectionDefinition> result;
    std::string q = toLower(query);
    for (const auto& [_, sec] : m_sections)
    {
        if (toLower(sec.id).find(q) != std::string::npos ||
            toLower(sec.name).find(q) != std::string::npos ||
            toLower(sec.category).find(q) != std::string::npos ||
            toLower(sec.shapeType).find(q) != std::string::npos ||
            toLower(sec.standard.name).find(q) != std::string::npos)
        {
            result.push_back(sec);
        }
    }
    return result;
}

std::vector<CableCatalogDefinition> LibraryRegistry::searchCables(const std::string& query) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CableCatalogDefinition> result;
    std::string q = toLower(query);
    for (const auto& [_, cab] : m_cables)
    {
        if (toLower(cab.id).find(q) != std::string::npos ||
            toLower(cab.name).find(q) != std::string::npos ||
            toLower(cab.category).find(q) != std::string::npos ||
            toLower(cab.grade).find(q) != std::string::npos)
        {
            result.push_back(cab);
        }
    }
    return result;
}

void LibraryRegistry::removeDefinitionsByLibraryId(const std::string& libraryId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto erasePred = [&libraryId](const auto& pair) {
        return pair.second.ref.libraryId == libraryId;
    };
    for (auto it = m_materials.begin(); it != m_materials.end(); )
    {
        if (erasePred(*it)) it = m_materials.erase(it);
        else ++it;
    }
    for (auto it = m_sections.begin(); it != m_sections.end(); )
    {
        if (erasePred(*it)) it = m_sections.erase(it);
        else ++it;
    }
    for (auto it = m_cables.begin(); it != m_cables.end(); )
    {
        if (erasePred(*it)) it = m_cables.erase(it);
        else ++it;
    }
}

void LibraryRegistry::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_materials.clear();
    m_sections.clear();
    m_cables.clear();
}

size_t LibraryRegistry::materialCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_materials.size();
}

size_t LibraryRegistry::sectionCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sections.size();
}

size_t LibraryRegistry::cableCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cables.size();
}

} // namespace TSA::ExtensionSystem
