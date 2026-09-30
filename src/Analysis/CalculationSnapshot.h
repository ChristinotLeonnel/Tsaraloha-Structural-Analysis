#pragma once

#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Load/NodalLoad.h"
#include "../Model/Load/MemberLoad.h"
#include "../Model/Load/LoadCase.h"
#include "../Model/Load/LoadCombination.h"

#include <map>
#include <vector>
#include <string>
#include <memory>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{

/**
 * @brief Nœud figé pour le calcul structural.
 */
struct SnapshotNode
{
    int id = 0;
    std::string name;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    TSA::Model::SupportType supportType = TSA::Model::SupportType::Free;
    bool fixTx = false;
    bool fixTy = false;
    bool fixTz = false;
    bool fixRx = false;
    bool fixRy = false;
    bool fixRz = false;
};

/**
 * @brief Barre linéaire figée (Poutre, Poteau, Bielle, Câble).
 */
struct SnapshotElement
{
    enum class ElementType { Beam, Column, Truss, Cable };

    int id = 0;
    ElementType type = ElementType::Beam;
    int startNodeId = 0;
    int endNodeId = 0;
    double rotation = 0.0;
    double length = 0.0;
    TSA::Model::Section section;
    TSA::Model::Material material;
    double initialTension = 0.0; // Pour les câbles
};

/**
 * @brief Snapshot calculatoire immuable isolé du modèle utilisateur.
 * Garantit que les conversions et post-traitements OpenSees ne modifient
 * jamais le modèle actif dans l'UI (exigence 44).
 */
class CalculationSnapshot
{
public:
    CalculationSnapshot() = default;

    /**
     * @brief Capture l'état instantané complet du modèle TSA.
     */
    static CalculationSnapshot capture(const TSA::Model::Model& model);

    const std::map<int, SnapshotNode>& nodes() const { return m_nodes; }
    const std::map<int, SnapshotElement>& elements() const { return m_elements; }
    const std::vector<TSA::Model::NodalLoad>& nodalLoads() const { return m_nodalLoads; }
    const std::vector<TSA::Model::MemberLoad>& memberLoads() const { return m_memberLoads; }
    const std::map<int, TSA::Model::LoadCase>& loadCases() const { return m_loadCases; }
    const std::map<int, TSA::Model::LoadCombination>& combinations() const { return m_combinations; }

    const SnapshotNode* getNode(int id) const;
    const SnapshotElement* getElement(int id) const;

    size_t nodeCount() const { return m_nodes.size(); }
    size_t elementCount() const { return m_elements.size(); }

    bool hasNode(int id) const { return m_nodes.find(id) != m_nodes.end(); }
    bool hasElement(int id) const { return m_elements.find(id) != m_elements.end(); }

private:
    std::map<int, SnapshotNode> m_nodes;
    std::map<int, SnapshotElement> m_elements;
    std::vector<TSA::Model::NodalLoad> m_nodalLoads;
    std::vector<TSA::Model::MemberLoad> m_memberLoads;
    std::map<int, TSA::Model::LoadCase> m_loadCases;
    std::map<int, TSA::Model::LoadCombination> m_combinations;
};

} // namespace TSA::Analysis
