#pragma once

// Rendu des cotations 3D (OCCT AIS) à partir de leur mise en page (Annotation::DimensionGeometry).
//  - lignes : une forme filaire par cotation, calque « Topmost » (jamais masquée par les barres, pas de
//    scintillement avec la géométrie), sélectionnable ;
//  - flèches et symbole de niveau : cônes à taille constante à l'écran (persistance de zoom), non
//    sélectionnables ;
//  - texte : étiquette face à la caméra, taille constante en pixels, fond sombre (lisible sur la
//    géométrie) ; option « texte dans le plan de la cotation ».
// Les valeurs affichées viennent des coordonnées du modèle, jamais de la projection écran.

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>

#include <map>
#include <set>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Viewer
{

class SelectionManager;

class DimensionRenderer
{
public:
    DimensionRenderer(const Handle(AIS_InteractiveContext)& context, SelectionManager* selection);
    ~DimensionRenderer();

    void setSelectionManager(SelectionManager* selection) { m_selection = selection; }

    /// Reconstruit toutes les cotations (style, chargement, Annuler).
    void rebuildAll(const TSA::Model::Model& model);
    /// Reconstruit une cotation (ou la retire si elle n'existe plus).
    void update(const TSA::Model::Model& model, int dimensionId);
    void remove(int dimensionId);
    void clear();
    /// Surbrillance des cotations sélectionnées.
    void setHighlighted(const TSA::Model::Model& model, const std::set<int>& ids);

    int count() const { return static_cast<int>(m_visuals.size()); }
    bool contains(int dimensionId) const { return m_visuals.count(dimensionId) > 0; }

private:
    struct Visual
    {
        Handle(AIS_Shape) lines;
        std::vector<Handle(AIS_Shape)> arrows;
        std::vector<Handle(AIS_TextLabel)> labels;
    };
    void build(const TSA::Model::Model& model, int dimensionId, bool highlighted);

    Handle(AIS_InteractiveContext) m_context;
    SelectionManager* m_selection = nullptr;
    std::map<int, Visual> m_visuals;
    std::set<int> m_highlighted;
};

} // namespace TSA::Viewer
