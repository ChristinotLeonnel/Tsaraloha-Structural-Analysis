#include "OccView.h"
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

void OccView::resizeEvent(QResizeEvent* /*event*/)
{
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
    case InteractionMode::Move3D:
        setCursor(Qt::CrossCursor);
        m_hasBasePoint = false;
        emit drawingPromptChanged(tr("Déplacement 3D : Cliquez sur le point de base"));
        break;
    case InteractionMode::Copy3D:
        setCursor(Qt::CrossCursor);
        m_hasBasePoint = false;
        emit drawingPromptChanged(tr("Copie 3D (Translation) : Cliquez sur le point de base"));
        break;
    case InteractionMode::Rotate3D:
        setCursor(Qt::CrossCursor);
        m_hasCenterPoint = false;
        m_hasBasePoint = false;
        emit drawingPromptChanged(tr("Rotation 3D : Cliquez sur le centre de rotation"));
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
    m_hasBasePoint = false;
    m_hasCenterPoint = false;

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
    case InteractionMode::Move3D:
        emit drawingPromptChanged(tr("Déplacement 3D : Cliquez sur le point de base"));
        break;
    case InteractionMode::Copy3D:
        emit drawingPromptChanged(tr("Copie 3D (Translation) : Cliquez sur le point de base"));
        break;
    case InteractionMode::Rotate3D:
        emit drawingPromptChanged(tr("Rotation 3D : Cliquez sur le centre de rotation"));
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

bool OccView::findNearest3DPoint(int px, int py, double& outX, double& outY, double& outZ,
                                int& outNodeId, QString& outDesc, TSA::Grid::GridSnapType& outType) const
{
    outNodeId = -1;
    outType = TSA::Grid::GridSnapType::None;
    if (m_view.IsNull())
        return false;

    double xEye = 0.0, yEye = 0.0, zEye = 0.0;
    double xDir = 0.0, yDir = 0.0, zDir = 0.0;
    m_view->ConvertWithProj(px, py, xEye, yEye, zEye, xDir, yDir, zDir);
    gp_Pnt eyePnt(xEye, yEye, zEye);
    gp_Dir viewDir(xDir, yDir, zDir);

    const double screenPixelRadius = 18.0; // 18 pixels d'aimantation écran
    double bestNodeDist2 = screenPixelRadius * screenPixelRadius;
    bool foundNode = false;
    gp_Pnt bestNodePnt;
    int bestNodeId = -1;

    // 1. Détection prioritaire N°1 : Nœuds structuraux réels du modèle dans l'espace 3D
    if (m_model)
    {
        for (const auto& [nId, node] : m_model->nodes())
        {
            gp_Pnt p(node.x(), node.y(), node.z());
            gp_Vec toP(eyePnt, p);
            if (toP.Dot(viewDir) < 0.0)
                continue;

            int sx = 0, sy = 0;
            m_view->Convert(p.X(), p.Y(), p.Z(), sx, sy);
            double dx = sx - px;
            double dy = sy - py;
            double dist2 = dx * dx + dy * dy;
            if (dist2 <= bestNodeDist2)
            {
                bestNodeDist2 = dist2;
                bestNodePnt = p;
                bestNodeId = nId;
                foundNode = true;
            }
        }
    }

    if (foundNode)
    {
        outX = bestNodePnt.X();
        outY = bestNodePnt.Y();
        outZ = bestNodePnt.Z();
        outNodeId = bestNodeId;
        outType = TSA::Grid::GridSnapType::Node;
        outDesc = QString("Nœud N%1 (%2, %3, %4 m)")
            .arg(bestNodeId)
            .arg(outX, 0, 'f', 2)
            .arg(outY, 0, 'f', 2)
            .arg(outZ, 0, 'f', 2);
        return true;
    }

    // 1b. Détection Object Snap analytique (Extrémité, Milieu, Centre, Perpendiculaire, Le plus proche)
    if (m_gridSnapManager && m_model)
    {
        double wx = 0.0, wy = 0.0, wz = 0.0;
        if (pixelToWorldPlane(px, py, wx, wy, wz))
        {
            auto objSnap = m_gridSnapManager->findObjectSnap(gp_Pnt(wx, wy, wz), m_model, 0.80);
            if (objSnap.snapped)
            {
                int sx = 0, sy = 0;
                m_view->Convert(objSnap.point.X(), objSnap.point.Y(), objSnap.point.Z(), sx, sy);
                double dx = sx - px;
                double dy = sy - py;
                if (dx * dx + dy * dy <= screenPixelRadius * screenPixelRadius)
                {
                    outX = objSnap.point.X();
                    outY = objSnap.point.Y();
                    outZ = objSnap.point.Z();
                    outNodeId = -1;
                    outType = objSnap.type;
                    outDesc = QString::fromStdString(objSnap.description);
                    return true;
                }
            }
        }
    }

    // Récupérer toutes les grilles visibles (la grille active en tête de liste)
    std::vector<const TSA::Grid::GridSystem*> visibleGrids;
    const TSA::Grid::GridSystem* activeGrid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    if (activeGrid && activeGrid->isVisible())
    {
        visibleGrids.push_back(activeGrid);
    }
    if (m_gridManager)
    {
        for (const auto& g : m_gridManager->grids())
        {
            if (g && g.get() != activeGrid && g->isVisible())
            {
                visibleGrids.push_back(g.get());
            }
        }
    }

    if (visibleGrids.empty())
    {
        return false;
    }

    // 2. Détection prioritaire N°2 : Intersections 3D & Origines des grilles (tous étages et montants verticaux)
    double bestGridDist2 = screenPixelRadius * screenPixelRadius;
    bool foundGridInter = false;
    gp_Pnt bestGridPnt;
    QString bestGridLabel;
    TSA::Grid::GridSnapType bestGridType = TSA::Grid::GridSnapType::Intersection;

    for (const auto* grid : visibleGrids)
    {
        if (grid->cartesian())
        {
            for (const auto& inter : grid->cartesian()->intersections())
            {
                const gp_Pnt& p = inter.point;
                gp_Vec toP(eyePnt, p);
                if (toP.Dot(viewDir) < 0.0)
                    continue;

                int sx = 0, sy = 0;
                m_view->Convert(p.X(), p.Y(), p.Z(), sx, sy);
                double dx = sx - px;
                double dy = sy - py;
                double dist2 = dx * dx + dy * dy;
                if (dist2 <= bestGridDist2)
                {
                    bestGridDist2 = dist2;
                    bestGridPnt = p;
                    foundGridInter = true;
                    bestGridType = TSA::Grid::GridSnapType::Intersection;
                    bestGridLabel = QString("Grille (%1, %2, Z=%3 m)")
                        .arg(QString::fromStdString(inter.labelX))
                        .arg(QString::fromStdString(inter.labelY))
                        .arg(p.Z(), 0, 'f', 2);
                }
            }
        }
        else if (grid->cylindrical())
        {
            const gp_Pnt& orig = grid->definition().origin();
            const auto& zLevels = grid->definition().zLevels();
            std::vector<double> levels = zLevels.empty() ? std::vector<double>{ 0.0 } : zLevels;

            // Centre de la grille à chaque niveau Z
            for (double zOffset : levels)
            {
                gp_Pnt centerPt(orig.X(), orig.Y(), orig.Z() + zOffset);
                gp_Vec toP(eyePnt, centerPt);
                if (toP.Dot(viewDir) < 0.0)
                    continue;

                int sx = 0, sy = 0;
                m_view->Convert(centerPt.X(), centerPt.Y(), centerPt.Z(), sx, sy);
                double dx = sx - px;
                double dy = sy - py;
                double dist2 = dx * dx + dy * dy;
                if (dist2 <= bestGridDist2)
                {
                    bestGridDist2 = dist2;
                    bestGridPnt = centerPt;
                    foundGridInter = true;
                    bestGridType = TSA::Grid::GridSnapType::Origin;
                    bestGridLabel = QString("Centre Grille Cylindrique (%1, %2, %3 m)")
                        .arg(centerPt.X(), 0, 'f', 2)
                        .arg(centerPt.Y(), 0, 'f', 2)
                        .arg(centerPt.Z(), 0, 'f', 2);
                }
            }

            // Intersections (Rayons x Angles)
            for (const auto& inter : grid->cylindrical()->intersections())
            {
                const gp_Pnt& p = inter.point;
                gp_Vec toP(eyePnt, p);
                if (toP.Dot(viewDir) < 0.0)
                    continue;

                int sx = 0, sy = 0;
                m_view->Convert(p.X(), p.Y(), p.Z(), sx, sy);
                double dx = sx - px;
                double dy = sy - py;
                double dist2 = dx * dx + dy * dy;
                if (dist2 <= bestGridDist2)
                {
                    bestGridDist2 = dist2;
                    bestGridPnt = p;
                    foundGridInter = true;
                    bestGridType = TSA::Grid::GridSnapType::Intersection;
                    bestGridLabel = QString("Grille Cylindrique (R=%1 m, %2°)")
                        .arg(inter.radius, 0, 'f', 2)
                        .arg(inter.angleDeg, 0, 'f', 1);
                }
            }
        }
        else if (grid->arbitrary())
        {
            for (const auto& p : grid->arbitrary()->intersections())
            {
                gp_Vec toP(eyePnt, p);
                if (toP.Dot(viewDir) < 0.0)
                    continue;

                int sx = 0, sy = 0;
                m_view->Convert(p.X(), p.Y(), p.Z(), sx, sy);
                double dx = sx - px;
                double dy = sy - py;
                double dist2 = dx * dx + dy * dy;
                if (dist2 <= bestGridDist2)
                {
                    bestGridDist2 = dist2;
                    bestGridPnt = p;
                    foundGridInter = true;
                    bestGridType = TSA::Grid::GridSnapType::Intersection;
                    bestGridLabel = QString("Intersection Arbitraire (%1, %2, %3 m)")
                        .arg(p.X(), 0, 'f', 2)
                        .arg(p.Y(), 0, 'f', 2)
                        .arg(p.Z(), 0, 'f', 2);
                }
            }
        }
    }

    if (foundGridInter)
    {
        outX = bestGridPnt.X();
        outY = bestGridPnt.Y();
        outZ = bestGridPnt.Z();
        outType = bestGridType;
        outDesc = bestGridLabel;
        return true;
    }

    // 3. Détection prioritaire N°3 : Lignes radiales (Cylindrique) et axes de grille (Cartésien)
    double bestLineDist2 = screenPixelRadius * screenPixelRadius;
    bool foundLine = false;
    gp_Pnt bestLinePnt;
    QString bestLineLabel;
    TSA::Grid::GridSnapType bestLineType = TSA::Grid::GridSnapType::None;

    for (const auto* grid : visibleGrids)
    {
        if (grid->cylindrical())
        {
            for (const auto& rad : grid->cylindrical()->radialLines())
            {
                int sx1 = 0, sy1 = 0, sx2 = 0, sy2 = 0;
                m_view->Convert(rad.start.X(), rad.start.Y(), rad.start.Z(), sx1, sy1);
                m_view->Convert(rad.end.X(), rad.end.Y(), rad.end.Z(), sx2, sy2);

                double vLineX = sx2 - sx1;
                double vLineY = sy2 - sy1;
                double len2 = vLineX * vLineX + vLineY * vLineY;
                if (len2 < 1.0) continue;

                double vPtX = px - sx1;
                double vPtY = py - sy1;
                double t = (vPtX * vLineX + vPtY * vLineY) / len2;
                t = std::clamp(t, 0.0, 1.0);

                double projScreenX = sx1 + t * vLineX;
                double projScreenY = sy1 + t * vLineY;
                double d2 = (projScreenX - px) * (projScreenX - px) + (projScreenY - py) * (projScreenY - py);

                if (d2 <= bestLineDist2)
                {
                    bestLineDist2 = d2;
                    gp_Vec v3D(rad.start, rad.end);
                    bestLinePnt = rad.start.Translated(v3D * t);
                    foundLine = true;
                    bestLineType = TSA::Grid::GridSnapType::RadialLine;
                    bestLineLabel = QString("Rayon Polaire %1 (%2°)")
                        .arg(QString::fromStdString(rad.label))
                        .arg(rad.angleDeg, 0, 'f', 1);
                }
            }
        }
        else if (grid->cartesian())
        {
            for (const auto& line : grid->cartesian()->allLines())
            {
                int sx1 = 0, sy1 = 0, sx2 = 0, sy2 = 0;
                m_view->Convert(line.start.X(), line.start.Y(), line.start.Z(), sx1, sy1);
                m_view->Convert(line.end.X(), line.end.Y(), line.end.Z(), sx2, sy2);

                double vLineX = sx2 - sx1;
                double vLineY = sy2 - sy1;
                double len2 = vLineX * vLineX + vLineY * vLineY;
                if (len2 < 1.0) continue;

                double vPtX = px - sx1;
                double vPtY = py - sy1;
                double t = (vPtX * vLineX + vPtY * vLineY) / len2;
                t = std::clamp(t, 0.0, 1.0);

                double projScreenX = sx1 + t * vLineX;
                double projScreenY = sy1 + t * vLineY;
                double d2 = (projScreenX - px) * (projScreenX - px) + (projScreenY - py) * (projScreenY - py);

                if (d2 <= bestLineDist2)
                {
                    bestLineDist2 = d2;
                    gp_Vec v3D(line.start, line.end);
                    bestLinePnt = line.start.Translated(v3D * t);
                    foundLine = true;
                    bestLineType = TSA::Grid::GridSnapType::AxisLine;
                    bestLineLabel = QString("Axe Grille %1: %2")
                        .arg(line.isXAxis ? "X" : "Y")
                        .arg(QString::fromStdString(line.label));
                }
            }
        }
    }

    if (foundLine)
    {
        outX = bestLinePnt.X();
        outY = bestLinePnt.Y();
        outZ = bestLinePnt.Z();
        outType = bestLineType;
        outDesc = bestLineLabel;
        return true;
    }

    // 4. Détection prioritaire N°4 : Arcs concentriques (Cylindrique)
    double bestArcDist2 = screenPixelRadius * screenPixelRadius;
    bool foundArc = false;
    gp_Pnt bestArcPnt;
    QString bestArcLabel;

    constexpr double RAD_TO_DEG_LOCAL = 180.0 / 3.14159265358979323846;

    for (const auto* grid : visibleGrids)
    {
        if (grid->cylindrical())
        {
            const auto& orig = grid->definition().origin();
            double rotDeg = grid->definition().rotationDeg();

            for (const auto& circ : grid->cylindrical()->circles())
            {
                if (circ.radius <= 1e-4) continue;

                if (std::abs(viewDir.Z()) > 1e-6)
                {
                    double tRay = (circ.zLevel - eyePnt.Z()) / viewDir.Z();
                    if (tRay > 0.0)
                    {
                        gp_Pnt planePnt = eyePnt.Translated(gp_Vec(viewDir) * tRay);
                        double dx = planePnt.X() - circ.center.X();
                        double dy = planePnt.Y() - circ.center.Y();
                        double curR = std::hypot(dx, dy);
                        if (curR > 1e-4)
                        {
                            double rad = std::atan2(dy, dx);
                            double angleDeg = rad * RAD_TO_DEG_LOCAL - rotDeg;
                            while (angleDeg < 0.0) angleDeg += 360.0;
                            while (angleDeg >= 360.0) angleDeg -= 360.0;

                            double snapAngle = angleDeg;
                            if (!circ.isFullCircle())
                            {
                                double start = circ.startAngleDeg;
                                while (start < 0.0) start += 360.0;
                                while (start >= 360.0) start -= 360.0;

                                double delta = angleDeg - start;
                                while (delta < 0.0) delta += 360.0;
                                while (delta >= 360.0) delta -= 360.0;

                                if (delta > circ.totalAngleDeg + 1e-4)
                                {
                                    double toStart = 360.0 - delta;
                                    double toEnd = delta - circ.totalAngleDeg;
                                    snapAngle = (toStart < toEnd) ? circ.startAngleDeg : (circ.startAngleDeg + circ.totalAngleDeg);
                                }
                            }

                            gp_Pnt pArc = grid->cylindrical()->polarToWorld(circ.radius, snapAngle, circ.zLevel - orig.Z());
                            int sx = 0, sy = 0;
                            m_view->Convert(pArc.X(), pArc.Y(), pArc.Z(), sx, sy);
                            double d2 = (sx - px) * (sx - px) + (sy - py) * (sy - py);
                            if (d2 <= bestArcDist2)
                            {
                                bestArcDist2 = d2;
                                bestArcPnt = pArc;
                                foundArc = true;
                                bestArcLabel = QString("Arc Polaire %1 (R=%2 m, %3°)")
                                    .arg(QString::fromStdString(circ.label))
                                    .arg(circ.radius, 0, 'f', 2)
                                    .arg(snapAngle, 0, 'f', 1);
                            }
                        }
                    }
                }
            }
        }
    }

    if (foundArc)
    {
        outX = bestArcPnt.X();
        outY = bestArcPnt.Y();
        outZ = bestArcPnt.Z();
        outType = TSA::Grid::GridSnapType::Circle;
        outDesc = bestArcLabel;
        return true;
    }

    return false;
}

bool OccView::getPointUnderCursor(const QPoint& mousePixelPos, double& x, double& y, double& z, int& detectedNodeId)
{
    detectedNodeId = -1;
    m_isCursorSnapped = false;
    if (m_view.IsNull())
        return false;

    const int px = mousePixelPos.x();
    const int py = mousePixelPos.y();

    // 1. Détection 3D sous le curseur (Proximité écran 18px sur nœuds structuraux, intersections, axes ou arcs)
    if (m_snapToObject)
    {
        QString snapDesc;
        TSA::Grid::GridSnapType snapType = TSA::Grid::GridSnapType::None;
        if (findNearest3DPoint(px, py, x, y, z, detectedNodeId, snapDesc, snapType))
        {
            if (m_projectionManager.is2D())
            {
                gp_Pnt pWorld(x, y, z);
                gp_Pnt pProj = m_projectionManager.projectPoint(pWorld, m_workPlane);
                x = pProj.X();
                y = pProj.Y();
                z = pProj.Z();
                if (pWorld.Distance(pProj) > 1e-4)
                {
                    detectedNodeId = -1;
                }
            }
            m_isCursorSnapped = true;
            TSA::Grid::GridSnapResult snapRes;
            snapRes.snapped = true;
            snapRes.point = gp_Pnt(x, y, z);
            snapRes.type = snapType;
            snapRes.description = snapDesc.toStdString();
            m_gridRenderer.showSnapMarker(snapRes, m_context);
            emit objectHovered(snapDesc);
            return true;
        }
    }

    // 2. Si aucun point 3D direct n'est détecté, projection sur le plan de référence actif
    double wx = 0.0, wy = 0.0, wz = 0.0;
    if (!pixelToWorldPlane(px, py, wx, wy, wz))
    {
        m_gridRenderer.hideSnapMarker(m_context);
        return false;
    }

    // 3. Accrochage magnétique aux grilles et au WorkPlane si activé
    if (m_snapToGrid)
    {
        gp_Pnt rawPnt(wx, wy, wz);
        TSA::Grid::GridSnapResult snapRes = m_snapManager.findWorkPlaneSnap(rawPnt, m_workPlane, m_model);
        if (!snapRes.snapped && m_gridSnapManager && m_gridManager)
        {
            snapRes = m_gridSnapManager->findSnap(rawPnt, m_gridManager, m_model);
        }

        if (snapRes.snapped)
        {
            m_isCursorSnapped = true;
            wx = snapRes.point.X();
            wy = snapRes.point.Y();
            wz = snapRes.point.Z();
            if (snapRes.type == TSA::Grid::GridSnapType::Node && m_model)
            {
                for (const auto& [nId, n] : m_model->nodes())
                {
                    if (std::abs(n.x() - wx) < 1e-4 && std::abs(n.y() - wy) < 1e-4 && std::abs(n.z() - wz) < 1e-4)
                    {
                        detectedNodeId = nId;
                        break;
                    }
                }
            }
            m_gridRenderer.showSnapMarker(snapRes, m_context);
            emit objectHovered(QString::fromStdString(snapRes.description));
        }
        else
        {
            m_gridRenderer.hideSnapMarker(m_context);
        }
    }
    else
    {
        m_gridRenderer.hideSnapMarker(m_context);
    }

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

void OccView::buildTransformPreviewGhosts(InteractionMode mode)
{
    // Les fantômes sont construits UNE SEULE FOIS à la position d'origine ;
    // le déplacement/la rotation de l'aperçu est ensuite appliqué par une
    // transformation locale (voir updateTransformPreview).
    const bool isRotation = (mode == InteractionMode::Rotate3D);
    const Quantity_Color barColor = isRotation ? Quantity_NOC_ORANGE : Quantity_NOC_CYAN;

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

    // Nœuds isolés : uniquement pour Déplacer / Copier (comportement inchangé)
    if (!isRotation)
    {
        for (int nId : m_selectionManager->selectedNodes())
        {
            const auto* node = m_model->getNode(nId);
            if (!node) continue;
            addGhost(TSA::Geometry::BeamGeometry::createNodeShape(*node, 0.10), 0.3, barColor);
        }
    }

    m_previewGhostMode = mode;
    m_previewGhostsBuilt = true;
}

void OccView::updateTransformPreview(const gp_Pnt& currentPnt)
{
    if (!m_model || !m_selectionManager || m_context.IsNull())
        return;

    const InteractionMode mode = interactionMode();
    gp_Trsf trsf;

    if (mode == InteractionMode::Move3D || mode == InteractionMode::Copy3D)
    {
        if (!m_hasBasePoint) { clearTransformPreview(); return; }
        gp_Vec delta(m_basePoint3D, currentPnt);
        if (delta.Magnitude() < 1e-4) { clearTransformPreview(); return; }
        trsf.SetTranslation(delta);
    }
    else if (mode == InteractionMode::Rotate3D)
    {
        if (!m_hasCenterPoint || !m_hasBasePoint) { clearTransformPreview(); return; }
        const double a1 = std::atan2(m_basePoint3D.Y() - m_centerPoint3D.Y(),
                                     m_basePoint3D.X() - m_centerPoint3D.X());
        const double a2 = std::atan2(currentPnt.Y() - m_centerPoint3D.Y(),
                                     currentPnt.X() - m_centerPoint3D.X());
        const double angleRad = a2 - a1;
        if (std::abs(angleRad) < 1e-4) { clearTransformPreview(); return; }
        trsf.SetRotation(gp_Ax1(m_centerPoint3D, gp_Dir(0.0, 0.0, 1.0)), angleRad);
    }
    else
    {
        clearTransformPreview();
        return;
    }

    if (!m_previewGhostsBuilt || m_previewGhostMode != mode || m_previewGhostShapes.empty())
    {
        clearTransformPreview();
        buildTransformPreviewGhosts(mode);
    }

    // Mise à jour ciblée : simple transformation locale, sans recréer ni géométrie ni AIS_Shape
    for (auto& ghost : m_previewGhostShapes)
    {
        if (!ghost.IsNull())
            ghost->SetLocalTransformation(trsf);
    }
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
    else if (interactionMode() == InteractionMode::Move3D ||
             interactionMode() == InteractionMode::Copy3D ||
             interactionMode() == InteractionMode::Rotate3D)
    {
        if (m_drawingPoints.empty())
            return;

        const gp_Pnt& pStart = m_drawingPoints.back();
        if (pStart.Distance(currentPnt) < 1e-4)
            return;

        shape = BRepBuilderAPI_MakeEdge(pStart, currentPnt).Edge();
        updateTransformPreview(currentPnt);
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


QImage OccView::captureViewImage(int width, int height)
{
    if (m_view.IsNull())
    {
        return QImage();
    }

    try
    {
        Image_PixMap pixmap;
        if (m_view->ToPixMap(pixmap, width, height, Graphic3d_BT_RGB))
        {
            QImage img(pixmap.Data(), static_cast<int>(pixmap.Width()), static_cast<int>(pixmap.Height()),
                       static_cast<int>(pixmap.SizeRowBytes()), QImage::Format_RGB888);
            return img.copy();
        }
    }
    catch (...)
    {
    }

    return grab().toImage().scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}


