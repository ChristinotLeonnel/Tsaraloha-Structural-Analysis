#pragma once

#include <QMainWindow>
#include "../Model/SelectionQuery.h"
#include "../Model/ModelCleanup.h"
#include <QImage>
#include <QPointer>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include "../Model/CreationPresets.h"
#include "../Model/Beam.h"
#include "../Model/StructuralClipboard.h"
#include "../Analysis/Engine/AnalysisContext.h"

namespace TSA::Model { class Model; class SupportDefinition; }
namespace TSA::AI { class AIOrchestrator; }
namespace TSA::Automation { class AutomationServer; }
namespace TSA::UI { class AICoEngineeringDock; class AIRuntimeDialog; struct NewProjectSettings; enum class ProjectTemplate; }
namespace TSA::Analysis { class ResultsModel; class ResultsValidityGuard; class OpenSeesSolver; class AnalysisEngineRegistry; class AnalysisManager; class AnalysisController; }
namespace TSA::Coordinate { class WorkPlane; }
namespace TSA::Interaction { class ModelingTool; class ModelingToolRegistry; struct ToolContext; }
namespace TSA::Project { class ProjectManager; class ProjectSession; }
namespace TSA::Viewer { class SelectionManager; }
namespace TSA::UndoRedo { class CommandManager; }
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
    class Diagram2DWidget;
    class SectionCutDialog;
    class RibbonBar;
    class VisibilityDock;
    class StructuralElementsDock;
    class LogConsoleDock;
    class HelpDialog;
    class BarCreationDialog;
    class CableCreationDialog;
    class SurfaceCreationDialog;
    class GridDialog;
    class GridSettingsDialog;
    class WorkPlaneDialog;
    class ProjectionViewDock;
    class ResultsDockWidget;
    class AnalysisDataDock;
    class WindowManager;
    class ProjectStatusOverlay;
    class TSALogoOverlay;
    class AnalysisEngineOptionsRegistry;
}

namespace TSA::NDC
{
    class NDCViewerWidget;
}

class OccView;
class QAction;
class QActionGroup;
class QToolButton;
class QMenu;
class QTimer;
class QStatusBar;
class QLabel;
class QDockWidget;

/// ProjectWorkspace de TSA : viewport, ruban, docks et barre d'état d'un projet ouvert.
/// Embarqué dans TSA::UI::AppShell (page du mode Workspace) ; créé au premier projet ouvert ou créé.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    TSA::Model::Model* model() { return m_model; }
    const TSA::Model::Model* model() const { return m_model; }

    /// Résultats du dernier calcul (nul si aucun) ; voir le signal resultsChanged().
    std::shared_ptr<const TSA::Analysis::ResultsModel> resultsModel() const { return m_resultsModel; }

    TSA::UndoRedo::CommandManager* commandManager() { return m_commandManager; }
    const TSA::UndoRedo::CommandManager* commandManager() const { return m_commandManager; }

    TSA::Grid::GridManager* gridManager() { return m_gridManager; }
    const TSA::Grid::GridManager* gridManager() const { return m_gridManager; }

    TSA::Project::ProjectManager* projectManager() { return m_projectManager; }
    const TSA::Project::ProjectManager* projectManager() const { return m_projectManager; }

    TSA::UI::WindowManager* windowManager() { return m_windowManager.get(); }
    const TSA::UI::WindowManager* windowManager() const { return m_windowManager.get(); }

    bool loadFile(const QString& filePath);
    bool saveFile(const QString& filePath);
    bool maybeSave();
    void updateWindowTitle();

    // Cycle de vie du projet (piloté par AppShell)
    /// Initialise un modèle selon le gabarit puis crée le fichier .tsa (ajouté aux projets récents).
    bool createProject(const TSA::UI::NewProjectSettings& settings);
    /// Propose d'enregistrer, mémorise l'aperçu puis décharge le projet (modèle, résultats, sélection).
    bool closeProject();
    /// Fermeture de l'application : enregistrement éventuel, aperçu et disposition des panneaux.
    bool prepareToClose();
    /// Faux (avec un message) si un calcul tourne : le projet ne peut alors être ni remplacé ni
    /// fermé, ses résultats seraient publiés sur un autre modèle. action : « quitter », « ouvrir un
    /// autre projet »…
    bool confirmNoRunningAnalysis(const QString& action);

    QAction* actionSave() const { return m_actionSave; }
    QAction* actionSaveAs() const { return m_actionSaveAs; }
    QAction* actionUndo() const { return m_actionUndo; }
    QAction* actionRedo() const { return m_actionRedo; }
    /// Menus classiques (Fichier, Édition…), présentés dans le menu d'application de la barre de titre.
    QList<QMenu*> applicationMenus() const;
    /// Synchronise l'action « Plein écran » avec l'état de la fenêtre hôte.
    void setFullScreenState(bool fullScreen);

signals:
    void newProjectRequested();
    void openProjectRequested();
    void closeProjectRequested();
    void exitRequested();
    void fullScreenRequested(bool fullScreen);
    void previewCaptured(const QString& path, const QImage& image);
    /// Nouveaux résultats publiés après un calcul, ou résultats effacés (projet fermé / rechargé).
    void resultsChanged();

private slots:
    void onFitAll();
    void onResetView();
    void onActionSelectAll();
    /// Applique une sélection ensembliste (SelectionQuery) et resynchronise viewport / panneaux.
    void applyElementSelection(const TSA::Model::ElementSet& elements, const QString& description);

    // Grille 3D & Niveaux
    void onNewGrid();
    void onGridManagerDialog();
    void onManageLevels();
    void onToggleGridVisible(bool checked);
    void onToggleGridSnap(bool checked);
    void onToggleObjectSnap(bool checked);
    void onToggleGridLabels(bool checked);
    void onToggleNodesVisible(bool checked);
    void onToggleNodeLabelsVisible(bool checked);
    void onToggleSupportsVisible(bool checked);
    void onToggleSupportLabelsVisible(bool checked);
    void onToggleLoadsVisible(bool checked);
    void onToggleForcesVisible(bool checked);
    void onToggleMomentsVisible(bool checked);
    void onToggleLoadValuesVisible(bool checked);
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
    void onModeDrawCable();
    void onModeDrawSlab();
    void onModeDrawWall();
    void onActionStructurePresets();
    void onActionLibrary(int tabIndex = 0);
    void onActionExtensionManager();
    void openBarCreationDialog(TSA::Model::BarRole role = TSA::Model::BarRole::Beam);
    // Mode de tracé du viewport (et bouton du ruban) piloté par une fenêtre de tracé : à son
    // ouverture et quand elle redevient active après une annulation par Échap.
    void activateBarDrawing(TSA::Model::BarRole role);
    void activateCableDrawing();
    void activateSurfaceDrawing(int surfaceType);
    void openCableCreationDialog();
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
    /// Projet ouvert : modèle, commandes, grilles, fichier (base partagée avec TSALab, ADR-024).
    /// Déclarée en premier : détruite en dernier, après les vues qui l'observent.
    std::unique_ptr<TSA::Project::ProjectSession> m_session;
    TSA::Model::Model* m_model = nullptr;                    ///< = &m_session->model()
    TSA::UndoRedo::CommandManager* m_commandManager = nullptr;
    std::unique_ptr<TSA::Viewer::SelectionManager> m_selectionManager;
    TSA::Grid::GridManager* m_gridManager = nullptr;
    TSA::Grid::GridSnapManager* m_gridSnapManager = nullptr;
    std::unique_ptr<TSA::UI::WindowManager> m_windowManager;

    OccView* m_occView = nullptr;
    TSA::UI::ViewportContainer* m_viewportContainer = nullptr;
    QDockWidget* m_diagramDock = nullptr;
    TSA::UI::Diagram2DWidget* m_diagramWidget = nullptr;
    QDockWidget* m_ndcDock = nullptr;
    TSA::NDC::NDCViewerWidget* m_ndcWidget = nullptr;
    /// Résultats courants du contrôleur d'analyse (copie du pointeur pour les vues de la fenêtre).
    std::shared_ptr<TSA::Analysis::ResultsModel> m_resultsModel;
    void onResultsBecameStale();
    std::unique_ptr<TSA::Analysis::OpenSeesSolver> m_openSeesSolver;
    TSA::UI::ModelTreeWidget* m_modelTree = nullptr;
    TSA::UI::PropertyPanel*   m_propertyPanel = nullptr;
    TSA::UI::RibbonBar*       m_ribbonBar = nullptr;

    QDockWidget* m_modelTreeDock = nullptr;
    QDockWidget* m_propertiesDock = nullptr;
    TSA::UI::VisibilityDock* m_visibilityDock = nullptr;
    TSA::UI::StructuralElementsDock* m_elementsDock = nullptr;
    TSA::UI::LogConsoleDock* m_consoleDock = nullptr;
    TSA::UI::ProjectionViewDock* m_projectionViewDock = nullptr;
    TSA::UI::ResultsDockWidget* m_resultsDock = nullptr;
    TSA::UI::AnalysisDataDock* m_analysisDataDock = nullptr;
    QDockWidget* m_projectStatusDock = nullptr;
    TSA::UI::ProjectStatusOverlay* m_projectStatusWidget = nullptr;
    // Analyse multi-moteurs : contrôleur partagé de la session (moteurs, réglages du projet, calcul en
    // tâche de fond, résultats, invalidation) et panneaux d'options des moteurs (propres à l'interface).
    TSA::Analysis::AnalysisController* m_analysis = nullptr;
    std::unique_ptr<TSA::UI::AnalysisEngineOptionsRegistry> m_engineOptions;
    /// Calcule le contexte avec le moteur choisi (disponibilité, validation, calcul, publication).
    bool runAnalysis(const TSA::Analysis::AnalysisContext& context);
    // Paramètres d'analyse ↔ modèle (chunk SETT du .tsa, BUG-013)
    void storeAnalysisContextInModel();
    void restoreAnalysisContextFromModel();
    // Outils de modification / dessin (MainWindow_ModelingTools.cpp)
    std::unique_ptr<TSA::Interaction::ModelingToolRegistry> m_toolRegistry;
    std::unique_ptr<TSA::Interaction::ModelingTool> m_activeTool;
    std::map<std::string, QAction*> m_toolActions;
    QAction* m_toolLauncher = nullptr;     // bouton de la commande d'outil en cours
    bool m_toolDialogRunning = false;      // outil piloté par sa fenêtre de paramètres (modale)
    QAction* m_actionToolInputViewport = nullptr;
    bool m_toolInputInViewport = true;
    void createModelingToolActions();
    /// swapInputMode : utiliser l'autre mode de saisie (Maj + clic, ou action « numérique »).
    /// launcher : bouton qui lance l'outil (coloré tant que la commande est active) ; par défaut
    /// celui du registre des outils.
    void startModelingTool(const std::string& id, bool swapInputMode = false, QAction* launcher = nullptr);
    void startModelingToolImpl(const std::string& id, bool swapInputMode, QAction* launcher);
    /// État coloré des boutons de commande (outils de modification, déplacement de l'origine) =
    /// commande réellement active : décoloré dès qu'elle est terminée ou annulée (Échap).
    void syncToolActionStates();
    void applyActiveModelingTool();
    TSA::Interaction::ToolContext modelingToolContext() const;
    /// Nettoyage topologique en une transaction ; retourne le bilan (vide si rien n'a changé).
    std::string applyModelCleanup(const TSA::Model::CleanupOptions& options);
    /// Diffuse les résultats du contrôleur d'analyse à toutes les vues (viewport, docks, propriétés, NDC).
    void onAnalysisResultsChanged();

    QLabel*  m_statusProject = nullptr;
    QLabel*  m_statusView = nullptr;
    QLabel*  m_statusUnits = nullptr;
    QLabel*  m_statusLevel = nullptr;
    QLabel*  m_statusCoordinates = nullptr;
    QLabel*  m_statusCoordinatesLocal = nullptr;
    QLabel*  m_statusInfo = nullptr;
    TSA::UI::TSALogoOverlay* m_statusLogo = nullptr;

    // Actions Fichier
    QAction* m_actionNew = nullptr;
    QAction* m_actionOpen = nullptr;
    QAction* m_actionSave = nullptr;
    QAction* m_actionSaveAs = nullptr;
    QAction* m_actionExit = nullptr;
    TSA::Project::ProjectManager* m_projectManager = nullptr;  ///< = &m_session->project()

    QAction* m_actionFitAll = nullptr;
    QAction* m_actionResetView = nullptr;

    // Actions Grille & Règles
    QAction* m_actionNewGrid = nullptr;
    QAction* m_actionGridManager = nullptr;
    QAction* m_actionManageLevels = nullptr;
    QAction* m_actionGridVisible = nullptr;
    QAction* m_actionGridSnap = nullptr;
    QAction* m_actionObjectSnap = nullptr;
    QAction* m_actionGridLabels = nullptr;
    QAction* m_actionNodesVisible = nullptr;
    QAction* m_actionNodeLabelsVisible = nullptr;
    QAction* m_actionSupportsVisible = nullptr;
    QAction* m_actionSupportLabelsVisible = nullptr;
    QAction* m_actionLoadsVisible = nullptr;
    QAction* m_actionForcesVisible = nullptr;
    QAction* m_actionMomentsVisible = nullptr;
    QAction* m_actionLoadValuesVisible = nullptr;
    QAction* m_actionLevelsVisible = nullptr;
    QAction* m_actionRulersVisible = nullptr;
    QAction* m_actionDarkMode = nullptr;
    QAction* m_actionFullScreen = nullptr;

    // Actions Modes d'interaction / Dessin 3D
    QActionGroup* m_drawModeGroup = nullptr;
    QAction* m_actionSelectMode = nullptr;
    QAction* m_actionDrawNode = nullptr;
    QAction* m_actionDrawBar = nullptr;
    QAction* m_actionDrawBeam = nullptr;
    QAction* m_actionDrawColumn = nullptr;
    QAction* m_actionDrawCable = nullptr;
    QAction* m_actionDrawSlab = nullptr;
    QAction* m_actionDrawWall = nullptr;
    QAction* m_actionStructurePresets = nullptr;

    TSA::UI::BarCreationDialog* m_barDialog = nullptr;
    TSA::UI::CableCreationDialog* m_cableDialog = nullptr;
    TSA::UI::SurfaceCreationDialog* m_surfaceDialog = nullptr;
    TSA::Model::StructurePresets m_presets;

    QAction* m_actionNewNode = nullptr;
    QAction* m_actionAddCube = nullptr;
    QAction* m_actionSelectAll = nullptr;
    QAction* m_actionDelete = nullptr;
    QAction* m_actionLibrary = nullptr;
    QAction* m_actionExtensionManager = nullptr;

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
    QAction* m_actionMirror = nullptr;
    QAction* m_actionCleanModel = nullptr;
    QAction* m_actionExportIfc = nullptr;   ///< Exporter IFC 4.3 (src/BIM/IFC)
    QAction* m_actionImportIfc = nullptr;
    QAction* m_actionSplitBars = nullptr;
    QAction* m_actionMergeNodes = nullptr;

    TSA::Model::StructuralClipboard m_clipboard;

    // Actions Barre "Vue" & Navigation (AutoCAD / Robot SA style)
    QAction* m_actionViewXY = nullptr;
    QAction* m_actionViewYZ = nullptr;
    QAction* m_actionViewXZ = nullptr;
    QAction* m_actionView3D = nullptr;
    QAction* m_actionViewHome = nullptr;
    QAction* m_actionViewTop = nullptr;
    QAction* m_actionViewBottom = nullptr;
    QAction* m_actionViewFront = nullptr;
    QAction* m_actionViewBack = nullptr;
    QAction* m_actionViewLeft = nullptr;
    QAction* m_actionViewRight = nullptr;
    QAction* m_actionViewIsometric = nullptr;

    QAction* m_actionFitSelection = nullptr;
    QAction* m_actionZoomIn = nullptr;
    QAction* m_actionZoomOut = nullptr;
    QAction* m_actionZoomWindow = nullptr;
    QAction* m_actionRotateLeft = nullptr;
    QAction* m_actionRotateRight = nullptr;
    QAction* m_actionPreviousView = nullptr;
    QAction* m_actionNextView = nullptr;

    QAction* m_actionWorkPlaneXY = nullptr;
    QAction* m_actionWorkPlaneXZ = nullptr;
    QAction* m_actionWorkPlaneYZ = nullptr;
    QAction* m_actionWorkPlaneLevel = nullptr;
    QAction* m_actionWorkPlaneCustom = nullptr;
    QAction* m_actionWorkPlaneVisible = nullptr;
    QAction* m_actionViewNormalToPlane = nullptr;
    QActionGroup* m_workPlaneGroup = nullptr;

    QAction* m_actionCoordSystem = nullptr;
    QAction* m_actionSectionCut = nullptr;

    QLabel* m_statusWorkPlane = nullptr;
    QLabel* m_statusSnap = nullptr;
    QLabel* m_statusCounts = nullptr; // Nœuds / Éléments / Sélection (rafraîchi par minuteur)
    void updateStatusCounts();
    /// Masque / réaffiche les indicateurs secondaires de la barre d'état selon la largeur disponible.
    void fitStatusBar();

    // Aperçus du dernier état du modèle pour le Start Center (MainWindow_Preview.cpp)
    void createPreviewCapture();
    void schedulePreviewCapture(int delayMs = 0);
    void capturePreview(bool synchronousWrite);
    void onModelRevisionPolled();
    void onProjectFileOpened(const QString& path);
    void onProjectFileSaved(const QString& path);
    /// Remet le workspace sur un projet vierge (modèle, grille selon le gabarit, résultats, vues).
    void resetWorkspace(TSA::UI::ProjectTemplate projectTemplate);
    QTimer* m_previewTimer = nullptr;
    QAction* m_actionCloseProject = nullptr;
    quint64 m_lastPolledRevision = 0;

    // IA Co-Engineering (MainWindow_AI.cpp)
    void createAIComponents();
    void createAIStatusWidget(QStatusBar* bar);
    void updateAIStatusWidget();
    void openAIConfig(int page);
    TSA::AI::AIOrchestrator* m_aiOrchestrator = nullptr;
    TSA::Automation::AutomationServer* m_automation = nullptr;   ///< pont MCP (Claude Code), docs/MCP.md
    TSA::UI::AICoEngineeringDock* m_aiDock = nullptr;
    TSA::UI::AIRuntimeDialog* m_aiDialog = nullptr;
    QToolButton* m_statusAI = nullptr;
    QAction* m_actionAIAssistant = nullptr;
    QAction* m_actionAIConfig = nullptr;
    QAction* m_actionAICheck = nullptr;
    QAction* m_actionAIAnalyze = nullptr;
    QAction* m_actionAIExplain = nullptr;

    // Actions Thème & Aide
    QAction* m_actionToggleTheme = nullptr;
    QAction* m_actionHelp = nullptr;
    QAction* m_actionShortcuts = nullptr;
    QAction* m_actionAbout = nullptr;
    QAction* m_actionExportDiagnostic = nullptr;

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
    QAction* m_actionLoadCases = nullptr;
    QAction* m_actionMeshGen = nullptr;
    QAction* m_actionAnalysisConfig = nullptr;
    QAction* m_actionRunSolve = nullptr;

    QAction* m_actionResultsDisp = nullptr;
    QAction* m_actionResultsForces = nullptr;
    QAction* m_actionResultsStress = nullptr;
    QAction* m_actionDeformedToggle = nullptr;
    QAction* m_actionDiagramMz = nullptr;
    QAction* m_actionDiagramMy = nullptr;
    QAction* m_actionDiagramMx = nullptr;
    QAction* m_actionDiagramVz = nullptr;
    QAction* m_actionDiagramVy = nullptr;
    QAction* m_actionDiagramN = nullptr;
    QAction* m_actionDiagramDeflection = nullptr;
    QAction* m_actionDiagramNone = nullptr;
    QAction* m_actionReactionsToggle = nullptr;
    QAction* m_actionFitModel = nullptr;
    QAction* m_actionFitResults = nullptr;
    QAction* m_actionFitDeformed = nullptr;
    QAction* m_actionNoteDeCalcul = nullptr;

    QAction* m_actionMeasure = nullptr;

    TSA::UI::SectionCutDialog* m_sectionCutDialog = nullptr;
    TSA::UI::HelpDialog* m_helpDialog = nullptr;
    QPointer<TSA::UI::GridDialog> m_gridDialog;
    QPointer<TSA::UI::GridSettingsDialog> m_gridSettingsDialog;

private slots:
    void onFitSelection();
    void onZoomIn();
    void onZoomOut();
    void onZoomWindow();
    void onPreviousView();
    void onNextView();
    void onActionViewHome();
    void onActionViewTop();
    void onActionViewBottom();
    void onActionViewFront();
    void onActionViewBack();
    void onActionViewLeft();
    void onActionViewRight();
    void onActionViewIsometric();
    void onRotate2DLeft();
    void onRotate2DRight();
    void onWorkPlaneXY();
    void onWorkPlaneXZ();
    void onWorkPlaneYZ();
    void onWorkPlaneLevel();
    void onActionWorkPlaneCustom();
    void onActionToggleWorkPlaneVisible(bool checked);
    void onActionViewNormalToPlane();
    void onWorkPlaneChanged(const TSA::Coordinate::WorkPlane& wp);

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
    void onActionExportDiagnosticReport();

    // Slots Résultats, Note de Calcul et Multi-Port
    void onActionNoteDeCalcul();
    void onActionToggleDeformed(bool checked);
    void onActionToggleReactions(bool checked);
    void onActionDiagramMz();
    void onActionDiagramMy();
    void onActionDiagramMx();
    void onActionDiagramVz();
    void onActionDiagramVy();
    void onActionDiagramN();
    void onActionDiagramDeflection();
    void onActionDiagramNone();
    void onFitModel();
    void onFitResults();
    void onFitDeformed();

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
    /// Assigne une condition d'appui à des nœuds (une entrée Annuler, notification du modèle).
    void assignSupport(const std::set<int>& nodeIds, const TSA::Model::SupportDefinition& support, const QString& label);
    void onActionPointLoad();
    void onActionDistLoad();
    void onActionMoment();
    void onActionLoadCases();
    void onActionMeshGen();
    void onActionAnalysisConfig();
    void onActionRunSolve();
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
    void onActionMirror();
    void onActionCleanModel();
    void onActionExportIfc();
    void onActionImportIfc();
    void onActionSplitBars();
    void onActionMergeNodes();

    void onOriginMoveRequested(const gp_Pnt& newOrigin);
    void onPasteAtPointRequested(const gp_Pnt& target);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
