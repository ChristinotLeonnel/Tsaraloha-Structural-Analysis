// Repères de la vue 3D : trièdre global à l'origine du modèle, indicateur d'angle, repères locaux des éléments sélectionnés.

#include "OccView.h"

#include "AnnotationStyle.h"
#include "AxisDisplayStyle.h"
#include "SelectionManager.h"
#include "../Coordinate/AxisColorConfig.h"

#include <V3d_View.hxx>
#include <gp_Ax3.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <utility>

using TSA::Viewer::AxisDisplayStyle;
using TSA::Viewer::AxisTriadAppearance;
using TSA::Viewer::ElementAxisFrame;

AxisTriadAppearance OccView::axisAppearance(bool global, bool transverseAxes) const
{
    const auto& style = AxisDisplayStyle::instance().settings();
    const double ratio = TSA::Viewer::AnnotationStyle::instance().settings().devicePixelRatio;
    const auto& colors = TSA::Coordinate::AxisColorConfig::instance();
    AxisTriadAppearance a;
    a.colors = { TSA::Viewer::axisScreenColor(colors.axisXColor()), TSA::Viewer::axisScreenColor(colors.axisYColor()),
                 TSA::Viewer::axisScreenColor(colors.axisZColor()) };
    a.labels = global ? std::array<std::string, 3> { "X", "Y", "Z" } : std::array<std::string, 3> { "x", "y", "z" };
    a.lengthPixels = style.axisPixels * ratio;
    a.labelPixels = style.labelPixels * ratio;
    a.letters = style.letters;
    a.transverseAxes = transverseAxes;
    a.outline = m_isDarkMode ? Quantity_Color(Quantity_NOC_WHITE) : Quantity_Color(Quantity_NOC_BLACK);
    return a;
}

void OccView::initAxisDisplay()
{
    m_globalAxisTriad = TSA::Viewer::createAxisTriad(axisAppearance(true, true));
    TSA::Viewer::placeAxisTriad(m_globalAxisTriad, gp_Trsf());
    if (AxisDisplayStyle::instance().settings().globalAxes) m_context->Display(m_globalAxisTriad, 0, -1, false);
    TSA::Viewer::styleViewCubeAxes(m_viewCube, axisAppearance(true, true));
    m_context->Redisplay(m_viewCube, false);
    connect(&AxisDisplayStyle::instance(), &AxisDisplayStyle::changed, this, &OccView::applyAxisDisplay);
}

void OccView::displayCornerIndicator()
{
    if (m_view.IsNull()) return;
    if (!AxisDisplayStyle::instance().settings().cornerIndicator)
    {
        m_view->TriedronErase();
        return;
    }
    m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, m_isDarkMode ? Quantity_NOC_WHITE : Quantity_NOC_BLACK, 0.1, V3d_ZBUFFER);
}

void OccView::showGlobalAxisTriad(bool show)
{
    if (m_context.IsNull() || m_globalAxisTriad.IsNull()) return;
    if (show && !m_context->IsDisplayed(m_globalAxisTriad)) m_context->Display(m_globalAxisTriad, 0, -1, false);
    if (!show && m_context->IsDisplayed(m_globalAxisTriad)) m_context->Erase(m_globalAxisTriad, false);
}

void OccView::fitScene()
{
    // OCCT compte dans FitAll le point d'ancrage de tout objet à taille constante : un trièdre à l'origine, loin du modèle,
    // ferait ajuster la vue sur cette distance. Effacés le temps du calcul, sans recalculer leur géométrie.
    const bool global = !m_globalAxisTriad.IsNull() && m_context->IsDisplayed(m_globalAxisTriad);
    const bool workPlane = !m_workPlaneTrihedron.IsNull() && m_context->IsDisplayed(m_workPlaneTrihedron);
    if (global) m_context->Erase(m_globalAxisTriad, false);
    if (workPlane) m_context->Erase(m_workPlaneTrihedron, false);
    m_view->FitAll();
    if (global) m_context->Display(m_globalAxisTriad, 0, -1, false);
    if (workPlane) m_context->Display(m_workPlaneTrihedron, 0, -1, false);
}

bool OccView::workPlaneTriadCoincidesWithGlobal() const
{
    if (!AxisDisplayStyle::instance().settings().globalAxes) return false;
    const gp_Ax3 frame = m_workPlane.coordinateSystem();
    return frame.Location().Distance(gp_Pnt(0.0, 0.0, 0.0)) < 1e-9 && frame.XDirection().IsEqual(gp_Dir(1.0, 0.0, 0.0), 1e-9) &&
           frame.Direction().IsEqual(gp_Dir(0.0, 0.0, 1.0), 1e-9) && frame.YDirection().IsEqual(gp_Dir(0.0, 1.0, 0.0), 1e-9);
}

AxisTriadAppearance OccView::workPlaneAxisAppearance() const
{
    const auto& colors = TSA::Coordinate::AxisColorConfig::instance();
    AxisTriadAppearance a = axisAppearance(false, true);
    a.colors = { TSA::Viewer::axisScreenColor(colors.workPlaneAxisXColor()), TSA::Viewer::axisScreenColor(colors.workPlaneAxisYColor()),
                 TSA::Viewer::axisScreenColor(colors.workPlaneAxisZColor()) };
    a.labels = { "Xwp", "Ywp", "Zwp" };
    a.lengthPixels = std::max(20.0, 0.45 * m_gizmoSize);
    return a;
}

void OccView::applyAxisDisplay()
{
    if (m_context.IsNull() || m_view.IsNull()) return;
    const auto& style = AxisDisplayStyle::instance().settings();
    displayCornerIndicator();
    if (!m_globalAxisTriad.IsNull())
    {
        TSA::Viewer::styleAxisTriad(m_globalAxisTriad, axisAppearance(true, true));
        if (m_context->IsDisplayed(m_globalAxisTriad)) m_context->Redisplay(m_globalAxisTriad, false);
        showGlobalAxisTriad(style.globalAxes);
    }
    if (!m_viewCube.IsNull())
    {
        TSA::Viewer::styleViewCubeAxes(m_viewCube, axisAppearance(true, true));
        m_context->Redisplay(m_viewCube, false);
    }
    for (auto& t : m_localAxisTriads)
    {
        TSA::Viewer::styleAxisTriad(t.object, axisAppearance(false, t.transverseAxes));
        if (m_context->IsDisplayed(t.object)) m_context->Redisplay(t.object, false);
    }
    // Trièdre du plan de travail confondu avec le repère global : un seul dessin, selon que ce dernier est affiché ou non.
    if (m_workPlaneAxesVisible && m_workPlaneTrihedron.IsNull() != workPlaneTriadCoincidesWithGlobal())
    {
        updateWorkPlaneVisual();
    }
    else if (!m_workPlaneTrihedron.IsNull())
    {
        TSA::Viewer::styleAxisTriad(m_workPlaneTrihedron, workPlaneAxisAppearance());
        if (m_context->IsDisplayed(m_workPlaneTrihedron)) m_context->Redisplay(m_workPlaneTrihedron, false);
    }
    updateSelectedElementLocalAxes();
    m_view->Redraw();
}

void OccView::rememberPickRay(int px, int py)
{
    m_pendingPickRay.reset();
    if (m_view.IsNull() || AxisDisplayStyle::instance().settings().anchor != TSA::Viewer::LocalAxisAnchor::SelectedLocation) return;
    double x = 0.0, y = 0.0, z = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;
    m_view->ConvertWithProj(px, py, x, y, z, vx, vy, vz);
    const gp_Vec v(vx, vy, vz);
    if (v.Magnitude() > 1e-12) m_pendingPickRay = gp_Lin(gp_Pnt(x, y, z), gp_Dir(v));
}

void OccView::clearSelectedElementLocalAxes()
{
    if (m_context.IsNull() || m_localAxisFrames.empty()) return;
    for (const auto& t : m_localAxisTriads)
        if (m_context->IsDisplayed(t.object)) m_context->Erase(t.object, false);
    m_localAxisFrames.clear();
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::updateSelectedElementLocalAxes()
{
    if (m_context.IsNull()) return;
    const auto& style = AxisDisplayStyle::instance().settings();
    std::vector<ElementAxisFrame> frames;
    m_localAxesTooMany = false;
    if (style.localAxes && m_model && m_selectionManager)
        frames = TSA::Viewer::elementAxisFrames(*m_model, m_selectionManager->selectedElements(), &m_localAxesTooMany);

    // Point cliqué : retenu pour l'élément choisi par ce clic, qu'une modification du modèle garde au même endroit de la barre.
    const std::optional<gp_Lin> pending = std::exchange(m_pendingPickRay, std::nullopt);
    if (style.anchor == TSA::Viewer::LocalAxisAnchor::SelectedLocation && frames.size() == 1)
    {
        const std::pair<int, int> key { static_cast<int>(frames[0].kind), frames[0].id };
        if (pending)
        {
            m_pickRay = pending;
            m_pickRayElement = key;
        }
        if (m_pickRay && m_pickRayElement == key) frames[0].origin = TSA::Viewer::axisAnchorOnElement(frames[0], *m_pickRay);
    }

    if (frames.empty())
    {
        clearSelectedElementLocalAxes();
        return;
    }

    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        const ElementAxisFrame& f = frames[i];
        if (i == m_localAxisTriads.size())
            m_localAxisTriads.push_back({ TSA::Viewer::createAxisTriad(axisAppearance(false, f.transverseAxes)), f.transverseAxes });
        auto& t = m_localAxisTriads[i];
        if (t.transverseAxes != f.transverseAxes)
        {
            t.transverseAxes = f.transverseAxes;
            TSA::Viewer::styleAxisTriad(t.object, axisAppearance(false, f.transverseAxes));
            if (m_context->IsDisplayed(t.object)) m_context->Redisplay(t.object, false);
        }
        TSA::Viewer::placeAxisTriad(t.object, f.origin, f.x, f.y, f.z);
        if (!m_context->IsDisplayed(t.object)) m_context->Display(t.object, 0, -1, false);
    }
    for (std::size_t i = frames.size(); i < m_localAxisTriads.size(); ++i)
        if (m_context->IsDisplayed(m_localAxisTriads[i].object)) m_context->Erase(m_localAxisTriads[i].object, false);

    m_localAxisFrames = std::move(frames);
    if (!m_view.IsNull()) m_view->Redraw();
}
