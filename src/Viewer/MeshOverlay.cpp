#include "MeshOverlay.h"

#include <Bnd_Box.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_AspectMarker3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Prs3d_Presentation.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <SelectMgr_Selection.hxx>

#include <algorithm>
#include <set>

namespace TSA::Viewer
{

namespace
{
/// Segments colorés par sommet et points marqueurs, sans sélection (superposition visuelle).
class SegmentsObject : public AIS_InteractiveObject
{
public:
    std::vector<gp_Pnt> points;
    std::vector<Quantity_Color> colors;
    std::vector<gp_Pnt> markers;
    Quantity_Color markerColor { 0.95, 0.95, 0.95, Quantity_TOC_sRGB };
    double width = 1.5;

    void Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs, const int mode) override
    {
        if (mode != 0) return;
        if (!points.empty())
        {
            Handle(Graphic3d_ArrayOfSegments) arr = new Graphic3d_ArrayOfSegments(int(points.size()), 0, Graphic3d_ArrayFlags_VertexColor);
            for (std::size_t i = 0; i < points.size(); ++i) arr->AddVertex(points[i], colors[i]);
            Handle(Graphic3d_Group) g = prs->NewGroup();
            g->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(Quantity_NOC_WHITE, Aspect_TOL_SOLID, width));
            g->AddPrimitiveArray(arr);
        }
        if (!markers.empty())
        {
            Handle(Graphic3d_ArrayOfPoints) pts = new Graphic3d_ArrayOfPoints(int(markers.size()));
            for (const auto& p : markers) pts->AddVertex(p);
            Handle(Graphic3d_Group) g = prs->NewGroup();
            g->SetGroupPrimitivesAspect(new Graphic3d_AspectMarker3d(Aspect_TOM_O_POINT, markerColor, 2.0));
            g->AddPrimitiveArray(pts);
        }
    }
    void ComputeSelection(const Handle(SelectMgr_Selection)&, const int) override {}
};

/// Arêtes (paires de nœuds de coin) d'un élément, ordre Gmsh.
void cornerEdges(const TSA::Mesh::FeElement& e, std::vector<std::pair<int, int>>& out)
{
    using TSA::Mesh::FeType;
    const auto& n = e.nodes;
    switch (TSA::Mesh::feTypeCornerCount(e.type))
    {
    case 2: out.push_back({ n[0], n[1] }); break;
    case 3:
        for (int k = 0; k < 3; ++k) out.push_back({ n[k], n[(k + 1) % 3] });
        break;
    case 4:
        if (TSA::Mesh::feTypeDimension(e.type) == 2)
            for (int k = 0; k < 4; ++k) out.push_back({ n[k], n[(k + 1) % 4] });
        else   // tétraèdre
            for (auto [a, b] : { std::pair { 0, 1 }, { 1, 2 }, { 2, 0 }, { 0, 3 }, { 1, 3 }, { 2, 3 } }) out.push_back({ n[a], n[b] });
        break;
    default:   // hexaèdre
        for (auto [a, b] : { std::pair { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } })
            out.push_back({ n[a], n[b] });
        break;
    }
}
} // namespace

MeshOverlay::MeshOverlay(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view)
    : m_context(context)
    , m_view(view)
{
}

MeshOverlay::~MeshOverlay() { clear(); }

void MeshOverlay::clear()
{
    clearHighlight();
    if (!m_context.IsNull())
        for (const auto& o : m_objects) m_context->Remove(o, false);
    m_objects.clear();
    if (!m_view.IsNull()) m_view->Redraw();
}

void MeshOverlay::clearHighlight()
{
    if (!m_context.IsNull() && !m_highlight.IsNull()) m_context->Remove(m_highlight, false);
    m_highlight.Nullify();
}

void MeshOverlay::show(const TSA::Mesh::FeMesh& mesh, Mode mode, const std::vector<double>* quality, double threshold)
{
    clear();
    if (m_context.IsNull() || mesh.empty()) return;
    const Quantity_Color base = mode == Mode::Preview ? Quantity_Color(1.0, 0.62, 0.15, Quantity_TOC_sRGB) : Quantity_Color(0.30, 0.90, 0.55, Quantity_TOC_sRGB);
    const Quantity_Color bad(1.0, 0.15, 0.15, Quantity_TOC_sRGB);
    Handle(SegmentsObject) obj = new SegmentsObject();
    std::set<std::pair<int, int>> seen;
    std::vector<std::pair<int, int>> edges;
    for (const auto& e : mesh.elements)
    {
        if (e.boundary) continue;
        edges.clear();
        cornerEdges(e, edges);
        const bool poor = quality && std::size_t(e.id - 1) < quality->size() && (*quality)[std::size_t(e.id - 1)] < threshold;
        for (auto [a, b] : edges)
        {
            if (!seen.insert({ std::min(a, b), std::max(a, b) }).second && !poor) continue;
            const auto& na = mesh.node(a);
            const auto& nb = mesh.node(b);
            obj->points.emplace_back(na.x, na.y, na.z);
            obj->points.emplace_back(nb.x, nb.y, nb.z);
            obj->colors.push_back(poor ? bad : base);
            obj->colors.push_back(poor ? bad : base);
        }
    }
    for (const auto& n : mesh.nodes)
        if (n.tsaNode) obj->markers.emplace_back(n.x, n.y, n.z);
    obj->width = mode == Mode::Preview ? 1.2 : 1.5;
    m_context->Display(obj, 0, -1, false);
    m_objects.push_back(obj);
    if (!m_view.IsNull()) m_view->Redraw();
}

void MeshOverlay::highlight(const TSA::Mesh::FeMesh& mesh, int elementId, int nodeId, bool zoom)
{
    clearHighlight();
    if (m_context.IsNull()) return;
    Handle(SegmentsObject) obj = new SegmentsObject();
    obj->width = 4.0;
    obj->markerColor = Quantity_Color(1.0, 0.9, 0.1, Quantity_TOC_sRGB);
    Bnd_Box box;
    if (elementId >= 1 && std::size_t(elementId) <= mesh.elements.size())
    {
        std::vector<std::pair<int, int>> edges;
        cornerEdges(mesh.elements[std::size_t(elementId - 1)], edges);
        for (auto [a, b] : edges)
            for (int id : { a, b })
            {
                const auto& n = mesh.node(id);
                obj->points.emplace_back(n.x, n.y, n.z);
                obj->colors.emplace_back(1.0, 0.9, 0.1, Quantity_TOC_sRGB);
                box.Add(gp_Pnt(n.x, n.y, n.z));
            }
    }
    if (nodeId >= 1 && std::size_t(nodeId) <= mesh.nodes.size())
    {
        const auto& n = mesh.node(nodeId);
        obj->markers.emplace_back(n.x, n.y, n.z);
        box.Add(gp_Pnt(n.x, n.y, n.z));
    }
    if (box.IsVoid()) return;
    m_context->Display(obj, 0, -1, false);
    m_highlight = obj;
    if (zoom && !m_view.IsNull())
    {
        box.Enlarge(std::max(0.5, std::sqrt(box.SquareExtent()) * 2.0));
        m_view->FitAll(box, 0.1);
    }
    if (!m_view.IsNull()) m_view->Redraw();
}

} // namespace TSA::Viewer
