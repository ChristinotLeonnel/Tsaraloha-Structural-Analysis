#pragma once

// Repère du modèle (axes X / Y, altitudes) et niveaux, à partir de la grille du projet.
//
// Sources de vérité :
//   - axes X / Y : la grille active du GridManager. Le repère du modèle n'en garde qu'une copie dérivée,
//     reconstruite ici à chaque changement de grille (et à l'ouverture d'un projet ou d'un template) ;
//   - niveaux horizontaux : les objets Level du modèle (LevelManager). Ils sont créés depuis la grille par une action
//     explicite (fenêtre Grille, nouveau projet) ou par un import IFC / TSA3D (étages référencés par les objets BIM),
//     et ne sont jamais écrasés par une simple modification de la grille.
// Les positions d'une grille sont mesurées depuis son origine : les coordonnées globales ajoutent l'origine.

namespace TSA::Coordinate
{
class CoordinateSystem;
class LevelManager;
}
namespace TSA::Model
{
class Model;
}

namespace TSA::Grid
{

class GridDefinition;
class GridManager;

/// Axes et altitudes de la grille dans le repère (coordonnées globales). Seule une grille cartésienne non tournée
/// définit des plans X = cte et Y = cte : les axes d'une grille tournée ou cylindrique ne sont pas proposés comme
/// plans globaux (ses altitudes le sont). grid nul : axes et altitudes vidés.
void syncCoordinateSystem(const GridDefinition* grid, TSA::Coordinate::CoordinateSystem& cs);

/// Niveaux du modèle remplacés par les altitudes de la grille (action explicite de l'utilisateur).
void applyGridLevels(const GridDefinition& grid, TSA::Coordinate::LevelManager& levels);

/// Niveaux fixes {0 ; 3 ; 6,5 ; 10 m} « RDC (Z=0.00) »… que les versions antérieures imposaient à tout modèle,
/// quelle que soit sa grille (et enregistraient dans le .tsa).
bool hasLegacyDefaultLevels(const TSA::Coordinate::LevelManager& levels);

/// Après l'ouverture d'un projet ou d'un template : repère reconstruit depuis la grille active ; les niveaux fixes
/// hérités sont remplacés par les altitudes de la grille, les autres niveaux (objets du projet) sont conservés.
void reconcileProject(TSA::Model::Model& model, const GridManager* grids);

} // namespace TSA::Grid
