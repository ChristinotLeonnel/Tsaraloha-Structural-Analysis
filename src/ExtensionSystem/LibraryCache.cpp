#include "LibraryCache.h"

namespace TSA::ExtensionSystem
{

LibraryCache& LibraryCache::instance()
{
    static LibraryCache inst;
    return inst;
}

void LibraryCache::putSnapshot(const std::string& qualifiedKey, const MechanicalSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCache[qualifiedKey] = snapshot;
}

const MechanicalSnapshot* LibraryCache::getSnapshot(const std::string& qualifiedKey) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_snapshotCache.find(qualifiedKey);
    if (it != m_snapshotCache.end())
    {
        m_hits++;
        return &it->second;
    }
    m_misses++;
    return nullptr;
}

void LibraryCache::putMaterial(const std::string& key, const TSA::Model::Material& mat)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_materialCache[key] = mat;
}

const TSA::Model::Material* LibraryCache::getMaterial(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_materialCache.find(key);
    if (it != m_materialCache.end())
    {
        m_hits++;
        return &it->second;
    }
    m_misses++;
    return nullptr;
}

void LibraryCache::putSection(const std::string& key, const TSA::Model::Section& sec)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sectionCache[key] = sec;
}

const TSA::Model::Section* LibraryCache::getSection(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sectionCache.find(key);
    if (it != m_sectionCache.end())
    {
        m_hits++;
        return &it->second;
    }
    m_misses++;
    return nullptr;
}

void LibraryCache::putCable(const std::string& key, const TSA::Model::CableDefinition& cable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cableCache[key] = cable;
}

const TSA::Model::CableDefinition* LibraryCache::getCable(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cableCache.find(key);
    if (it != m_cableCache.end())
    {
        m_hits++;
        return &it->second;
    }
    m_misses++;
    return nullptr;
}

void LibraryCache::putShape(const std::string& key, const TopoDS_Shape& shape)
{
    if (shape.IsNull()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shapeCache[key] = shape;
}

TopoDS_Shape LibraryCache::getShape(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_shapeCache.find(key);
    if (it != m_shapeCache.end())
    {
        m_hits++;
        return it->second;
    }
    m_misses++;
    return TopoDS_Shape();
}

bool LibraryCache::hasShape(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shapeCache.find(key) != m_shapeCache.end();
}

void LibraryCache::invalidate(const std::string& definitionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto eraseMatches = [&](auto& map) {
        for (auto it = map.begin(); it != map.end(); )
        {
            if (it->first.find(definitionId) != std::string::npos)
            {
                it = map.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    eraseMatches(m_snapshotCache);
    eraseMatches(m_materialCache);
    eraseMatches(m_sectionCache);
    eraseMatches(m_cableCache);
    eraseMatches(m_shapeCache);
}

void LibraryCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCache.clear();
    m_materialCache.clear();
    m_sectionCache.clear();
    m_cableCache.clear();
    m_shapeCache.clear();
}

void LibraryCache::clearShapes()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shapeCache.clear();
}

size_t LibraryCache::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshotCache.size() + m_materialCache.size() + m_sectionCache.size() + m_cableCache.size() + m_shapeCache.size();
}

size_t LibraryCache::snapshotCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshotCache.size();
}

size_t LibraryCache::materialCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_materialCache.size();
}

size_t LibraryCache::sectionCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sectionCache.size();
}

size_t LibraryCache::cableCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cableCache.size();
}

size_t LibraryCache::shapeCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shapeCache.size();
}

double LibraryCache::hitRatio() const
{
    uint64_t h = m_hits.load();
    uint64_t m = m_misses.load();
    if (h + m == 0) return 0.0;
    return static_cast<double>(h) / static_cast<double>(h + m);
}

void LibraryCache::resetMetrics()
{
    m_hits.store(0);
    m_misses.store(0);
}

} // namespace TSA::ExtensionSystem
