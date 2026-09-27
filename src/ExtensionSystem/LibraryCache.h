#pragma once

#include "DefinitionModels.h"
#include <unordered_map>
#include <string>
#include <mutex>

namespace TSA::ExtensionSystem
{

/**
 * @brief Cache haute performance en mémoire vive pour les définitions et les snapshots mécaniques.
 * Évite le re-parsing JSON répétitif et accélère les calculs et le rendu OCCT.
 */
class LibraryCache
{
public:
    static LibraryCache& instance();

    // Cache des snapshots mécaniques
    void putSnapshot(const std::string& qualifiedKey, const MechanicalSnapshot& snapshot);
    const MechanicalSnapshot* getSnapshot(const std::string& qualifiedKey) const;

    // Invalidation sélective ou totale
    void invalidate(const std::string& definitionId);
    void clear();

    size_t size() const;

public:
    LibraryCache() = default;
    ~LibraryCache() = default;

    LibraryCache(const LibraryCache&) = delete;
    LibraryCache& operator=(const LibraryCache&) = delete;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, MechanicalSnapshot> m_snapshotCache;
};

} // namespace TSA::ExtensionSystem
