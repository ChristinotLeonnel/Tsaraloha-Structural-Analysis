#include "GridRenderer.h"

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <Quantity_Color.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>

namespace TSA::Grid
{

GridRenderer::GridRenderer()
    : m_gridVisible(true)
    , m_labelsVisible(true)
    , m_intersectionsVisible(true)
    , m_levelsVisible(true)
{
}

void GridRenderer::clearGrid(const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull())
        return;

    hideSnapMarker(context);
    m_labelRenderer.removeLabels(context);

    if (!m_axesShape.IsNull())
    {
        context->Remove(m_axesShape, false);
        m_axesShape.Nullify();
    }
    if (!m_circlesShape.IsNull())
    {
        context->Remove(m_circlesShape, false);
        m_circlesShape.Nullify();
    }
    if (!m_intersectionsShape.IsNull())
    {
        context->Remove(m_intersectionsShape, false);
        m_intersectionsShape.Nullify();
    }
    if (!m_originShape.IsNull())
    {
        context->Remove(m_originShape, false);
        m_originShape.Nullify();
    }
    if (!m_levelAxisShape.IsNull())
    {
        context->Remove(m_levelAxisShape, false);
        m_levelAxisShape.Nullify();
    }
    if (!m_levelPlanesShape.IsNull())
    {
        context->Remove(m_levelPlanesShape, false);
        m_levelPlanesShape.Nullify();
    }
    if (!m_verticalConnectionsShape.IsNull())
    {
        context->Remove(m_verticalConnectionsShape, false);
        m_verticalConnectionsShape.Nullify();
    }
    if (!m_activeLevelPlaneShape.IsNull())
    {
        context->Remove(m_activeLevelPlaneShape, false);
        m_activeLevelPlaneShape.Nullify();
    }
}

void GridRenderer::setGridVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_gridVisible = visible;
    if (context.IsNull())
        return;

    auto updateVis = [&](Handle(AIS_Shape)& shape) {
        if (!shape.IsNull())
        {
            if (m_gridVisible)
                context->Display(shape, false);
            else
                context->Erase(shape, false);
        }
    };

    updateVis(m_axesShape);
    updateVis(m_verticalConnectionsShape);
    updateVis(m_activeLevelPlaneShape);
    updateVis(m_circlesShape);
    updateVis(m_intersectionsShape);
    updateVis(m_originShape);
    updateVis(m_levelAxisShape);
    updateVis(m_levelPlanesShape);

    m_labelRenderer.setVisible(m_gridVisible && m_labelsVisible, context);
}

void GridRenderer::setLabelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_labelsVisible = visible;
    m_labelRenderer.setVisible(m_gridVisible && m_labelsVisible, context);
}

void GridRenderer::setIntersectionsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_intersectionsVisible = visible;
    if (!m_intersectionsShape.IsNull() && !context.IsNull())
    {
        if (m_gridVisible && m_intersectionsVisible)
            context->Display(m_intersectionsShape, false);
        else
            context->Erase(m_intersectionsShape, false);
    }
}

void GridRenderer::setLevelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_levelsVisible = visible;
    if (context.IsNull())
        return;

    if (!m_levelAxisShape.IsNull())
    {
        if (m_gridVisible && m_levelsVisible)
            context->Display(m_levelAxisShape, false);
        else
            context->Erase(m_levelAxisShape, false);
    }

    if (!m_levelPlanesShape.IsNull())
    {
        if (m_gridVisible && m_levelsVisible)
            context->Display(m_levelPlanesShape, false);
        else
            context->Erase(m_levelPlanesShape, false);
    }
}

void GridRenderer::renderGrid(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    clearGrid(context);

    if (context.IsNull() || !gridSystem.isVisible())
    {
        return;
    }

    if (gridSystem.type() == GridType::Cartesian)
    {
        renderCartesian(gridSystem, context);
    }
    else
    {
        renderCylindrical(gridSystem, context);
    }

    if (m_labelsVisible && gridSystem.showLabels())
    {
        m_labelRenderer.updateLabels(gridSystem, context);
    }
}

void GridRenderer::renderCartesian(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    const auto* cartesian = gridSystem.cartesian();
    if (!cartesian)
        return;

    BRep_Builder builder;

    // 1. Lignes d'axes (Compound d'arêtes)
    TopoDS_Compound axesCompound;
    builder.MakeCompound(axesCompound);

    for (const auto& line : cartesian->allLines())
    {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
        if (!edge.IsNull())
        {
            builder.Add(axesCompound, edge);
        }
    }

    m_axesShape = new AIS_Shape(axesCompound);
    Quantity_Color axesColor = m_isDarkMode
        ? Quantity_Color(0.48, 0.54, 0.62, Quantity_TOC_RGB)
        : Quantity_Color(0.55, 0.60, 0.68, Quantity_TOC_RGB);
    Handle(Prs3d_LineAspect) dashAspect = new Prs3d_LineAspect(
        axesColor,
        Aspect_TOL_DASH,
        1.0
    );
    m_axesShape->Attributes()->SetWireAspect(dashAspect);
    m_axesShape->Attributes()->SetLineAspect(dashAspect);
    m_axesShape->SetColor(axesColor);
    m_axesShape->SetWidth(1.0);
    context->Display(m_axesShape, false);

    // 1b. Lignes de connexion verticales à chaque intersection (X_i, Y_j) reliant tous les étages
    if (!cartesian->verticalConnectionLines().empty())
    {
        TopoDS_Compound connCompound;
        builder.MakeCompound(connCompound);
        for (const auto& line : cartesian->verticalConnectionLines())
        {
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
            if (!edge.IsNull())
            {
                builder.Add(connCompound, edge);
            }
        }
        m_verticalConnectionsShape = new AIS_Shape(connCompound);
        Quantity_Color vConnColor = m_isDarkMode
            ? Quantity_Color(0.42, 0.48, 0.56, Quantity_TOC_RGB)
            : Quantity_Color(0.58, 0.62, 0.70, Quantity_TOC_RGB);
        Handle(Prs3d_LineAspect) vDashAspect = new Prs3d_LineAspect(
            vConnColor,
            Aspect_TOL_DASH,
            1.2
        );
        m_verticalConnectionsShape->Attributes()->SetWireAspect(vDashAspect);
        m_verticalConnectionsShape->Attributes()->SetLineAspect(vDashAspect);
        m_verticalConnectionsShape->SetColor(vConnColor);
        m_verticalConnectionsShape->SetWidth(1.2);
        context->Display(m_verticalConnectionsShape, false);
    }

    // 1c. Mise en surbrillance du plan de l'étage actif
    updateActiveLevelHighlight(gridSystem, context);

    // 2. Intersections (petites sphères discrètes aux nœuds de grille)
    if (gridSystem.showIntersections())
    {
        TopoDS_Compound interCompound;
        builder.MakeCompound(interCompound);

        for (const auto& inter : cartesian->intersections())
        {
            BRepPrimAPI_MakeSphere sphereMaker(inter.point, 0.05);
            if (sphereMaker.IsDone())
            {
                builder.Add(interCompound, sphereMaker.Shape());
            }
        }

        m_intersectionsShape = new AIS_Shape(interCompound);
        m_intersectionsShape->SetColor(Quantity_Color(0.35, 0.70, 0.90, Quantity_TOC_RGB));
        m_intersectionsShape->SetDisplayMode(AIS_Shaded);
        context->Display(m_intersectionsShape, false);
    }

    // 3. Origine de la grille (trièdre local / croix tridimensionnelle)
    const gp_Pnt& orig = gridSystem.definition().origin();
    TopoDS_Compound origCompound;
    builder.MakeCompound(origCompound);

    double armLen = 1.0;
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X() + armLen, orig.Y(), orig.Z())).Edge());
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X(), orig.Y() + armLen, orig.Z())).Edge());
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X(), orig.Y(), orig.Z() + armLen)).Edge());

    m_originShape = new AIS_Shape(origCompound);
    m_originShape->SetColor(Quantity_NOC_YELLOW);
    m_originShape->SetWidth(3.0);
    context->Display(m_originShape, false);

    // 4. Colonne verticale Z reliant tous les étages (exigence centrale)
    if (!cartesian->verticalLevelLines().empty())
    {
        TopoDS_Compound vertCompound;
        builder.MakeCompound(vertCompound);

        for (const auto& line : cartesian->verticalLevelLines())
        {
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
            if (!edge.IsNull())
            {
                builder.Add(vertCompound, edge);
            }
        }

        m_levelAxisShape = new AIS_Shape(vertCompound);
        m_levelAxisShape->SetColor(Quantity_Color(0.95, 0.75, 0.15, Quantity_TOC_RGB)); // Or structural éclatant
        m_levelAxisShape->SetWidth(2.6);
        context->Display(m_levelAxisShape, false);
    }

    // 5. Cadres de contour des niveaux d'étages
    if (!cartesian->levelBoundaryPlanes().empty())
    {
        TopoDS_Compound planesCompound;
        builder.MakeCompound(planesCompound);

        for (const auto& line : cartesian->levelBoundaryPlanes())
        {
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
            if (!edge.IsNull())
            {
                builder.Add(planesCompound, edge);
            }
        }

        m_levelPlanesShape = new AIS_Shape(planesCompound);
        m_levelPlanesShape->SetColor(Quantity_Color(0.25, 0.50, 0.75, Quantity_TOC_RGB)); // Bleu acier moderne
        m_levelPlanesShape->SetWidth(1.6);
        context->Display(m_levelPlanesShape, false);
    }
}

void GridRenderer::setActiveLevelElevation(double z, const GridSystem* gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    m_activeLevelZ = z;
    if (gridSystem)
    {
        updateActiveLevelHighlight(*gridSystem, context);
    }
}

void GridRenderer::updateActiveLevelHighlight(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull())
        return;

    if (!m_activeLevelPlaneShape.IsNull())
    {
        context->Remove(m_activeLevelPlaneShape, false);
        m_activeLevelPlaneShape.Nullify();
    }

    if (!m_gridVisible || gridSystem.type() != GridType::Cartesian || !gridSystem.cartesian())
        return;

    const auto* cartesian = gridSystem.cartesian();
    BRep_Builder builder;
    TopoDS_Compound activeCompound;
    builder.MakeCompound(activeCompound);

    bool hasLines = false;
    for (const auto& line : cartesian->allLines())
    {
        if (std::abs(line.zLevel - m_activeLevelZ) < 1e-4)
        {
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
            if (!edge.IsNull())
            {
                builder.Add(activeCompound, edge);
                hasLines = true;
            }
        }
    }

    if (hasLines)
    {
        m_activeLevelPlaneShape = new AIS_Shape(activeCompound);
        Quantity_Color activeColor = m_isDarkMode
            ? Quantity_Color(0.25, 0.70, 0.95, Quantity_TOC_RGB)   // Cyan électrique éclatant en mode sombre
            : Quantity_Color(0.10, 0.40, 0.80, Quantity_TOC_RGB);  // Bleu royal soutenu en mode clair
        Handle(Prs3d_LineAspect) activeAspect = new Prs3d_LineAspect(
            activeColor,
            Aspect_TOL_SOLID,
            2.0
        );
        m_activeLevelPlaneShape->Attributes()->SetWireAspect(activeAspect);
        m_activeLevelPlaneShape->Attributes()->SetLineAspect(activeAspect);
        m_activeLevelPlaneShape->SetColor(activeColor);
        m_activeLevelPlaneShape->SetWidth(2.0);
        context->Display(m_activeLevelPlaneShape, false);
    }
}

void GridRenderer::renderCylindrical(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    const auto* cyl = gridSystem.cylindrical();
    if (!cyl)
        return;

    BRep_Builder builder;

    // 1. Cercles concentriques
    TopoDS_Compound circlesCompound;
    builder.MakeCompound(circlesCompound);

    for (const auto& circ : cyl->circles())
    {
        if (circ.radius <= 1e-4) continue;
        gp_Circ occtCirc(gp_Ax2(circ.center, gp_Dir(0, 0, 1)), circ.radius);
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(occtCirc);
        if (!edge.IsNull())
        {
            builder.Add(circlesCompound, edge);
        }
    }

    m_circlesShape = new AIS_Shape(circlesCompound);
    m_circlesShape->SetColor(Quantity_Color(0.35, 0.65, 0.75, Quantity_TOC_RGB));
    m_circlesShape->SetWidth(1.4);
    context->Display(m_circlesShape, false);

    // 2. Lignes radiales
    TopoDS_Compound radCompound;
    builder.MakeCompound(radCompound);

    for (const auto& rad : cyl->radialLines())
    {
        if (rad.start.Distance(rad.end) <= 1e-4) continue;
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(rad.start, rad.end);
        if (!edge.IsNull())
        {
            builder.Add(radCompound, edge);
        }
    }

    m_axesShape = new AIS_Shape(radCompound);
    m_axesShape->SetColor(Quantity_Color(0.45, 0.50, 0.60, Quantity_TOC_RGB));
    m_axesShape->SetWidth(1.2);
    context->Display(m_axesShape, false);

    // 3. Intersections (Cercles x Rayons)
    if (gridSystem.showIntersections())
    {
        TopoDS_Compound interCompound;
        builder.MakeCompound(interCompound);

        for (const auto& inter : cyl->intersections())
        {
            BRepPrimAPI_MakeSphere sphereMaker(inter.point, 0.05);
            if (sphereMaker.IsDone())
            {
                builder.Add(interCompound, sphereMaker.Shape());
            }
        }

        m_intersectionsShape = new AIS_Shape(interCompound);
        m_intersectionsShape->SetColor(Quantity_Color(0.20, 0.85, 0.85, Quantity_TOC_RGB));
        m_intersectionsShape->SetDisplayMode(AIS_Shaded);
        context->Display(m_intersectionsShape, false);
    }

    // 4. Centre de la grille cylindrique
    const gp_Pnt& orig = gridSystem.definition().origin();
    TopoDS_Compound centerCompound;
    builder.MakeCompound(centerCompound);

    BRepPrimAPI_MakeSphere sphereMaker(orig, 0.12);
    if (sphereMaker.IsDone())
    {
        builder.Add(centerCompound, sphereMaker.Shape());
    }

    m_originShape = new AIS_Shape(centerCompound);
    m_originShape->SetColor(Quantity_NOC_YELLOW);
    m_originShape->SetDisplayMode(AIS_Shaded);
    context->Display(m_originShape, false);
}

void GridRenderer::showSnapMarker(const GridSnapResult& snap, const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull() || !snap.snapped)
    {
        hideSnapMarker(context);
        return;
    }

    // Marqueur visuel élégant (Diamant / Boîte rotatée ou sphère éclatante)
    double sz = 0.10;
    gp_Pnt minP(snap.point.X() - sz, snap.point.Y() - sz, snap.point.Z() - sz);
    gp_Pnt maxP(snap.point.X() + sz, snap.point.Y() + sz, snap.point.Z() + sz);
    TopoDS_Shape boxShape = BRepPrimAPI_MakeBox(minP, maxP).Shape();

    if (m_snapMarkerShape.IsNull())
    {
        m_snapMarkerShape = new AIS_Shape(boxShape);
        m_snapMarkerShape->SetDisplayMode(AIS_Shaded);
        m_snapMarkerShape->SetColor(snap.type == GridSnapType::Intersection ? Quantity_NOC_SPRINGGREEN : Quantity_NOC_CYAN1);
        context->Display(m_snapMarkerShape, false);
    }
    else
    {
        m_snapMarkerShape->SetShape(boxShape);
        m_snapMarkerShape->SetColor(snap.type == GridSnapType::Intersection ? Quantity_NOC_SPRINGGREEN : Quantity_NOC_CYAN1);
        if (!context->IsDisplayed(m_snapMarkerShape))
        {
            context->Display(m_snapMarkerShape, false);
        }
        else
        {
            context->Redisplay(m_snapMarkerShape, false);
        }
    }
}

void GridRenderer::hideSnapMarker(const Handle(AIS_InteractiveContext)& context)
{
    if (!m_snapMarkerShape.IsNull() && !context.IsNull())
    {
        if (context->IsDisplayed(m_snapMarkerShape))
        {
            context->Erase(m_snapMarkerShape, false);
        }
    }
}

} // namespace TSA::Grid
