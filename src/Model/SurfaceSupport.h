#pragma once

// Appui linéique sur un bord d'une surface (dalle, voile) : condition aux limites appliquée à tous les nœuds du maillage
// situés sur ce bord (docs/SURFACES.md). Par degré de liberté (Tx, Ty, Tz, Rx, Ry, Rz, axes globaux) : bloqué, élastique
// (raideur par unité de longueur, répartie sur les nœuds au prorata de leur longueur d'influence) ou libre.
//
// Bords : dalle — bord i = côté du contour allant du sommet i au sommet i+1 ; voile — 0 pied, 1 extrémité de fin, 2 tête,
// 3 extrémité de début (rectangle brut ; une porte au pied interrompt l'appui, faute de nœuds). edge = -1 : tout le contour.

#include <array>
#include <string>

namespace TSA::Model
{

struct SurfaceEdgeSupport
{
    int edge = -1;                          ///< bord visé, -1 = tout le contour
    std::array<bool, 6> fixed {};           ///< DDL bloqués (Tx, Ty, Tz, Rx, Ry, Rz)
    std::array<double, 6> stiffness {};     ///< raideurs par unité de longueur : kN/m/m (translations), kN·m/rad/m (rotations)
    std::string name;

    bool any() const
    {
        for (int d = 0; d < 6; ++d)
            if (fixed[d] || stiffness[d] > 0.0) return true;
        return false;
    }
    bool operator==(const SurfaceEdgeSupport& o) const
    {
        return edge == o.edge && fixed == o.fixed && stiffness == o.stiffness && name == o.name;
    }
};

} // namespace TSA::Model
