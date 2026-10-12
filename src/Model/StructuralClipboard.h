#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>
#include <unordered_map>
#include <cstddef>
#include "Beam.h"
#include "Slab.h"
#include "Wall.h"
#include "TrussMember.h"
#include "SupportDefinition.h"
#include "Cable/Cable.h"
#include "Load/LoadCase.h"
#include "Load/NodalLoad.h"
#include "Load/MemberLoad.h"
#include "Load/SurfaceLoad.h"
#include "Load/LoadCombination.h"
#include "Foundation.h"
#include "SelectionQuery.h"
#include "../Annotation/Dimension.h"
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
    bool hasSupport = false;          ///< appui copié (option « appuis »)
    SupportDefinition support;
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
    Slab slab;                        ///< dalle complète (ouvertures, formulation, appuis linéaires…)
};

struct ClipboardWall
{
    int originalId = 0;
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    Wall wall;
};

struct ClipboardTruss
{
    int originalId = 0;
    int originalStartNodeId = 0;
    int originalEndNodeId = 0;
    TrussMember member;
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

struct ClipboardFoundation
{
    int originalId = 0;
    int originalNodeId = 0;
    Foundation foundation;
};

/// Sélection copiée (identifiants du projet source, par famille : un nœud 1 et une poutre 1 coexistent).
struct ClipboardSelection
{
    std::set<int> nodes, beams, columns, slabs, walls, trusses, cables, foundations;
    std::set<int> dimensions;   ///< cotations (annotations) : ancrages sur les nœuds copiés reliés, les autres figés
};

/// Nœuds du collage qui tombent sur des nœuds existants du projet cible.
enum class NodeMergeMode
{
    KeepSeparate,     ///< nœuds toujours créés (modèles indépendants, comportement historique)
    MergeCoincident   ///< nœud existant réutilisé s'il est à moins de la tolérance et compatible
};

/// Deux nœuds fusionnés portent des appuis différents (deux appuis non libres et non identiques).
enum class SupportConflictPolicy
{
    Ask,         ///< rien n'est collé ; les conflits sont rendus pour que l'utilisateur décide
    KeepTarget,  ///< l'appui du projet cible est conservé
    UseCopied,   ///< l'appui copié remplace celui du projet cible
    DontMerge    ///< ces nœuds ne sont pas fusionnés (nœuds coïncidents séparés)
};

/// Élément collé dont tous les nœuds sont des nœuds existants déjà reliés par un élément de la même famille.
enum class DuplicateElementPolicy
{
    Skip,  ///< élément (et ses charges) non collé : pas de rigidité doublée
    Keep   ///< élément collé quand même
};

struct PasteOptions
{
    NodeMergeMode merge = NodeMergeMode::KeepSeparate;
    double tolerance = 1.0e-3;   ///< m (unité de longueur du modèle) : distance réelle maximale pour fusionner
    SupportConflictPolicy supportConflicts = SupportConflictPolicy::Ask;
    DuplicateElementPolicy duplicates = DuplicateElementPolicy::Skip;
};

struct PasteConflict
{
    enum class Kind
    {
        Support,     ///< appuis incompatibles sur deux nœuds coïncidents
        Degenerate,  ///< élément qui deviendrait dégénéré (deux de ses nœuds fusionnés en un seul)
        Duplicate    ///< élément identique à un élément existant (mêmes nœuds, même famille)
    };
    Kind kind = Kind::Support;
    int clipboardId = 0;   ///< nœud ou élément source
    ElementKind element = ElementKind::Node;   ///< famille de l'élément (Degenerate, Duplicate)
    int targetNodeId = 0;  ///< nœud existant concerné (fusion)
    std::string message;
};

/// Analyse préalable du collage (aucune modification) : fusions retenues, conflits, avertissements.
struct PastePlan
{
    std::map<int, int> merges;   ///< nœud source → nœud existant du projet cible
    std::vector<PasteConflict> supportConflicts;   ///< non résolus (politique « Ask »)
    std::vector<PasteConflict> degenerate;         ///< bloquants
    std::vector<PasteConflict> duplicates;
    std::vector<std::string> warnings;
    bool needsDecision() const { return !supportConflicts.empty(); }
    bool blocked() const { return !degenerate.empty(); }
};

/// Ce que la copie emporte en plus de la géométrie, des sections, matériaux et orientations (toujours copiés).
/// Les résultats de calcul ne sont jamais copiés : le projet cible doit être recalculé.
struct ClipboardOptions
{
    bool supports = true;   ///< appuis des nœuds copiés
    bool loads = false;     ///< charges nodales, sur barres et surfaciques des éléments copiés (cas de charge associés)
};

struct PasteResult
{
    std::vector<int> nodeIds;
    std::vector<int> beamIds;
    std::vector<int> columnIds;
    std::vector<int> slabIds;
    std::vector<int> wallIds;
    std::vector<int> trussIds;
    std::vector<int> cableIds;
    std::vector<int> nodalLoadIds;
    std::vector<int> memberLoadIds;
    std::vector<int> surfaceLoadIds;
    std::vector<int> createdLoadCaseIds;   ///< cas de charge créés dans le projet cible (absents par leur nom)
    std::vector<int> foundationIds;
    std::vector<int> dimensionIds;
    std::vector<int> combinationIds;       ///< combinaisons créées (une combinaison identique existante est réutilisée)
    std::vector<int> mergedNodeIds;        ///< nœuds existants réutilisés (fusion)
    int skippedDuplicates = 0;             ///< éléments en double non collés (politique Skip)
    std::vector<std::string> warnings;     ///< renommages, combinaisons non copiées, charges ignorées…
    std::vector<PasteConflict> conflicts;  ///< conflits à trancher (needsDecision) ou bloquants
    bool needsDecision = false;            ///< rien n'a été collé : appuis incompatibles, décision demandée

    /// Tables de correspondance source → cible (traçabilité, sélection du collage, tests)
    std::map<int, int> nodeMap;
    std::map<TSA::BIM::AnalyticalRef, TSA::BIM::AnalyticalRef> elementMap;
    std::map<int, int> loadCaseMap;

    bool complete = true;   ///< faux : un élément n'a pas pu être recréé (le collage protégé est alors annulé)
    std::string error;

    bool empty() const {
        return nodeIds.empty() && mergedNodeIds.empty() && beamIds.empty() && columnIds.empty() && slabIds.empty() && wallIds.empty() &&
               trussIds.empty() && cableIds.empty() && foundationIds.empty() && dimensionIds.empty();
    }
    std::size_t elementCount() const {
        return beamIds.size() + columnIds.size() + slabIds.size() + wallIds.size() + trussIds.size() + cableIds.size() + foundationIds.size();
    }
    std::size_t loadCount() const { return nodalLoadIds.size() + memberLoadIds.size() + surfaceLoadIds.size(); }
};

/**
 * @brief Presse-papier structurel pour la copie et le collage d'entités 3D, au sein d'un projet ou d'un projet à l'autre.
 * Découplé de toute interface utilisateur. Le contenu est une copie par valeur, indépendante du projet source
 * (qui peut être fermé avant le collage). Les dépendances (nœuds, sections, matériaux, cas de charge) sont
 * recréées ou rapprochées dans le projet cible au collage, par des tables de correspondance.
 */
class StructuralClipboard
{
public:
    StructuralClipboard() = default;

    /// Presse-papier de l'application, commun à tous les documents ouverts (copier dans A, coller dans B).
    static StructuralClipboard& shared();

    bool hasData() const noexcept { return m_hasData && (!m_nodes.empty() || !m_dimensions.empty()); }
    void clear() noexcept;

    size_t nodeCount() const noexcept { return m_nodes.size(); }
    size_t beamCount() const noexcept { return m_beams.size(); }
    size_t columnCount() const noexcept { return m_columns.size(); }
    size_t slabCount() const noexcept { return m_slabs.size(); }
    size_t wallCount() const noexcept { return m_walls.size(); }
    size_t trussCount() const noexcept { return m_trusses.size(); }
    size_t cableCount() const noexcept { return m_cables.size(); }
    size_t foundationCount() const noexcept { return m_foundations.size(); }
    size_t dimensionCount() const noexcept { return m_dimensions.size(); }
    size_t combinationCount() const noexcept { return m_combinations.size(); }
    size_t loadCount() const noexcept { return m_nodalLoads.size() + m_memberLoads.size() + m_surfaceLoads.size(); }
    size_t totalElementCount() const noexcept
    {
        return m_beams.size() + m_columns.size() + m_slabs.size() + m_walls.size() + m_trusses.size() + m_cables.size() + m_foundations.size();
    }
    /// Avertissements de la copie (combinaison non copiée faute de cas de charge résoluble…)
    const std::vector<std::string>& copyWarnings() const noexcept { return m_copyWarnings; }

    void copyFrom(const Model& model, const ClipboardSelection& selection, const ClipboardOptions& options = {});

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

    /// Analyse du collage sans rien modifier : fusions de nœuds, conflits d'appuis, éléments dégénérés ou en double.
    PastePlan analyzePaste(const Model& model, double targetX, double targetY, double targetZ, const PasteOptions& options = {}) const;
    /// Collage avec le coin minimal (x, y, z) de la sélection copiée placé au point cible. Ne gère pas l'historique.
    PasteResult pasteTo(Model& model, double targetX, double targetY, double targetZ, const PasteOptions& options = {}) const;
    /// Collage à la position d'origine (mêmes coordonnées que dans le projet source).
    PasteResult pasteAtOrigin(Model& model, const PasteOptions& options = {}) const { return pasteTo(model, m_refOriginX, m_refOriginY, m_refOriginZ, options); }
    /// Collage « tout ou rien » : une étape d'annulation (Ctrl+Z annule le collage entier) ; si un élément ne peut pas
    /// être recréé, le modèle est rétabli dans son état d'avant le collage (aucun collage partiel). Conflit d'appuis à
    /// trancher (politique Ask) ou élément dégénéré : rien n'est modifié, `needsDecision` / `conflicts` renseignés.
    PasteResult pasteUndoable(Model& model, double targetX, double targetY, double targetZ, const std::string& actionName = "Coller",
                              const PasteOptions& options = {}) const;
    /// Vide si le contenu est collable ; sinon la première dépendance manquante (nœud, cas de charge).
    std::string validate() const;

    double originX() const noexcept { return m_refOriginX; }
    double originY() const noexcept { return m_refOriginY; }
    double originZ() const noexcept { return m_refOriginZ; }
    const ClipboardOptions& options() const noexcept { return m_options; }

    const std::vector<ClipboardNode>& nodes() const noexcept { return m_nodes; }
    const std::vector<ClipboardBeam>& beams() const noexcept { return m_beams; }
    const std::vector<ClipboardColumn>& columns() const noexcept { return m_columns; }
    const std::vector<ClipboardSlab>& slabs() const noexcept { return m_slabs; }
    const std::vector<ClipboardWall>& walls() const noexcept { return m_walls; }
    const std::vector<ClipboardTruss>& trusses() const noexcept { return m_trusses; }
    const std::vector<ClipboardCable>& cables() const noexcept { return m_cables; }

private:
    bool m_hasData = false;
    double m_refOriginX = 0.0;
    double m_refOriginY = 0.0;
    double m_refOriginZ = 0.0;
    ClipboardOptions m_options;

    std::vector<ClipboardNode> m_nodes;
    std::vector<ClipboardBeam> m_beams;
    std::vector<ClipboardColumn> m_columns;
    std::vector<ClipboardSlab> m_slabs;
    std::vector<ClipboardWall> m_walls;
    std::vector<ClipboardTruss> m_trusses;
    std::vector<ClipboardCable> m_cables;
    /// Charges des éléments copiés (identifiants source) et cas de charge auxquels elles appartiennent
    std::vector<NodalLoad> m_nodalLoads;
    std::vector<MemberLoad> m_memberLoads;
    std::vector<SurfaceLoad> m_surfaceLoads;
    std::map<int, LoadCase> m_loadCases;
    /// Combinaisons qui utilisent un cas de charge copié (tous leurs cas sont dans m_loadCases)
    std::vector<LoadCombination> m_combinations;
    std::vector<ClipboardFoundation> m_foundations;
    /// Cotations : ancrages associés aux nœuds copiés (identifiants source) ou figés (points absolus)
    std::vector<TSA::Annotation::Dimension> m_dimensions;
    std::vector<std::string> m_copyWarnings;
    /// Produits physiques (métadonnées BIM, regroupement 1:N) des éléments copiés : le collage
    /// reforme un produit par produit source (BUG-029).
    std::vector<TSA::BIM::PhysicalElement> m_bimProducts;
};

} // namespace TSA::Model
