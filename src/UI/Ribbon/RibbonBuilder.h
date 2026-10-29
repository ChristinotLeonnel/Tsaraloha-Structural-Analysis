#pragma once

// Ruban de TSA (docs/UI_RIBBON.md) : dix onglets dans l'ordre du travail de l'ingénieur.
//   Accueil · Modélisation · Modifier · Grille et plans · Charges · Maillage · Analyse · Résultats · Affichage · Outils
// Chaque commande a une place, une seule (hors barre d'accès rapide) ; le ruban ne pose que des actions réelles de la
// fenêtre (aucun bouton de remplacement) ; quand la place manque, RibbonTab réduit puis replie les groupes (jamais de
// commande perdue).

#include <QCoreApplication>
#include <QObject>

#include <map>
#include <string>
#include <vector>

class QAction;
class QMenu;
class QWidget;

namespace TSA::UI
{

class RibbonBar;
class RibbonTab;
class RibbonPanel;

struct RibbonActions
{
    // Projet
    QAction* actionNew = nullptr;
    QAction* actionCloseProject = nullptr;
    QAction* actionOpen = nullptr;
    QAction* actionSave = nullptr;
    QAction* actionSaveAs = nullptr;
    QAction* actionExit = nullptr;   ///< menu de l'application seulement

    // Historique & presse-papiers
    QAction* actionUndo = nullptr;
    QAction* actionRedo = nullptr;
    QAction* actionCopyClipboard = nullptr;
    QAction* actionPasteClipboard = nullptr;
    QAction* actionCutClipboard = nullptr;
    QAction* actionPasteOriginal = nullptr;

    // Échanges, paramètres du projet, préférences
    QAction* actionImportIfc = nullptr;
    QAction* actionExportIfc = nullptr;
    QAction* actionImportTsa3d = nullptr;
    QAction* actionExportTsa3d = nullptr;
    QAction* actionImportViaModule = nullptr;
    QAction* actionBimGeometryVisible = nullptr;   ///< couche des objets IFC importés
    QAction* actionTopology = nullptr;
    QAction* actionStructurePresets = nullptr;     ///< paramètres de modélisation
    QAction* actionLanguage = nullptr;
    QAction* actionToggleTheme = nullptr;
    QAction* actionShortcutEditor = nullptr;

    // Modélisation
    QAction* actionSelectMode = nullptr;
    QAction* actionSelectAll = nullptr;
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
    QAction* actionSlabOpenings = nullptr;
    QAction* actionAddCube = nullptr;
    std::vector<QAction*> drawTools;                     ///< outils de dessin (registre)
    QAction* actionSecI = nullptr;
    QAction* actionSecRect = nullptr;
    QAction* actionSecCirc = nullptr;
    QAction* actionConcrete = nullptr;
    QAction* actionSteel = nullptr;
    QAction* actionLibrary = nullptr;
    QAction* actionFixed = nullptr;
    QAction* actionPinned = nullptr;
    QAction* actionRoller = nullptr;
    QAction* actionSurfaceSupports = nullptr;

    // Modifier
    QAction* actionMove3D = nullptr;
    QAction* actionMove = nullptr;
    QAction* actionCopy3D = nullptr;
    QAction* actionCopy = nullptr;
    QAction* actionRotate3D = nullptr;
    QAction* actionMirror = nullptr;
    QAction* actionSplitBars = nullptr;
    QAction* actionMergeNodes = nullptr;
    QAction* actionCleanModel = nullptr;
    QAction* actionToolInputMode = nullptr;             ///< saisie 3D / fenêtre des outils
    std::vector<QAction*> advancedModifyTools;           ///< outils de modification (registre)
    QAction* actionDelete = nullptr;

    // Grille et plans
    QAction* actionNewGrid = nullptr;
    QAction* actionGridManager = nullptr;
    QAction* actionManageLevels = nullptr;
    QAction* actionWorkPlaneXY = nullptr;
    QAction* actionWorkPlaneXZ = nullptr;
    QAction* actionWorkPlaneYZ = nullptr;
    QAction* actionWorkPlaneLevel = nullptr;
    QAction* actionWorkPlaneCustom = nullptr;
    QAction* actionWorkPlaneVisible = nullptr;
    QAction* actionViewNormalToPlane = nullptr;
    QAction* actionWorkPlaneNextRef = nullptr;
    QAction* actionWorkPlanePrevRef = nullptr;
    QAction* actionWorkPlaneBack = nullptr;
    QAction* actionWorkPlaneNextInter = nullptr;
    QAction* actionWorkPlanePrevInter = nullptr;
    QAction* actionWorkPlaneAlignInter = nullptr;
    QAction* actionGridSnap = nullptr;
    QAction* actionObjectSnap = nullptr;
    QAction* actionGridVisible = nullptr;
    QAction* actionLevelsVisible = nullptr;
    QAction* actionGridLabels = nullptr;
    QAction* actionRulersVisible = nullptr;
    QMenu* annotationScaleMenu = nullptr;   ///< « Échelle des annotations »
    QAction* actionCoordSystem = nullptr;
    QAction* actionMoveOrigin = nullptr;
    QAction* actionSectionCut = nullptr;

    // Charges
    QAction* actionPointLoad = nullptr;
    QAction* actionDistLoad = nullptr;
    QAction* actionTrapLoad = nullptr;
    QAction* actionBarPointLoad = nullptr;
    QAction* actionSurfaceLoad = nullptr;
    QAction* actionSelfWeight = nullptr;
    QAction* actionLoadCases = nullptr;
    QAction* actionLoadsVisible = nullptr;
    QAction* actionForcesVisible = nullptr;
    QAction* actionMomentsVisible = nullptr;
    QAction* actionLoadValuesVisible = nullptr;
    QAction* actionSurfaceLoadsVisible = nullptr;
    QMenu* loadScaleMenu = nullptr;         ///< « Échelle des charges »

    // Maillage
    QAction* actionMeshGen = nullptr;
    QAction* actionMeshRegenerate = nullptr;
    QAction* actionMeshClear = nullptr;
    QAction* actionMeshSettings = nullptr;
    QAction* actionMeshLocal = nullptr;
    QAction* actionMeshZone = nullptr;
    QAction* actionMeshPreview = nullptr;
    QAction* actionMeshQuality = nullptr;
    QAction* actionMeshShow = nullptr;
    QMenu* meshViewMenu = nullptr;          ///< « Affichage du maillage » (modes, filtres, sélection, résultats)
    QAction* actionMeshExport = nullptr;
    QAction* actionMeshImport = nullptr;

    // Analyse
    QAction* actionRunSolve = nullptr;
    QAction* actionAnalysisConfig = nullptr;
    QAction* actionAICheck = nullptr;
    QAction* actionAIAnalyze = nullptr;

    // Résultats
    QAction* actionResultsDock = nullptr;
    QAction* actionDeformedToggle = nullptr;
    QAction* actionResultsDisp = nullptr;
    QAction* actionResultsForces = nullptr;
    QAction* actionResultsStress = nullptr;
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
    QAction* actionReportTemplates = nullptr;
    QAction* actionAIExplain = nullptr;

    // Affichage
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
    QAction* actionNamedViews = nullptr;
    QAction* actionFitAll = nullptr;
    QAction* actionFitSelection = nullptr;
    QAction* actionResetView = nullptr;
    QAction* actionZoomIn = nullptr;
    QAction* actionZoomOut = nullptr;
    QAction* actionZoomWindow = nullptr;
    QAction* actionPreviousView = nullptr;
    QAction* actionNextView = nullptr;
    QAction* actionRotateLeft = nullptr;
    QAction* actionRotateRight = nullptr;
    std::vector<QAction*> modelDisplayModes;             ///< physique, filaire analytique, éléments finis, superposition
    std::vector<QAction*> renderModes;                   ///< rendu texturé, ombré, filaire
    QAction* renderEdges = nullptr;                      ///< arêtes des faces
    QAction* renderLightweight = nullptr;                ///< mode léger (PC modestes)
    QAction* actionNodesVisible = nullptr;
    QAction* actionNodeLabelsVisible = nullptr;
    QAction* actionSupportsVisible = nullptr;
    QAction* actionSupportLabelsVisible = nullptr;
    QAction* actionFullScreen = nullptr;
    QAction* actionToggleModelTree = nullptr;
    QAction* actionToggleProperties = nullptr;
    QAction* actionToggleVisibility = nullptr;
    QAction* actionToggleConsole = nullptr;
    // Espace de travail central (docs/UI.md) : dispositions, partage vertical, mode focus, disposition par défaut
    QAction* actionWorkspace3D = nullptr;
    QAction* actionWorkspaceBlueprint = nullptr;
    QAction* actionWorkspaceSplit = nullptr;
    QAction* actionWorkspaceVertical = nullptr;
    QAction* actionFocusMode = nullptr;
    QAction* actionResetLayout = nullptr;

    // Blueprint (programmation structurelle)
    QAction* actionBlueprintNew = nullptr;
    QAction* actionBlueprintOpen = nullptr;
    QAction* actionBlueprintSave = nullptr;
    QAction* actionBlueprintValidate = nullptr;
    QAction* actionBlueprintRun = nullptr;
    QAction* actionBlueprintDebug = nullptr;
    QAction* actionBlueprintFitAll = nullptr;
    QAction* actionBlueprintFitSelection = nullptr;

    // Outils
    QAction* actionMeasure = nullptr;
    std::map<std::string, QAction*> dimensionTools;      ///< outils de cotation (registre, par identifiant)
    std::vector<QAction*> dimensionCommands;             ///< modifier, supprimer, nettoyer, afficher, style
    QAction* actionAIAssistant = nullptr;
    QAction* actionAIConfig = nullptr;
    QAction* actionExtensionManager = nullptr;
    QAction* actionModules = nullptr;
    QAction* actionHelp = nullptr;
    QAction* actionOnlineDocs = nullptr;
    QAction* actionShortcuts = nullptr;
    QAction* actionAbout = nullptr;
    QAction* actionReportProblem = nullptr;
    QAction* actionExportDiagnostic = nullptr;
};

class RibbonBuilder
{
    Q_DECLARE_TR_FUNCTIONS(TSA::UI::RibbonBuilder)

public:
    /// Construit les onze onglets et la barre d'accès rapide.
    static void buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    /// Actions que le ruban doit rendre atteignables (toutes celles de RibbonActions sauf « Quitter »), sans doublon.
    static std::vector<QAction*> placedActions(const RibbonActions& acts);

private:
    static void buildHomeTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildModelingTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildModifyTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildBlueprintTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildGridTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildLoadsTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildMeshTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildAnalysisTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildResultsTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildViewTab(RibbonBar* bar, const RibbonActions& acts);
    static void buildToolsTab(RibbonBar* bar, const RibbonActions& acts);
};

} // namespace TSA::UI
