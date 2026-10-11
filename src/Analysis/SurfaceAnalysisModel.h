#pragma once

// Éléments surfaciques transmis au calcul (docs/SURFACES.md). Produit par l'adaptateur maillage → moteur
// (Mesh/Solvers/OpenSeesMeshAdapter) à partir du maillage du projet : l'analyse ne dépend ni de Gmsh ni du maillage,
// elle reçoit des nœuds, des éléments de coque et des forces nodales déjà converties (charges nodales cohérentes).
// Jamais persisté : reconstruit au lancement de chaque calcul depuis le maillage à jour.

#include "../Model/Material.h"
#include "../Model/SurfaceFormulation.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis
{

struct SurfaceMeshNode
{
    int meshNode = 0;           ///< identifiant du nœud dans le maillage du projet (FeNode::id)
    double x = 0, y = 0, z = 0;
    int tsaNode = 0;            ///< nœud TSA confondu (0 : nœud propre au maillage)
};

struct SurfaceMeshElement
{
    int meshElement = 0;        ///< identifiant de l'élément dans le maillage du projet (FeElement::id)
    std::string source;         ///< surface d'origine (SL2, W5)
    std::vector<int> meshNodes; ///< 3 (triangle) ou 4 (quadrangle) nœuds, ordre du maillage
    TSA::Model::SurfaceFormulation formulation = TSA::Model::SurfaceFormulation::Shell;
    double thickness = 0.0;     ///< m
    TSA::Model::Material material;
};

/// Force nodale équivalente (kN, repère global) issue d'une charge surfacique ou du poids propre d'une surface.
struct SurfaceNodalForce
{
    int meshNode = 0;
    int loadCaseId = 0;         ///< 0 : poids propre (appliqué aux cas qui l'incluent, avec leur facteur)
    double fx = 0, fy = 0, fz = 0;
    std::string label;
};

/// Appui d'un nœud du maillage issu d'un appui linéique de bord (SurfaceEdgeSupport) : DDL bloqués et raideurs
/// nodales (raideur linéique × longueur d'influence du nœud sur le bord), axes globaux.
struct SurfaceNodeSupport
{
    int meshNode = 0;
    std::array<bool, 6> fixed {};
    std::array<double, 6> stiffness {};   ///< kN/m (translations), kN·m/rad (rotations)
    double length = 0.0;                  ///< longueur d'influence (m)
    std::string label;
};

struct SurfaceAnalysisModel
{
    std::vector<SurfaceMeshNode> nodes;
    std::vector<SurfaceMeshElement> elements;
    std::vector<SurfaceNodalForce> loads;       ///< charges surfaciques, par cas de charge
    std::vector<SurfaceNodalForce> selfWeight;  ///< poids propre des surfaces (facteur 1)
    std::vector<SurfaceNodeSupport> supports;   ///< appuis linéiques de bord répartis sur les nœuds du maillage
    std::map<std::string, double> selfWeightBySurface;  ///< kN, contrôle et note de calcul
    std::vector<std::string> notes;             ///< conversions effectuées, contrôles de résultante
    std::string meshStamp;                      ///< empreinte du maillage utilisé (date de génération + paramètres)

    bool empty() const { return elements.empty(); }
};

} // namespace TSA::Analysis
