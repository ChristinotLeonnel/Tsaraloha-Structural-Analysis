#pragma once

#include "../Interaction/Tools/ModelingTool.h"
#include <QWidget>
#include <QJsonObject>
#include <QPoint>
#include <map>

#include "../Model/Model.h"
#include "../Model/ModelDiff.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_DisplayConnection.hxx>

#include <gp_Pnt.hxx>
#include <vector>

namespace TSA::Analysis
{
    class ResultsModel;
}

namespace TSA::Viewer
{
    class SelectionManager;
    class ResultsVisualManager;
}

namespace TSA::Grid
{
    class GridManager;
    class GridSnapManager;
}
#include "../Grid/GridRenderer.h"
#include "../Model/CreationPresets.h"
#include "../Interaction/InteractionManager.h"
#include "../Coordinate/WorkPlane.h"
#include <AIS_ViewCube.hxx>
#include <AIS_RubberBand.hxx>
#include <AIS_Manipulator.hxx>
#include <AIS_Trihedron.hxx>
#include <gp_Trsf.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <Graphic3d_Camera.hxx>
#include <gp_Ax3.hxx>
#include <optional>
#include "../Model/SelectionQuery.h"
#include "MaterialVisual.h"
#include "ProjectionManager.h"
#include "ViewManager.h"
#include "../Grid/SnapManager.h"

class QTimer;

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
    TSA::Model::Model* model() const noexcept { return m_model; }
    void setSelectionManager(TSA::Viewer::SelectionManager* selectionManager);
    void rebuildAllShapes();

    // Mode d'affichage et de rendu des matériaux
    TSA::Viewer::RenderDisplayMode renderDisplayMode() const { return m_renderDisplayMode; }
    void setRenderDisplayMode(TSA::Viewer::RenderDisplayMode mode);

    // Visualisation des résultats OpenSees (déformée, diagrammes 3D, réactions)
    TSA::Viewer::ResultsVisualManager* resultsVisual() const { return m_resultsVisual.get(); }
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    // Gestion des formes 3D (avec mode batch / diff pour éviter les redraws multiples)
    void updateNodeShape(int nodeId, bool redrawImmediately = true);
    void updateSupportShape(int nodeId, bool redrawImmediately = true);
    void updateBeamShape(int beamId, bool redrawImmediately = true);
    void updateColumnShape(int columnId, bool redrawImmediately = true);
    void updateSlabShape(int slabId, bool redrawImmediately = true);
    void updateWallShape(int wallId, bool redrawImmediately = true);
    void updateFoundationShape(int foundationId, bool redrawImmediately = true);
    void updateTrussMemberShape(int memberId, bool redrawImmediately = true);
    void updateCableShape(int cableId, bool redrawImmediately = true);

    void removeNodeShape(int nodeId, bool redrawImmediately = true);
    void removeSupportShape(int nodeId, bool redrawImmediately = true);
    void removeBeamShape(int beamId, bool redrawImmediately = true);
    void removeColumnShape(int columnId, bool redrawImmediately = true);
    void removeSlabShape(int slabId, bool redrawImmediately = true);
    void removeWallShape(int wallId, bool redrawImmediately = true);
    void removeFoundationShape(int foundationId, bool redrawImmediately = true);
    void removeTrussMemberShape(int memberId, bool redrawImmediately = true);
    void removeCableShape(int cableId, bool redrawImmediately = true);
    /// Redessin différé et regroupé (une seule passe pour une rafale de notifications).
    void scheduleRedraw();

    void setSupportsVisible(bool visible);
    bool areSupportsVisible() const { return m_supportsVisible; }
    void setSupportLabelsVisible(bool visible);
    bool areSupportLabelsVisible() const { return m_supportLabelsVisible; }

    // Mise en surbrillance / Sélection visuelle
    void highlightNode(int nodeId);
    void highlightBeam(int beamId);
    void highlightColumn(int columnId);
    void highlightSlab(int slabId);
    void highlightWall(int wallId);
    void highlightFoundation(int foundationId);
    void highlightTrussMember(int memberId);
    void highlightCable(int cableId);
    void clearHighlight();
    /// Surbrillance de toute la sélection courante du SelectionManager, en une passe et un redraw.
    void highlightSelection();

    TSA::Viewer::SelectionManager* selectionManager() const { return m_selectionManager; }

    // Actions de vue et navigation étendues (AutoCAD / Robot SA style)
    void fitAll();
    void fitSelection();
    void fitModel();
    void fitResults();
    void fitDeformed();
    void resetView();
    void viewHome();
    void viewTop();
    void viewBottom();
    void viewFront();
    void viewBack();
    void viewLeft();
    void viewRight();
    void viewIsometric();

    void zoomIn(double factor = 1.25);
    void zoomOut(double factor = 1.25);
    void zoomAtCursor(const QPointF& logicalMousePos, double zoomFactor);
    void zoomWindow(int x1, int y1, int x2, int y2);
    void startInteractiveZoomWindow();
    void rotate2D(double angleDeg);

    // Historique de navigation de caméra (Previous / Next view)
    void pushCameraHistory();
    void previousView();
    void nextView();
    bool hasPreviousView() const;
    bool hasNextView() const;

    // Gestion du Plan de Travail actif (Work Plane)
    const TSA::Coordinate::WorkPlane& activeWorkPlane() const { return m_workPlane; }
    void setActiveWorkPlane(const TSA::Coordinate::WorkPlane& wp);
    void setWorkPlaneElevation(double elevation);
    void setWorkPlaneType(TSA::Coordinate::WorkPlaneType type, double offset = 0.0);
    void setWorkPlaneAxisAndOffset(TSA::Coordinate::WorkPlaneAxis axis, double offset, const std::string& name = "");
    void setWorkPlaneVisible(bool visible);
    bool isWorkPlaneVisible() const { return m_workPlaneVisible; }
    void viewNormalToWorkPlane();
    void updateWorkPlaneVisual();

    // Gestionnaires spécialisés Découplés (Section 11)
    TSA::Viewer::ProjectionManager* projectionManager() noexcept { return &m_projectionManager; }
    const TSA::Viewer::ProjectionManager* projectionManager() const noexcept { return &m_projectionManager; }

    TSA::Viewer::ViewManager* viewManager() noexcept { return &m_viewManager; }
    const TSA::Viewer::ViewManager* viewManager() const noexcept { return &m_viewManager; }

    TSA::Grid::SnapManager* snapManager() noexcept { return &m_snapManager; }
    const TSA::Grid::SnapManager* snapManager() const noexcept { return &m_snapManager; }

    // Mode et direction de projection (Section 5, 6 & 7)
    void setProjectionMode(TSA::Viewer::ProjectionMode mode);
    TSA::Viewer::ProjectionMode projectionMode() const { return m_projectionManager.mode(); }
    void setProjectionDirection(TSA::Viewer::ProjectionDirection dir);
    TSA::Viewer::ProjectionDirection projectionDirection() const { return m_projectionManager.direction(); }

    void applyStandardView(TSA::Viewer::StandardCameraView view);

    // Repère local du WorkPlane (Section 2)
    void setWorkPlaneAxesVisible(bool visible);
    bool isWorkPlaneAxesVisible() const noexcept { return m_workPlaneAxesVisible; }

    // Taille visuelle du Gizmo (Section 12)
    double gizmoSize() const noexcept { return m_gizmoSize; }
    void setGizmoSize(double size);

    // Manipulation 3D interactive, Gizmo & Isolation du Plan de Travail
    void attachManipulatorToWorkPlane();
    void detachManipulator();
    bool isManipulatingWorkPlane() const { return m_isManipulatingWorkPlane; }
    void applyWorkPlaneTransformation();
    void setWorkPlaneIsolation(bool isolated, double distance = 1.0);
    void updateElementIsolation();

    // Isolation par éléments (commandes « Isoler la sélection », « Masquer la sélection »…). Même
    // passe de visibilité que le plan de travail et les calques (updateElementIsolation). Chaque
    // opération empile l'état précédent : undoElementIsolation() y revient.
    void isolateElements(const TSA::Model::ElementSet& elements);
    void hideElements(const TSA::Model::ElementSet& elements);
    void invertElementIsolation();
    bool undoElementIsolation();
    void showAllElements();
    bool hasElementIsolation() const noexcept { return m_isolatedElements.has_value() || m_hiddenElements.size() > 0; }

    // Filtre d'affichage par famille d'éléments (dock Calques & Visibilité). S'applique dans
    // updateElementIsolation() : aucune autre mécanique de visibilité n'est créée.
    enum class ElementCategory { Beams, Columns, Slabs, Walls, Foundations, Trusses, Cables };
    void setElementCategoryVisible(ElementCategory category, bool visible);
    bool isElementCategoryVisible(ElementCategory category) const noexcept
    {
        return (m_hiddenElementCategories & (1u << static_cast<unsigned>(category))) == 0;
    }

    // Mode 2D CAO & Isolation Automatique du Plan de Travail
    bool isMode2D() const noexcept { return m_mode2DActive; }
    void setMode2D(bool enabled);
    void toggleMode2D() { setMode2D(!m_mode2DActive); }

    // Prédicats géométriques d'appartenance au plan actif (tolérance en mètres)
    bool isPointOnActiveWorkPlane(const gp_Pnt& pt, double tol = 0.05) const;
    bool isNodeOnActiveWorkPlane(int nodeId, double tol = 0.05) const;
    bool isLinearElementOnActiveWorkPlane(int startNodeId, int endNodeId, double tol = 0.05) const;
    bool isSurfaceElementOnActiveWorkPlane(const std::vector<int>& nodeIds, double tol = 0.05) const;

    // Repère Local des Éléments Structuraux (LCS - Règle 10 & 11)
    void setShowLocalAxes(bool show);
    bool showLocalAxes() const { return m_showLocalAxes; }
    void updateSelectedElementLocalAxes();
    void clearSelectedElementLocalAxes();

    /// Rendu hors écran du viewport réel (miniature .tsa, aperçus). Le cube de navigation et le
    /// trièdre, aides d'interface, sont masqués pendant la capture puis réaffichés.
    QImage captureViewImage(int width = 512, int height = 512, bool hideNavigationAids = true);

    // État de vue persistant (aperçus « dernier état », réouverture du projet) :
    // caméra OCCT réelle (eye, center, up, scale, projection) et principaux drapeaux d'affichage.
    QJsonObject cameraState() const;
    bool applyCameraState(const QJsonObject& state);
    QJsonObject viewState() const;

    // Intégration du système de Grille 3D paramétrique
    void setGridManager(TSA::Grid::GridManager* gridManager, TSA::Grid::GridSnapManager* snapManager);
    void rebuildGrid();
    /// Repères d'axes adaptés à la vue courante (plan / élévation / 3D) : recalculés seulement quand la
    /// catégorie de vue ou le niveau actif (en plan) change. Appelé à chaque viewCameraChanged.
    void updateGridLabelView();

    /// Structure d'origine pendant l'affichage de la déformée : normale, atténuée (déformée + initiale)
    /// ou masquée (déformée seule). Barres et nœuds ; appliqué dans la passe de visibilité unique.
    enum class ResultsStructureDisplay { Normal, Ghosted, Hidden };
    void setResultsStructureDisplay(ResultsStructureDisplay mode);
    ResultsStructureDisplay resultsStructureDisplay() const { return m_resultsStructureDisplay; }

    void setGridVisible(bool visible);
    bool isGridVisible() const;

    void setGridSnapEnabled(bool enabled);
    bool isGridSnapEnabled() const;

    void setObjectSnapEnabled(bool enabled);
    bool isObjectSnapEnabled() const;

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
    // Affichage du plan de section (rectangle visuel), indépendant de l'activation de la coupe
    void setSectionPlaneVisible(bool visible);
    bool isSectionPlaneVisible() const { return m_sectionPlaneVisible; }

    // Détection 3D intelligente sous le curseur : moteur d'accrochage écran (SnapEngine)
    static constexpr double kSnapApertureLogicalPx = 14.0;   ///< ouverture d'accrochage (pixels logiques)
    TSA::Grid::GridSnapResult computeSnap(int px, int py) const;
    const TSA::Grid::GridSnapResult& lastSnapResult() const noexcept { return m_lastSnapResult; }
    bool findNearest3DPoint(int px, int py, double& outX, double& outY, double& outZ,
                            int& outNodeId, QString& outDesc, TSA::Grid::GridSnapType& outType) const;

    // Dessin en hauteur & Niveaux
    void setActiveLevelElevation(double z);
    double activeLevelElevation() const { return m_activeLevelZ; }

    /// Synchronisation explicite Niveau -> WorkPlane (voir WorkPlane::moveToElevation pour la règle).
    void setSyncWorkPlaneWithLevel(bool enabled) { m_syncWorkPlaneWithLevel = enabled; }
    bool syncWorkPlaneWithLevel() const { return m_syncWorkPlaneWithLevel; }

    using InteractionMode = TSA::Interaction::InteractionMode;

    TSA::Interaction::InteractionManager* interactionManager() { return m_interactionManager.get(); }
    const TSA::Interaction::InteractionManager* interactionManager() const { return m_interactionManager.get(); }

    InteractionMode interactionMode() const;
    void setInteractionMode(InteractionMode mode);
    void cancelCurrentDrawing();
    size_t previewGhostBuildCount() const { return m_previewGhostBuildCount; } ///< Mesure : fantômes créés depuis le début

    const TSA::Model::StructurePresets& creationPresets() const { return m_presets; }
    TSA::Model::StructurePresets& creationPresets() { return m_presets; }
    void setCreationPresets(const TSA::Model::StructurePresets& p) { m_presets = p; }

    const TSA::Model::BarProperties& currentBarProperties() const { return m_currentBarProps; }
    void setCurrentBarProperties(const TSA::Model::BarProperties& props);
    void startChainedBarDrawing(const gp_Pnt& originPt, int originNodeId);

    // Dessin d'éléments surfaciques (Dalles & Voiles)
    void finishCurrentSlab();
    void resetCurrentSlabContour();

    // Visibilité et étiquetage 3D des nœuds
    enum class NodeDisplayFilter
    {
        All = 0,
        FreeOnly,
        SupportedOnly,
        SelectedOnly
    };

    void setNodesVisible(bool visible);
    bool areNodesVisible() const noexcept { return m_nodesVisible; }

    void setNodeLabelsVisible(bool visible);
    bool areNodeLabelsVisible() const noexcept { return m_nodeLabelsVisible; }

    void setNodeDisplayFilter(NodeDisplayFilter filter);
    NodeDisplayFilter nodeDisplayFilter() const noexcept { return m_nodeDisplayFilter; }

    // Visibilité et étiquetage 3D des charges (Forces, Moments, Réparties)
    void setLoadsVisible(bool visible);
    bool areLoadsVisible() const noexcept { return m_loadsVisible; }

    void setForcesVisible(bool visible);
    bool areForcesVisible() const noexcept { return m_forcesVisible; }

    void setMomentsVisible(bool visible);
    bool areMomentsVisible() const noexcept { return m_momentsVisible; }

    void setLoadValuesVisible(bool visible);
    bool areLoadValuesVisible() const noexcept { return m_loadValuesVisible; }

    void setLoadScale(double scale);
    double loadScale() const noexcept { return m_loadScale; }

    void updateNodalLoadShape(int loadId, bool redrawImmediately = true);
    void removeNodalLoadShape(int loadId, bool redrawImmediately = true);
    void updateMemberLoadShape(int loadId, bool redrawImmediately = true);
    void removeMemberLoadShape(int loadId, bool redrawImmediately = true);
    void updateAllLoadShapes();
    void clearLoadShapes();

    /// owner : objet dont les rappels dépendent (fenêtre, widget). Sa destruction annule la requête
    /// sans appeler les rappels ; sans owner, un clic après la fermeture de la fenêtre appelait un
    /// rappel sur un objet détruit (crash).
    void pickPoint3D(const std::function<void(const gp_Pnt& pt, int nodeId)>& onPicked,
                     const std::function<void()>& onCancelled = nullptr,
                     QObject* owner = nullptr);

    // Outils de modification / dessin en saisie 3D (OccView_Tools.cpp). L'outil appartient à
    // l'appelant ; la vue ne fait que la saisie et l'aperçu.
    void startModelingTool(TSA::Interaction::ModelingTool* tool, const TSA::Interaction::ToolContext& ctx);
    /// Après exécution de l'opération : poursuivre (nouvelle saisie) ou revenir à la sélection.
    void modelingToolApplied(bool continueTool);
    void setModelingToolSelection(const TSA::Model::ElementSet& selection);
    TSA::Interaction::ModelingTool* activeModelingTool() const { return m_activeTool; }

signals:
    void fileDropped(const QString& filePath);
    void mouseCoordinatesChanged(double x, double y, double z);
    void mousePixelPositionChanged(int px, int py);
    void viewCameraChanged();
    void objectHovered(const QString& info);
    void gridVisibilityChanged(bool visible);
    void gridSnapChanged(bool enabled);
    void objectSnapChanged(bool enabled);
    void nodesVisibilityChanged(bool visible);
    void nodeLabelsVisibilityChanged(bool visible);
    void nodeDisplayFilterChanged(NodeDisplayFilter filter);
    void loadsVisibilityChanged(bool visible);
    void forcesVisibilityChanged(bool visible);
    void momentsVisibilityChanged(bool visible);
    void loadValuesVisibilityChanged(bool visible);
    void loadScaleChanged(double scale);
    void interactionModeChanged(InteractionMode mode);
    void drawingPromptChanged(const QString& prompt);
    void viewPlaneModeChanged(ViewPlaneMode mode);
    void coordinateSystemChanged(bool isLocal);
    void clippingChanged(bool enabled, int axisIndex, double position, bool flip);

    // Signaux Barres (Robot Structural Analysis)
    void barFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void barSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void barDrawingCancelled();

    // Signaux Câbles (Tension Systems)
    void cableFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void cableSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void cableDrawingCancelled();

    // Signaux Surfaciques (Dalles & Voiles)
    void slabNodePicked(int nodeId, const gp_Pnt& pt, int totalCount);
    void slabDrawingCancelled();
    void slabCreated(int slabId);
    void wallFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void wallSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void wallDrawingCancelled();
    void wallCreated(int wallId);

    // Signaux de manipulation 3D directe
    void originMoveRequested(const gp_Pnt& newOrigin);
    void pasteAtPointRequested(const gp_Pnt& target);
    void elementCreated();
    /// L'outil actif a toutes ses données : exécuter l'opération.
    void modelingToolReady();

    // Signaux Navigation & Modélisation CAO avancée
    void cameraHistoryChanged(bool hasPrev, bool hasNext);
    void workPlaneChanged(const TSA::Coordinate::WorkPlane& wp);
    void snapChanged(const TSA::Grid::GridSnapResult& snap);
    void projectionModeChanged(TSA::Viewer::ProjectionMode mode);
    void projectionDirectionChanged(TSA::Viewer::ProjectionDirection dir);
    void standardViewChanged(TSA::Viewer::StandardCameraView view);
    void workPlaneAxesVisibleChanged(bool visible);
    void gizmoSizeChanged(double size);
    void mouseLocalCoordinatesChanged(double xwp, double ywp);
    void mode2DChanged(bool active);

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

    void onCableAdded(const TSA::Model::Cable& cable) override;
    void onCableModified(const TSA::Model::Cable& cable) override;
    void onCableRemoved(int cableId) override;

    void onNodalLoadAdded(int loadId) override;
    void onNodalLoadModified(int loadId) override;
    void onNodalLoadRemoved(int loadId) override;

    void onMemberLoadAdded(int loadId) override;
    void onMemberLoadModified(int loadId) override;
    void onMemberLoadRemoved(int loadId) override;

    void onLoadAdded(int loadId) override;
    void onLoadModified(int loadId) override;
    void onLoadRemoved(int loadId) override;
    void onLoadCaseChanged(int caseId) override;

    void onModelDiffApplied(const TSA::Model::ModelDiff& diff) override;
    void onModelCleared() override;
    /// Après toute modification : sélection débarrassée des éléments supprimés (différé, regroupé).
    void onModelEdited() override;
    void onModelDestroyed() override { m_model = nullptr; }

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
    void clearTransformPreview();
    void buildTransformPreviewGhosts();
    int getOrCreateNode(double x, double y, double z, int existingNodeId);
    void updateClipPlaneEquation();
    void updateSectionPlaneVisual();
    /// Applique au trièdre du WorkPlane la rotation (sans translation) + l'ancrage au point d'origine
    void applyWorkPlaneTrihedronTransform(const gp_Trsf& workPlaneTrsf);

private:
    TSA::Model::Model* m_model = nullptr;
    TSA::Viewer::SelectionManager* m_selectionManager = nullptr;

    Handle(Aspect_DisplayConnection) m_displayConnection;
    Handle(OpenGl_GraphicDriver)    m_graphicDriver;
    Handle(V3d_Viewer)              m_viewer;
    Handle(V3d_View)                m_view;
    Handle(AIS_InteractiveContext)  m_context;

    std::map<int, Handle(AIS_Shape)> m_nodeShapes;
    std::map<int, Handle(AIS_TextLabel)> m_nodeLabels;
    std::map<int, Handle(AIS_Shape)> m_supportShapes;
    std::map<int, Handle(AIS_TextLabel)> m_supportLabels;
    bool m_supportsVisible = true;
    bool m_supportLabelsVisible = false;
    unsigned m_hiddenElementCategories = 0; // masque de bits ElementCategory masquées
    std::map<int, Handle(AIS_Shape)> m_beamShapes;
    std::map<int, Handle(AIS_Shape)> m_columnShapes;
    std::map<int, Handle(AIS_Shape)> m_slabShapes;
    std::map<int, Handle(AIS_Shape)> m_wallShapes;
    std::map<int, Handle(AIS_Shape)> m_foundationShapes;
    std::map<int, Handle(AIS_Shape)> m_trussShapes;
    std::map<int, Handle(AIS_Shape)> m_cableShapes;
    std::map<int, std::vector<Handle(AIS_Shape)>> m_nodalLoadShapes;
    std::map<int, Handle(AIS_TextLabel)> m_nodalLoadLabels;
    std::map<int, std::vector<Handle(AIS_Shape)>> m_memberLoadShapes;
    std::map<int, Handle(AIS_TextLabel)> m_memberLoadLabels;
    /// Longueur des flèches de charges réparties au dernier rendu (palier de zoom).
    double m_memberLoadArrowLength = 0.0;
    double memberLoadArrowLength() const;
    /// Caméra modifiée : charges réparties redessinées seulement si le palier de zoom change.
    void updateMemberLoadZoomScale();
    bool m_loadsVisible = true;
    bool m_forcesVisible = true;
    bool m_momentsVisible = true;
    bool m_loadValuesVisible = true;
    double m_loadScale = 1.0;

    bool m_isInitialized = false;

    enum class CurrentAction
    {
        Nothing,
        Pan,
        Rotation,
        WindowSelect,
        ZoomWindow
    };

    CurrentAction m_currentAction = CurrentAction::Nothing;
    QPoint m_lastMousePos;
    QPoint m_pressMousePos;
    QPoint m_dragStartPos;

    TSA::Grid::GridManager* m_gridManager = nullptr;
    TSA::Grid::GridSnapManager* m_gridSnapManager = nullptr;
    TSA::Grid::GridRenderer m_gridRenderer;

    bool m_snapToGrid = true;
    bool m_snapToObject = true;
    bool m_gridVisible = true;
    bool m_gridLabelsVisible = true;
    bool m_nodesVisible = true;
    bool m_nodeLabelsVisible = false;
    NodeDisplayFilter m_nodeDisplayFilter = NodeDisplayFilter::All;
    bool isNodeVisibleByFilter(int nodeId) const;
    void updateNodeVisibilities();
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
    Handle(AIS_Shape) m_sectionPlaneShape;   ///< Rectangle visuel du plan de section (non sélectionnable)
    bool m_sectionPlaneVisible = false;

    Handle(AIS_ViewCube) m_viewCube;

    // Plan de Travail (WorkPlane) actif & Visualiseur 3D
    TSA::Coordinate::WorkPlane m_workPlane;
    Handle(AIS_Shape) m_workPlaneShape;
    /// Axes X/Y/Z du WorkPlane avec lettres : taille FIXE À L'ÉCRAN (persistance de zoom, unités = pixels)
    Handle(AIS_Trihedron) m_workPlaneTrihedron;
    Handle(AIS_Shape) m_workPlaneOriginShape;
    bool m_workPlaneAxesVisible = true;
    double m_gizmoSize = 100.0;
    TSA::Viewer::ProjectionManager m_projectionManager;
    TSA::Viewer::ViewManager m_viewManager;
    TSA::Grid::SnapManager m_snapManager;
    bool m_workPlaneVisible = true;
    bool m_syncWorkPlaneWithLevel = true; ///< Niveau sélectionné -> déplace le WorkPlane horizontal actif
    Handle(AIS_Manipulator) m_manipulator;
    bool m_isManipulatingWorkPlane = false;
    TSA::Coordinate::WorkPlane m_manipulatorStartWp;

    // Mode 2D CAO & Isolation Automatique non-destructive
    bool m_mode2DActive = false;
    Handle(Graphic3d_Camera) m_savedCamera3D;
    bool m_savedWasOrtho = false;
    bool m_pre2DNodesVisible = true;
    bool m_pre2DNodeLabelsVisible = false;
    bool m_pre2DSupportsVisible = true;
    bool m_pre2DLoadsVisible = true;
    bool m_pre2DWorkPlaneAxesVisible = true;
    void savePre2DVisibility();
    void restorePre2DVisibility();

    // Isolation (mode 2D ou « Isoler le plan ») : la visibilité de chaque objet est recalculée à
    // partir des drapeaux d'affichage et de l'appartenance au plan actif.
    ResultsStructureDisplay m_resultsStructureDisplay = ResultsStructureDisplay::Normal;
    std::map<const AIS_InteractiveObject*, std::pair<Handle(AIS_InteractiveObject), double>> m_resultsGhostSaved;
    bool m_isolationApplied = false; ///< Vrai si la dernière passe d'isolation a masqué des objets
    std::optional<TSA::Model::ElementSet> m_isolatedElements; ///< seuls ces éléments (et leurs nœuds) restent visibles
    TSA::Model::ElementSet m_hiddenElements;                  ///< éléments masqués par l'utilisateur
    std::vector<std::pair<std::optional<TSA::Model::ElementSet>, TSA::Model::ElementSet>> m_elementIsolationHistory;
    void pushElementIsolationState();
    bool isIsolationActive() const noexcept;
    double isolationTolerance() const noexcept;
    bool keepNodeUnderIsolation(int nodeId) const;
    bool keepLinearUnderIsolation(int startNodeId, int endNodeId) const;
    bool keepSurfaceUnderIsolation(const std::vector<int>& nodeIds) const;
    /// Repasse l'isolation si elle est (ou était) appliquée ; retourne vrai si une passe a eu lieu.
    bool reapplyIsolationIfActive();

    // Repère local de l'élément sélectionné
    Handle(AIS_Shape) m_elementLocalAxesShape;
    bool m_showLocalAxes = true;

    // Historique Caméra (Previous / Next View)
    std::vector<Handle(Graphic3d_Camera)> m_cameraUndoStack;
    std::vector<Handle(Graphic3d_Camera)> m_cameraRedoStack;
    bool m_isRestoringCamera = false;
    static constexpr size_t MAX_CAMERA_HISTORY = 30;

    // Marqueur visuel interactif d'accrochage (Snap Marker)
    TSA::Grid::GridSnapResult m_lastSnapResult;
    bool m_snapMarkerDirty = false;   ///< marqueur modifié : un Redraw est dû (MoveTo ne redessine que la couche immédiate)
    void setLastSnap(const TSA::Grid::GridSnapResult& snap);
    void refreshSnapAtCursor();   ///< recalcule le marqueur à la position courante (bascule F3 / S)
    Handle(AIS_Shape) m_snapMarkerShape;
    void updateSnapMarker(const TSA::Grid::GridSnapResult& snap);
    void clearSnapMarker();

    std::unique_ptr<TSA::Interaction::InteractionManager> m_interactionManager;
    std::vector<int> m_drawingNodeIds;
    std::vector<gp_Pnt> m_drawingPoints;

    // Outil de modification / dessin actif (OccView_Tools.cpp)
    TSA::Interaction::ModelingTool* m_activeTool = nullptr;
    TSA::Interaction::ToolContext m_toolCtx;
    QString m_toolInput;
    Handle(AIS_Shape) m_toolPreviewShape;
    void handleModelingToolClick(const QPoint& p, Qt::KeyboardModifiers modifiers);
    void updateModelingToolPreview(const gp_Pnt& cursor);
    bool handleModelingToolKey(QKeyEvent* event);
    bool pickBarAt(const QPoint& p, TSA::Interaction::ToolPick& pick);
    void clearModelingToolPreview();
    void emitModelingToolPrompt();
    Handle(AIS_Shape) m_rubberBandShape;
    Handle(AIS_RubberBand) m_selectRubberBand;
    std::vector<Handle(AIS_Shape)> m_previewGhostShapes;
    bool m_previewGhostsBuilt = false;
    size_t m_previewGhostBuildCount = 0; ///< Nombre de fantômes créés (mesure : doit rester constant pendant un déplacement de souris)


    TSA::Model::StructurePresets m_presets;
    TSA::Model::BarProperties m_currentBarProps;
    TSA::Viewer::RenderDisplayMode m_renderDisplayMode = TSA::Viewer::RenderDisplayMode::Materials;
    std::unique_ptr<TSA::Viewer::ResultsVisualManager> m_resultsVisual;
    QTimer* m_redrawTimer = nullptr;
};
