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
#include "SurfaceAnalysisModel.h"
#include "../Model/SelectionQuery.h"

#include <array>
#include <set>

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
    bool pinnedSupport = false;   ///< appui articulé (translations bloquées, rotations libres)
    bool rollerSupport = false;   ///< appui simple (seule la translation verticale bloquée)
};

/// Blocage ajouté par le calcul 3D, absent des appuis définis par l'utilisateur. Jamais silencieux :
/// listé avant le calcul et contrôlé après (effort repris par ce blocage).
struct AutomaticRestraint
{
    int nodeId = 0;
    int dof = 0;          ///< 0..5 : Tx, Ty, Tz, Rx, Ry, Rz (repère global)
    std::string reason;
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
    double initialTension = 0.0; // Pour les câbles : tension initiale T0 en N (Cable::initialTension)
    /// Relâchements d'extrémité (poutres). Rotules de flexion (my, mz) transmises à OpenSees
    /// (-releasey / -releasez) et à Custom2D ; les autres relâchements sont signalés par ModelValidator.
    TSA::Model::EndRelease startRelease;
    TSA::Model::EndRelease endRelease;
};

/// Correspondance d'une barre subdivisée selon le maillage (docs/MESH.md §J) : sous-éléments transmis au
/// moteur (identifiants >= kSubElementIdBase, même famille) et nœuds intermédiaires (absents du modèle TSA).
/// Élément de coque figé (dalle ou voile maillé) : 3 ou 4 nœuds du snapshot.
struct SnapshotShell
{
    int tag = 0;                ///< tag OpenSees, au-delà des tags des barres
    int meshElement = 0;        ///< élément du maillage du projet
    std::string source;         ///< SL2, W5
    std::vector<int> nodes;
    double thickness = 0.0;
    TSA::Model::Material material;
};

/// Force nodale (kN, repère global) d'une charge surfacique (loadCaseId > 0) ou du poids propre des surfaces (0).
struct SnapshotNodalForce
{
    int nodeId = 0;
    int loadCaseId = 0;
    double fx = 0, fy = 0, fz = 0;
    std::string label;
};

/// Correspondance nœuds du maillage → nœuds du snapshot (nœud TSA, nœud de subdivision d'une barre, ou nœud propre
/// à la surface, à retirer des résultats nodaux de TSA).
struct SurfaceNodeMap
{
    std::map<int, int> meshToNode;
    std::set<int> surfaceOnlyNodes;
    bool empty() const { return meshToNode.empty(); }
};

struct SubdivisionMap
{
    static constexpr int kSubElementIdBase = 1000000000;
    struct Part
    {
        ElementKey key;          ///< sous-élément
        double x0 = 0, x1 = 0;   ///< abscisses (m) sur la barre d'origine
    };
    std::map<ElementKey, std::vector<Part>> parts;   ///< barre TSA → sous-éléments dans l'ordre
    std::set<int> internalNodes;
    bool empty() const { return parts.empty(); }
    std::size_t subElementCount() const;
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
    /// Idem, avec le mode de blocage automatique des appuis articulés / simples choisi par l'utilisateur.
    static CalculationSnapshot capture(const TSA::Model::Model& model, const TSA::Model::ElementSet* scope,
                                       SupportRestraintMode supportRestraints);

    /**
     * @brief Snapshot dont les barres fléchies (poutres, poteaux) sont subdivisées aux positions relatives
     * données (0 < s < 1) : nœuds intermédiaires libres, relâchements aux seules extrémités, charges sur
     * barre réparties exactement sur les sous-éléments (portions de charges réparties, charge ponctuelle sur
     * le sous-élément qui la porte). Treillis et câbles ne sont jamais subdivisés (mécanisme) : *notes le
     * signale. Les identifiants des sous-éléments et des nœuds intermédiaires sont dans *map.
     */
    static CalculationSnapshot subdivided(const CalculationSnapshot& source, const std::map<ElementKey, std::vector<double>>& interiorPositions,
                                          SubdivisionMap* map, std::vector<std::string>* notes = nullptr);

    const std::map<int, SnapshotNode>& nodes() const { return m_nodes; }
    const std::map<int, SnapshotElement>& elements() const { return m_elements; }
    const std::vector<TSA::Model::NodalLoad>& nodalLoads() const { return m_nodalLoads; }
    const std::vector<TSA::Model::MemberLoad>& memberLoads() const { return m_memberLoads; }
    const std::map<int, TSA::Model::LoadCase>& loadCases() const { return m_loadCases; }
    const std::map<int, TSA::Model::LoadCombination>& combinations() const { return m_combinations; }
    /// Blocages ajoutés aux appuis articulés / simples (appui à fourche, structure plane), voir capture().
    const std::vector<AutomaticRestraint>& automaticRestraints() const { return m_automaticRestraints; }
    /// Remarques sur les appuis non complétés (barre inclinée : rotation autour de son axe laissée libre).
    const std::vector<std::string>& restraintNotes() const { return m_restraintNotes; }
    /// Risques des blocages appliqués (mode historique : RX globale qui encastre la flexion d'une barre non
    /// parallèle à X) : DDL, risque et correction ; à présenter comme avertissements avant le calcul.
    const std::vector<std::string>& restraintWarnings() const { return m_restraintWarnings; }
    SupportRestraintMode supportRestraintMode() const { return m_supportRestraintMode; }

    /**
     * @brief Snapshot complété par les éléments surfaciques (coques) : chaque nœud du maillage est rattaché au nœud
     * TSA qu'il représente, sinon au nœud du snapshot situé au même point (nœud de subdivision d'une barre : maillage
     * conforme, l'arête commune est une seule courbe du mailleur), sinon devient un nœud libre. Seule la formulation
     * coque est acceptée (membrane et plaque seules : *error). Forces des charges surfaciques et du poids propre
     * reportées sur les nœuds correspondants.
     */
    static CalculationSnapshot withSurfaces(const CalculationSnapshot& source, const SurfaceAnalysisModel& surfaces, SurfaceNodeMap* map,
                                            std::vector<std::string>* notes, std::string* error);

    const std::vector<SnapshotShell>& shells() const { return m_shells; }
    const std::vector<SnapshotNodalForce>& surfaceForces() const { return m_surfaceForces; }
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
    /// Blocages anti-singularité des appuis articulés / simples (après capture des barres).
    static void applyAutomaticRestraints(CalculationSnapshot& snap, SupportRestraintMode mode);

    std::map<int, SnapshotNode> m_nodes;
    std::map<int, SnapshotElement> m_elements;          ///< clé : tag OpenSees unique
    std::vector<AutomaticRestraint> m_automaticRestraints;
    std::vector<std::string> m_restraintNotes;
    std::vector<std::string> m_restraintWarnings;
    SupportRestraintMode m_supportRestraintMode = SupportRestraintMode::Fork;
    std::map<ElementKey, int> m_tagByKey;
    std::vector<TSA::Model::NodalLoad> m_nodalLoads;
    std::vector<TSA::Model::MemberLoad> m_memberLoads;
    std::map<int, TSA::Model::LoadCase> m_loadCases;
    std::map<int, TSA::Model::LoadCombination> m_combinations;
    std::vector<SnapshotShell> m_shells;
    std::vector<SnapshotNodalForce> m_surfaceForces;
};

} // namespace TSA::Analysis
