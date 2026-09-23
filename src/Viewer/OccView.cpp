#include "OccView.h"
#include "SelectionManager.h"
#include "../Model/Model.h"
#include "../Geometry/BeamGeometry.h"
#include "../Geometry/SlabGeometry.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

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
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <SelectMgr_ViewerSelector.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <cmath>

OccView::OccView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

OccView::~OccView()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void OccView::onNodeAdded(const TSA::Model::Node& node)
{
    updateNodeShape(node.id());
}

void OccView::onNodeModified(const TSA::Model::Node& node)
{
    updateNodeShape(node.id());
}

void OccView::onNodeRemoved(int nodeId)
{
    removeNodeShape(nodeId);
}

void OccView::onBeamAdded(const TSA::Model::Beam& beam)
{
    updateBeamShape(beam.id());
}

void OccView::onBeamModified(const TSA::Model::Beam& beam)
{
    updateBeamShape(beam.id());
}

void OccView::onBeamRemoved(int beamId)
{
    removeBeamShape(beamId);
}

void OccView::onColumnAdded(const TSA::Model::Column& column)
{
    updateColumnShape(column.id());
}

void OccView::onColumnModified(const TSA::Model::Column& column)
{
    updateColumnShape(column.id());
}

void OccView::onColumnRemoved(int columnId)
{
    removeColumnShape(columnId);
}

void OccView::onSlabAdded(const TSA::Model::Slab& slab)
{
    updateSlabShape(slab.id());
}

void OccView::onSlabModified(const TSA::Model::Slab& slab)
{
    updateSlabShape(slab.id());
}

void OccView::onSlabRemoved(int slabId)
{
    removeSlabShape(slabId);
}

void OccView::onModelCleared()
{
    rebuildAllShapes();
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

    setCursor(Qt::CrossCursor);

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

    Quantity_Color topColor(0.18, 0.22, 0.28, Quantity_TOC_RGB);   // Dark AutoCAD / Steel charcoal
    Quantity_Color bottomColor(0.08, 0.10, 0.13, Quantity_TOC_RGB); // Dark deep viewport
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
    m_viewCube->SetBoxColor(Quantity_Color(0.92, 0.94, 0.96, Quantity_TOC_RGB));
    m_viewCube->SetInnerColor(Quantity_Color(0.85, 0.88, 0.92, Quantity_TOC_RGB));
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
    if (m_model)
    {
        m_model->addObserver(this);
    }
    if (m_isInitialized)
    {
        rebuildAllShapes();
    }
}

void OccView::setSelectionManager(TSA::Viewer::SelectionManager* selectionManager)
{
    m_selectionManager = selectionManager;
}

void OccView::highlightNode(int nodeId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightBeam(int beamId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightColumn(int columnId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightSlab(int slabId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::clearHighlight()
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::rebuildAllShapes()
{
    if (m_context.IsNull())
        return;

    // Nettoyer tous les objets existants
    for (auto& [id, aisShape] : m_nodeShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_nodeShapes.clear();

    for (auto& [id, aisShape] : m_beamShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_beamShapes.clear();

    for (auto& [id, aisShape] : m_columnShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_columnShapes.clear();

    for (auto& [id, aisShape] : m_slabShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_slabShapes.clear();

    if (m_selectionManager)
    {
        m_selectionManager->clearRegistry();
    }

    if (!m_model)
        return;

    // 1. Créer les formes des nœuds
    for (const auto& [nodeId, node] : m_model->nodes())
    {
        updateNodeShape(nodeId);
    }

    // 2. Créer les formes des poutres
    for (const auto& [beamId, beam] : m_model->beams())
    {
        updateBeamShape(beamId);
    }

    // 3. Créer les formes des poteaux
    for (const auto& [columnId, col] : m_model->columns())
    {
        updateColumnShape(columnId);
    }

    // 4. Créer les formes des dalles
    for (const auto& [slabId, slab] : m_model->slabs())
    {
        updateSlabShape(slabId);
    }

    m_context->UpdateCurrentViewer();
    fitAll();
}

void OccView::updateNodeShape(int nodeId)
{
    if (m_context.IsNull() || !m_model)
        return;

    const auto* node = m_model->getNode(nodeId);
    if (!node)
        return;

    // 1. Supprimer l'ancienne forme
    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        m_context->Remove(it->second, false);
        m_nodeShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterNode(nodeId);
        }
    }

    // 2. Créer la nouvelle forme 3D
    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createNodeShape(*node, 0.12);
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisNode = new AIS_Shape(shape);
        aisNode->SetColor(Quantity_NOC_GOLD);
        aisNode->SetMaterial(Graphic3d_NOM_COPPER);
        aisNode->SetDisplayMode(AIS_Shaded);

        m_context->Display(aisNode, false);
        m_nodeShapes[nodeId] = aisNode;
        if (m_selectionManager)
        {
            m_selectionManager->registerNode(nodeId, aisNode);
        }
    }

    // 3. Mettre à jour aussi toutes les barres et dalles reliées à ce nœud
    for (const auto& [beamId, beam] : m_model->beams())
    {
        if (beam.startNodeId() == nodeId || beam.endNodeId() == nodeId)
        {
            updateBeamShape(beamId);
        }
    }

    for (const auto& [colId, col] : m_model->columns())
    {
        if (col.startNodeId() == nodeId || col.endNodeId() == nodeId)
        {
            updateColumnShape(colId);
        }
    }

    for (const auto& [slabId, slab] : m_model->slabs())
    {
        const auto& nids = slab.nodeIds();
        if (std::find(nids.begin(), nids.end(), nodeId) != nids.end())
        {
            updateSlabShape(slabId);
        }
    }

    // 4. Actualiser immédiatement l'affichage 3D OpenCASCADE
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::updateBeamShape(int beamId)
{
    if (m_context.IsNull() || !m_model)
        return;

    const auto* beam = m_model->getBeam(beamId);
    if (!beam)
        return;

    const auto* nodeA = m_model->getNode(beam->startNodeId());
    const auto* nodeB = m_model->getNode(beam->endNodeId());
    if (!nodeA || !nodeB)
        return;

    // 1. Supprimer l'ancienne forme
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        m_context->Remove(it->second, false);
        m_beamShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterBeam(beamId);
        }
    }

    // 2. Créer le nouveau solide 3D
    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createBeamShape(
        *nodeA, *nodeB, beam->width(), beam->height()
    );

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisBeam = new AIS_Shape(shape);
        aisBeam->SetColor(Quantity_NOC_STEELBLUE);
        aisBeam->SetMaterial(Graphic3d_NOM_STEEL);
        aisBeam->SetDisplayMode(AIS_Shaded);

        m_context->Display(aisBeam, false);
        m_beamShapes[beamId] = aisBeam;
        if (m_selectionManager)
        {
            m_selectionManager->registerBeam(beamId, aisBeam);
        }
    }

    // 3. Actualiser immédiatement l'affichage 3D
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::updateColumnShape(int columnId)
{
    if (m_context.IsNull() || !m_model)
        return;

    const auto* col = m_model->getColumn(columnId);
    if (!col)
        return;

    const auto* nodeA = m_model->getNode(col->startNodeId());
    const auto* nodeB = m_model->getNode(col->endNodeId());
    if (!nodeA || !nodeB)
        return;

    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        m_context->Remove(it->second, false);
        m_columnShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterColumn(columnId);
        }
    }

    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createBeamShape(
        *nodeA, *nodeB, col->width(), col->height()
    );

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisCol = new AIS_Shape(shape);
        aisCol->SetColor(Quantity_NOC_DARKSLATEBLUE);
        aisCol->SetMaterial(Graphic3d_NOM_STONE);
        aisCol->SetDisplayMode(AIS_Shaded);

        m_context->Display(aisCol, false);
        m_columnShapes[columnId] = aisCol;
        if (m_selectionManager)
        {
            m_selectionManager->registerColumn(columnId, aisCol);
        }
    }

    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::updateSlabShape(int slabId)
{
    if (m_context.IsNull() || !m_model)
        return;

    const auto* slab = m_model->getSlab(slabId);
    if (!slab)
        return;

    std::vector<const TSA::Model::Node*> contourNodes;
    for (int nid : slab->nodeIds())
    {
        const auto* n = m_model->getNode(nid);
        if (n)
        {
            contourNodes.push_back(n);
        }
    }

    if (contourNodes.size() < 3)
        return;

    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        m_context->Remove(it->second, false);
        m_slabShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSlab(slabId);
        }
    }

    TopoDS_Shape shape = TSA::Geometry::SlabGeometry::createSlabShape(contourNodes, slab->thickness());

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisSlab = new AIS_Shape(shape);
        aisSlab->SetColor(Quantity_NOC_GRAY70);
        aisSlab->SetMaterial(Graphic3d_NOM_STONE);
        aisSlab->SetDisplayMode(AIS_Shaded);
        aisSlab->SetTransparency(0.35f);

        m_context->Display(aisSlab, false);
        m_slabShapes[slabId] = aisSlab;
        if (m_selectionManager)
        {
            m_selectionManager->registerSlab(slabId, aisSlab);
        }
    }

    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeNodeShape(int nodeId)
{
    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            m_context->UpdateCurrentViewer();
        }
        m_nodeShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterNode(nodeId);
        }
    }

    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeBeamShape(int beamId)
{
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            m_context->UpdateCurrentViewer();
        }
        m_beamShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterBeam(beamId);
        }
    }

    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeColumnShape(int columnId)
{
    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            m_context->UpdateCurrentViewer();
        }
        m_columnShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterColumn(columnId);
        }
    }

    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeSlabShape(int slabId)
{
    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            m_context->UpdateCurrentViewer();
        }
        m_slabShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSlab(slabId);
        }
    }

    if (!m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
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

void OccView::resetView()
{
    if (!m_view.IsNull())
    {
        m_view->SetUp(0.0, 0.0, 1.0);
        m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
        fitAll();
    }
}

bool OccView::pixelToWorldPlane(int px, int py, double& wx, double& wy, double& wz) const
{
    if (m_view.IsNull())
        return false;

    double xEye = 0.0, yEye = 0.0, zEye = 0.0;
    double xDir = 0.0, yDir = 0.0, zDir = 0.0;
    m_view->ConvertWithProj(px, py, xEye, yEye, zEye, xDir, yDir, zDir);

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
        m_view->Redraw();
    }
    emit clippingChanged(m_isClippingEnabled, m_clipAxisIndex, m_clipPosition, m_isClipFlipped);
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
    if (m_view.IsNull())
        return;

    if (enabled)
    {
        Quantity_Color topColor(0.82, 0.88, 0.95, Quantity_TOC_RGB);
        Quantity_Color bottomColor(0.92, 0.94, 0.98, Quantity_TOC_RGB);
        m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);
    }
    else
    {
        Quantity_Color topColor(0.18, 0.22, 0.28, Quantity_TOC_RGB);
        Quantity_Color bottomColor(0.08, 0.10, 0.13, Quantity_TOC_RGB);
        m_view->SetBgGradientColors(topColor, bottomColor, Aspect_GFM_VER);
    }
    m_view->Redraw();
}

void OccView::setGridManager(TSA::Grid::GridManager* gridManager, TSA::Grid::GridSnapManager* snapManager)
{
    m_gridManager = gridManager;
    m_gridSnapManager = snapManager;
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
    const TSA::Grid::GridSystem* grid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    m_gridRenderer.setActiveLevelElevation(z, grid, m_context);
    if (!m_view.IsNull())
    {
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

void OccView::setInteractionMode(InteractionMode mode)
{
    if (m_interactionMode == mode)
        return;

    cancelCurrentDrawing();
    m_interactionMode = mode;

    switch (m_interactionMode)
    {
    case InteractionMode::Select:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Sélection actif"));
        break;
    case InteractionMode::DrawNode:
        setCursor(Qt::CrossCursor);
        emit drawingPromptChanged(tr("Mode Dessin Nœud : Cliquez dans le viewport pour créer un nœud"));
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
    }

    emit interactionModeChanged(m_interactionMode);
}

void OccView::cancelCurrentDrawing()
{
    clearRubberBand();
    m_drawingNodeIds.clear();
    m_drawingPoints.clear();

    switch (m_interactionMode)
    {
    case InteractionMode::DrawNode:
        emit drawingPromptChanged(tr("Mode Dessin Nœud : Cliquez dans le viewport pour créer un nœud"));
        break;
    case InteractionMode::DrawBeam:
        emit drawingPromptChanged(tr("Mode Dessin Poutre : Cliquez pour sélectionner ou créer le 1er nœud"));
        break;
    case InteractionMode::DrawColumn:
        emit drawingPromptChanged(tr("Mode Dessin Poteau : Cliquez pour définir la base du poteau"));
        break;
    case InteractionMode::DrawSlab:
        emit drawingPromptChanged(tr("Mode Dessin Dalle : Cliquez les nœuds du contour polygonal (Clic droit ou Entrée pour valider)"));
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

    // 2. Détection prioritaire N°2 : Intersections 3D de la grille (tous étages et montants verticaux)
    const TSA::Grid::GridSystem* activeGrid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
    if (activeGrid && activeGrid->isActive() && activeGrid->isVisible())
    {
        double bestGridDist2 = screenPixelRadius * screenPixelRadius;
        bool foundGridInter = false;
        gp_Pnt bestGridPnt;
        QString bestGridLabel;

        if (activeGrid->cartesian())
        {
            for (const auto& inter : activeGrid->cartesian()->intersections())
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
                    bestGridLabel = QString("Grille (%1, %2, Z=%3 m)")
                        .arg(QString::fromStdString(inter.labelX))
                        .arg(QString::fromStdString(inter.labelY))
                        .arg(p.Z(), 0, 'f', 2);
                    foundGridInter = true;
                }
            }
        }
        else if (activeGrid->cylindrical())
        {
            for (const auto& inter : activeGrid->cylindrical()->intersections())
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
                    bestGridLabel = QString("Grille Cylindrique (R=%1, %2°)")
                        .arg(p.Distance(gp_Pnt(0, 0, p.Z())), 0, 'f', 2)
                        .arg(inter.angleDeg, 0, 'f', 1);
                    foundGridInter = true;
                }
            }
        }

        if (foundGridInter)
        {
            outX = bestGridPnt.X();
            outY = bestGridPnt.Y();
            outZ = bestGridPnt.Z();
            outType = TSA::Grid::GridSnapType::Intersection;
            outDesc = bestGridLabel;
            return true;
        }
    }

    return false;
}

bool OccView::getPointUnderCursor(const QPoint& mousePixelPos, double& x, double& y, double& z, int& detectedNodeId)
{
    detectedNodeId = -1;
    if (m_view.IsNull())
        return false;

    const int px = mousePixelPos.x();
    const int py = mousePixelPos.y();

    // 1. Détection 3D sous le curseur (Proximité écran 18px sur nœuds structuraux ou intersections 3D de grille)
    if (m_snapToGrid)
    {
        QString snapDesc;
        TSA::Grid::GridSnapType snapType = TSA::Grid::GridSnapType::None;
        if (findNearest3DPoint(px, py, x, y, z, detectedNodeId, snapDesc, snapType))
        {
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

    // 3. Accrochage magnétique à la grille du plan si activé
    if (m_snapToGrid && m_gridSnapManager)
    {
        gp_Pnt rawPnt(wx, wy, wz);
        const TSA::Grid::GridSystem* activeGrid = m_gridManager ? m_gridManager->activeGrid() : nullptr;
        TSA::Grid::GridSnapResult snapRes = m_gridSnapManager->findSnap(rawPnt, activeGrid, m_model);
        if (snapRes.snapped)
        {
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

void OccView::clearRubberBand()
{
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

    if (m_interactionMode == InteractionMode::DrawBeam || m_interactionMode == InteractionMode::DrawColumn)
    {
        if (m_drawingPoints.empty())
            return;

        const gp_Pnt& pStart = m_drawingPoints.back();
        if (pStart.Distance(currentPnt) < 1e-4)
            return;

        shape = BRepBuilderAPI_MakeEdge(pStart, currentPnt).Edge();
    }
    else if (m_interactionMode == InteractionMode::DrawSlab)
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
        m_rubberBandShape->SetWidth(2.5);
        m_context->Display(m_rubberBandShape, false);
    }
    else
    {
        m_rubberBandShape->SetShape(shape);
        m_context->Redisplay(m_rubberBandShape, false);
    }

    m_view->Redraw();
}

void OccView::mousePressEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();
    m_lastMousePos = p;
    m_pressMousePos = p;
    m_dragStartPos = p;

    if (event->button() == Qt::LeftButton)
    {
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
                        m_viewCube->HandleClick(cubeOwner);
                        m_view->Redraw();
                        emit viewCameraChanged();
                        return;
                    }
                }
            }
        }

        if (m_interactionMode == InteractionMode::Select)
        {
            // En mode sélection, on attend le mouvement pour distinguer un clic d'un glissé fenêtre/capture
            m_currentAction = CurrentAction::Nothing;
        }
        else if (m_interactionMode == InteractionMode::DrawNode)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                int newId = getOrCreateNode(wx, wy, wz, detectedId);
                emit drawingPromptChanged(tr("Nœud N%1 créé en (X = %2 m, Y = %3 m, Z = %4 m)")
                    .arg(newId)
                    .arg(wx, 0, 'f', 3)
                    .arg(wy, 0, 'f', 3)
                    .arg(wz, 0, 'f', 3));
            }
        }
        else if (m_interactionMode == InteractionMode::DrawBeam)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                int nodeId = getOrCreateNode(wx, wy, wz, detectedId);
                if (m_drawingNodeIds.empty())
                {
                    m_drawingNodeIds.push_back(nodeId);
                    const auto* node = m_model->getNode(nodeId);
                    if (node)
                    {
                        m_drawingPoints.push_back(gp_Pnt(node->x(), node->y(), node->z()));
                    }
                    emit drawingPromptChanged(tr("Mode Dessin Poutre : 1er nœud N%1 sélectionné. Cliquez pour le 2nd nœud (Échap pour annuler)").arg(nodeId));
                }
                else
                {
                    int startId = m_drawingNodeIds[0];
                    int endId = nodeId;
                    if (startId != endId)
                    {
                        int beamId = m_model->addBeam(startId, endId, 0.30, 0.50);
                        emit drawingPromptChanged(tr("Poutre B%1 créée reliant N%2 à N%3. Cliquez pour tracer une autre poutre").arg(beamId).arg(startId).arg(endId));
                    }
                    clearRubberBand();
                    m_drawingNodeIds.clear();
                    m_drawingPoints.clear();
                }
            }
        }
        else if (m_interactionMode == InteractionMode::DrawColumn)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                int nodeId = getOrCreateNode(wx, wy, wz, detectedId);
                if (m_drawingNodeIds.empty())
                {
                    m_drawingNodeIds.push_back(nodeId);
                    const auto* node = m_model->getNode(nodeId);
                    if (node)
                    {
                        m_drawingPoints.push_back(gp_Pnt(node->x(), node->y(), node->z()));
                    }
                    emit drawingPromptChanged(tr("Mode Dessin Poteau : Base N%1 définie. Cliquez pour le nœud sommital").arg(nodeId));
                }
                else
                {
                    int startId = m_drawingNodeIds[0];
                    int endId = nodeId;
                    if (startId == endId)
                    {
                        // Si l'utilisateur reclique au même endroit, crée automatiquement un nœud 3m au-dessus
                        const auto* n = m_model->getNode(startId);
                        if (n)
                        {
                            endId = getOrCreateNode(n->x(), n->y(), n->z() + 3.0, -1);
                        }
                    }
                    if (startId != endId)
                    {
                        int colId = m_model->addColumn(startId, endId, 0.30, 0.30);
                        emit drawingPromptChanged(tr("Poteau C%1 créé reliant N%2 à N%3. Cliquez pour un autre poteau").arg(colId).arg(startId).arg(endId));
                    }
                    clearRubberBand();
                    m_drawingNodeIds.clear();
                    m_drawingPoints.clear();
                }
            }
        }
        else if (m_interactionMode == InteractionMode::DrawSlab)
        {
            double wx = 0.0, wy = 0.0, wz = 0.0;
            int detectedId = -1;
            if (getPointUnderCursor(p, wx, wy, wz, detectedId) && m_model)
            {
                int nodeId = getOrCreateNode(wx, wy, wz, detectedId);

                // Si clic sur le 1er nœud pour fermer le polygone
                if (!m_drawingNodeIds.empty() && nodeId == m_drawingNodeIds.front() && m_drawingNodeIds.size() >= 3)
                {
                    int slabId = m_model->addSlab(m_drawingNodeIds, 0.20);
                    emit drawingPromptChanged(tr("Dalle S%1 créée (%2 nœuds). Cliquez pour une nouvelle dalle").arg(slabId).arg(m_drawingNodeIds.size()));
                    clearRubberBand();
                    m_drawingNodeIds.clear();
                    m_drawingPoints.clear();
                }
                else
                {
                    m_drawingNodeIds.push_back(nodeId);
                    const auto* node = m_model->getNode(nodeId);
                    if (node)
                    {
                        m_drawingPoints.push_back(gp_Pnt(node->x(), node->y(), node->z()));
                    }
                    emit drawingPromptChanged(tr("Dalle : Nœud N%1 ajouté (total : %2 nœuds). Cliquez pour ajouter, ou fermez sur N%3 / Clic droit")
                        .arg(nodeId)
                        .arg(m_drawingNodeIds.size())
                        .arg(m_drawingNodeIds.front()));
                }
            }
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        m_currentAction = CurrentAction::Rotation;
        m_view->StartRotation(px, py);
    }
    else if (event->button() == Qt::MiddleButton)
    {
        m_currentAction = CurrentAction::Pan;
    }
}

void OccView::mouseReleaseEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();

    if (event->button() == Qt::LeftButton)
    {
        if (m_interactionMode == InteractionMode::Select)
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

                    if (m_selectionManager)
                    {
                        m_selectionManager->setMultipleObjectsSelected(selectedObjs, multi);
                    }

                    m_context->UpdateCurrentViewer();
                    m_view->Redraw();
                }

                m_currentAction = CurrentAction::Nothing;
                emit objectHovered(QString());
                setCursor(Qt::CrossCursor);
                return;
            }
            else
            {
                // Simple clic ponctuel gauche
                if (!m_context.IsNull() && !m_view.IsNull())
                {
                    bool multi = (event->modifiers() & Qt::ControlModifier);
                    m_context->MoveTo(px, py, m_view, false);

                    if (m_context->HasDetected())
                    {
                        Handle(AIS_InteractiveObject) obj = m_context->DetectedInteractive();
                        m_context->SelectDetected(multi ? AIS_SelectionScheme_XOR : AIS_SelectionScheme_Replace);
                        if (m_selectionManager)
                        {
                            m_selectionManager->selectObject(obj, multi);
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
        if (distSq <= 16 && m_interactionMode != InteractionMode::Select)
        {
            if (m_interactionMode == InteractionMode::DrawSlab && m_drawingNodeIds.size() >= 3 && m_model)
            {
                int slabId = m_model->addSlab(m_drawingNodeIds, 0.20);
                emit drawingPromptChanged(tr("Dalle S%1 validée et créée (%2 nœuds)").arg(slabId).arg(m_drawingNodeIds.size()));
                cancelCurrentDrawing();
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

    m_currentAction = CurrentAction::Nothing;
    setCursor(Qt::CrossCursor);
}

void OccView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();

    emit mousePixelPositionChanged(px, py);

    // Mode Sélection rectangulaire (Fenêtre gauche->droite ou Capture droite->gauche)
    if (m_interactionMode == InteractionMode::Select && (event->buttons() & Qt::LeftButton))
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
        m_view->Rotation(px, py);
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

            // Détection survol d'objets pour affichage des propriétés et infobulles
            AIS_StatusOfDetection status = m_context->MoveTo(px, py, m_view, true);
            bool objectDetected = (status != AIS_SOD_Nothing && m_context->HasDetected());

            if (objectDetected)
            {
                setCursor(Qt::PointingHandCursor);

                Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
                int beamId = m_selectionManager ? m_selectionManager->getBeamId(detectedObj) : -1;
                int colId = m_selectionManager ? m_selectionManager->getColumnId(detectedObj) : -1;
                int slabId = m_selectionManager ? m_selectionManager->getSlabId(detectedObj) : -1;
                int nodeId = m_selectionManager ? m_selectionManager->getNodeId(detectedObj) : -1;

                if (nodeId > 0 && m_model)
                {
                    const auto* node = m_model->getNode(nodeId);
                    if (node)
                    {
                        emit mouseCoordinatesChanged(node->x(), node->y(), node->z());
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
            }
            else
            {
                setCursor(Qt::CrossCursor);
                emit objectHovered(QString());
                emit mouseCoordinatesChanged(wx, wy, wz);
                if (m_snapToGrid && !m_view.IsNull())
                {
                    m_view->Redraw();
                }
            }
        }
        break;
    }
}

void OccView::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0 || m_view.IsNull())
        return;

    const QPoint p = convertMousePos(event->position());
    const int px = p.x();
    const int py = p.y();

    // 1. Convertir les coordonnées pixels de la souris en coordonnées monde 3D avant zoom
    double wx = 0.0, wy = 0.0, wz = 0.0;
    m_view->Convert(px, py, wx, wy, wz);

    // 2. Appliquer le zoom
    constexpr double zoomFactor = 1.15;
    if (delta > 0)
    {
        m_view->SetZoom(zoomFactor);
    }
    else
    {
        m_view->SetZoom(1.0 / zoomFactor);
    }

    // 3. Re-projeter le point 3D initial en nouvelles coordonnées pixels après zoom
    int newPx = 0, newPy = 0;
    m_view->Convert(wx, wy, wz, newPx, newPy);

    // 4. Déplacer la vue (Pan) pour réaligner exactement le point 3D sous le curseur
    m_view->Pan(px - newPx, newPy - py);

    m_view->Redraw();
    emit viewCameraChanged();
}

void OccView::keyPressEvent(QKeyEvent* event)
{
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
        if (m_interactionMode == InteractionMode::DrawSlab && m_drawingNodeIds.size() >= 3 && m_model)
        {
            int slabId = m_model->addSlab(m_drawingNodeIds, 0.20);
            emit drawingPromptChanged(tr("Dalle S%1 créée (%2 nœuds)").arg(slabId).arg(m_drawingNodeIds.size()));
            cancelCurrentDrawing();
        }
    }
    else if (event->key() == Qt::Key_F)
    {
        fitAll();
    }
    else if (event->key() == Qt::Key_R)
    {
        resetView();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}
