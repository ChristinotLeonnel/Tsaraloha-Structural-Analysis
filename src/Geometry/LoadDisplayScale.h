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
// L_réf vient de la STRUCTURE (structureLoadReferenceLength), jamais du zoom : en zoomant, flèches et valeurs grandissent
// avec la scène et deviennent lisibles (2026-11-01 ; auparavant longueur constante à l'écran, adaptée au zoom par paliers).

#include <array>
#include <string>

namespace TSA::Model
{
class LoadManager;
class Model;
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

/// Intensités par famille, sur toutes les charges du modèle (tous les cas : l'échelle ne change pas d'un cas affiché à
/// l'autre, les flèches de G et de Q restent comparables).
struct LoadFamilyMaxima
{
    std::array<double, static_cast<int>(LoadFamily::Count)> max {};     ///< intensité maximale (valeur absolue)
    /// Échelle robuste : le maximum, sauf charge exceptionnelle (maximum > 3 × 90e centile) : 3 × 90e centile. Les autres
    /// charges ne sont plus écrasées par une seule valeur hors norme, plafonnée à la longueur maximale (valeur affichée).
    std::array<double, static_cast<int>(LoadFamily::Count)> scale {};
    double of(LoadFamily f) const { return max[static_cast<int>(f)]; }
    double scaleOf(LoadFamily f) const { return scale[static_cast<int>(f)]; }
    bool operator==(const LoadFamilyMaxima&) const = default;
};
LoadFamilyMaxima loadFamilyMaxima(const TSA::Model::LoadManager& loads);

/// Longueur de référence des flèches (m) d'après la structure, jamais le zoom : 35 % de la hauteur d'étage typique (médiane
/// des éléments quasi verticaux), à défaut 30 % de la longueur médiane des barres, bornée à [1 %, 10 %] de l'envergure ; 1 m
/// sans barre. La flèche d'intensité maximale tient ainsi sous le plancher supérieur (R+7, étages de 3 m : 1,05 m).
double structureLoadReferenceLength(const TSA::Model::Model& model);

/// Longueur graphique (m) d'une flèche d'intensité « intensity » (valeur absolue utilisée).
double loadArrowLength(double intensity, double familyMax, double referenceLength, const LoadScaleSettings& s);

/// Texte de légende : « kN : 1 m de flèche = 50 kN » par famille présente.
std::string loadScaleLegend(const LoadFamilyMaxima& maxima, double referenceLength, const LoadScaleSettings& s);

} // namespace TSA::Geometry
