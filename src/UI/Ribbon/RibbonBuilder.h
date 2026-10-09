#pragma once

#include <vector>
#include <QObject>

class QAction;
class QWidget;

namespace TSA::UI
{

class RibbonBar;
class RibbonTab;
class RibbonPanel;

struct RibbonActions
{
    // 1. Accueil & Fichier
    QAction* actionNew = nullptr;
    QAction* actionCloseProject = nullptr;
    QAction* actionOpen = nullptr;
    QAction* actionSave = nullptr;
    QAction* actionSaveAs = nullptr;
    QAction* actionExit = nullptr;

    // Historique & Presse-papier
    QAction* actionUndo = nullptr;
    QAction* actionRedo = nullptr;
    QAction* actionCopyClipboard = nullptr;
    QAction* actionPasteClipboard = nullptr;

    // 2. Modélisation
    QAction* actionDrawNode = nullptr;
    QAction* actionNewNode = nullptr;
    QAction* actionDrawBar = nullptr;
    QAction* actionDrawBeam = nullptr;
    QAction* actionDrawColumn = nullptr;
    QAction* actionDrawCable = nullptr;
    QAction* actionDrawSlab = nullptr;
    QAction* actionDrawWall = nullptr;
    QAction* actionTruss = nullptr;
    QAction* actionFooting = nullptr;
    QAction* actionAddCube = nullptr;
    QAction* actionStructurePresets = nullptr;

    // Trame & Niveaux
    QAction* actionNewGrid = nullptr;
    QAction* actionGridManager = nullptr;
    QAction* actionManageLevels = nullptr;

    // 3. Structure
    QAction* actionSecI = nullptr;
    QAction* actionSecRect = nullptr;
    QAction* actionSecCirc = nullptr;
    QAction* actionConcrete = nullptr;
    QAction* actionSteel = nullptr;
    QAction* actionFixed = nullptr;
    QAction* actionPinned = nullptr;
    QAction* actionRoller = nullptr;
    QAction* actionLibrary = nullptr;
    QAction* actionExtensionManager = nullptr;

    // 4. Charges & Actions
    QAction* actionPointLoad = nullptr;
    QAction* actionDistLoad = nullptr;
    QAction* actionLoadCases = nullptr;
    QAction* actionLoadsVisible = nullptr;
    QAction* actionForcesVisible = nullptr;
    QAction* actionMomentsVisible = nullptr;
    QAction* actionLoadValuesVisible = nullptr;

    // 5. Analyse & Calcul
    QAction* actionMeshGen = nullptr;
    QAction* actionAnalysisConfig = nullptr;
    QAction* actionRunSolve = nullptr;

    QAction* actionResultsDock = nullptr;
    QAction* actionResultsDisp = nullptr;
    QAction* actionResultsForces = nullptr;
    QAction* actionResultsStress = nullptr;
    QAction* actionDeformedToggle = nullptr;
    QAction* actionDiagramMz = nullptr;
    QAction* actionDiagramMy = nullptr;
    QAction* actionDiagramMx = nullptr;
    QAction* actionDiagramVz = nullptr;
    QAction* actionDiagramVy = nullptr;
    QAction* actionDiagramN = nullptr;
    QAction* actionDiagramDeflection = nullptr;
    QAction* actionDiagramNone = nullptr;
    QAction* actionReactionsToggle = nullptr;
    QAction* actionFitModel = nullptr;
    QAction* actionFitResults = nullptr;
    QAction* actionFitDeformed = nullptr;
    QAction* actionOpenNDC = nullptr;

    // 6. Édition & Transformations
    QAction* actionSelectMode = nullptr;
    QAction* actionMove3D = nullptr;
    QAction* actionMove = nullptr;
    QAction* actionCopy3D = nullptr;
    QAction* actionCopy = nullptr;
    QAction* actionRotate3D = nullptr;
    QAction* actionMirror = nullptr;
    QAction* actionSplitBars = nullptr;
    QAction* actionMergeNodes = nullptr;
    QAction* actionCleanModel = nullptr;
    QAction* actionImportIfc = nullptr;
    QAction* actionExportIfc = nullptr;
    QAction* actionToolInputMode = nullptr;             ///< saisie 3D / fenêtre des outils
    std::vector<QAction*> advancedModifyTools;           ///< outils de modification (registre)
    std::vector<QAction*> drawTools;                     ///< outils de dessin (registre)
    QAction* actionMoveOrigin = nullptr;
    QAction* actionDelete = nullptr;

    // 7. Affichage & Vues
    QAction* actionView3D = nullptr;
    QAction* actionViewXY = nullptr;
    QAction* actionViewXZ = nullptr;
    QAction* actionViewYZ = nullptr;
    QAction* actionViewTop = nullptr;
    QAction* actionViewBottom = nullptr;
    QAction* actionViewFront = nullptr;
    QAction* actionViewBack = nullptr;
    QAction* actionViewLeft = nullptr;
    QAction* actionViewRight = nullptr;
    QAction* actionViewIsometric = nullptr;
    QAction* actionViewHome = nullptr;

    QAction* actionFitAll = nullptr;
    QAction* actionFitSelection = nullptr;
    QAction* actionResetView = nullptr;
    QAction* actionZoomIn = nullptr;
    QAction* actionZoomOut = nullptr;
    QAction* actionZoomWindow = nullptr;
    QAction* actionPreviousView = nullptr;
    QAction* actionNextView = nullptr;

    QAction* actionCoordSystem = nullptr;
    QAction* actionWorkPlaneXY = nullptr;
    QAction* actionWorkPlaneXZ = nullptr;
    QAction* actionWorkPlaneYZ = nullptr;
    QAction* actionWorkPlaneLevel = nullptr;
    QAction* actionWorkPlaneCustom = nullptr;
    QAction* actionWorkPlaneVisible = nullptr;
    QAction* actionViewNormalToPlane = nullptr;
    QAction* actionSectionCut = nullptr;

    QAction* actionGridVisible = nullptr;
    QAction* actionLevelsVisible = nullptr;
    QAction* actionGridLabels = nullptr;
    QAction* actionGridSnap = nullptr;
    QAction* actionObjectSnap = nullptr;
    QAction* actionRulersVisible = nullptr;
    QAction* actionNodesVisible = nullptr;
    QAction* actionNodeLabelsVisible = nullptr;
    QAction* actionFullScreen = nullptr;

    QAction* actionToggleModelTree = nullptr;
    QAction* actionToggleProperties = nullptr;
    QAction* actionToggleVisibility = nullptr;
    QAction* actionToggleConsole = nullptr;

    // 8. Outils & Préférences
    QAction* actionMeasure = nullptr;
    QAction* actionToggleTheme = nullptr;
    QAction* actionHelp = nullptr;
    QAction* actionShortcuts = nullptr;
    QAction* actionAbout = nullptr;

    // 9. IA Co-Engineering
    QAction* actionAIAssistant = nullptr;
    QAction* actionAIConfig = nullptr;
    QAction* actionAICheck = nullptr;
    QAction* actionAIAnalyze = nullptr;
    QAction* actionAIExplain = nullptr;
};

class RibbonBuilder
{
public:
    static void buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);

    static RibbonTab* buildHomeTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildModelingTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildStructureTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildLoadsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildAnalysisTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildCalculationTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildResultsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildEditTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildViewTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildToolsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
};

} // namespace TSA::UI

