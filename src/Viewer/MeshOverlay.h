#pragma once

// Affichage d'un maillage TSA (docs/MESH.md) dans la vue 3D : arêtes des éléments en tableaux de segments
// (rapide pour de grands maillages), couleur par qualité (éléments sous le seuil en rouge), aperçu
// (orange) distinct du maillage généré (vert), nœuds du modèle repérés, mise en évidence et cadrage d'un
// élément (liste de qualité). Purement visuel : ne modifie ni le modèle ni le maillage.

#include "../Mesh/MeshTypes.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <V3d_View.hxx>

#include <vector>

namespace TSA::Viewer
{

class MeshOverlay
{
public:
    enum class Mode
    {
        Generated,
        Preview
    };

    MeshOverlay(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view);
    ~MeshOverlay();

    /// @param quality qualité par élément (index = id − 1, 1 = parfait) ; éléments < threshold en rouge
    void show(const TSA::Mesh::FeMesh& mesh, Mode mode, const std::vector<double>* quality = nullptr, double threshold = 0.0);
    /// Met en évidence un élément (ou un nœud si elementId = 0) et cadre la vue autour.
    void highlight(const TSA::Mesh::FeMesh& mesh, int elementId, int nodeId = 0, bool zoom = true);
    void clearHighlight();
    void clear();
    bool visible() const { return !m_objects.empty(); }

private:
    Handle(AIS_InteractiveContext) m_context;
    Handle(V3d_View) m_view;
    std::vector<Handle(AIS_InteractiveObject)> m_objects;
    Handle(AIS_InteractiveObject) m_highlight;
};

} // namespace TSA::Viewer
