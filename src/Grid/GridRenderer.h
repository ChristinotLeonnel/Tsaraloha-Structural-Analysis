#pragma once

#include "GridSystem.h"
#include "GridLabelRenderer.h"
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Point.hxx>
#include <vector>

namespace TSA::Grid
{

struct PerGridRenderObjects
{
    std::string gridId;
    Handle(AIS_Shape) axesShape;
    Handle(AIS_Shape) verticalConnectionsShape;
    Handle(AIS_Shape) activeLevelPlaneShape;
    Handle(AIS_Shape) circlesShape;
    Handle(AIS_Shape) intersectionsShape;
    Handle(AIS_Shape) originShape;
    Handle(AIS_Shape) levelAxisShape;
    Handle(AIS_Shape) levelPlanesShape;
};

class GridRenderer
{
public:
    GridRenderer();
    ~GridRenderer() = default;

    // Rendu d'un système de grille spécifique
    void renderGrid(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);
    void removeGrid(const std::string& gridId, const Handle(AIS_InteractiveContext)& context);
    void clearGrid(const Handle(AIS_InteractiveContext)& context);

    // Contrôles de visibilité globale et partielle
    void setGridVisible(bool visible, const Handle(AIS_InteractiveContext)& context);
    void setGridVisible(const std::string& gridId, bool visible, const Handle(AIS_InteractiveContext)& context);
    void setLabelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context);
    void setIntersectionsVisible(bool visible, const Handle(AIS_InteractiveContext)& context);
    void setLevelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context);

    bool isGridVisible() const { return m_gridVisible; }
    bool areLabelsVisible() const { return m_labelsVisible; }
    bool areIntersectionsVisible() const { return m_intersectionsVisible; }
    bool areLevelsVisible() const { return m_levelsVisible; }

    // Marqueur visuel interactif de Snapping (aimantation temps réel)
    void showSnapMarker(const GridSnapResult& snap, const Handle(AIS_InteractiveContext)& context);
    void hideSnapMarker(const Handle(AIS_InteractiveContext)& context);

    // Niveau actif pour le dessin en hauteur
    void setActiveLevelElevation(double z, const GridSystem* gridSystem, const Handle(AIS_InteractiveContext)& context);
    double activeLevelElevation() const { return m_activeLevelZ; }

    // Mode sombre / clair
    void setDarkMode(bool dark) { m_isDarkMode = dark; m_labelRenderer.setDarkMode(dark); }
    bool isDarkMode() const { return m_isDarkMode; }

private:
    void renderCartesian(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context);
    void renderCylindrical(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context);
    void updateActiveLevelHighlight(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context);
    void clearGridObjects(PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context);

private:
    bool m_gridVisible = true;
    bool m_labelsVisible = true;
    bool m_intersectionsVisible = true;
    bool m_levelsVisible = true;
    bool m_isDarkMode = true;
    double m_activeLevelZ = 0.0;

    // Représentations 3D AIS indexées par l'ID unique de la grille
    std::unordered_map<std::string, PerGridRenderObjects> m_gridObjectsMap;

    // Rendu des libellés et bulles
    GridLabelRenderer m_labelRenderer;

    // Marqueur dynamique d'aimantation (Snap)
    Handle(AIS_Shape) m_snapMarkerShape;
};

} // namespace TSA::Grid
