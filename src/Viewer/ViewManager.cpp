#include "ViewManager.h"
#include <Graphic3d_Camera.hxx>

namespace TSA::Viewer
{

ViewManager::ViewManager(QObject* parent)
    : QObject(parent)
    , m_currentView(StandardCameraView::ThreeD_Axo)
    , m_isOrthographic(false)
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

void ViewManager::applyStandardView(StandardCameraView stdView,
                                    const Handle(V3d_View)& view,
                                    const TSA::Coordinate::WorkPlane& wp)
{
    if (view.IsNull())
        return;

    m_currentView = stdView;

    switch (stdView)
    {
    case StandardCameraView::ThreeD_Axo:
        view->SetUp(0.0, 0.0, 1.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
        break;

    case StandardCameraView::Top:
        view->SetUp(0.0, 1.0, 0.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Top, false);
        break;

    case StandardCameraView::Bottom:
        view->SetUp(0.0, 1.0, 0.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Bottom, false);
        break;

    case StandardCameraView::Front:
        view->SetUp(0.0, 0.0, 1.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Front, false);
        break;

    case StandardCameraView::Back:
        view->SetUp(0.0, 0.0, 1.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Back, false);
        break;

    case StandardCameraView::Left:
        view->SetUp(0.0, 0.0, 1.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Left, false);
        break;

    case StandardCameraView::Right:
        view->SetUp(0.0, 0.0, 1.0);
        view->SetProj(V3d_TypeOfOrientation_Zup_Right, false);
        break;

    case StandardCameraView::NormalToPlane:
        viewNormalToWorkPlane(view, wp, false);
        return;

    case StandardCameraView::OppositePlane:
        viewNormalToWorkPlane(view, wp, true);
        return;

    case StandardCameraView::Custom:
    default:
        break;
    }

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

    gp_Pnt orig = wp.origin();
    gp_Dir norm = wp.normal();
    gp_Dir up   = wp.yDirection(); // Axe vertical local Ywp

    if (opposite)
    {
        norm = gp_Dir(gp_Vec(norm) * -1.0);
    }

    // Regard le plan le long de sa normale
    view->SetUp(up.X(), up.Y(), up.Z());
    view->SetProj(norm.X(), norm.Y(), norm.Z());
    view->SetAt(orig.X(), orig.Y(), orig.Z());
    view->FitAll();
    view->Redraw();

    emit cameraViewChanged(m_currentView);
}

} // namespace TSA::Viewer
