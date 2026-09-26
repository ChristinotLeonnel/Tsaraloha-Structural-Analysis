#pragma once

#include <vector>
#include <set>
#include <unordered_map>
#include <cstddef>
#include "Beam.h"

namespace TSA::Model
{
class Model;

struct ClipboardNode
{
    int originalId = 0;
    double relX = 0.0;
    double relY = 0.0;
    double relZ = 0.0;
};

struct ClipboardBeam
{
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    BarProperties props;
};

struct ClipboardColumn
{
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    BarProperties props;
};

struct ClipboardSlab
{
    std::vector<int> originalNodeIds;
    double thickness = 0.20;
};

struct PasteResult
{
    std::vector<int> nodeIds;
    std::vector<int> beamIds;
    std::vector<int> columnIds;
    std::vector<int> slabIds;

    bool empty() const {
        return nodeIds.empty() && beamIds.empty() && columnIds.empty() && slabIds.empty();
    }
};

/**
 * @brief Presse-papier structurel pour la copie et le collage d'entités 3D.
 * Découplé de toute interface utilisateur pour une modularité totale.
 */
class StructuralClipboard
{
public:
    StructuralClipboard() = default;

    bool hasData() const noexcept { return m_hasData && !m_nodes.empty(); }
    void clear() noexcept;

    size_t nodeCount() const noexcept { return m_nodes.size(); }
    size_t beamCount() const noexcept { return m_beams.size(); }
    size_t columnCount() const noexcept { return m_columns.size(); }
    size_t slabCount() const noexcept { return m_slabs.size(); }
    size_t totalElementCount() const noexcept { return m_beams.size() + m_columns.size() + m_slabs.size(); }

    void copyFrom(const Model& model,
                  const std::vector<int>& selectedNodes,
                  const std::vector<int>& selectedBeams,
                  const std::vector<int>& selectedColumns,
                  const std::vector<int>& selectedSlabs);

    void copyFrom(const Model& model,
                  const std::set<int>& selectedNodes,
                  const std::set<int>& selectedBeams,
                  const std::set<int>& selectedColumns,
                  const std::set<int>& selectedSlabs);

    PasteResult pasteTo(Model& model, double targetX, double targetY, double targetZ) const;

    const std::vector<ClipboardNode>& nodes() const noexcept { return m_nodes; }
    const std::vector<ClipboardBeam>& beams() const noexcept { return m_beams; }
    const std::vector<ClipboardColumn>& columns() const noexcept { return m_columns; }
    const std::vector<ClipboardSlab>& slabs() const noexcept { return m_slabs; }

private:
    bool m_hasData = false;
    double m_refOriginX = 0.0;
    double m_refOriginY = 0.0;
    double m_refOriginZ = 0.0;

    std::vector<ClipboardNode> m_nodes;
    std::vector<ClipboardBeam> m_beams;
    std::vector<ClipboardColumn> m_columns;
    std::vector<ClipboardSlab> m_slabs;
};

} // namespace TSA::Model
