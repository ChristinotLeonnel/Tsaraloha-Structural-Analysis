#pragma once

// Repères d'axes de grille pertinents pour une vue (responsabilités 3 et 4 : choix et placement).
// La géométrie des grilles (CartesianGrid, CylindricalGrid) ne sait rien de la caméra ; le rendu
// (GridLabelRenderer) ne sait rien des règles : il dessine la liste produite ici.
//
// Règles (une seule occurrence par axe et par vue, sauf GridDisplaySettings::labelsBothEnds) :
//   - Plan (visée ∥ Z)           : lettres et chiffres au niveau actif (le plus proche), bulle couchée.
//   - Élévation visée ∥ Y local  : seuls les axes X (A, B, C…) sont lisibles ; au-dessus du dernier niveau.
//   - Élévation visée ∥ X local  : seuls les axes Y (1, 2, 3…) ; au-dessus du dernier niveau.
//   - Axonométrie / 3D           : les deux familles au niveau le plus bas uniquement (aucune copie par étage).
// Les étiquettes de niveaux (colonne Z) sont omises en plan, où elles se superposeraient toutes.

#include "CartesianGrid.h"
#include "CylindricalGrid.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <cmath>
#include <string>
#include <vector>

namespace TSA::Grid
{

enum class GridViewKind
{
    Plan,             ///< visée verticale (vue de dessus / dessous)
    ElevationAlongX,  ///< visée parallèle à l'axe X local de la grille : plan Y-Z visible
    ElevationAlongY,  ///< visée parallèle à l'axe Y local de la grille : plan X-Z visible
    Axonometric       ///< toute autre orientation (vue 3D)
};

struct GridLabelView
{
    GridViewKind kind = GridViewKind::Axonometric;
    bool hasActiveLevel = false;
    double activeLevelZ = 0.0;   ///< altitude globale du niveau actif (vue en plan)

    bool sameLayoutAs(const GridLabelView& o) const
    {
        if (kind != o.kind) return false;
        if (kind != GridViewKind::Plan) return true;
        return hasActiveLevel == o.hasActiveLevel && std::abs(activeLevelZ - o.activeLevelZ) < 1e-6;
    }
};

struct PlacedGridLabel
{
    gp_Pnt position;
    std::string text;
    gp_Dir bubbleNormal { 0.0, 0.0, 1.0 };   ///< normale du cercle de bulle (face à la vue)
    bool isBold = false;
    bool isLevelLabel = false;               ///< étiquette de niveau (colonne Z), sans bulle
    char family = 'X';                       ///< 'X', 'Y' (cartésien), 'R', 'A' (cylindrique), 'Z' (niveau)
    int index = -1;
};

/// Classe une direction de visée (caméra, de l'œil vers la cible) dans le repère de la grille.
GridViewKind classifyGridView(const gp_Dir& viewDirection, double gridRotationDeg, double toleranceDeg = 10.0);

std::vector<PlacedGridLabel> layoutCartesianLabels(const CartesianGrid& grid, const GridLabelView& view);
std::vector<PlacedGridLabel> layoutCylindricalLabels(const CylindricalGrid& grid, const GridLabelView& view);

} // namespace TSA::Grid
