#pragma once

// Modèle d'affichage d'un maillage TSA (docs/MESH_VISUALIZATION.md), indépendant de tout moteur graphique :
// topologie de chaque type d'élément (ordre des nœuds Gmsh), éléments et nœuds visibles selon un filtre
// (dimensions, groupes isolés), faces extérieures des volumes, arêtes uniques, correspondances
// identifiant métier ↔ indice, erreurs de conversion. Le moteur graphique (MeshVS d'OCCT) ne lit que ce modèle :
// il ne dépend ni de Gmsh ni d'objets temporaires.

#include "MeshTypes.h"

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace TSA::Mesh
{

/// Topologie d'un type d'élément, indices LOCAUX dans FeElement::nodes (ordre Gmsh : coins puis milieux).
struct ElementTopology
{
    FeType type = FeType::Line2;
    int dimension = 1;
    int nodeCount = 2;                          ///< nœuds attendus (coins + milieux d'arêtes)
    int cornerCount = 2;
    std::vector<std::array<int, 2>> edges;      ///< arêtes (coins) ; milieu : edgeMid[k] (−1 au premier ordre)
    std::vector<int> edgeMid;
    std::vector<std::vector<int>> faces;        ///< faces orientées vers l'extérieur (coins) ; 2D : la face unique
};

/// Topologie d'un type, nullptr si non pris en charge (aucun type inconnu n'est deviné).
const ElementTopology* elementTopology(FeType type);
/// Nombre de nœuds attendu pour un type (0 si non pris en charge).
int feTypeNodeCount(FeType type);
/// Libellé lisible d'un type (« Quad8 — quadrangle à 8 nœuds »).
std::string feTypeLabel(FeType type);
/// Contour d'une face avec les nœuds milieux intercalés (second ordre) : indices locaux dans l'élément.
std::vector<int> faceOutline(const ElementTopology& topo, const std::vector<int>& face);

struct MeshDisplayFilter
{
    bool show1D = true;
    bool show2D = true;
    bool show3D = true;
    bool boundaryElements = false;      ///< éléments de frontière (contours de surfaces, faces de volumes)
    std::set<std::string> isolatedGroups; ///< vide : tous ; sinon seuls les éléments de ces groupes
    bool volumeExteriorOnly = true;     ///< volumes : faces extérieures seules (faux : toutes les faces, inspection en coupe)
    /// Coupe par éléments : seuls les éléments dont le centre (moyenne des coins) vérifie
    /// cutNormal · c <= cutOffset restent ; les faces des volumes ainsi exposées deviennent extérieures.
    bool cut = false;
    std::array<double, 3> cutNormal { 0.0, 0.0, 1.0 };
    double cutOffset = 0.0;
};

/// Arête unique (identifiants de nœuds TSA, a < b) et nœud milieu éventuel (0 sinon).
struct DisplayEdge
{
    int a = 0, b = 0, mid = 0;
};

class MeshDisplayModel
{
public:
    struct Issue
    {
        int elementId = 0;
        std::string message;
    };

    /// Construit le modèle. Les identifiants ne sont jamais supposés consécutifs. Retourne faux si aucun élément
    /// affichable ; les éléments non convertibles sont écartés et listés dans issues().
    bool build(const FeMesh& mesh, const MeshDisplayFilter& filter = {});

    const FeMesh* mesh() const { return m_mesh; }
    const MeshDisplayFilter& filter() const { return m_filter; }

    /// Identifiants métier (FeElement::id, FeNode::id) des entités affichées, triés.
    const std::vector<int>& visibleElements() const { return m_visibleElements; }
    const std::vector<int>& visibleNodes() const { return m_visibleNodes; }
    bool isElementVisible(int id) const;

    /// Correspondance identifiant métier → indice dans FeMesh::elements / FeMesh::nodes (−1 : inconnu).
    int elementIndex(int elementId) const;
    int nodeIndex(int nodeId) const;
    const FeElement* element(int elementId) const;
    const FeNode* node(int nodeId) const;

    /// Faces affichées d'un volume visible (indices locaux, contour avec milieux) ; vide pour un non-volume.
    const std::vector<std::vector<int>>& volumeFaces(int elementId) const;
    std::size_t exteriorFaceCount() const { return m_exteriorFaces; }
    std::size_t interiorFaceCount() const { return m_interiorFaces; }
    /// Arêtes uniques des éléments 2D et 3D visibles (une seule fois même si partagée).
    const std::vector<DisplayEdge>& edges() const { return m_edges; }

    /// Groupes contenant l'élément (noms), groupes de nœuds contenant le nœud.
    std::vector<std::string> groupsOfElement(int elementId) const;
    std::vector<std::string> groupsOfNode(int nodeId) const;

    const std::vector<Issue>& issues() const { return m_issues; }
    /// Nombre d'éléments affichés par dimension (1, 2, 3).
    std::array<std::size_t, 4> countByDimension() const { return m_countByDim; }

    /// Étiquettes : au plus maxLabels entités ; vrai si la liste a été tronquée.
    static bool labelSubset(const std::vector<int>& ids, std::size_t maxLabels, std::vector<int>* out);

private:
    const FeMesh* m_mesh = nullptr;
    MeshDisplayFilter m_filter;
    std::unordered_map<int, int> m_elementIndex, m_nodeIndex;
    std::vector<int> m_visibleElements, m_visibleNodes;
    std::vector<char> m_visibleFlag;    ///< par indice d'élément
    std::unordered_map<int, std::vector<std::vector<int>>> m_volumeFaces;
    std::size_t m_exteriorFaces = 0, m_interiorFaces = 0;
    std::vector<DisplayEdge> m_edges;
    std::unordered_multimap<int, int> m_groupsByElement, m_groupsByNode; ///< id → indice de groupe
    std::vector<Issue> m_issues;
    std::array<std::size_t, 4> m_countByDim { 0, 0, 0, 0 };
};

} // namespace TSA::Mesh
