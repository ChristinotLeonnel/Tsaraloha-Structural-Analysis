#pragma once

// Géométrie physique des produits BIM importés (PhysicalElement::geometry : maillages IFC) dans la vue 3D.
// Un objet OCCT par produit : surbrillance au survol, désignation au clic (OccView::bimProductPicked), visibilité.
// Seuls les produits SANS élément de calcul sont tracés (« à préparer », non porteurs) : les autres sont représentés
// par leurs éléments TSA (barres, dalles…), qui restent la référence du calcul — pas de doublon superposé.
// Ombrage plat (sommets dédoublés par triangle : les maillages soudés d'IfcOpenShell lisseraient les arêtes vives) ;
// sommets relatifs à l'origine du maillage (float), placés par une transformation locale (double) : les coordonnées
// géoréférencées ne perdent pas leur précision dans les tampons graphiques.

#include "../BIM/Core/BimGeometry.h"

#include <AIS_InteractiveContext.hxx>
#include <Bnd_Box.hxx>
#include <SelectMgr_EntityOwner.hxx>

#include <map>

namespace TSA::BIM
{
class BimModel;
struct PhysicalElement;
}

namespace TSA::Viewer
{

class BimGeometryRenderer
{
public:
    explicit BimGeometryRenderer(const Handle(AIS_InteractiveContext)& context) : m_context(context) {}
    ~BimGeometryRenderer() { clear(); }

    /// Produit tracé par cette couche : maillage valide et aucun élément de calcul.
    static bool drawn(const TSA::BIM::PhysicalElement& e);

    /// Met les objets en accord avec le modèle BIM : crée les manquants, retire les produits disparus ou désormais
    /// reliés au calcul, garde les autres (même maillage). Sans mise à jour de la vue ; true si quelque chose a changé.
    bool sync(const TSA::BIM::BimModel& bim);
    void clear();
    /// Visibilité de la couche (les objets sont conservés).
    void setVisible(bool visible);
    bool visible() const { return m_visible; }
    std::size_t size() const { return m_objects.size(); }
    /// Objets effectivement affichés dans le contexte (0 si la couche est masquée).
    std::size_t displayedCount() const;
    /// Emprise des produits tracés (vide s'il n'y en a aucun).
    Bnd_Box bounds() const;
    /// Produit (id BimModel) d'une entité détectée ; 0 si elle n'appartient pas à cette couche.
    int productOf(const Handle(SelectMgr_EntityOwner)& owner) const;

private:
    Handle(AIS_InteractiveContext) m_context;
    std::map<int, Handle(AIS_InteractiveObject)> m_objects;   ///< id produit → objet
    bool m_visible = true;
};

} // namespace TSA::Viewer
