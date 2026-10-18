#pragma once

// Métrés dérivés du modèle (longueurs, surfaces nettes, volumes, masses), jamais saisis : recalculés
// à chaque appel à partir des nœuds, sections, épaisseurs, ouvertures et matériaux. Par élément
// analytique, puis cumulés par produit physique, matériau, section / épaisseur et famille.
// Référence : docs/BIM_ARCHITECTURE.md, « Quantités ».

#include "BimModel.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM
{

struct ElementQuantities
{
    AnalyticalRef ref;
    std::string label;          ///< « B3 », « S1 »…
    std::string family;         ///< « Poutre », « Dalle »…
    std::string material;       ///< nom du matériau
    std::string section;        ///< section (éléments linéaires) ou « ép. 0.200 m » (dalles, voiles), « 1.50 × 1.50 × 0.50 m » (semelles)
    double length = 0.0;        ///< m : axe des éléments linéaires, longueur en plan d'un voile
    double grossArea = 0.0;     ///< m² : dalle (contour), voile (élévation L × H), semelle (A × B)
    double openingArea = 0.0;   ///< m² : ouvertures déduites
    double area = 0.0;          ///< m² : surface nette = brute − ouvertures
    double volume = 0.0;        ///< m³
    double mass = 0.0;          ///< kg : volume × masse volumique du matériau
};

struct QuantityTotals
{
    int count = 0;
    double length = 0.0;
    double area = 0.0;
    double volume = 0.0;
    double mass = 0.0;

    void add(const ElementQuantities& q);
};

struct QuantityReport
{
    std::vector<ElementQuantities> elements;            ///< dans l'ordre des familles, puis des ids
    std::map<int, QuantityTotals> byProduct;             ///< id de produit physique
    std::map<std::string, QuantityTotals> byMaterial;
    std::map<std::string, QuantityTotals> bySection;
    std::map<std::string, QuantityTotals> byFamily;
    QuantityTotals total;
};

/// Quantités d'un élément analytique (nœuds et éléments inexistants : rien).
std::optional<ElementQuantities> elementQuantities(const TSA::Model::Model& model, const AnalyticalRef& ref);
/// Somme des éléments analytiques d'un produit physique (un produit importé sans élément de calcul : vide).
QuantityTotals productQuantities(const TSA::Model::Model& model, const PhysicalElement& product);
/// Métré complet du modèle.
QuantityReport takeoff(const TSA::Model::Model& model);

} // namespace TSA::BIM
