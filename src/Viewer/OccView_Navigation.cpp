#include "OccView.h"
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
#include "../Coordinate/CoordinateTransformationService.h"
#include "../Coordinate/AxisColorConfig.h"
#include "ResultsVisualManager.h"
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
#include <Prs3d_DatumParts.hxx>
#include <Prs3d_DatumMode.hxx>
#include <TCollection_ExtendedString.hxx>
#include <Prs3d_LineAspect.hxx>
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

void OccView::fitModel()
{
    if (m_view.IsNull() || !m_model || m_model->nodes().empty())
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
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
        fitAll();
    }
}

void OccView::viewHome()
{
    resetView();
}

void OccView::viewTop()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 1.0, 0.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Top, false);
        fitAll();
    }
}

void OccView::viewBottom()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 1.0, 0.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Bottom, false);
        fitAll();
    }
}

void OccView::viewFront()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Front, false);
        fitAll();
    }
}

void OccView::viewBack()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Back, false);
        fitAll();
    }
}

void OccView::viewLeft()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Left, false);
        fitAll();
    }
}

void OccView::viewRight()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Right, false);
        fitAll();
    }
}

void OccView::viewIsometric()
{
    if (!m_view.IsNull())
    {
        pushCameraHistory();
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
        fitAll();
    }
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

    emit cameraHistoryChanged(hasPreviousView(), hasNextView());
    emit viewCameraChanged();
}

void OccView::setActiveWorkPlane(const TSA::Coordinate::WorkPlane& wp)
{
    bool needFullRebuild = (m_workPlaneShape.IsNull() ||
                            std::abs(m_workPlane.width() - wp.width()) > 1e-4 ||
                            std::abs(m_workPlane.height() - wp.height()) > 1e-4 ||
                            m_workPlane.isVisible() != wp.isVisible());

    m_workPlane = wp;
    if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXY ||
        wp.type() == TSA::Coordinate::WorkPlaneType::ElevationZ)
    {
        m_activeLevelZ = wp.offset();
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
    }

    emit workPlaneChanged(m_workPlane);
    if (m_projectionManager.is2D())
    {
        m_viewManager.viewNormalToWorkPlane(m_view, m_workPlane, false);
    }
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setWorkPlaneElevation(double elevation)
{
    m_workPlane.setOffset(elevation);
    m_activeLevelZ = elevation;
    TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(elevation, grid, m_context);
    applyWorkPlaneTransformation();
    emit workPlaneChanged(m_workPlane);
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::setWorkPlaneType(TSA::Coordinate::WorkPlaneType type, double offset)
{
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
    if (!m_view.IsNull())
        m_view->Redraw();
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
    if (m_view.IsNull())
        return;

    pushCameraHistory();

    gp_Pnt orig = m_workPlane.origin();
    gp_Dir norm = m_workPlane.normal();
    gp_Dir up = m_workPlane.yDirection();

    m_view->SetUp(up.X(), up.Y(), up.Z());
    m_view->SetProj(norm.X(), norm.Y(), norm.Z());
    m_view->SetAt(orig.X(), orig.Y(), orig.Z());
    m_view->FitAll();
    m_view->Redraw();

    emit viewCameraChanged();
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

    applyWorkPlaneTrihedronTransform(trsf);

    if (!m_manipulator.IsNull() && m_manipulator->IsAttached())
    {
        m_manipulator->SetPosition(m_workPlane.coordinateSystem().Ax2());
    }

    if (!m_viewer.IsNull())
    {
        m_viewer->SetPrivilegedPlane(m_workPlane.coordinateSystem());
    }

    if (m_workPlane.isIsolated())
    {
        updateElementIsolation();
    }

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::setWorkPlaneIsolation(bool isolated, double distance)
{
    m_workPlane.setIsIsolated(isolated);
    m_workPlane.setIsolationDistance(distance);
    updateElementIsolation();
    if (!m_view.IsNull())
        m_view->Redraw();
}

void OccView::updateElementIsolation()
{
    if (m_context.IsNull() || !m_model)
        return;

    bool isolate = m_workPlane.isIsolated();
    double maxDist = m_workPlane.isolationDistance();

    auto isNearPlane = [&](double x, double y, double z) {
        if (!isolate) return true;
        double d = std::abs(m_workPlane.distanceTo(gp_Pnt(x, y, z)));
        return d <= maxDist;
    };

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

    for (const auto& [nid, shape] : m_nodeShapes)
    {
        const auto* n = m_model->getNode(nid);
        if (n)
        {
            bool keep = isNearPlane(n->x(), n->y(), n->z());
            setShapeVisibility(shape, keep && m_nodesVisible);
            auto itLbl = m_nodeLabels.find(nid);
            if (itLbl != m_nodeLabels.end() && !itLbl->second.IsNull())
            {
                setShapeVisibility(itLbl->second, keep && m_nodesVisible && m_nodeLabelsVisible);
            }
        }
    }

    for (const auto& [bid, shape] : m_beamShapes)
    {
        const auto* b = m_model->getBeam(bid);
        if (b)
        {
            const auto* n1 = m_model->getNode(b->startNodeId());
            const auto* n2 = m_model->getNode(b->endNodeId());
            bool keep = !isolate || (n1 && n2 && (isNearPlane(n1->x(), n1->y(), n1->z()) || isNearPlane(n2->x(), n2->y(), n2->z())));
            setShapeVisibility(shape, keep);
        }
    }

    for (const auto& [cid, shape] : m_columnShapes)
    {
        const auto* col = m_model->getColumn(cid);
        if (col)
        {
            const auto* n1 = m_model->getNode(col->startNodeId());
            const auto* n2 = m_model->getNode(col->endNodeId());
            bool keep = !isolate || (n1 && n2 && (isNearPlane(n1->x(), n1->y(), n1->z()) || isNearPlane(n2->x(), n2->y(), n2->z())));
            setShapeVisibility(shape, keep);
        }
    }

    for (const auto& [sid, shape] : m_slabShapes)
    {
        const auto* slab = m_model->getSlab(sid);
        if (slab)
        {
            bool keep = !isolate;
            if (isolate)
            {
                for (int nid : slab->nodeIds())
                {
                    const auto* n = m_model->getNode(nid);
                    if (n && isNearPlane(n->x(), n->y(), n->z())) { keep = true; break; }
                }
            }
            setShapeVisibility(shape, keep);
        }
    }

    for (const auto& [wid, shape] : m_wallShapes)
    {
        const auto* wall = m_model->getWall(wid);
        if (wall)
        {
            bool keep = !isolate;
            if (isolate)
            {
                const auto* n1 = m_model->getNode(wall->startNodeId());
                const auto* n2 = m_model->getNode(wall->endNodeId());
                if ((n1 && isNearPlane(n1->x(), n1->y(), n1->z())) ||
                    (n2 && isNearPlane(n2->x(), n2->y(), n2->z())))
                {
                    keep = true;
                }
            }
            setShapeVisibility(shape, keep);
        }
    }

    for (const auto& [kid, shape] : m_cableShapes)
    {
        const auto* cab = m_model->getCable(kid);
        if (cab)
        {
            const auto* n1 = m_model->getNode(cab->startNodeId());
            const auto* n2 = m_model->getNode(cab->endNodeId());
            bool keep = !isolate || (n1 && n2 && (isNearPlane(n1->x(), n1->y(), n1->z()) || isNearPlane(n2->x(), n2->y(), n2->z())));
            setShapeVisibility(shape, keep);
        }
    }
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

    if (!m_workPlaneVisible || !m_workPlane.isVisible())
    {
        if (!m_view.IsNull())
            m_view->Redraw();
        return;
    }

    // 2. Détermination de la dimension du plan (largeur / hauteur paramétriques ou dynamique)
    double hx = (m_workPlane.width() > 0.0) ? (m_workPlane.width() * 0.5) : 10.0;
    double hy = (m_workPlane.height() > 0.0) ? (m_workPlane.height() * 0.5) : 10.0;
    if (m_workPlane.width() <= 0.0 || m_workPlane.height() <= 0.0)
    {
        if (m_model && !m_model->nodes().empty())
        {
            double minX = 1e9, maxX = -1e9;
            double minY = 1e9, maxY = -1e9;
            double minZ = 1e9, maxZ = -1e9;
            for (const auto& [nid, n] : m_model->nodes())
            {
                if (n.x() < minX) minX = n.x();
                if (n.x() > maxX) maxX = n.x();
                if (n.y() < minY) minY = n.y();
                if (n.y() > maxY) maxY = n.y();
                if (n.z() < minZ) minZ = n.z();
                if (n.z() > maxZ) maxZ = n.z();
            }
            double span = std::max({ maxX - minX, maxY - minY, maxZ - minZ });
            if (span > 5.0)
            {
                double L = std::max(10.0, span * 0.6);
                hx = L;
                hy = L;
            }
        }
    }

    // 3. Panneau surfacique semi-transparent centré en (0,0,0) local
    gp_Pnt p00(-hx, -hy, 0.0);
    gp_Pnt p10( hx, -hy, 0.0);
    gp_Pnt p11( hx,  hy, 0.0);
    gp_Pnt p01(-hx,  hy, 0.0);

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

    if (m_workPlane.isIsolated())
    {
        updateElementIsolation();
    }

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
    if (!m_view.IsNull())
    {
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(orientation, false);
        m_view->FitAll();
        m_view->Redraw();
        emit viewCameraChanged();
    }
}

void OccView::setViewPlaneMode(ViewPlaneMode mode)
{
    m_viewPlaneMode = mode;
    if (m_view.IsNull())
        return;

    m_view->SetUp(0.0, 0.0, 1.0);

    switch (mode)
    {
    case ViewPlaneMode::PlanXY:
        // Vue en plan horizontal d'étage (Zup_Top)
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Top, false);
        break;
    case ViewPlaneMode::PlanXZ:
        // Élévation de face / portique (Zup_Front)
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Front, false);
        break;
    case ViewPlaneMode::PlanYZ:
        // Élévation latérale / pignon (Zup_Right)
        m_view->SetProj(V3d_TypeOfOrientation_Zup_Right, false);
        break;
    case ViewPlaneMode::Perspective3D:
    default:
        // Vue 3D axonométrique globale
        m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
        break;
    }

    m_view->FitAll();
    m_view->Redraw();
    emit viewPlaneModeChanged(mode);
    emit viewCameraChanged();
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

    if (!m_view.IsNull())
    {
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

    if (moved)
    {
        TSA::Coordinate::CoordinateTransformationService::instance().setActiveWorkPlane(m_workPlane);
        applyWorkPlaneTransformation(); // simple transformation locale des objets AIS du plan, sans reconstruction
        emit workPlaneChanged(m_workPlane);
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

void OccView::setGridSnapEnabled(bool enabled)
{
    m_snapToGrid = enabled;
    if (m_gridSnapManager)
    {
        m_gridSnapManager->setSnapEnabled(enabled);
    }
    if (!enabled && !m_context.IsNull())
    {
        m_gridRenderer.hideSnapMarker(m_context);
        if (!m_view.IsNull())
        {
            m_view->Redraw();
        }
    }
    emit gridSnapChanged(enabled);
}

bool OccView::isGridSnapEnabled() const
{
    return m_snapToGrid;
}

void OccView::setObjectSnapEnabled(bool enabled)
{
    m_snapToObject = enabled;
    if (!enabled && !m_snapToGrid && !m_context.IsNull())
    {
        m_gridRenderer.hideSnapMarker(m_context);
        if (!m_view.IsNull())
        {
            m_view->Redraw();
        }
    }
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

void OccView::setLoadsVisible(bool visible)
{
    m_loadsVisible = visible;
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
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->Redraw();
        }
    }
    emit loadValuesVisibilityChanged(visible);
}


void OccView::pickPoint3D(const std::function<void(const gp_Pnt& pt, int nodeId)>& onPicked,
                         const std::function<void()>& onCancelled)
{
    if (!m_interactionManager)
        return;

    TSA::Interaction::SelectionRequest req;
    req.mode = TSA::Interaction::SelectionMode::SelectPoint;
    req.targetField = tr("Sélection point 3D");
    req.snapEnabled = true;
    req.keepWindowOpen = true;
    req.sender = this;
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
    if (mode == TSA::Viewer::ProjectionMode::TwoD)
    {
        m_viewManager.setOrthographic(true, m_view);
        m_viewManager.viewNormalToWorkPlane(m_view, m_workPlane, false);
    }
    emit projectionModeChanged(mode);
}

void OccView::setProjectionDirection(TSA::Viewer::ProjectionDirection dir)
{
    m_projectionManager.setDirection(dir);
    emit projectionDirectionChanged(dir);
}

void OccView::applyStandardView(TSA::Viewer::StandardCameraView view)
{
    m_viewManager.applyStandardView(view, m_view, m_workPlane);
    emit standardViewChanged(view);
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
