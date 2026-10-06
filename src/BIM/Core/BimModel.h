#pragma once

// Modèle BIM de TSA : produits physiques, structure spatiale, identités, propriétés.
// Les éléments TSA existants (nœuds, barres, dalles, voiles, fondations) constituent le modèle
// ANALYTIQUE ; chaque produit physique (IfcBeam, IfcColumn…) référence explicitement un ou
// plusieurs éléments analytiques (PhysicalToAnalyticalMap, 1 → N). Le BimModel vit dans le
// TSA::Model::Model : même snapshot (Annuler / Rétablir), même fichier .tsa (chunk BIMM).
// Référence : docs/BIM_ARCHITECTURE.md, docs/ANALYTICAL_MODEL.md

#include "BimTypes.h"

#include <QJsonObject>

#include <map>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM
{

/// Produit physique BIM (élément porteur).
struct PhysicalElement
{
    int id = 0;                      ///< identifiant interne TSA, stable, jamais réutilisé
    std::string globalId;            ///< IfcGloballyUniqueId (22 car.), distinct de id
    BimCategory category = BimCategory::Beam;
    std::string predefinedType;      ///< IFC PredefinedType (BEAM, COLUMN, BRACE, PAD_FOOTING…)
    std::string name;
    std::string objectType;
    std::string tag;
    std::string description;
    std::string storeyLevelId;       ///< étage imposé (id de niveau) ; vide = déduit de la géométrie
    std::vector<AnalyticalRef> analytical;   ///< éléments analytiques, dans l'ordre de l'axe
    std::vector<PropertySet> propertySets;   ///< Psets saisis (les Psets calculés sont ajoutés à l'export)
    std::vector<ClassificationReference> classifications;

    const PropertySet* propertySet(const std::string& name) const;
};

struct SpatialElement
{
    std::string globalId;
    std::string name;
    std::string description;
};

/// Projet / Site / Bâtiment ; un étage IFC par niveau TSA (LevelManager).
struct SpatialStructure
{
    SpatialElement project { {}, "Projet TSA", {} };
    SpatialElement site { {}, "Site", {} };
    SpatialElement building { {}, "Bâtiment", {} };
    SpatialElement analysisModel { {}, "Modèle analytique TSA", {} };   ///< IfcStructuralAnalysisModel
    std::map<std::string, std::string> storeyGlobalIds;   ///< id de niveau → GlobalId de l'étage
};

class BimModel
{
public:
    const std::map<int, PhysicalElement>& elements() const { return m_elements; }
    const PhysicalElement* element(int id) const;
    PhysicalElement* element(int id);
    const PhysicalElement* elementByGlobalId(const std::string& globalId) const;
    /// Produit physique contenant cet élément analytique (nullptr si aucun).
    const PhysicalElement* physicalOf(const AnalyticalRef& ref) const;
    /// GlobalId de l'objet analytique (IfcStructuralPointConnection / CurveMember…).
    std::string analyticalGlobalId(const AnalyticalRef& ref) const;
    const std::map<AnalyticalRef, std::string>& analyticalGlobalIds() const { return m_analyticalGuids; }

    const SpatialStructure& spatial() const { return m_spatial; }
    SpatialStructure& spatial() { return m_spatial; }

    /// Met en cohérence avec le modèle analytique : un produit (1:1) pour tout élément analytique
    /// non rattaché, références orphelines retirées, produits vides supprimés, GlobalId attribués
    /// (produits, objets analytiques, projet / site / bâtiment / étages). Idempotent ; retourne
    /// true si quelque chose a changé.
    bool synchronize(const TSA::Model::Model& model);

    /// Division d'une barre : le tronçon créé rejoint le produit physique de l'original, juste
    /// après lui (une poutre physique divisée reste UNE poutre, N éléments analytiques).
    void attachSplit(const AnalyticalRef& original, const AnalyticalRef& created);
    /// Copie : les copies d'éléments d'un même produit forment un nouveau produit (métadonnées
    /// copiées, nouveaux identifiants).
    void registerCopies(const std::vector<std::pair<AnalyticalRef, AnalyticalRef>>& originalToCopy);
    /// Regroupe des éléments analytiques en un seul produit physique (le premier produit est
    /// conservé, les autres produits vidés sont supprimés). Retourne l'id du produit.
    int group(const std::vector<AnalyticalRef>& refs);
    /// Sépare un élément analytique dans son propre produit. Retourne l'id du nouveau produit.
    int ungroup(const AnalyticalRef& ref);

    /// Import : ajoute un produit en conservant son GlobalId (s'il est valide et libre) ; ses
    /// éléments analytiques sont retirés de tout autre produit. Retourne l'id interne attribué.
    int insert(PhysicalElement e);
    /// Import : impose le GlobalId d'un objet analytique (ignoré s'il n'est pas valide).
    void setAnalyticalGlobalId(const AnalyticalRef& ref, const std::string& globalId);

    /// Étage (id de niveau) du produit : imposé, sinon niveau du nœud le plus bas (Node::levelId),
    /// sinon niveau de cote ≤ z la plus haute. Vide s'il n'y a aucun niveau.
    std::string resolvedStorey(const TSA::Model::Model& model, const PhysicalElement& e) const;

    QJsonObject toJson() const;
    /// Lecture tolérante (schéma versionné) ; false si le schéma est d'une version future.
    static bool fromJson(const QJsonObject& json, BimModel& out);

    bool empty() const { return m_elements.empty() && m_analyticalGuids.empty(); }

private:
    int create(const PhysicalElement& templ);
    void rebuildIndex();

    std::map<int, PhysicalElement> m_elements;
    int m_nextId = 1;
    std::map<AnalyticalRef, std::string> m_analyticalGuids;
    SpatialStructure m_spatial;
    std::map<AnalyticalRef, int> m_index;   ///< dérivé (non sérialisé)
};

/// Nom par défaut d'un produit sans nom (« Poutre B3 ») : utilisé à l'export, ignoré à l'import.
std::string defaultProductName(const PhysicalElement& e);
/// Repère par défaut d'un produit sans repère : libellés analytiques (« B1,B2 »).
std::string defaultProductTag(const PhysicalElement& e);

/// Catégorie et type prédéfini IFC par défaut d'un élément analytique TSA.
std::pair<BimCategory, std::string> defaultCategory(const TSA::Model::Model& model, const AnalyticalRef& ref);

} // namespace TSA::BIM
