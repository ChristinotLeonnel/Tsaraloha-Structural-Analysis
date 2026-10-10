#include "OccView.h"
#include "DimensionRenderer.h"
#include "SelectionManager.h"
#include "ResultsVisualManager.h"
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
#include "../Grid/SnapEngine.h"
#include "../UI/Theme/ThemeManager.h"
#include "../Coordinate/CoordinateTransformationService.h"
#include "../Commands/ModifyCommands.h"
#include <gp_Trsf.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax1.hxx>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <algorithm>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QColor>
#include <Image_PixMap.hxx>

#ifdef _WIN32
    #include <windows.h>
    #include <WNT_Window.hxx>
#elif defined(__APPLE__)
    #include <Cocoa_Window.hxx>
#else
    #include <Xw_Window.hxx>
#endif

#include <Quantity_Color.hxx>
#include <Aspect_Grid.hxx>
#include <Prs3d_Drawer.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <SelectMgr_ViewerSelector.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <Graphic3d_Camera.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <Bnd_Box.hxx>
#include <cmath>

OccView::OccView(QWidget* parent)
    : QWidget(parent)
    , m_isDarkMode(TSA::UI::ThemeManager::instance().isDarkMode())
    , m_interactionManager(std::make_unique<TSA::Interaction::InteractionManager>(this))
    , m_resultsVisual(std::make_unique<TSA::Viewer::ResultsVisualManager>(this))
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);

    // Repères de grille : une seule logique de visibilité pour toutes les vues (GridLabelLayout).
    connect(this, &OccView::viewCameraChanged, this, &OccView::updateGridLabelView);
    connect(this, &OccView::viewCameraChanged, this, &OccView::updateMemberLoadZoomScale);

    if (m_interactionManager)
    {
        connect(m_interactionManager.get(), &TSA::Interaction::InteractionManager::modeChanged, this, [this](TSA::Interaction::InteractionMode mode) {
            emit interactionModeChanged(mode);
        });
        connect(m_interactionManager.get(), &TSA::Interaction::InteractionManager::promptChanged, this, &OccView::drawingPromptChanged);
        connect(m_interactionManager.get(), &TSA::Interaction::InteractionManager::selectionRequested, this, [this](const TSA::Interaction::SelectionRequest& /*req*/) {
            setCursor(Qt::CrossCursor);
            emit drawingPromptChanged(m_interactionManager->promptText());
        });
        connect(m_interactionManager.get(), &TSA::Interaction::InteractionManager::selectionCompleted, this, [this](const TSA::Interaction::SelectedEntity& /*result*/) {
            setCursor(Qt::ArrowCursor);
            m_gridRenderer.hideSnapMarker(m_context);
            if (!m_view.IsNull()) m_view->Redraw();
        });
        connect(m_interactionManager.get(), &TSA::Interaction::InteractionManager::selectionCancelled, this, [this]() {
            setCursor(Qt::ArrowCursor);
            m_gridRenderer.hideSnapMarker(m_context);
            if (!m_view.IsNull()) m_view->Redraw();
        });
    }
}

OccView::~OccView()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}


void OccView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!m_isInitialized)
    {
        initOcc();
        m_isInitialized = true;
        rebuildAllShapes();
        rebuildGrid();
        updateWorkPlaneVisual();
        fitAll();
    }
}

void OccView::initOcc()
{
    m_displayConnection = new Aspect_DisplayConnection();
    m_graphicDriver = new OpenGl_GraphicDriver(m_displayConnection);

    m_viewer = new V3d_Viewer(m_graphicDriver);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();

    // Plan privilégié horizontal (XY) à Z=0 avec normale dirigée vers +Z
    m_viewer->SetPrivilegedPlane(gp_Ax3(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0)));

    m_context = new AIS_InteractiveContext(m_viewer);
    m_context->SetPixelTolerance(8);
    m_dimensionRenderer = std::make_unique<TSA::Viewer::DimensionRenderer>(m_context, m_selectionManager);

    // Style de surbrillance dynamique (survol souris) : Cyan éclatant
    m_context->HighlightStyle()->SetColor(Quantity_NOC_CYAN1);
    m_context->HighlightStyle()->SetMethod(Aspect_TOHM_COLOR);
    m_context->HighlightStyle()->SetTransparency(0.2f);

    // Style de sélection (clic) : Orange vif
    m_context->SelectionStyle()->SetColor(Quantity_NOC_ORANGE);
    m_context->SelectionStyle()->SetMethod(Aspect_TOHM_COLOR);
    m_context->SelectionStyle()->SetTransparency(0.0f);

    setCursor(Qt::ArrowCursor);

    m_view = m_viewer->CreateView();

#ifdef _WIN32
    Handle(Aspect_Window) wind = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#elif defined(__APPLE__)
    Handle(Aspect_Window) wind = new Cocoa_Window(reinterpret_cast<NSView*>(winId()));
#else
    Handle(Aspect_Window) wind = new Xw_Window(m_displayConnection, static_cast<Window>(winId()));
#endif
    m_view->SetWindow(wind);
    if (!wind->IsMapped())
    {
        wind->Map();
    }

    Quantity_Color topColor = m_isDarkMode ? Quantity_Color(0.12, 0.14, 0.18, Quantity_TOC_RGB)
                                           : Quantity_Color(0.82, 0.88, 0.95, Quantity_TOC_RGB);
    Quantity_Color bottomColor = m_isDarkMode ? Quantity_Color(0.06, 0.08, 0.10, Quantity_TOC_RGB)
                                              : Quantity_Color(0.92, 0.94, 0.98, Quantity_TOC_RGB);
    m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);

    // Configuration explicite des axes du trièdre : X=Rouge, Y=Vert, Z=Bleu
    m_view->ZBufferTriedronSetup(
        Quantity_NOC_RED,       // Axe X
        Quantity_NOC_GREEN,     // Axe Y
        Quantity_NOC_BLUE1,     // Axe Z
        0.8,
        0.05,
        12
    );

    m_view->TriedronDisplay(
        Aspect_TOTP_LEFT_LOWER,
        Quantity_NOC_BLACK,
        0.1,
        V3d_ZBUFFER
    );

    // Orientation standard génie civil : +Z vers le haut (élévation), projection axonométrique droite Z-up
    m_view->SetUp(0.0, 0.0, 1.0);
    m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
    m_view->MustBeResized();

    // 3D ViewCube (Cube de navigation 3D interactif comme Robot Structural Analysis)
    m_viewCube = new AIS_ViewCube();
    m_viewCube->SetSize(62.0);
    if (m_isDarkMode)
    {
        m_viewCube->SetBoxColor(Quantity_Color(0.24, 0.28, 0.34, Quantity_TOC_RGB));
        m_viewCube->SetInnerColor(Quantity_Color(0.16, 0.19, 0.24, Quantity_TOC_RGB));
        m_viewCube->SetTextColor(Quantity_Color(0.90, 0.93, 0.96, Quantity_TOC_RGB));
    }
    else
    {
        m_viewCube->SetBoxColor(Quantity_Color(0.92, 0.94, 0.96, Quantity_TOC_RGB));
        m_viewCube->SetInnerColor(Quantity_Color(0.85, 0.88, 0.92, Quantity_TOC_RGB));
        m_viewCube->SetTextColor(Quantity_Color(0.10, 0.12, 0.15, Quantity_TOC_RGB));
    }
    m_viewCube->SetRoundRadius(0.10);
    m_viewCube->SetYup(false); // +Z vertical (élévation)

    // Libellés français conformes à Robot Structural Analysis
    m_viewCube->SetBoxSideLabel(V3d_Zpos, "HAUT");
    m_viewCube->SetBoxSideLabel(V3d_Zneg, "BAS");
    m_viewCube->SetBoxSideLabel(V3d_Ypos, "ARRIERE");
    m_viewCube->SetBoxSideLabel(V3d_Yneg, "AVANT");
    m_viewCube->SetBoxSideLabel(V3d_Xpos, "DROITE");
    m_viewCube->SetBoxSideLabel(V3d_Xneg, "GAUCHE");

    m_viewCube->SetTransformPersistence(new Graphic3d_TransformPers(
        Graphic3d_TMF_TriedronPers,
        Aspect_TOTP_RIGHT_UPPER,
        NCollection_Vec2<int>(85, 85)
    ));

    m_context->Display(m_viewCube, false);
}

void OccView::setModel(TSA::Model::Model* model)
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
    m_model = model;
    if (m_resultsVisual)
    {
        m_resultsVisual->setModel(model);
    }
    if (m_model)
    {
        m_model->addObserver(this);
    }
    if (m_isInitialized)
    {
        rebuildAllShapes();
    }
}

void OccView::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    if (m_resultsVisual)
    {
        m_resultsVisual->setResultsModel(results);
    }
}

void OccView::setSelectionManager(TSA::Viewer::SelectionManager* selectionManager)
{
    m_selectionManager = selectionManager;
    if (m_dimensionRenderer) m_dimensionRenderer->setSelectionManager(selectionManager);
    if (m_selectionManager)
    {
        // Surbrillance des cotations sélectionnées (objets hors des tables de formes structurelles).
        connect(m_selectionManager, &TSA::Viewer::SelectionManager::selectionChanged, this, [this]() {
            if (m_dimensionRenderer && m_model && m_selectionManager)
            {
                m_dimensionRenderer->setHighlighted(*m_model, m_selectionManager->selectedDimensions());
                if (!m_view.IsNull()) m_view->Redraw();
            }
        });
    }
    if (m_selectionManager && !m_workPlaneShape.IsNull())
    {
        m_selectionManager->registerWorkPlane(m_workPlane.id(), m_workPlaneShape);
    }
}


void OccView::paintEvent(QPaintEvent* /*event*/)
{
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_view.IsNull())
    {
        m_view->MustBeResized();
        emit viewCameraChanged();
    }
}


bool OccView::pixelToWorldPlane(int px, int py, double& wx, double& wy, double& wz) const
{
    if (m_view.IsNull())
        return false;

    double xEye = 0.0, yEye = 0.0, zEye = 0.0;
    double xDir = 0.0, yDir = 0.0, zDir = 0.0;
    m_view->ConvertWithProj(px, py, xEye, yEye, zEye, xDir, yDir, zDir);

    // 1. Raycast direct sur le Plan de Travail actif via ProjectionManager
    gp_Pnt eye(xEye, yEye, zEye);
    gp_Dir dir(xDir, yDir, zDir);
    gp_Pnt hitPnt;
    if (m_projectionManager.projectCursorRay(eye, dir, m_workPlane, hitPnt))
    {
        wx = hitPnt.X();
        wy = hitPnt.Y();
        wz = hitPnt.Z();
        return true;
    }

    // 2. Repli selon le mode de vue standard
    if (m_viewPlaneMode == ViewPlaneMode::PlanXZ || (m_viewPlaneMode == ViewPlaneMode::Perspective3D && std::abs(yDir) > 0.85))
    {
        if (std::abs(yDir) > 1e-6)
        {
            double t = (0.0 - yEye) / yDir;
            wx = xEye + t * xDir;
            wy = 0.0;
            wz = zEye + t * zDir;
            return true;
        }
    }
    else if (m_viewPlaneMode == ViewPlaneMode::PlanYZ || (m_viewPlaneMode == ViewPlaneMode::Perspective3D && std::abs(xDir) > 0.85))
    {
        if (std::abs(xDir) > 1e-6)
        {
            double t = (0.0 - xEye) / xDir;
            wx = 0.0;
            wy = yEye + t * yDir;
            wz = zEye + t * zDir;
            return true;
        }
    }
    else // PlanXY ou Perspective3D standard (plan horizontal à m_activeLevelZ)
    {
        if (std::abs(zDir) > 1e-6)
        {
            double t = (m_activeLevelZ - zEye) / zDir;
            wx = xEye + t * xDir;
            wy = yEye + t * yDir;
            wz = m_activeLevelZ;
            return true;
        }
    }

    m_view->Convert(px, py, wx, wy, wz);
    return true;
}

void OccView::worldToPixel(double wx, double wy, double wz, int& px, int& py) const
{
    if (m_view.IsNull())
    {
        px = 0;
        py = 0;
        return;
    }
    int occX = 0, occY = 0;
    m_view->Convert(wx, wy, wz, occX, occY);
    px = occX;
    py = occY;
}


QPoint OccView::convertMousePos(const QPointF& logicalPos) const
{
    if (!m_view.IsNull() && !m_view->Window().IsNull() && width() > 0 && height() > 0)
    {
        int winW = 0, winH = 0;
        m_view->Window()->Size(winW, winH);
        if (winW > 0 && winH > 0)
        {
            int px = static_cast<int>(std::round(logicalPos.x() * static_cast<double>(winW) / width()));
            int py = static_cast<int>(std::round(logicalPos.y() * static_cast<double>(winH) / height()));
            return QPoint(px, py);
        }
    }
    const qreal dpr = devicePixelRatioF();
    return QPoint(static_cast<int>(std::round(logicalPos.x() * dpr)),
                  static_cast<int>(std::round(logicalPos.y() * dpr)));
}

OccView::InteractionMode OccView::interactionMode() const
{
    return m_interactionManager ? m_interactionManager->mode() : InteractionMode::Select;
}

void OccView::setInteractionMode(InteractionMode mode)
{
    if (m_interactionManager)
    {
        if (m_interactionManager->mode() == mode)
            return;

        cancelCurrentDrawing();
        m_interactionManager->setMode(mode);
    }
    if (mode != InteractionMode::ModelingTool)
        m_activeTool = nullptr;   // l'outil appartient à MainWindow : la vue l'oublie simplement

    switch (interactionMode())
    {
    case InteractionMode::Select:
        setCursor(Qt::ArrowCursor);
        emit drawingPromptChanged(tr("Mode Sélection actif"));
        break;
    case InteractionMode::DrawNode:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Nœud : Cliquez dans le viewport pour créer un nœud"));
        break;
    case InteractionMode::DrawBar:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Outil Barre (Robot) : Cliquez sur le premier nœud ou saisissez ses coordonnées"));
        break;
    case InteractionMode::DrawBeam:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Poutre : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawColumn:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Poteau : Cliquez pour définir la base du poteau"));
        break;
    case InteractionMode::DrawSlab:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Dalle : Cliquez les nœuds du contour polygonal (Clic droit ou Entrée pour valider)"));
        break;
    case InteractionMode::DrawWall:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Voile : Cliquez pour définir le 1er nœud du voile (Ép=%1m, H=%2m)").arg(m_presets.wall.thickness).arg(m_presets.wall.height));
        break;
    case InteractionMode::DrawFoundation:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Fondation : Cliquez sur un nœud pour créer une semelle"));
        break;
    case InteractionMode::DrawTruss:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Treillis : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawCable:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Câble : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawStayCable:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Hauban : Cliquez sur le nœud de pylône"));
        break;
    case InteractionMode::DrawSuspensionCable:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Câble Porteur : Cliquez sur le premier ancrage/pylône"));
        break;
    case InteractionMode::DrawHanger:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Suspente : Cliquez sur le câble porteur ou nœud supérieur"));
        break;
    case InteractionMode::MoveOrigin3D:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Déplacer le Repère 3D : Cliquez sur le nouvel emplacement de l'origine"));
        break;
    case InteractionMode::Paste3D:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Coller en 3D : Cliquez à l'endroit désiré pour déposer les éléments (ou Échap pour annuler)"));
        break;
    }

    emit interactionModeChanged(interactionMode());
}

void OccView::setCurrentBarProperties(const TSA::Model::BarProperties& props)
{
    m_currentBarProps = props;

    m_presets.beam.section = props.section;
    m_presets.beam.material = props.material;
    m_presets.beam.betaAngle = props.rotation;
    if (!props.color.empty()) m_presets.beam.color = props.color;

    m_presets.column.section = props.section;
    m_presets.column.material = props.material;
    m_presets.column.betaAngle = props.rotation;
    if (!props.color.empty()) m_presets.column.color = props.color;

    if (!m_drawingPoints.empty() && !m_lastMousePos.isNull())
    {
        double wx = 0.0, wy = 0.0, wz = 0.0;
        int detNodeId = -1;
        if (getPointUnderCursor(m_lastMousePos, wx, wy, wz, detNodeId))
        {
            updateRubberBand(gp_Pnt(wx, wy, wz));
        }
    }
}

void OccView::finishBarSegment(int endNodeId, bool created)
{
    clearRubberBand();
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();
    const auto* end = (created && m_barChainMode && m_model) ? m_model->getNode(endNodeId) : nullptr;
    if (!end) return;
    // « Étirer » : l'extrémité devient le départ de l'élément suivant (même mode, même réglages).
    const gp_Pnt pt(end->x(), end->y(), end->z());
    m_drawingNodeIds.push_back(endNodeId);
    m_drawingPoints.push_back(pt);
    emit barFirstPointPicked(pt, endNodeId);
    emit drawingPromptChanged(tr("Étirer : départ N%1. Cliquez pour l'élément suivant (Échap pour terminer)").arg(endNodeId));
}

void OccView::startChainedBarDrawing(const gp_Pnt& originPt, int originNodeId)
{
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();
    m_drawingNodeIds.push_back(originNodeId);
    m_drawingPoints.push_back(originPt);
    setInteractionMode(InteractionMode::DrawBar);
    emit drawingPromptChanged(tr("Barre : Origine N%1 fixée en (%2; %3; %4). Cliquez pour l'extrémité")
        .arg(originNodeId).arg(originPt.X(), 0, 'f', 2).arg(originPt.Y(), 0, 'f', 2).arg(originPt.Z(), 0, 'f', 2));
}

void OccView::finishCurrentSlab()
{
    if (m_drawingNodeIds.size() >= 3 && m_model)
    {
        m_model->pushUndoState(tr("Création Dalle").toStdString());
        int slabId = m_model->addSlab(m_drawingNodeIds, m_presets.slab.thickness);
        if (auto* s = m_model->getSlab(slabId))
        {
            s->setMaterial(TSA::Model::Material::findByName(m_presets.slab.material.name));
            if (!m_presets.slab.color.empty()) s->setColor(m_presets.slab.color);
            updateSlabShape(slabId);
        }
        emit elementCreated();
        emit slabCreated(slabId);
        emit drawingPromptChanged(tr("Dalle S%1 créée (%2 nœuds, ép=%3m). Cliquez pour une nouvelle dalle").arg(slabId).arg(m_drawingNodeIds.size()).arg(m_presets.slab.thickness));
        clearRubberBand();
        m_drawingNodeIds.clear();
        m_drawingPoints.clear();
    }
}

void OccView::resetCurrentSlabContour()
{
    clearRubberBand();
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();
    emit slabDrawingCancelled();
}

void OccView::cancelCurrentDrawing()
{
    if (interactionMode() == InteractionMode::ModelingTool)
    {
        clearModelingToolPreview();
        m_toolInput.clear();
    }
    if (m_isManipulatingWorkPlane)
    {
        m_isManipulatingWorkPlane = false;
        if (!m_manipulator.IsNull() && m_manipulator->IsAttached() && m_manipulator->HasActiveMode())
        {
            m_manipulator->StopTransform(false);
        }
    }
    clearRubberBand();
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();

    switch (interactionMode())
    {
    case InteractionMode::DrawNode:
        emit drawingPromptChanged(tr("Mode Dessin Nœud : Cliquez dans le viewport pour créer un nœud"));
        break;
    case InteractionMode::DrawBar:
        emit barDrawingCancelled();
        emit drawingPromptChanged(tr("Outil Barre (Robot) : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawBeam:
        emit drawingPromptChanged(tr("Mode Dessin Poutre : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawColumn:
        emit drawingPromptChanged(tr("Mode Dessin Poteau : Cliquez pour définir la base du poteau"));
        break;
    case InteractionMode::DrawSlab:
        emit slabDrawingCancelled();
        emit drawingPromptChanged(tr("Mode Dessin Dalle : Cliquez les nœuds du contour polygonal (Clic droit ou Entrée pour valider)"));
        break;
    case InteractionMode::DrawWall:
        emit wallDrawingCancelled();
        emit drawingPromptChanged(tr("Mode Dessin Voile : Cliquez pour définir le 1er nœud du voile (Ép=%1m, H=%2m)").arg(m_presets.wall.thickness).arg(m_presets.wall.height));
        break;
    case InteractionMode::DrawFoundation:
        emit drawingPromptChanged(tr("Mode Dessin Fondation : Cliquez sur un nœud pour créer une semelle"));
        break;
    case InteractionMode::DrawTruss:
        emit drawingPromptChanged(tr("Mode Dessin Treillis : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawCable:
        emit cableDrawingCancelled();
        emit drawingPromptChanged(tr("Mode Dessin Câble : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawStayCable:
        emit drawingPromptChanged(tr("Mode Dessin Hauban : Cliquez sur le nœud de pylône"));
        break;
    case InteractionMode::DrawSuspensionCable:
        emit drawingPromptChanged(tr("Mode Dessin Câble Porteur : Cliquez sur le premier ancrage/pylône"));
        break;
    case InteractionMode::DrawHanger:
        emit drawingPromptChanged(tr("Mode Dessin Suspente : Cliquez sur le câble porteur ou nœud supérieur"));
        break;
    case InteractionMode::MoveOrigin3D:
        emit drawingPromptChanged(tr("Déplacer le Repère 3D : Cliquez sur le nouvel emplacement de l'origine"));
        break;
    case InteractionMode::Paste3D:
        emit drawingPromptChanged(tr("Coller en 3D : Cliquez pour déposer les éléments (ou Échap pour annuler)"));
        break;
    default:
        break;
    }
}

TSA::Grid::GridSnapResult OccView::computeSnap(int px, int py) const
{
    TSA::Grid::GridSnapResult none;
    if (m_view.IsNull() || (!m_snapToObject && !m_snapToGrid))
        return none;
    const Handle(Graphic3d_Camera)& cam = m_view->Camera();
    int winW = 0, winH = 0;
    if (!m_view->Window().IsNull()) m_view->Window()->Size(winW, winH);
    if (cam.IsNull() || winW <= 0 || winH <= 0)
        return none;

    // Rayon de visée passant par le curseur (perspective comme orthographique)
    double xEye = 0.0, yEye = 0.0, zEye = 0.0, xDir = 0.0, yDir = 0.0, zDir = 0.0;
    m_view->ConvertWithProj(px, py, xEye, yEye, zEye, xDir, yDir, zDir);
    if (std::abs(xDir) + std::abs(yDir) + std::abs(zDir) < 1e-12)
        return none;

    TSA::Grid::SnapQuery q;
    q.cursorX = px;
    q.cursorY = py;
    q.rayOrigin = gp_Pnt(xEye, yEye, zEye);
    q.rayDir = gp_Dir(xDir, yDir, zDir);
    q.radiusPx = kSnapApertureLogicalPx * devicePixelRatioF();   // ouverture constante à l'écran
    q.modes = m_gridSnapManager ? m_gridSnapManager->activeModes() : TSA::Grid::SnapMode::All;
    q.objects = m_snapToObject;
    q.grids = m_snapToGrid;
    if (!m_drawingPoints.empty())
    {
        q.hasReference = true;   // perpendiculaire depuis le point précédent du tracé
        q.reference = m_drawingPoints.back();
    }

    // Projection monde → pixels de la fenêtre de rendu (même repère que la souris)
    const bool ortho = cam->IsOrthographic();
    const gp_Pnt eye = cam->Eye();
    const gp_Dir viewDir = cam->Direction();
    q.project = [&cam, ortho, eye, viewDir, winW, winH](const gp_Pnt& p, double& sx, double& sy) {
        if (!ortho && gp_Vec(eye, p).Dot(gp_Vec(viewDir)) <= 0.0)
            return false;   // derrière l'observateur
        const gp_Pnt ndc = cam->Project(p);
        sx = (ndc.X() + 1.0) * 0.5 * winW;
        sy = (1.0 - ndc.Y()) * 0.5 * winH;
        return std::isfinite(sx) && std::isfinite(sy);
    };
    // Seuls les objets affichés sont accrochables (calques, filtre de nœuds, isolation / mode 2D)
    q.acceptElement = [this](int kind, int id) {
        using K = TSA::Model::ElementKind;
        if (!m_model) return false;
        switch (static_cast<K>(kind))
        {
        case K::Node: return isNodeVisibleByFilter(id) && keepNodeUnderIsolation(id);
        case K::Beam:
            if (const auto* e = m_model->getBeam(id))
                return isElementCategoryVisible(ElementCategory::Beams) && keepLinearUnderIsolation(e->startNodeId(), e->endNodeId());
            return false;
        case K::Column:
            if (const auto* e = m_model->getColumn(id))
                return isElementCategoryVisible(ElementCategory::Columns) && keepLinearUnderIsolation(e->startNodeId(), e->endNodeId());
            return false;
        case K::TrussMember:
            if (const auto* e = m_model->getTrussMember(id))
                return isElementCategoryVisible(ElementCategory::Trusses) && keepLinearUnderIsolation(e->startNodeId(), e->endNodeId());
            return false;
        case K::Cable:
            if (const auto* e = m_model->getCable(id))
                return isElementCategoryVisible(ElementCategory::Cables) && keepLinearUnderIsolation(e->startNodeId(), e->endNodeId());
            return false;
        case K::Slab:
            if (const auto* e = m_model->getSlab(id))
                return isElementCategoryVisible(ElementCategory::Slabs) && keepSurfaceUnderIsolation(e->nodeIds());
            return false;
        case K::Wall:
            if (const auto* e = m_model->getWall(id))
                return isElementCategoryVisible(ElementCategory::Walls) && keepLinearUnderIsolation(e->startNodeId(), e->endNodeId());
            return false;
        default: return true;
        }
    };
    if (m_mode2DActive)
        q.acceptPoint = [this](const gp_Pnt& p) { return std::abs(m_workPlane.distanceTo(p)) <= 0.05; };

    TSA::Grid::SnapEngine engine(q);
    if (m_model) engine.collectModel(*m_model);

    // Grilles visibles (la grille active en tête)
    std::vector<const TSA::Grid::GridSystem*> grids;
    if (m_gridManager)
    {
        const TSA::Grid::GridSystem* active = m_gridManager->activeGrid();
        if (active && active->isVisible()) grids.push_back(active);
        for (const auto& g : m_gridManager->grids())
            if (g && g.get() != active && g->isVisible()) grids.push_back(g.get());
    }
    engine.collectGrids(grids);
    return engine.result();
}

bool OccView::findNearest3DPoint(int px, int py, double& outX, double& outY, double& outZ,
                                int& outNodeId, QString& outDesc, TSA::Grid::GridSnapType& outType) const
{
    const TSA::Grid::GridSnapResult r = computeSnap(px, py);
    outNodeId = (r.snapped && r.type == TSA::Grid::GridSnapType::Node) ? r.targetEntityId : -1;
    outType = r.snapped ? r.type : TSA::Grid::GridSnapType::None;
    if (!r.snapped)
        return false;
    outX = r.point.X();
    outY = r.point.Y();
    outZ = r.point.Z();
    outDesc = QString::fromStdString(r.description);
    return true;
}

void OccView::setLastSnap(const TSA::Grid::GridSnapResult& snap)
{
    const auto& prev = m_lastSnapResult;
    if (snap.snapped != prev.snapped
        || (snap.snapped && (snap.type != prev.type || snap.source != prev.source || snap.description != prev.description
                             || snap.point.SquareDistance(prev.point) > 1e-18)))
        m_snapMarkerDirty = true;
    m_lastSnapResult = snap;
}

bool OccView::getPointUnderCursor(const QPoint& mousePixelPos, double& x, double& y, double& z, int& detectedNodeId)
{
    // Point unique utilisé par l'aperçu (survol) ET par le clic : ce que l'utilisateur voit
    // (marqueur placé sur SnapResult.point) est exactement ce qui sera créé.
    detectedNodeId = -1;
    m_isCursorSnapped = false;
    if (m_view.IsNull())
        return false;

    const int px = mousePixelPos.x();
    const int py = mousePixelPos.y();
    m_gridRenderer.setSnapMarkerPixelScale(devicePixelRatioF());

    // 1. Accrochage écran : nœuds, barres, faces, intersections, grilles (SnapEngine)
    TSA::Grid::GridSnapResult snap = computeSnap(px, py);
    if (snap.snapped && m_projectionManager.is2D())
    {
        const gp_Pnt projected = m_projectionManager.projectPoint(snap.point, m_workPlane);
        if (snap.point.Distance(projected) > 1e-4)
        {
            snap.point = projected;   // projeté sur le plan 2D : ce n'est plus le nœud lui-même
            snap.targetEntityId = -1;
            snap.type = TSA::Grid::GridSnapType::Nearest;
        }
    }

    double wx = 0.0, wy = 0.0, wz = 0.0;
    if (!snap.snapped)
    {
        // 2. Aucun objet sous le curseur : point du plan de travail, arrondi au pas de sa grille
        if (!pixelToWorldPlane(px, py, wx, wy, wz))
        {
            setLastSnap(TSA::Grid::GridSnapResult());
            m_gridRenderer.hideSnapMarker(m_context);
            return false;
        }
        if (m_snapToGrid)
        {
            snap = m_snapManager.snapToWorkPlaneGrid(gp_Pnt(wx, wy, wz), m_workPlane);
            snap.source = TSA::Grid::SnapSource::WorkPlane;
        }
    }

    setLastSnap(snap);
    if (snap.snapped)
    {
        m_isCursorSnapped = true;
        x = snap.point.X();
        y = snap.point.Y();
        z = snap.point.Z();
        if (snap.type == TSA::Grid::GridSnapType::Node)
            detectedNodeId = snap.targetEntityId;
        m_gridRenderer.showSnapMarker(snap, m_context);
        emit objectHovered(QString::fromStdString(snap.description));
        return true;
    }

    m_gridRenderer.hideSnapMarker(m_context);
    x = wx;
    y = wy;
    z = wz;
    return true;
}

int OccView::getOrCreateNode(double x, double y, double z, int existingNodeId)
{
    if (existingNodeId > 0 && m_model && m_model->getNode(existingNodeId))
        return existingNodeId;

    if (!m_model)
        return -1;

    for (const auto& [id, node] : m_model->nodes())
    {
        double dx = node.x() - x;
        double dy = node.y() - y;
        double dz = node.z() - z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1e-3)
        {
            return id;
        }
    }

    return m_model->addNode(x, y, z);
}

void OccView::clearTransformPreview()
{
    m_previewGhostsBuilt = false;
    if (m_context.IsNull())
    {
        m_previewGhostShapes.clear();
        return;
    }

    for (auto& ghost : m_previewGhostShapes)
    {
        if (!ghost.IsNull() && m_context->IsDisplayed(ghost))
        {
            m_context->Remove(ghost, false);
        }
    }
    m_previewGhostShapes.clear();
}

void OccView::buildTransformPreviewGhosts()
{
    // Les fantômes sont construits UNE SEULE FOIS à la position d'origine ; l'aperçu de l'outil
    // en cours leur applique ensuite une transformation locale.
    const Quantity_Color barColor = Quantity_NOC_CYAN;

    auto addGhost = [&](const TopoDS_Shape& s, double transparency, const Quantity_Color& color)
    {
        if (s.IsNull())
            return;
        Handle(AIS_Shape) ghost = new AIS_Shape(s);
        ghost->SetColor(color);
        ghost->SetTransparency(transparency);
        m_context->Display(ghost, false);
        m_previewGhostShapes.push_back(ghost);
        ++m_previewGhostBuildCount;
    };

    for (int bId : m_selectionManager->selectedBeams())
    {
        const auto* b = m_model->getBeam(bId);
        if (!b) continue;
        const auto* nA = m_model->getNode(b->startNodeId());
        const auto* nB = m_model->getNode(b->endNodeId());
        if (!nA || !nB) continue;
        addGhost(TSA::Geometry::BeamGeometry::createBeamShape(*nA, *nB, b->section(), b->rotation(), b->eccentricity()),
                 0.4, barColor);
    }

    for (int cId : m_selectionManager->selectedColumns())
    {
        const auto* col = m_model->getColumn(cId);
        if (!col) continue;
        const auto* nA = m_model->getNode(col->startNodeId());
        const auto* nB = m_model->getNode(col->endNodeId());
        if (!nA || !nB) continue;
        addGhost(TSA::Geometry::BeamGeometry::createBeamShape(*nA, *nB, col->section(), col->rotation()),
                 0.4, barColor);
    }

    // Nœuds isolés sélectionnés
    for (int nId : m_selectionManager->selectedNodes())
    {
        const auto* node = m_model->getNode(nId);
        if (!node) continue;
        addGhost(TSA::Geometry::BeamGeometry::createNodeShape(*node, 0.10), 0.3, barColor);
    }

    m_previewGhostsBuilt = true;
}

void OccView::clearRubberBand()
{
    clearTransformPreview();
    if (!m_rubberBandShape.IsNull() && !m_context.IsNull())
    {
        m_context->Remove(m_rubberBandShape, false);
        m_rubberBandShape.Nullify();
        if (!m_view.IsNull())
            m_view->Redraw();
    }
}

void OccView::updateRubberBand(const gp_Pnt& currentPnt)
{
    if (m_context.IsNull() || m_view.IsNull())
        return;

    TopoDS_Shape shape;

    if (interactionMode() == InteractionMode::DrawBar ||
        interactionMode() == InteractionMode::DrawBeam ||
        interactionMode() == InteractionMode::DrawColumn)
    {
        if (m_drawingPoints.empty())
            return;

        const gp_Pnt& pStart = m_drawingPoints.back();
        if (pStart.Distance(currentPnt) < 1e-4)
            return;

        TSA::Model::Node tempA(0, pStart.X(), pStart.Y(), pStart.Z());
        TSA::Model::Node tempB(1, currentPnt.X(), currentPnt.Y(), currentPnt.Z());

        TSA::Model::Section currentSec = m_currentBarProps.section;
        if (currentSec.width <= 0.0 && currentSec.diameter <= 0.0)
        {
            currentSec = (interactionMode() == InteractionMode::DrawColumn) ? m_presets.column.section : m_presets.beam.section;
        }
        double rot = (m_currentBarProps.rotation != 0.0) ? m_currentBarProps.rotation
            : ((interactionMode() == InteractionMode::DrawColumn) ? m_presets.column.betaAngle : m_presets.beam.betaAngle);
        TSA::Model::BarEccentricity ecc = m_currentBarProps.eccentricity;

        shape = TSA::Geometry::BeamGeometry::createBeamShape(tempA, tempB, currentSec, rot, ecc);
        if (shape.IsNull())
        {
            shape = BRepBuilderAPI_MakeEdge(pStart, currentPnt).Edge();
        }
    }
    else if (interactionMode() == InteractionMode::DrawSlab)
    {
        if (m_drawingPoints.empty())
            return;

        BRepBuilderAPI_MakePolygon poly;
        for (const auto& p : m_drawingPoints)
        {
            poly.Add(p);
        }
        if (m_drawingPoints.back().Distance(currentPnt) > 1e-4)
        {
            poly.Add(currentPnt);
        }

        if (!poly.IsDone() || poly.Wire().IsNull())
            return;

        shape = poly.Wire();
    }
    else if (interactionMode() == InteractionMode::DrawWall)
    {
        if (m_drawingPoints.empty())
            return;

        const gp_Pnt& pStart = m_drawingPoints.front();
        if (pStart.Distance(currentPnt) < 1e-4)
            return;

        TSA::Model::Node tempA(0, pStart.X(), pStart.Y(), pStart.Z());
        TSA::Model::Node tempB(1, currentPnt.X(), currentPnt.Y(), currentPnt.Z());
        shape = TSA::Geometry::WallGeometry::createWallShape(tempA, tempB,
                    m_presets.wall.height, m_presets.wall.thickness, m_presets.wall.offset);
        if (shape.IsNull())
        {
            shape = BRepBuilderAPI_MakeEdge(pStart, currentPnt).Edge();
        }
    }
    else if (interactionMode() == InteractionMode::DrawCable ||
             interactionMode() == InteractionMode::DrawStayCable ||
             interactionMode() == InteractionMode::DrawSuspensionCable ||
             interactionMode() == InteractionMode::DrawHanger)
    {
        if (m_drawingPoints.empty())
            return;

        const gp_Pnt& pStart = m_drawingPoints.back();
        if (pStart.Distance(currentPnt) < 1e-4)
            return;

        shape = TSA::Geometry::CableGeometry3D::createStraightCable(pStart, currentPnt, 0.020);
        if (shape.IsNull())
        {
            shape = BRepBuilderAPI_MakeEdge(pStart, currentPnt).Edge();
        }
    }
    else
    {
        return;
    }

    if (shape.IsNull())
        return;

    if (m_rubberBandShape.IsNull())
    {
        m_rubberBandShape = new AIS_Shape(shape);
        m_rubberBandShape->SetColor(Quantity_NOC_ORANGE);
        m_rubberBandShape->SetTransparency(0.35);
        m_rubberBandShape->SetWidth(2.5);
        m_context->Display(m_rubberBandShape, false);
    }
    else
    {
        m_rubberBandShape->SetShape(shape);
        m_rubberBandShape->SetColor(Quantity_NOC_ORANGE);
        m_rubberBandShape->SetTransparency(0.35);
        m_context->Redisplay(m_rubberBandShape, false);
    }

    m_view->Redraw();
}


QImage OccView::captureViewImage(int width, int height, bool hideNavigationAids)
{
    if (m_view.IsNull())
    {
        return QImage();
    }

    const bool cubeShown = hideNavigationAids && !m_context.IsNull() && !m_viewCube.IsNull() && m_context->IsDisplayed(m_viewCube);
    if (cubeShown) m_context->Erase(m_viewCube, false);
    if (hideNavigationAids) m_view->TriedronErase();
    auto restoreAids = [&] {
        if (!hideNavigationAids) return;
        m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, m_isDarkMode ? Quantity_NOC_WHITE : Quantity_NOC_BLACK, 0.1,
                                V3d_ZBUFFER); // mêmes réglages que setDarkMode()
        if (cubeShown) m_context->Display(m_viewCube, false);
        m_view->Redraw();
    };

    try
    {
        Image_PixMap pixmap;
        if (m_view->ToPixMap(pixmap, width, height, Graphic3d_BT_RGB))
        {
            QImage img(pixmap.Data(), static_cast<int>(pixmap.Width()), static_cast<int>(pixmap.Height()),
                       static_cast<int>(pixmap.SizeRowBytes()), QImage::Format_RGB888);
            QImage result = img.copy();
            // Lecture OpenGL : les lignes sont rangées du bas vers le haut. Sans ce retournement,
            // l'aperçu du projet (miniature de l'Explorateur, Start Center) était à l'envers (BUG-066).
            if (!pixmap.IsTopDown()) result.flip(Qt::Vertical);
            restoreAids();
            return result;
        }
    }
    catch (...)
    {
    }
    restoreAids();

    return grab().toImage().scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}


