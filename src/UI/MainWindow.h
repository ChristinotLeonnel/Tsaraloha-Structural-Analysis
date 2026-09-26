#pragma once

#include <QMainWindow>
#include <memory>
#include <vector>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include "../Model/CreationPresets.h"
#include "../Model/Beam.h"
#include "../Model/StructuralClipboard.h"

namespace TSA::Model { class Model; }
namespace TSA::Project { class ProjectManager; }
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
    class BarCreationDialog;
    class SurfaceCreationDialog;
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
    const TSA::Grid::GridManager* gridManager() const { return m_gridManager.get(); }

    TSA::Project::ProjectManager* projectManager() { return m_projectManager.get(); }
    const TSA::Project::ProjectManager* projectManager() const { return m_projectManager.get(); }

    bool loadFile(const QString& filePath);
    bool saveFile(const QString& filePath);
    bool maybeSave();
    void updateWindowTitle();

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
    void onModeDrawWire();
    void onModeDrawSurface();
    void onModeDrawBar();
    void onModeDrawBeam();
    void onModeDrawColumn();
    void onModeDrawSlab();
    void onModeDrawWall();
    void onActionStructurePresets();
    void onActionLibrary(int tabIndex = 0);
    void openBarCreationDialog(TSA::Model::BarRole role = TSA::Model::BarRole::Beam);
    void openSurfaceCreationDialog(int surfaceType = 0);

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
    void onActionSaveAs();

private:
    void updateUndoRedoActions();
    void setupUi();
    void createActions();
    void createMenus();
    void createRibbon();
    void createToolBars();
    void createDockWindows();
    void createStatusBar();
    void applyTheme(bool dark);

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
    QAction* m_actionSaveAs = nullptr;
    QAction* m_actionExit = nullptr;
    std::unique_ptr<TSA::Project::ProjectManager> m_projectManager;

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
    QAction* m_actionDrawBar = nullptr;
    QAction* m_actionDrawBeam = nullptr;
    QAction* m_actionDrawColumn = nullptr;
    QAction* m_actionDrawSlab = nullptr;
    QAction* m_actionDrawWall = nullptr;
    QAction* m_actionStructurePresets = nullptr;

    TSA::UI::BarCreationDialog* m_barDialog = nullptr;
    TSA::UI::SurfaceCreationDialog* m_surfaceDialog = nullptr;
    TSA::Model::StructurePresets m_presets;

    QAction* m_actionNewNode = nullptr;
    QAction* m_actionAddCube = nullptr;
    QAction* m_actionDelete = nullptr;
    QAction* m_actionLibrary = nullptr;

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

    TSA::Model::StructuralClipboard m_clipboard;

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
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
