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
    if (it != m_snapshotCache.end()) return &it->second;
    return nullptr;
}

void LibraryCache::invalidate(const std::string& definitionId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_snapshotCache.begin(); it != m_snapshotCache.end(); )
    {
        if (it->first.find(definitionId) != std::string::npos)
        {
            it = m_snapshotCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void LibraryCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshotCache.clear();
}

size_t LibraryCache::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshotCache.size();
}

} // namespace TSA::ExtensionSystem
