#include "ViewManager.h"
#include <Graphic3d_Camera.hxx>
#include <V3d.hxx>

#include <cmath>

namespace TSA::Viewer
{

ViewManager::ViewManager(QObject* parent)
    : QObject(parent)
    , m_currentView(StandardCameraView::ThreeD_Axo)
{
}

void ViewManager::setOrthographic(bool ortho, const Handle(V3d_View)& view)
{
    m_isOrthographic = ortho;
    if (!view.IsNull() && !view->Camera().IsNull())
    {
        view->Camera()->SetProjectionType(ortho ? Graphic3d_Camera::Projection_Orthographic
                                               : Graphic3d_Camera::Projection_Perspective);
        view->Redraw();
    }
    emit projectionTypeChanged(m_isOrthographic);
}

bool ViewManager::cameraIsOrthographic(const Handle(V3d_View)& view)
{
    return view.IsNull() || view->Camera().IsNull() || view->Camera()->IsOrthographic();
}

CameraFrame ViewManager::frameForOrientation(V3d_TypeOfOrientation orientation)
{
    // GetProjAxis : de la cible vers l'œil ; la caméra regarde dans le sens opposé.
    const gp_Dir direction = V3d::GetProjAxis(orientation).Reversed();
    const gp_Vec z(0.0, 0.0, 1.0);
    const double along = gp_Vec(direction).Dot(z);
    if (std::abs(along) > 1.0 - 1e-12)
    {
        // Regard vertical : haut +Y vu de dessus, -Y vu de dessous (X reste vers la droite de l'écran).
        return { direction, along < 0.0 ? gp_Dir(0.0, 1.0, 0.0) : gp_Dir(0.0, -1.0, 0.0) };
    }
    return { direction, gp_Dir(z - gp_Vec(direction) * along) };
}

V3d_TypeOfOrientation ViewManager::orientationOf(StandardCameraView view)
{
    switch (view)
    {
    case StandardCameraView::Top: return V3d_Zpos;
    case StandardCameraView::Bottom: return V3d_Zneg;
    case StandardCameraView::Front: return V3d_Yneg;
    case StandardCameraView::Back: return V3d_Ypos;
    case StandardCameraView::Left: return V3d_Xneg;
    case StandardCameraView::Right: return V3d_Xpos;
    case StandardCameraView::ThreeD_Axo:
    default: return V3d_TypeOfOrientation_Zup_AxoRight;
    }
}

CameraFrame ViewManager::standardFrame(StandardCameraView view)
{
    return frameForOrientation(orientationOf(view));
}

CameraFrame ViewManager::planeFrame(const TSA::Coordinate::WorkPlane& wp, bool opposite)
{
    const gp_Dir n = wp.normal();
    // Base du plan reconstruite (U, V, N orthonormés même si le plan a été saisi avec un léger défaut d'équerre)
    const gp_Dir v = gp_Dir(n.Crossed(wp.xDirection()));
    return { opposite ? n : n.Reversed(), v };
}

void ViewManager::applyFrame(const Handle(V3d_View)& view, const CameraFrame& frame, const gp_Pnt& center)
{
    if (view.IsNull() || view->Camera().IsNull())
        return;
    const Handle(Graphic3d_Camera)& cam = view->Camera();
    double distance = cam->Distance();
    if (!(distance > 1e-6) || !std::isfinite(distance))
        distance = 100.0;
    cam->SetEyeAndCenter(center.Translated(gp_Vec(frame.direction) * -distance), center);
    cam->SetUp(frame.up);
}

void ViewManager::applyStandardView(StandardCameraView stdView,
                                    const Handle(V3d_View)& view,
                                    const TSA::Coordinate::WorkPlane& wp)
{
    if (view.IsNull())
        return;

    switch (stdView)
    {
    case StandardCameraView::NormalToPlane:
        viewNormalToWorkPlane(view, wp, false);
        return;
    case StandardCameraView::OppositePlane:
        viewNormalToWorkPlane(view, wp, true);
        return;
    case StandardCameraView::Custom:
        return;
    default:
        break;
    }

    m_currentView = stdView;
    applyFrame(view, standardFrame(stdView), view->Camera()->Center());
    view->FitAll();
    view->Redraw();
    emit cameraViewChanged(m_currentView);
}

void ViewManager::viewNormalToWorkPlane(const Handle(V3d_View)& view,
                                       const TSA::Coordinate::WorkPlane& wp,
                                       bool opposite)
{
    if (view.IsNull())
        return;

    m_currentView = opposite ? StandardCameraView::OppositePlane : StandardCameraView::NormalToPlane;

    // Regard le plan le long de sa normale, centré sur son origine
    applyFrame(view, planeFrame(wp, opposite), wp.origin());
    view->FitAll();
    view->Redraw();

    emit cameraViewChanged(m_currentView);
}

} // namespace TSA::Viewer
