#include "OccView.h"
#include "../Coordinate/AxisColorConfig.h"
#include "AnnotationStyle.h"
#include "BimGeometryRenderer.h"
#include "DimensionRenderer.h"
#include "DisplayModeRenderers.h"
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

#include <QTimer>
#include <QPlatformSurfaceEvent>
#include <OpenGl_Context.hxx>
#include <QWindow>
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
#include <TColStd_IndexedDataMapOfStringString.hxx>
#include <OpenGl_Caps.hxx>
#include "../Diagnostics/Logger.h"
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
    connect(this, &OccView::viewCameraChanged, this, &OccView::refreshWorkPlaneGrid);
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
        if (m_fitModelOnInit) fitModel();
        else fitAll();
        m_fitModelOnInit = false;
    }
    else if (!m_view.IsNull())
    {
        // Vue réaffichée (onglet de document ramené au premier plan) : la fenêtre a pu être redimensionnée pendant que
        // la vue était cachée ; Qt ne redimensionne la fenêtre native qu'à l'affichage, MustBeResized avait donc lu
        // l'ancienne taille (bande noire). Ajustement au tour suivant de la boucle, une fois la taille native appliquée.
        QTimer::singleShot(0, this, [this] {
            if (m_view.IsNull() || !isVisible()) return;
            m_view->MustBeResized();
            m_view->Invalidate();
            m_view->Redraw();
            emit viewCameraChanged();
        });
    }
}

QString OccView::RenderingInfo::summary() const
{
    if (device.isEmpty()) return QStringLiteral("rendu non initialisé");
    return QStringLiteral("%1 — %2, OpenGL %3%4 (%5)%6")
        .arg(vendor, device, version, glsl.isEmpty() ? QString() : QStringLiteral(", GLSL ") + glsl,
             hardware ? QStringLiteral("accélération matérielle") : QStringLiteral("RENDU LOGICIEL : affichage lent sur les grands modèles"),
             compatibilityMode ? QStringLiteral(", mode de compatibilité") : QString());
}

OccView::RenderingInfo OccView::renderingInfo() const
{
    RenderingInfo r;
    if (m_view.IsNull()) return r;
    TColStd_IndexedDataMapOfStringString dict;
    m_view->DiagnosticInformation(dict, Graphic3d_DiagnosticInfo_Device);
    auto get = [&](const char* key) {
        const TCollection_AsciiString k(key);
        return dict.Contains(k) ? QString::fromLatin1(dict.FindFromKey(k).ToCString()) : QString();
    };
    r.vendor = get("GLvendor");
    r.device = get("GLdevice");
    r.version = get("GLversion");
    r.glsl = get("GLSLversion");
    // Rendus logiciels connus : GDI de Windows, Microsoft Basic Render, llvmpipe / softpipe (Mesa), SwiftShader
    const QString d = r.device.toLower();
    r.hardware = !r.device.isEmpty() && !d.contains("gdi generic") && !d.contains("basic render") && !d.contains("llvmpipe") && !d.contains("softpipe") &&
                 !d.contains("swiftshader");
    r.compatibilityMode = !m_graphicDriver.IsNull() && m_graphicDriver->Options().ffpEnable;
    return r;
}

Handle(Aspect_Window) OccView::createAspectWindow()
{
#ifdef _WIN32
    return new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#elif defined(__APPLE__)
    return new Cocoa_Window(reinterpret_cast<NSView*>(winId()));
#else
    return new Xw_Window(m_displayConnection, static_cast<Aspect_Drawable>(winId()));
#endif
}

void OccView::applyViewDecorations()
{
    Quantity_Color topColor = m_isDarkMode ? Quantity_Color(0.12, 0.14, 0.18, Quantity_TOC_RGB)
                                           : Quantity_Color(0.82, 0.88, 0.95, Quantity_TOC_RGB);
    Quantity_Color bottomColor = m_isDarkMode ? Quantity_Color(0.06, 0.08, 0.10, Quantity_TOC_RGB)
                                              : Quantity_Color(0.92, 0.94, 0.98, Quantity_TOC_RGB);
    m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);

    // Configuration explicite des axes du trièdre : X=Rouge, Y=Vert, Z=Bleu
    const auto& axisColors = TSA::Coordinate::AxisColorConfig::instance();
    m_view->ZBufferTriedronSetup(TSA::Viewer::axisScreenColor(axisColors.axisXColor()), TSA::Viewer::axisScreenColor(axisColors.axisYColor()),
                                 TSA::Viewer::axisScreenColor(axisColors.axisZColor()), 0.8, 0.05, 12);
    displayCornerIndicator();
}

bool OccView::contextHasCore20() const
{
    // core20fwd n'est rempli par OCCT que si les fonctions OpenGL 2.0 ont réellement été chargées (OpenGl_GlFunctions :
    // version ≥ 2.0 ET points d'entrée trouvés), pas d'après la seule version déclarée. Contexte absent ou invalide :
    // repli par sécurité.
    if (m_graphicDriver.IsNull()) return false;
    const Handle(OpenGl_Context)& ctx = m_graphicDriver->GetSharedContext();
    return !ctx.IsNull() && ctx->IsValid() && ctx->core20fwd != nullptr;
}

void OccView::setTextBackgroundSupported(bool supported)
{
    if (supported == m_textBackgroundSupported) return;   // contexte OpenGL ≥ 2.0 : rien ne change
    m_textBackgroundSupported = supported;
    m_gridRenderer.setSnapLabelBackground(supported, m_context);
    if (m_dimensionRenderer)
    {
        m_dimensionRenderer->setTextBackground(supported);
        if (m_isInitialized && m_model && !m_context.IsNull()) m_dimensionRenderer->rebuildAll(*m_model);   // à l'initialisation : rebuildAllShapes suit
    }
    if (!supported)
        TSA_LOG_WARN("Viewer", "TextBackground",
                     "Contexte OpenGL sans fonctions OpenGL 2.0 (core20fwd) : textes sur fond affichés entourés (BUG-122)");
}

bool OccView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::PlatformSurface && watched == windowHandle())
    {
        switch (static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType())
        {
        case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed: releaseViewWindow(); break;
        case QPlatformSurfaceEvent::SurfaceCreated: restoreViewWindow(); break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void OccView::releaseViewWindow()
{
    // La fenêtre native existe encore : la vue OCCT (OpenGl_View, OpenGl_Window et son contexte) est détruite ici, et
    // non plus après la fenêtre (destruction par le widget parent). Le visualiseur et le contexte interactif (objets
    // affichés) sont conservés ; m_view nul = état « pas encore affichée », déjà géré partout.
    if (m_view.IsNull() || m_viewWindowReleased) return;
    m_releasedCamera = new Graphic3d_Camera();
    m_releasedCamera->Copy(m_view->Camera());
    m_view->Remove();
    m_view.Nullify();
    m_viewWindowReleased = true;
}

void OccView::restoreViewWindow()
{
    // Surface recréée (fenêtre parente refermée puis rouverte) : nouvelle vue du même visualiseur, sur la nouvelle
    // fenêtre native, avec la caméra, le décor et le plan de coupe d'avant.
    if (!m_viewWindowReleased || m_viewer.IsNull()) return;
    m_view = m_viewer->CreateView();
    Handle(Aspect_Window) wind = createAspectWindow();
    m_view->SetWindow(wind);
    if (!wind->IsMapped()) wind->Map();
    // Nouveau contexte OpenGL (la surface peut être recréée sous un autre pilote : bureau à distance), avant tout tracé
    setTextBackgroundSupported(contextHasCore20());
    applyViewDecorations();
    if (!m_releasedCamera.IsNull()) m_view->Camera()->Copy(m_releasedCamera);
    if (!m_clipPlane.IsNull() && m_clipPlane->IsOn()) m_view->AddClipPlane(m_clipPlane);
    m_view->MustBeResized();
    m_viewWindowReleased = false;
    m_view->Redraw();
}

void OccView::initOcc()
{
    m_displayConnection = new Aspect_DisplayConnection();
    m_graphicDriver = new OpenGl_GraphicDriver(m_displayConnection);
    // Mode de compatibilité pour les cartes graphiques anciennes ou les pilotes défaillants (TSA_RENDER_COMPAT=1) :
    // contexte OpenGL de compatibilité et pipeline fixe, au lieu du profil moderne par défaut.
    if (qEnvironmentVariableIntValue("TSA_RENDER_COMPAT") == 1)
    {
        m_graphicDriver->ChangeOptions().contextCompatible = true;
        m_graphicDriver->ChangeOptions().ffpEnable = true;
    }

    m_viewer = new V3d_Viewer(m_graphicDriver);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();

    // Plan privilégié horizontal (XY) à Z=0 avec normale dirigée vers +Z
    m_viewer->SetPrivilegedPlane(gp_Ax3(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0)));

    m_context = new AIS_InteractiveContext(m_viewer);
    m_context->SetPixelTolerance(8);
    m_dimensionRenderer = std::make_unique<TSA::Viewer::DimensionRenderer>(m_context, m_selectionManager);
    m_analyticalRenderer = std::make_unique<TSA::Viewer::AnalyticalModelRenderer>(m_context);
    m_meshRenderer = std::make_unique<TSA::Viewer::FiniteElementMeshRenderer>(m_context);
    m_bimRenderer = std::make_unique<TSA::Viewer::BimGeometryRenderer>(m_context);

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

    Handle(Aspect_Window) wind = createAspectWindow();
    m_view->SetWindow(wind);
    if (!wind->IsMapped())
    {
        wind->Map();
    }
    if (QWindow* native = windowHandle()) native->installEventFilter(this);   // destruction de la surface (BUG-119)
    // Le contexte OpenGL est initialisé par SetWindow : ses tables de fonctions sont connues (BUG-122)
    setTextBackgroundSupported(contextHasCore20());
    // Annotations : facteur d'échelle de l'écran (DPI) ; reconstruction des textes à chaque changement de réglage
    // (jamais au mouvement de la caméra : les textes constants à l'écran sont dessinés en pixels par OCCT)
    TSA::Viewer::AnnotationStyle::instance().setDevicePixelRatio(devicePixelRatioF());
    connect(&TSA::Viewer::AnnotationStyle::instance(), &TSA::Viewer::AnnotationStyle::changed, this, [this]() {
        if (m_context.IsNull()) return;
        rebuildAllShapes();
        rebuildGrid();
        if (m_resultsVisual) m_resultsVisual->updateAllVisuals();
    });
    // Rendu réellement obtenu, journalisé (diagnostic des postes sans accélération matérielle)
    TSA_LOG_INFO("Viewer", "OpenGLRenderer", renderingInfo().summary().toStdString());

    applyViewDecorations();

    // Orientation standard génie civil : +Z vers le haut (élévation), projection axonométrique droite Z-up
    TSA::Viewer::ViewManager::applyFrame(m_view, TSA::Viewer::ViewManager::standardFrame(TSA::Viewer::StandardCameraView::ThreeD_Axo),
                                         gp_Pnt(0.0, 0.0, 0.0));
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

    // Libellés conformes à Robot Structural Analysis, dans la langue de l'interface. OCCT ne les accepte qu'en ASCII
    // (TCollection_AsciiString) : les traductions de ces six mots restent sans accent.
    const auto sideLabel = [](const QString& text) { return TCollection_AsciiString(text.toLatin1().constData()); };
    m_viewCube->SetBoxSideLabel(V3d_Zpos, sideLabel(tr("HAUT", "face du ViewCube, ASCII")));
    m_viewCube->SetBoxSideLabel(V3d_Zneg, sideLabel(tr("BAS", "face du ViewCube, ASCII")));
    m_viewCube->SetBoxSideLabel(V3d_Ypos, sideLabel(tr("ARRIERE", "face du ViewCube, ASCII")));
    m_viewCube->SetBoxSideLabel(V3d_Yneg, sideLabel(tr("AVANT", "face du ViewCube, ASCII")));
    m_viewCube->SetBoxSideLabel(V3d_Xpos, sideLabel(tr("DROITE", "face du ViewCube, ASCII")));
    m_viewCube->SetBoxSideLabel(V3d_Xneg, sideLabel(tr("GAUCHE", "face du ViewCube, ASCII")));

    m_viewCube->SetTransformPersistence(new Graphic3d_TransformPers(
        Graphic3d_TMF_TriedronPers,
        Aspect_TOTP_RIGHT_UPPER,
        NCollection_Vec2<int>(85, 85)
    ));

    m_context->Display(m_viewCube, false);
    initAxisDisplay();
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
    m_lastResults = results;
    if (TSA::Viewer::DisplayPolicy::solverMesh(m_modelDisplayMode)) refreshDisplayModeOverlays();
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
            updateSelectedElementLocalAxes();
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
        {
            // Échap (raccourci « Mode sélection » de la fenêtre, ou touche reçue par la vue) pendant un glisser du
            // manipulateur : le mode Sélection est déjà actif, mais le glisser doit être annulé
            if (m_isManipulatingWorkPlane)
                cancelCurrentDrawing();
            return;
        }

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
    if (m_drawingPoints.size() < 3 || !m_model) return;

    const auto slab = resolvedPresets().slab;
    const auto r = TSA::Interaction::ElementCreation(*m_model).createSlab(drawingPicks(), slab);
    clearDrawing();
    if (!r.success)
    {
        // Contour refusé (aligné, non plan, croisé, doublon) : rien n'a été créé, le tracé repart de zéro.
        emit slabDrawingCancelled();
        reportCreation(r, {});
        return;
    }
    updateSlabShape(r.elementId);
    emit slabCreated(r.elementId);
    reportCreation(r, tr("Dalle S%1 créée (%2 nœuds, ép=%3m). Cliquez pour une nouvelle dalle").arg(r.elementId).arg(r.nodeIds.size()).arg(slab.thickness));
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
        // Le panneau revient à sa position de départ : le plan (et sa grille) aussi
        m_workPlane = m_manipulatorStartWp;
        applyWorkPlaneTransformation();
        emit workPlaneChanged(m_workPlane);
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
        emit barDrawingCancelled();   // le dialogue « Barre » sert aussi aux poutres : origine effacée
        emit drawingPromptChanged(tr("Mode Dessin Poutre : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawColumn:
        emit barDrawingCancelled();
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
            // Pas de la grille affichée (multiple du pas du plan) : on accroche ce que l'on voit ; tolérance
            // proportionnelle au pas (0,5 m pour le pas de 1 m d'origine)
            TSA::Coordinate::WorkPlane snapPlane = m_workPlane;
            const double baseTol = m_snapManager.snapTolerance();
            double tol = baseTol;
            if (!m_workPlaneGrid.IsNull() && m_workPlane.gridSpacingX() > 1e-9)
            {
                const auto& g = m_workPlaneGrid->layout();
                snapPlane.setGridSpacingX(g.stepU);
                snapPlane.setGridSpacingY(g.stepV);
                tol = baseTol * g.stepU / m_workPlane.gridSpacingX();
            }
            m_snapManager.setSnapTolerance(tol);
            snap = m_snapManager.snapToWorkPlaneGrid(gp_Pnt(wx, wy, wz), snapPlane);
            m_snapManager.setSnapTolerance(baseTol);
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

TSA::Interaction::DrawPoint OccView::drawPointAt(double x, double y, double z, int detectedNodeId) const
{
    TSA::Interaction::DrawPoint dp(gp_Pnt(x, y, z), detectedNodeId);
    if (!m_model) return dp;
    const TSA::Interaction::ElementCreation creation(*m_model);
    dp.nodeId = creation.existingNode(dp);
    dp.point = creation.position(dp);
    return dp;
}

void OccView::pushDrawPoint(const TSA::Interaction::DrawPoint& p)
{
    m_drawingNodeIds.push_back(p.nodeId);
    m_drawingPoints.push_back(p.point);
}

std::vector<TSA::Interaction::DrawPoint> OccView::drawingPicks() const
{
    std::vector<TSA::Interaction::DrawPoint> picks;
    for (size_t i = 0; i < m_drawingPoints.size(); ++i)
        picks.emplace_back(m_drawingPoints[i], i < m_drawingNodeIds.size() ? m_drawingNodeIds[i] : -1);
    return picks;
}

void OccView::clearDrawing()
{
    clearRubberBand();
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();
}

QString OccView::pointLabel(const TSA::Interaction::DrawPoint& p) const
{
    if (p.nodeId > 0) return tr("N%1").arg(p.nodeId);
    return tr("(%1; %2; %3)").arg(p.point.X(), 0, 'f', 2).arg(p.point.Y(), 0, 'f', 2).arg(p.point.Z(), 0, 'f', 2);
}

TSA::Model::StructurePresets OccView::resolvedPresets() const
{
    using TSA::Model::Material;
    auto p = m_presets;
    p.beam.material = Material::findByName(p.beam.material.name);
    p.column.material = Material::findByName(p.column.material.name);
    p.slab.material = Material::findByName(p.slab.material.name);
    p.wall.material = Material::findByName(p.wall.material.name);
    p.foundation.material = Material::findByName(p.foundation.material.name);
    p.truss.material = Material::findByName(p.truss.material.name);
    return p;
}

TSA::Model::BarProperties OccView::drawingBarProperties(TSA::Model::BarRole role) const
{
    const bool column = role == TSA::Model::BarRole::Column;
    const auto presets = resolvedPresets();
    TSA::Model::BarProperties props;
    props.id = 0;   // identifiant attribué par le modèle
    props.role = role;
    // Réglages du dialogue « Barre » s'ils sont définis, sinon préréglages poutre / poteau.
    props.section = (m_currentBarProps.section.width > 0.0 || m_currentBarProps.section.diameter > 0.0)
        ? m_currentBarProps.section : (column ? presets.column.section : presets.beam.section);
    props.material = (m_currentBarProps.material.E > 0.0)
        ? m_currentBarProps.material : (column ? presets.column.material : presets.beam.material);
    props.rotation = (m_currentBarProps.rotation != 0.0)
        ? m_currentBarProps.rotation : (column ? presets.column.betaAngle : presets.beam.betaAngle);
    props.color = !m_currentBarProps.color.empty() ? m_currentBarProps.color : (column ? presets.column.color : presets.beam.color);
    props.name = props.section.name;
    return props;
}

void OccView::reportCreation(const TSA::Interaction::CreationResult& r, const QString& successPrompt)
{
    if (!r.success)
    {
        emit drawingPromptChanged(tr("Non créé : %1").arg(QString::fromStdString(r.message)));
        return;
    }
    emit elementCreated();
    QString prompt = successPrompt;
    for (const auto& w : r.warnings)
        prompt += tr(" — Attention : %1").arg(QString::fromStdString(w));
    emit drawingPromptChanged(prompt);
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
    const bool globalShown = hideNavigationAids && !m_globalAxisTriad.IsNull() && !m_context.IsNull() && m_context->IsDisplayed(m_globalAxisTriad);
    const bool localShown = hideNavigationAids && !m_localAxisFrames.empty();
    if (globalShown) showGlobalAxisTriad(false);
    if (localShown) clearSelectedElementLocalAxes();
    auto restoreAids = [&] {
        if (!hideNavigationAids) return;
        displayCornerIndicator();
        if (globalShown) showGlobalAxisTriad(true);
        if (localShown) updateSelectedElementLocalAxes();
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


