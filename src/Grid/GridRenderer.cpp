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
    , m_isDarkMode(true)
{
}

void GridRenderer::clearGridObjects(PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull())
        return;

    auto removeShape = [&](Handle(AIS_Shape)& s) {
        if (!s.IsNull())
        {
            context->Remove(s, false);
            s.Nullify();
        }
    };

    removeShape(objs.axesShape);
    removeShape(objs.verticalConnectionsShape);
    removeShape(objs.activeLevelPlaneShape);
    removeShape(objs.circlesShape);
    removeShape(objs.intersectionsShape);
    removeShape(objs.originShape);
    removeShape(objs.levelAxisShape);
    removeShape(objs.levelPlanesShape);
}

void GridRenderer::removeGrid(const std::string& gridId, const Handle(AIS_InteractiveContext)& context)
{
    auto it = m_gridObjectsMap.find(gridId);
    if (it != m_gridObjectsMap.end())
    {
        clearGridObjects(it->second, context);
        m_gridObjectsMap.erase(it);
    }
    m_labelRenderer.removeLabels(gridId, context);
}

void GridRenderer::clearGrid(const Handle(AIS_InteractiveContext)& context)
{
    hideSnapMarker(context);
    for (auto& [id, objs] : m_gridObjectsMap)
    {
        clearGridObjects(objs, context);
    }
    m_gridObjectsMap.clear();
    m_labelRenderer.removeAllLabels(context);
}

void GridRenderer::setGridVisible(const std::string& gridId, bool visible, const Handle(AIS_InteractiveContext)& context)
{
    auto it = m_gridObjectsMap.find(gridId);
    if (it == m_gridObjectsMap.end() || context.IsNull())
        return;

    auto& objs = it->second;
    bool show = visible && m_gridVisible;

    auto updateVis = [&](Handle(AIS_Shape)& shape, bool alsoVisible) {
        if (!shape.IsNull())
        {
            if (show && alsoVisible)
                context->Display(shape, false);
            else
                context->Erase(shape, false);
        }
    };

    updateVis(objs.axesShape, true);
    updateVis(objs.verticalConnectionsShape, true);
    updateVis(objs.activeLevelPlaneShape, true);
    updateVis(objs.circlesShape, true);
    updateVis(objs.intersectionsShape, m_intersectionsVisible);
    updateVis(objs.originShape, true);
    updateVis(objs.levelAxisShape, m_levelsVisible);
    updateVis(objs.levelPlanesShape, m_levelsVisible);

    m_labelRenderer.setGridLabelsVisible(gridId, show && m_labelsVisible, context);
}

void GridRenderer::setGridVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_gridVisible = visible;
    if (context.IsNull())
        return;

    for (auto& [id, objs] : m_gridObjectsMap)
    {
        setGridVisible(id, visible, context);
    }
}

void GridRenderer::setLabelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_labelsVisible = visible;
    m_labelRenderer.setVisible(m_gridVisible && m_labelsVisible, context);
}

void GridRenderer::setIntersectionsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_intersectionsVisible = visible;
    if (context.IsNull())
        return;

    for (auto& [id, objs] : m_gridObjectsMap)
    {
        if (!objs.intersectionsShape.IsNull())
        {
            if (m_gridVisible && m_intersectionsVisible)
                context->Display(objs.intersectionsShape, false);
            else
                context->Erase(objs.intersectionsShape, false);
        }
    }
}

void GridRenderer::setLevelsVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    m_levelsVisible = visible;
    if (context.IsNull())
        return;

    for (auto& [id, objs] : m_gridObjectsMap)
    {
        if (!objs.levelAxisShape.IsNull())
        {
            if (m_gridVisible && m_levelsVisible)
                context->Display(objs.levelAxisShape, false);
            else
                context->Erase(objs.levelAxisShape, false);
        }
        if (!objs.levelPlanesShape.IsNull())
        {
            if (m_gridVisible && m_levelsVisible)
                context->Display(objs.levelPlanesShape, false);
            else
                context->Erase(objs.levelPlanesShape, false);
        }
    }
}

void GridRenderer::renderGrid(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    std::string id = gridSystem.id();
    removeGrid(id, context);

    if (context.IsNull() || !gridSystem.isVisible() || !m_gridVisible)
    {
        return;
    }

    PerGridRenderObjects objs;
    objs.gridId = id;

    if (gridSystem.type() == GridType::Cartesian)
    {
        renderCartesian(gridSystem, objs, context);
    }
    else if (gridSystem.type() == GridType::Cylindrical)
    {
        renderCylindrical(gridSystem, objs, context);
    }
    else if (gridSystem.type() == GridType::Arbitrary)
    {
        renderArbitrary(gridSystem, objs, context);
    }

    if (m_labelsVisible && gridSystem.showLabels())
    {
        m_labelRenderer.updateLabels(gridSystem, context);
    }

    m_gridObjectsMap[id] = std::move(objs);
}

void GridRenderer::renderCartesian(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context)
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

    objs.axesShape = new AIS_Shape(axesCompound);
    Quantity_Color axesColor = m_isDarkMode
        ? Quantity_Color(0.48, 0.54, 0.62, Quantity_TOC_RGB)
        : Quantity_Color(0.55, 0.60, 0.68, Quantity_TOC_RGB);
    Handle(Prs3d_LineAspect) dashAspect = new Prs3d_LineAspect(
        axesColor,
        Aspect_TOL_DASH,
        1.0
    );
    objs.axesShape->Attributes()->SetWireAspect(dashAspect);
    objs.axesShape->Attributes()->SetLineAspect(dashAspect);
    objs.axesShape->SetColor(axesColor);
    objs.axesShape->SetWidth(1.0);
    context->Display(objs.axesShape, false);

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
        objs.verticalConnectionsShape = new AIS_Shape(connCompound);
        Quantity_Color vConnColor = m_isDarkMode
            ? Quantity_Color(0.42, 0.48, 0.56, Quantity_TOC_RGB)
            : Quantity_Color(0.58, 0.62, 0.70, Quantity_TOC_RGB);
        Handle(Prs3d_LineAspect) vDashAspect = new Prs3d_LineAspect(
            vConnColor,
            Aspect_TOL_DASH,
            1.2
        );
        objs.verticalConnectionsShape->Attributes()->SetWireAspect(vDashAspect);
        objs.verticalConnectionsShape->Attributes()->SetLineAspect(vDashAspect);
        objs.verticalConnectionsShape->SetColor(vConnColor);
        objs.verticalConnectionsShape->SetWidth(1.2);
        context->Display(objs.verticalConnectionsShape, false);
    }

    // 1c. Mise en surbrillance du plan de l'étage actif (si cette grille est active)
    if (gridSystem.isActive())
    {
        updateActiveLevelHighlight(gridSystem, objs, context);
    }

    // 2. Intersections (petites sphères discrètes aux nœuds de grille)
    if (gridSystem.showIntersections() && m_intersectionsVisible)
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

        objs.intersectionsShape = new AIS_Shape(interCompound);
        objs.intersectionsShape->SetColor(Quantity_Color(0.35, 0.70, 0.90, Quantity_TOC_RGB));
        objs.intersectionsShape->SetDisplayMode(AIS_Shaded);
        context->Display(objs.intersectionsShape, false);
    }

    // 3. Origine de la grille (trièdre local / croix tridimensionnelle)
    const gp_Pnt& orig = gridSystem.definition().origin();
    TopoDS_Compound origCompound;
    builder.MakeCompound(origCompound);

    double armLen = 1.0;
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X() + armLen, orig.Y(), orig.Z())).Edge());
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X(), orig.Y() + armLen, orig.Z())).Edge());
    builder.Add(origCompound, BRepBuilderAPI_MakeEdge(orig, gp_Pnt(orig.X(), orig.Y(), orig.Z() + armLen)).Edge());

    objs.originShape = new AIS_Shape(origCompound);
    objs.originShape->SetColor(gridSystem.isActive() ? Quantity_NOC_YELLOW : Quantity_NOC_GRAY60);
    objs.originShape->SetWidth(3.0);
    context->Display(objs.originShape, false);

    // 4. Colonne verticale Z reliant tous les étages
    if (gridSystem.showLevels() && m_levelsVisible && !cartesian->verticalLevelLines().empty())
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

        objs.levelAxisShape = new AIS_Shape(vertCompound);
        objs.levelAxisShape->SetColor(Quantity_Color(0.95, 0.75, 0.15, Quantity_TOC_RGB));
        objs.levelAxisShape->SetWidth(2.6);
        context->Display(objs.levelAxisShape, false);
    }

    // 5. Cadres de contour des niveaux d'étages
    if (gridSystem.showLevels() && m_levelsVisible && !cartesian->levelBoundaryPlanes().empty())
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

        objs.levelPlanesShape = new AIS_Shape(planesCompound);
        objs.levelPlanesShape->SetColor(Quantity_Color(0.25, 0.50, 0.75, Quantity_TOC_RGB));
        objs.levelPlanesShape->SetWidth(1.6);
        context->Display(objs.levelPlanesShape, false);
    }
}

void GridRenderer::setActiveLevelElevation(double z, const GridSystem* gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    m_activeLevelZ = z;
    if (gridSystem)
    {
        auto it = m_gridObjectsMap.find(gridSystem->id());
        if (it != m_gridObjectsMap.end())
        {
            updateActiveLevelHighlight(*gridSystem, it->second, context);
        }
    }
}

void GridRenderer::updateActiveLevelHighlight(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull())
        return;

    if (!objs.activeLevelPlaneShape.IsNull())
    {
        context->Remove(objs.activeLevelPlaneShape, false);
        objs.activeLevelPlaneShape.Nullify();
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
        objs.activeLevelPlaneShape = new AIS_Shape(activeCompound);
        Quantity_Color activeColor = m_isDarkMode
            ? Quantity_Color(0.25, 0.70, 0.95, Quantity_TOC_RGB)
            : Quantity_Color(0.10, 0.40, 0.80, Quantity_TOC_RGB);
        Handle(Prs3d_LineAspect) activeAspect = new Prs3d_LineAspect(
            activeColor,
            Aspect_TOL_SOLID,
            2.0
        );
        objs.activeLevelPlaneShape->Attributes()->SetWireAspect(activeAspect);
        objs.activeLevelPlaneShape->Attributes()->SetLineAspect(activeAspect);
        objs.activeLevelPlaneShape->SetColor(activeColor);
        objs.activeLevelPlaneShape->SetWidth(2.0);
        context->Display(objs.activeLevelPlaneShape, false);
    }
}

void GridRenderer::renderCylindrical(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context)
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

    objs.circlesShape = new AIS_Shape(circlesCompound);
    objs.circlesShape->SetColor(Quantity_Color(0.35, 0.65, 0.75, Quantity_TOC_RGB));
    objs.circlesShape->SetWidth(1.4);
    context->Display(objs.circlesShape, false);

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

    objs.axesShape = new AIS_Shape(radCompound);
    objs.axesShape->SetColor(Quantity_Color(0.45, 0.50, 0.60, Quantity_TOC_RGB));
    objs.axesShape->SetWidth(1.2);
    context->Display(objs.axesShape, false);

    // 3. Intersections (Cercles x Rayons)
    if (gridSystem.showIntersections() && m_intersectionsVisible)
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

        objs.intersectionsShape = new AIS_Shape(interCompound);
        objs.intersectionsShape->SetColor(Quantity_Color(0.20, 0.85, 0.85, Quantity_TOC_RGB));
        objs.intersectionsShape->SetDisplayMode(AIS_Shaded);
        context->Display(objs.intersectionsShape, false);
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

    objs.originShape = new AIS_Shape(centerCompound);
    objs.originShape->SetColor(Quantity_NOC_YELLOW);
    objs.originShape->SetDisplayMode(AIS_Shaded);
    context->Display(objs.originShape, false);
}

void GridRenderer::renderArbitrary(const GridSystem& gridSystem, PerGridRenderObjects& objs, const Handle(AIS_InteractiveContext)& context)
{
    const auto* arbitrary = gridSystem.arbitrary();
    if (!arbitrary)
        return;

    BRep_Builder builder;
    TopoDS_Compound axesCompound;
    builder.MakeCompound(axesCompound);

    for (const auto& line : arbitrary->renderLines())
    {
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line.start, line.end);
        if (!edge.IsNull())
        {
            builder.Add(axesCompound, edge);
        }
    }

    objs.axesShape = new AIS_Shape(axesCompound);

    const auto& ds = gridSystem.definition().displaySettings();
    Aspect_TypeOfLine occtLineStyle = Aspect_TOL_DASH;
    if (ds.lineStyle == "solid") occtLineStyle = Aspect_TOL_SOLID;
    else if (ds.lineStyle == "dot") occtLineStyle = Aspect_TOL_DOT;
    else if (ds.lineStyle == "dashdot") occtLineStyle = Aspect_TOL_DOTDASH;

    Quantity_Color axesColor;
    if (!ds.lineColor.empty() && ds.lineColor.front() == '#' && ds.lineColor.size() == 7)
    {
        try {
            int r = std::stoi(ds.lineColor.substr(1, 2), nullptr, 16);
            int g = std::stoi(ds.lineColor.substr(3, 2), nullptr, 16);
            int b = std::stoi(ds.lineColor.substr(5, 2), nullptr, 16);
            axesColor = Quantity_Color(r / 255.0, g / 255.0, b / 255.0, Quantity_TOC_RGB);
        } catch (...) {
            axesColor = Quantity_Color(0.48, 0.54, 0.62, Quantity_TOC_RGB);
        }
    }
    else
    {
        axesColor = m_isDarkMode
            ? Quantity_Color(0.48, 0.54, 0.62, Quantity_TOC_RGB)
            : Quantity_Color(0.55, 0.60, 0.68, Quantity_TOC_RGB);
    }

    double width = ds.lineWidth > 0.0 ? ds.lineWidth : 1.2;

    Handle(Prs3d_LineAspect) lineAspect = new Prs3d_LineAspect(
        axesColor,
        occtLineStyle,
        width
    );
    objs.axesShape->Attributes()->SetWireAspect(lineAspect);
    objs.axesShape->Attributes()->SetLineAspect(lineAspect);
    objs.axesShape->SetColor(axesColor);
    objs.axesShape->SetWidth(width);
    context->Display(objs.axesShape, false);

    // Intersections de lignes arbitraires
    if (gridSystem.showIntersections() && m_intersectionsVisible)
    {
        TopoDS_Compound interCompound;
        builder.MakeCompound(interCompound);

        for (const auto& inter : arbitrary->intersections())
        {
            BRepPrimAPI_MakeSphere sphereMaker(inter, 0.05);
            if (sphereMaker.IsDone())
            {
                builder.Add(interCompound, sphereMaker.Shape());
            }
        }

        objs.intersectionsShape = new AIS_Shape(interCompound);
        objs.intersectionsShape->SetColor(Quantity_Color(0.95, 0.40, 0.40, Quantity_TOC_RGB));
        objs.intersectionsShape->SetDisplayMode(AIS_Shaded);
        context->Display(objs.intersectionsShape, false);
    }
}

void GridRenderer::showSnapMarker(const GridSnapResult& snap, const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull() || !snap.snapped)
    {
        hideSnapMarker(context);
        return;
    }

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
