#pragma once

// Références structurelles d'un plan de travail, calculées depuis la grille structurelle du projet (intersections réelles
// des grilles cartésiennes, cylindriques et quelconques, origine et rotation comprises) — jamais depuis des valeurs codées
// en dur ni en supposant un espacement régulier. Les identifiants d'axes (A, B, 1, 2, « 5 »…) restent des libellés : seule
// la position géométrique sert au calcul.
//
// Repère du plan : origine O, base orthonormée (U, V, N). Un point P du monde a pour coordonnées locales
// u = (P − O)·U, v = (P − O)·V, w = (P − O)·N ; P est dans le plan si |w| ≤ tolérance.

#include "GridSystem.h"
#include "../Coordinate/WorkPlane.h"

#include <gp_Pnt.hxx>

#include <optional>
#include <string>
#include <vector>

namespace TSA::Grid
{

/// Plan parallèle au plan de travail passant par des intersections de la grille : décalage le long de N depuis l'origine.
struct PlaneReference
{
    double offset = 0.0;
    std::string label;   ///< « axe 3 », « niveau R+1 »… (libellé commun des intersections du plan), sinon vide
};

/// Intersection de la grille située dans le plan de travail.
struct InPlaneIntersection
{
    gp_Pnt point;
    double u = 0.0, v = 0.0;
    std::string label;   ///< « B / 3 », « B / 3 / R+1 »… selon la grille
};

class WorkPlaneReferences
{
public:
    /// Tolérance par défaut (m) : regroupement des intersections d'un même plan, appartenance au plan.
    static constexpr double kTolerance = 1e-6;

    /// Toutes les intersections des grilles données (visibles), avec leur libellé.
    static std::vector<InPlaneIntersection> allIntersections(const std::vector<const GridSystem*>& grids,
                                                             const TSA::Coordinate::WorkPlane& wp);
    /// Plans parallèles passant par des intersections, triés par décalage croissant (espacement réel quelconque).
    static std::vector<PlaneReference> parallelPlanes(const std::vector<const GridSystem*>& grids, const TSA::Coordinate::WorkPlane& wp,
                                                      double tol = kTolerance);
    /// Intersections situées dans le plan, triées par (v, u).
    static std::vector<InPlaneIntersection> intersectionsInPlane(const std::vector<const GridSystem*>& grids,
                                                                 const TSA::Coordinate::WorkPlane& wp, double tol = kTolerance);
    /// Plan parallèle suivant (direction > 0, décalage > 0) ou précédent (direction < 0) ; aucun au-delà des extrêmes.
    static std::optional<PlaneReference> nextPlane(const std::vector<const GridSystem*>& grids, const TSA::Coordinate::WorkPlane& wp,
                                                   int direction, double tol = kTolerance);
    /// Intersection du plan suivante / précédente après l'origine, dans l'ordre (v, u).
    static std::optional<InPlaneIntersection> nextIntersection(const std::vector<const GridSystem*>& grids,
                                                               const TSA::Coordinate::WorkPlane& wp, int direction, double tol = kTolerance);
    /// Intersection du plan la plus proche d'un point (projeté sur le plan).
    static std::optional<InPlaneIntersection> nearestIntersection(const std::vector<const GridSystem*>& grids,
                                                                  const TSA::Coordinate::WorkPlane& wp, const gp_Pnt& target, double tol = kTolerance);
};

} // namespace TSA::Grid
