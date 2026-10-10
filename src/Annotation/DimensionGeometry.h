#pragma once

// Géométrie des cotations : points d'ancrage résolus sur le modèle, puis lignes d'attache, ligne de
// cote, flèches, textes et valeurs — toujours calculés à partir des coordonnées réelles (jamais de la
// projection à l'écran). Fonctions pures, testées (tests/test_dimensions.cpp).

#include "Dimension.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <string>
#include <utility>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Annotation
{

struct DimensionArrow
{
    gp_Pnt tip;
    gp_Dir direction; ///< du corps de la flèche vers la pointe
};

struct DimensionText
{
    gp_Pnt position;
    std::string text;
    gp_Dir along{ 1, 0, 0 };  ///< direction de lecture (texte dans le plan)
    gp_Dir up{ 0, 0, 1 };     ///< côté opposé aux points mesurés
};

struct DimensionLayout
{
    std::vector<std::pair<gp_Pnt, gp_Pnt>> segments; ///< lignes d'attache, ligne de cote, arc, repère
    std::vector<DimensionArrow> arrows;
    std::vector<DimensionText> texts;
    std::vector<gp_Pnt> levelMarkers;  ///< symbole de niveau (triangle) à ces points
    std::vector<double> values;        ///< valeurs mesurées (m, ou degrés pour l'angle)
    bool valid = false;                ///< false : géométrie dégénérée (voir error)
    bool invalidReference = false;     ///< un ancrage a perdu son nœud
    std::string error;
};

/// Points d'ancrage : position du nœud associé s'il existe, sinon point fixe (dernière position connue).
std::vector<gp_Pnt> resolveAnchors(const TSA::Model::Model& model, const Dimension& dim, bool* invalidReference = nullptr);

/// Mise en page à partir de points déjà résolus (aperçu des outils, tests).
DimensionLayout computeLayout(const Dimension& dim, const std::vector<gp_Pnt>& points, const DimensionStyle& style);

/// Résolution + mise en page.
DimensionLayout layoutFor(const TSA::Model::Model& model, const Dimension& dim, const DimensionStyle& style);

std::string formatLength(const DimensionStyle& style, double meters);
std::string formatAngle(const DimensionStyle& style, double degrees);
std::string formatLevel(const DimensionStyle& style, double meters);
/// Texte imposé : « <> » remplacé par la valeur ; vide → valeur.
std::string applyTextOverride(const std::string& override, const std::string& value);

/// Cotation linéaire automatique : axe global (X, Y ou Z) choisi selon la position du curseur par
/// rapport aux deux points (le curseur s'écarte perpendiculairement à l'axe mesuré).
MeasureAxis autoLinearAxis(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& cursor);

} // namespace TSA::Annotation
