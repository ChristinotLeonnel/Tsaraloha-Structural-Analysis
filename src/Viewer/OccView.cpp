#include "OccView.h"
#include "SelectionManager.h"
#include "../Model/Model.h"
#include "../Geometry/BeamGeometry.h"
#include "../Geometry/SlabGeometry.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

#ifdef _WIN32
    #include <windows.h>
    #include <WNT_Window.hxx>
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

    m_viewer->SetRectangularGridValues(0.0, 0.0, 1.0, 1.0, 0.0);
    m_viewer->SetRectangularGridGraphicValues(20.0, 20.0, 0.0);
    setGridVisible(true);

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
    Handle(WNT_Window) wind = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#else
    Handle(Aspect_Window) wind = nullptr;
#endif
    m_view->SetWindow(wind);
    if (!wind->IsMapped())
    {
        wind->Map();
    }

    Quantity_Color topColor(0.18, 0.22, 0.28, Quantity_TOC_RGB);
    Quantity_Color bottomColor(0.08, 0.10, 0.13, Quantity_TOC_RGB);
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
        Quantity_NOC_WHITE,
        0.1,
        V3d_ZBUFFER
    );

    // Orientation standard génie civil : +Z vers le haut (élévation), projection axonométrique droite Z-up
    m_view->SetUp(0.0, 0.0, 1.0);
    m_view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight, false);
    m_view->MustBeResized();
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
    }
}

void OccView::fitAll()
{
    if (!m_view.IsNull())
    {
        m_view->FitAll();
        m_view->ZFitAll();
        m_view->Redraw();
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

void OccView::showCartesianGrid(double xStep, double yStep,
                                double xSize, double ySize,
                                double zOffset, bool pointsMode)
{
    if (m_viewer.IsNull())
        return;

    m_gridZOffset = zOffset;
    m_currentGridType = GridType::Cartesian;

    m_viewer->SetPrivilegedPlane(gp_Ax3(gp_Pnt(0.0, 0.0, zOffset), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0)));
    m_viewer->SetRectangularGridValues(0.0, 0.0, xStep, yStep, 0.0);
    m_viewer->SetRectangularGridGraphicValues(xSize, ySize, zOffset);

    Aspect_GridDrawMode mode = pointsMode ? Aspect_GDM_Points : Aspect_GDM_Lines;
    m_viewer->ActivateGrid(Aspect_GT_Rectangular, mode);

    auto grid = m_viewer->Grid(Aspect_GT_Rectangular, true);
    if (!grid.IsNull())
    {
        grid->SetColors(Quantity_NOC_GRAY40, Quantity_NOC_STEELBLUE);
    }

    emit gridTypeChanged(GridType::Cartesian);

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::showCylindricalGrid(double radiusStep, int divisionNumber,
                                  double maxRadius, double zOffset,
                                  bool pointsMode)
{
    if (m_viewer.IsNull())
        return;

    m_gridZOffset = zOffset;
    m_currentGridType = GridType::Cylindrical;

    m_viewer->SetPrivilegedPlane(gp_Ax3(gp_Pnt(0.0, 0.0, zOffset), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0)));
    m_viewer->SetCircularGridValues(0.0, 0.0, radiusStep, divisionNumber, 0.0);
    m_viewer->SetCircularGridGraphicValues(maxRadius, zOffset);

    Aspect_GridDrawMode mode = pointsMode ? Aspect_GDM_Points : Aspect_GDM_Lines;
    m_viewer->ActivateGrid(Aspect_GT_Circular, mode);

    auto grid = m_viewer->Grid(Aspect_GT_Circular, true);
    if (!grid.IsNull())
    {
        grid->SetColors(Quantity_NOC_GRAY40, Quantity_NOC_CYAN3);
    }

    emit gridTypeChanged(GridType::Cylindrical);

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::hideGrid()
{
    if (m_viewer.IsNull())
        return;

    m_viewer->DeactivateGrid();
    m_currentGridType = GridType::None;

    emit gridTypeChanged(GridType::None);

    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::setGridVisible(bool visible)
{
    if (visible)
    {
        if (m_currentGridType == GridType::Cylindrical)
        {
            showCylindricalGrid();
        }
        else
        {
            showCartesianGrid();
        }
    }
    else
    {
        hideGrid();
    }
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

bool OccView::getPointUnderCursor(const QPoint& mousePixelPos, double& x, double& y, double& z, int& detectedNodeId)
{
    detectedNodeId = -1;
    if (m_view.IsNull())
        return false;

    const int px = mousePixelPos.x();
    const int py = mousePixelPos.y();

    // 1. Détection prioritaire d'un nœud existant sous le curseur
    if (!m_context.IsNull())
    {
        AIS_StatusOfDetection status = m_context->MoveTo(px, py, m_view, false);
        if (status != AIS_SOD_Nothing && m_context->HasDetected())
        {
            Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
            int nId = m_selectionManager ? m_selectionManager->getNodeId(detectedObj) : -1;
            if (nId > 0 && m_model)
            {
                const auto* node = m_model->getNode(nId);
                if (node)
                {
                    x = node->x();
                    y = node->y();
                    z = node->z();
                    detectedNodeId = nId;
                    return true;
                }
            }
        }
    }

    // 2. Si aucun nœud n'est détecté, projection 3D sur le plan de référence de la grille (Z = m_gridZOffset)
    double xEye = 0.0, yEye = 0.0, zEye = 0.0;
    double xDir = 0.0, yDir = 0.0, zDir = 0.0;
    m_view->ConvertWithProj(px, py, xEye, yEye, zEye, xDir, yDir, zDir);

    double wx = 0.0, wy = 0.0, wz = m_gridZOffset;
    if (std::abs(zDir) > 1e-6)
    {
        double t = (m_gridZOffset - zEye) / zDir;
        wx = xEye + t * xDir;
        wy = yEye + t * yDir;
    }
    else
    {
        m_view->Convert(px, py, wx, wy, wz);
    }

    // 3. Accrochage magnétique à la grille si activé
    if (m_snapToGrid && !m_viewer.IsNull() && m_viewer->IsGridActive())
    {
        double gx = wx, gy = wy;
        auto activeGrid = m_viewer->Grid(false);
        if (!activeGrid.IsNull())
        {
            activeGrid->Hit(wx, wy, gx, gy);
            wx = gx;
            wy = gy;
        }
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

            m_selectRubberBand->SetRectangle(minX, minY, maxX, maxY);

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
        break;

    case CurrentAction::Pan:
        m_view->Pan(px - m_lastMousePos.x(),
                    m_lastMousePos.y() - py);
        m_lastMousePos = p;
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
