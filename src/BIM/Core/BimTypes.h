#pragma once

// Types de base du modèle BIM de TSA. Les catégories reprennent les entités IFC 4.3
// (ISO 16739-1:2024) qui leur correspondent ; aucune catégorie n'est créée si une entité IFC
// existe déjà pour le même rôle. Référence : docs/BIM_ARCHITECTURE.md, docs/IFC_MAPPING.md

#include "../../Model/SelectionQuery.h"   // ElementKind (élément analytique TSA)

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace TSA::BIM
{

/// Catégories d'objets. Physiques et spatiales : produits IFC ; analytiques et résultats :
/// objets du modèle analytique (IfcStructural* à l'échange) ou des calculs.
enum class BimCategory
{
    // Spatial
    Project, Site, Building, BuildingStorey, Grid, Space,
    // Physique (produits porteurs)
    Column, Beam, Member, Slab, Wall, Footing, Pile, Plate,
    // Physique, autre produit importé (porte, escalier, revêtement, objet générique…) : classe IFC d'origine
    // conservée dans PhysicalElement::ifcClass, IfcBuildingElementProxy à défaut.
    Proxy,
    // Analytique
    AnalyticalNode, AnalyticalMember, AnalyticalSurface, BoundaryCondition, Release, LoadApplication
};

/// Passage au modèle de calcul d'un produit physique. Un produit importé sans élément de calcul n'est jamais
/// converti en silence : il attend une préparation explicite (docs/IFC_MAPPING.md, « Préparer le modèle de calcul »).
enum class AnalyticalStatus
{
    Analytical,         ///< représenté par un ou plusieurs éléments de calcul
    NeedsPreparation,   ///< porteur probable, sans élément de calcul : préparation requise
    NotStructural       ///< non porteur (porte, fenêtre, revêtement…) : conservé, exclu du calcul
};

const char* analyticalStatusKey(AnalyticalStatus s);   ///< "analytical", "to_prepare", "not_structural"
bool analyticalStatusFromKey(const std::string& key, AnalyticalStatus& out);

/// Entité IFC 4.3 correspondante ("IfcBeam"…) ; vide si aucune (objet interne).
const char* ifcEntity(BimCategory c);
const char* categoryName(BimCategory c);
bool categoryFromName(const std::string& name, BimCategory& out);

/// Référence d'un élément analytique TSA existant (nœud, barre, dalle, voile, fondation…).
struct AnalyticalRef
{
    TSA::Model::ElementKind kind = TSA::Model::ElementKind::Beam;
    int id = 0;
    bool operator<(const AnalyticalRef& o) const { return kind != o.kind ? kind < o.kind : id < o.id; }
    bool operator==(const AnalyticalRef& o) const { return kind == o.kind && id == o.id; }
    std::string label() const;   ///< « B12 », « C3 », « N7 »…
};

const char* elementKindKey(TSA::Model::ElementKind k);   ///< "beam", "column"… (sérialisation)
bool elementKindFromKey(const std::string& key, TSA::Model::ElementKind& out);

/// Valeur de propriété typée (IfcPropertySingleValue).
struct PropertyValue
{
    enum class Type { Text, Label, Real, Integer, Boolean, Length, Force, Pressure, Ratio };
    Type type = Type::Text;
    std::string text;
    double number = 0.0;
    bool flag = false;

    static PropertyValue ofText(std::string s, Type t = Type::Text) { PropertyValue v; v.type = t; v.text = std::move(s); return v; }
    static PropertyValue ofReal(double d, Type t = Type::Real) { PropertyValue v; v.type = t; v.number = d; return v; }
    static PropertyValue ofBool(bool b) { PropertyValue v; v.type = Type::Boolean; v.flag = b; return v; }
    std::string toString() const;
    bool operator==(const PropertyValue& o) const
    {
        return type == o.type && text == o.text && number == o.number && flag == o.flag;
    }
};

/// Jeu de propriétés (IfcPropertySet) : nom (« Pset_BeamCommon », « Pset_TSA_Structural »…).
struct PropertySet
{
    std::string name;
    std::map<std::string, PropertyValue> properties;
};

/// Référence de classification (IfcClassificationReference), ex. Uniclass, OmniClass.
struct ClassificationReference
{
    std::string system;        ///< « Uniclass 2015 »
    std::string identification;///< « Ss_20_10_75 »
    std::string name;
};

} // namespace TSA::BIM
