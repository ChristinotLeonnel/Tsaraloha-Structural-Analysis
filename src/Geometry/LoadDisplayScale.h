#pragma once

// Échelle d'affichage des charges (docs/DISPLAY_SCALES.md) : longueur graphique des flèches en fonction de l'intensité,
// SANS jamais modifier la valeur de la charge (seule la représentation change).
//
// Les charges sont regroupées en familles de même unité (une force en kN ne se compare pas à une charge linéique en
// kN/m) ; dans une famille, la flèche de l'intensité maximale vaut la longueur de référence × facteur, les autres
// lui sont proportionnelles :
//   longueur = L_réf × facteur × |I| / I_max(famille), bornée à [L_min, L_max]
//   - proportionnel : rapports d'intensité exacts (seule une longueur minimale de visibilité de 3 % s'applique) ;
//   - automatique   : longueur minimale garantie (25 % par défaut) : les charges faibles restent visibles,
//                     rapports conservés au-dessus de ce seuil ;
//   - normalisé     : toutes les flèches à L_réf × facteur ; l'intensité est donnée par le texte (ne prétend pas
//                     être proportionnel).
// L_réf est fournie par la vue (longueur à peu près constante à l'écran, adaptée au zoom par paliers).

#include <array>
#include <string>

namespace TSA::Model
{
class LoadManager;
}

namespace TSA::Geometry
{

enum class LoadFamily
{
    Force = 0,        ///< kN (forces nodales, forces ponctuelles sur barre)
    Moment,           ///< kN·m (moments nodaux)
    LineForce,        ///< kN/m (charges réparties sur barre)
    LineMoment,       ///< kN·m/m (moments répartis)
    Pressure,         ///< kN/m² (charges surfaciques)
    Count
};

const char* loadFamilyUnit(LoadFamily f);
const char* loadFamilyName(LoadFamily f);

enum class LoadScaleMode
{
    Proportional,
    Automatic,
    Normalized
};

struct LoadScaleSettings
{
    LoadScaleMode mode = LoadScaleMode::Automatic;
    double factor = 1.0;            ///< facteur global d'affichage
    double minRatio = 0.25;         ///< mode automatique : longueur minimale / longueur de référence
    double maxRatio = 1.0;          ///< longueur maximale / longueur de référence (flèche de l'intensité maximale)
    bool showValues = true;
    bool showUnits = true;
    bool showLegend = true;
    bool operator==(const LoadScaleSettings&) const = default;
};

/// Intensités maximales (valeur absolue) par famille, sur toutes les charges du modèle.
struct LoadFamilyMaxima
{
    std::array<double, static_cast<int>(LoadFamily::Count)> max {};
    double of(LoadFamily f) const { return max[static_cast<int>(f)]; }
    bool operator==(const LoadFamilyMaxima&) const = default;
};
LoadFamilyMaxima loadFamilyMaxima(const TSA::Model::LoadManager& loads);

/// Longueur graphique (m) d'une flèche d'intensité « intensity » (valeur absolue utilisée).
double loadArrowLength(double intensity, double familyMax, double referenceLength, const LoadScaleSettings& s);

/// Texte de légende : « kN : 1 m de flèche = 50 kN » par famille présente.
std::string loadScaleLegend(const LoadFamilyMaxima& maxima, double referenceLength, const LoadScaleSettings& s);

} // namespace TSA::Geometry
