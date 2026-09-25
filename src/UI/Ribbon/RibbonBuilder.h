#pragma once

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

    // 4. Calcul
    QAction* actionPointLoad = nullptr;
    QAction* actionDistLoad = nullptr;
    QAction* actionMoment = nullptr;
    QAction* actionSeismic = nullptr;
    QAction* actionMeshGen = nullptr;
    QAction* actionRunSolve = nullptr;
    QAction* actionModal = nullptr;

    // 5. Résultats
    QAction* actionResultsDisp = nullptr;
    QAction* actionResultsForces = nullptr;
    QAction* actionResultsStress = nullptr;

    // 6. Édition & Transformations
    QAction* actionSelectMode = nullptr;
    QAction* actionMove3D = nullptr;
    QAction* actionMove = nullptr;
    QAction* actionCopy3D = nullptr;
    QAction* actionCopy = nullptr;
    QAction* actionRotate3D = nullptr;
    QAction* actionMoveOrigin = nullptr;
    QAction* actionDelete = nullptr;

    // 7. Affichage & Vues
    QAction* actionView3D = nullptr;
    QAction* actionViewXY = nullptr;
    QAction* actionViewXZ = nullptr;
    QAction* actionViewYZ = nullptr;
    QAction* actionFitAll = nullptr;
    QAction* actionResetView = nullptr;
    QAction* actionCoordSystem = nullptr;
    QAction* actionSectionCut = nullptr;

    QAction* actionGridVisible = nullptr;
    QAction* actionLevelsVisible = nullptr;
    QAction* actionGridLabels = nullptr;
    QAction* actionGridSnap = nullptr;
    QAction* actionRulersVisible = nullptr;
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
};

class RibbonBuilder
{
public:
    static void buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);

    static RibbonTab* buildHomeTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildModelingTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildStructureTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildCalculationTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildResultsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildEditTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildViewTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildToolsTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
};

} // namespace TSA::UI
