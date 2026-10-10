#pragma once

// Représentation du modèle dans la vue 3D (Affichage > Représentation). Purement visuelle : aucun mode
// ne modifie le modèle, les sections, les matériaux, les appuis, les résultats ou leur révision.
//
//   Physique          : sections volumiques habituelles.
//   Filaire analytique: chaque barre est affichée par son axe nœud à nœud (même objet AIS : sélection,
//                       surbrillance, isolation inchangées) ; dalles, voiles, fondations en arêtes.
//   Éléments finis    : axes analytiques + maillage RÉELLEMENT transmis au moteur lors du dernier calcul
//                       à jour (ResultsModel::solverMesh) ; rien n'est déduit ni subdivisé ici.
//   Superposition     : sections volumiques translucides + axes analytiques par-dessus.
//
// Ce fichier ne dépend pas d'OpenGL : la politique et la géométrie sont testées sans vue.

#include "../Analysis/SolverMesh.h"
#include "../Model/Beam.h"

#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::Viewer
{

enum class ModelDisplayMode
{
    Physical,
    Analytical,
    FiniteElement,
    Overlay
};

const char* modelDisplayModeName(ModelDisplayMode mode);

namespace DisplayPolicy
{
/// Barres affichées par leur axe analytique (au lieu de la section volumique).
inline bool linearAsAxis(ModelDisplayMode m) { return m == ModelDisplayMode::Analytical || m == ModelDisplayMode::FiniteElement; }
/// Dalles, voiles, fondations en arêtes seules (ne masquent ni nœuds ni axes).
inline bool planarAsEdges(ModelDisplayMode m) { return linearAsAxis(m); }
/// Axes analytiques superposés à la géométrie physique.
inline bool analyticalOverlay(ModelDisplayMode m) { return m == ModelDisplayMode::Overlay; }
/// Maillage du solveur affiché.
inline bool solverMesh(ModelDisplayMode m) { return m == ModelDisplayMode::FiniteElement; }
/// Transparence minimale imposée aux solides (Superposition), 0 sinon.
inline double physicalTransparency(ModelDisplayMode m) { return m == ModelDisplayMode::Overlay ? 0.6 : 0.0; }
} // namespace DisplayPolicy

/// Axe analytique d'une barre : segment nœud à nœud (forme nulle si les nœuds sont confondus).
TopoDS_Shape analyticalAxisShape(const gp_Pnt& a, const gp_Pnt& b);

/// Couleur des axes analytiques par famille (lisible sur les fonds clair et sombre de TSA).
Quantity_Color analyticalColor(TSA::Analysis::StructuralElementKind kind);

/// Famille d'affichage d'une barre (Model::Beam) selon son rôle : un poteau saisi comme barre de rôle
/// « Poteau » s'affiche comme un poteau ; contreventement, tirant, treillis comme un treillis.
TSA::Analysis::StructuralElementKind analyticalKindOf(TSA::Model::BarRole role);

/// État du maillage du solveur pour le mode Éléments finis.
struct SolverMeshStatus
{
    bool available = false;
    std::string message;    ///< explication affichée à l'utilisateur
};

/// results : derniers résultats (nul s'il n'y en a pas). planarElements : dalles + voiles du modèle,
/// signalés s'ils ne figurent pas dans le maillage.
SolverMeshStatus solverMeshStatus(const TSA::Analysis::ResultsModel* results, std::size_t planarElements);

/// Géométrie à tracer pour un maillage : segments (arêtes des éléments), nœuds issus du modèle,
/// nœuds propres au moteur (subdivisions, auxiliaires).
struct SolverMeshGeometry
{
    std::vector<std::pair<gp_Pnt, gp_Pnt>> segments;
    std::vector<gp_Pnt> modelNodes;
    std::vector<gp_Pnt> solverNodes;
    std::size_t cells = 0;          ///< éléments tracés
    std::size_t hiddenCells = 0;    ///< éléments dont l'élément TSA est masqué (isolation, filtres)
};

/// visible(key) : l'élément TSA d'origine est-il affiché ? (éléments propres au moteur : toujours tracés).
SolverMeshGeometry buildSolverMeshGeometry(const TSA::Analysis::SolverMesh& mesh,
                                           const std::function<bool(const TSA::Analysis::ElementKey&)>& visible = {});

} // namespace TSA::Viewer
