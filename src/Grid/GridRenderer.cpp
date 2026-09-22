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
    updateVis(m_circlesShape);
    updateVis(m_intersectionsShape);
    updateVis(m_originShape);

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
    m_axesShape->SetColor(Quantity_Color(0.42, 0.48, 0.58, Quantity_TOC_RGB));
    m_axesShape->SetWidth(1.2);
    context->Display(m_axesShape, false);

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
        m_snapMarkerShape->SetColor(Quantity_NOC_SPRINGGREEN);
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
