#include "DimensionRenderer.h"

#include "MaterialVisual.h"
#include "SelectionManager.h"
#include "../Annotation/DimensionGeometry.h"
#include "../Model/Model.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRep_Builder.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Quaternion.hxx>

#include <map>

namespace TSA::Viewer
{

namespace
{
Quantity_Color colorOf(const std::string& hex, const Quantity_Color& fallback)
{
    Quantity_Color c;
    return MaterialVisual::parseHexColor(hex, c) ? c : fallback;
}

/// Cône pointe à l'origine, orienté vers +Z (unités locales = pixels sous persistance de zoom). Mis en
/// cache par taille : toutes les flèches partagent la même géométrie (et sa triangulation).
const TopoDS_Shape& coneShape(double lengthPx)
{
    static std::map<int, TopoDS_Shape> cache;
    const int key = static_cast<int>(lengthPx * 10.0);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    const double radius = std::max(1.5, lengthPx * 0.3);
    TopoDS_Shape cone = BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0, 0, -lengthPx), gp_Dir(0, 0, 1)), radius, 0.0, lengthPx).Shape();
    return cache.emplace(key, cone).first->second;
}

Handle(AIS_Shape) makeArrow(const gp_Pnt& tip, const gp_Dir& dir, double lengthPx, const Quantity_Color& color)
{
    gp_Trsf rot;
    rot.SetRotation(gp_Quaternion(gp_Vec(0, 0, 1), gp_Vec(dir)));
    Handle(AIS_Shape) ais = new AIS_Shape(coneShape(lengthPx).Moved(TopLoc_Location(rot)));
    ais->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, tip));
    ais->SetColor(color);
    ais->SetDisplayMode(AIS_Shaded);
    ais->SetZLayer(Graphic3d_ZLayerId_Topmost);
    return ais;
}
} // namespace

DimensionRenderer::DimensionRenderer(const Handle(AIS_InteractiveContext)& context, SelectionManager* selection)
    : m_context(context)
    , m_selection(selection)
{
}

DimensionRenderer::~DimensionRenderer() = default;

void DimensionRenderer::clear()
{
    std::vector<int> ids;
    for (const auto& [id, v] : m_visuals) ids.push_back(id);
    for (int id : ids) remove(id);
}

void DimensionRenderer::remove(int dimensionId)
{
    auto it = m_visuals.find(dimensionId);
    if (it == m_visuals.end()) return;
    if (!m_context.IsNull())
    {
        if (!it->second.lines.IsNull()) m_context->Remove(it->second.lines, false);
        for (auto& a : it->second.arrows) m_context->Remove(a, false);
        for (auto& l : it->second.labels) m_context->Remove(l, false);
    }
    if (m_selection) m_selection->unregisterDimension(dimensionId);
    m_visuals.erase(it);
}

void DimensionRenderer::rebuildAll(const TSA::Model::Model& model)
{
    clear();
    for (const auto& [id, d] : model.dimensions().items) build(model, id, m_highlighted.count(id) > 0);
    if (!m_context.IsNull()) m_context->UpdateCurrentViewer();
}

void DimensionRenderer::update(const TSA::Model::Model& model, int dimensionId)
{
    remove(dimensionId);
    if (model.dimensions().items.count(dimensionId)) build(model, dimensionId, m_highlighted.count(dimensionId) > 0);
}

void DimensionRenderer::setHighlighted(const TSA::Model::Model& model, const std::set<int>& ids)
{
    if (ids == m_highlighted) return;
    std::set<int> changed;
    for (int id : ids)
        if (!m_highlighted.count(id)) changed.insert(id);
    for (int id : m_highlighted)
        if (!ids.count(id)) changed.insert(id);
    m_highlighted = ids;
    for (int id : changed) update(model, id);
    if (!m_context.IsNull()) m_context->UpdateCurrentViewer();
}

void DimensionRenderer::build(const TSA::Model::Model& model, int dimensionId, bool highlighted)
{
    if (m_context.IsNull()) return;
    const auto& set = model.dimensions();
    if (!set.style.visible) return;
    auto dit = set.items.find(dimensionId);
    if (dit == set.items.end()) return;
    const TSA::Annotation::Dimension& dim = dit->second;
    const TSA::Annotation::DimensionStyle& st = set.style;
    const TSA::Annotation::DimensionLayout layout = TSA::Annotation::layoutFor(model, dim, st);

    const bool invalid = layout.invalidReference || !layout.valid;
    Quantity_Color color = colorOf(dim.color, colorOf(st.color, Quantity_Color(Quantity_NOC_GOLD)));
    if (invalid) color = colorOf(st.invalidColor, Quantity_Color(Quantity_NOC_RED));
    if (highlighted) color = Quantity_Color(Quantity_NOC_CYAN1);

    Visual v;
    // Lignes (une forme par cotation : peu d'objets graphiques, même avec de nombreuses cotations).
    TopoDS_Compound comp;
    BRep_Builder builder;
    builder.MakeCompound(comp);
    int edges = 0;
    for (const auto& [a, b] : layout.segments)
    {
        if (a.Distance(b) < 1e-9) continue;
        builder.Add(comp, BRepBuilderAPI_MakeEdge(a, b).Edge());
        ++edges;
    }
    if (edges > 0)
    {
        v.lines = new AIS_Shape(comp);
        v.lines->SetColor(color);
        v.lines->SetWidth(highlighted ? 2.5 : 1.5);
        v.lines->SetZLayer(Graphic3d_ZLayerId_Topmost);
        m_context->Display(v.lines, AIS_WireFrame, 0, false);
        if (m_selection) m_selection->registerDimension(dimensionId, v.lines);
    }

    for (const auto& arrow : layout.arrows)
    {
        Handle(AIS_Shape) ais = makeArrow(arrow.tip, arrow.direction, st.arrowSizePx, color);
        m_context->Display(ais, AIS_Shaded, -1, false); // non sélectionnable
        v.arrows.push_back(ais);
    }
    for (const auto& marker : layout.levelMarkers)
    {
        Handle(AIS_Shape) ais = makeArrow(marker, gp_Dir(0, 0, -1), st.arrowSizePx * 1.2, color);
        m_context->Display(ais, AIS_Shaded, -1, false);
        v.arrows.push_back(ais);
    }

    auto addLabel = [&](const gp_Pnt& pos, const std::string& text, const gp_Dir& along, const gp_Dir& up) {
        Handle(AIS_TextLabel) label = new AIS_TextLabel();
        label->SetText(TCollection_ExtendedString(text.c_str(), true));
        label->SetPosition(pos);
        label->SetColor(color);
        label->SetHJustification(Graphic3d_HTA_CENTER);
        label->SetVJustification(Graphic3d_VTA_BOTTOM);
        label->SetFontAspect(Font_FA_Bold);
        label->SetZLayer(Graphic3d_ZLayerId_Topmost);
        // Fond sombre derrière le texte : lisible au-dessus des barres et de la grille.
        label->SetDisplayType(Aspect_TODT_SUBTITLE);
        label->SetColorSubTitle(Quantity_Color(0.08, 0.09, 0.11, Quantity_TOC_sRGB));
        if (st.textInPlane)
        {
            // Texte dans le plan de la cotation (taille en m, retourné pour rester lisible).
            gp_Vec normal = gp_Vec(along).Crossed(gp_Vec(up));
            if (normal.Magnitude() > 1e-9)
            {
                label->SetOrientation3D(gp_Ax2(pos, gp_Dir(normal), along));
                label->SetFlipping(true);
                label->SetHeight(0.15 * st.textHeightPx / 14.0);
            }
            else
                label->SetHeight(st.textHeightPx);
        }
        else
        {
            label->SetHeight(st.textHeightPx); // pixels : taille constante au zoom, face à la caméra
        }
        m_context->Display(label, 0, 0, false);
        if (m_selection) m_selection->registerDimension(dimensionId, label);
        v.labels.push_back(label);
    };
    if (layout.valid)
    {
        for (const auto& t : layout.texts)
            addLabel(t.position, invalid ? t.text + " (réf. supprimée)" : t.text, t.along, t.up);
    }
    else
    {
        const auto& p = dim.position;
        addLabel(gp_Pnt(p[0], p[1], p[2]), "Cotation invalide : " + layout.error, gp_Dir(1, 0, 0), gp_Dir(0, 0, 1));
    }
    m_visuals[dimensionId] = std::move(v);
}

} // namespace TSA::Viewer
