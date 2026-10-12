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

/// Sélection copiée (identifiants du projet source, par famille : un nœud 1 et une poutre 1 coexistent).
struct ClipboardSelection
{
    std::set<int> nodes, beams, columns, slabs, walls, trusses, cables;
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

    /// Tables de correspondance source → cible (traçabilité, sélection du collage, tests)
    std::map<int, int> nodeMap;
    std::map<TSA::BIM::AnalyticalRef, TSA::BIM::AnalyticalRef> elementMap;
    std::map<int, int> loadCaseMap;

    bool complete = true;   ///< faux : un élément n'a pas pu être recréé (le collage protégé est alors annulé)
    std::string error;

    bool empty() const {
        return nodeIds.empty() && beamIds.empty() && columnIds.empty() && slabIds.empty() && wallIds.empty() && trussIds.empty() && cableIds.empty();
    }
    std::size_t elementCount() const {
        return beamIds.size() + columnIds.size() + slabIds.size() + wallIds.size() + trussIds.size() + cableIds.size();
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

    bool hasData() const noexcept { return m_hasData && !m_nodes.empty(); }
    void clear() noexcept;

    size_t nodeCount() const noexcept { return m_nodes.size(); }
    size_t beamCount() const noexcept { return m_beams.size(); }
    size_t columnCount() const noexcept { return m_columns.size(); }
    size_t slabCount() const noexcept { return m_slabs.size(); }
    size_t wallCount() const noexcept { return m_walls.size(); }
    size_t trussCount() const noexcept { return m_trusses.size(); }
    size_t cableCount() const noexcept { return m_cables.size(); }
    size_t loadCount() const noexcept { return m_nodalLoads.size() + m_memberLoads.size() + m_surfaceLoads.size(); }
    size_t totalElementCount() const noexcept
    {
        return m_beams.size() + m_columns.size() + m_slabs.size() + m_walls.size() + m_trusses.size() + m_cables.size();
    }

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

    /// Collage avec le coin minimal (x, y, z) de la sélection copiée placé au point cible. Ne gère pas l'historique.
    PasteResult pasteTo(Model& model, double targetX, double targetY, double targetZ) const;
    /// Collage à la position d'origine (mêmes coordonnées que dans le projet source).
    PasteResult pasteAtOrigin(Model& model) const { return pasteTo(model, m_refOriginX, m_refOriginY, m_refOriginZ); }
    /// Collage « tout ou rien » : une étape d'annulation (Ctrl+Z annule le collage entier) ; si un élément ne peut pas
    /// être recréé, le modèle est rétabli dans son état d'avant le collage (aucun collage partiel).
    PasteResult pasteUndoable(Model& model, double targetX, double targetY, double targetZ, const std::string& actionName = "Coller") const;
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
    /// Produits physiques (métadonnées BIM, regroupement 1:N) des éléments copiés : le collage
    /// reforme un produit par produit source (BUG-029).
    std::vector<TSA::BIM::PhysicalElement> m_bimProducts;
};

} // namespace TSA::Model
