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
    // Fichier
    QAction* actionNew = nullptr;
    QAction* actionOpen = nullptr;
    QAction* actionSave = nullptr;
    QAction* actionExit = nullptr;

    // Sélection & Édition
    QAction* actionSelectMode = nullptr;
    QAction* actionMove = nullptr;
    QAction* actionCopy = nullptr;
    QAction* actionDelete = nullptr;

    // Géométrie
    QAction* actionDrawNode = nullptr;
    QAction* actionNewNode = nullptr;
    QAction* actionAddCube = nullptr;

    // Structure
    QAction* actionDrawBeam = nullptr;
    QAction* actionNewBeam = nullptr;
    QAction* actionDrawColumn = nullptr;
    QAction* actionNewColumn = nullptr;
    QAction* actionDrawSlab = nullptr;
    QAction* actionNewSlab = nullptr;

    // Grilles & Niveaux
    QAction* actionNewGrid = nullptr;
    QAction* actionGridManager = nullptr;
    QAction* actionManageLevels = nullptr;
    QAction* actionGridVisible = nullptr;
    QAction* actionGridSnap = nullptr;
    QAction* actionGridLabels = nullptr;
    QAction* actionLevelsVisible = nullptr;
    QAction* actionRulersVisible = nullptr;

    // Vues & Navigation
    QAction* actionViewXY = nullptr;
    QAction* actionViewYZ = nullptr;
    QAction* actionViewXZ = nullptr;
    QAction* actionView3D = nullptr;
    QAction* actionCoordSystem = nullptr;
    QAction* actionSectionCut = nullptr;
    QAction* actionFitAll = nullptr;
    QAction* actionResetView = nullptr;

    // Affichage des Panneaux Docks
    QAction* actionToggleModelTree = nullptr;
    QAction* actionToggleProperties = nullptr;
    QAction* actionToggleVisibility = nullptr;
    QAction* actionToggleConsole = nullptr;

    // Thème & Documentation
    QAction* actionToggleTheme = nullptr;
    QAction* actionHelp = nullptr;
    QAction* actionShortcuts = nullptr;
    QAction* actionAbout = nullptr;

    // Éléments avancés & Métier
    QAction* actionWall = nullptr;
    QAction* actionTruss = nullptr;
    QAction* actionFooting = nullptr;

    // Profilés & Sections
    QAction* actionSecI = nullptr;
    QAction* actionSecRect = nullptr;
    QAction* actionSecCirc = nullptr;

    // Matériaux
    QAction* actionConcrete = nullptr;
    QAction* actionSteel = nullptr;

    // Appuis
    QAction* actionFixed = nullptr;
    QAction* actionPinned = nullptr;
    QAction* actionRoller = nullptr;

    // Charges & Analyse
    QAction* actionPointLoad = nullptr;
    QAction* actionDistLoad = nullptr;
    QAction* actionMoment = nullptr;
    QAction* actionSeismic = nullptr;
    QAction* actionMeshGen = nullptr;
    QAction* actionRunSolve = nullptr;
    QAction* actionModal = nullptr;

    // Résultats & Outils
    QAction* actionResultsDisp = nullptr;
    QAction* actionResultsForces = nullptr;
    QAction* actionResultsStress = nullptr;
    QAction* actionMeasure = nullptr;
};

class RibbonBuilder
{
public:
    static void buildAllTabs(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);

    static RibbonTab* buildHomeTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildStructureTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildAnalysisTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
    static RibbonTab* buildViewTab(RibbonBar* bar, const RibbonActions& acts, QWidget* parentWindow);
};

} // namespace TSA::UI
