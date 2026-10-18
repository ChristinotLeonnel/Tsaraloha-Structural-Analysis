#include <QJsonArray>
#include "OccView.h"
#include "BimGeometryRenderer.h"
#include "../Coordinate/GeometryTolerance.h"
#include "SelectionManager.h"
#include "../Model/Model.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/Slab.h"
#include "../Model/Wall.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "../Grid/WorkPlaneReferences.h"
#include "../Coordinate/CoordinateTransformationService.h"
#include "../Coordinate/AxisColorConfig.h"
#include "ResultsVisualManager.h"
#include "../Model/SelectionQuery.h"
#include <tuple>
#include <QRegularExpression>
#include "../Analysis/ResultsModel.h"

#include <AIS_Shape.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Manipulator.hxx>
#include <AIS_ViewCube.hxx>
#include <V3d_View.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <Prs3d_Drawer.hxx>
#include <AIS_Trihedron.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <QCursor>
#include <QElapsedTimer>
#include <QThread>
#include <Prs3d_DatumParts.hxx>
#include <Prs3d_DatumMode.hxx>
#include <TCollection_ExtendedString.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <Bnd_Box.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <cmath>

void OccView::fitAll()
{
    if (!m_view.IsNull())
    {
        m_view->FitAll();
        m_view->ZFitAll();
        m_view->Redraw();
        emit viewCameraChanged();
    }
}

void OccView::fitSelection()
{
    if (m_view.IsNull() || !m_model || !m_selectionManager || !m_selectionManager->hasSelection())
    {
        fitAll();
        return;
    }

    pushCameraHistory();
    Bnd_Box bndBox;
    bool hasGeom = false;

    for (int nId : m_selectionManager->selectedNodes())
    {
        if (const auto* n = m_model->getNode(nId))
        {
            bndBox.Add(gp_Pnt(n->x(), n->y(), n->z()));
            hasGeom = true;
        }
    }
    for (int bId : m_selectionManager->selectedBeams())
    {
        if (const auto* b = m_model->getBeam(bId))
        {
            const auto* n1 = m_model->getNode(b->startNodeId());
            const auto* n2 = m_model->getNode(b->endNodeId());
            if (n1) { bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z())); hasGeom = true; }
            if (n2) { bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z())); hasGeom = true; }
        }
    }
    for (int cId : m_selectionManager->selectedColumns())
    {
        if (const auto* c = m_model->getColumn(cId))
        {
            const auto* n1 = m_model->getNode(c->startNodeId());
            const auto* n2 = m_model->getNode(c->endNodeId());
            if (n1) { bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z())); hasGeom = true; }
            if (n2) { bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z())); hasGeom = true; }
        }
    }
    for (int cbId : m_selectionManager->selectedCables())
    {
        if (const auto* cb = m_model->getCable(cbId))
        {
            const auto* n1 = m_model->getNode(cb->startNodeId());
            const auto* n2 = m_model->getNode(cb->endNodeId());
            if (n1) { bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z())); hasGeom = true; }
            if (n2) { bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z())); hasGeom = true; }
        }
    }
    for (int trId : m_selectionManager->selectedTrussMembers())
    {
        if (const auto* tr = m_model->getTrussMember(trId))
        {
            const auto* n1 = m_model->getNode(tr->startNodeId());
            const auto* n2 = m_model->getNode(tr->endNodeId());
            if (n1) { bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z())); hasGeom = true; }
            if (n2) { bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z())); hasGeom = true; }
        }
    }
    for (int sId : m_selectionManager->selectedSlabs())
    {
        if (const auto* s = m_model->getSlab(sId))
        {
            for (int nid : s->nodeIds())
            {
                if (const auto* n = m_model->getNode(nid))
                {
                    bndBox.Add(gp_Pnt(n->x(), n->y(), n->z()));
                    hasGeom = true;
                }
            }
        }
    }
    for (int wId : m_selectionManager->selectedWalls())
    {
        if (const auto* w = m_model->getWall(wId))
        {
            const auto* n1 = m_model->getNode(w->startNodeId());
            const auto* n2 = m_model->getNode(w->endNodeId());
            if (n1)
            {
                bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z()));
                bndBox.Add(gp_Pnt(n1->x(), n1->y(), n1->z() + w->height()));
                hasGeom = true;
            }
            if (n2)
            {
                bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z()));
                bndBox.Add(gp_Pnt(n2->x(), n2->y(), n2->z() + w->height()));
                hasGeom = true;
            }
        }
    }
    for (int fId : m_selectionManager->selectedFoundations())
    {
        if (const auto* f = m_model->getFoundation(fId))
        {
            if (const auto* n = m_model->getNode(f->nodeId()))
            {
                bndBox.Add(gp_Pnt(n->x() - f->widthA() * 0.5, n->y() - f->lengthB() * 0.5, n->z() - f->heightH()));
                bndBox.Add(gp_Pnt(n->x() + f->widthA() * 0.5, n->y() + f->lengthB() * 0.5, n->z()));
                hasGeom = true;
            }
        }
    }

    if (!hasGeom || bndBox.IsVoid())
    {
        fitAll();
        return;
    }

    bndBox.Enlarge(0.5);
    m_view->FitAll(bndBox, 0.15, true);
    m_view->ZFitAll();
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::fitModelWhenReady()
{
    if (m_view.IsNull()) m_fitModelOnInit = true;
    else fitModel();
}

void OccView::fitModel()
{
    const bool imported = m_bimRenderer && m_bimRenderer->visible() && m_bimRenderer->size() > 0;   // couche masquée : hors cadrage
    if (m_view.IsNull() || !m_model || (m_model->nodes().empty() && !imported))
    {
        fitAll();
        return;
    }

    pushCameraHistory();
    Bnd_Box bndBox;
    for (const auto& [id, node] : m_model->nodes())
    {
        bndBox.Add(gp_Pnt(node.x(), node.y(), node.z()));
    }
    if (imported) bndBox.Add(m_bimRenderer->bounds());   // objets physiques importés sans élément de calcul

    if (bndBox.IsVoid())
    {
        fitAll();
        return;
    }

    bndBox.Enlarge(0.5);
    m_view->FitAll(bndBox, 0.15, true);
    m_view->ZFitAll();
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::fitBox(const Bnd_Box& box)
{
    if (m_view.IsNull() || box.IsVoid()) return;
    pushCameraHistory();
    Bnd_Box b = box;
    b.Enlarge(std::max(0.05, std::sqrt(b.SquareExtent()) * 0.02));
    m_view->FitAll(b, 0.1, true);
    m_view->ZFitAll();
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::fitDeformed()
{
    if (m_view.IsNull() || !m_model || !m_resultsVisual || !m_resultsVisual->hasResults())
    {
        fitModel();
        return;
    }

    pushCameraHistory();
    Bnd_Box bndBox;
    double scale = m_resultsVisual->deformationScale();
    const auto results = m_resultsVisual->resultsModel();

    for (const auto& [id, node] : m_model->nodes())
    {
        double dx = 0.0, dy = 0.0, dz = 0.0;
        if (results && results->hasNodeDisplacement(id))
        {
            const auto& disp = results->nodeDisplacement(id);
            dx = disp.ux * scale;
            dy = disp.uy * scale;
            dz = disp.uz * scale;
        }
        bndBox.Add(gp_Pnt(node.x() + dx, node.y() + dy, node.z() + dz));
    }

    if (bndBox.IsVoid())
    {
        fitModel();
        return;
    }

    bndBox.Enlarge(0.5);
    m_view->FitAll(bndBox, 0.15, true);
    m_view->ZFitAll();
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::fitResults()
{
    if (m_view.IsNull() || !m_model || !m_resultsVisual || !m_resultsVisual->hasResults())
    {
        fitModel();
        return;
    }

    pushCameraHistory();
    Bnd_Box bndBox;
    double defScale = m_resultsVisual->deformationScale();
    const auto results = m_resultsVisual->resultsModel();

    for (const auto& [id, node] : m_model->nodes())
    {
        bndBox.Add(gp_Pnt(node.x(), node.y(), node.z()));

        if (results && results->hasNodeDisplacement(id))
        {
            const auto& disp = results->nodeDisplacement(id);
            bndBox.Add(gp_Pnt(node.x() + disp.ux * defScale,
                              node.y() + disp.uy * defScale,
                              node.z() + disp.uz * defScale));
        }
    }

    if (bndBox.IsVoid())
    {
        fitAll();
        return;
    }

    bndBox.Enlarge(1.0);
    m_view->FitAll(bndBox, 0.15, true);
    m_view->ZFitAll();
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::resetView()
{
    showStandardView(TSA::Viewer::StandardCameraView::ThreeD_Axo);
}

void OccView::viewHome()
{
    resetView();
}

void OccView::viewTop()
{
    showStandardView(TSA::Viewer::StandardCameraView::Top);
}

void OccView::viewBottom()
{
    showStandardView(TSA::Viewer::StandardCameraView::Bottom);
}

void OccView::viewFront()
{
    showStandardView(TSA::Viewer::StandardCameraView::Front);
}

void OccView::viewBack()
{
    showStandardView(TSA::Viewer::StandardCameraView::Back);
}

void OccView::viewLeft()
{
    showStandardView(TSA::Viewer::StandardCameraView::Left);
}

void OccView::viewRight()
{
    showStandardView(TSA::Viewer::StandardCameraView::Right);
}

void OccView::viewIsometric()
{
    showStandardView(TSA::Viewer::StandardCameraView::ThreeD_Axo);
}

namespace
{
bool isFace(TSA::Viewer::StandardCameraView v)
{
    using V = TSA::Viewer::StandardCameraView;
    return v == V::Top || v == V::Bottom || v == V::Front || v == V::Back || v == V::Left || v == V::Right;
}

/// Face du cube correspondant à une orientation (Custom pour une arête, un coin ou l'axonométrie).
TSA::Viewer::StandardCameraView faceOf(V3d_TypeOfOrientation o)
{
    using V = TSA::Viewer::StandardCameraView;
    switch (o)
    {
    case V3d_Zpos: return V::Top;
    case V3d_Zneg: return V::Bottom;
    case V3d_Yneg: return V::Front;
    case V3d_Ypos: return V::Back;
    case V3d_Xneg: return V::Left;
    case V3d_Xpos: return V::Right;
    default: return V::Custom;
    }
}

/// Plan parallèle à la vue (normale alignée sur la direction de visée, au signe près).
bool facesView(const gp_Dir& normal, const gp_Dir& direction)
{
    return std::abs(std::abs(normal.Dot(direction)) - 1.0) < 1e-9;
}
} // namespace

void OccView::applyCameraFrame(const TSA::Viewer::CameraFrame& frame, bool orthographic, bool animate)
{
    if (m_view.IsNull() || m_view->Camera().IsNull())
        return;
    pushCameraHistory();
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    Handle(Graphic3d_Camera) start = new Graphic3d_Camera(cam);
    if (orthographic && !cam->IsOrthographic())
        m_viewManager.setOrthographic(true, m_view);
    TSA::Viewer::ViewManager::applyFrame(m_view, frame, cam->Center());
    m_view->FitAll();
    m_view->ZFitAll();

    if (animate)
        animateCameraFrom(start);
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::animateCameraFrom(const Handle(Graphic3d_Camera)& start)
{
    // Transition de la caméra de départ vers la caméra actuelle (finale), réglée sur le temps écoulé : durée bornée
    // même si une image est lente (grand modèle : une seule image intermédiaire, ou aucune), puis caméra finale
    // exacte — aucune orientation intermédiaire ne peut rester.
    if (m_viewTransitionMs <= 0 || !isVisible() || start.IsNull() || m_view.IsNull() || m_view->Camera().IsNull())
        return;
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    Handle(Graphic3d_Camera) end = new Graphic3d_Camera(cam);
    if (start->Direction().IsEqual(end->Direction(), 1e-9) && start->Up().IsEqual(end->Up(), 1e-9)
        && start->Center().IsEqual(end->Center(), 1e-9) && std::abs(start->Scale() - end->Scale()) < 1e-9)
        return;
    Handle(Graphic3d_Camera) from = new Graphic3d_Camera(start);
    from->SetProjectionType(end->ProjectionType());   // jamais de perspective intermédiaire vers une vue orthographique
    Handle(Graphic3d_Camera) step = new Graphic3d_Camera(cam);
    QElapsedTimer timer;
    timer.start();
    for (;;)
    {
        const double t = double(timer.elapsed()) / m_viewTransitionMs;
        if (t >= 1.0)
            break;
        Graphic3d_Camera::Interpolate(from, end, t * t * (3.0 - 2.0 * t), step);   // départ et arrivée adoucis
        cam->Copy(step);
        m_view->Redraw();
        QThread::msleep(4);
    }
    cam->Copy(end);
}

void OccView::showStandardView(TSA::Viewer::StandardCameraView view)
{
    using V = TSA::Viewer::StandardCameraView;
    if (m_view.IsNull())
        return;
    if (view == V::NormalToPlane || view == V::OppositePlane)
    {
        if (m_mode2DActive)
        {
            m_plan2DFace = V::Custom;
            orient2DView(true);
            return;
        }
        applyCameraFrame(TSA::Viewer::ViewManager::planeFrame(m_workPlane, view == V::OppositePlane), true, true);
        return;
    }
    if (!isFace(view))
    {
        // Axonométrie (ou orientation libre) : vue 3D, jamais en mode 2D.
        if (m_mode2DActive)
            leaveMode2D(false);
        applyCameraFrame(TSA::Viewer::ViewManager::standardFrame(V::ThreeD_Axo), false, true);
        m_viewManager.setCurrentView(V::ThreeD_Axo);
        emit standardViewChanged(V::ThreeD_Axo);
        return;
    }
    if (m_mode2DActive)
    {
        showPlanView(view);
        return;
    }
    const TSA::Viewer::CameraFrame frame = TSA::Viewer::ViewManager::standardFrame(view);
    // Plan de travail vu par la tranche : tout point cliqué tomberait hors de lui ; plan de la face à la place.
    if (std::abs(m_workPlane.normal().Dot(frame.direction)) < 1e-6)
        setActiveWorkPlane(workPlaneForFace(view));
    applyCameraFrame(frame, true, true);
    m_viewManager.setCurrentView(view);
    emit standardViewChanged(view);
}

TSA::Coordinate::WorkPlane OccView::workPlaneForFace(TSA::Viewer::StandardCameraView face) const
{
    using V = TSA::Viewer::StandardCameraView;
    using T = TSA::Coordinate::WorkPlaneType;
    const gp_Dir direction = TSA::Viewer::ViewManager::standardFrame(face).direction;
    if (facesView(m_workPlane.normal(), direction))
        return m_workPlane;   // plan actif déjà face à la vue (niveau, coupe nommée…) : conservé
    switch (face)
    {
    case V::Front:
    case V::Back:
        return TSA::Coordinate::WorkPlane(T::GlobalXZ, "Élévation Y", m_lastElevationXZOffset);
    case V::Left:
    case V::Right:
        return TSA::Coordinate::WorkPlane(T::GlobalYZ, "Élévation X", m_lastElevationYZOffset);
    default:
        return TSA::Coordinate::WorkPlane(T::GlobalXY, "Plan Z", m_activeLevelZ);
    }
}

TSA::Viewer::CameraFrame OccView::plan2DFrame() const
{
    if (isFace(m_plan2DFace))
    {
        const TSA::Viewer::CameraFrame face = TSA::Viewer::ViewManager::standardFrame(m_plan2DFace);
        if (facesView(m_workPlane.normal(), face.direction))
            return face;
    }
    return TSA::Viewer::ViewManager::planeFrame(m_workPlane);
}

void OccView::orient2DView(bool animate)
{
    if (m_view.IsNull() || m_view->Camera().IsNull())
        return;
    Handle(Graphic3d_Camera) start = new Graphic3d_Camera(m_view->Camera());
    if (!m_view->Camera()->IsOrthographic())
        m_viewManager.setOrthographic(true, m_view);
    TSA::Viewer::ViewManager::applyFrame(m_view, plan2DFrame(), m_workPlane.origin());
    syncViewPlaneMode();
    updateElementIsolation();
    m_view->FitAll();
    m_view->ZFitAll();
    if (animate)
        animateCameraFrom(start);
    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::showPlanView(TSA::Viewer::StandardCameraView face)
{
    if (m_view.IsNull() || !isFace(face))
        return;
    pushCameraHistory();
    m_plan2DFace = face;
    const TSA::Coordinate::WorkPlane wp = workPlaneForFace(face);
    if (!m_mode2DActive)
    {
        if (!facesView(m_workPlane.normal(), wp.normal()) || !m_workPlane.origin().IsEqual(wp.origin(), 1e-9))
            setActiveWorkPlane(wp);
        setMode2D(true);   // orientation de la face (plan2DFrame)
    }
    else
    {
        // mode 2D : setActiveWorkPlane réoriente sur la face ; transition depuis la caméra d'avant le changement
        Handle(Graphic3d_Camera) start = new Graphic3d_Camera(m_view->Camera());
        setActiveWorkPlane(wp);
        orient2DView();
        animateCameraFrom(start);
        m_view->Redraw();
    }
    m_viewManager.setCurrentView(face);
    emit standardViewChanged(face);
}

void OccView::show3DView()
{
    if (m_mode2DActive)
    {
        setMode2D(false);
        return;
    }
    showStandardView(TSA::Viewer::StandardCameraView::ThreeD_Axo);
}

namespace
{
const char* faceKey(TSA::Viewer::StandardCameraView v)
{
    using V = TSA::Viewer::StandardCameraView;
    switch (v)
    {
    case V::Top: return "top";
    case V::Bottom: return "bottom";
    case V::Front: return "front";
    case V::Back: return "back";
    case V::Left: return "left";
    case V::Right: return "right";
    default: return "custom";
    }
}

TSA::Viewer::StandardCameraView faceFromKey(const std::string& key)
{
    using V = TSA::Viewer::StandardCameraView;
    for (V v : { V::Top, V::Bottom, V::Front, V::Back, V::Left, V::Right })
        if (key == faceKey(v))
            return v;
    return V::Custom;
}
} // namespace

TSA::Model::NamedView OccView::captureNamedView(const std::string& name) const
{
    TSA::Model::NamedView v;
    v.name = name;
    if (m_view.IsNull() || m_view->Camera().IsNull())
        return v;
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    // Œil recalculé depuis la direction : en orthographique, OCCT peut ramener l'œil sur la cible (distance nulle) en
    // gardant la direction à part ; un œil confondu avec la cible perdrait l'orientation.
    const gp_Pnt center = cam->Center();
    const double distance = std::max(cam->Distance(), std::max(cam->Scale(), 1.0));
    const gp_Pnt eye = center.Translated(gp_Vec(cam->Direction()) * -distance);
    const gp_Dir up = cam->Up();
    v.eye = { eye.X(), eye.Y(), eye.Z() };
    v.center = { center.X(), center.Y(), center.Z() };
    v.up = { up.X(), up.Y(), up.Z() };
    v.scale = cam->Scale();
    v.orthographic = cam->IsOrthographic();
    v.mode2D = m_mode2DActive;
    v.planFace = faceKey(m_mode2DActive ? m_plan2DFace : TSA::Viewer::StandardCameraView::Custom);
    v.workPlaneJson = m_workPlane.toJson();
    return v;
}

bool OccView::restoreNamedView(const TSA::Model::NamedView& v)
{
    if (m_view.IsNull() || m_view->Camera().IsNull())
        return false;
    const gp_Pnt eye(v.eye[0], v.eye[1], v.eye[2]), center(v.center[0], v.center[1], v.center[2]);
    const gp_Vec upVec(v.up[0], v.up[1], v.up[2]);
    if (!(v.scale > 0.0) || eye.Distance(center) < 1e-9 || upVec.Magnitude() < 1e-12 || gp_Vec(eye, center).IsParallel(upVec, 1e-9))
        return false;
    pushCameraHistory();
    Handle(Graphic3d_Camera) start = new Graphic3d_Camera(m_view->Camera());

    // Plan de travail et mode 2D de la vue (le plan d'abord : en mode 2D, il détermine l'orientation)
    if (v.mode2D)
    {
        m_plan2DFace = faceFromKey(v.planFace);
        if (!v.workPlaneJson.empty())
            setActiveWorkPlane(TSA::Coordinate::WorkPlane::fromJson(v.workPlaneJson));
        if (!m_mode2DActive)
            setMode2D(true);
        m_plan2DFace = faceFromKey(v.planFace);
    }
    else
    {
        if (m_mode2DActive)
            leaveMode2D(false);
        if (!v.workPlaneJson.empty())
            setActiveWorkPlane(TSA::Coordinate::WorkPlane::fromJson(v.workPlaneJson));
    }

    // Caméra exacte de la vue
    m_viewManager.setOrthographic(v.orthographic, m_view);
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    cam->SetEyeAndCenter(eye, center);
    cam->SetUp(gp_Dir(upVec));
    cam->SetScale(v.scale);
    m_view->ZFitAll();
    keep2DConsistent();
    animateCameraFrom(start);
    syncViewPlaneMode();
    m_view->Redraw();
    m_viewManager.setCurrentView(m_mode2DActive ? m_plan2DFace : TSA::Viewer::StandardCameraView::Custom);
    emit standardViewChanged(m_viewManager.currentView());
    emit viewCameraChanged();
    return true;
}

void OccView::showCubeOrientation(V3d_TypeOfOrientation orientation)
{
    const TSA::Viewer::StandardCameraView face = faceOf(orientation);
    if (face != TSA::Viewer::StandardCameraView::Custom)
    {
        showStandardView(face);
        return;
    }
    // Arête ou coin : vue 3D déterministe (projection inchangée), transition puis orientation exacte.
    if (m_mode2DActive)
        leaveMode2D(false);
    applyCameraFrame(TSA::Viewer::ViewManager::frameForOrientation(orientation), false, true);
    m_viewManager.setCurrentView(TSA::Viewer::StandardCameraView::Custom);
    emit standardViewChanged(TSA::Viewer::StandardCameraView::Custom);
}

void OccView::zoomIn(double factor)
{
    if (m_view.IsNull() || factor <= 0.0)
        return;
    pushCameraHistory();
    QPoint center(width() / 2, height() / 2);
    zoomAtCursor(center, factor);
    emit viewCameraChanged();
}

void OccView::zoomOut(double factor)
{
    if (m_view.IsNull() || factor <= 0.0)
        return;
    pushCameraHistory();
    QPoint center(width() / 2, height() / 2);
    zoomAtCursor(center, 1.0 / factor);
    emit viewCameraChanged();
}

void OccView::zoomWindow(int x1, int y1, int x2, int y2)
{
    if (m_view.IsNull())
        return;
    pushCameraHistory();
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2);
    if (maxX - minX > 5 && maxY - minY > 5)
    {
        m_view->WindowFitAll(minX, minY, maxX, maxY);
        m_view->Redraw();
        emit viewCameraChanged();
    }
}

void OccView::startInteractiveZoomWindow()
{
    m_currentAction = CurrentAction::ZoomWindow;
    setCursor(Qt::CrossCursor);
}

void OccView::rotate2D(double angleDeg)
{
    if (m_view.IsNull() || std::abs(angleDeg) < 1e-4)
        return;
    pushCameraHistory();
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    if (!cam.IsNull())
    {
        double angleRad = angleDeg * 3.14159265358979323846 / 180.0;
        gp_Dir dir = cam->Direction();
        gp_Dir up = cam->Up();
        gp_Trsf rot;
        rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), dir), angleRad);
        up.Transform(rot);
        cam->SetUp(up);
        m_view->Update();
        m_view->Redraw();
        emit viewCameraChanged();
    }
}

void OccView::pushCameraHistory()
{
    if (m_view.IsNull() || m_isRestoringCamera)
        return;

    const Handle(Graphic3d_Camera)& currentCam = m_view->Camera();
    if (currentCam.IsNull())
        return;

    if (!m_cameraUndoStack.empty())
    {
        const auto& top = m_cameraUndoStack.back();
        if (top->Center().IsEqual(currentCam->Center(), 1e-4) &&
            top->Eye().IsEqual(currentCam->Eye(), 1e-4) &&
            top->Up().IsEqual(currentCam->Up(), 1e-4) &&
            std::abs(top->Scale() - currentCam->Scale()) < 1e-4)
        {
            return;
        }
    }

    Handle(Graphic3d_Camera) savedCam = new Graphic3d_Camera();
    savedCam->Copy(currentCam);
    m_cameraUndoStack.push_back(savedCam);
    if (m_cameraUndoStack.size() > MAX_CAMERA_HISTORY)
    {
        m_cameraUndoStack.erase(m_cameraUndoStack.begin());
    }
    m_cameraRedoStack.clear();

    emit cameraHistoryChanged(hasPreviousView(), hasNextView());
}

bool OccView::hasPreviousView() const
{
    return !m_cameraUndoStack.empty();
}

bool OccView::hasNextView() const
{
    return !m_cameraRedoStack.empty();
}

void OccView::previousView()
{
    if (m_view.IsNull() || m_cameraUndoStack.empty())
        return;

    const Handle(Graphic3d_Camera)& currentCam = m_view->Camera();
    Handle(Graphic3d_Camera) currSaved = new Graphic3d_Camera();
    currSaved->Copy(currentCam);
    m_cameraRedoStack.push_back(currSaved);

    Handle(Graphic3d_Camera) prevCam = m_cameraUndoStack.back();
    m_cameraUndoStack.pop_back();

    m_isRestoringCamera = true;
    m_view->Camera()->Copy(prevCam);
    m_view->Update();
    m_view->Redraw();
    m_isRestoringCamera = false;
    keep2DConsistent();

    emit cameraHistoryChanged(hasPreviousView(), hasNextView());
    emit viewCameraChanged();
}

void OccView::nextView()
{
    if (m_view.IsNull() || m_cameraRedoStack.empty())
        return;

    const Handle(Graphic3d_Camera)& currentCam = m_view->Camera();
    Handle(Graphic3d_Camera) currSaved = new Graphic3d_Camera();
    currSaved->Copy(currentCam);
    m_cameraUndoStack.push_back(currSaved);

    Handle(Graphic3d_Camera) nextCam = m_cameraRedoStack.back();
    m_cameraRedoStack.pop_back();

    m_isRestoringCamera = true;
    m_view->Camera()->Copy(nextCam);
    m_view->Update();
    m_view->Redraw();
    m_isRestoringCamera = false;
    keep2DConsistent();

    emit cameraHistoryChanged(hasPreviousView(), hasNextView());
    emit viewCameraChanged();
}

void OccView::setActiveWorkPlane(const TSA::Coordinate::WorkPlane& requested)
{
    const TSA::Coordinate::WorkPlane wp = withCurrentPositionName(requested, m_workPlane);
    rememberWorkPlane(m_workPlane, wp);
    bool needFullRebuild = (m_workPlaneShape.IsNull() ||
                            std::abs(m_workPlane.width() - wp.width()) > 1e-4 ||
                            std::abs(m_workPlane.height() - wp.height()) > 1e-4 ||
                            m_workPlane.isVisible() != wp.isVisible() ||
                            m_workPlane.isGridVisible() != wp.isGridVisible());

    m_workPlane = wp;
    if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXY ||
        wp.type() == TSA::Coordinate::WorkPlaneType::ElevationZ)
    {
        m_activeLevelZ = wp.offset();
    }
    else if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXZ)
    {
        m_lastElevationXZOffset = wp.offset();
    }
    else if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalYZ)
    {
        m_lastElevationYZOffset = wp.offset();
    }
    else if (std::abs(wp.normal().Z()) > 1.0 - 1e-9)
    {
        m_activeLevelZ = wp.origin().Z();   // plan personnalisé horizontal : niveau actif à son altitude
    }
    TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(m_activeLevelZ, grid, m_context);

    if (needFullRebuild)
    {
        updateWorkPlaneVisual();
    }
    else
    {
        applyWorkPlaneTransformation();
        refreshWorkPlaneGrid();   // pas de la grille du nouveau plan
    }

    emit workPlaneChanged(m_workPlane);
    if (m_mode2DActive || m_projectionManager.is2D())
    {
        orient2DView();
    }
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setWorkPlaneElevation(double elevation)
{
    TSA::Coordinate::WorkPlane next = m_workPlane;
    next.setOffset(elevation);
    rememberWorkPlane(m_workPlane, next);
    m_workPlane.setOffset(elevation);
    m_activeLevelZ = elevation;
    TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(elevation, grid, m_context);
    applyWorkPlaneTransformation();
    emit workPlaneChanged(m_workPlane);
    if (m_mode2DActive || m_projectionManager.is2D())
    {
        orient2DView();
    }
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setWorkPlaneType(TSA::Coordinate::WorkPlaneType type, double offset)
{
    rememberWorkPlane(m_workPlane, TSA::Coordinate::WorkPlane(type, "Plan", offset));
    m_workPlane = TSA::Coordinate::WorkPlane(type, "Plan", offset);
    if (type == TSA::Coordinate::WorkPlaneType::GlobalXY ||
        type == TSA::Coordinate::WorkPlaneType::ElevationZ)
    {
        m_activeLevelZ = offset;
    }
    TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(m_activeLevelZ, grid, m_context);
    applyWorkPlaneTransformation();
    emit workPlaneChanged(m_workPlane);
    if (m_mode2DActive || m_projectionManager.is2D())
    {
        orient2DView();
    }
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setWorkPlaneAxisAndOffset(TSA::Coordinate::WorkPlaneAxis axis, double offset, const std::string& name)
{
    TSA::Coordinate::WorkPlaneType type = TSA::Coordinate::WorkPlaneType::GlobalXY;
    std::string defaultName = "Plan Z";
    if (axis == TSA::Coordinate::WorkPlaneAxis::X)
    {
        type = TSA::Coordinate::WorkPlaneType::GlobalYZ;
        defaultName = "Coupe X";
    }
    else if (axis == TSA::Coordinate::WorkPlaneAxis::Y)
    {
        type = TSA::Coordinate::WorkPlaneType::GlobalXZ;
        defaultName = "Coupe Y";
    }

    TSA::Coordinate::WorkPlane wp(type, name.empty() ? defaultName : name, offset);
    setActiveWorkPlane(wp);
}

void OccView::setWorkPlaneVisible(bool visible)
{
    if (m_workPlaneVisible == visible)
        return;
    m_workPlaneVisible = visible;
    m_workPlane.setIsVisible(visible);
    updateWorkPlaneVisual();
}

void OccView::viewNormalToWorkPlane()
{
    showStandardView(TSA::Viewer::StandardCameraView::NormalToPlane);
}

void OccView::applyWorkPlaneTrihedronTransform(const gp_Trsf& workPlaneTrsf)
{
    if (m_workPlaneTrihedron.IsNull())
        return;

    // En persistance de zoom, le repère local est en pixels et son origine est le point
    // d'ancrage (coordonnées monde) : la transformation locale ne doit donc contenir QUE
    // la rotation ; la translation est portée par le point d'ancrage.
    const gp_Pnt anchor(workPlaneTrsf.TranslationPart());
    gp_Trsf rotOnly = workPlaneTrsf;
    rotOnly.SetTranslationPart(gp_Vec(0.0, 0.0, 0.0));
    m_workPlaneTrihedron->SetLocalTransformation(rotOnly);

    const Handle(Graphic3d_TransformPers)& pers = m_workPlaneTrihedron->TransformPersistence();
    if (pers.IsNull() || pers->Mode() != Graphic3d_TMF_ZoomPers || !pers->AnchorPoint().IsEqual(anchor, 0.0))
    {
        m_workPlaneTrihedron->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, anchor));
    }
}

void OccView::attachManipulatorToWorkPlane()
{
    if (m_context.IsNull() || m_workPlaneShape.IsNull())
        return;

    if (m_workPlane.isLocked())
    {
        detachManipulator();
        return;
    }

    if (m_manipulator.IsNull())
    {
        m_manipulator = new AIS_Manipulator();
        m_manipulator->SetModeActivationOnDetection(true);
        m_manipulator->EnableMode(AIS_MM_Translation);
        m_manipulator->EnableMode(AIS_MM_Rotation);
        m_manipulator->SetPart(0, AIS_MM_Scaling, false);
        m_manipulator->SetPart(1, AIS_MM_Scaling, false);
        m_manipulator->SetPart(2, AIS_MM_Scaling, false);
    }

    if (m_manipulator->IsAttached())
    {
        m_manipulator->Detach();
    }

    AIS_Manipulator::OptionsForAttach opts;
    opts.SetAdjustPosition(false);
    // AIS_Manipulator est en persistance de zoom : sa taille est en PIXELS. AdjustSize la
    // remplaçait par la dimension du plan en mètres (~10) => gizmo minuscule (~10 px).
    opts.SetAdjustSize(false);
    opts.SetEnableModes(true);

    m_manipulator->Attach(m_workPlaneShape, opts);
    m_manipulator->SetSize(static_cast<float>(m_gizmoSize));
    m_manipulator->SetPosition(m_workPlane.coordinateSystem().Ax2());

    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::detachManipulator()
{
    m_isManipulatingWorkPlane = false;
    if (!m_manipulator.IsNull())
    {
        if (m_manipulator->HasActiveMode())
        {
            m_manipulator->StopTransform(false);
            m_manipulator->DeactivateCurrentMode();
        }
        if (m_manipulator->IsAttached())
        {
            m_manipulator->Detach();
        }
        if (!m_view.IsNull())
            m_view->Redraw();
    }
}

void OccView::applyWorkPlaneTransformation()
{
    if (m_workPlaneShape.IsNull())
        return;

    gp_Trsf trsf;
    gp_Ax3 stdCS(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    trsf.SetDisplacement(stdCS, m_workPlane.coordinateSystem());
    m_workPlaneShape->SetLocalTransformation(trsf);

    if (!m_manipulator.IsNull() && m_manipulator->IsAttached())
    {
        m_manipulator->SetPosition(m_workPlane.coordinateSystem().Ax2());
    }

    syncWorkPlaneDependents(trsf);
}

void OccView::syncWorkPlaneDependents(const gp_Trsf& trsf)
{
    // Grille du plan : même transformation que le panneau (aucune grille laissée à l'ancienne position)
    if (!m_workPlaneGrid.IsNull())
        m_workPlaneGrid->SetLocalTransformation(trsf);
    applyWorkPlaneTrihedronTransform(trsf);

    if (!m_viewer.IsNull())
    {
        m_viewer->SetPrivilegedPlane(m_workPlane.coordinateSystem());
    }
    TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);

    // Plan horizontal : niveau actif = son altitude (surlignage du niveau de la grille structurelle, pointage de repli)
    if (std::abs(m_workPlane.normal().Z()) > 1.0 - 1e-9 && std::abs(m_workPlane.origin().Z() - m_activeLevelZ) > 1e-12)
    {
        m_activeLevelZ = m_workPlane.origin().Z();
        const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
        m_gridRenderer.setActiveLevelElevation(m_activeLevelZ, grid, m_context);
    }

    updateElementIsolation(); // applique, met à jour ou lève l'isolation (no-op si jamais appliquée)

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::syncWorkPlaneFromManipulator()
{
    if (m_workPlaneShape.IsNull())
        return;
    const gp_Trsf trsf = m_workPlaneShape->LocalTransformation();
    gp_Pnt orig(0, 0, 0);
    orig.Transform(trsf);
    gp_Dir dirX(1, 0, 0), dirY(0, 1, 0), dirZ(0, 0, 1);
    dirX.Transform(trsf);
    dirY.Transform(trsf);
    dirZ.Transform(trsf);
    // Rotation réelle seulement : une translation garde le type du plan (plan d'étage, coupe) et son décalage
    if (!dirX.IsEqual(m_workPlane.xDirection(), 1e-12) || !dirY.IsEqual(m_workPlane.yDirection(), 1e-12))
        m_workPlane.setLocalAxes(dirX, dirY, dirZ);
    m_workPlane.setOrigin(orig);
    syncWorkPlaneDependents(trsf);
    emit workPlaneChanged(m_workPlane);
}

void OccView::rememberWorkPlane(const TSA::Coordinate::WorkPlane& previous, const TSA::Coordinate::WorkPlane& next)
{
    if (m_restoringWorkPlane)
        return;
    const bool same = previous.origin().Distance(next.origin()) < 1e-9 && previous.normal().IsEqual(next.normal(), 1e-12)
                      && previous.xDirection().IsEqual(next.xDirection(), 1e-12) && previous.type() == next.type();
    if (same)
        return;
    m_workPlaneHistory.push_back(previous);
    if (m_workPlaneHistory.size() > 50)
        m_workPlaneHistory.erase(m_workPlaneHistory.begin());
}

TSA::Coordinate::WorkPlane OccView::withCurrentPositionName(const TSA::Coordinate::WorkPlane& wp,
                                                            const TSA::Coordinate::WorkPlane& previous) const
{
    using T = TSA::Coordinate::WorkPlaneType;
    using A = TSA::Coordinate::WorkPlaneAxis;
    auto axisOf = [](const TSA::Coordinate::WorkPlane& p, A& axis, QChar& letter, double& coord) {
        switch (p.type())
        {
        case T::GlobalXY:
        case T::ElevationZ: axis = A::Z; letter = 'Z'; coord = p.origin().Z(); return true;
        case T::GlobalXZ: axis = A::Y; letter = 'Y'; coord = p.origin().Y(); return true;
        case T::GlobalYZ: axis = A::X; letter = 'X'; coord = p.origin().X(); return true;
        default: return false;
        }
    };
    A axis = A::Z, previousAxis = A::Z;
    QChar letter, previousLetter;
    double coord = 0.0, previousCoord = 0.0;
    if (!axisOf(wp, axis, letter, coord) || !axisOf(previous, previousAxis, previousLetter, previousCoord)
        || axis != previousAxis || std::abs(coord - previousCoord) < 1e-9)
        return wp;

    // Le nom cite-t-il la cote d'avant (arrondie à l'affichage, 2 décimales) ?
    static const QRegularExpression positional(QStringLiteral("([XYZ])\\s*=\\s*(-?\\d+(?:[.,]\\d+)?)"));
    bool stale = false;
    for (auto it = positional.globalMatch(QString::fromStdString(wp.name())); it.hasNext() && !stale;)
    {
        const auto m = it.next();
        stale = m.captured(1) == QString(letter)
                && std::abs(m.captured(2).replace(',', '.').toDouble() - previousCoord) < 0.006;
    }
    if (!stale)
        return wp;

    TSA::Coordinate::WorkPlane renamed = wp;
    QString name;
    if (m_model)
        for (const auto& level : m_model->detectStructuralPlanes(axis))
            if (std::abs(level.offset - coord) < 0.006)
            {
                name = QString::fromStdString(level.name);
                break;
            }
    if (name.isEmpty())
        for (const auto& ref : TSA::Grid::WorkPlaneReferences::parallelPlanes(structuralGrids(), wp))
            if (std::abs(ref.offset) < 1e-6 && !ref.label.empty())
            {
                name = QStringLiteral("%1 (%2=%3)").arg(QString::fromStdString(ref.label), QString(letter)).arg(coord, 0, 'f', 2);
                break;
            }
    if (name.isEmpty())
        name = QStringLiteral("%1 = %2 m").arg(QString(letter)).arg(coord, 0, 'f', 2);   // format du sélecteur de niveau
    renamed.setName(name.toStdString());
    return renamed;
}

std::vector<const TSA::Grid::GridSystem*> OccView::structuralGrids() const
{
    std::vector<const TSA::Grid::GridSystem*> grids;
    if (m_gridManager)
        for (const auto& g : m_gridManager->grids())
            if (g && g->isVisible())
                grids.push_back(g.get());
    return grids;
}

namespace
{
QString originText(const gp_Pnt& p)
{
    return QStringLiteral("(%1 ; %2 ; %3) m").arg(p.X(), 0, 'f', 3).arg(p.Y(), 0, 'f', 3).arg(p.Z(), 0, 'f', 3);
}
} // namespace

QString OccView::stepWorkPlaneAlongNormal(int direction)
{
    const auto ref = TSA::Grid::WorkPlaneReferences::nextPlane(structuralGrids(), m_workPlane, direction);
    if (!ref)
        return {};
    TSA::Coordinate::WorkPlane wp = m_workPlane;
    wp.setOrigin(m_workPlane.origin().Translated(gp_Vec(m_workPlane.normal()) * ref->offset));
    setActiveWorkPlane(wp);
    return tr("Plan de travail %1 : %2, origine %3")
        .arg(direction > 0 ? tr("suivant") : tr("précédent"),
             ref->label.empty() ? tr("décalage de %1 m").arg(ref->offset, 0, 'f', 3) : QString::fromStdString(ref->label),
             originText(m_workPlane.origin()));
}

QString OccView::stepWorkPlaneIntersection(int direction)
{
    const auto inter = TSA::Grid::WorkPlaneReferences::nextIntersection(structuralGrids(), m_workPlane, direction);
    if (!inter)
        return {};
    TSA::Coordinate::WorkPlane wp = m_workPlane;
    wp.setOrigin(inter->point);
    setActiveWorkPlane(wp);
    return tr("Origine du plan sur l'intersection %1 %2").arg(QString::fromStdString(inter->label), originText(inter->point));
}

QString OccView::alignWorkPlaneOriginToIntersection(const gp_Pnt& target)
{
    const auto inter = TSA::Grid::WorkPlaneReferences::nearestIntersection(structuralGrids(), m_workPlane, target);
    if (!inter)
        return {};
    TSA::Coordinate::WorkPlane wp = m_workPlane;
    wp.setOrigin(inter->point);
    setActiveWorkPlane(wp);
    return tr("Origine du plan sur l'intersection la plus proche %1 %2").arg(QString::fromStdString(inter->label), originText(inter->point));
}

QString OccView::alignWorkPlaneOriginToCursorIntersection()
{
    return alignWorkPlaneOriginToIntersection(m_hasCursorPoint ? m_lastCursorPoint : m_workPlane.origin());
}

bool OccView::previousWorkPlane()
{
    if (m_workPlaneHistory.empty())
        return false;
    const TSA::Coordinate::WorkPlane previous = m_workPlaneHistory.back();
    m_workPlaneHistory.pop_back();
    m_restoringWorkPlane = true;
    setActiveWorkPlane(previous);
    m_restoringWorkPlane = false;
    return true;
}

void OccView::moveWorkPlaneBy(const gp_Vec& delta)
{
    setWorkPlaneOrigin(m_workPlane.origin().Translated(delta));
}

void OccView::setWorkPlaneOrigin(const gp_Pnt& origin)
{
    TSA::Coordinate::WorkPlane wp = m_workPlane;
    wp.setOrigin(origin);
    setActiveWorkPlane(wp);
}

void OccView::pushElementIsolationState()
{
    m_elementIsolationHistory.emplace_back(m_isolatedElements, m_hiddenElements);
}

void OccView::isolateElements(const TSA::Model::ElementSet& elements)
{
    if (elements.size() == 0) return;
    pushElementIsolationState();
    m_isolatedElements = elements;
    m_hiddenElements = {};
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::hideElements(const TSA::Model::ElementSet& elements)
{
    if (elements.size() == 0) return;
    pushElementIsolationState();
    auto merge = [](std::set<int>& into, const std::set<int>& from) { into.insert(from.begin(), from.end()); };
    merge(m_hiddenElements.nodes, elements.nodes);
    merge(m_hiddenElements.beams, elements.beams);
    merge(m_hiddenElements.columns, elements.columns);
    merge(m_hiddenElements.slabs, elements.slabs);
    merge(m_hiddenElements.walls, elements.walls);
    merge(m_hiddenElements.foundations, elements.foundations);
    merge(m_hiddenElements.trussMembers, elements.trussMembers);
    merge(m_hiddenElements.cables, elements.cables);
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::invertElementIsolation()
{
    if (!m_model || !hasElementIsolation()) return;
    // Visible ⇔ masqué : on isole exactement ce que la passe courante masque.
    TSA::Model::ElementSet all = TSA::Model::SelectionQuery::all(*m_model);
    TSA::Model::ElementSet inverted;
    const TSA::Model::ElementSet noIsolation;
    const TSA::Model::ElementSet& iso = m_isolatedElements ? *m_isolatedElements : noIsolation;
    auto invert = [&](const std::set<int>& every, const std::set<int>& isolated, const std::set<int>& hidden, std::set<int>& out) {
        for (int id : every)
        {
            const bool visible = !hidden.count(id) && (!m_isolatedElements || isolated.count(id));
            if (!visible) out.insert(id);
        }
    };
    invert(all.beams, iso.beams, m_hiddenElements.beams, inverted.beams);
    invert(all.columns, iso.columns, m_hiddenElements.columns, inverted.columns);
    invert(all.slabs, iso.slabs, m_hiddenElements.slabs, inverted.slabs);
    invert(all.walls, iso.walls, m_hiddenElements.walls, inverted.walls);
    invert(all.foundations, iso.foundations, m_hiddenElements.foundations, inverted.foundations);
    invert(all.trussMembers, iso.trussMembers, m_hiddenElements.trussMembers, inverted.trussMembers);
    invert(all.cables, iso.cables, m_hiddenElements.cables, inverted.cables);
    if (inverted.size() == 0) return;
    pushElementIsolationState();
    m_isolatedElements = inverted;
    m_hiddenElements = {};
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
}

bool OccView::undoElementIsolation()
{
    if (m_elementIsolationHistory.empty()) return false;
    std::tie(m_isolatedElements, m_hiddenElements) = m_elementIsolationHistory.back();
    m_elementIsolationHistory.pop_back();
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
    return true;
}

void OccView::showAllElements()
{
    if (!hasElementIsolation()) return;
    pushElementIsolationState();
    m_isolatedElements.reset();
    m_hiddenElements = {};
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::setWorkPlaneIsolation(bool isolated, double distance)
{
    m_workPlane.setIsIsolated(isolated);
    m_workPlane.setIsolationDistance(distance);
    updateElementIsolation();
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::savePre2DVisibility()
{
    // Seuls les drapeaux d'affichage sont mémorisés. La visibilité de chaque objet AIS se déduit
    // entièrement de ces drapeaux et de l'isolation (voir updateElementIsolation). Mémoriser les
    // handles AIS eux-mêmes était fragile : toute forme recréée ou supprimée pendant le mode 2D
    // (modification, Undo, suppression) laissait un handle périmé qui était RÉAFFICHÉ en sortie
    // du mode 2D (objet fantôme non sélectionnable, impossible à supprimer).
    m_pre2DNodesVisible = m_nodesVisible;
    m_pre2DNodeLabelsVisible = m_nodeLabelsVisible;
    m_pre2DSupportsVisible = m_supportsVisible;
    m_pre2DLoadsVisible = m_loadsVisible;
    m_pre2DWorkPlaneAxesVisible = m_workPlaneAxesVisible;
}

void OccView::restorePre2DVisibility()
{
    m_nodesVisible = m_pre2DNodesVisible;
    m_nodeLabelsVisible = m_pre2DNodeLabelsVisible;
    m_supportsVisible = m_pre2DSupportsVisible;
    m_loadsVisible = m_pre2DLoadsVisible;
    m_workPlaneAxesVisible = m_pre2DWorkPlaneAxesVisible;

    // m_mode2DActive est déjà faux : recalcul complet (restaure tout, ou conserve l'isolation
    // de proximité du WorkPlane si elle est active).
    updateElementIsolation();
}

bool OccView::isIsolationActive() const noexcept
{
    return m_mode2DActive || m_workPlane.isIsolated();
}

double OccView::isolationTolerance() const noexcept
{
    return m_mode2DActive ? TSA::Coordinate::GeometryTolerance::planeMembership : m_workPlane.isolationDistance();
}

bool OccView::keepNodeUnderIsolation(int nodeId) const
{
    return !isIsolationActive() || isNodeOnActiveWorkPlane(nodeId, isolationTolerance());
}

bool OccView::keepLinearUnderIsolation(int startNodeId, int endNodeId) const
{
    return !isIsolationActive() || isLinearElementOnActiveWorkPlane(startNodeId, endNodeId, isolationTolerance());
}

bool OccView::keepSurfaceUnderIsolation(const std::vector<int>& nodeIds) const
{
    return !isIsolationActive() || isSurfaceElementOnActiveWorkPlane(nodeIds, isolationTolerance());
}

bool OccView::reapplyIsolationIfActive()
{
    if (!isIsolationActive() && !m_isolationApplied)
        return false;
    updateElementIsolation();
    return true;
}

bool OccView::isPointOnActiveWorkPlane(const gp_Pnt& pt, double tol) const
{
    return std::abs(m_workPlane.distanceTo(pt)) <= tol;
}

bool OccView::isNodeOnActiveWorkPlane(int nodeId, double tol) const
{
    if (!m_model) return false;
    const auto* n = m_model->getNode(nodeId);
    if (!n) return false;
    return isPointOnActiveWorkPlane(gp_Pnt(n->x(), n->y(), n->z()), tol);
}

bool OccView::isLinearElementOnActiveWorkPlane(int startNodeId, int endNodeId, double tol) const
{
    if (!m_model) return false;
    const auto* n1 = m_model->getNode(startNodeId);
    const auto* n2 = m_model->getNode(endNodeId);
    if (!n1 || !n2) return false;

    gp_Pnt p1(n1->x(), n1->y(), n1->z());
    gp_Pnt p2(n2->x(), n2->y(), n2->z());

    double u1 = 0, v1 = 0, w1 = 0;
    double u2 = 0, v2 = 0, w2 = 0;
    m_workPlane.toLocal(p1, u1, v1, w1);
    m_workPlane.toLocal(p2, u2, v2, w2);

    if (std::abs(w1) <= tol || std::abs(w2) <= tol)
        return true;

    if ((w1 < -tol && w2 > tol) || (w1 > tol && w2 < -tol))
        return true;

    return false;
}

bool OccView::isSurfaceElementOnActiveWorkPlane(const std::vector<int>& nodeIds, double tol) const
{
    if (!m_model || nodeIds.empty()) return false;

    double minW = 1e12;
    double maxW = -1e12;
    bool hasNodeOnPlane = false;

    for (int nid : nodeIds)
    {
        const auto* n = m_model->getNode(nid);
        if (!n) continue;
        gp_Pnt p(n->x(), n->y(), n->z());
        double u = 0, v = 0, w = 0;
        m_workPlane.toLocal(p, u, v, w);

        if (std::abs(w) <= tol)
        {
            hasNodeOnPlane = true;
        }
        if (w < minW) minW = w;
        if (w > maxW) maxW = w;
    }

    if (hasNodeOnPlane) return true;
    if (minW < -tol && maxW > tol) return true;

    return false;
}

void OccView::setMode2D(bool enabled)
{
    if (m_mode2DActive == enabled)
    {
        if (enabled && !m_view.IsNull())
            orient2DView();
        return;
    }

    if (!enabled)
    {
        leaveMode2D(true);
        return;
    }

    m_mode2DActive = true;

    // 1. Sauvegarder la caméra 3D courante et sa projection réelle (celle de la caméra, pas un état mémorisé à part)
    if (!m_view.IsNull() && !m_view->Camera().IsNull())
    {
        m_savedCamera3D = new Graphic3d_Camera(m_view->Camera());
    }
    m_savedWasOrtho = TSA::Viewer::ViewManager::cameraIsOrthographic(m_view);
    m_viewBefore2D = m_viewManager.currentView();

    // 2. Sauvegarder l'état exact de visibilité non-destructif de tous les objets
    savePre2DVisibility();

    // 3. Afficher le plan de travail clairement
    m_workPlaneVisible = true;
    m_workPlane.setIsVisible(true);
    updateWorkPlaneVisual();

    m_projectionManager.setMode(TSA::Viewer::ProjectionMode::TwoD);

    // 4. Projection orthographique, caméra perpendiculaire au plan (face choisie ou normale du plan), isolation
    //    des éléments du plan actif, cadrage
    orient2DView(true);

    emit mode2DChanged(true);
    emit projectionModeChanged(TSA::Viewer::ProjectionMode::TwoD);
    emit viewCameraChanged();
}

void OccView::leaveMode2D(bool restoreCamera)
{
    if (!m_mode2DActive)
        return;
    m_mode2DActive = false;
    m_plan2DFace = TSA::Viewer::StandardCameraView::Custom;

    // 1. Restaurer la caméra 3D précédente (sauf orientation 3D demandée à la place : ViewCube, isométrie) et sa
    //    projection
    if (restoreCamera && !m_savedCamera3D.IsNull() && !m_view.IsNull() && !m_view->Camera().IsNull())
    {
        m_view->Camera()->Copy(m_savedCamera3D);
    }
    m_savedCamera3D.Nullify();
    m_viewManager.setOrthographic(m_savedWasOrtho, m_view);
    m_viewManager.setCurrentView(restoreCamera ? m_viewBefore2D : TSA::Viewer::StandardCameraView::Custom);

    // 2. Restaurer l'état de visibilité non-destructif initial
    restorePre2DVisibility();

    m_projectionManager.setMode(TSA::Viewer::ProjectionMode::ThreeD);
    syncViewPlaneMode();

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }

    emit mode2DChanged(false);
    emit projectionModeChanged(TSA::Viewer::ProjectionMode::ThreeD);
    emit viewCameraChanged();
}

void OccView::setElementCategoryVisible(ElementCategory category, bool visible)
{
    const unsigned bit = 1u << static_cast<unsigned>(category);
    const unsigned before = m_hiddenElementCategories;
    m_hiddenElementCategories = visible ? (m_hiddenElementCategories & ~bit) : (m_hiddenElementCategories | bit);
    if (before == m_hiddenElementCategories) return;

    m_isolationApplied = true; // force la passe même sans isolation de plan
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
}

void OccView::updateElementIsolation()
{
    if (m_context.IsNull() || !m_model)
        return;
    scheduleDisplayModeOverlays(); // axes superposés / maillage : suivent les éléments affichés

    const bool isolate = isIsolationActive();
    const bool byElements = hasElementIsolation();
    const bool hideForResults = m_resultsStructureDisplay == ResultsStructureDisplay::Hidden;
    // Rien n'a été masqué (isolation ou filtre de familles) et rien n'est actif : aucun travail.
    if (!isolate && !byElements && !m_isolationApplied && m_hiddenElementCategories == 0 && !hideForResults)
        return;
    // Quand l'isolation vient d'être désactivée (case « Isoler le plan » décochée, sortie du
    // mode 2D), cette passe réaffiche tout selon les seuls drapeaux d'affichage. Auparavant la
    // fonction sortait immédiatement et les éléments masqués le restaient définitivement.
    m_isolationApplied = isolate || byElements || m_hiddenElementCategories != 0 || hideForResults;

    // Isolation / masquage par éléments : un élément est visible s'il n'est pas masqué et, si une
    // isolation est active, s'il en fait partie. Un nœud isolé suit l'élément (ou est isolé lui-même).
    auto userKept = [&](const std::set<int>& isolated, const std::set<int>& hidden, int id) {
        if (hidden.count(id)) return false;
        return !m_isolatedElements || isolated.count(id) > 0;
    };
    const TSA::Model::ElementSet noIsolation;
    const TSA::Model::ElementSet& iso = m_isolatedElements ? *m_isolatedElements : noIsolation;
    std::set<int> isolatedNodes = iso.nodes;
    if (m_isolatedElements)
    {
        auto addEnds = [&](int a, int b) { isolatedNodes.insert(a); isolatedNodes.insert(b); };
        for (int id : iso.beams) if (const auto* e = m_model->getBeam(id)) addEnds(e->startNodeId(), e->endNodeId());
        for (int id : iso.columns) if (const auto* e = m_model->getColumn(id)) addEnds(e->startNodeId(), e->endNodeId());
        for (int id : iso.trussMembers) if (const auto* e = m_model->getTrussMember(id)) addEnds(e->startNodeId(), e->endNodeId());
        for (int id : iso.cables) if (const auto* e = m_model->getCable(id)) addEnds(e->startNodeId(), e->endNodeId());
        for (int id : iso.walls) if (const auto* e = m_model->getWall(id)) addEnds(e->startNodeId(), e->endNodeId());
        for (int id : iso.slabs) if (const auto* e = m_model->getSlab(id)) isolatedNodes.insert(e->nodeIds().begin(), e->nodeIds().end());
        for (int id : iso.foundations) if (const auto* e = m_model->getFoundation(id)) isolatedNodes.insert(e->nodeId());
    }
    auto nodeUserKept = [&](int nodeId) { return userKept(isolatedNodes, m_hiddenElements.nodes, nodeId); };

    const double tol = isolationTolerance();
    auto linearKept = [&](int a, int b) { return !isolate || isLinearElementOnActiveWorkPlane(a, b, tol); };
    auto catOn = [&](ElementCategory c) { return isElementCategoryVisible(c); };

    auto setShapeVisibility = [&](const Handle(AIS_InteractiveObject)& shape, bool visible) {
        if (shape.IsNull()) return;
        if (visible)
        {
            if (!m_context->IsDisplayed(shape))
                m_context->Display(shape, false);
        }
        else
        {
            if (m_context->IsDisplayed(shape))
                m_context->Erase(shape, false);
        }
    };

    // Cache : chaque nœud n'est testé qu'une fois par passe (nœuds, appuis, fondations, charges).
    std::map<int, bool> nodeOnPlane;
    auto nodeKept = [&](int nodeId) {
        if (!nodeUserKept(nodeId)) return false;
        if (!isolate) return true;
        auto it = nodeOnPlane.find(nodeId);
        if (it != nodeOnPlane.end()) return it->second;
        const bool k = isNodeOnActiveWorkPlane(nodeId, tol);
        nodeOnPlane.emplace(nodeId, k);
        return k;
    };

    // 1. Nœuds & libellés (filtre d'affichage des nœuds + isolation)
    for (const auto& [nid, shape] : m_nodeShapes)
    {
        const bool keep = !hideForResults && isNodeVisibleByFilter(nid) && nodeKept(nid);
        setShapeVisibility(shape, keep);
        auto itLbl = m_nodeLabels.find(nid);
        if (itLbl != m_nodeLabels.end())
            setShapeVisibility(itLbl->second, keep && m_nodeLabelsVisible);
    }

    // 2. Appuis & libellés (n'étaient pas filtrés : symboles d'appui hors plan visibles en 2D)
    for (const auto& [nid, shape] : m_supportShapes)
        setShapeVisibility(shape, m_supportsVisible && nodeKept(nid));
    for (const auto& [nid, lbl] : m_supportLabels)
        setShapeVisibility(lbl, m_supportsVisible && m_supportLabelsVisible && nodeKept(nid));

    // 3. Éléments linéaires
    for (const auto& [id, shape] : m_beamShapes)
    {
        const auto* e = m_model->getBeam(id);
        setShapeVisibility(shape, e && !hideForResults && catOn(ElementCategory::Beams) && userKept(iso.beams, m_hiddenElements.beams, id)
                                      && linearKept(e->startNodeId(), e->endNodeId()));
    }
    for (const auto& [id, shape] : m_columnShapes)
    {
        const auto* e = m_model->getColumn(id);
        setShapeVisibility(shape, e && !hideForResults && catOn(ElementCategory::Columns) && userKept(iso.columns, m_hiddenElements.columns, id)
                                      && linearKept(e->startNodeId(), e->endNodeId()));
    }
    for (const auto& [id, shape] : m_trussShapes)
    {
        const auto* e = m_model->getTrussMember(id);
        setShapeVisibility(shape, e && !hideForResults && catOn(ElementCategory::Trusses) && userKept(iso.trussMembers, m_hiddenElements.trussMembers, id)
                                      && linearKept(e->startNodeId(), e->endNodeId()));
    }
    for (const auto& [id, shape] : m_cableShapes)
    {
        const auto* e = m_model->getCable(id);
        setShapeVisibility(shape, e && !hideForResults && catOn(ElementCategory::Cables) && userKept(iso.cables, m_hiddenElements.cables, id)
                                      && linearKept(e->startNodeId(), e->endNodeId()));
    }
    for (const auto& [id, shape] : m_wallShapes)
    {
        const auto* e = m_model->getWall(id);
        setShapeVisibility(shape, e && catOn(ElementCategory::Walls) && userKept(iso.walls, m_hiddenElements.walls, id)
                                      && linearKept(e->startNodeId(), e->endNodeId()));
    }

    // 4. Éléments surfaciques & fondations
    for (const auto& [id, shape] : m_slabShapes)
    {
        const auto* e = m_model->getSlab(id);
        setShapeVisibility(shape, e && catOn(ElementCategory::Slabs) && userKept(iso.slabs, m_hiddenElements.slabs, id)
                                      && (!isolate || isSurfaceElementOnActiveWorkPlane(e->nodeIds(), tol)));
    }
    for (const auto& [id, shape] : m_foundationShapes)
    {
        const auto* e = m_model->getFoundation(id);
        setShapeVisibility(shape, e && catOn(ElementCategory::Foundations) && userKept(iso.foundations, m_hiddenElements.foundations, id)
                                      && nodeKept(e->nodeId()));
    }

    // 5. Charges nodales
    for (const auto& [nlId, shapes] : m_nodalLoadShapes)
    {
        const auto* nl = m_model->loadManager().getNodalLoad(nlId);
        const bool keep = nl && m_loadsVisible && nodeKept(nl->nodeId());
        for (const auto& sh : shapes)
            setShapeVisibility(sh, keep);
        auto itLbl = m_nodalLoadLabels.find(nlId);
        if (itLbl != m_nodalLoadLabels.end())
            setShapeVisibility(itLbl->second, keep && m_loadValuesVisible);
    }

    // 6. Charges sur barres : l'élément porteur dépend de targetType (poutre, poteau, treillis,
    //    câble). Auparavant seuls poutres/poteaux étaient testés, avec confusion possible entre
    //    une poutre et un poteau portant le même identifiant.
    for (const auto& [mlId, shapes] : m_memberLoadShapes)
    {
        const auto* ml = m_model->loadManager().getMemberLoad(mlId);
        bool keep = false;
        if (ml && m_loadsVisible)
        {
            int a = -1, b = -1;
            auto take = [&](const auto* host) {
                if (host) { a = host->startNodeId(); b = host->endNodeId(); }
            };
            switch (ml->targetType())
            {
            case TSA::Model::MemberTargetType::Beam:   take(m_model->getBeam(ml->elementId())); break;
            case TSA::Model::MemberTargetType::Column: take(m_model->getColumn(ml->elementId())); break;
            case TSA::Model::MemberTargetType::Truss:  take(m_model->getTrussMember(ml->elementId())); break;
            case TSA::Model::MemberTargetType::Cable:  take(m_model->getCable(ml->elementId())); break;
            }
            keep = a > 0 && b > 0 && linearKept(a, b);
        }
        for (const auto& sh : shapes)
            setShapeVisibility(sh, keep);
        auto itLbl = m_memberLoadLabels.find(mlId);
        if (itLbl != m_memberLoadLabels.end())
            setShapeVisibility(itLbl->second, keep && m_loadValuesVisible);
    }
}

void OccView::setResultsStructureDisplay(ResultsStructureDisplay mode)
{
    if (mode == m_resultsStructureDisplay)
        return;
    const ResultsStructureDisplay previous = m_resultsStructureDisplay;
    m_resultsStructureDisplay = mode;
    if (m_context.IsNull())
        return;

    // Atténuation : transparence des barres et nœuds d'origine, valeurs initiales restaurées ensuite.
    if (previous == ResultsStructureDisplay::Ghosted)
    {
        for (auto& [ptr, saved] : m_resultsGhostSaved)
            m_context->SetTransparency(saved.first, saved.second, false);
        m_resultsGhostSaved.clear();
    }
    if (mode == ResultsStructureDisplay::Ghosted)
    {
        auto ghost = [this](const auto& shapes) {
            for (const auto& [id, shape] : shapes)
            {
                if (shape.IsNull()) continue;
                m_resultsGhostSaved.emplace(shape.get(), std::make_pair(Handle(AIS_InteractiveObject)(shape), shape->Transparency()));
                m_context->SetTransparency(shape, 0.8, false);
            }
        };
        ghost(m_beamShapes);
        ghost(m_columnShapes);
        ghost(m_trussShapes);
        ghost(m_cableShapes);
        ghost(m_nodeShapes);
    }
    updateElementIsolation(); // masquage (déformée seule) dans la passe de visibilité unique
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setShowLocalAxes(bool show)
{
    m_showLocalAxes = show;
    if (!show)
    {
        clearSelectedElementLocalAxes();
    }
    else
    {
        updateSelectedElementLocalAxes();
    }
}

void OccView::clearSelectedElementLocalAxes()
{
    if (!m_elementLocalAxesShape.IsNull() && !m_context.IsNull())
    {
        m_context->Remove(m_elementLocalAxesShape, false);
        m_elementLocalAxesShape.Nullify();
        if (!m_view.IsNull())
            m_view->Redraw();
    }
}

void OccView::updateSelectedElementLocalAxes()
{
    if (m_context.IsNull() || !m_model || !m_selectionManager || !m_showLocalAxes)
        return;

    clearSelectedElementLocalAxes();

    gp_Pnt origin;
    gp_Dir dirX, dirY, dirZ;
    bool found = false;

    auto extractLinearElementAxes = [&](int n1Id, int n2Id, double rotationDeg) {
        const auto* n1 = m_model->getNode(n1Id);
        const auto* n2 = m_model->getNode(n2Id);
        if (n1 && n2)
        {
            gp_Pnt p1(n1->x(), n1->y(), n1->z());
            gp_Pnt p2(n2->x(), n2->y(), n2->z());
            if (p1.Distance(p2) > 1e-4)
            {
                origin = gp_Pnt((p1.X() + p2.X()) * 0.5, (p1.Y() + p2.Y()) * 0.5, (p1.Z() + p2.Z()) * 0.5);
                gp_Ax3 frame = TSA::Coordinate::CoordinateTransformationService::computeElementLocalFrame(p1, p2, rotationDeg);
                dirX = frame.XDirection();
                dirY = frame.YDirection();
                dirZ = frame.Direction();
                found = true;
            }
        }
    };

    if (!m_selectionManager->selectedBeams().empty())
    {
        int bId = *m_selectionManager->selectedBeams().begin();
        if (const auto* b = m_model->getBeam(bId))
            extractLinearElementAxes(b->startNodeId(), b->endNodeId(), b->rotation());
    }
    else if (!m_selectionManager->selectedColumns().empty())
    {
        int cId = *m_selectionManager->selectedColumns().begin();
        if (const auto* col = m_model->getColumn(cId))
            extractLinearElementAxes(col->startNodeId(), col->endNodeId(), col->rotation());
    }
    else if (!m_selectionManager->selectedCables().empty())
    {
        int cabId = *m_selectionManager->selectedCables().begin();
        if (const auto* cab = m_model->getCable(cabId))
            extractLinearElementAxes(cab->startNodeId(), cab->endNodeId(), 0.0);
    }
    else if (!m_selectionManager->selectedTrussMembers().empty())
    {
        int trId = *m_selectionManager->selectedTrussMembers().begin();
        if (const auto* tr = m_model->getTrussMember(trId))
            extractLinearElementAxes(tr->startNodeId(), tr->endNodeId(), 0.0);
    }

    if (!found) return;

    double L = 0.8;
    BRep_Builder b;
    TopoDS_Compound comp;
    b.MakeCompound(comp);

    gp_Pnt pX = origin.Translated(gp_Vec(dirX) * L);
    gp_Pnt pY = origin.Translated(gp_Vec(dirY) * L);
    gp_Pnt pZ = origin.Translated(gp_Vec(dirZ) * L);

    b.Add(comp, BRepBuilderAPI_MakeEdge(origin, pX).Edge());
    b.Add(comp, BRepBuilderAPI_MakeEdge(origin, pY).Edge());
    b.Add(comp, BRepBuilderAPI_MakeEdge(origin, pZ).Edge());

    m_elementLocalAxesShape = new AIS_Shape(comp);
    m_elementLocalAxesShape->SetColor(Quantity_NOC_YELLOW);
    m_elementLocalAxesShape->SetWidth(2.5);
    m_context->Display(m_elementLocalAxesShape, false);

    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::refreshWorkPlaneGrid()
{
    if (m_workPlaneGrid.IsNull() || m_view.IsNull() || m_view->Camera().IsNull() || m_view->Window().IsNull())
        return;
    int winW = 0, winH = 0;
    m_view->Window()->Size(winW, winH);
    // Taille d'un pixel dans le monde au centre de la vue (orthographique comme perspective)
    const double worldPerPixel = m_view->Camera()->ViewDimensions().Y() / std::max(1, winH);
    TSA::Viewer::WorkPlaneGrid::Layout layout;
    layout.halfU = m_workPlanePanelHalfU;
    layout.halfV = m_workPlanePanelHalfV;
    layout.centerU = m_workPlanePanelCenterU;
    layout.centerV = m_workPlanePanelCenterV;
    layout.stepU = TSA::Viewer::WorkPlaneGrid::displayedStep(m_workPlane.gridSpacingX(), worldPerPixel, layout.halfU);
    layout.stepV = TSA::Viewer::WorkPlaneGrid::displayedStep(m_workPlane.gridSpacingY(), worldPerPixel, layout.halfV);
    bool changed = m_workPlaneGrid->setColors(
        m_isDarkMode ? Quantity_Color(0.36, 0.43, 0.52, Quantity_TOC_sRGB) : Quantity_Color(0.62, 0.68, 0.76, Quantity_TOC_sRGB),
        m_isDarkMode ? Quantity_Color(0.55, 0.66, 0.80, Quantity_TOC_sRGB) : Quantity_Color(0.36, 0.46, 0.60, Quantity_TOC_sRGB));
    changed = m_workPlaneGrid->setLayout(layout) || changed;
    if (changed && !m_context.IsNull() && m_context->IsDisplayed(m_workPlaneGrid))
    {
        m_context->Redisplay(m_workPlaneGrid, false);
        m_view->Redraw();
    }
}

void OccView::updateWorkPlaneVisual()
{
    if (m_context.IsNull())
        return;

    // 1. Supprimer l'ancienne forme visuelle si existante
    if (!m_workPlaneShape.IsNull())
    {
        if (m_selectionManager)
        {
            m_selectionManager->unregisterWorkPlane(m_workPlane.id());
        }
        if (!m_manipulator.IsNull() && m_manipulator->IsAttached())
        {
            m_manipulator->Detach();
        }
        m_context->Remove(m_workPlaneShape, false);
        m_workPlaneShape.Nullify();
    }
    if (!m_workPlaneTrihedron.IsNull())
    {
        if (m_context->IsDisplayed(m_workPlaneTrihedron))
            m_context->Remove(m_workPlaneTrihedron, false);
        m_workPlaneTrihedron.Nullify();
    }
    if (!m_workPlaneGrid.IsNull())
    {
        m_context->Remove(m_workPlaneGrid, false);
        m_workPlaneGrid.Nullify();
    }

    if (!m_workPlaneVisible || !m_workPlane.isVisible())
    {
        updateElementIsolation(); // un plan masqué peut rester isolant (ou cesser de l'être)
        if (!m_view.IsNull())
            m_view->Redraw();
        return;
    }

    // 2. Détermination de la dimension du plan (largeur / hauteur paramétriques ou dynamique)
    double hx = (m_workPlane.width() > 0.0) ? (m_workPlane.width() * 0.5) : 10.0;
    double hy = (m_workPlane.height() > 0.0) ? (m_workPlane.height() * 0.5) : 10.0;
    double cu = 0.0, cv = 0.0;   // centre du panneau dans le repère du plan
    {
        if (m_model && !m_model->nodes().empty())
        {
            // Taille du plan = minimum (20 m × 20 m par défaut). Si le modèle projeté dans le plan (U, V) en dépasse,
            // le panneau, et sa grille, sont agrandis et recentrés sur l'emprise du modèle, avec une marge.
            double minU = 1e30, maxU = -1e30, minV = 1e30, maxV = -1e30;
            for (const auto& [nid, n] : m_model->nodes())
            {
                double u = 0.0, v = 0.0;
                m_workPlane.toLocal(gp_Pnt(n.x(), n.y(), n.z()), u, v);
                minU = std::min(minU, u);
                maxU = std::max(maxU, u);
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
            const double span = std::max(maxU - minU, maxV - minV);
            const bool outside = minU < -hx || maxU > hx || minV < -hy || maxV > hy;
            if (outside && span > 5.0)
            {
                cu = 0.5 * (minU + maxU);
                cv = 0.5 * (minV + maxV);
                hx = std::max(hx, 0.6 * (maxU - minU) + 0.1 * span);
                hy = std::max(hy, 0.6 * (maxV - minV) + 0.1 * span);
            }
        }
    }

    // 3. Panneau surfacique semi-transparent centré en (cu, cv) dans le repère local du plan
    gp_Pnt p00(cu - hx, cv - hy, 0.0);
    gp_Pnt p10(cu + hx, cv - hy, 0.0);
    gp_Pnt p11(cu + hx, cv + hy, 0.0);
    gp_Pnt p01(cu - hx, cv + hy, 0.0);

    BRepBuilderAPI_MakePolygon poly(p00, p10, p11, p01, true);
    if (poly.IsDone())
    {
        BRepBuilderAPI_MakeFace mkFace(poly.Wire());
        if (mkFace.IsDone())
        {
            // Rectangle plein (plus de quadrillage UV) : face teintée + contour net.
            m_workPlaneShape = new AIS_Shape(mkFace.Face());
            m_workPlaneShape->SetColor(Quantity_NOC_STEELBLUE);
            m_workPlaneShape->SetTransparency(0.55);
            m_workPlaneShape->Attributes()->SetFaceBoundaryDraw(true);
            m_workPlaneShape->Attributes()->SetFaceBoundaryAspect(
                new Prs3d_LineAspect(Quantity_NOC_STEELBLUE, Aspect_TOL_SOLID, 1.5));
            m_workPlaneShape->SetDisplayMode(AIS_Shaded);
            // Face repoussée en profondeur : les lignes de la grille, dans le même plan, restent devant elle
            m_workPlaneShape->Attributes()->SetupOwnShadingAspect();
            m_workPlaneShape->Attributes()->ShadingAspect()->Aspect()->SetPolygonOffsets(Aspect_POM_Fill, 1.0f, 2.0f);
            m_context->Display(m_workPlaneShape, false);
        }
    }

    // 4. Transformation 3D globale du plan
    gp_Trsf trsf;
    gp_Ax3 stdCS(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    trsf.SetDisplacement(stdCS, m_workPlane.coordinateSystem());
    if (!m_workPlaneShape.IsNull())
    {
        m_workPlaneShape->SetLocalTransformation(trsf);
    }

    // 4 bis. Grille du plan (lignes au pas d'accrochage, repère local du plan, même transformation que le panneau)
    m_workPlanePanelHalfU = hx;
    m_workPlanePanelHalfV = hy;
    m_workPlanePanelCenterU = cu;
    m_workPlanePanelCenterV = cv;
    if (m_workPlane.isGridVisible())
    {
        m_workPlaneGrid = new TSA::Viewer::WorkPlaneGrid();
        m_workPlaneGrid->SetLocalTransformation(trsf);
        refreshWorkPlaneGrid();
        m_context->Display(m_workPlaneGrid, 0, -1, false);   // purement visuel (sélection -1)
    }

    // 5. Axes Xwp, Ywp, Zwp avec lettres, couleurs AxisColorConfig (Section 2 & 12).
    //    Trièdre court à taille constante à l'écran, comme le trièdre 3D de la vue :
    //    longueur en PIXELS (proportionnelle à la taille du gizmo), indépendante du zoom
    //    et des dimensions du modèle.
    if (m_workPlaneAxesVisible)
    {
        const auto& colorConfig = TSA::Coordinate::AxisColorConfig::instance();
        const Quantity_Color cx = colorConfig.workPlaneAxisXColor();
        const Quantity_Color cy = colorConfig.workPlaneAxisYColor();
        const Quantity_Color cz = colorConfig.workPlaneAxisZColor();

        Handle(Geom_Axis2Placement) place = new Geom_Axis2Placement(gp::XOY());
        m_workPlaneTrihedron = new AIS_Trihedron(place);
        m_workPlaneTrihedron->SetDatumDisplayMode(Prs3d_DM_Shaded);
        m_workPlaneTrihedron->SetSize(std::max(20.0, 0.45 * m_gizmoSize));

        m_workPlaneTrihedron->SetDatumPartColor(Prs3d_DP_XAxis, cx);
        m_workPlaneTrihedron->SetDatumPartColor(Prs3d_DP_YAxis, cy);
        m_workPlaneTrihedron->SetDatumPartColor(Prs3d_DP_ZAxis, cz);
        m_workPlaneTrihedron->SetArrowColor(Prs3d_DP_XAxis, cx);
        m_workPlaneTrihedron->SetArrowColor(Prs3d_DP_YAxis, cy);
        m_workPlaneTrihedron->SetArrowColor(Prs3d_DP_ZAxis, cz);
        m_workPlaneTrihedron->SetTextColor(Prs3d_DP_XAxis, cx);
        m_workPlaneTrihedron->SetTextColor(Prs3d_DP_YAxis, cy);
        m_workPlaneTrihedron->SetTextColor(Prs3d_DP_ZAxis, cz);
        m_workPlaneTrihedron->SetLabel(Prs3d_DP_XAxis, TCollection_ExtendedString("X"));
        m_workPlaneTrihedron->SetLabel(Prs3d_DP_YAxis, TCollection_ExtendedString("Y"));
        m_workPlaneTrihedron->SetLabel(Prs3d_DP_ZAxis, TCollection_ExtendedString("Z"));

        applyWorkPlaneTrihedronTransform(trsf);
        // Mode de sélection -1 : purement visuel (ne gêne ni la sélection ni le snapping)
        m_context->Display(m_workPlaneTrihedron, 0, -1, false);
    }

    if (m_selectionManager && !m_workPlaneShape.IsNull())
    {
        m_selectionManager->registerWorkPlane(m_workPlane.id(), m_workPlaneShape);
    }

    if (!m_viewer.IsNull())
    {
        m_viewer->SetPrivilegedPlane(m_workPlane.coordinateSystem());
    }

    if (m_selectionManager && m_selectionManager->isWorkPlaneSelected() &&
        m_selectionManager->selectedWorkPlaneId() == m_workPlane.id())
    {
        attachManipulatorToWorkPlane();
    }

    updateElementIsolation(); // applique, met à jour ou lève l'isolation (no-op si jamais appliquée)

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::updateSnapMarker(const TSA::Grid::GridSnapResult& snap)
{
    m_lastSnapResult = snap;
    m_gridRenderer.showSnapMarker(snap, m_context);
    emit snapChanged(snap);
}

void OccView::clearSnapMarker()
{
    m_lastSnapResult = TSA::Grid::GridSnapResult();
    m_gridRenderer.hideSnapMarker(m_context);
    emit snapChanged(m_lastSnapResult);
}


void OccView::setViewOrientation(V3d_TypeOfOrientation orientation)
{
    showCubeOrientation(orientation);
}

void OccView::setViewPlaneMode(ViewPlaneMode mode)
{
    if (m_view.IsNull())
    {
        m_viewPlaneMode = mode;
        return;
    }

    switch (mode)
    {
    case ViewPlaneMode::PlanXY:
        // Vue en plan d'étage : vrai plan 2D sur le niveau actif
        showPlanView(TSA::Viewer::StandardCameraView::Top);
        break;
    case ViewPlaneMode::PlanXZ:
        // Élévation de face / portique
        showPlanView(TSA::Viewer::StandardCameraView::Front);
        break;
    case ViewPlaneMode::PlanYZ:
        // Élévation latérale / pignon
        showPlanView(TSA::Viewer::StandardCameraView::Right);
        break;
    case ViewPlaneMode::Perspective3D:
    default:
        // Retour à la vue 3D
        show3DView();
        break;
    }
}

void OccView::keep2DConsistent()
{
    // Mode 2D = caméra perpendiculaire au plan actif ; une caméra restaurée (historique, état enregistré) qui ne
    // l'est plus ramène en 3D sans toucher à cette caméra.
    if (m_mode2DActive && !m_view.IsNull() && !m_view->Camera().IsNull() && !facesView(m_workPlane.normal(), m_view->Camera()->Direction()))
        leaveMode2D(false);
}

void OccView::syncViewPlaneMode()
{
    using V = TSA::Viewer::StandardCameraView;
    ViewPlaneMode mode = ViewPlaneMode::Perspective3D;
    if (m_mode2DActive)
    {
        const gp_Dir n = m_workPlane.normal();
        if (std::abs(n.Z()) > 1.0 - 1e-9 || m_plan2DFace == V::Top || m_plan2DFace == V::Bottom)
            mode = ViewPlaneMode::PlanXY;
        else if (std::abs(n.Y()) > 1.0 - 1e-9)
            mode = ViewPlaneMode::PlanXZ;
        else if (std::abs(n.X()) > 1.0 - 1e-9)
            mode = ViewPlaneMode::PlanYZ;
    }
    if (mode != m_viewPlaneMode)
    {
        m_viewPlaneMode = mode;
        emit viewPlaneModeChanged(mode);
    }
}

void OccView::setLocalCoordinateSystem(bool local)
{
    m_isLocalCoordinateSystem = local;
    emit coordinateSystemChanged(local);
}

void OccView::setClippingEnabled(bool enabled)
{
    m_isClippingEnabled = enabled;
    if (m_view.IsNull())
        return;

    if (m_clipPlane.IsNull())
    {
        m_clipPlane = new Graphic3d_ClipPlane();
        m_clipPlane->SetCapping(true);
        m_clipPlane->SetCappingColor(Quantity_Color(0.85, 0.65, 0.15, Quantity_TOC_RGB));
    }

    if (enabled)
    {
        updateClipPlaneEquation();
        m_clipPlane->SetOn(true);
        m_view->AddClipPlane(m_clipPlane);
    }
    else
    {
        m_clipPlane->SetOn(false);
        m_view->RemoveClipPlane(m_clipPlane);
    }
    m_view->Redraw();
    emit clippingChanged(m_isClippingEnabled, m_clipAxisIndex, m_clipPosition, m_isClipFlipped);
}

void OccView::setClipPlane(int axisIndex, double position, bool flip)
{
    m_clipAxisIndex = axisIndex;
    m_clipPosition = position;
    m_isClipFlipped = flip;

    if (m_isClippingEnabled && !m_view.IsNull())
    {
        updateClipPlaneEquation();
    }
    // Ne met à jour que la transformation du rectangle (pas de reconstruction) et redessine.
    updateSectionPlaneVisual();
    if (!m_view.IsNull())
        m_view->Redraw();
    emit clippingChanged(m_isClippingEnabled, m_clipAxisIndex, m_clipPosition, m_isClipFlipped);
}

void OccView::setSectionPlaneVisible(bool visible)
{
    if (m_sectionPlaneVisible == visible)
        return;
    m_sectionPlaneVisible = visible;
    updateSectionPlaneVisual();
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::updateSectionPlaneVisual()
{
    if (m_context.IsNull())
        return;

    if (!m_sectionPlaneVisible)
    {
        if (!m_sectionPlaneShape.IsNull())
        {
            if (m_context->IsDisplayed(m_sectionPlaneShape))
                m_context->Remove(m_sectionPlaneShape, false);
            m_sectionPlaneShape.Nullify();
        }
        return;
    }

    // Centre et demi-taille déduits du modèle (repli : 10 m autour de l'origine)
    double cx = 0.0, cy = 0.0, cz = 0.0, half = 10.0;
    if (m_model && !m_model->nodes().empty())
    {
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
        for (const auto& [nid, n] : m_model->nodes())
        {
            minX = std::min(minX, n.x()); maxX = std::max(maxX, n.x());
            minY = std::min(minY, n.y()); maxY = std::max(maxY, n.y());
            minZ = std::min(minZ, n.z()); maxZ = std::max(maxZ, n.z());
        }
        cx = 0.5 * (minX + maxX);
        cy = 0.5 * (minY + maxY);
        cz = 0.5 * (minZ + maxZ);
        half = std::max(5.0, 0.6 * std::max({ maxX - minX, maxY - minY, maxZ - minZ }));
    }

    // Création unique : rectangle centré à l'origine local, dans son plan XY local.
    // Ensuite seule la transformation locale change (position / axe), sans rebuild.
    if (m_sectionPlaneShape.IsNull())
    {
        BRepBuilderAPI_MakePolygon poly(gp_Pnt(-half, -half, 0.0), gp_Pnt(half, -half, 0.0),
                                        gp_Pnt(half, half, 0.0), gp_Pnt(-half, half, 0.0), true);
        if (!poly.IsDone())
            return;
        BRepBuilderAPI_MakeFace mkFace(poly.Wire());
        if (!mkFace.IsDone())
            return;

        m_sectionPlaneShape = new AIS_Shape(mkFace.Face());
        m_sectionPlaneShape->SetColor(Quantity_Color(0.85, 0.65, 0.15, Quantity_TOC_RGB));
        m_sectionPlaneShape->SetTransparency(0.6);
        m_sectionPlaneShape->Attributes()->SetFaceBoundaryDraw(true);
        m_sectionPlaneShape->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(Quantity_Color(0.85, 0.65, 0.15, Quantity_TOC_RGB), Aspect_TOL_SOLID, 2.0));
        // Mode de sélection -1 : purement visuel, ne gêne ni la sélection ni le snapping.
        m_context->Display(m_sectionPlaneShape, AIS_Shaded, -1, false);
    }
    else if (!m_context->IsDisplayed(m_sectionPlaneShape))
    {
        m_context->Display(m_sectionPlaneShape, AIS_Shaded, -1, false);
    }

    // Décalage de 1 mm du côté conservé par la coupe : évite que le rectangle, situé
    // exactement sur le plan de coupe, soit lui-même écrêté (z-fighting / clipping).
    // Sens : normale du plan de coupe, identique à updateClipPlaneEquation().
    constexpr double kSectionPlaneOffset = 0.001;
    const double sgn = m_isClipFlipped ? -1.0 : 1.0;
    const double d = m_clipPosition + sgn * kSectionPlaneOffset;

    gp_Ax3 planeCS;
    if (m_clipAxisIndex == 1)       // XZ : coupe selon Y
        planeCS = gp_Ax3(gp_Pnt(cx, d, cz), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0));
    else if (m_clipAxisIndex == 2)  // YZ : coupe selon X
        planeCS = gp_Ax3(gp_Pnt(d, cy, cz), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
    else                            // XY : coupe selon Z
        planeCS = gp_Ax3(gp_Pnt(cx, cy, d), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));

    gp_Trsf trsf;
    trsf.SetDisplacement(gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), planeCS);
    m_sectionPlaneShape->SetLocalTransformation(trsf);
}

void OccView::updateClipPlaneEquation()
{
    if (m_clipPlane.IsNull())
        return;

    gp_Dir normal(0, 0, 1);
    gp_Pnt pnt(0, 0, m_clipPosition);

    if (m_clipAxisIndex == 0) // XY, coupe selon Z
    {
        normal = m_isClipFlipped ? gp_Dir(0, 0, -1) : gp_Dir(0, 0, 1);
        pnt = gp_Pnt(0, 0, m_clipPosition);
    }
    else if (m_clipAxisIndex == 1) // XZ, coupe selon Y
    {
        normal = m_isClipFlipped ? gp_Dir(0, -1, 0) : gp_Dir(0, 1, 0);
        pnt = gp_Pnt(0, m_clipPosition, 0);
    }
    else if (m_clipAxisIndex == 2) // YZ, coupe selon X
    {
        normal = m_isClipFlipped ? gp_Dir(-1, 0, 0) : gp_Dir(1, 0, 0);
        pnt = gp_Pnt(m_clipPosition, 0, 0);
    }

    m_clipPlane->SetEquation(gp_Pln(pnt, normal));
}

void OccView::setCadBlueprintTheme(bool enabled)
{
    setDarkMode(!enabled);
}

void OccView::setDarkMode(bool dark)
{
    m_isDarkMode = dark;
    m_gridRenderer.setDarkMode(dark);
    refreshWorkPlaneGrid();   // couleurs de la grille du plan selon le thème

    if (!m_view.IsNull())
    {
        m_view->TriedronDisplay(
            Aspect_TOTP_LEFT_LOWER,
            dark ? Quantity_NOC_WHITE : Quantity_NOC_BLACK,
            0.1,
            V3d_ZBUFFER
        );

        if (dark)
        {
            Quantity_Color topColor(0.12, 0.14, 0.18, Quantity_TOC_RGB);
            Quantity_Color bottomColor(0.06, 0.08, 0.10, Quantity_TOC_RGB);
            m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);

            if (!m_viewCube.IsNull())
            {
                m_viewCube->SetBoxColor(Quantity_Color(0.24, 0.28, 0.34, Quantity_TOC_RGB));
                m_viewCube->SetInnerColor(Quantity_Color(0.16, 0.19, 0.24, Quantity_TOC_RGB));
                m_viewCube->SetTextColor(Quantity_Color(0.90, 0.93, 0.96, Quantity_TOC_RGB));
                if (!m_context.IsNull() && m_context->IsDisplayed(m_viewCube))
                {
                    m_context->Redisplay(m_viewCube, false);
                }
            }
        }
        else
        {
            Quantity_Color topColor(0.82, 0.88, 0.95, Quantity_TOC_RGB);
            Quantity_Color bottomColor(0.92, 0.94, 0.98, Quantity_TOC_RGB);
            m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);

            if (!m_viewCube.IsNull())
            {
                m_viewCube->SetBoxColor(Quantity_Color(0.92, 0.94, 0.96, Quantity_TOC_RGB));
                m_viewCube->SetInnerColor(Quantity_Color(0.85, 0.88, 0.92, Quantity_TOC_RGB));
                m_viewCube->SetTextColor(Quantity_Color(0.10, 0.12, 0.15, Quantity_TOC_RGB));
                if (!m_context.IsNull() && m_context->IsDisplayed(m_viewCube))
                {
                    m_context->Redisplay(m_viewCube, false);
                }
            }
        }
    }

    Quantity_Color labelColor = dark
        ? Quantity_Color(0.2, 0.9, 0.9, Quantity_TOC_RGB)
        : Quantity_Color(0.0, 0.4, 0.6, Quantity_TOC_RGB);
    for (auto& [id, lbl] : m_nodeLabels)
    {
        if (!lbl.IsNull())
        {
            lbl->SetColor(labelColor);
            if (!m_context.IsNull() && m_context->IsDisplayed(lbl))
            {
                m_context->Redisplay(lbl, false);
            }
        }
    }

    rebuildGrid();

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::setGridManager(TSA::Grid::GridManager* gridManager, TSA::Grid::GridSnapManager* snapManager)
{
    if (m_gridManager)
    {
        disconnect(m_gridManager, nullptr, this, nullptr);
    }

    m_gridManager = gridManager;
    m_gridSnapManager = snapManager;
    m_snapManager.setGridSnapManager(snapManager);

    if (m_gridManager)
    {
        connect(m_gridManager, &TSA::Grid::GridManager::gridAdded, this, [this](const std::string&) { rebuildGrid(); });
        connect(m_gridManager, &TSA::Grid::GridManager::gridModified, this, [this](const std::string&) { rebuildGrid(); });
        connect(m_gridManager, &TSA::Grid::GridManager::gridRemoved, this, [this](const std::string&) { rebuildGrid(); });
        connect(m_gridManager, &TSA::Grid::GridManager::activeGridChanged, this, [this](const std::string&) { rebuildGrid(); });
        connect(m_gridManager, &TSA::Grid::GridManager::gridVisibilityChanged, this, [this](const std::string&, bool) { rebuildGrid(); });
    }

    rebuildGrid();
}

void OccView::rebuildGrid()
{
    if (m_context.IsNull())
        return;

    m_gridRenderer.clearGrid(m_context);
    updateGridLabelView(); // vue courante pour les repères des grilles reconstruites ci-dessous

    if (m_gridVisible && m_gridManager)
    {
        for (const auto& grid : m_gridManager->grids())
        {
            if (grid && grid->isVisible())
            {
                m_gridRenderer.renderGrid(*grid, m_context);
                m_gridRenderer.setActiveLevelElevation(m_activeLevelZ, grid.get(), m_context);
            }
        }
    }

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::updateGridLabelView()
{
    if (m_context.IsNull() || m_view.IsNull() || m_view->Camera().IsNull() || !m_gridManager)
        return;

    // Repère de la grille active (rotation) pour classer la visée ; les autres grilles suivent la
    // même catégorie (cas courant : grilles parallèles).
    const TSA::Grid::GridSystem* active = m_gridManager->activeGrid();
    const double rotation = active ? active->definition().rotationDeg() : 0.0;
    TSA::Grid::GridLabelView view;
    view.kind = TSA::Grid::classifyGridView(m_view->Camera()->Direction(), rotation);
    view.hasActiveLevel = true;
    view.activeLevelZ = m_activeLevelZ;
    if (view.sameLayoutAs(m_gridRenderer.labelView()))
        return;

    m_gridRenderer.setLabelView(view);
    if (!m_gridVisible)
        return;
    for (const auto& grid : m_gridManager->grids())
        if (grid && grid->isVisible())
            m_gridRenderer.refreshLabels(*grid, m_context);
    m_view->Redraw();
}

void OccView::setActiveLevelElevation(double z)
{
    m_activeLevelZ = z;

    // Règle : le niveau ne déplace le WorkPlane que si la synchronisation est active ET que le
    // plan est horizontal. Un plan vertical/incliné/personnalisé reste indépendant du niveau.
    // (Avant : setOffset() reconstruisait le repère en XY/XZ/YZ standard, ce qui détruisait
    // l'orientation d'un plan personnalisé et déplaçait un plan XZ/YZ sur le mauvais axe.)
    const bool moved = m_syncWorkPlaneWithLevel && m_workPlane.moveToElevation(z);

    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(z, grid, m_context);
    updateGridLabelView(); // en plan, les repères suivent le niveau actif

    if (moved)
    {
        TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
        applyWorkPlaneTransformation(); // simple transformation locale des objets AIS du plan, sans reconstruction
        emit workPlaneChanged(m_workPlane);
    }
    if (m_mode2DActive)
    {
        m_viewManager.viewNormalToWorkPlane(m_view, m_workPlane, false);
        updateElementIsolation();
        if (!m_view.IsNull())
        {
            m_view->FitAll();
        }
    }
    if (!m_view.IsNull())
    {
        if (!moved || m_workPlaneShape.IsNull())
            m_view->Redraw();
        emit viewCameraChanged();
    }
}

void OccView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    m_gridRenderer.setGridVisible(visible, m_context);
    if (!visible)
    {
        m_gridRenderer.hideSnapMarker(m_context);
    }
    emit gridVisibilityChanged(visible);

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

bool OccView::isGridVisible() const
{
    return m_gridVisible;
}

void OccView::refreshSnapAtCursor()
{
    if (m_context.IsNull() || m_view.IsNull())
        return;
    if (!m_snapToGrid && !m_snapToObject)
    {
        setLastSnap(TSA::Grid::GridSnapResult());
        m_isCursorSnapped = false;
        m_gridRenderer.hideSnapMarker(m_context);
    }
    else if (rect().contains(mapFromGlobal(QCursor::pos())))
    {
        double x = 0.0, y = 0.0, z = 0.0;
        int nodeId = -1;
        getPointUnderCursor(m_lastMousePos, x, y, z, nodeId);
    }
    else
    {
        m_gridRenderer.hideSnapMarker(m_context);
    }
    m_snapMarkerDirty = false;
    m_view->Redraw();
}

void OccView::setGridSnapEnabled(bool enabled)
{
    m_snapToGrid = enabled;
    if (m_gridSnapManager)
    {
        m_gridSnapManager->setSnapEnabled(enabled);
    }
    refreshSnapAtCursor();   // ON/OFF instantané : marqueur recalculé sans attendre un mouvement
    emit gridSnapChanged(enabled);
}

bool OccView::isGridSnapEnabled() const
{
    return m_snapToGrid;
}

void OccView::setObjectSnapEnabled(bool enabled)
{
    m_snapToObject = enabled;
    refreshSnapAtCursor();
    emit objectSnapChanged(enabled);
}

bool OccView::isObjectSnapEnabled() const
{
    return m_snapToObject;
}

void OccView::setGridLabelsVisible(bool visible)
{
    m_gridLabelsVisible = visible;
    m_gridRenderer.setLabelsVisible(visible, m_context);
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

bool OccView::areGridLabelsVisible() const
{
    return m_gridLabelsVisible;
}

void OccView::setGridLevelsVisible(bool visible)
{
    m_gridRenderer.setLevelsVisible(visible, m_context);
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

bool OccView::areGridLevelsVisible() const
{
    return true;
}

bool OccView::isNodeVisibleByFilter(int nodeId) const
{
    if (!m_nodesVisible) return false;
    if (!m_model) return true;

    switch (m_nodeDisplayFilter)
    {
    case NodeDisplayFilter::All:
        return true;
    case NodeDisplayFilter::FreeOnly:
        return m_model->isNodeFree(nodeId);
    case NodeDisplayFilter::SupportedOnly:
        if (const auto* n = m_model->getNode(nodeId))
            return n->support().isSupported();
        return false;
    case NodeDisplayFilter::SelectedOnly:
        return m_selectionManager && m_selectionManager->selectedNodes().count(nodeId) > 0;
    }
    return true;
}

void OccView::updateNodeVisibilities()
{
    if (m_context.IsNull()) return;

    for (auto& [id, shape] : m_nodeShapes)
    {
        if (!shape.IsNull())
        {
            if (isNodeVisibleByFilter(id))
                m_context->Display(shape, false);
            else
                m_context->Erase(shape, false);
        }
    }
    for (auto& [id, lbl] : m_nodeLabels)
    {
        if (!lbl.IsNull())
        {
            if (isNodeVisibleByFilter(id) && m_nodeLabelsVisible)
                m_context->Display(lbl, false);
            else
                m_context->Erase(lbl, false);
        }
    }
    reapplyIsolationIfActive(); // le filtre de nœuds ne doit pas réafficher les nœuds hors plan
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::setNodesVisible(bool visible)
{
    m_nodesVisible = visible;
    updateNodeVisibilities();
    emit nodesVisibilityChanged(visible);
}

void OccView::setNodeLabelsVisible(bool visible)
{
    m_nodeLabelsVisible = visible;
    updateNodeVisibilities();
    emit nodeLabelsVisibilityChanged(visible);
}

void OccView::setNodeDisplayFilter(NodeDisplayFilter filter)
{
    if (m_nodeDisplayFilter == filter) return;
    m_nodeDisplayFilter = filter;
    updateNodeVisibilities();
    emit nodeDisplayFilterChanged(filter);
}

void OccView::setSurfaceLoadsVisible(bool visible)
{
    m_surfaceLoadsVisible = visible;
    updateSurfaceLoadShapes();
    if (!m_context.IsNull()) m_context->UpdateCurrentViewer();
}

void OccView::setLoadsVisible(bool visible)
{
    m_loadsVisible = visible;
    updateSurfaceLoadShapes();
    if (!m_context.IsNull())
    {
        for (auto& [id, shapes] : m_nodalLoadShapes)
        {
            for (auto& s : shapes)
            {
                if (!s.IsNull())
                {
                    if (visible) m_context->Display(s, false);
                    else m_context->Erase(s, false);
                }
            }
        }
        for (auto& [id, shapes] : m_memberLoadShapes)
        {
            for (auto& s : shapes)
            {
                if (!s.IsNull())
                {
                    if (visible) m_context->Display(s, false);
                    else m_context->Erase(s, false);
                }
            }
        }
        for (auto& [id, lbl] : m_nodalLoadLabels)
        {
            if (!lbl.IsNull())
            {
                if (visible && m_loadValuesVisible) m_context->Display(lbl, false);
                else m_context->Erase(lbl, false);
            }
        }
        for (auto& [id, lbl] : m_memberLoadLabels)
        {
            if (!lbl.IsNull())
            {
                if (visible && m_loadValuesVisible) m_context->Display(lbl, false);
                else m_context->Erase(lbl, false);
            }
        }
        reapplyIsolationIfActive();
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->Redraw();
        }
    }
    emit loadsVisibilityChanged(visible);
}

void OccView::setForcesVisible(bool visible)
{
    m_forcesVisible = visible;
    updateAllLoadShapes();
    emit forcesVisibilityChanged(visible);
}

void OccView::setMomentsVisible(bool visible)
{
    m_momentsVisible = visible;
    updateAllLoadShapes();
    emit momentsVisibilityChanged(visible);
}

void OccView::setLoadScale(double scale)
{
    if (scale < 0.05) scale = 0.05;
    if (scale > 20.0) scale = 20.0;
    m_loadScale = scale;
    updateAllLoadShapes();
    emit loadScaleChanged(scale);
}

void OccView::setLoadValuesVisible(bool visible)
{
    m_loadValuesVisible = visible;
    if (!m_context.IsNull())
    {
        for (auto& [id, lbl] : m_nodalLoadLabels)
        {
            if (!lbl.IsNull())
            {
                if (visible && m_loadsVisible) m_context->Display(lbl, false);
                else m_context->Erase(lbl, false);
            }
        }
        for (auto& [id, lbl] : m_memberLoadLabels)
        {
            if (!lbl.IsNull())
            {
                if (visible && m_loadsVisible) m_context->Display(lbl, false);
                else m_context->Erase(lbl, false);
            }
        }
        reapplyIsolationIfActive();
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->Redraw();
        }
    }
    emit loadValuesVisibilityChanged(visible);
}


void OccView::pickPoint3D(const std::function<void(const gp_Pnt& pt, int nodeId)>& onPicked,
                         const std::function<void()>& onCancelled,
                         QObject* owner)
{
    if (!m_interactionManager)
        return;

    TSA::Interaction::SelectionRequest req;
    req.mode = TSA::Interaction::SelectionMode::SelectPoint;
    req.targetField = tr("Sélection point 3D");
    req.snapEnabled = true;
    req.keepWindowOpen = true;
    req.sender = owner ? owner : this;
    req.onSelected = [onPicked](const TSA::Interaction::SelectedEntity& entity) {
        if (onPicked)
        {
            onPicked(entity.point, entity.entityId);
        }
    };
    req.onCancelled = [onCancelled]() {
        if (onCancelled)
        {
            onCancelled();
        }
    };
    m_interactionManager->requestSelection(req);
}

// =============================================================================
// Gestionnaires spécialisés découplés (Section 11)
// =============================================================================

void OccView::setProjectionMode(TSA::Viewer::ProjectionMode mode)
{
    m_projectionManager.setMode(mode);
    setMode2D(mode == TSA::Viewer::ProjectionMode::TwoD);
}

void OccView::setProjectionDirection(TSA::Viewer::ProjectionDirection dir)
{
    m_projectionManager.setDirection(dir);
    emit projectionDirectionChanged(dir);
}

void OccView::applyStandardView(TSA::Viewer::StandardCameraView view)
{
    showStandardView(view);
}

void OccView::setWorkPlaneAxesVisible(bool visible)
{
    if (m_workPlaneAxesVisible == visible)
        return;

    m_workPlaneAxesVisible = visible;
    updateWorkPlaneVisual();
    emit workPlaneAxesVisibleChanged(visible);
}

void OccView::setGizmoSize(double size)
{
    if (std::abs(m_gizmoSize - size) < 0.01)
        return;

    m_gizmoSize = size;
    updateWorkPlaneVisual();
    emit gizmoSizeChanged(size);
}

// -----------------------------------------------------------------------------
// État de vue persistant (aperçus de projet, réouverture)
// -----------------------------------------------------------------------------

QJsonObject OccView::cameraState() const
{
    if (m_view.IsNull() || m_view->Camera().IsNull()) return {};
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    const gp_Pnt center = cam->Center();
    const gp_Pnt eye = center.Translated(gp_Vec(cam->Direction()) * -std::max(cam->Distance(), std::max(cam->Scale(), 1.0)));   // voir captureNamedView
    const gp_Dir up = cam->Up();
    return QJsonObject{
        { "eye", QJsonArray{ eye.X(), eye.Y(), eye.Z() } },
        { "center", QJsonArray{ center.X(), center.Y(), center.Z() } },
        { "up", QJsonArray{ up.X(), up.Y(), up.Z() } },
        { "scale", cam->Scale() },
        { "projection", cam->ProjectionType() == Graphic3d_Camera::Projection_Orthographic ? "orthographic" : "perspective" } };
}

bool OccView::applyCameraState(const QJsonObject& state)
{
    if (m_view.IsNull() || m_view->Camera().IsNull()) return false;
    const QJsonArray eye = state["eye"].toArray(), center = state["center"].toArray(), up = state["up"].toArray();
    if (eye.size() != 3 || center.size() != 3 || up.size() != 3 || !(state["scale"].toDouble() > 0.0)) return false;
    const gp_Vec upVec(up[0].toDouble(), up[1].toDouble(), up[2].toDouble());
    const gp_Pnt e(eye[0].toDouble(), eye[1].toDouble(), eye[2].toDouble());
    const gp_Pnt c(center[0].toDouble(), center[1].toDouble(), center[2].toDouble());
    if (upVec.Magnitude() < 1e-9 || e.Distance(c) < 1e-9) return false;

    m_viewManager.setOrthographic(state["projection"].toString() == "orthographic", m_view);
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    cam->SetEye(e);
    cam->SetCenter(c);
    cam->SetUp(gp_Dir(upVec));
    cam->SetScale(state["scale"].toDouble());
    m_view->ZFitAll();
    m_view->Redraw();
    keep2DConsistent();
    emit viewCameraChanged();
    return true;
}

QJsonObject OccView::viewState() const
{
    return QJsonObject{
        { "mode2D", m_mode2DActive },
        { "isolation", isIsolationActive() },
        { "gridVisible", isGridVisible() },
        { "levelsVisible", areGridLevelsVisible() },
        { "nodesVisible", m_nodesVisible },
        { "loadsVisible", m_loadsVisible },
        { "supportsVisible", m_supportsVisible },
        { "workPlaneVisible", m_workPlaneVisible },
        { "hiddenElementCategories", static_cast<int>(m_hiddenElementCategories) } };
}
