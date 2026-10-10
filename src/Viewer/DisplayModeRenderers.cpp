#include "DisplayModeRenderers.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRep_Builder.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_PointAspect.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

namespace TSA::Viewer
{

namespace
{
TopoDS_Compound segmentsCompound(const std::vector<std::pair<gp_Pnt, gp_Pnt>>& segments)
{
    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);
    for (const auto& [a, b] : segments)
    {
        if (a.Distance(b) < 1e-9) continue;
        BRepBuilderAPI_MakeEdge e(a, b);
        if (e.IsDone()) builder.Add(comp, e.Edge());
    }
    return comp;
}

/// Objet de tracé : arêtes ou sommets, couleur, épaisseur, calque supérieur, non sélectionnable.
Handle(AIS_Shape) overlayObject(const TopoDS_Shape& shape, const Quantity_Color& color, double width)
{
    Handle(AIS_Shape) ais = new AIS_Shape(shape);
    ais->SetDisplayMode(AIS_WireFrame);
    ais->SetColor(color);
    ais->SetWidth(width);
    ais->SetZLayer(Graphic3d_ZLayerId_Topmost);
    ais->SetAutoHilight(false);
    return ais;
}

void show(const Handle(AIS_InteractiveContext)& ctx, const Handle(AIS_Shape)& ais, std::vector<Handle(AIS_Shape)>& into)
{
    ctx->Display(ais, AIS_WireFrame, -1, false); // mode de sélection -1 : jamais sélectionnable
    into.push_back(ais);
}
} // namespace

// ─── Axes analytiques (Superposition) ─────────────────────────────────────────────────────────

void AnalyticalModelRenderer::rebuild(
    const std::map<TSA::Analysis::StructuralElementKind, std::vector<std::pair<gp_Pnt, gp_Pnt>>>& axes)
{
    clear();
    if (m_context.IsNull()) return;
    for (const auto& [kind, segments] : axes)
    {
        if (segments.empty()) continue;
        show(m_context, overlayObject(segmentsCompound(segments), analyticalColor(kind), 2.5), m_objects);
    }
}

void AnalyticalModelRenderer::clear()
{
    if (!m_context.IsNull())
        for (const auto& o : m_objects) m_context->Remove(o, false);
    m_objects.clear();
}

// ─── Maillage du solveur (Éléments finis) ─────────────────────────────────────────────────────

void FiniteElementMeshRenderer::rebuild(const SolverMeshGeometry& g)
{
    clear();
    if (m_context.IsNull()) return;
    if (!g.segments.empty())
        show(m_context, overlayObject(segmentsCompound(g.segments), Quantity_Color(0.72, 1.0, 0.0, Quantity_TOC_sRGB), 2.0),
             m_objects); // vert-jaune : distinct des charges (cyan), des axes et de la déformée

    // Nœuds du maillage : marqueurs à taille constante à l'écran (nœuds du modèle : cercle blanc ;
    // nœuds propres au moteur — subdivisions, auxiliaires — : croix orange).
    auto markers = [&](const std::vector<gp_Pnt>& pts, Aspect_TypeOfMarker type, const Quantity_Color& color, double scale) {
        if (pts.empty()) return;
        BRep_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);
        for (const auto& p : pts) builder.Add(comp, BRepBuilderAPI_MakeVertex(p).Vertex());
        Handle(AIS_Shape) ais = overlayObject(comp, color, 1.0);
        ais->Attributes()->SetPointAspect(new Prs3d_PointAspect(type, color, scale));
        show(m_context, ais, m_objects);
    };
    markers(g.modelNodes, Aspect_TOM_O_POINT, Quantity_Color(0.95, 0.95, 0.95, Quantity_TOC_sRGB), 2.0);
    markers(g.solverNodes, Aspect_TOM_X, Quantity_Color(1.0, 0.55, 0.0, Quantity_TOC_sRGB), 2.5);
}

void FiniteElementMeshRenderer::clear()
{
    if (!m_context.IsNull())
        for (const auto& o : m_objects) m_context->Remove(o, false);
    m_objects.clear();
}

} // namespace TSA::Viewer
