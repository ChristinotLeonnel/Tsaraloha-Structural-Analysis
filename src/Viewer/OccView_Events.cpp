#include "OccView.h"
#include "App/ProductInfo.h"
#include "SelectionManager.h"
#include "../Model/Model.h"
#include "../Model/ModelDiff.h"
#include "../Geometry/BeamGeometry.h"
#include "../Geometry/SlabGeometry.h"
#include "../Geometry/WallGeometry.h"
#include "../Geometry/FoundationGeometry.h"
#include "../Model/Wall.h"
#include "../Model/Foundation.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Geometry/CableGeometry3D.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "../UI/Theme/ThemeManager.h"
#include "../Coordinate/CoordinateTransformationService.h"
#include "../Commands/ModifyCommands.h"
#include "../Coordinate/GeometryTolerance.h"
#include <gp_Trsf.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax1.hxx>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QColor>
#include <Graphic3d_Camera.hxx>
#include <SelectMgr_ViewerSelector.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <Bnd_Box.hxx>
#include <cmath>
#include <algorithm>

void OccView::mousePressEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();
    emit mousePixelPositionChanged(event->position().toPoint().x(), event->position().toPoint().y());
    m_lastMousePos = p;
    m_pressMousePos = p;
    m_dragStartPos = p;

    if (event->button() == Qt::LeftButton)
    {
        // 0. Clic interactif prioritaire sur le Gizmo AIS_Manipulator du Plan de Travail
        if (!m_manipulator.IsNull() && m_manipulator->IsAttached() && m_manipulator->HasActiveMode() && !m_workPlaneShape.IsNull() && !m_view.IsNull())
        {
            m_isManipulatingWorkPlane = true;
            m_manipulatorStartWp = m_workPlane;
            m_manipulator->StartTransform(px, py, m_view);
            return;
        }

        // 1. Clic prioritaire sur le ViewCube 3D (réorientation de caméra)
        if (!m_context.IsNull() && !m_viewCube.IsNull())
        {
            m_context->MoveTo(px, py, m_view, false);
            if (m_context->HasDetected())
            {
                Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
                if (detectedObj == m_viewCube)
                {
                    Handle(AIS_ViewCubeOwner) cubeOwner = Handle(AIS_ViewCubeOwner)::DownCast(m_context->DetectedOwner());
                    if (!cubeOwner.IsNull())
                    {
                        // Service de navigation : face = vue orthographique exacte (plan 2D suivi en mode 2D),
                        // arête / coin = orientation 3D déterministe ; caméra finale exacte après la transition.
                        // Clic entièrement consommé ici : sans cela, le relâchement passait à la sélection, qui
                        // relançait l'animation interne du cube (HandleMouseClick) sur la partie du cube alors sous le
                        // curseur — après une arête ou un coin, une face voisine : orientation finale imprévisible.
                        m_viewCubeClickPending = true;
                        showCubeOrientation(cubeOwner->MainOrientation());
                        return;
                    }
                }
            }
        }

        // Interception du mode Zoom Fenêtre interactif
        if (m_currentAction == CurrentAction::ZoomWindow)
        {
            m_dragStartPos = p;
            m_pressMousePos = p;
            m_lastMousePos = p;
            return;
        }

        // 2. Interception prioritaire : Requête de sélection 3D non-bloquante pour formulaire / dialogue
        if (m_interactionManager && m_interactionManager->hasActiveSelectionRequest())
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId))
            {
                const auto& req = *m_interactionManager->activeSelectionRequest();
                TSA::Interaction::SelectedEntity entity;
                entity.mode = req.mode;
                entity.point = gp_Pnt(wx, wy, wz);
                entity.entityId = detectedId;
                entity.targetField = req.targetField;
                if (detectedId > 0)
                {
                    entity.description = tr("Nœud N%1").arg(detectedId);
                }
                else
                {
                    entity.description = tr("Point (%1, %2, %3)").arg(wx, 0, 'f', 3).arg(wy, 0, 'f', 3).arg(wz, 0, 'f', 3);
                }

                m_gridRenderer.hideSnapMarker(m_context);
                m_interactionManager->completeSelection(entity);
                if (!m_view.IsNull())
                {
                    m_view->Redraw();
                }
                return;
            }
        }

        if (interactionMode() == InteractionMode::ModelingTool)
        {
            handleModelingToolClick(p, event->modifiers());
            return;
        }

        if (interactionMode() == InteractionMode::Select)
        {
            // En mode sélection, on attend le mouvement pour distinguer un clic d'un glissé fenêtre/capture
            m_currentAction = CurrentAction::Nothing;
        }
        else if (interactionMode() == InteractionMode::DrawNode)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto r = TSA::Interaction::ElementCreation(*m_model).createNode(drawPointAt(wx, wy, wz, detectedId));
                reportCreation(r, tr("Nœud N%1 créé en (X = %2 m, Y = %3 m, Z = %4 m)")
                    .arg(r.elementId)
                    .arg(wx, 0, 'f', 3)
                    .arg(wy, 0, 'f', 3)
                    .arg(wz, 0, 'f', 3));
            }
        }
        else if (interactionMode() == InteractionMode::DrawBar)
        {
            // Le dialogue « Barre » crée l'élément (ElementCreation) à la réception du 2nd point.
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit barFirstPointPicked(dp.point, dp.nodeId);
                    emit drawingPromptChanged(tr("Barre : 1er point %1 fixé en (%2; %3; %4). Cliquez pour le 2nd point")
                        .arg(pointLabel(dp)).arg(dp.point.X(), 0, 'f', 2).arg(dp.point.Y(), 0, 'f', 2).arg(dp.point.Z(), 0, 'f', 2));
                }
                else
                {
                    clearDrawing();
                    emit barSecondPointPicked(dp.point, dp.nodeId);
                    emit elementCreated();
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawBeam || interactionMode() == InteractionMode::DrawColumn)
        {
            const bool column = interactionMode() == InteractionMode::DrawColumn;
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit barFirstPointPicked(dp.point, dp.nodeId);
                    emit drawingPromptChanged(column
                        ? tr("Mode Dessin Poteau : base %1 définie. Cliquez pour le sommet (Échap pour annuler)").arg(pointLabel(dp))
                        : tr("Mode Dessin Poutre : 1er point %1. Cliquez pour le 2nd point (Échap pour annuler)").arg(pointLabel(dp)));
                }
                else
                {
                    const auto props = drawingBarProperties(column ? TSA::Model::BarRole::Column : TSA::Model::BarRole::Beam);
                    const auto r = TSA::Interaction::ElementCreation(*m_model).createBar(drawingPicks().front(), dp, props);
                    if (r.success)
                    {
                        if (column) updateColumnShape(r.elementId);
                        else updateBeamShape(r.elementId);
                        reportCreation(r, column
                            ? tr("Poteau C%1 créé reliant N%2 à N%3 (%4). Cliquez pour un autre poteau")
                                  .arg(r.elementId).arg(r.nodeIds[0]).arg(r.nodeIds[1]).arg(QString::fromStdString(props.section.name))
                            : tr("Poutre B%1 créée reliant N%2 à N%3 (%4). Cliquez pour continuer")
                                  .arg(r.elementId).arg(r.nodeIds[0]).arg(r.nodeIds[1]).arg(QString::fromStdString(props.section.name)));
                        finishBarSegment(r.nodeIds[1], true);
                    }
                    else
                    {
                        // Refus (longueur nulle, doublon…) : le 1er point reste fixé, rien n'a été créé.
                        reportCreation(r, {});
                    }
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawSlab)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                const bool closesContour = m_drawingPoints.size() >= 3
                    && ((dp.nodeId > 0 && dp.nodeId == m_drawingNodeIds.front())
                        || dp.point.Distance(m_drawingPoints.front()) <= TSA::Coordinate::GeometryTolerance::nodeCoincidence);
                if (closesContour)
                {
                    finishCurrentSlab();
                }
                else
                {
                    pushDrawPoint(dp);
                    emit slabNodePicked(dp.nodeId, dp.point, static_cast<int>(m_drawingPoints.size()));
                    emit drawingPromptChanged(tr("Dalle : point %1 ajouté (total : %2). Cliquez pour ajouter, ou fermez sur le 1er point / Clic droit")
                        .arg(pointLabel(dp))
                        .arg(m_drawingPoints.size()));
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawWall)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit wallFirstPointPicked(dp.point, dp.nodeId);
                    emit drawingPromptChanged(tr("Mode Voile : 1er point %1. Cliquez pour le 2nd point (H=%2m, ép=%3m)").arg(pointLabel(dp)).arg(m_presets.wall.height).arg(m_presets.wall.thickness));
                }
                else
                {
                    const auto r = TSA::Interaction::ElementCreation(*m_model).createWall(drawingPicks().front(), dp, resolvedPresets().wall);
                    if (r.success)
                    {
                        emit wallSecondPointPicked(dp.point, r.nodeIds[1]);
                        updateWallShape(r.elementId);
                        reportCreation(r, tr("Voile W%1 créé reliant N%2 à N%3 (H=%4m, ép=%5m). Cliquez pour continuer").arg(r.elementId).arg(r.nodeIds[0]).arg(r.nodeIds[1]).arg(m_presets.wall.height).arg(m_presets.wall.thickness));
                        emit wallCreated(r.elementId);
                        // Enchaînement continu du tracé de voile
                        clearDrawing();
                        pushDrawPoint(TSA::Interaction::DrawPoint(dp.point, r.nodeIds[1]));
                    }
                    else
                    {
                        reportCreation(r, {});
                    }
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawFoundation)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto& fp = m_presets.foundation;
                const auto r = TSA::Interaction::ElementCreation(*m_model).createFoundation(drawPointAt(wx, wy, wz, detectedId), fp);
                reportCreation(r, tr("Semelle F%1 créée sous le nœud N%2 (%3 x %4 x %5 m)")
                    .arg(r.elementId).arg(r.nodeIds.empty() ? -1 : r.nodeIds[0])
                    .arg(fp.widthA, 0, 'f', 2).arg(fp.lengthB, 0, 'f', 2).arg(fp.heightH, 0, 'f', 2));
            }
        }
        else if (interactionMode() == InteractionMode::DrawTruss)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit drawingPromptChanged(tr("Mode Treillis : 1er point %1. Cliquez pour le 2nd point (%2)").arg(pointLabel(dp)).arg(QString::fromStdString(m_presets.truss.section.name)));
                }
                else
                {
                    const auto r = TSA::Interaction::ElementCreation(*m_model).createTrussMember(drawingPicks().front(), dp, m_presets.truss);
                    reportCreation(r, tr("Barre TR%1 créée reliant N%2 à N%3 (%4)").arg(r.elementId)
                        .arg(r.nodeIds.empty() ? -1 : r.nodeIds[0]).arg(r.nodeIds.size() < 2 ? -1 : r.nodeIds[1])
                        .arg(QString::fromStdString(m_presets.truss.section.name)));
                    if (r.success) clearDrawing();
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawCable)
        {
            // Le dialogue « Câble » crée l'élément (ElementCreation) à la réception du 2nd point.
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit cableFirstPointPicked(dp.point, dp.nodeId);
                    emit drawingPromptChanged(tr("Mode Dessin Câble : 1er point %1. Cliquez pour le 2nd point (Échap pour annuler)").arg(pointLabel(dp)));
                }
                else
                {
                    clearDrawing();
                    emit cableSecondPointPicked(dp.point, dp.nodeId);
                    emit elementCreated();
                }
            }
        }
        else if (interactionMode() == InteractionMode::DrawStayCable ||
                 interactionMode() == InteractionMode::DrawSuspensionCable ||
                 interactionMode() == InteractionMode::DrawHanger)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                const auto dp = drawPointAt(wx, wy, wz, detectedId);
                const QString typeStr = (interactionMode() == InteractionMode::DrawStayCable) ? tr("Hauban") :
                                        (interactionMode() == InteractionMode::DrawSuspensionCable) ? tr("Câble Porteur") : tr("Suspente");
                if (m_drawingPoints.empty())
                {
                    pushDrawPoint(dp);
                    emit drawingPromptChanged(tr("Mode Dessin %1 : 1er point %2. Cliquez pour le 2nd point").arg(typeStr).arg(pointLabel(dp)));
                }
                else
                {
                    TSA::Model::CableType cType = TSA::Model::CableType::Hanger;
                    if (interactionMode() == InteractionMode::DrawStayCable) cType = TSA::Model::CableType::StayCable;
                    else if (interactionMode() == InteractionMode::DrawSuspensionCable) cType = TSA::Model::CableType::SuspensionCable;

                    const auto r = TSA::Interaction::ElementCreation(*m_model).createCable(drawingPicks().front(), dp, cType);
                    if (r.success) updateCableShape(r.elementId);
                    reportCreation(r, tr("%1 C%2 créé reliant N%3 à N%4. Cliquez pour continuer").arg(typeStr).arg(r.elementId)
                        .arg(r.nodeIds.empty() ? -1 : r.nodeIds[0]).arg(r.nodeIds.size() < 2 ? -1 : r.nodeIds[1]));
                    if (r.success) clearDrawing();
                }
            }
        }
        else if (interactionMode() == InteractionMode::MoveOrigin3D)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId))
            {
                gp_Pnt newOrigin(wx, wy, wz);
                emit originMoveRequested(newOrigin);
                emit drawingPromptChanged(tr("Origine 3D déplacée en (X = %1 m, Y = %2 m, Z = %3 m)")
                    .arg(wx, 0, 'f', 3).arg(wy, 0, 'f', 3).arg(wz, 0, 'f', 3));
                setInteractionMode(InteractionMode::Select);
            }
        }
        else if (interactionMode() == InteractionMode::Paste3D)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId))
            {
                gp_Pnt target(wx, wy, wz);
                emit pasteAtPointRequested(target);
                emit drawingPromptChanged(tr("Éléments collés à l'emplacement ! Vous pouvez cliquer pour coller à nouveau ou Échap"));
            }
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        pushCameraHistory();
        if (m_mode2DActive)
        {
            m_currentAction = CurrentAction::Pan;
        }
        else
        {
            m_currentAction = CurrentAction::Rotation;
            m_view->StartRotation(px, py);
        }
    }
    else if (event->button() == Qt::MiddleButton)
    {
        pushCameraHistory();
        m_currentAction = CurrentAction::Pan;
    }
}

void OccView::mouseReleaseEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();

    if (m_currentAction == CurrentAction::ZoomWindow)
    {
        if (!m_selectRubberBand.IsNull() && !m_context.IsNull() && m_context->IsDisplayed(m_selectRubberBand))
        {
            m_context->Erase(m_selectRubberBand, false);
        }
        zoomWindow(m_dragStartPos.x(), m_dragStartPos.y(), px, py);
        m_currentAction = CurrentAction::Nothing;
        setCursor(interactionMode() == InteractionMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && m_viewCubeClickPending)
    {
        m_viewCubeClickPending = false;   // relâchement d'un clic sur le ViewCube : déjà traité à l'appui
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        // 0. Fin de manipulation interactive du Plan de Travail via Gizmo
        if (m_isManipulatingWorkPlane)
        {
            m_isManipulatingWorkPlane = false;
            if (!m_manipulator.IsNull() && m_manipulator->IsAttached() && m_manipulator->HasActiveMode() && !m_view.IsNull())
            {
                m_manipulator->StopTransform(true);
            }

            m_workPlane = withCurrentPositionName(m_workPlane, m_manipulatorStartWp);
            // Les plans de travail ne sont pas dans l'historique du modèle (snapshot) : leur historique est celui de
            // la vue (rememberWorkPlane). Une entrée Annuler ici serait vide et déclarerait les résultats périmés.
            if (m_model && m_model->workPlaneManager())
            {
                m_model->workPlaneManager()->updateWorkPlane(m_workPlane);
            }
            rememberWorkPlane(m_manipulatorStartWp, m_workPlane);

            emit workPlaneChanged(m_workPlane);
            if (!m_view.IsNull())
                m_view->Redraw();
            return;
        }

        if (interactionMode() == InteractionMode::Select)
        {
            if (m_currentAction == CurrentAction::WindowSelect)
            {
                // Masquer le rectangle de sélection rubberband
                if (!m_selectRubberBand.IsNull() && !m_context.IsNull() && m_context->IsDisplayed(m_selectRubberBand))
                {
                    m_context->Erase(m_selectRubberBand, false);
                }

                int minX = std::min(m_dragStartPos.x(), px);
                int maxX = std::max(m_dragStartPos.x(), px);
                int minY = std::min(m_dragStartPos.y(), py);
                int maxY = std::max(m_dragStartPos.y(), py);

                bool isCrossing = (px < m_dragStartPos.x()); // De droite à gauche
                bool multi = (event->modifiers() & Qt::ControlModifier);

                if (!m_context.IsNull() && !m_view.IsNull())
                {
                    auto selector = m_context->MainSelector();
                    if (!selector.IsNull())
                    {
                        selector->AllowOverlapDetection(isCrossing);
                    }

                    NCollection_Vec2<int> pMin(minX, minY);
                    NCollection_Vec2<int> pMax(maxX, maxY);

                    m_context->SelectRectangle(pMin, pMax, m_view, multi ? AIS_SelectionScheme_XOR : AIS_SelectionScheme_Replace);

                    std::vector<Handle(AIS_InteractiveObject)> selectedObjs;
                    for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected())
                    {
                        selectedObjs.push_back(m_context->SelectedInteractive());
                    }

                    // Le contexte contient déjà le résultat (remplacement, ou bascule XOR avec Ctrl) : la
                    // sélection du modèle reprend exactement cet ensemble.
                    if (m_selectionManager)
                    {
                        m_selectionManager->setMultipleObjectsSelected(selectedObjs, false);
                    }

                    m_context->UpdateCurrentViewer();
                    m_view->Redraw();
                }

                m_currentAction = CurrentAction::Nothing;
                emit objectHovered(QString());
                setCursor(interactionMode() == InteractionMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
                return;
            }
            else
            {
                // Simple clic ponctuel gauche
                if (!m_context.IsNull() && !m_view.IsNull())
                {
                    bool multi = (event->modifiers() & Qt::ControlModifier);
                    m_context->MoveTo(px, py, m_view, false);

                    if (m_context->HasDetected() && overlayConsumesClick())
                    {
                        // Entité d'un calque (maillage) : traitée par le calque ; la sélection du modèle est vidée
                        // pour que propriétés et surbrillance ne désignent qu'une seule chose.
                        if (!multi)
                        {
                            m_context->ClearSelected(false);
                            if (m_selectionManager) m_selectionManager->clearSelection();
                        }
                    }
                    else if (m_context->HasDetected())
                    {
                        Handle(AIS_InteractiveObject) obj = detectedPreferringModel();
                        const bool preferred = obj != m_context->DetectedInteractive();
                        if (m_mode2DActive && (obj == m_workPlaneShape || obj == m_workPlaneTrihedron || obj == m_workPlaneOriginShape))
                        {
                            if (!multi)
                            {
                                m_context->ClearSelected(false);
                                if (m_selectionManager)
                                {
                                    m_selectionManager->clearSelection();
                                }
                            }
                        }
                        else if (multi)
                        {
                            // Ctrl + clic : bascule de l'élément dans la sélection (ajout ou retrait), les
                            // autres sont conservés. La surbrillance suit le gestionnaire (une passe sur
                            // tout l'ensemble), jamais l'inverse : affichage et sélection restent identiques.
                            if (m_selectionManager) m_selectionManager->toggleObject(obj);
                        }
                        else
                        {
                            if (preferred) m_context->SetSelected(obj, false);
                            else m_context->SelectDetected(AIS_SelectionScheme_Replace);
                            if (m_selectionManager)
                            {
                                m_selectionManager->selectObject(obj, false);
                            }
                        }
                    }
                    else
                    {
                        if (!multi)
                        {
                            m_context->ClearSelected(false);
                            if (m_selectionManager)
                            {
                                m_selectionManager->clearSelection();
                            }
                        }
                    }

                    m_context->UpdateCurrentViewer();
                    m_view->Redraw();
                }
            }
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        // Si le bouton droit a été relâché sans déplacement significatif (simple clic droit)
        int distSq = (p.x() - m_pressMousePos.x()) * (p.x() - m_pressMousePos.x()) +
                     (p.y() - m_pressMousePos.y()) * (p.y() - m_pressMousePos.y());
        if (distSq <= 16 && interactionMode() != InteractionMode::Select)
        {
            if (interactionMode() == InteractionMode::DrawSlab && m_drawingNodeIds.size() >= 3 && m_model)
            {
                finishCurrentSlab();
            }
            else
            {
                if (!m_drawingPoints.empty())
                {
                    cancelCurrentDrawing();
                }
                else
                {
                    setInteractionMode(InteractionMode::Select);
                }
            }
        }
    }

    if (m_currentAction == CurrentAction::Pan || m_currentAction == CurrentAction::Rotation)
    {
        emit viewCameraChanged();
    }

    m_currentAction = CurrentAction::Nothing;
    setCursor(interactionMode() == InteractionMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void OccView::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    emit mousePixelPositionChanged(event->position().toPoint().x(), event->position().toPoint().y());
}

void OccView::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    emit mousePixelPositionChanged(-1, -1);
}

void OccView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();

    // Émettre les coordonnées logiques exactes pour le suivi parfait du curseur par le triangle des règles
    emit mousePixelPositionChanged(event->position().toPoint().x(), event->position().toPoint().y());

    // 0. Déplacement interactif du Plan de Travail via le Gizmo AIS_Manipulator
    if (m_isManipulatingWorkPlane)
    {
        if (!m_manipulator.IsNull() && m_manipulator->IsAttached() && m_manipulator->HasActiveMode() && !m_workPlaneShape.IsNull() && !m_view.IsNull())
        {
            m_manipulator->Transform(px, py, m_view);
            // Plan, grille, trièdre, niveau actif, accrochage : synchronisés à chaque mouvement
            syncWorkPlaneFromManipulator();
            return;
        }
        else
        {
            m_isManipulatingWorkPlane = false;
        }
    }

    // Mode Zoom Fenêtre interactif
    if (m_currentAction == CurrentAction::ZoomWindow && (event->buttons() & Qt::LeftButton))
    {
        int minX = std::min(m_dragStartPos.x(), px);
        int maxX = std::max(m_dragStartPos.x(), px);
        int minY = std::min(m_dragStartPos.y(), py);
        int maxY = std::max(m_dragStartPos.y(), py);

        if (m_selectRubberBand.IsNull())
        {
            m_selectRubberBand = new AIS_RubberBand();
        }

        m_selectRubberBand->SetLineColor(Quantity_NOC_ORANGE);
        m_selectRubberBand->SetLineType(Aspect_TOL_DASH);
        m_selectRubberBand->SetLineWidth(1.8);
        m_selectRubberBand->SetFilling(Quantity_NOC_GOLD, 0.85);

        int winH = 0;
        if (!m_view.IsNull() && !m_view->Window().IsNull())
        {
            int winW = 0;
            m_view->Window()->Size(winW, winH);
        }
        int rbMinY = winH - maxY;
        int rbMaxY = winH - minY;

        m_selectRubberBand->SetRectangle(minX, rbMinY, maxX, rbMaxY);

        if (!m_context.IsNull())
        {
            if (!m_context->IsDisplayed(m_selectRubberBand))
                m_context->Display(m_selectRubberBand, false);
            else
                m_context->Redisplay(m_selectRubberBand, false);
        }
        if (!m_view.IsNull())
            m_view->Redraw();
        return;
    }

    // Mode Sélection rectangulaire (Fenêtre gauche->droite ou Capture droite->gauche)
    if (interactionMode() == InteractionMode::Select && (event->buttons() & Qt::LeftButton))
    {
        int dx = px - m_dragStartPos.x();
        int dy = py - m_dragStartPos.y();
        if (m_currentAction == CurrentAction::WindowSelect || (dx * dx + dy * dy >= 16))
        {
            m_currentAction = CurrentAction::WindowSelect;

            int minX = std::min(m_dragStartPos.x(), px);
            int maxX = std::max(m_dragStartPos.x(), px);
            int minY = std::min(m_dragStartPos.y(), py);
            int maxY = std::max(m_dragStartPos.y(), py);

            bool isCrossing = (px < m_dragStartPos.x()); // De droite à gauche

            if (m_selectRubberBand.IsNull())
            {
                m_selectRubberBand = new AIS_RubberBand();
            }

            if (isCrossing)
            {
                // Capture (Droite vers Gauche) : Vert, contour tireté, remplissage semi-transparent
                m_selectRubberBand->SetLineColor(Quantity_NOC_LIMEGREEN);
                m_selectRubberBand->SetLineType(Aspect_TOL_DASH);
                m_selectRubberBand->SetLineWidth(1.5);
                m_selectRubberBand->SetFilling(Quantity_NOC_GREEN1, 0.75);
                emit objectHovered(tr("Capture (Droite -> Gauche) : Sélectionne tout élément touché ou inclus"));
            }
            else
            {
                // Fenêtre (Gauche vers Droite) : Bleu, contour plein, remplissage semi-transparent
                m_selectRubberBand->SetLineColor(Quantity_NOC_DEEPSKYBLUE1);
                m_selectRubberBand->SetLineType(Aspect_TOL_SOLID);
                m_selectRubberBand->SetLineWidth(1.5);
                m_selectRubberBand->SetFilling(Quantity_NOC_DEEPSKYBLUE1, 0.75);
                emit objectHovered(tr("Fenêtre (Gauche -> Droite) : Sélectionne uniquement les éléments entièrement inclus"));
            }

            // Inverser l'axe Y pour AIS_RubberBand (convention OpenGL : Y=0 en bas)
            // Qt fournit des coordonnées écran (Y=0 en haut)
            int winH = 0;
            if (!m_view.IsNull() && !m_view->Window().IsNull())
            {
                int winW = 0;
                m_view->Window()->Size(winW, winH);
            }
            int rbMinY = winH - maxY;
            int rbMaxY = winH - minY;

            m_selectRubberBand->SetRectangle(minX, rbMinY, maxX, rbMaxY);

            if (!m_context.IsNull())
            {
                if (!m_context->IsDisplayed(m_selectRubberBand))
                {
                    m_context->Display(m_selectRubberBand, false);
                }
                else
                {
                    m_context->Redisplay(m_selectRubberBand, false);
                }
            }

            if (!m_view.IsNull())
            {
                m_view->Redraw();
            }
            return;
        }
    }

    switch (m_currentAction)
    {
    case CurrentAction::Rotation:
        if (m_mode2DActive)
        {
            m_view->Pan(px - m_lastMousePos.x(),
                        m_lastMousePos.y() - py);
            m_lastMousePos = p;
        }
        else
        {
            m_view->Rotation(px, py);
        }
        emit viewCameraChanged();
        break;

    case CurrentAction::Pan:
        m_view->Pan(px - m_lastMousePos.x(),
                    m_lastMousePos.y() - py);
        m_lastMousePos = p;
        emit viewCameraChanged();
        break;

    case CurrentAction::Nothing:
    default:
        if (!m_context.IsNull() && !m_view.IsNull())
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedNodeId = -1;
            getPointUnderCursor(p, wx, wy, wz, detectedNodeId);

            // Mise à jour de la prévisualisation élastique (Rubberband) si un tracé est en cours
            if (!m_drawingPoints.empty())
            {
                updateRubberBand(gp_Pnt(wx, wy, wz));
            }
            if (interactionMode() == InteractionMode::ModelingTool)
            {
                updateModelingToolPreview(gp_Pnt(wx, wy, wz));
            }

            // Détection survol d'objets pour affichage des propriétés et infobulles
            AIS_StatusOfDetection status = m_context->MoveTo(px, py, m_view, true);
            bool objectDetected = (status != AIS_SOD_Nothing && m_context->HasDetected());
            // MoveTo ne redessine que la couche immédiate (surbrillance) : le marqueur d'accrochage,
            // dans la couche principale, n'était pas rafraîchi au survol d'un objet.
            if (m_snapMarkerDirty)
            {
                m_snapMarkerDirty = false;
                m_view->Redraw();
            }

            if (m_interactionManager && m_interactionManager->hasActiveSelectionRequest())
            {
                setCursor(Qt::CrossCursor);
            }
            else if (objectDetected)
            {
                setCursor(Qt::PointingHandCursor);

                Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
                int beamId = m_selectionManager ? m_selectionManager->getBeamId(detectedObj) : -1;
                int colId = m_selectionManager ? m_selectionManager->getColumnId(detectedObj) : -1;
                int slabId = m_selectionManager ? m_selectionManager->getSlabId(detectedObj) : -1;
                int nodeId = m_selectionManager ? m_selectionManager->getNodeId(detectedObj) : -1;
                int cableId = m_selectionManager ? m_selectionManager->getCableId(detectedObj) : -1;
                if (const QString text = overlayHoverText(); !text.isEmpty()) emit objectHovered(text);

                if (nodeId > 0 && m_model)
                {
                    const auto* node = m_model->getNode(nodeId);
                    if (node)
                    {
                        emit mouseCoordinatesChanged(node->x(), node->y(), node->z());
                        double uwp = 0.0, vwp = 0.0, wwp = 0.0;
                        m_workPlane.toLocal(gp_Pnt(node->x(), node->y(), node->z()), uwp, vwp, wwp);
                        emit mouseLocalCoordinatesChanged(uwp, vwp);
                        emit objectHovered(tr("Survol : Nœud %1 (X = %2 m, Y = %3 m, Z = %4 m)")
                            .arg(nodeId)
                            .arg(node->x(), 0, 'f', 3)
                            .arg(node->y(), 0, 'f', 3)
                            .arg(node->z(), 0, 'f', 3));
                    }
                }
                else if (beamId > 0 && m_model)
                {
                    const auto* beam = m_model->getBeam(beamId);
                    if (beam)
                    {
                        const auto* nA = m_model->getNode(beam->startNodeId());
                        const auto* nB = m_model->getNode(beam->endNodeId());
                        double length = 0.0;
                        if (nA && nB)
                        {
                            double dx = nB->x() - nA->x();
                            double dy = nB->y() - nA->y();
                            double dz = nB->z() - nA->z();
                            length = std::sqrt(dx * dx + dy * dy + dz * dz);
                        }

                        emit objectHovered(tr("Survol : Poutre %1 (Nœuds %2 -> %3 | Longueur = %4 m | Section %5x%6 m)")
                            .arg(beamId)
                            .arg(beam->startNodeId())
                            .arg(beam->endNodeId())
                            .arg(length, 0, 'f', 3)
                            .arg(beam->width(), 0, 'f', 2)
                            .arg(beam->height(), 0, 'f', 2));
                    }
                }
                else if (colId > 0 && m_model)
                {
                    const auto* col = m_model->getColumn(colId);
                    if (col)
                    {
                        emit objectHovered(tr("Survol : Poteau %1 (Nœuds %2 -> %3 | Hauteur = %4 m | Section %5x%6 m)")
                            .arg(colId)
                            .arg(col->startNodeId())
                            .arg(col->endNodeId())
                            .arg(col->length(*m_model), 0, 'f', 3)
                            .arg(col->width(), 0, 'f', 2)
                            .arg(col->height(), 0, 'f', 2));
                    }
                }
                else if (slabId > 0 && m_model)
                {
                    const auto* slab = m_model->getSlab(slabId);
                    if (slab)
                    {
                        emit objectHovered(tr("Survol : Dalle %1 (%2 nœuds | Épaisseur = %3 m | Aire = %4 m²)")
                            .arg(slabId)
                            .arg(slab->nodeIds().size())
                            .arg(slab->thickness(), 0, 'f', 2)
                            .arg(slab->area(*m_model), 0, 'f', 2));
                    }
                }
                else if (cableId > 0 && m_model)
                {
                    const auto* cable = m_model->getCable(cableId);
                    if (cable)
                    {
                        emit objectHovered(tr("Survol : Câble C%1 (Nœuds %2 -> %3 | Longueur = %4 m | Ø%5 mm | T0 = %6 kN)")
                            .arg(cableId)
                            .arg(cable->startNodeId())
                            .arg(cable->endNodeId())
                            .arg(cable->length(*m_model), 0, 'f', 3)
                            .arg(cable->diameter() * 1000.0, 0, 'f', 1)
                            .arg(cable->initialTension() / 1000.0, 0, 'f', 1));
                    }
                }
            }
            else
            {
                setCursor(interactionMode() == InteractionMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
                if (!m_isCursorSnapped)
                {
                    emit objectHovered(QString());
                }
                emit mouseCoordinatesChanged(wx, wy, wz);
                m_lastCursorPoint = gp_Pnt(wx, wy, wz);
                m_hasCursorPoint = true;
                double uwp = 0.0, vwp = 0.0, wwp = 0.0;
                m_workPlane.toLocal(gp_Pnt(wx, wy, wz), uwp, vwp, wwp);
                emit mouseLocalCoordinatesChanged(uwp, vwp);
                if ((m_snapToGrid || m_snapToObject) && !m_view.IsNull())
                {
                    m_view->Redraw();
                }
            }
        }
        break;
    }
}

void OccView::zoomAtCursor(const QPointF& logicalMousePos, double zoomFactor)
{
    if (zoomFactor <= 0.0 || m_view.IsNull())
        return;

    const Handle(Graphic3d_Camera)& aCam = m_view->Camera();
    if (aCam.IsNull())
        return;

    // Récupérer les dimensions réelles de la fenêtre OCCT (en pixels physiques/fenêtre)
    int winW = 0, winH = 0;
    if (!m_view->Window().IsNull())
    {
        m_view->Window()->Size(winW, winH);
    }
    if (winW <= 0 || winH <= 0)
    {
        const qreal dpr = devicePixelRatioF();
        winW = static_cast<int>(std::round(width() * dpr));
        winH = static_cast<int>(std::round(height() * dpr));
    }

    if (winW <= 0 || winH <= 0)
        return;

    // Position exacte de la souris dans le repère de la fenêtre OCCT (en pixels physiques)
    const QPoint p = convertMousePos(logicalMousePos);

    // Calcul des coordonnées normalisées de l'écran (NDC : [-1, 1] en X et Y)
    // ndcX : -1.0 (gauche) -> 0.0 (centre) -> +1.0 (droite)
    // ndcY : +1.0 (haut)   -> 0.0 (centre) -> -1.0 (bas)
    double normX = 0.5;
    double normY = 0.5;
    if (winW > 0 && winH > 0)
    {
        normX = static_cast<double>(p.x()) / static_cast<double>(winW);
        normY = static_cast<double>(p.y()) / static_cast<double>(winH);
    }
    else if (width() > 0 && height() > 0)
    {
        normX = logicalMousePos.x() / static_cast<double>(width());
        normY = logicalMousePos.y() / static_cast<double>(height());
    }

    const double ndcX = std::clamp((2.0 * normX) - 1.0, -1.0, 1.0);
    const double ndcY = std::clamp(1.0 - (2.0 * normY), -1.0, 1.0);
    const gp_Pnt ndcPnt(ndcX, ndcY, 0.0);

    if (aCam->IsOrthographic())
    {
        const double curScale = aCam->Scale();
        double newScale = curScale / zoomFactor;
        if (newScale < 1e-4) newScale = 1e-4;
        if (newScale > 1e8)  newScale = 1e8;

        // 1. Point 3D exact dans l'espace monde actuellement sous le curseur avant zoom
        const gp_Pnt pntBefore = aCam->UnProject(ndcPnt);

        // 2. Application de la nouvelle échelle de zoom
        aCam->SetScale(newScale);

        // 3. Point 3D qui se retrouverait sous le même curseur après zoom sans translation
        const gp_Pnt pntAfter = aCam->UnProject(ndcPnt);

        // 4. Déplacement exact de la caméra pour maintenir le point 3D initial rigoureusement immobile sous le curseur
        const gp_Vec aShift(pntAfter, pntBefore);
        aCam->SetEyeAndCenter(aCam->Eye().Translated(aShift), aCam->Center().Translated(aShift));
    }
    else
    {
        // En projection perspective : trouver le point 3D exact sous le curseur et zoomer le long du rayon
        double wx = 0.0, wy = 0.0, wz = 0.0;
        int detectedId = -1;

        if (!getPointUnderCursor(p, wx, wy, wz, detectedId))
        {
            const gp_Pnt pntOnPlane = aCam->UnProject(ndcPnt);
            wx = pntOnPlane.X();
            wy = pntOnPlane.Y();
            wz = pntOnPlane.Z();
        }

        const gp_Pnt targetPnt(wx, wy, wz);
        const gp_Vec eyeToTarget(aCam->Eye(), targetPnt);

        const double moveFactor = 1.0 - (1.0 / zoomFactor);
        const gp_Vec aShift = eyeToTarget * moveFactor;

        const double curDist = eyeToTarget.Magnitude();
        if (curDist > 1e-3 || moveFactor < 0.0)
        {
            aCam->SetEyeAndCenter(aCam->Eye().Translated(aShift), aCam->Center().Translated(aShift));
        }
    }

    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0 || m_view.IsNull())
        return;

    // Normalisation continue du facteur de zoom selon l'angle de rotation de la molette
    const double zoomFactor = std::pow(1.15, static_cast<double>(delta) / 120.0);
    zoomAtCursor(event->position(), zoomFactor);
}

void OccView::keyPressEvent(QKeyEvent* event)
{
    if (interactionMode() == InteractionMode::ModelingTool && handleModelingToolKey(event))
        return;

    if (m_currentAction == CurrentAction::WindowSelect)
    {
        if (!m_selectRubberBand.IsNull() && !m_context.IsNull() && m_context->IsDisplayed(m_selectRubberBand))
        {
            m_context->Erase(m_selectRubberBand, false);
            if (!m_view.IsNull()) m_view->Redraw();
        }
        m_currentAction = CurrentAction::Nothing;
        emit objectHovered(QString());
        return;
    }

    if (event->key() == Qt::Key_Escape)
    {
        if (m_interactionManager && m_interactionManager->hasActiveSelectionRequest())
        {
            m_gridRenderer.hideSnapMarker(m_context);
            m_interactionManager->cancelSelectionRequest();
            if (!m_view.IsNull()) m_view->Redraw();
            return;
        }
        if (!m_drawingPoints.empty())
        {
            cancelCurrentDrawing();
        }
        else
        {
            setInteractionMode(InteractionMode::Select);
        }
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (interactionMode() == InteractionMode::DrawSlab && m_drawingPoints.size() >= 3 && m_model)
        {
            finishCurrentSlab();   // même validation et même transaction que la fermeture du contour
        }
    }
    // F (zoom étendu), Maj+F, R : raccourcis des actions (ShortcutManager, personnalisables). Les traiter
    // ici ferait encore réagir l'ancienne touche après une personnalisation.
    else
    {
        QWidget::keyPressEvent(event);
    }
}


void OccView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl& url : event->mimeData()->urls())
        {
            if (TSA::Product::isOpenableProjectFile(url.toLocalFile()))
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void OccView::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl& url : event->mimeData()->urls())
        {
            QString filePath = url.toLocalFile();
            if (TSA::Product::isOpenableProjectFile(filePath))
            {
                event->acceptProposedAction();
                emit fileDropped(filePath);
                return;
            }
        }
    }
    QWidget::dropEvent(event);
}

