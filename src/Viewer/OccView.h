#pragma once

#include <QWidget>
#include <QPoint>
#include <map>

#include "../Model/Model.h"
#include "../Model/ModelDiff.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_DisplayConnection.hxx>

#include <gp_Pnt.hxx>
#include <vector>

namespace TSA::Viewer
{
    class SelectionManager;
}

namespace TSA::Grid
{
    class GridManager;
    class GridSnapManager;
}
#include "../Grid/GridRenderer.h"
#include "../Model/CreationPresets.h"
#include "../Interaction/InteractionManager.h"
#include <AIS_ViewCube.hxx>
#include <AIS_RubberBand.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <gp_Ax3.hxx>
#include "MaterialVisual.h"

class OccView : public QWidget, public TSA::Model::IModelObserver
{
    Q_OBJECT

public:
    explicit OccView(QWidget* parent = nullptr);
    ~OccView() override;

    const Handle(AIS_InteractiveContext)& context() const { return m_context; }
    const Handle(V3d_View)& view() const { return m_view; }
    const Handle(V3d_Viewer)& viewer() const { return m_viewer; }

    // Liaison avec le modèle et la sélection
    void setModel(TSA::Model::Model* model);
    void setSelectionManager(TSA::Viewer::SelectionManager* selectionManager);
    void rebuildAllShapes();

    // Mode d'affichage et de rendu des matériaux
    TSA::Viewer::RenderDisplayMode renderDisplayMode() const { return m_renderDisplayMode; }
    void setRenderDisplayMode(TSA::Viewer::RenderDisplayMode mode);

    // Gestion des formes 3D (avec mode batch / diff pour éviter les redraws multiples)
    void updateNodeShape(int nodeId, bool redrawImmediately = true);
    void updateBeamShape(int beamId, bool redrawImmediately = true);
    void updateColumnShape(int columnId, bool redrawImmediately = true);
    void updateSlabShape(int slabId, bool redrawImmediately = true);
    void updateWallShape(int wallId, bool redrawImmediately = true);
    void updateFoundationShape(int foundationId, bool redrawImmediately = true);
    void updateTrussMemberShape(int memberId, bool redrawImmediately = true);

    void removeNodeShape(int nodeId, bool redrawImmediately = true);
    void removeBeamShape(int beamId, bool redrawImmediately = true);
    void removeColumnShape(int columnId, bool redrawImmediately = true);
    void removeSlabShape(int slabId, bool redrawImmediately = true);
    void removeWallShape(int wallId, bool redrawImmediately = true);
    void removeFoundationShape(int foundationId, bool redrawImmediately = true);
    void removeTrussMemberShape(int memberId, bool redrawImmediately = true);

    // Mise en surbrillance / Sélection visuelle
    void highlightNode(int nodeId);
    void highlightBeam(int beamId);
    void highlightColumn(int columnId);
    void highlightSlab(int slabId);
    void highlightWall(int wallId);
    void highlightFoundation(int foundationId);
    void highlightTrussMember(int memberId);
    void clearHighlight();

    TSA::Viewer::SelectionManager* selectionManager() const { return m_selectionManager; }

    // Actions de vue
    void fitAll();
    void resetView();
    void zoomAtCursor(const QPointF& logicalMousePos, double zoomFactor);
    QImage captureViewImage(int width = 512, int height = 512);

    // Intégration du système de Grille 3D paramétrique
    void setGridManager(TSA::Grid::GridManager* gridManager, TSA::Grid::GridSnapManager* snapManager);
    void rebuildGrid();

    void setGridVisible(bool visible);
    bool isGridVisible() const;

    void setGridSnapEnabled(bool enabled);
    bool isGridSnapEnabled() const;

    void setGridLabelsVisible(bool visible);
    bool areGridLabelsVisible() const;

    void setGridLevelsVisible(bool visible);
    bool areGridLevelsVisible() const;

    // Support des règles et projections
    bool pixelToWorldPlane(int px, int py, double& wx, double& wy, double& wz) const;
    void worldToPixel(double wx, double wy, double wz, int& px, int& py) const;
    void setViewOrientation(V3d_TypeOfOrientation orientation);
    void setCadBlueprintTheme(bool enabled);
    void setDarkMode(bool dark);
    bool isDarkMode() const { return m_isDarkMode; }

    // Vues en Plan & Projections (Robot SA style)
    enum class ViewPlaneMode
    {
        Perspective3D,
        PlanXY,
        PlanXZ,
        PlanYZ
    };
    ViewPlaneMode viewPlaneMode() const { return m_viewPlaneMode; }
    void setViewPlaneMode(ViewPlaneMode mode);

    // Repère Global vs Local
    bool isLocalCoordinateSystem() const { return m_isLocalCoordinateSystem; }
    void setLocalCoordinateSystem(bool local);

    // Système de Coupe / Section 3D (Graphic3d_ClipPlane)
    void setClippingEnabled(bool enabled);
    bool isClippingEnabled() const { return m_isClippingEnabled; }
    void setClipPlane(int axisIndex, double position, bool flip = false); // 0=XY, 1=XZ, 2=YZ
    double clipPosition() const { return m_clipPosition; }
    int clipAxisIndex() const { return m_clipAxisIndex; }
    bool isClipFlipped() const { return m_isClipFlipped; }

    // Détection 3D intelligente sous le curseur
    bool findNearest3DPoint(int px, int py, double& outX, double& outY, double& outZ,
                            int& outNodeId, QString& outDesc, TSA::Grid::GridSnapType& outType) const;

    // Dessin en hauteur & Niveaux
    void setActiveLevelElevation(double z);
    double activeLevelElevation() const { return m_activeLevelZ; }

    using InteractionMode = TSA::Interaction::InteractionMode;

    TSA::Interaction::InteractionManager* interactionManager() { return m_interactionManager.get(); }
    const TSA::Interaction::InteractionManager* interactionManager() const { return m_interactionManager.get(); }

    InteractionMode interactionMode() const;
    void setInteractionMode(InteractionMode mode);
    void cancelCurrentDrawing();

    const TSA::Model::StructurePresets& creationPresets() const { return m_presets; }
    TSA::Model::StructurePresets& creationPresets() { return m_presets; }
    void setCreationPresets(const TSA::Model::StructurePresets& p) { m_presets = p; }

    const TSA::Model::BarProperties& currentBarProperties() const { return m_currentBarProps; }
    void setCurrentBarProperties(const TSA::Model::BarProperties& props);
    void startChainedBarDrawing(const gp_Pnt& originPt, int originNodeId);

    // Dessin d'éléments surfaciques (Dalles & Voiles)
    void finishCurrentSlab();
    void resetCurrentSlabContour();

signals:
    void fileDropped(const QString& filePath);
    void mouseCoordinatesChanged(double x, double y, double z);
    void mousePixelPositionChanged(int px, int py);
    void viewCameraChanged();
    void objectHovered(const QString& info);
    void gridVisibilityChanged(bool visible);
    void gridSnapChanged(bool enabled);
    void interactionModeChanged(InteractionMode mode);
    void drawingPromptChanged(const QString& prompt);
    void viewPlaneModeChanged(ViewPlaneMode mode);
    void coordinateSystemChanged(bool isLocal);
    void clippingChanged(bool enabled, int axisIndex, double position, bool flip);

    // Signaux Barres (Robot Structural Analysis)
    void barFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void barSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void barDrawingCancelled();

    // Signaux Surfaciques (Dalles & Voiles)
    void slabNodePicked(int nodeId, const gp_Pnt& pt, int totalCount);
    void slabDrawingCancelled();
    void slabCreated(int slabId);
    void wallFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void wallSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void wallDrawingCancelled();
    void wallCreated(int wallId);

    // Signaux de manipulation 3D directe
    void pointToPointMoveRequested(const gp_Pnt& base, const gp_Pnt& target, bool isCopy);
    void pointToPointRotateRequested(const gp_Pnt& center, double angleRad, bool isCopy);
    void originMoveRequested(const gp_Pnt& newOrigin);
    void pasteAtPointRequested(const gp_Pnt& target);
    void elementCreated();

protected:
    // IModelObserver overrides
    void onNodeAdded(const TSA::Model::Node& node) override;
    void onNodeModified(const TSA::Model::Node& node) override;
    void onNodeRemoved(int nodeId) override;

    void onBeamAdded(const TSA::Model::Beam& beam) override;
    void onBeamModified(const TSA::Model::Beam& beam) override;
    void onBeamRemoved(int beamId) override;

    void onColumnAdded(const TSA::Model::Column& column) override;
    void onColumnModified(const TSA::Model::Column& column) override;
    void onColumnRemoved(int columnId) override;

    void onSlabAdded(const TSA::Model::Slab& slab) override;
    void onSlabModified(const TSA::Model::Slab& slab) override;
    void onSlabRemoved(int slabId) override;

    void onWallAdded(const TSA::Model::Wall& wall) override;
    void onWallModified(const TSA::Model::Wall& wall) override;
    void onWallRemoved(int wallId) override;

    void onFoundationAdded(const TSA::Model::Foundation& foundation) override;
    void onFoundationModified(const TSA::Model::Foundation& foundation) override;
    void onFoundationRemoved(int foundationId) override;

    void onTrussMemberAdded(const TSA::Model::TrussMember& member) override;
    void onTrussMemberModified(const TSA::Model::TrussMember& member) override;
    void onTrussMemberRemoved(int memberId) override;

    void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override;
    void onModelCleared() override;

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initOcc();
    QPoint convertMousePos(const QPointF& logicalPos) const;

    bool getPointUnderCursor(const QPoint& mousePixelPos, double& x, double& y, double& z, int& detectedNodeId);
    void updateRubberBand(const gp_Pnt& currentPnt);
    void clearRubberBand();
    void updateTransformPreview(const gp_Pnt& currentPnt);
    void clearTransformPreview();
    int getOrCreateNode(double x, double y, double z, int existingNodeId);
    void updateClipPlaneEquation();

private:
    TSA::Model::Model* m_model = nullptr;
    TSA::Viewer::SelectionManager* m_selectionManager = nullptr;

    Handle(Aspect_DisplayConnection) m_displayConnection;
    Handle(OpenGl_GraphicDriver)    m_graphicDriver;
    Handle(V3d_Viewer)              m_viewer;
    Handle(V3d_View)                m_view;
    Handle(AIS_InteractiveContext)  m_context;

    std::map<int, Handle(AIS_Shape)> m_nodeShapes;
    std::map<int, Handle(AIS_Shape)> m_beamShapes;
    std::map<int, Handle(AIS_Shape)> m_columnShapes;
    std::map<int, Handle(AIS_Shape)> m_slabShapes;
    std::map<int, Handle(AIS_Shape)> m_wallShapes;
    std::map<int, Handle(AIS_Shape)> m_foundationShapes;
    std::map<int, Handle(AIS_Shape)> m_trussShapes;

    bool m_isInitialized = false;

    enum class CurrentAction
    {
        Nothing,
        Pan,
        Rotation,
        WindowSelect
    };

    CurrentAction m_currentAction = CurrentAction::Nothing;
    QPoint m_lastMousePos;
    QPoint m_pressMousePos;
    QPoint m_dragStartPos;

    TSA::Grid::GridManager* m_gridManager = nullptr;
    TSA::Grid::GridSnapManager* m_gridSnapManager = nullptr;
    TSA::Grid::GridRenderer m_gridRenderer;

    bool m_snapToGrid = true;
    bool m_gridVisible = true;
    bool m_gridLabelsVisible = true;
    bool m_isDarkMode = true;
    mutable bool m_isCursorSnapped = false;
    double m_gridZOffset = 0.0;
    double m_activeLevelZ = 0.0;

    ViewPlaneMode m_viewPlaneMode = ViewPlaneMode::Perspective3D;
    bool m_isLocalCoordinateSystem = false;
    gp_Ax3 m_localCS;

    bool m_isClippingEnabled = false;
    int m_clipAxisIndex = 0; // 0=XY, 1=XZ, 2=YZ
    double m_clipPosition = 0.0;
    bool m_isClipFlipped = false;
    Handle(Graphic3d_ClipPlane) m_clipPlane;

    Handle(AIS_ViewCube) m_viewCube;

    std::unique_ptr<TSA::Interaction::InteractionManager> m_interactionManager;
    std::vector<int> m_drawingNodeIds;
    std::vector<gp_Pnt> m_drawingPoints;
    Handle(AIS_Shape) m_rubberBandShape;
    Handle(AIS_RubberBand) m_selectRubberBand;
    std::vector<Handle(AIS_Shape)> m_previewGhostShapes;

    gp_Pnt m_basePoint3D;
    gp_Pnt m_centerPoint3D;
    bool m_hasBasePoint = false;
    bool m_hasCenterPoint = false;

    TSA::Model::StructurePresets m_presets;
    TSA::Model::BarProperties m_currentBarProps;
    TSA::Viewer::RenderDisplayMode m_renderDisplayMode = TSA::Viewer::RenderDisplayMode::Materials;
};
