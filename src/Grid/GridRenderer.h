#pragma once

#include "GridSystem.h"
#include "GridLabelRenderer.h"
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Point.hxx>
#include <vector>

namespace TSA::Grid
{

class GridRenderer
{
public:
    GridRenderer();
    ~GridRenderer() = default;

    // Rendu complet d'un système de grille
    void renderGrid(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);
    void clearGrid(const Handle(AIS_InteractiveContext)& context);

    // Contrôles de visibilité globale et partielle
    void setGridVisible(bool visible, const Handle(AIS_InteractiveContext)& context);
    void setLabelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context);
    void setIntersectionsVisible(bool visible, const Handle(AIS_InteractiveContext)& context);

    bool isGridVisible() const { return m_gridVisible; }
    bool areLabelsVisible() const { return m_labelsVisible; }
    bool areIntersectionsVisible() const { return m_intersectionsVisible; }

    // Marqueur visuel interactif de Snapping (aimantation temps réel)
    void showSnapMarker(const GridSnapResult& snap, const Handle(AIS_InteractiveContext)& context);
    void hideSnapMarker(const Handle(AIS_InteractiveContext)& context);

private:
    void renderCartesian(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);
    void renderCylindrical(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);

private:
    bool m_gridVisible = true;
    bool m_labelsVisible = true;
    bool m_intersectionsVisible = true;

    // Formes d'axes et de lignes
    Handle(AIS_Shape) m_axesShape;
    Handle(AIS_Shape) m_circlesShape;
    Handle(AIS_Shape) m_intersectionsShape;
    Handle(AIS_Shape) m_originShape;

    // Rendu des libellés et bulles
    GridLabelRenderer m_labelRenderer;

    // Marqueur dynamique d'aimantation (Snap)
    Handle(AIS_Shape) m_snapMarkerShape;
};

} // namespace TSA::Grid
