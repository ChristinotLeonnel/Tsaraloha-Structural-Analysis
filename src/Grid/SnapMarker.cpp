#include "SnapMarker.h"

#include "SnapEngine.h"

#include <Font_NameOfFont.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_AspectText3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_Text.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_Presentation.hxx>

#include <cmath>
#include <utility>
#include <vector>

namespace TSA::Grid
{

namespace
{
using Seg = std::pair<std::pair<double, double>, std::pair<double, double>>;

void line(std::vector<Seg>& out, double x1, double y1, double x2, double y2) { out.push_back({ { x1, y1 }, { x2, y2 } }); }

void polygon(std::vector<Seg>& out, std::initializer_list<std::pair<double, double>> pts)
{
    const std::vector<std::pair<double, double>> v(pts);
    for (std::size_t i = 0; i < v.size(); ++i) line(out, v[i].first, v[i].second, v[(i + 1) % v.size()].first, v[(i + 1) % v.size()].second);
}

void circle(std::vector<Seg>& out, double r, int n = 20)
{
    for (int i = 0; i < n; ++i)
    {
        const double a0 = 2.0 * 3.14159265358979323846 * i / n, a1 = 2.0 * 3.14159265358979323846 * (i + 1) / n;
        line(out, r * std::cos(a0), r * std::sin(a0), r * std::cos(a1), r * std::sin(a1));
    }
}

/// Symbole du type d'accrochage, en pixels autour de l'origine (x à droite, y vers le haut).
std::vector<Seg> symbol(GridSnapType type, SnapSource source, double s)
{
    std::vector<Seg> g;
    if (source == SnapSource::WorkPlane)   // pas du plan de travail : simple « + » discret
    {
        line(g, -0.7 * s, 0.0, 0.7 * s, 0.0);
        line(g, 0.0, -0.7 * s, 0.0, 0.7 * s);
        return g;
    }
    switch (type)
    {
    case GridSnapType::Node:   // nœud TSA : cercle barré d'une croix
        circle(g, s);
        line(g, -0.55 * s, -0.55 * s, 0.55 * s, 0.55 * s);
        line(g, -0.55 * s, 0.55 * s, 0.55 * s, -0.55 * s);
        break;
    case GridSnapType::Endpoint:
        polygon(g, { { -s, -s }, { s, -s }, { s, s }, { -s, s } });
        break;
    case GridSnapType::Midpoint:
        polygon(g, { { 0.0, 1.15 * s }, { -s, -0.75 * s }, { s, -0.75 * s } });
        break;
    case GridSnapType::Center:
        circle(g, s);
        line(g, -0.3 * s, 0.0, 0.3 * s, 0.0);
        line(g, 0.0, -0.3 * s, 0.0, 0.3 * s);
        break;
    case GridSnapType::Intersection:
        if (source == SnapSource::Grid)   // nœud de grille : losange
        {
            polygon(g, { { 0.0, s }, { s, 0.0 }, { 0.0, -s }, { -s, 0.0 } });
            line(g, -0.3 * s, 0.0, 0.3 * s, 0.0);
        }
        else
        {
            line(g, -s, -s, s, s);
            line(g, -s, s, s, -s);
        }
        break;
    case GridSnapType::Perpendicular:
        line(g, -s, -s, s, -s);
        line(g, -s, -s, -s, s);
        polygon(g, { { -s, -s }, { 0.0, -s }, { 0.0, 0.0 }, { -s, 0.0 } });
        break;
    case GridSnapType::Nearest:   // sablier
        line(g, -s, s, s, s);
        line(g, s, s, -s, -s);
        line(g, -s, -s, s, -s);
        line(g, s, -s, -s, s);
        break;
    case GridSnapType::Origin:
        circle(g, s);
        line(g, -1.4 * s, 0.0, 1.4 * s, 0.0);
        line(g, 0.0, -1.4 * s, 0.0, 1.4 * s);
        break;
    case GridSnapType::AxisLine:
    case GridSnapType::RadialLine:
    case GridSnapType::Circle:   // sur un axe de grille : petit losange traversé
        line(g, -1.6 * s, 0.0, 1.6 * s, 0.0);
        polygon(g, { { 0.0, 0.6 * s }, { 0.6 * s, 0.0 }, { 0.0, -0.6 * s }, { -0.6 * s, 0.0 } });
        break;
    case GridSnapType::Face:   // parallélogramme
        polygon(g, { { -s, -0.6 * s }, { 0.45 * s, -0.6 * s }, { s, 0.6 * s }, { -0.45 * s, 0.6 * s } });
        break;
    default:
        line(g, -0.7 * s, 0.0, 0.7 * s, 0.0);
        line(g, 0.0, -0.7 * s, 0.0, 0.7 * s);
        break;
    }
    return g;
}

/// Couleur par famille : accrochages d'objet (ambre), suivis (cyan), grille (vert).
Quantity_Color colorFor(GridSnapType type, SnapSource source)
{
    if (source == SnapSource::Grid || source == SnapSource::WorkPlane) return Quantity_Color(0.29, 0.87, 0.50, Quantity_TOC_sRGB);
    if (!SnapEngine::isDiscrete(type)) return Quantity_Color(0.13, 0.83, 0.93, Quantity_TOC_sRGB);
    return Quantity_Color(1.00, 0.72, 0.00, Quantity_TOC_sRGB);
}
} // namespace

SnapMarker::SnapMarker()
{
    SetInfiniteState(true);   // hors des calculs d'emprise (FitAll ignore le marqueur)
    SetMutable(true);
}

std::string SnapMarker::labelFor(const GridSnapResult& snap)
{
    return SnapEngine::displayLabel(snap);
}

bool SnapMarker::setSnap(const GridSnapResult& snap)
{
    // Ancre exacte : le marqueur est dessiné en pixels autour de SnapResult.point
    SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomRotatePers, snap.point));
    // Pas du plan de travail : actif presque partout, il ne porte pas de libellé (comme le SNAP CAO)
    const std::string label = snap.source == SnapSource::WorkPlane ? std::string() : labelFor(snap);
    if (snap.type == m_type && snap.source == m_source && label == m_label) return false;
    m_type = snap.type;
    m_source = snap.source;
    m_label = label;
    SetToUpdate();
    return true;
}

void SnapMarker::setPixelScale(double scale)
{
    if (scale > 0.0 && std::abs(scale - m_scale) > 1e-6)
    {
        m_scale = scale;
        SetToUpdate();
    }
}

void SnapMarker::Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs, const int mode)
{
    if (mode != 0 || m_type == GridSnapType::None) return;
    const double s = 6.0 * m_scale;
    const auto segs = symbol(m_type, m_source, s);

    auto draw = [&](const Quantity_Color& color, double width) {
        Handle(Graphic3d_ArrayOfSegments) arr = new Graphic3d_ArrayOfSegments(static_cast<int>(segs.size()) * 2);
        for (const auto& [a, b] : segs)
        {
            arr->AddVertex(a.first, a.second, 0.0);
            arr->AddVertex(b.first, b.second, 0.0);
        }
        Handle(Graphic3d_Group) g = prs->NewGroup();
        g->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(color, Aspect_TOL_SOLID, width));
        g->AddPrimitiveArray(arr);
    };
    // Halo sombre puis trait coloré : lisible sur fond clair comme sur fond sombre
    draw(Quantity_Color(0.05, 0.06, 0.08, Quantity_TOC_sRGB), 4.0 * m_scale);
    draw(colorFor(m_type, m_source), 2.0 * m_scale);

    if (!m_label.empty())
    {
        Handle(Graphic3d_Group) g = prs->NewGroup();
        Handle(Graphic3d_AspectText3d) aspect =
            new Graphic3d_AspectText3d(Quantity_Color(0.96, 0.97, 0.98, Quantity_TOC_sRGB), Font_NOF_SANS_SERIF, 1.0, 0.0,
                                       Aspect_TOST_NORMAL, Aspect_TODT_SUBTITLE);
        aspect->SetColorSubTitle(Quantity_Color(0.07, 0.09, 0.12, Quantity_TOC_sRGB));
        g->SetGroupPrimitivesAspect(aspect);
        Handle(Graphic3d_Text) text = new Graphic3d_Text(static_cast<float>(13.0 * m_scale));
        text->SetText(m_label.c_str());
        text->SetPosition(gp_Pnt(1.9 * s, 1.4 * s, 0.0));
        text->SetHorizontalAlignment(Graphic3d_HTA_LEFT);
        text->SetVerticalAlignment(Graphic3d_VTA_BOTTOM);
        g->AddText(text);
    }
}

} // namespace TSA::Grid
