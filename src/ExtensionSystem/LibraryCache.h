#pragma once

#include "DefinitionModels.h"
#include "../Model/Material.h"
#include "../Model/Section.h"
#include "../Model/Cable/CableDefinition.h"
#include <TopoDS_Shape.hxx>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>

namespace TSA::ExtensionSystem
{

/**
 * @brief Cache haute performance multi-niveaux en mémoire vive :
 * 1. Snapshots mécaniques de calcul
 * 2. Modèles structurels convertis (Material, Section, CableDefinition)
 * 3. Solides géométriques 3D B-Rep OpenCASCADE (TopoDS_Shape)
 * 4. Métriques de performance et télémétrie (Hits, Misses, Hit Ratio)
 */
class LibraryCache
{
public:
    static LibraryCache& instance();

    // Cache des snapshots mécaniques
    void putSnapshot(const std::string& qualifiedKey, const MechanicalSnapshot& snapshot);
    const MechanicalSnapshot* getSnapshot(const std::string& qualifiedKey) const;

    // Cache des modèles structurels convertis
    void putMaterial(const std::string& key, const TSA::Model::Material& mat);
    const TSA::Model::Material* getMaterial(const std::string& key) const;

    void putSection(const std::string& key, const TSA::Model::Section& sec);
    const TSA::Model::Section* getSection(const std::string& key) const;

    void putCable(const std::string& key, const TSA::Model::CableDefinition& cable);
    const TSA::Model::CableDefinition* getCable(const std::string& key) const;

    // Cache des géométries solides 3D OpenCASCADE
    void putShape(const std::string& key, const TopoDS_Shape& shape);
    TopoDS_Shape getShape(const std::string& key) const;
    bool hasShape(const std::string& key) const;

    // Invalidation sélective ou totale
    void invalidate(const std::string& definitionId);
    void clear();
    void clearShapes();

    // Tailles des caches
    size_t size() const;
    size_t snapshotCount() const;
    size_t materialCount() const;
    size_t sectionCount() const;
    size_t cableCount() const;
    size_t shapeCount() const;

    // Télémétrie et métriques
    uint64_t hitCount() const { return m_hits.load(); }
    uint64_t missCount() const { return m_misses.load(); }
    double hitRatio() const;
    void resetMetrics();

public:
    LibraryCache() = default;
    ~LibraryCache() = default;

    LibraryCache(const LibraryCache&) = delete;
    LibraryCache& operator=(const LibraryCache&) = delete;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, MechanicalSnapshot> m_snapshotCache;
    std::unordered_map<std::string, TSA::Model::Material> m_materialCache;
    std::unordered_map<std::string, TSA::Model::Section> m_sectionCache;
    std::unordered_map<std::string, TSA::Model::CableDefinition> m_cableCache;
    std::unordered_map<std::string, TopoDS_Shape> m_shapeCache;

    mutable std::atomic<uint64_t> m_hits{ 0 };
    mutable std::atomic<uint64_t> m_misses{ 0 };
};

} // namespace TSA::ExtensionSystem
