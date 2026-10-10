#pragma once

// Cotations 3D : annotations du modèle (jamais transmises au calcul, à la masse, aux charges ni au
// maillage). Une cotation conserve ses points d'ancrage (nœud associé ou point fixe) et sa position ;
// sa géométrie et sa valeur sont recalculées à partir des coordonnées réelles du modèle
// (DimensionGeometry), son rendu est construit à part (src/Viewer/DimensionRenderer).
// Persistance : chunk DIMS du .tsa (format 1.6). Référence : docs/DIMENSIONS.md

#include <array>
#include <map>
#include <string>
#include <vector>

namespace TSA::Annotation
{

enum class DimensionKind
{
    Linear,      ///< distance entre deux points, mesurée selon MeasureAxis
    Angular,     ///< angle au sommet (sommet, point du 1er bras, point du 2e bras)
    Level,       ///< altitude d'un point (Z − référence de niveau)
    Chain,       ///< distances successives entre plusieurs points
    Cumulative   ///< distances depuis le premier point (origine commune)
};

enum class MeasureAxis
{
    Aligned,     ///< vraie grandeur (direction premier → dernier point)
    X,
    Y,
    Z,           ///< verticale
    Horizontal   ///< projection sur le plan horizontal
};

/// Point d'ancrage : nœud associé (suivi automatiquement) ou point fixe.
struct DimensionAnchor
{
    int nodeId = -1;                       ///< > 0 : associé au nœud
    std::array<double, 3> point{ 0, 0, 0 }; ///< point fixe, ou dernière position connue du nœud
    bool orphaned = false;                 ///< le nœud associé a été supprimé : référence invalide

    bool associative() const { return nodeId > 0 && !orphaned; }
    bool operator==(const DimensionAnchor& o) const = default;
};

struct Dimension
{
    int id = 0;
    DimensionKind kind = DimensionKind::Linear;
    MeasureAxis axis = MeasureAxis::Aligned;
    std::vector<DimensionAnchor> anchors;
    std::array<double, 3> position{ 0, 0, 0 }; ///< emplacement de la ligne de cote (ou de l'arc, du texte)
    std::string textOverride;                  ///< texte imposé ; « <> » est remplacé par la valeur mesurée
    std::string color;                         ///< couleur propre (#RRGGBB) ; vide : style du projet

    bool hasInvalidReference() const;
    /// Nombre d'ancrages attendu (Chain / Cumulative : au moins 2).
    static int requiredAnchors(DimensionKind kind);
    bool operator==(const Dimension& o) const = default;
};

enum class LengthUnit
{
    Meter,
    Centimeter,
    Millimeter
};

struct DimensionStyle
{
    LengthUnit unit = LengthUnit::Meter;
    int decimals = 3;
    double rounding = 0.0;          ///< pas d'arrondi dans l'unité affichée (0 : aucun)
    bool showUnit = true;
    int angleDecimals = 1;
    double textHeightPx = 14.0;     ///< hauteur du texte à l'écran (taille constante au zoom)
    double arrowSizePx = 12.0;      ///< longueur des flèches à l'écran
    double extensionGap = 0.05;     ///< m : écart entre le point mesuré et la ligne d'attache
    double extensionOvershoot = 0.10; ///< m : dépassement de la ligne d'attache au-delà de la ligne de cote
    double levelReference = 0.0;    ///< m : cote de référence des cotations de niveau
    std::string color = "#FFD54F";
    std::string invalidColor = "#FF5252";
    bool textInPlane = false;       ///< false : texte face à la caméra ; true : dans le plan de la cotation
    bool visible = true;

    std::string unitSymbol() const;
    double unitFactor() const;      ///< m → unité affichée
    bool operator==(const DimensionStyle& o) const = default;
};

/// Ensemble des cotations d'un projet (partie du modèle, comprise dans l'historique Annuler).
struct DimensionSet
{
    std::map<int, Dimension> items;
    DimensionStyle style;
    int nextId = 1;

    bool operator==(const DimensionSet& o) const = default;

    /// JSON UTF-8 versionné (« schema »). Lecture tolérante : entrée invalide ignorée et signalée.
    std::string toJson() const;
    static DimensionSet fromJson(const std::string& json, std::string* warnings = nullptr);
};

std::string kindKey(DimensionKind k);
std::string axisKey(MeasureAxis a);
std::string kindDisplayName(DimensionKind k, MeasureAxis a);

} // namespace TSA::Annotation
