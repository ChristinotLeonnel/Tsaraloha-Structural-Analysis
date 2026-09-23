#pragma once

#include <QMainWindow>
#include <memory>
#include <vector>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

namespace TSA::Model { class Model; }
namespace TSA::Viewer { class SelectionManager; }
namespace TSA::Grid
{
    class GridManager;
    class GridSnapManager;
}
namespace TSA::UI
{
    class ModelTreeWidget;
    class PropertyPanel;
    class ViewportContainer;
    class SectionCutDialog;
    class RibbonBar;
    class VisibilityDock;
    class LogConsoleDock;
    class HelpDialog;
}

class OccView;
class QAction;
class QActionGroup;
class QLabel;
class QDockWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    TSA::Model::Model* model() { return m_model.get(); }
    const TSA::Model::Model* model() const { return m_model.get(); }

    TSA::Grid::GridManager* gridManager() { return m_gridManager.get(); }

private slots:
    void onFitAll();
    void onResetView();

    // Grille 3D & Niveaux
    void onNewGrid();
    void onGridManagerDialog();
    void onManageLevels();
    void onToggleGridVisible(bool checked);
    void onToggleGridSnap(bool checked);
    void onToggleGridLabels(bool checked);
    void onToggleLevelsVisible(bool checked);
    void onToggleRulersVisible(bool checked);
    void onToggleDarkMode(bool checked);
    void onToggleFullScreen(bool checked);

    // Modes d'interaction (Dessin 3D)
    void onModeSelect();
    void onModeDrawNode();
    void onModeDrawBeam();
    void onModeDrawColumn();
    void onModeDrawSlab();

    // Actions structurales (Dialogues)
    void onActionNewNode();
    void onActionNewBeam();
    void onActionNewColumn();
    void onActionNewSlab();
    void onActionAddCube();
    void onActionDeleteSelected();

    // Opérations géométriques
    void onActionMove();
    void onActionCopy();

    // Undo / Redo (Ctrl+Z / Ctrl+Y)
    void onActionUndo();
    void onActionRedo();

    // Fichier
    void onActionNew();
    void onActionOpen();
    void onActionSave();

private:
    void updateUndoRedoActions();
    void setupUi();
    void createActions();
    void createMenus();
    void createRibbon();
    void createToolBars();
    void createDockWindows();
    void createStatusBar();

private:
    std::unique_ptr<TSA::Model::Model> m_model;
    std::unique_ptr<TSA::Viewer::SelectionManager> m_selectionManager;
    std::unique_ptr<TSA::Grid::GridManager> m_gridManager;
    std::unique_ptr<TSA::Grid::GridSnapManager> m_gridSnapManager;

    OccView* m_occView = nullptr;
    TSA::UI::ViewportContainer* m_viewportContainer = nullptr;
    TSA::UI::ModelTreeWidget* m_modelTree = nullptr;
    TSA::UI::PropertyPanel*   m_propertyPanel = nullptr;
    TSA::UI::RibbonBar*       m_ribbonBar = nullptr;

    QDockWidget* m_modelTreeDock = nullptr;
    QDockWidget* m_propertiesDock = nullptr;
    TSA::UI::VisibilityDock* m_visibilityDock = nullptr;
    TSA::UI::LogConsoleDock* m_consoleDock = nullptr;

    QLabel*  m_statusCoordinates = nullptr;
    QLabel*  m_statusInfo = nullptr;

    // Actions Fichier
    QAction* m_actionNew = nullptr;
    QAction* m_actionOpen = nullptr;
    QAction* m_actionSave = nullptr;
    QAction* m_actionExit = nullptr;

    QAction* m_actionFitAll = nullptr;
    QAction* m_actionResetView = nullptr;

    // Actions Grille & Règles
    QAction* m_actionNewGrid = nullptr;
    QAction* m_actionGridManager = nullptr;
    QAction* m_actionManageLevels = nullptr;
    QAction* m_actionGridVisible = nullptr;
    QAction* m_actionGridSnap = nullptr;
    QAction* m_actionGridLabels = nullptr;
    QAction* m_actionLevelsVisible = nullptr;
    QAction* m_actionRulersVisible = nullptr;
    QAction* m_actionDarkMode = nullptr;
    QAction* m_actionFullScreen = nullptr;
    bool m_wasMaximizedBeforeFullScreen = false;

    // Actions Modes d'interaction / Dessin 3D
    QActionGroup* m_drawModeGroup = nullptr;
    QAction* m_actionSelectMode = nullptr;
    QAction* m_actionDrawNode = nullptr;
    QAction* m_actionDrawBeam = nullptr;
    QAction* m_actionDrawColumn = nullptr;
    QAction* m_actionDrawSlab = nullptr;

    QAction* m_actionNewNode = nullptr;
    QAction* m_actionNewBeam = nullptr;
    // Presse-papier structural pour copier-coller 3D (Ctrl+C / Ctrl+V)
    struct ClipboardNode {
        int originalId = 0;
        double relX = 0.0, relY = 0.0, relZ = 0.0;
    };
    struct ClipboardBeam {
        int originalStartNodeId = 0;
        int originalEndNodeId = 0;
        double width = 0.30;
        double height = 0.50;
    };
    struct ClipboardColumn {
        int originalStartNodeId = 0;
        int originalEndNodeId = 0;
        double width = 0.40;
        double height = 0.40;
    };
    struct ClipboardSlab {
        std::vector<int> originalNodeIds;
        double thickness = 0.20;
    };
    struct StructuralClipboard {
        bool hasData = false;
        double refOriginX = 0.0, refOriginY = 0.0, refOriginZ = 0.0;
        std::vector<ClipboardNode> nodes;
        std::vector<ClipboardBeam> beams;
        std::vector<ClipboardColumn> columns;
        std::vector<ClipboardSlab> slabs;
    };

    QAction* m_actionNewColumn = nullptr;
    QAction* m_actionNewSlab = nullptr;
    QAction* m_actionAddCube = nullptr;
    QAction* m_actionDelete = nullptr;

    QAction* m_actionMove = nullptr;
    QAction* m_actionCopy = nullptr;

    // Actions Undo / Redo
    QAction* m_actionUndo = nullptr;
    QAction* m_actionRedo = nullptr;

    // Actions Transformations 3D directes & Presse-papier
    QAction* m_actionMove3D = nullptr;
    QAction* m_actionCopy3D = nullptr;
    QAction* m_actionRotate3D = nullptr;
    QAction* m_actionMoveOrigin = nullptr;
    QAction* m_actionCopyClipboard = nullptr;
    QAction* m_actionPasteClipboard = nullptr;

    StructuralClipboard m_clipboard;

    // Actions Barre "Vue" (Robot SA style)
    QAction* m_actionViewXY = nullptr;
    QAction* m_actionViewYZ = nullptr;
    QAction* m_actionViewXZ = nullptr;
    QAction* m_actionView3D = nullptr;
    QAction* m_actionCoordSystem = nullptr;
    QAction* m_actionSectionCut = nullptr;

    // Actions Thème & Aide
    QAction* m_actionToggleTheme = nullptr;
    QAction* m_actionHelp = nullptr;
    QAction* m_actionShortcuts = nullptr;
    QAction* m_actionAbout = nullptr;

    // Actions Métier & Outils Avancés
    QAction* m_actionWall = nullptr;
    QAction* m_actionTruss = nullptr;
    QAction* m_actionFooting = nullptr;

    QAction* m_actionSecI = nullptr;
    QAction* m_actionSecRect = nullptr;
    QAction* m_actionSecCirc = nullptr;

    QAction* m_actionConcrete = nullptr;
    QAction* m_actionSteel = nullptr;

    QAction* m_actionFixed = nullptr;
    QAction* m_actionPinned = nullptr;
    QAction* m_actionRoller = nullptr;

    QAction* m_actionPointLoad = nullptr;
    QAction* m_actionDistLoad = nullptr;
    QAction* m_actionMoment = nullptr;
    QAction* m_actionSeismic = nullptr;
    QAction* m_actionMeshGen = nullptr;
    QAction* m_actionRunSolve = nullptr;
    QAction* m_actionModal = nullptr;

    QAction* m_actionResultsDisp = nullptr;
    QAction* m_actionResultsForces = nullptr;
    QAction* m_actionResultsStress = nullptr;
    QAction* m_actionMeasure = nullptr;

    TSA::UI::SectionCutDialog* m_sectionCutDialog = nullptr;
    TSA::UI::HelpDialog* m_helpDialog = nullptr;

private slots:
    void onActionViewXY();
    void onActionViewYZ();
    void onActionViewXZ();
    void onActionView3D();
    void onActionCoordSystem();
    void onActionSectionCut();
    void onToggleTheme();
    void onActionHelp();
    void onActionShortcuts();
    void onActionAbout();

    // Slots Outils Métier
    void onActionWall();
    void onActionTruss();
    void onActionFooting();
    void onActionSecI();
    void onActionSecRect();
    void onActionSecCirc();
    void onActionConcrete();
    void onActionSteel();
    void onActionFixed();
    void onActionPinned();
    void onActionRoller();
    void onActionPointLoad();
    void onActionDistLoad();
    void onActionMoment();
    void onActionSeismic();
    void onActionMeshGen();
    void onActionRunSolve();
    void onActionModal();
    void onActionResultsDisp();
    void onActionResultsForces();
    void onActionResultsStress();
    void onActionMeasure();

    // Slots Transformations 3D directes & Presse-papier
    void onActionMove3D();
    void onActionCopy3D();
    void onActionRotate3D();
    void onActionMoveOrigin();
    void onActionCopyClipboard();
    void onActionPasteClipboard();

    void onPointToPointMoveRequested(const gp_Pnt& base, const gp_Pnt& target, bool isCopy);
    void onPointToPointRotateRequested(const gp_Pnt& center, double angleRad, bool isCopy);
    void onOriginMoveRequested(const gp_Pnt& newOrigin);
    void onPasteAtPointRequested(const gp_Pnt& target);

protected:
    void changeEvent(QEvent* event) override;
};
