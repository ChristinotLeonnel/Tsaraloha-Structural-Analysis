#pragma once

// ============================================================
// NORMATIVE REFERENCE
// Standard   : ISO/IEC 25010:2023 §4.2.5 (Fault Tolerance & Integrity)
// Area       : Analysis FEM / Decoupling
// Requirement: REQ-CALC-SNAP-001 (Immutable Calculation Snapshot)
// Purpose    : Isolates finite element calculations from UI model mutations
// ============================================================

#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Load/NodalLoad.h"
#include "../Model/Load/MemberLoad.h"
#include "../Model/Load/LoadCase.h"
#include "../Model/Load/LoadCombination.h"
#include "AnalysisTypes.h"
#include "../Model/SelectionQuery.h"

#include <array>

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
    double kTx = 0.0;
    double kTy = 0.0;
    double kTz = 0.0;
    double kRx = 0.0;
    double kRy = 0.0;
    double kRz = 0.0;
    /// Blocages tels que définis par l'utilisateur (Tx, Ty, Tz, Rx, Ry, Rz), AVANT les blocages
    /// anti-singularité propres au calcul 3D appliqués à fix* (rotation de forage des appuis
    /// articulés, Ty des appuis glissants). Un moteur 2D doit utiliser ceux-ci.
    std::array<bool, 6> definedFix { false, false, false, false, false, false };
};

/**
 * @brief Barre linéaire figée (Poutre, Poteau, Bielle, Câble).
 */
struct SnapshotElement
{
    using ElementType = StructuralElementKind;

    int id = 0;          ///< identifiant TSA (unique dans sa famille seulement)
    int tag = 0;         ///< indice d'analyse unique 1..N (ordre Beam, Column, Truss, Cable) ; = tag OpenSees
    ElementType type = ElementType::Beam;

    ElementKey key() const { return { type, id }; }
    int startNodeId = 0;
    int endNodeId = 0;
    double rotation = 0.0;
    double length = 0.0;
    TSA::Model::Section section;
    TSA::Model::Material material;
    double initialTension = 0.0; // Pour les câbles
    /// Relâchements d'extrémité (poutres). Rotules de flexion (my, mz) transmises à OpenSees
    /// (-releasey / -releasez) et à Custom2D ; les autres relâchements sont signalés par ModelValidator.
    TSA::Model::EndRelease startRelease;
    TSA::Model::EndRelease endRelease;
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

    /**
     * @brief Capture restreinte à une portée : barres de la portée, nœuds qu'elles relient,
     * charges appliquées à ces nœuds / barres, tous les cas et combinaisons.
     * @param scope nullptr = tout le modèle (identique à capture(model)).
     */
    static CalculationSnapshot capture(const TSA::Model::Model& model, const TSA::Model::ElementSet* scope);

    const std::map<int, SnapshotNode>& nodes() const { return m_nodes; }
    const std::map<int, SnapshotElement>& elements() const { return m_elements; }
    const std::vector<TSA::Model::NodalLoad>& nodalLoads() const { return m_nodalLoads; }
    const std::vector<TSA::Model::MemberLoad>& memberLoads() const { return m_memberLoads; }
    const std::map<int, TSA::Model::LoadCase>& loadCases() const { return m_loadCases; }
    const std::map<int, TSA::Model::LoadCombination>& combinations() const { return m_combinations; }

    const SnapshotNode* getNode(int id) const;
    /// Élément par tag OpenSees (clé de elements()).
    const SnapshotElement* getElementByTag(int tag) const;
    /// Élément par identité TSA (famille, id).
    const SnapshotElement* findElement(StructuralElementKind kind, int id) const;
    const SnapshotElement* findElement(const ElementKey& key) const { return findElement(key.kind, key.id); }
    /// Élément porteur d'une charge sur barre (MemberLoad::targetType + elementId).
    const SnapshotElement* findElementForLoad(const TSA::Model::MemberLoad& load) const;

    static StructuralElementKind kindOf(TSA::Model::MemberTargetType t);

    size_t nodeCount() const { return m_nodes.size(); }
    size_t elementCount() const { return m_elements.size(); }

    bool hasNode(int id) const { return m_nodes.find(id) != m_nodes.end(); }
    bool hasElement(StructuralElementKind kind, int id) const { return findElement(kind, id) != nullptr; }

private:
    std::map<int, SnapshotNode> m_nodes;
    std::map<int, SnapshotElement> m_elements;          ///< clé : tag OpenSees unique
    std::map<ElementKey, int> m_tagByKey;
    std::vector<TSA::Model::NodalLoad> m_nodalLoads;
    std::vector<TSA::Model::MemberLoad> m_memberLoads;
    std::map<int, TSA::Model::LoadCase> m_loadCases;
    std::map<int, TSA::Model::LoadCombination> m_combinations;
};

} // namespace TSA::Analysis
