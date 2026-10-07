#pragma once

#include <vector>
#include <set>
#include <unordered_map>
#include <cstddef>
#include "Beam.h"
#include "Cable/Cable.h"
#include "../BIM/Core/BimModel.h"

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
    int originalId = 0;
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    BarProperties props;
};

struct ClipboardColumn
{
    int originalId = 0;
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    BarProperties props;
};

struct ClipboardSlab
{
    int originalId = 0;
    std::vector<int> originalNodeIds;
    double thickness = 0.20;
    Material material = Material::concreteC25_30();
};

struct ClipboardCable
{
    int originalId = 0;
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    CableDefinition definition;
    CableType type = CableType::Generic;
    CableGeometryMode geometryMode = CableGeometryMode::Straight;
    double sag = 0.0;
    Section section;
    Material material;
    CablePrestress prestress;
    CableAnalysisProperties analysis;
    CableAnchor startAnchor;
    CableAnchor endAnchor;
    std::string color;
    std::string name;
};

struct PasteResult
{
    std::vector<int> nodeIds;
    std::vector<int> beamIds;
    std::vector<int> columnIds;
    std::vector<int> slabIds;
    std::vector<int> cableIds;

    bool empty() const {
        return nodeIds.empty() && beamIds.empty() && columnIds.empty() && slabIds.empty() && cableIds.empty();
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
    size_t cableCount() const noexcept { return m_cables.size(); }
    size_t totalElementCount() const noexcept { return m_beams.size() + m_columns.size() + m_slabs.size() + m_cables.size(); }

    void copyFrom(const Model& model,
                  const std::vector<int>& selectedNodes,
                  const std::vector<int>& selectedBeams,
                  const std::vector<int>& selectedColumns,
                  const std::vector<int>& selectedSlabs,
                  const std::vector<int>& selectedCables = {});

    void copyFrom(const Model& model,
                  const std::set<int>& selectedNodes,
                  const std::set<int>& selectedBeams,
                  const std::set<int>& selectedColumns,
                  const std::set<int>& selectedSlabs,
                  const std::set<int>& selectedCables = {});

    PasteResult pasteTo(Model& model, double targetX, double targetY, double targetZ) const;

    const std::vector<ClipboardNode>& nodes() const noexcept { return m_nodes; }
    const std::vector<ClipboardBeam>& beams() const noexcept { return m_beams; }
    const std::vector<ClipboardColumn>& columns() const noexcept { return m_columns; }
    const std::vector<ClipboardSlab>& slabs() const noexcept { return m_slabs; }
    const std::vector<ClipboardCable>& cables() const noexcept { return m_cables; }

private:
    bool m_hasData = false;
    double m_refOriginX = 0.0;
    double m_refOriginY = 0.0;
    double m_refOriginZ = 0.0;

    std::vector<ClipboardNode> m_nodes;
    std::vector<ClipboardBeam> m_beams;
    std::vector<ClipboardColumn> m_columns;
    std::vector<ClipboardSlab> m_slabs;
    std::vector<ClipboardCable> m_cables;
    /// Produits physiques (métadonnées BIM, regroupement 1:N) des éléments copiés : le collage
    /// reforme un produit par produit source (BUG-029).
    std::vector<TSA::BIM::PhysicalElement> m_bimProducts;
};

} // namespace TSA::Model
