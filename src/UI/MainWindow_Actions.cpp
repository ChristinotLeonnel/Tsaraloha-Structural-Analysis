#include "Dock/AnalysisDataDock.h"
#include "../Coordinate/GeometryTolerance.h"
#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "../Coordinate/WorkPlane.h"
#include "../Coordinate/WorkPlaneManager.h"
#include "../Coordinate/LevelManager.h"
#include "../Coordinate/CoordinateSystem.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Properties/PropertyPanel.h"
#include "Ruler/ViewportContainer.h"
#include "Ribbon/RibbonBar.h"
#include "Ribbon/RibbonBuilder.h"
#include "Dock/VisibilityDock.h"
#include "Dock/StructuralElementsDock.h"
#include "Dock/LogConsoleDock.h"
#include "Dock/ProjectionViewDock.h"
#include "Dock/ResultsDockWidget.h"
#include "../Viewer/ResultsVisualManager.h"
#include "Dialogs/BarCreationDialog.h"
#include "Dialogs/CableCreationDialog.h"
#include "Theme/ThemeManager.h"
#include "WindowManager/WindowManager.h"
#include "Widgets/ProjectStatusOverlay.h"
#include "Diagrams/Diagram2DWidget.h"
#include "../NDC/NDCViewerWidget.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QLabel>
#include <QIcon>
#include <QKeySequence>

namespace
{
static inline QIcon makePlanIcon(const QColor&, const QColor&, const QColor&, const QString& l1, const QString& l2)
{
    if (l1 == "X" && l2 == "Y") return QIcon(":/icons/view/view_top.svg");
    if (l1 == "X" && l2 == "Z") return QIcon(":/icons/view/view_front.svg");
    if (l1 == "Y" && l2 == "Z") return QIcon(":/icons/view/view_side.svg");
    return QIcon(":/icons/view/view_3d.svg");
}

static inline QIcon make3DIsoIcon() { return QIcon(":/icons/view/view_3d.svg"); }
static inline QIcon makeCoordSystemIcon() { return QIcon(":/icons/view/coord_system.svg"); }
static inline QIcon makeSectionCutIcon() { return QIcon(":/icons/view/section_cut.svg"); }
static inline QIcon makeThemeIcon(bool dark) { return QIcon(dark ? ":/icons/common/theme_dark.svg" : ":/icons/common/theme_light.svg"); }
static inline QIcon makeHelpIcon() { return QIcon(":/icons/common/help.svg"); }
static inline QIcon makeShortcutsIcon() { return QIcon(":/icons/common/shortcuts.svg"); }
static inline QIcon makeAboutIcon() { return QIcon(":/icons/common/about.svg"); }
static inline QIcon makeRotateIcon() { return QIcon(":/icons/edit/rotate.svg"); }
static inline QIcon makeOriginMoveIcon() { return QIcon(":/icons/structure/struct_move.svg"); }
static inline QIcon makeUndoIcon() { return QIcon(":/icons/edit/undo.svg"); }
static inline QIcon makeRedoIcon() { return QIcon(":/icons/edit/redo.svg"); }
} // namespace

void MainWindow::createActions()
{
    // Actions Fichier
    m_actionNew = new QAction(tr("&Nouveau Projet"), this);
    m_actionNew->setIcon(QIcon(":/icons/file_new.svg"));
    m_actionNew->setToolTip(tr("Nouveau Projet (Ctrl+N)"));
    m_actionNew->setShortcut(QKeySequence::New);
    connect(m_actionNew, &QAction::triggered, this, &MainWindow::onActionNew);

    m_actionOpen = new QAction(tr("&Ouvrir..."), this);
    m_actionOpen->setIcon(QIcon(":/icons/file_open.svg"));
    m_actionOpen->setToolTip(tr("Ouvrir un projet existant (Ctrl+O)"));
    m_actionOpen->setShortcut(QKeySequence::Open);
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::onActionOpen);

    m_actionSave = new QAction(tr("&Enregistrer"), this);
    m_actionSave->setIcon(QIcon(":/icons/file_save.svg"));
    m_actionSave->setToolTip(tr("Enregistrer le projet (Ctrl+S)"));
    m_actionSave->setShortcut(QKeySequence::Save);
    connect(m_actionSave, &QAction::triggered, this, &MainWindow::onActionSave);

    m_actionSaveAs = new QAction(tr("Enregistrer &sous..."), this);
    m_actionSaveAs->setIcon(QIcon(":/icons/file/file_save_as.svg"));
    m_actionSaveAs->setToolTip(tr("Enregistrer le projet sous un nouveau nom (Ctrl+Shift+S)"));
    m_actionSaveAs->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_actionSaveAs, &QAction::triggered, this, &MainWindow::onActionSaveAs);

    m_actionExit = new QAction(tr("&Quitter"), this);
    m_actionExit->setIcon(QIcon(":/icons/file_exit.svg"));
    m_actionExit->setToolTip(tr("Quitter l'application (Alt+F4)"));
    m_actionExit->setShortcut(QKeySequence::Quit);
    connect(m_actionExit, &QAction::triggered, this, &QWidget::close);

    // Actions Édition & Transformation
    m_actionMove = new QAction(tr("Translation &Numérique (Dialogue)..."), this);
    m_actionMove->setIcon(QIcon(":/icons/move.svg"));
    m_actionMove->setToolTip(tr("Translation numérique par incréments dX, dY, dZ (Ctrl+Shift+M)..."));
    m_actionMove->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(m_actionMove, &QAction::triggered, this, &MainWindow::onActionMove);

    m_actionCopy = new QAction(tr("&Copie Numérique (Répétition)..."), this);
    m_actionCopy->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopy->setToolTip(tr("Copie numérique paramétrique avec répétitions multiples (Ctrl+D)..."));
    m_actionCopy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_actionCopy, &QAction::triggered, this, &MainWindow::onActionCopy);

    m_actionDelete = new QAction(tr("&Supprimer"), this);
    m_actionDelete->setIcon(QIcon(":/icons/delete.svg"));
    m_actionDelete->setToolTip(tr("Supprimer les éléments sélectionnés (Suppr)"));
    m_actionDelete->setShortcut(QKeySequence::Delete);
    connect(m_actionDelete, &QAction::triggered, this, &MainWindow::onActionDeleteSelected);

    m_actionSelectAll = new QAction(tr("&Tout Sélectionner"), this);
    m_actionSelectAll->setIcon(QIcon(":/icons/edit/select_all.svg"));
    m_actionSelectAll->setToolTip(tr("Sélectionner tous les éléments du modèle (Ctrl+A)"));
    m_actionSelectAll->setShortcut(QKeySequence::SelectAll);
    connect(m_actionSelectAll, &QAction::triggered, this, &MainWindow::onActionSelectAll);

    // Actions Vues et Projections
    m_actionViewXY = new QAction(tr("Plan &XY (Vue d'étage)"), this);
    m_actionViewXY->setIcon(makePlanIcon(QColor(255, 140, 140), Qt::blue, Qt::darkGreen, "X", "Y"));
    m_actionViewXY->setToolTip(tr("Vue en Plan XY (Étage actif)"));
    connect(m_actionViewXY, &QAction::triggered, this, &MainWindow::onActionViewXY);

    m_actionViewYZ = new QAction(tr("Plan &YZ (Coupe latérale / Pignon)"), this);
    m_actionViewYZ->setIcon(makePlanIcon(QColor(140, 160, 255), Qt::darkGreen, Qt::red, "Y", "Z"));
    m_actionViewYZ->setToolTip(tr("Vue en Plan YZ (Coupe latérale / Pignon)"));
    connect(m_actionViewYZ, &QAction::triggered, this, &MainWindow::onActionViewYZ);

    m_actionViewXZ = new QAction(tr("Plan &XZ (Élévation de face / Portique)"), this);
    m_actionViewXZ->setIcon(makePlanIcon(QColor(140, 230, 160), Qt::blue, Qt::red, "X", "Z"));
    m_actionViewXZ->setToolTip(tr("Vue en Plan XZ (Élévation de face / Portique)"));
    connect(m_actionViewXZ, &QAction::triggered, this, &MainWindow::onActionViewXZ);

    m_actionView3D = new QAction(tr("Vue &3D (Axonométrique)"), this);
    m_actionView3D->setIcon(make3DIsoIcon());
    m_actionView3D->setToolTip(tr("Vue 3D Isométrique"));
    connect(m_actionView3D, &QAction::triggered, this, &MainWindow::onActionView3D);

    m_actionCoordSystem = new QAction(tr("Repère &Local / Global"), this);
    m_actionCoordSystem->setIcon(makeCoordSystemIcon());
    m_actionCoordSystem->setCheckable(true);
    m_actionCoordSystem->setToolTip(tr("Basculer entre Repère Global (GCS) et Repère Local (LCS)"));
    connect(m_actionCoordSystem, &QAction::triggered, this, &MainWindow::onActionCoordSystem);

    m_actionSectionCut = new QAction(tr("&Coupes de la structure (Section 3D)..."), this);
    m_actionSectionCut->setIcon(makeSectionCutIcon());
    m_actionSectionCut->setToolTip(tr("Définir et activer des plans de coupe 3D (Graphic3d_ClipPlane)"));
    connect(m_actionSectionCut, &QAction::triggered, this, &MainWindow::onActionSectionCut);

    m_actionFitAll = new QAction(tr("&Zoom Étendu (Fit All)"), this);
    m_actionFitAll->setIcon(QIcon(":/icons/fit_all.svg"));
    m_actionFitAll->setToolTip(tr("Ajuster la vue à l'ensemble du modèle (F)"));
    m_actionFitAll->setShortcut(QKeySequence(Qt::Key_F));
    connect(m_actionFitAll, &QAction::triggered, this, &MainWindow::onFitAll);

    m_actionResetView = new QAction(tr("&Réinitialiser Vue"), this);
    m_actionResetView->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionResetView->setToolTip(tr("Réinitialiser l'orientation de caméra 3D (R)"));
    m_actionResetView->setShortcut(QKeySequence(Qt::Key_R));
    connect(m_actionResetView, &QAction::triggered, this, &MainWindow::onResetView);

    m_actionFitSelection = new QAction(tr("Zoom &Sélection (Fit Selection)"), this);
    m_actionFitSelection->setIcon(QIcon(":/icons/fit_all.svg"));
    m_actionFitSelection->setToolTip(tr("Cadrer la vue sur les éléments sélectionnés (Maj+F)"));
    m_actionFitSelection->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F));
    connect(m_actionFitSelection, &QAction::triggered, this, &MainWindow::onFitSelection);

    m_actionZoomIn = new QAction(tr("Zoom &Avant (+)"), this);
    m_actionZoomIn->setIcon(QIcon(":/icons/zoom_in.svg"));
    m_actionZoomIn->setToolTip(tr("Agrandir la vue (+)"));
    m_actionZoomIn->setShortcut(QKeySequence(Qt::Key_Plus));
    connect(m_actionZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_actionZoomOut = new QAction(tr("Zoom A&rrière (-)"), this);
    m_actionZoomOut->setIcon(QIcon(":/icons/zoom_out.svg"));
    m_actionZoomOut->setToolTip(tr("Réduire la vue (-)"));
    m_actionZoomOut->setShortcut(QKeySequence(Qt::Key_Minus));
    connect(m_actionZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_actionZoomWindow = new QAction(tr("Zoom &Fenêtre"), this);
    m_actionZoomWindow->setIcon(QIcon(":/icons/zoom_window.svg"));
    m_actionZoomWindow->setToolTip(tr("Agrandir une région rectangulaire par glisser-déposer"));
    connect(m_actionZoomWindow, &QAction::triggered, this, &MainWindow::onZoomWindow);

    m_actionRotateLeft = new QAction(tr("Pivoter Vue 2D &Gauche (-15°)"), this);
    m_actionRotateLeft->setIcon(QIcon(":/icons/edit/rotate.svg"));
    m_actionRotateLeft->setToolTip(tr("Pivoter la vue de 15° vers la gauche"));
    connect(m_actionRotateLeft, &QAction::triggered, this, &MainWindow::onRotate2DLeft);

    m_actionRotateRight = new QAction(tr("Pivoter Vue 2D &Droite (+15°)"), this);
    m_actionRotateRight->setIcon(QIcon(":/icons/edit/rotate.svg"));
    m_actionRotateRight->setToolTip(tr("Pivoter la vue de 15° vers la droite"));
    connect(m_actionRotateRight, &QAction::triggered, this, &MainWindow::onRotate2DRight);

    m_actionPreviousView = new QAction(tr("Vue &Précédente"), this);
    m_actionPreviousView->setIcon(QIcon(":/icons/edit/undo.svg"));
    m_actionPreviousView->setToolTip(tr("Revenir à la vue de caméra précédente (Alt+Gauche)"));
    m_actionPreviousView->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    m_actionPreviousView->setEnabled(false);
    connect(m_actionPreviousView, &QAction::triggered, this, &MainWindow::onPreviousView);

    m_actionNextView = new QAction(tr("Vue &Suivante"), this);
    m_actionNextView->setIcon(QIcon(":/icons/edit/redo.svg"));
    m_actionNextView->setToolTip(tr("Rétablir la vue de caméra suivante (Alt+Droite)"));
    m_actionNextView->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    m_actionNextView->setEnabled(false);
    connect(m_actionNextView, &QAction::triggered, this, &MainWindow::onNextView);

    m_actionViewHome = new QAction(tr("Vue d'&Accueil (Home)"), this);
    m_actionViewHome->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionViewHome->setToolTip(tr("Réorienter la caméra en vue d'accueil 3D (Home)"));
    m_actionViewHome->setShortcut(QKeySequence(Qt::Key_Home));
    connect(m_actionViewHome, &QAction::triggered, this, &MainWindow::onActionViewHome);

    m_actionViewTop = new QAction(tr("Vue de &Dessus (Top)"), this);
    m_actionViewTop->setIcon(makePlanIcon(QColor(255, 140, 140), Qt::blue, Qt::darkGreen, "X", "Y"));
    m_actionViewTop->setToolTip(tr("Orienter la vue de dessus (Plan XY, +Z) (Num7)"));
    m_actionViewTop->setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_7));
    connect(m_actionViewTop, &QAction::triggered, this, &MainWindow::onActionViewTop);

    m_actionViewBottom = new QAction(tr("Vue de Dessou&s (Bottom)"), this);
    m_actionViewBottom->setIcon(makePlanIcon(QColor(200, 200, 200), Qt::blue, Qt::darkGreen, "X", "Y"));
    m_actionViewBottom->setToolTip(tr("Orienter la vue de dessous (-Z) (Ctrl+Num7)"));
    m_actionViewBottom->setShortcut(QKeySequence(Qt::CTRL | Qt::KeypadModifier | Qt::Key_7));
    connect(m_actionViewBottom, &QAction::triggered, this, &MainWindow::onActionViewBottom);

    m_actionViewFront = new QAction(tr("Vue de &Face (Front)"), this);
    m_actionViewFront->setIcon(makePlanIcon(QColor(140, 230, 160), Qt::blue, Qt::red, "X", "Z"));
    m_actionViewFront->setToolTip(tr("Orienter la vue de face (Élévation XZ, -Y) (Num1)"));
    m_actionViewFront->setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_1));
    connect(m_actionViewFront, &QAction::triggered, this, &MainWindow::onActionViewFront);

    m_actionViewBack = new QAction(tr("Vue Arriè&re (Back)"), this);
    m_actionViewBack->setIcon(makePlanIcon(QColor(140, 200, 160), Qt::blue, Qt::red, "X", "Z"));
    m_actionViewBack->setToolTip(tr("Orienter la vue arrière (+Y) (Ctrl+Num1)"));
    m_actionViewBack->setShortcut(QKeySequence(Qt::CTRL | Qt::KeypadModifier | Qt::Key_1));
    connect(m_actionViewBack, &QAction::triggered, this, &MainWindow::onActionViewBack);

    m_actionViewLeft = new QAction(tr("Vue &Gauche (Left)"), this);
    m_actionViewLeft->setIcon(makePlanIcon(QColor(140, 160, 255), Qt::darkGreen, Qt::red, "Y", "Z"));
    m_actionViewLeft->setToolTip(tr("Orienter la vue gauche (-X) (Num3)"));
    m_actionViewLeft->setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_3));
    connect(m_actionViewLeft, &QAction::triggered, this, &MainWindow::onActionViewLeft);

    m_actionViewRight = new QAction(tr("Vue &Droite (Right)"), this);
    m_actionViewRight->setIcon(makePlanIcon(QColor(140, 160, 255), Qt::darkGreen, Qt::red, "Y", "Z"));
    m_actionViewRight->setToolTip(tr("Orienter la vue droite (+X) (Ctrl+Num3)"));
    m_actionViewRight->setShortcut(QKeySequence(Qt::CTRL | Qt::KeypadModifier | Qt::Key_3));
    connect(m_actionViewRight, &QAction::triggered, this, &MainWindow::onActionViewRight);

    m_actionViewIsometric = new QAction(tr("Vue &Isométrique"), this);
    m_actionViewIsometric->setIcon(make3DIsoIcon());
    m_actionViewIsometric->setToolTip(tr("Orienter la vue en projection axonométrique isométrique (Num5)"));
    m_actionViewIsometric->setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_5));
    connect(m_actionViewIsometric, &QAction::triggered, this, &MainWindow::onActionViewIsometric);

    m_workPlaneGroup = new QActionGroup(this);
    m_workPlaneGroup->setExclusive(true);

    m_actionWorkPlaneXY = new QAction(tr("Plan de Travail &XY"), this);
    m_actionWorkPlaneXY->setIcon(QIcon(":/icons/view_top.svg"));
    m_actionWorkPlaneXY->setToolTip(tr("Définir le plan de travail horizontal (Global XY, Z=0)"));
    m_actionWorkPlaneXY->setCheckable(true);
    m_actionWorkPlaneXY->setChecked(true);
    connect(m_actionWorkPlaneXY, &QAction::triggered, this, &MainWindow::onWorkPlaneXY);
    m_workPlaneGroup->addAction(m_actionWorkPlaneXY);

    m_actionWorkPlaneLevel = new QAction(tr("Plan de Travail sur &Étage"), this);
    m_actionWorkPlaneLevel->setIcon(QIcon(":/icons/levels.svg"));
    m_actionWorkPlaneLevel->setToolTip(tr("Aligner le plan de travail horizontal sur l'altitude de l'étage actif"));
    m_actionWorkPlaneLevel->setCheckable(true);
    connect(m_actionWorkPlaneLevel, &QAction::triggered, this, &MainWindow::onWorkPlaneLevel);
    m_workPlaneGroup->addAction(m_actionWorkPlaneLevel);

    m_actionWorkPlaneXZ = new QAction(tr("Plan de Travail &XZ"), this);
    m_actionWorkPlaneXZ->setIcon(QIcon(":/icons/view_front.svg"));
    m_actionWorkPlaneXZ->setToolTip(tr("Définir le plan de travail vertical frontal (Global XZ, Façade)"));
    m_actionWorkPlaneXZ->setCheckable(true);
    connect(m_actionWorkPlaneXZ, &QAction::triggered, this, &MainWindow::onWorkPlaneXZ);
    m_workPlaneGroup->addAction(m_actionWorkPlaneXZ);

    m_actionWorkPlaneYZ = new QAction(tr("Plan de Travail &YZ"), this);
    m_actionWorkPlaneYZ->setIcon(QIcon(":/icons/view_right.svg"));
    m_actionWorkPlaneYZ->setToolTip(tr("Définir le plan de travail vertical latéral (Global YZ, Pignon)"));
    m_actionWorkPlaneYZ->setCheckable(true);
    connect(m_actionWorkPlaneYZ, &QAction::triggered, this, &MainWindow::onWorkPlaneYZ);
    m_workPlaneGroup->addAction(m_actionWorkPlaneYZ);

    m_actionWorkPlaneCustom = new QAction(tr("Plan de Travail &Personnalisé..."), this);
    m_actionWorkPlaneCustom->setIcon(QIcon(":/icons/settings.svg"));
    m_actionWorkPlaneCustom->setToolTip(tr("Définir un plan de travail personnalisé (3 points, décalage, options 3D)..."));
    connect(m_actionWorkPlaneCustom, &QAction::triggered, this, &MainWindow::onActionWorkPlaneCustom);

    m_actionWorkPlaneVisible = new QAction(tr("Afficher le &Plan de Travail 3D"), this);
    m_actionWorkPlaneVisible->setIcon(QIcon(":/icons/view_home.svg"));
    m_actionWorkPlaneVisible->setToolTip(tr("Afficher ou masquer la trame et le panneau 3D du plan de travail actif"));
    m_actionWorkPlaneVisible->setCheckable(true);
    m_actionWorkPlaneVisible->setChecked(true);
    connect(m_actionWorkPlaneVisible, &QAction::toggled, this, &MainWindow::onActionToggleWorkPlaneVisible);

    m_actionViewNormalToPlane = new QAction(tr("&Vue Normale au Plan"), this);
    m_actionViewNormalToPlane->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionViewNormalToPlane->setToolTip(tr("Orienter la caméra perpendiculairement au plan de travail actif"));
    connect(m_actionViewNormalToPlane, &QAction::triggered, this, &MainWindow::onActionViewNormalToPlane);

    // Actions Grilles & Niveaux
    m_actionNewGrid = new QAction(tr("&Nouvelle Grille 3D..."), this);
    m_actionNewGrid->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_actionNewGrid->setToolTip(tr("Créer une nouvelle grille paramétrique 3D (Cartésienne ou Cylindrique)..."));
    connect(m_actionNewGrid, &QAction::triggered, this, &MainWindow::onNewGrid);

    m_actionGridManager = new QAction(tr("&Gestionnaire de Grilles..."), this);
    m_actionGridManager->setIcon(QIcon(":/icons/settings.svg"));
    m_actionGridManager->setToolTip(tr("Gérer les grilles, plan actif, visibilité et magnétisme..."));
    connect(m_actionGridManager, &QAction::triggered, this, &MainWindow::onGridManagerDialog);

    m_actionManageLevels = new QAction(tr("Gestion des &Étages / Niveaux..."), this);
    m_actionManageLevels->setIcon(QIcon(":/icons/levels.svg"));
    m_actionManageLevels->setToolTip(tr("Gérer les hauteurs d'étages, niveaux altimétriques et liaisons verticales (Ctrl+L)..."));
    m_actionManageLevels->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(m_actionManageLevels, &QAction::triggered, this, &MainWindow::onManageLevels);

    m_actionGridVisible = new QAction(tr("&Afficher Grille 3D"), this);
    m_actionGridVisible->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_actionGridVisible->setToolTip(tr("Activer ou masquer la grille 3D (G / F7)"));
    m_actionGridVisible->setCheckable(true);
    m_actionGridVisible->setChecked(true);
    m_actionGridVisible->setShortcuts({ QKeySequence(Qt::Key_G), QKeySequence(Qt::Key_F7) });
    connect(m_actionGridVisible, &QAction::toggled, this, &MainWindow::onToggleGridVisible);

    m_actionLevelsVisible = new QAction(tr("Afficher Plans d'&Étages"), this);
    m_actionLevelsVisible->setIcon(QIcon(":/icons/levels.svg"));
    m_actionLevelsVisible->setToolTip(tr("Afficher ou masquer les plans 3D des étages et marqueurs altimétriques"));
    m_actionLevelsVisible->setCheckable(true);
    m_actionLevelsVisible->setChecked(true);
    connect(m_actionLevelsVisible, &QAction::toggled, this, &MainWindow::onToggleLevelsVisible);

    m_actionGridSnap = new QAction(tr("&Magnétisme Grille (Snap)"), this);
    m_actionGridSnap->setIcon(QIcon(":/icons/snap.svg"));
    m_actionGridSnap->setToolTip(tr("Accrochage magnétique du curseur aux intersections de grille (S)"));
    m_actionGridSnap->setCheckable(true);
    m_actionGridSnap->setChecked(true);
    m_actionGridSnap->setShortcut(QKeySequence(Qt::Key_S));
    connect(m_actionGridSnap, &QAction::toggled, this, &MainWindow::onToggleGridSnap);

    m_actionObjectSnap = new QAction(tr("Accrochage &Objets (OSNAP)"), this);
    m_actionObjectSnap->setIcon(QIcon(":/icons/view/snap.svg"));
    m_actionObjectSnap->setToolTip(tr("Accrochage magnétique intelligent aux nœuds, milieux et extrémités (F3)"));
    m_actionObjectSnap->setCheckable(true);
    m_actionObjectSnap->setChecked(true);
    m_actionObjectSnap->setShortcut(QKeySequence(Qt::Key_F3));
    connect(m_actionObjectSnap, &QAction::toggled, this, &MainWindow::onToggleObjectSnap);

    m_actionGridLabels = new QAction(tr("Afficher Libellés d'&Axes"), this);
    m_actionGridLabels->setIcon(QIcon(":/icons/grid_labels.svg"));
    m_actionGridLabels->setToolTip(tr("Afficher ou masquer les bulles d'axes et libellés en 3D"));
    m_actionGridLabels->setCheckable(true);
    m_actionGridLabels->setChecked(true);
    connect(m_actionGridLabels, &QAction::toggled, this, &MainWindow::onToggleGridLabels);

    m_actionNodesVisible = new QAction(tr("Afficher les &Nœuds"), this);
    m_actionNodesVisible->setIcon(QIcon(":/icons/draw_node.svg"));
    m_actionNodesVisible->setToolTip(tr("Afficher ou masquer les nœuds du modèle dans la vue 3D"));
    m_actionNodesVisible->setCheckable(true);
    m_actionNodesVisible->setChecked(true);
    connect(m_actionNodesVisible, &QAction::toggled, this, &MainWindow::onToggleNodesVisible);

    m_actionNodeLabelsVisible = new QAction(tr("Afficher les Numéros de &Nœuds"), this);
    m_actionNodeLabelsVisible->setIcon(QIcon(":/icons/grid_labels.svg"));
    m_actionNodeLabelsVisible->setToolTip(tr("Afficher ou masquer les numéros et labels des nœuds (N1, N2...) en 3D"));
    m_actionNodeLabelsVisible->setCheckable(true);
    m_actionNodeLabelsVisible->setChecked(false);
    connect(m_actionNodeLabelsVisible, &QAction::toggled, this, &MainWindow::onToggleNodeLabelsVisible);

    m_actionSupportsVisible = new QAction(tr("Afficher les &Appuis 3D"), this);
    m_actionSupportsVisible->setIcon(QIcon(":/icons/draw_node.svg"));
    m_actionSupportsVisible->setToolTip(tr("Afficher ou masquer les appuis structuraux (encastrements, rotules, rouleaux, ressorts)"));
    m_actionSupportsVisible->setCheckable(true);
    m_actionSupportsVisible->setChecked(true);
    connect(m_actionSupportsVisible, &QAction::toggled, this, &MainWindow::onToggleSupportsVisible);

    m_actionSupportLabelsVisible = new QAction(tr("Afficher les &Étiquettes d'Appuis"), this);
    m_actionSupportLabelsVisible->setToolTip(tr("Afficher ou masquer les étiquettes textuelles des appuis 3D"));
    m_actionSupportLabelsVisible->setCheckable(true);
    m_actionSupportLabelsVisible->setChecked(false);
    connect(m_actionSupportLabelsVisible, &QAction::toggled, this, &MainWindow::onToggleSupportLabelsVisible);

    m_actionLoadsVisible = new QAction(tr("Afficher les &Charges 3D"), this);
    m_actionLoadsVisible->setIcon(QIcon(":/icons/load_dist.svg"));
    m_actionLoadsVisible->setToolTip(tr("Afficher ou masquer les représentations et flèches 3D des charges"));
    m_actionLoadsVisible->setCheckable(true);
    m_actionLoadsVisible->setChecked(true);
    connect(m_actionLoadsVisible, &QAction::toggled, this, &MainWindow::onToggleLoadsVisible);

    m_actionForcesVisible = new QAction(tr("Afficher les &Forces 3D"), this);
    m_actionForcesVisible->setIcon(QIcon(":/icons/load_point.svg"));
    m_actionForcesVisible->setToolTip(tr("Afficher ou masquer les flèches 3D de forces (Fx, Fy, Fz)"));
    m_actionForcesVisible->setCheckable(true);
    m_actionForcesVisible->setChecked(true);
    connect(m_actionForcesVisible, &QAction::toggled, this, &MainWindow::onToggleForcesVisible);

    m_actionMomentsVisible = new QAction(tr("Afficher les &Moments 3D"), this);
    m_actionMomentsVisible->setIcon(QIcon(":/icons/load_moment.svg"));
    m_actionMomentsVisible->setToolTip(tr("Afficher ou masquer les arcs 3D orientés de moments (Mx, My, Mz)"));
    m_actionMomentsVisible->setCheckable(true);
    m_actionMomentsVisible->setChecked(true);
    connect(m_actionMomentsVisible, &QAction::toggled, this, &MainWindow::onToggleMomentsVisible);

    m_actionLoadValuesVisible = new QAction(tr("Afficher les &Valeurs des Charges"), this);
    m_actionLoadValuesVisible->setIcon(QIcon(":/icons/results_forces.svg"));
    m_actionLoadValuesVisible->setToolTip(tr("Afficher ou masquer les étiquettes de valeurs des charges (kN, kNm) en 3D"));
    m_actionLoadValuesVisible->setCheckable(true);
    m_actionLoadValuesVisible->setChecked(true);
    connect(m_actionLoadValuesVisible, &QAction::toggled, this, &MainWindow::onToggleLoadValuesVisible);

    m_actionRulersVisible = new QAction(tr("Afficher &Règles Graduées"), this);
    m_actionRulersVisible->setIcon(QIcon(":/icons/rulers.svg"));
    m_actionRulersVisible->setToolTip(tr("Afficher ou masquer les règles graduées du viewport"));
    m_actionRulersVisible->setCheckable(true);
    m_actionRulersVisible->setChecked(true);
    connect(m_actionRulersVisible, &QAction::toggled, this, &MainWindow::onToggleRulersVisible);

    m_actionFullScreen = new QAction(tr("Mode &Plein écran"), this);
    m_actionFullScreen->setIcon(QIcon(":/icons/fullscreen.svg"));
    m_actionFullScreen->setToolTip(tr("Basculer en mode plein écran (F11)"));
    m_actionFullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    m_actionFullScreen->setCheckable(true);
    connect(m_actionFullScreen, &QAction::toggled, this, &MainWindow::onToggleFullScreen);

    // Modes d'interaction / Dessin 3D
    m_drawModeGroup = new QActionGroup(this);

    m_actionSelectMode = new QAction(tr("&Sélection"), this);
    m_actionSelectMode->setIcon(QIcon(":/icons/select.svg"));
    m_actionSelectMode->setToolTip(tr("Mode Sélection - Sélection par clic ou fenêtre (Échap)"));
    m_actionSelectMode->setCheckable(true);
    m_actionSelectMode->setChecked(true);
    m_actionSelectMode->setShortcut(QKeySequence(Qt::Key_Escape));
    m_actionSelectMode->setStatusTip(tr("Sélectionner et inspecter les éléments structuraux (Échap)"));
    connect(m_actionSelectMode, &QAction::triggered, this, &MainWindow::onModeSelect);
    m_drawModeGroup->addAction(m_actionSelectMode);

    m_actionDrawNode = new QAction(tr("Dessiner &Nœud"), this);
    m_actionDrawNode->setIcon(QIcon(":/icons/draw_node.svg"));
    m_actionDrawNode->setToolTip(tr("Dessiner un Nœud en 3D (N)"));
    m_actionDrawNode->setCheckable(true);
    m_actionDrawNode->setShortcut(QKeySequence(Qt::Key_N));
    m_actionDrawNode->setStatusTip(tr("Cliquez en 3D ou sur la grille pour créer un Nœud (N)"));
    connect(m_actionDrawNode, &QAction::triggered, this, &MainWindow::onModeDrawNode);
    m_drawModeGroup->addAction(m_actionDrawNode);

    m_actionDrawBar = new QAction(tr("Outil &Barres"), this);
    m_actionDrawBar->setIcon(QIcon(":/icons/modeling/draw_bar.svg"));
    m_actionDrawBar->setToolTip(tr("Outil Barres (style Robot Structural Analysis) : définition et dessin direct en 3D"));
    m_actionDrawBar->setCheckable(true);
    connect(m_actionDrawBar, &QAction::triggered, this, &MainWindow::onModeDrawBar);
    m_drawModeGroup->addAction(m_actionDrawBar);

    m_actionDrawBeam = new QAction(tr("Dessiner &Poutre"), this);
    m_actionDrawBeam->setIcon(QIcon(":/icons/draw_beam.svg"));
    m_actionDrawBeam->setToolTip(tr("Dessiner une Poutre (B)"));
    m_actionDrawBeam->setCheckable(true);
    m_actionDrawBeam->setShortcut(QKeySequence(Qt::Key_B));
    m_actionDrawBeam->setStatusTip(tr("Ouvre l'interface filaire préconfigurée en mode Poutre (B)"));
    connect(m_actionDrawBeam, &QAction::triggered, this, &MainWindow::onModeDrawBeam);
    m_drawModeGroup->addAction(m_actionDrawBeam);

    m_actionDrawColumn = new QAction(tr("Dessiner &Poteau"), this);
    m_actionDrawColumn->setIcon(QIcon(":/icons/draw_column.svg"));
    m_actionDrawColumn->setToolTip(tr("Dessiner un Poteau (C)"));
    m_actionDrawColumn->setCheckable(true);
    m_actionDrawColumn->setShortcut(QKeySequence(Qt::Key_C));
    m_actionDrawColumn->setStatusTip(tr("Ouvre l'interface filaire préconfigurée en mode Poteau (C)"));
    connect(m_actionDrawColumn, &QAction::triggered, this, &MainWindow::onModeDrawColumn);
    m_drawModeGroup->addAction(m_actionDrawColumn);

    m_actionDrawCable = new QAction(tr("Dessiner &Câble"), this);
    m_actionDrawCable->setIcon(QIcon(":/icons/draw_cable.svg"));
    m_actionDrawCable->setToolTip(tr("Dessiner un Câble (Alt+C) - Élément filaire tendu"));
    m_actionDrawCable->setCheckable(true);
    m_actionDrawCable->setShortcut(QKeySequence(Qt::ALT | Qt::Key_C));
    m_actionDrawCable->setStatusTip(tr("Active le mode dessin Câble reliant deux nœuds (Alt+C)"));
    connect(m_actionDrawCable, &QAction::triggered, this, &MainWindow::onModeDrawCable);
    m_drawModeGroup->addAction(m_actionDrawCable);

    m_actionDrawSlab = new QAction(tr("Dessiner &Dalle"), this);
    m_actionDrawSlab->setIcon(QIcon(":/icons/draw_slab.svg"));
    m_actionDrawSlab->setToolTip(tr("Dessiner une Dalle (L)"));
    m_actionDrawSlab->setCheckable(true);
    m_actionDrawSlab->setShortcut(QKeySequence(Qt::Key_L));
    m_actionDrawSlab->setStatusTip(tr("Ouvre l'interface surfacique en mode Dalle (L)"));
    connect(m_actionDrawSlab, &QAction::triggered, this, &MainWindow::onModeDrawSlab);
    m_drawModeGroup->addAction(m_actionDrawSlab);

    m_actionDrawWall = new QAction(tr("Dessiner &Voile"), this);
    m_actionDrawWall->setIcon(QIcon(":/icons/struct_wall.svg"));
    m_actionDrawWall->setToolTip(tr("Dessiner un Voile (W)"));
    m_actionDrawWall->setCheckable(true);
    m_actionDrawWall->setShortcut(QKeySequence(Qt::Key_W));
    m_actionDrawWall->setStatusTip(tr("Ouvre l'interface surfacique en mode Voile (W)"));
    connect(m_actionDrawWall, &QAction::triggered, this, &MainWindow::onModeDrawWall);
    m_drawModeGroup->addAction(m_actionDrawWall);

    m_actionStructurePresets = new QAction(tr("&Paramètres de Modélisation..."), this);
    m_actionStructurePresets->setIcon(QIcon(":/icons/settings.svg"));
    m_actionStructurePresets->setToolTip(tr("Configurer les caractéristiques des structures avant de dessiner (sections, épaisseurs, matériaux)..."));
    m_actionStructurePresets->setStatusTip(tr("Configurer les sections, hauteurs, épaisseurs et matériaux par défaut pour le dessin 3D"));
    connect(m_actionStructurePresets, &QAction::triggered, this, &MainWindow::onActionStructurePresets);

    m_actionNewNode = new QAction(tr("Nouveau &Nœud (Dialogue)..."), this);
    m_actionNewNode->setIcon(QIcon(":/icons/node_add.svg"));
    m_actionNewNode->setToolTip(tr("Créer un Nœud par saisie de coordonnées numériques..."));
    connect(m_actionNewNode, &QAction::triggered, this, &MainWindow::onActionNewNode);

    m_actionAddCube = new QAction(tr("Cube &Structurel 3D"), this);
    m_actionAddCube->setIcon(QIcon(":/icons/geom_cube.svg"));
    m_actionAddCube->setToolTip(tr("Générer un module 3D complet (8 nœuds, 4 poteaux, 8 poutres, 1 dalle)"));
    connect(m_actionAddCube, &QAction::triggered, this, &MainWindow::onActionAddCube);

    // Actions Undo / Redo
    m_actionUndo = new QAction(tr("&Annuler"), this);
    m_actionUndo->setIcon(makeUndoIcon());
    m_actionUndo->setToolTip(tr("Annuler la dernière action (Ctrl+Z)"));
    m_actionUndo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    m_actionUndo->setEnabled(false);
    connect(m_actionUndo, &QAction::triggered, this, &MainWindow::onActionUndo);

    m_actionRedo = new QAction(tr("&Rétablir"), this);
    m_actionRedo->setIcon(makeRedoIcon());
    m_actionRedo->setToolTip(tr("Rétablir la dernière action annulée (Ctrl+Y)"));
    m_actionRedo->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) });
    m_actionRedo->setEnabled(false);
    connect(m_actionRedo, &QAction::triggered, this, &MainWindow::onActionRedo);

    // Actions Presse-papier & Transformations 3D
    m_actionCopyClipboard = new QAction(tr("&Copier (Presse-papier)"), this);
    m_actionCopyClipboard->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopyClipboard->setToolTip(tr("Copier la sélection dans le presse-papier structural (Ctrl+C)"));
    m_actionCopyClipboard->setShortcut(QKeySequence::Copy);
    connect(m_actionCopyClipboard, &QAction::triggered, this, &MainWindow::onActionCopyClipboard);

    m_actionPasteClipboard = new QAction(tr("C&oller en 3D"), this);
    m_actionPasteClipboard->setIcon(QIcon(":/icons/edit/paste.svg"));
    m_actionPasteClipboard->setToolTip(tr("Coller les éléments copiés dans la vue 3D au clic souris (Ctrl+V)"));
    m_actionPasteClipboard->setShortcut(QKeySequence::Paste);
    connect(m_actionPasteClipboard, &QAction::triggered, this, &MainWindow::onActionPasteClipboard);

    m_actionMove3D = new QAction(tr("&Déplacement 3D (Point à Point)..."), this);
    m_actionMove3D->setIcon(QIcon(":/icons/structure/struct_move.svg"));
    m_actionMove3D->setToolTip(tr("Déplacer interactivement les éléments dans la vue 3D (M)"));
    m_actionMove3D->setShortcut(QKeySequence(Qt::Key_M));
    m_actionMove3D->setCheckable(true);
    connect(m_actionMove3D, &QAction::triggered, this, &MainWindow::onActionMove3D);

    m_actionCopy3D = new QAction(tr("C&opie 3D (Translation)..."), this);
    m_actionCopy3D->setIcon(QIcon(":/icons/structure/struct_copy.svg"));
    m_actionCopy3D->setToolTip(tr("Copier interactivement les éléments par translation en 3D"));
    m_actionCopy3D->setCheckable(true);
    connect(m_actionCopy3D, &QAction::triggered, this, &MainWindow::onActionCopy3D);

    m_actionRotate3D = new QAction(tr("&Rotation 3D..."), this);
    m_actionRotate3D->setIcon(makeRotateIcon());
    m_actionRotate3D->setToolTip(tr("Faire tourner les éléments sélectionnés autour d'un axe 3D (Ctrl+R)"));
    m_actionRotate3D->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actionRotate3D->setCheckable(true);
    connect(m_actionRotate3D, &QAction::triggered, this, &MainWindow::onActionRotate3D);

    m_actionMirror = new QAction(tr("&Symétrie (Miroir)..."), this);
    m_actionMirror->setIcon(QIcon(":/icons/edit/mirror.svg"));
    m_actionMirror->setToolTip(tr("Copier ou retourner la sélection par symétrie par rapport à un plan X, Y ou Z"));
    connect(m_actionMirror, &QAction::triggered, this, &MainWindow::onActionMirror);

    m_actionSplitBars = new QAction(tr("&Diviser les barres..."), this);
    m_actionSplitBars->setIcon(QIcon(":/icons/structure/struct_split.svg"));
    m_actionSplitBars->setToolTip(tr("Diviser les poutres et poteaux sélectionnés en N tronçons égaux"));
    connect(m_actionSplitBars, &QAction::triggered, this, &MainWindow::onActionSplitBars);

    m_actionMergeNodes = new QAction(tr("&Fusionner les nœuds confondus..."), this);
    m_actionMergeNodes->setIcon(QIcon(":/icons/structure/struct_merge.svg"));
    m_actionMergeNodes->setToolTip(tr("Fusionner les nœuds géométriquement confondus (éléments, appuis et charges reportés)"));
    connect(m_actionMergeNodes, &QAction::triggered, this, &MainWindow::onActionMergeNodes);

    m_actionMoveOrigin = new QAction(tr("Déplacer l'&Origine 3D..."), this);
    m_actionMoveOrigin->setIcon(makeOriginMoveIcon());
    m_actionMoveOrigin->setToolTip(tr("Positionner le repère global / la grille 3D par clic ou snap"));
    m_actionMoveOrigin->setCheckable(true);
    connect(m_actionMoveOrigin, &QAction::triggered, this, &MainWindow::onActionMoveOrigin);

    // Actions Thème & Aide
    m_actionToggleTheme = new QAction(tr("Mode &Sombre / Clair"), this);
    m_actionToggleTheme->setIcon(makeThemeIcon(true));
    m_actionToggleTheme->setToolTip(tr("Basculer entre Mode Sombre (AutoCAD) et Mode Clair (Ctrl+T / F10)"));
    m_actionToggleTheme->setCheckable(true);
    m_actionToggleTheme->setChecked(true);
    m_actionToggleTheme->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_actionToggleTheme, &QAction::triggered, this, &MainWindow::onToggleTheme);

    m_actionHelp = new QAction(tr("&Aide Complète TSA..."), this);
    m_actionHelp->setIcon(makeHelpIcon());
    m_actionHelp->setToolTip(tr("Ouvrir le centre d'aide, guide et documentation"));
    connect(m_actionHelp, &QAction::triggered, this, &MainWindow::onActionHelp);

    m_actionShortcuts = new QAction(tr("&Raccourcis Clavier..."), this);
    m_actionShortcuts->setIcon(makeShortcutsIcon());
    m_actionShortcuts->setToolTip(tr("Afficher la liste des raccourcis clavier et commandes console (F1)"));
    m_actionShortcuts->setShortcut(QKeySequence::HelpContents);
    connect(m_actionShortcuts, &QAction::triggered, this, &MainWindow::onActionShortcuts);

    m_actionAbout = new QAction(tr("À &propos de TSA..."), this);
    m_actionAbout->setIcon(makeAboutIcon());
    m_actionAbout->setToolTip(tr("Informations sur l'application, OpenCASCADE et crédits"));
    connect(m_actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout);

    m_actionExportDiagnostic = new QAction(tr("&Exporter Rapport de Diagnostic..."), this);
    m_actionExportDiagnostic->setIcon(QIcon(":/icons/console.svg"));
    m_actionExportDiagnostic->setToolTip(tr("Générer un rapport de diagnostic complet (système, modèle, 100 derniers événements)"));
    connect(m_actionExportDiagnostic, &QAction::triggered, this, &MainWindow::onActionExportDiagnosticReport);

    // Actions Métier & Outils Avancés
    m_actionTruss = new QAction(tr("&Treillis Paramétrique..."), this);
    m_actionTruss->setIcon(QIcon(":/icons/struct_truss.svg"));
    m_actionTruss->setToolTip(tr("Générer une ferme ou poutre en treillis (Warren, Pratt, Howe)"));
    connect(m_actionTruss, &QAction::triggered, this, &MainWindow::onActionTruss);

    m_actionFooting = new QAction(tr("&Semelle / Fondation..."), this);
    m_actionFooting->setIcon(QIcon(":/icons/struct_foundation.svg"));
    m_actionFooting->setToolTip(tr("Générer des semelles isolées BA sous les poteaux"));
    connect(m_actionFooting, &QAction::triggered, this, &MainWindow::onActionFooting);

    m_actionSecI = new QAction(tr("Profilé en &I/H (IPE/HEA/HEB)..."), this);
    m_actionSecI->setIcon(QIcon(":/icons/section_i.svg"));
    m_actionSecI->setToolTip(tr("Sélectionner un profilé standard européen en I ou H"));
    connect(m_actionSecI, &QAction::triggered, this, &MainWindow::onActionSecI);

    m_actionSecRect = new QAction(tr("Section &Rectangulaire..."), this);
    m_actionSecRect->setIcon(QIcon(":/icons/section_rect.svg"));
    m_actionSecRect->setToolTip(tr("Définir une section rectangulaire (b x h)"));
    connect(m_actionSecRect, &QAction::triggered, this, &MainWindow::onActionSecRect);

    m_actionSecCirc = new QAction(tr("Section &Circulaire..."), this);
    m_actionSecCirc->setIcon(QIcon(":/icons/section_circle.svg"));
    m_actionSecCirc->setToolTip(tr("Définir une section circulaire ou tubulaire"));
    connect(m_actionSecCirc, &QAction::triggered, this, &MainWindow::onActionSecCirc);

    m_actionConcrete = new QAction(tr("&Béton Armé (C25/30)..."), this);
    m_actionConcrete->setIcon(QIcon(":/icons/material_concrete.svg"));
    m_actionConcrete->setToolTip(tr("Assigner les propriétés mécaniques du béton armé (Eurocode 2)"));
    connect(m_actionConcrete, &QAction::triggered, this, &MainWindow::onActionConcrete);

    m_actionSteel = new QAction(tr("&Acier Structural (S355)..."), this);
    m_actionSteel->setIcon(QIcon(":/icons/material_steel.svg"));
    m_actionSteel->setToolTip(tr("Assigner les propriétés de l'acier de construction (Eurocode 3)"));
    connect(m_actionSteel, &QAction::triggered, this, &MainWindow::onActionSteel);

    m_actionFixed = new QAction(tr("Encastrement &Parfait (6 DDL)"), this);
    m_actionFixed->setIcon(QIcon(":/icons/support_fixed.svg"));
    m_actionFixed->setToolTip(tr("Bloquer les 6 degrés de liberté (Tx, Ty, Tz, Rx, Ry, Rz)"));
    connect(m_actionFixed, &QAction::triggered, this, &MainWindow::onActionFixed);

    m_actionPinned = new QAction(tr("&Articulation (Rotule 3D)"), this);
    m_actionPinned->setIcon(QIcon(":/icons/support_pinned.svg"));
    m_actionPinned->setToolTip(tr("Bloquer les 3 translations (Tx, Ty, Tz)"));
    connect(m_actionPinned, &QAction::triggered, this, &MainWindow::onActionPinned);

    m_actionRoller = new QAction(tr("Appui &Simple (Rouleau)"), this);
    m_actionRoller->setIcon(QIcon(":/icons/support_roller.svg"));
    m_actionRoller->setToolTip(tr("Bloquer le déplacement vertical Tz"));
    connect(m_actionRoller, &QAction::triggered, this, &MainWindow::onActionRoller);

    m_actionLibrary = new QAction(tr("&Bibliothèque Personnalisée..."), this);
    m_actionLibrary->setIcon(QIcon(":/icons/structure_preset.svg"));
    m_actionLibrary->setToolTip(tr("Gérer la bibliothèque de sections, matériaux, textures, couleurs et structures personnalisées"));
    connect(m_actionLibrary, &QAction::triggered, this, [this]() { onActionLibrary(0); });

    m_actionExtensionManager = new QAction(tr("&Gestionnaire TSALib..."), this);
    m_actionExtensionManager->setIcon(QIcon(":/icons/file_new.svg"));
    m_actionExtensionManager->setToolTip(tr("Gérer les extensions, explorer les catalogues Eurocodes (matériaux, sections, câbles, textures) et recharger à chaud"));
    connect(m_actionExtensionManager, &QAction::triggered, this, &MainWindow::onActionExtensionManager);

    m_actionPointLoad = new QAction(tr("&Force Ponctuelle..."), this);
    m_actionPointLoad->setIcon(QIcon(":/icons/load_point.svg"));
    m_actionPointLoad->setToolTip(tr("Appliquer une force ponctuelle (Fx, Fy, Fz)"));
    connect(m_actionPointLoad, &QAction::triggered, this, &MainWindow::onActionPointLoad);

    m_actionDistLoad = new QAction(tr("Charge &Linéique Répartie..."), this);
    m_actionDistLoad->setIcon(QIcon(":/icons/load_dist.svg"));
    m_actionDistLoad->setToolTip(tr("Appliquer une charge répartie q sur les poutres"));
    connect(m_actionDistLoad, &QAction::triggered, this, &MainWindow::onActionDistLoad);

    m_actionMoment = new QAction(tr("&Moment Nodal..."), this);
    m_actionMoment->setIcon(QIcon(":/icons/load_moment.svg"));
    m_actionMoment->setToolTip(tr("Appliquer un moment fléchissant ou de torsion"));
    connect(m_actionMoment, &QAction::triggered, this, &MainWindow::onActionMoment);

    m_actionLoadCases = new QAction(tr("&Cas de Charges && Combinaisons..."), this);
    m_actionLoadCases->setIcon(QIcon(":/icons/analysis_modal.svg"));
    m_actionLoadCases->setToolTip(tr("Gérer les cas de charges, combinaisons Eurocodes et export OpenSees"));
    connect(m_actionLoadCases, &QAction::triggered, this, &MainWindow::onActionLoadCases);

    m_actionSeismic = new QAction(tr("Action &Sismique (Eurocode 8)..."), this);
    m_actionSeismic->setIcon(QIcon(":/icons/load_seismic.svg"));
    m_actionSeismic->setToolTip(tr("Définir le spectre sismique réglementaire"));
    connect(m_actionSeismic, &QAction::triggered, this, &MainWindow::onActionSeismic);

    m_actionMeshGen = new QAction(tr("&Générer le Maillage EF..."), this);
    m_actionMeshGen->setIcon(QIcon(":/icons/mesh_generate.svg"));
    m_actionMeshGen->setToolTip(tr("Discrétiser les barres et dalles en éléments finis"));
    connect(m_actionMeshGen, &QAction::triggered, this, &MainWindow::onActionMeshGen);

    m_actionAnalysisConfig = new QAction(tr("&Paramètres de Résolution..."), this);
    m_actionAnalysisConfig->setIcon(QIcon(":/icons/analysis_settings.svg"));
    m_actionAnalysisConfig->setToolTip(tr("Configurer la méthode de résolution, l'algorithme et l'intégrateur EF"));
    connect(m_actionAnalysisConfig, &QAction::triggered, this, &MainWindow::onActionAnalysisConfig);

    m_actionRunSolve = new QAction(tr("&Lancer le Calcul Structurel"), this);
    m_actionRunSolve->setIcon(QIcon(":/icons/analysis_run.svg"));
    m_actionRunSolve->setToolTip(tr("Lancer la résolution par éléments finis [K]{u} = {F} (F5)"));
    m_actionRunSolve->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_actionRunSolve, &QAction::triggered, this, &MainWindow::onActionRunSolve);

    m_actionModal = new QAction(tr("Analyse &Modale Dynamique..."), this);
    m_actionModal->setIcon(QIcon(":/icons/analysis_modal.svg"));
    m_actionModal->setToolTip(tr("Calculer les modes propres et fréquences de vibration"));
    connect(m_actionModal, &QAction::triggered, this, &MainWindow::onActionModal);

    m_actionPushover = new QAction(tr("Analyse &Pushover Non-Linéaire..."), this);
    m_actionPushover->setIcon(QIcon(":/icons/analysis_pushover.svg"));
    m_actionPushover->setToolTip(tr("Exécuter une analyse statique non-linéaire (Pushover)"));
    connect(m_actionPushover, &QAction::triggered, this, &MainWindow::onActionPushover);

    m_actionDeformedToggle = new QAction(tr("Afficher la &Déformée 3D"), this);
    m_actionDeformedToggle->setCheckable(true);
    m_actionDeformedToggle->setChecked(true);
    m_actionDeformedToggle->setIcon(QIcon(":/icons/results_disp.svg"));
    m_actionDeformedToggle->setToolTip(tr("Activer ou masquer la vue de la structure déformée"));
    connect(m_actionDeformedToggle, &QAction::toggled, this, &MainWindow::onActionToggleDeformed);

    m_actionDiagramMz = new QAction(tr("Moment &Mz"), this);
    m_actionDiagramMz->setToolTip(tr("Afficher le diagramme 3D du moment fléchissant Mz"));
    connect(m_actionDiagramMz, &QAction::triggered, this, &MainWindow::onActionDiagramMz);

    m_actionDiagramMy = new QAction(tr("Moment M&y"), this);
    m_actionDiagramMy->setToolTip(tr("Afficher le diagramme 3D du moment fléchissant secondaire My"));
    connect(m_actionDiagramMy, &QAction::triggered, this, &MainWindow::onActionDiagramMy);

    m_actionDiagramMx = new QAction(tr("Torsion M&x"), this);
    m_actionDiagramMx->setToolTip(tr("Afficher le diagramme 3D du moment de torsion Mx"));
    connect(m_actionDiagramMx, &QAction::triggered, this, &MainWindow::onActionDiagramMx);

    m_actionDiagramVz = new QAction(tr("Tranchant &Vz"), this);
    m_actionDiagramVz->setToolTip(tr("Afficher le diagramme 3D de l'effort tranchant Vz"));
    connect(m_actionDiagramVz, &QAction::triggered, this, &MainWindow::onActionDiagramVz);

    m_actionDiagramVy = new QAction(tr("Tranchant V&y"), this);
    m_actionDiagramVy->setToolTip(tr("Afficher le diagramme 3D de l'effort tranchant Vy"));
    connect(m_actionDiagramVy, &QAction::triggered, this, &MainWindow::onActionDiagramVy);

    m_actionDiagramN = new QAction(tr("Effort Normal &N"), this);
    m_actionDiagramN->setToolTip(tr("Afficher le diagramme 3D de l'effort normal N"));
    connect(m_actionDiagramN, &QAction::triggered, this, &MainWindow::onActionDiagramN);

    m_actionDiagramDeflection = new QAction(tr("Flèches &UZ / Ures"), this);
    m_actionDiagramDeflection->setToolTip(tr("Afficher le diagramme 3D des flèches transversales le long des éléments"));
    connect(m_actionDiagramDeflection, &QAction::triggered, this, &MainWindow::onActionDiagramDeflection);

    m_actionDiagramNone = new QAction(tr("&Masquer Diagrammes"), this);
    connect(m_actionDiagramNone, &QAction::triggered, this, &MainWindow::onActionDiagramNone);

    m_actionReactionsToggle = new QAction(tr("Afficher les &Réactions"), this);
    m_actionReactionsToggle->setCheckable(true);
    m_actionReactionsToggle->setChecked(true);
    m_actionReactionsToggle->setToolTip(tr("Afficher les flèches et valeurs des réactions d'appui en 3D"));
    connect(m_actionReactionsToggle, &QAction::toggled, this, &MainWindow::onActionToggleReactions);

    m_actionFitModel = new QAction(tr("Cadrer &Modèle"), this);
    m_actionFitModel->setIcon(QIcon(":/icons/fit_all.svg"));
    m_actionFitModel->setToolTip(tr("Cadrer la vue sur l'ensemble du modèle initial non déformé"));
    connect(m_actionFitModel, &QAction::triggered, this, &MainWindow::onFitModel);

    m_actionFitResults = new QAction(tr("Cadrer &Résultats"), this);
    m_actionFitResults->setIcon(QIcon(":/icons/results_disp.svg"));
    m_actionFitResults->setToolTip(tr("Cadrer la vue sur l'enveloppe globale des résultats et diagrammes 3D"));
    connect(m_actionFitResults, &QAction::triggered, this, &MainWindow::onFitResults);

    m_actionFitDeformed = new QAction(tr("Cadrer &Déformée"), this);
    m_actionFitDeformed->setIcon(QIcon(":/icons/results_disp.svg"));
    m_actionFitDeformed->setToolTip(tr("Cadrer la vue sur l'enveloppe de la structure déformée"));
    connect(m_actionFitDeformed, &QAction::triggered, this, &MainWindow::onFitDeformed);

    m_actionNoteDeCalcul = new QAction(tr("Note de &Calcul..."), this);
    m_actionNoteDeCalcul->setIcon(QIcon(":/icons/ndc_report.svg"));
    m_actionNoteDeCalcul->setToolTip(tr("Ouvrir l'inspecteur et générateur de Note de Calcul (F8)"));
    m_actionNoteDeCalcul->setShortcut(QKeySequence(Qt::Key_F8));
    connect(m_actionNoteDeCalcul, &QAction::triggered, this, &MainWindow::onActionNoteDeCalcul);

    m_actionResultsDisp = new QAction(tr("Déformée && &Déplacements"), this);
    m_actionResultsDisp->setIcon(QIcon(":/icons/results_disp.svg"));
    m_actionResultsDisp->setToolTip(tr("Afficher la déformée amplifiée et les déplacements nodaux"));
    connect(m_actionResultsDisp, &QAction::triggered, this, &MainWindow::onActionResultsDisp);

    m_actionResultsForces = new QAction(tr("Diagrammes des &Efforts (M/N/V)"), this);
    m_actionResultsForces->setIcon(QIcon(":/icons/results_force.svg"));
    m_actionResultsForces->setToolTip(tr("Afficher les diagrammes de moments, efforts tranchants et normaux"));
    connect(m_actionResultsForces, &QAction::triggered, this, &MainWindow::onActionResultsForces);

    m_actionResultsStress = new QAction(tr("Contraintes de &Von Mises"), this);
    m_actionResultsStress->setIcon(QIcon(":/icons/results_stress.svg"));
    m_actionResultsStress->setToolTip(tr("Afficher la cartographie des contraintes"));
    connect(m_actionResultsStress, &QAction::triggered, this, &MainWindow::onActionResultsStress);

    m_actionMeasure = new QAction(tr("&Mesurer Distance 3D..."), this);
    m_actionMeasure->setIcon(QIcon(":/icons/measure.svg"));
    m_actionMeasure->setToolTip(tr("Mesurer la distance spatiale 3D, horizontale et dénivelée entre nœuds"));
    connect(m_actionMeasure, &QAction::triggered, this, &MainWindow::onActionMeasure);
}

void MainWindow::createMenus()
{
    // 1. Menu Fichier (Accueil)
    QMenu* fileMenu = menuBar()->addMenu(tr("&Fichier"));
    fileMenu->addAction(m_actionNew);
    fileMenu->addAction(m_actionOpen);
    fileMenu->addAction(m_actionSave);
    fileMenu->addAction(m_actionSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionExit);

    // 2. Menu Édition
    QMenu* editMenu = menuBar()->addMenu(tr("&Édition"));
    editMenu->addAction(m_actionUndo);
    editMenu->addAction(m_actionRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actionSelectMode);
    editMenu->addSeparator();
    editMenu->addAction(m_actionCopyClipboard);
    editMenu->addAction(m_actionPasteClipboard);
    editMenu->addSeparator();
    editMenu->addAction(m_actionMove3D);
    editMenu->addAction(m_actionMove);
    editMenu->addSeparator();
    editMenu->addAction(m_actionCopy3D);
    editMenu->addAction(m_actionCopy);
    editMenu->addAction(m_actionRotate3D);
    editMenu->addAction(m_actionMirror);
    editMenu->addSeparator();
    editMenu->addAction(m_actionSplitBars);
    editMenu->addAction(m_actionMergeNodes);
    editMenu->addSeparator();
    editMenu->addAction(m_actionSelectAll);
    {
        using TSA::Model::ElementKind;
        namespace SQ = TSA::Model::SelectionQuery;

        QAction* invertAct = editMenu->addAction(tr("&Inverser la sélection"));
        invertAct->setShortcut(QKeySequence("Ctrl+Alt+I")); // Ctrl+I = isolation par coupe (CommandCatalog)
        connect(invertAct, &QAction::triggered, this, [this]() {
            if (!m_model || !m_selectionManager) return;
            applyElementSelection(SQ::invert(*m_model, m_selectionManager->selectedElements()), tr("Sélection inversée"));
        });

        QMenu* byType = editMenu->addMenu(tr("Sélectionner par &type"));
        const std::pair<QString, ElementKind> kinds[] = {
            { tr("Nœuds"), ElementKind::Node }, { tr("Poutres"), ElementKind::Beam },
            { tr("Poteaux"), ElementKind::Column }, { tr("Dalles"), ElementKind::Slab },
            { tr("Voiles"), ElementKind::Wall }, { tr("Fondations"), ElementKind::Foundation },
            { tr("Barres de treillis"), ElementKind::TrussMember }, { tr("Câbles"), ElementKind::Cable } };
        for (const auto& [label, kind] : kinds)
        {
            connect(byType->addAction(label), &QAction::triggered, this, [this, label = label, kind = kind]() {
                if (m_model) applyElementSelection(SQ::byKind(*m_model, kind), label);
            });
        }

        QAction* sameSection = editMenu->addAction(tr("Même &section que la sélection"));
        connect(sameSection, &QAction::triggered, this, [this]() {
            if (!m_model || !m_selectionManager) return;
            applyElementSelection(SQ::sameSection(*m_model, m_selectionManager->selectedElements()), tr("Même section"));
        });
        QAction* sameMaterial = editMenu->addAction(tr("Même &matériau que la sélection"));
        connect(sameMaterial, &QAction::triggered, this, [this]() {
            if (!m_model || !m_selectionManager) return;
            applyElementSelection(SQ::sameMaterial(*m_model, m_selectionManager->selectedElements()), tr("Même matériau"));
        });
        QAction* onLevel = editMenu->addAction(tr("Éléments du &niveau actif"));
        connect(onLevel, &QAction::triggered, this, [this]() {
            if (!m_model || !m_viewportContainer) return;
            applyElementSelection(SQ::atElevation(*m_model, m_viewportContainer->activeLevelElevation(),
                                                  TSA::Coordinate::GeometryTolerance::planeMembership),
                                  tr("Niveau actif"));
        });
        QAction* onPlane = editMenu->addAction(tr("Éléments du &plan de travail actif"));
        connect(onPlane, &QAction::triggered, this, [this]() {
            if (!m_model || !m_occView) return;
            applyElementSelection(SQ::onWorkPlane(*m_model, m_occView->activeWorkPlane(),
                                                  TSA::Coordinate::GeometryTolerance::planeMembership),
                                  tr("Plan de travail actif"));
        });
    }
    editMenu->addSeparator();
    editMenu->addAction(m_actionDelete);

    // 3. Menu Modélisation
    QMenu* modelMenu = menuBar()->addMenu(tr("&Modélisation"));
    QMenu* filarSub = modelMenu->addMenu(tr("Éléments Filaires (1D)"));
    filarSub->addAction(m_actionDrawBeam);
    filarSub->addAction(m_actionDrawColumn);
    filarSub->addAction(m_actionDrawBar);
    filarSub->addAction(m_actionDrawCable);
    filarSub->addAction(m_actionTruss);

    QMenu* surfSub = modelMenu->addMenu(tr("Éléments Surfaciques (2D)"));
    surfSub->addAction(m_actionDrawSlab);
    surfSub->addAction(m_actionDrawWall);
    surfSub->addAction(m_actionFooting);

    modelMenu->addSeparator();
    modelMenu->addAction(m_actionDrawNode);
    modelMenu->addAction(m_actionNewNode);
    modelMenu->addAction(m_actionAddCube);
    modelMenu->addSeparator();
    modelMenu->addAction(m_actionMoveOrigin);
    modelMenu->addSeparator();
    QMenu* gridSub = modelMenu->addMenu(tr("Trame && Niveaux"));
    gridSub->addAction(m_actionNewGrid);
    gridSub->addAction(m_actionGridManager);
    gridSub->addAction(m_actionManageLevels);
    modelMenu->addSeparator();
    modelMenu->addAction(m_actionStructurePresets);

    // 4. Menu Structure
    QMenu* structMenu = menuBar()->addMenu(tr("&Structure"));
    QMenu* secSubMenu = structMenu->addMenu(tr("Sections && Profilés"));
    secSubMenu->addAction(m_actionSecI);
    secSubMenu->addAction(m_actionSecRect);
    secSubMenu->addAction(m_actionSecCirc);

    QMenu* matSubMenu = structMenu->addMenu(tr("Matériaux"));
    matSubMenu->addAction(m_actionConcrete);
    matSubMenu->addAction(m_actionSteel);

    structMenu->addSeparator();
    QMenu* supSubMenu = structMenu->addMenu(tr("Conditions d'Appuis"));
    supSubMenu->addAction(m_actionFixed);
    supSubMenu->addAction(m_actionPinned);
    supSubMenu->addAction(m_actionRoller);

    // 5. Menu Calculs
    QMenu* analysisMenu = menuBar()->addMenu(tr("&Calculs"));
    QMenu* loadSubMenu = analysisMenu->addMenu(tr("Charges && Actions"));
    loadSubMenu->addAction(m_actionPointLoad);
    loadSubMenu->addAction(m_actionDistLoad);
    loadSubMenu->addAction(m_actionMoment);
    loadSubMenu->addAction(m_actionSeismic);
    loadSubMenu->addSeparator();
    loadSubMenu->addAction(m_actionLoadCases);
    analysisMenu->addSeparator();
    analysisMenu->addAction(m_actionMeshGen);
    analysisMenu->addSeparator();
    analysisMenu->addAction(m_actionAnalysisConfig);
    analysisMenu->addAction(m_actionRunSolve);
    analysisMenu->addAction(m_actionModal);
    analysisMenu->addAction(m_actionPushover);

    // 6. Menu Résultats
    QMenu* resMenu = menuBar()->addMenu(tr("&Résultats"));
    if (m_resultsDock) resMenu->addAction(m_resultsDock->toggleViewAction());
    if (m_analysisDataDock) resMenu->addAction(m_analysisDataDock->toggleViewAction());
    resMenu->addSeparator();
    resMenu->addAction(m_actionDeformedToggle);
    QMenu* diagSub = resMenu->addMenu(tr("Diagrammes d'Efforts 3D"));
    diagSub->addAction(m_actionDiagramMz);
    diagSub->addAction(m_actionDiagramMy);
    diagSub->addAction(m_actionDiagramMx);
    diagSub->addSeparator();
    diagSub->addAction(m_actionDiagramVz);
    diagSub->addAction(m_actionDiagramVy);
    diagSub->addSeparator();
    diagSub->addAction(m_actionDiagramN);
    diagSub->addSeparator();
    diagSub->addAction(m_actionDiagramDeflection);
    diagSub->addSeparator();
    diagSub->addAction(m_actionDiagramNone);
    resMenu->addAction(m_actionReactionsToggle);
    resMenu->addSeparator();
    QMenu* camSub = resMenu->addMenu(tr("Cadrage Résultats"));
    camSub->addAction(m_actionFitDeformed);
    camSub->addAction(m_actionFitResults);
    camSub->addAction(m_actionFitModel);
    camSub->addAction(m_actionFitAll);
    resMenu->addSeparator();
    resMenu->addAction(m_actionNoteDeCalcul);

    // 7. Menu Affichage
    QMenu* viewMenu = menuBar()->addMenu(tr("&Affichage"));
    QMenu* projSub = viewMenu->addMenu(tr("Projections && Orientations"));
    projSub->addAction(m_actionView3D);
    projSub->addAction(m_actionViewIsometric);
    projSub->addAction(m_actionViewHome);
    projSub->addSeparator();
    projSub->addAction(m_actionViewTop);
    projSub->addAction(m_actionViewBottom);
    projSub->addAction(m_actionViewFront);
    projSub->addAction(m_actionViewBack);
    projSub->addAction(m_actionViewLeft);
    projSub->addAction(m_actionViewRight);

    QMenu* navSub = viewMenu->addMenu(tr("Navigation && Zoom"));
    navSub->addAction(m_actionFitAll);
    navSub->addAction(m_actionFitSelection);
    navSub->addAction(m_actionZoomWindow);
    navSub->addSeparator();
    navSub->addAction(m_actionZoomIn);
    navSub->addAction(m_actionZoomOut);
    navSub->addSeparator();
    navSub->addAction(m_actionPreviousView);
    navSub->addAction(m_actionNextView);
    navSub->addSeparator();
    navSub->addAction(m_actionRotateLeft);
    navSub->addAction(m_actionRotateRight);
    navSub->addAction(m_actionResetView);

    QMenu* wpSub = viewMenu->addMenu(tr("Plans de Travail"));
    wpSub->addAction(m_actionWorkPlaneXY);
    wpSub->addAction(m_actionWorkPlaneLevel);
    wpSub->addAction(m_actionWorkPlaneXZ);
    wpSub->addAction(m_actionWorkPlaneYZ);
    wpSub->addSeparator();
    wpSub->addAction(m_actionViewNormalToPlane);
    wpSub->addAction(m_actionWorkPlaneVisible);
    wpSub->addSeparator();
    wpSub->addAction(m_actionWorkPlaneCustom);

    viewMenu->addSeparator();
    viewMenu->addAction(m_actionCoordSystem);
    viewMenu->addAction(m_actionSectionCut);
    viewMenu->addSeparator();
    QMenu* visSub = viewMenu->addMenu(tr("Aides Visuelles"));
    visSub->addAction(m_actionGridVisible);
    visSub->addAction(m_actionLevelsVisible);
    visSub->addAction(m_actionGridLabels);
    visSub->addAction(m_actionNodesVisible);
    visSub->addAction(m_actionNodeLabelsVisible);
    visSub->addAction(m_actionSupportsVisible);
    visSub->addAction(m_actionSupportLabelsVisible);
    visSub->addAction(m_actionLoadsVisible);
    visSub->addAction(m_actionForcesVisible);
    visSub->addAction(m_actionMomentsVisible);
    visSub->addAction(m_actionLoadValuesVisible);
    visSub->addAction(m_actionGridSnap);
    visSub->addAction(m_actionObjectSnap);
    visSub->addAction(m_actionRulersVisible);
    visSub->addAction(m_actionFullScreen);

    // 8. Menu Fenêtres (généré et synchronisé dynamiquement par WindowManager)
    if (m_windowManager)
    {
        m_windowManager->createWindowsMenu(menuBar());
    }

    // 9. Menu Outils
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Outils"));
    toolsMenu->addAction(m_actionMeasure);
    toolsMenu->addSeparator();
    toolsMenu->addAction(m_actionToggleTheme);

    // 10. Menu Aide
    QMenu* helpMenu = menuBar()->addMenu(tr("&Aide"));
    helpMenu->addAction(m_actionHelp);
    helpMenu->addAction(m_actionShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actionExportDiagnostic);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actionAbout);
}

void MainWindow::createRibbon()
{
    m_ribbonBar = new TSA::UI::RibbonBar(this);

    TSA::UI::RibbonActions acts;
    acts.actionNew = m_actionNew;
    acts.actionOpen = m_actionOpen;
    acts.actionSave = m_actionSave;
    acts.actionSaveAs = m_actionSaveAs;
    acts.actionExit = m_actionExit;

    acts.actionUndo = m_actionUndo;
    acts.actionRedo = m_actionRedo;
    acts.actionCopyClipboard = m_actionCopyClipboard;
    acts.actionPasteClipboard = m_actionPasteClipboard;

    acts.actionSelectMode = m_actionSelectMode;
    acts.actionMove3D = m_actionMove3D;
    acts.actionMove = m_actionMove;
    acts.actionCopy3D = m_actionCopy3D;
    acts.actionCopy = m_actionCopy;
    acts.actionRotate3D = m_actionRotate3D;
    acts.actionMirror = m_actionMirror;
    acts.actionSplitBars = m_actionSplitBars;
    acts.actionMergeNodes = m_actionMergeNodes;
    acts.actionMoveOrigin = m_actionMoveOrigin;
    acts.actionDelete = m_actionDelete;

    acts.actionDrawNode = m_actionDrawNode;
    acts.actionNewNode = m_actionNewNode;
    acts.actionDrawBar = m_actionDrawBar;
    acts.actionDrawBeam = m_actionDrawBeam;
    acts.actionDrawColumn = m_actionDrawColumn;
    acts.actionDrawCable = m_actionDrawCable;
    acts.actionDrawSlab = m_actionDrawSlab;
    acts.actionDrawWall = m_actionDrawWall;
    acts.actionTruss = m_actionTruss;
    acts.actionFooting = m_actionFooting;
    acts.actionAddCube = m_actionAddCube;
    acts.actionStructurePresets = m_actionStructurePresets;

    acts.actionNewGrid = m_actionNewGrid;
    acts.actionGridManager = m_actionGridManager;
    acts.actionManageLevels = m_actionManageLevels;

    acts.actionSecI = m_actionSecI;
    acts.actionSecRect = m_actionSecRect;
    acts.actionSecCirc = m_actionSecCirc;

    acts.actionConcrete = m_actionConcrete;
    acts.actionSteel = m_actionSteel;

    acts.actionFixed = m_actionFixed;
    acts.actionPinned = m_actionPinned;
    acts.actionRoller = m_actionRoller;
    acts.actionLibrary = m_actionLibrary;
    acts.actionExtensionManager = m_actionExtensionManager;

    acts.actionPointLoad = m_actionPointLoad;
    acts.actionDistLoad = m_actionDistLoad;
    acts.actionMoment = m_actionMoment;
    acts.actionSeismic = m_actionSeismic;
    acts.actionLoadCases = m_actionLoadCases;
    acts.actionLoadsVisible = m_actionLoadsVisible;
    acts.actionForcesVisible = m_actionForcesVisible;
    acts.actionMomentsVisible = m_actionMomentsVisible;
    acts.actionLoadValuesVisible = m_actionLoadValuesVisible;

    acts.actionMeshGen = m_actionMeshGen;
    acts.actionAnalysisConfig = m_actionAnalysisConfig;
    acts.actionRunSolve = m_actionRunSolve;
    acts.actionModal = m_actionModal;
    acts.actionPushover = m_actionPushover;

    acts.actionResultsDock = m_resultsDock ? m_resultsDock->toggleViewAction() : nullptr;
    acts.actionResultsDisp = m_actionResultsDisp;
    acts.actionResultsForces = m_actionResultsForces;
    acts.actionResultsStress = m_actionResultsStress;
    acts.actionDeformedToggle = m_actionDeformedToggle;
    acts.actionDiagramMz = m_actionDiagramMz;
    acts.actionDiagramMy = m_actionDiagramMy;
    acts.actionDiagramMx = m_actionDiagramMx;
    acts.actionDiagramVz = m_actionDiagramVz;
    acts.actionDiagramVy = m_actionDiagramVy;
    acts.actionDiagramN = m_actionDiagramN;
    acts.actionDiagramDeflection = m_actionDiagramDeflection;
    acts.actionDiagramNone = m_actionDiagramNone;
    acts.actionReactionsToggle = m_actionReactionsToggle;
    acts.actionFitModel = m_actionFitModel;
    acts.actionFitResults = m_actionFitResults;
    acts.actionFitDeformed = m_actionFitDeformed;
    acts.actionOpenNDC = m_actionNoteDeCalcul;

    acts.actionView3D = m_actionView3D;
    acts.actionViewXY = m_actionViewXY;
    acts.actionViewXZ = m_actionViewXZ;
    acts.actionViewYZ = m_actionViewYZ;
    acts.actionViewTop = m_actionViewTop;
    acts.actionViewBottom = m_actionViewBottom;
    acts.actionViewFront = m_actionViewFront;
    acts.actionViewBack = m_actionViewBack;
    acts.actionViewLeft = m_actionViewLeft;
    acts.actionViewRight = m_actionViewRight;
    acts.actionViewIsometric = m_actionViewIsometric;
    acts.actionViewHome = m_actionViewHome;

    acts.actionFitAll = m_actionFitAll;
    acts.actionFitSelection = m_actionFitSelection;
    acts.actionResetView = m_actionResetView;
    acts.actionZoomIn = m_actionZoomIn;
    acts.actionZoomOut = m_actionZoomOut;
    acts.actionZoomWindow = m_actionZoomWindow;
    acts.actionPreviousView = m_actionPreviousView;
    acts.actionNextView = m_actionNextView;

    acts.actionCoordSystem = m_actionCoordSystem;
    acts.actionWorkPlaneXY = m_actionWorkPlaneXY;
    acts.actionWorkPlaneXZ = m_actionWorkPlaneXZ;
    acts.actionWorkPlaneYZ = m_actionWorkPlaneYZ;
    acts.actionWorkPlaneLevel = m_actionWorkPlaneLevel;
    acts.actionWorkPlaneCustom = m_actionWorkPlaneCustom;
    acts.actionWorkPlaneVisible = m_actionWorkPlaneVisible;
    acts.actionViewNormalToPlane = m_actionViewNormalToPlane;
    acts.actionSectionCut = m_actionSectionCut;

    acts.actionGridVisible = m_actionGridVisible;
    acts.actionLevelsVisible = m_actionLevelsVisible;
    acts.actionGridLabels = m_actionGridLabels;
    acts.actionNodesVisible = m_actionNodesVisible;
    acts.actionNodeLabelsVisible = m_actionNodeLabelsVisible;
    acts.actionGridSnap = m_actionGridSnap;
    acts.actionObjectSnap = m_actionObjectSnap;
    acts.actionRulersVisible = m_actionRulersVisible;
    acts.actionFullScreen = m_actionFullScreen;

    if (m_modelTreeDock) acts.actionToggleModelTree = m_modelTreeDock->toggleViewAction();
    if (m_propertiesDock) acts.actionToggleProperties = m_propertiesDock->toggleViewAction();
    if (m_visibilityDock) acts.actionToggleVisibility = m_visibilityDock->toggleViewAction();
    if (m_consoleDock) acts.actionToggleConsole = m_consoleDock->toggleViewAction();

    acts.actionMeasure = m_actionMeasure;
    acts.actionToggleTheme = m_actionToggleTheme;
    acts.actionHelp = m_actionHelp;
    acts.actionShortcuts = m_actionShortcuts;
    acts.actionAbout = m_actionAbout;

    TSA::UI::RibbonBuilder::buildAllTabs(m_ribbonBar, acts, this);

    // Intégration en tant que barre d'outils supérieure fixe non-flottante façon AutoCAD Ribbon
    auto* ribbonToolBar = addToolBar(tr("Ruban Principal"));
    ribbonToolBar->setObjectName("RibbonToolBar");
    ribbonToolBar->setMovable(false);
    ribbonToolBar->setFloatable(false);
    ribbonToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    ribbonToolBar->setStyleSheet("QToolBar { border: none; background: transparent; margin: 0; padding: 0; }");
    ribbonToolBar->addWidget(m_ribbonBar);
}

void MainWindow::createToolBars()
{
    // Remplacé par createRibbon()
}

void MainWindow::createDockWindows()
{
    // 1. Dock gauche : MODEL TREE
    m_modelTreeDock = new QDockWidget(tr("ARBRE DU MODÈLE"), this);
    m_modelTreeDock->setObjectName("ModelTreeDock");
    m_modelTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_modelTree = new TSA::UI::ModelTreeWidget(m_model.get(), m_modelTreeDock);
    m_modelTreeDock->setWidget(m_modelTree);
    m_modelTreeDock->setMinimumWidth(280);
    m_modelTreeDock->toggleViewAction()->setIcon(QIcon(":/icons/model_tree.svg"));
    addDockWidget(Qt::LeftDockWidgetArea, m_modelTreeDock);

    // 2. Dock gauche ongletisé : CALQUES & VISIBILITÉ
    m_visibilityDock = new TSA::UI::VisibilityDock(this);
    m_visibilityDock->toggleViewAction()->setIcon(QIcon(":/icons/visibility.svg"));
    addDockWidget(Qt::LeftDockWidgetArea, m_visibilityDock);
    tabifyDockWidget(m_modelTreeDock, m_visibilityDock);
    m_modelTreeDock->raise();

    m_visibilityDock->bindGridVisibleAction(m_actionGridVisible);
    m_visibilityDock->bindLevelsVisibleAction(m_actionLevelsVisible);
    m_visibilityDock->bindGridLabelsAction(m_actionGridLabels);
    m_visibilityDock->bindNodesVisibleAction(m_actionNodesVisible);
    m_visibilityDock->bindNodeLabelsAction(m_actionNodeLabelsVisible);
    m_visibilityDock->bindLoadsVisibleAction(m_actionLoadsVisible);
    m_visibilityDock->bindLoadValuesVisibleAction(m_actionLoadValuesVisible);
    m_visibilityDock->bindRulersVisibleAction(m_actionRulersVisible);
    m_visibilityDock->bindCoordSystemAction(m_actionCoordSystem);
    m_visibilityDock->bindWorkPlaneVisibleAction(m_actionWorkPlaneVisible);

    // 3. Dock gauche ongletisé : ÉLÉMENTS STRUCTURAUX (Volet de dessin)
    m_elementsDock = new TSA::UI::StructuralElementsDock(this);
    m_elementsDock->toggleViewAction()->setIcon(QIcon(":/icons/draw_cable.svg"));
    addDockWidget(Qt::LeftDockWidgetArea, m_elementsDock);
    tabifyDockWidget(m_modelTreeDock, m_elementsDock);
    m_modelTreeDock->raise();

    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawBeamTriggered, this, &MainWindow::onModeDrawBeam);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawColumnTriggered, this, &MainWindow::onModeDrawColumn);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawBarTriggered, this, &MainWindow::onModeDrawBar);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawCableTriggered, this, &MainWindow::onModeDrawCable);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawTrussTriggered, this, &MainWindow::onActionTruss);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawSlabTriggered, this, &MainWindow::onModeDrawSlab);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawWallTriggered, this, &MainWindow::onModeDrawWall);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawPanelTriggered, this, &MainWindow::onModeDrawSlab);
    connect(m_elementsDock, &TSA::UI::StructuralElementsDock::drawFootingTriggered, this, &MainWindow::onActionFooting);

    // 3. Dock droit : PROPERTIES
    m_propertiesDock = new QDockWidget(tr("PROPRIÉTÉS"), this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_propertyPanel = new TSA::UI::PropertyPanel(m_model.get(), m_propertiesDock);
    m_propertiesDock->setWidget(m_propertyPanel);
    m_propertiesDock->setMinimumWidth(280);
    m_propertiesDock->toggleViewAction()->setIcon(QIcon(":/icons/properties.svg"));
    m_propertiesDock->toggleViewAction()->setShortcut(QKeySequence(Qt::Key_P));
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 4. Dock droit : PROJECTION & VUE (WorkPlane, 2D/3D, Caméra)
    m_projectionViewDock = new TSA::UI::ProjectionViewDock(this);
    m_projectionViewDock->setModel(m_model.get());
    m_projectionViewDock->toggleViewAction()->setIcon(QIcon(":/icons/view_normal_workplane.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_projectionViewDock);
    tabifyDockWidget(m_propertiesDock, m_projectionViewDock);
    m_propertiesDock->raise();

    // 5. Dock droit tabifié : ÉTAT DU PROJET
    m_projectStatusDock = new QDockWidget(tr("ÉTAT DU PROJET"), this);
    m_projectStatusDock->setObjectName("ProjectStatusDock");
    m_projectStatusDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_projectStatusWidget = new TSA::UI::ProjectStatusOverlay(m_occView, m_projectStatusDock);
    m_projectStatusWidget->setModel(m_model.get());
    m_projectStatusWidget->setGridManager(m_gridManager.get());
    m_projectStatusDock->setWidget(m_projectStatusWidget);
    m_projectStatusDock->setMinimumWidth(280);
    m_projectStatusDock->toggleViewAction()->setIcon(QIcon(":/icons/properties.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_projectStatusDock);
    tabifyDockWidget(m_propertiesDock, m_projectStatusDock);
    m_propertiesDock->raise();

    if (m_occView)
    {
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::standardViewRequested,
                m_occView, &OccView::applyStandardView);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::projectionModeRequested,
                m_occView, &OccView::setProjectionMode);
        connect(m_occView, &OccView::projectionModeChanged,
                m_projectionViewDock, &TSA::UI::ProjectionViewDock::syncProjectionMode);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::projectionDirectionRequested,
                m_occView, &OccView::setProjectionDirection);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::alignViewToWorkPlaneRequested,
                m_occView, &OccView::viewNormalToWorkPlane);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::workPlaneAxesVisibleToggled,
                m_occView, &OccView::setWorkPlaneAxesVisible);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::gizmoSizeChanged,
                m_occView, &OccView::setGizmoSize);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::workPlaneVisibleToggled,
                this, &MainWindow::onActionToggleWorkPlaneVisible);
        connect(m_projectionViewDock, &TSA::UI::ProjectionViewDock::activeWorkPlaneSelected,
                this, [this](int wpId) {
                    if (m_model && m_model->workPlaneManager() && m_occView)
                    {
                        m_model->workPlaneManager()->setActiveWorkPlane(wpId);
                        if (const auto* wp = m_model->workPlaneManager()->activeWorkPlane())
                        {
                            m_occView->setActiveWorkPlane(*wp);
                        }
                    }
                });
    }

    // 5. Dock droit : RÉSULTATS STRUCTURAUX 3D
    m_resultsDock = new TSA::UI::ResultsDockWidget(this);
    m_resultsDock->setModel(m_model.get());
    m_resultsDock->toggleViewAction()->setIcon(QIcon(":/icons/results_disp.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_resultsDock);
    tabifyDockWidget(m_propertiesDock, m_resultsDock);

    // Données numériques du calcul (matrices, DDL, forces brutes, export) : dock dédié,
    // le viewport n'affiche pas ces informations.
    m_analysisDataDock = new TSA::UI::AnalysisDataDock(this);
    m_analysisDataDock->setModel(m_model.get());
    addDockWidget(Qt::BottomDockWidgetArea, m_analysisDataDock);
    m_analysisDataDock->hide();
    m_propertiesDock->raise();

    if (m_occView)
    {
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::deformedToggled,
                this, &MainWindow::onActionToggleDeformed);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::deformedDisplayModeChanged,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setDeformedDisplayMode);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::deformationScalePresetChanged,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setDeformationScalePreset);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::diagramTypeChanged,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setDiagramType);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::diagramScalePresetChanged,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setDiagramScalePreset);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::diagramLabelsToggled,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setDiagramLabelsVisible);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::reactionsToggled,
                this, &MainWindow::onActionToggleReactions);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::activeStepChanged,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setActiveStep);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::legendToggled,
                m_occView->resultsVisual(), &TSA::Viewer::ResultsVisualManager::setLegendVisible);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::nodesVisibleToggled,
                m_occView, &OccView::setNodesVisible);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::nodeLabelsToggled,
                m_occView, &OccView::setNodeLabelsVisible);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::nodeFilterChanged,
                m_occView, &OccView::setNodeDisplayFilter);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::fitModelRequested,
                m_occView, &OccView::fitModel);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::fitResultsRequested,
                m_occView, &OccView::fitResults);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::fitDeformedRequested,
                m_occView, &OccView::fitDeformed);
        connect(m_resultsDock, &TSA::UI::ResultsDockWidget::fitSelectionRequested,
                m_occView, &OccView::fitSelection);
    }

    // 6. Dock inférieur : CONSOLE & HISTORIQUE COMMANDES
    m_consoleDock = new TSA::UI::LogConsoleDock(this);
    m_consoleDock->toggleViewAction()->setIcon(QIcon(":/icons/console.svg"));
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);

    // 7. Dock inférieur tabifié : DIAGRAMMES 2D & COURBES
    m_diagramDock = new QDockWidget(tr("DIAGRAMMES 2D"), this);
    m_diagramDock->setObjectName("DiagramDock");
    m_diagramDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    m_diagramWidget = new TSA::UI::Diagram2DWidget(m_diagramDock);
    m_diagramWidget->setModel(m_model.get());
    m_diagramDock->setWidget(m_diagramWidget);
    m_diagramDock->toggleViewAction()->setIcon(QIcon(":/icons/results_force.svg"));
    addDockWidget(Qt::BottomDockWidgetArea, m_diagramDock);
    if (m_consoleDock)
    {
        tabifyDockWidget(m_consoleDock, m_diagramDock);
        m_consoleDock->raise();
    }
    m_diagramDock->hide();

    // 8. Dock droit tabifié : NOTE DE CALCUL (NDC)
    m_ndcDock = new QDockWidget(tr("NOTE DE CALCUL"), this);
    m_ndcDock->setObjectName("NdcDock");
    m_ndcDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_ndcWidget = new TSA::NDC::NDCViewerWidget(m_ndcDock);
    m_ndcWidget->setModel(m_model.get());
    m_ndcDock->setWidget(m_ndcWidget);
    m_ndcDock->toggleViewAction()->setIcon(QIcon(":/icons/ndc_report.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_ndcDock);
    tabifyDockWidget(m_propertiesDock, m_ndcDock);
    m_propertiesDock->raise();
    m_ndcDock->hide();

    connect(m_consoleDock, &TSA::UI::LogConsoleDock::commandEntered, this, [this](const QString& cmd) {
        QString c = cmd.toUpper().trimmed();
        if (c == "FIT") onFitAll();
        else if (c == "FITSEL" || c == "FS") onFitSelection();
        else if (c == "ZOOMIN" || c == "ZI" || c == "+") onZoomIn();
        else if (c == "ZOOMOUT" || c == "ZO" || c == "-") onZoomOut();
        else if (c == "ZOOMW" || c == "ZW") onZoomWindow();
        else if (c == "PREV" || c == "VPREV") onPreviousView();
        else if (c == "NEXT" || c == "VNEXT") onNextView();
        else if (c == "HOME" || c == "VHOME") onActionViewHome();
        else if (c == "TOP" || c == "VTOP") onActionViewTop();
        else if (c == "BOTTOM" || c == "VBOT") onActionViewBottom();
        else if (c == "FRONT" || c == "VFRONT") onActionViewFront();
        else if (c == "BACK" || c == "VBACK") onActionViewBack();
        else if (c == "LEFT" || c == "VLEFT") onActionViewLeft();
        else if (c == "RIGHT" || c == "VRIGHT") onActionViewRight();
        else if (c == "ISO" || c == "VISO") onActionViewIsometric();
        else if (c == "WPXY") onWorkPlaneXY();
        else if (c == "WPXZ") onWorkPlaneXZ();
        else if (c == "WPYZ") onWorkPlaneYZ();
        else if (c == "WPLEVEL") onWorkPlaneLevel();
        else if (c == "WPCUSTOM" || c == "WP") onActionWorkPlaneCustom();
        else if (c == "WPNORMAL" || c == "VPN") onActionViewNormalToPlane();
        else if (c == "WPSHOW") onActionToggleWorkPlaneVisible(true);
        else if (c == "WPHIDE") onActionToggleWorkPlaneVisible(false);
        else if (c == "SNAP" || c == "GRIDS") {
            if (m_actionGridSnap) m_actionGridSnap->setChecked(!m_actionGridSnap->isChecked());
        }
        else if (c == "OSNAP") {
            if (m_actionObjectSnap) m_actionObjectSnap->setChecked(!m_actionObjectSnap->isChecked());
        }
        else if (c == "SELECTALL" || c == "ALL") onActionSelectAll();
        else if (c == "PROP" || c == "PROPERTIES" || c == "P") {
            if (m_propertiesDock) m_propertiesDock->setVisible(!m_propertiesDock->isVisible());
        }
        else if (c == "UNDO" || c == "U") onActionUndo();
        else if (c == "REDO") onActionRedo();
        else if (c == "SAVE") onActionSave();
        else if (c == "OPEN") onActionOpen();
        else if (c == "NEW") onActionNew();
        else if (c == "FULLSCREEN" || c == "FSCR") {
            if (m_actionFullScreen) m_actionFullScreen->trigger();
        }
        else if (c == "RESET") onResetView();
        else if (c == "SELECT" || c == "ESC") onModeSelect();
        else if (c == "NODE" || c == "N") onModeDrawNode();
        else if (c == "WIRE" || c == "FILAIRE") onModeDrawWire();
        else if (c == "BAR" || c == "BARRE") onModeDrawBar();
        else if (c == "BEAM" || c == "B" || c == "POUTRE") onModeDrawBeam();
        else if (c == "COLUMN" || c == "C" || c == "POTEAU") onModeDrawColumn();
        else if (c == "CABLE" || c == "CABL") onModeDrawCable();
        else if (c == "SURF" || c == "SURFACE") onModeDrawSurface();
        else if (c == "SLAB" || c == "L" || c == "DALLE") onModeDrawSlab();
        else if (c == "WALL" || c == "W" || c == "VOILE") onModeDrawWall();
        else if (c == "TRUSS" || c == "TREILLIS") onActionTruss();
        else if (c == "FOOTING" || c == "SEMELLE" || c == "FONDATION") onActionFooting();
        else if (c == "SECI" || c == "IPE" || c == "HEA" || c == "HEB") onActionSecI();
        else if (c == "SECRECT" || c == "RECT") onActionSecRect();
        else if (c == "SECCIRC" || c == "CIRC") onActionSecCirc();
        else if (c == "CONCRETE" || c == "BETON") onActionConcrete();
        else if (c == "STEEL" || c == "ACIER") onActionSteel();
        else if (c == "FIXED" || c == "ENCASTREMENT") onActionFixed();
        else if (c == "PINNED" || c == "ROTULE") onActionPinned();
        else if (c == "ROLLER" || c == "APPUI") onActionRoller();
        else if (c == "LOAD" || c == "FORCE" || c == "CHARGE") onActionPointLoad();
        else if (c == "DISTLOAD" || c == "QLOAD") onActionDistLoad();
        else if (c == "MOMENT") onActionMoment();
        else if (c == "CAS" || c == "LOADCASE" || c == "COMBINAISON" || c == "OPENSEES") onActionLoadCases();
        else if (c == "SEISMIC" || c == "SEISME") onActionSeismic();
        else if (c == "MESH" || c == "MAILLAGE") onActionMeshGen();
        else if (c == "SOLVE" || c == "CALC" || c == "RUN") onActionRunSolve();
        else if (c == "MODAL" || c == "FREQ") onActionModal();
        else if (c == "DISP" || c == "DEPLACEMENT") onActionResultsDisp();
        else if (c == "FORCES" || c == "DIAGRAM") onActionResultsForces();
        else if (c == "STRESS" || c == "CONTRAINTE") onActionResultsStress();
        else if (c == "MEASURE" || c == "DIST" || c == "DI") onActionMeasure();
        else if (c == "GRID" || c == "G") {
            if (m_actionGridVisible) m_actionGridVisible->setChecked(!m_actionGridVisible->isChecked());
        }
        else if (c == "DEL" || c == "DELETE") onActionDeleteSelected();
        else if (c == "MOVE" || c == "M") onActionMove();
        else if (c == "COPY") onActionCopy();
        else if (c == "MIRROR" || c == "MI") onActionMirror();
        else if (c == "SPLIT" || c == "DIVIDE") onActionSplitBars();
        else if (c == "MERGE" || c == "FUSION") onActionMergeNodes();
        else if (c == "THEME") onToggleTheme();
        else if (c == "DARK") {
            if (!TSA::UI::ThemeManager::instance().isDarkMode()) onToggleTheme();
        }
        else if (c == "LIGHT") {
            if (TSA::UI::ThemeManager::instance().isDarkMode()) onToggleTheme();
        }
        else if (c == "HELP" || c == "AIDE" || c == "?") onActionHelp();
        else if (c == "DIAG" || c == "REPORT" || c == "DIAGNOSTIC") onActionExportDiagnosticReport();
        else {
            m_consoleDock->appendLog(tr("Commande inconnue : '%1'. Commandes supportées : BEAM, COLUMN, SLAB, WALL, TRUSS, FOOTING, SECI, SECRECT, SECCIRC, CONCRETE, STEEL, FIXED, PINNED, ROLLER, LOAD, DISTLOAD, MOMENT, SEISMIC, MESH, SOLVE, MODAL, DISP, FORCES, STRESS, MEASURE, FIT, RESET, GRID, DEL, MOVE, COPY, MIRROR, SPLIT, MERGE, THEME, DIAG, HELP").arg(cmd), "WARN");
        }
    });

    connect(m_consoleDock, &TSA::UI::LogConsoleDock::exportReportRequested, this, &MainWindow::onActionExportDiagnosticReport);

    // 1. Sélection depuis le MODEL TREE
    connect(m_modelTree, &TSA::UI::ModelTreeWidget::levelSelected, this, [this](const QString& levelId) {
        m_selectionManager->clearSelection();
        m_occView->clearHighlight();
        m_propertyPanel->showLevelProperties(levelId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::workPlaneSelected, this, [this](int axis, double offset, const QString& name) {
        if (m_viewportContainer)
        {
            m_viewportContainer->setActivePlane(static_cast<TSA::Coordinate::WorkPlaneAxis>(axis), offset);
        }
        else if (m_occView)
        {
            m_occView->setWorkPlaneAxisAndOffset(static_cast<TSA::Coordinate::WorkPlaneAxis>(axis), offset, name.toStdString());
        }
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::nodeSelected, this, [this](int nodeId) {
        m_selectionManager->selectNode(nodeId);
        m_occView->highlightNode(nodeId);
        m_propertyPanel->showNodeProperties(nodeId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::beamSelected, this, [this](int beamId) {
        m_selectionManager->selectBeam(beamId);
        m_occView->highlightBeam(beamId);
        m_propertyPanel->showBeamProperties(beamId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::columnSelected, this, [this](int columnId) {
        m_selectionManager->selectColumn(columnId);
        m_occView->highlightColumn(columnId);
        m_propertyPanel->showColumnProperties(columnId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::slabSelected, this, [this](int slabId) {
        m_selectionManager->selectSlab(slabId);
        m_occView->highlightSlab(slabId);
        m_propertyPanel->showSlabProperties(slabId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::wallSelected, this, [this](int wallId) {
        m_selectionManager->selectWall(wallId);
        m_occView->highlightWall(wallId);
        m_propertyPanel->showWallProperties(wallId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::foundationSelected, this, [this](int fId) {
        m_selectionManager->selectFoundation(fId);
        m_occView->highlightFoundation(fId);
        m_propertyPanel->showFoundationProperties(fId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::trussMemberSelected, this, [this](int trId) {
        m_selectionManager->selectTrussMember(trId);
        m_occView->highlightTrussMember(trId);
        m_propertyPanel->showTrussMemberProperties(trId);
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::selectionCleared, this, [this]() {
        m_selectionManager->clearSelection();
        m_occView->clearHighlight();
        m_propertyPanel->clearProperties();
    });

    // 2. Sélection depuis le VIEWPORT 3D (clic souris)
    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::nodeSelected, this, [this](int nodeId) {
        m_modelTree->selectNodeItem(nodeId);
        m_occView->highlightNode(nodeId);
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showNodeProperties(nodeId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Node %1").arg(nodeId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::beamSelected, this, [this](int beamId) {
        m_modelTree->selectBeamItem(beamId);
        m_occView->highlightBeam(beamId);
        m_occView->detachManipulator();
        m_occView->updateSelectedElementLocalAxes();
        m_propertyPanel->showBeamProperties(beamId);
        if (m_barDialog && m_barDialog->isVisible() && m_model)
        {
            if (const auto* b = m_model->getBeam(beamId))
            {
                m_barDialog->loadFromBar(*b);
            }
        }
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Beam %1").arg(beamId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::columnSelected, this, [this](int columnId) {
        m_modelTree->selectColumnItem(columnId);
        m_occView->highlightColumn(columnId);
        m_occView->detachManipulator();
        m_occView->updateSelectedElementLocalAxes();
        m_propertyPanel->showColumnProperties(columnId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Column %1").arg(columnId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::slabSelected, this, [this](int slabId) {
        m_modelTree->selectSlabItem(slabId);
        m_occView->highlightSlab(slabId);
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showSlabProperties(slabId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Slab %1").arg(slabId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::wallSelected, this, [this](int wallId) {
        m_modelTree->selectWallItem(wallId);
        m_occView->highlightWall(wallId);
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showWallProperties(wallId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Wall %1").arg(wallId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::foundationSelected, this, [this](int fId) {
        m_modelTree->selectFoundationItem(fId);
        m_occView->highlightFoundation(fId);
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showFoundationProperties(fId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Foundation %1").arg(fId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::trussMemberSelected, this, [this](int trId) {
        m_modelTree->selectTrussMemberItem(trId);
        m_occView->highlightTrussMember(trId);
        m_occView->detachManipulator();
        m_occView->updateSelectedElementLocalAxes();
        m_propertyPanel->showTrussMemberProperties(trId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Truss Member %1").arg(trId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::cableSelected, this, [this](int cableId) {
        m_modelTree->selectCableItem(cableId);
        m_occView->highlightCable(cableId);
        m_occView->detachManipulator();
        m_occView->updateSelectedElementLocalAxes();
        m_propertyPanel->showCableProperties(cableId);
        if (m_cableDialog && m_cableDialog->isVisible() && m_model)
        {
            if (const auto* c = m_model->getCable(cableId))
            {
                m_cableDialog->loadFromCable(*c);
            }
        }
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Câble sélectionné C%1").arg(cableId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::workPlaneSelected, this, [this](int wpId) {
        m_propertyPanel->showWorkPlaneProperties(wpId);
        m_occView->attachManipulatorToWorkPlane();
        m_occView->clearSelectedElementLocalAxes();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Plan de travail WP%1 sélectionné (Manipulateur 3D interactif)").arg(wpId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::nodalLoadSelected, this, [this](int loadId) {
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showNodalLoadProperties(loadId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Charge Nodale #%1 sélectionnée").arg(loadId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::memberLoadSelected, this, [this](int loadId) {
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->showMemberLoadProperties(loadId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Charge sur Barre #%1 sélectionnée").arg(loadId));
        }
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::cableSelected, this, [this](int cableId) {
        m_selectionManager->clearSelection();
        m_selectionManager->selectCable(cableId);
        m_occView->highlightCable(cableId);
        m_propertyPanel->showCableProperties(cableId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Câble sélectionné C%1").arg(cableId));
        }
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::loadSelected, this, [this](int loadId) {
        m_selectionManager->clearSelection();
        m_occView->clearHighlight();
        m_propertyPanel->showMemberLoadProperties(loadId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Charge #%1 sélectionnée").arg(loadId));
        }
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::supportSelected, this, [this](int nodeId) {
        m_selectionManager->clearSelection();
        m_selectionManager->selectNode(nodeId);
        m_occView->highlightNode(nodeId);
        m_propertyPanel->showNodeProperties(nodeId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Appui sur Nœud N%1 sélectionné").arg(nodeId));
        }
    });

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::resultsSelected, this, [this]() {
        if (m_resultsDock)
        {
            m_resultsDock->show();
            m_resultsDock->raise();
        }
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Résultats d'analyse"));
        }
    });

    connect(m_propertyPanel, &TSA::UI::PropertyPanel::elementModified, this, [this]() {
        // L'arbre est observateur du modèle (notify*Modified met à jour la ligne concernée) :
        // pas de reconstruction complète à chaque édition de propriété.
        m_occView->update();
        updateUndoRedoActions();
    });

    connect(m_propertyPanel, &TSA::UI::PropertyPanel::workPlaneModified, this, [this](const TSA::Coordinate::WorkPlane& wp) {
        if (m_model && m_model->workPlaneManager())
        {
            m_model->workPlaneManager()->updateWorkPlane(wp);
        }
        m_occView->setActiveWorkPlane(wp);
        updateUndoRedoActions();
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::selectionCleared, this, [this]() {
        m_modelTree->clearTreeSelection();
        m_occView->clearHighlight();
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_propertyPanel->clearProperties();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Ready"));
        }
    });

    // Sélection ensembliste (tout sélectionner, inverser, par type...) : surbrillance de tout
    // l'ensemble en une passe, propriétés de l'élément principal, arbre désélectionné (sélectionner
    // des milliers d'items dans l'arbre serait coûteux et illisible).
    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::multipleSelectionChanged, this, [this]() {
        m_modelTree->clearTreeSelection();
        m_occView->detachManipulator();
        m_occView->clearSelectedElementLocalAxes();
        m_occView->highlightSelection();
        const int id = m_selectionManager->primarySelectedId();
        switch (m_selectionManager->currentSelectionType())
        {
        case TSA::Viewer::SelectionType::Node: m_propertyPanel->showNodeProperties(id); break;
        case TSA::Viewer::SelectionType::Beam: m_propertyPanel->showBeamProperties(id); break;
        case TSA::Viewer::SelectionType::Column: m_propertyPanel->showColumnProperties(id); break;
        case TSA::Viewer::SelectionType::Slab: m_propertyPanel->showSlabProperties(id); break;
        case TSA::Viewer::SelectionType::Wall: m_propertyPanel->showWallProperties(id); break;
        case TSA::Viewer::SelectionType::Foundation: m_propertyPanel->showFoundationProperties(id); break;
        case TSA::Viewer::SelectionType::TrussMember: m_propertyPanel->showTrussMemberProperties(id); break;
        case TSA::Viewer::SelectionType::Cable: m_propertyPanel->showCableProperties(id); break;
        default: m_propertyPanel->clearProperties(); break;
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::selectionChanged, this, [this]() {
        size_t total = m_selectionManager->totalSelectedCount();
        if (total > 1 && m_statusInfo)
        {
            m_statusInfo->setText(tr("Sélection multiple : %1 éléments (%2 nœuds, %3 poutres, %4 poteaux, %5 dalles)")
                .arg(total)
                .arg(m_selectionManager->selectedNodes().size())
                .arg(m_selectionManager->selectedBeams().size())
                .arg(m_selectionManager->selectedColumns().size())
                .arg(m_selectionManager->selectedSlabs().size()));
        }
    });

    // Enregistrement centralisé de toutes les fenêtres et panneaux dans WindowManager
    if (m_windowManager)
    {
        m_windowManager->registerWindow(
            "viewport", tr("Vue 3D"), tr("Général"), m_viewportContainer,
            Qt::NoDockWidgetArea, true, QKeySequence("Ctrl+1"), QIcon(":/icons/view/view_3d.svg"));

        m_windowManager->registerDock(
            "model_browser", tr("Navigateur du modèle"), tr("Modélisation"), m_modelTreeDock,
            Qt::LeftDockWidgetArea, true, QKeySequence("Ctrl+3"), QIcon(":/icons/model_tree.svg"));

        m_windowManager->registerDock(
            "visibility", tr("Calques & Visibilité"), tr("Affichage"), m_visibilityDock,
            Qt::LeftDockWidgetArea, true, QKeySequence(), QIcon(":/icons/visibility.svg"));

        m_windowManager->registerDock(
            "elements", tr("Éléments structuraux"), tr("Modélisation"), m_elementsDock,
            Qt::LeftDockWidgetArea, true, QKeySequence(), QIcon(":/icons/draw_cable.svg"));

        m_windowManager->registerDock(
            "properties", tr("Propriétés"), tr("Général"), m_propertiesDock,
            Qt::RightDockWidgetArea, true, QKeySequence("Ctrl+2"), QIcon(":/icons/properties.svg"));

        m_windowManager->registerDock(
            "work_planes", tr("Plans de travail & Vues"), tr("Modélisation"), m_projectionViewDock,
            Qt::RightDockWidgetArea, true, QKeySequence("Ctrl+4"), QIcon(":/icons/view_normal_workplane.svg"));

        if (m_analysisDataDock)
        {
            m_windowManager->registerDock(
                "analysis_data", tr("Données d'analyse (OpenSees)"), tr("Résultats"), m_analysisDataDock,
                Qt::BottomDockWidgetArea, false, QKeySequence(), QIcon(":/icons/results_disp.svg"));
        }

        m_windowManager->registerDock(
            "console", tr("Console & Messages"), tr("Outils"), m_consoleDock,
            Qt::BottomDockWidgetArea, true, QKeySequence(Qt::Key_F2), QIcon(":/icons/console.svg"));
    }
}

void MainWindow::createStatusBar()
{
    QStatusBar* bar = statusBar();

    // 1. Nom du fichier / Projet .tsa
    // IMPORTANT : le QStatusBar impose comme largeur minimale la somme des tailles minimales
    // de ses widgets. Tout label dont le texte varie doit donc avoir une largeur stable
    // (setFixedWidth) ou une politique Ignored, sinon la fenêtre s'agrandit toute seule.
    m_statusProject = new QLabel(tr("Sans titre.tsa"), this);
    m_statusProject->setStyleSheet("font-weight: bold; color: #38bdf8; padding: 2px 10px; border-right: 1px solid #475569;");
    m_statusProject->setMinimumWidth(120);
    m_statusProject->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_statusProject);

    // 2. Vue actuelle (ISO, Dessus, etc.)
    m_statusView = new QLabel(tr("Vue ISO"), this);
    m_statusView->setStyleSheet("font-weight: 500; color: #a78bfa; padding: 2px 10px; border-right: 1px solid #475569;");
    bar->addWidget(m_statusView);

    // 3. Unités de calcul et de modélisation
    m_statusUnits = new QLabel(tr("kN, m"), this);
    m_statusUnits->setStyleSheet("font-weight: 500; color: #34d399; padding: 2px 10px; border-right: 1px solid #475569;");
    bar->addWidget(m_statusUnits);

    // 4. Niveau actif
    m_statusLevel = new QLabel(tr("Niveau : Tous"), this);
    m_statusLevel->setStyleSheet("font-weight: 500; color: #fbbf24; padding: 2px 10px; border-right: 1px solid #475569;");
    m_statusLevel->setMinimumWidth(110);
    m_statusLevel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_statusLevel);

    // 5. Coordonnées globales X, Y, Z
    m_statusCoordinates = new QLabel(tr("X: 0.000 m   Y: 0.000 m   Z: 0.000 m"), this);
    m_statusCoordinates->setFixedWidth(330);
    m_statusCoordinates->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; padding: 2px 8px;");
    bar->addWidget(m_statusCoordinates);

    m_statusCoordinatesLocal = new QLabel(tr("Xwp: 0.000 m   Ywp: 0.000 m"), this);
    m_statusCoordinatesLocal->setFixedWidth(250);
    m_statusCoordinatesLocal->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; padding: 2px 8px; color: #a78bfa;");
    bar->addWidget(m_statusCoordinatesLocal);

    m_statusWorkPlane = new QLabel(tr("Plan: XY (Z=0.00 m)"), this);
    m_statusWorkPlane->setStyleSheet("font-family: Consolas, monospace; padding: 2px 8px; color: #38bdf8; font-weight: bold;");
    m_statusWorkPlane->setMinimumWidth(160);
    m_statusWorkPlane->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_statusWorkPlane);

    m_statusSnap = new QLabel(tr("SNAP: ACTIF"), this);
    m_statusSnap->setStyleSheet("font-family: Consolas, monospace; padding: 2px 8px; color: #4ade80; font-weight: bold;");
    m_statusSnap->setMinimumWidth(110);
    m_statusSnap->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_statusSnap);

    // Message d'information (survol, niveau actif...) : texte libre et potentiellement long
    // => ne doit jamais contribuer à la largeur minimale de la fenêtre.
    m_statusInfo = new QLabel(tr("Prêt"), this);
    m_statusInfo->setMinimumWidth(150);
    m_statusInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_statusInfo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bar->addPermanentWidget(m_statusInfo, 1);

    m_statusLogo = new TSA::UI::TSALogoOverlay(this);
    m_statusLogo->setDarkMode(TSA::UI::ThemeManager::instance().isDarkMode());
    bar->addPermanentWidget(m_statusLogo);

    // Synchronisation du plan de travail et de l'historique caméra
    connect(m_occView, &OccView::workPlaneChanged, this, &MainWindow::onWorkPlaneChanged);
    if (m_occView)
    {
        onWorkPlaneChanged(m_occView->activeWorkPlane());
    }

    if (m_viewportContainer)
    {
        connect(m_viewportContainer, &TSA::UI::ViewportContainer::activeLevelChanged, this, [this](double elev, const QString& name) {
            if (m_statusLevel)
            {
                m_statusLevel->setText(tr("Niveau : %1").arg(name.isEmpty() ? tr("Tous") : name));
            }
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Niveau actif : %1 (Z=%2 m)").arg(name).arg(elev, 0, 'f', 2));
            }
        });

        connect(m_viewportContainer, &TSA::UI::ViewportContainer::activeWorkPlaneChanged, this, [this](TSA::Coordinate::WorkPlaneAxis axis, double offset, const QString& name) {
            QString axisStr = (axis == TSA::Coordinate::WorkPlaneAxis::Z) ? "Plan Z" : ((axis == TSA::Coordinate::WorkPlaneAxis::X) ? "Coupe X" : "Coupe Y");
            if (m_statusLevel)
            {
                m_statusLevel->setText(tr("%1 : %2").arg(axisStr).arg(name.isEmpty() ? tr("Tous") : name));
            }
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("%1 actif : %2 (offset=%3 m)").arg(axisStr).arg(name).arg(offset, 0, 'f', 2));
            }
        });
    }

    connect(m_occView, &OccView::cameraHistoryChanged, this, [this](bool hasPrev, bool hasNext) {
        if (m_actionPreviousView) m_actionPreviousView->setEnabled(hasPrev);
        if (m_actionNextView) m_actionNextView->setEnabled(hasNext);
    });

    // Suivi continu des coordonnées du pointeur de souris
    connect(m_occView, &OccView::mouseCoordinatesChanged, this, [this](double x, double y, double z) {
        if (m_statusCoordinates)
        {
            m_statusCoordinates->setText(tr("X: %1 m   Y: %2 m   Z: %3 m")
                .arg(x, 7, 'f', 3)
                .arg(y, 7, 'f', 3)
                .arg(z, 7, 'f', 3));
        }
    });

    // Suivi continu des coordonnées locales WorkPlane
    connect(m_occView, &OccView::mouseLocalCoordinatesChanged, this, [this](double xwp, double ywp) {
        if (m_statusCoordinatesLocal)
        {
            m_statusCoordinatesLocal->setText(tr("Xwp: %1 m   Ywp: %2 m")
                .arg(xwp, 7, 'f', 3)
                .arg(ywp, 7, 'f', 3));
        }
    });

    // Détection et repérage au survol des objets
    connect(m_occView, &OccView::objectHovered, this, [this](const QString& info) {
        if (m_statusInfo)
        {
            if (!info.isEmpty())
            {
                m_statusInfo->setText(info);
            }
            else if (m_selectionManager && m_selectionManager->hasSelection())
            {
                if (m_selectionManager->currentSelectionType() == TSA::Viewer::SelectionType::Node)
                {
                    m_statusInfo->setText(tr("Selected Node %1").arg(m_selectionManager->primarySelectedId()));
                }
                else if (m_selectionManager->currentSelectionType() == TSA::Viewer::SelectionType::Beam)
                {
                    m_statusInfo->setText(tr("Selected Beam %1").arg(m_selectionManager->primarySelectedId()));
                }
                else if (m_selectionManager->currentSelectionType() == TSA::Viewer::SelectionType::Column)
                {
                    m_statusInfo->setText(tr("Selected Column %1").arg(m_selectionManager->primarySelectedId()));
                }
                else if (m_selectionManager->currentSelectionType() == TSA::Viewer::SelectionType::Slab)
                {
                    m_statusInfo->setText(tr("Selected Slab %1").arg(m_selectionManager->primarySelectedId()));
                }
            }
            else if (m_model)
            {
                m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
                    .arg(m_model->nodes().size())
                    .arg(m_model->beams().size())
                    .arg(m_model->columns().size())
                    .arg(m_model->slabs().size()));
            }
        }
    });

    connect(m_occView, &OccView::gridVisibilityChanged, this, [this](bool visible) {
        if (m_actionGridVisible) m_actionGridVisible->setChecked(visible);
    });

    connect(m_occView, &OccView::gridSnapChanged, this, [this](bool enabled) {
        if (m_actionGridSnap) m_actionGridSnap->setChecked(enabled);
    });

    connect(m_occView, &OccView::objectSnapChanged, this, [this](bool enabled) {
        if (m_actionObjectSnap) m_actionObjectSnap->setChecked(enabled);
    });

    connect(m_occView, &OccView::interactionModeChanged, this, [this](OccView::InteractionMode mode) {
        switch (mode)
        {
        case OccView::InteractionMode::Select:
            if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
            break;
        case OccView::InteractionMode::DrawNode:
            if (m_actionDrawNode) m_actionDrawNode->setChecked(true);
            break;
        case OccView::InteractionMode::DrawBar:
            if (m_actionDrawBar) m_actionDrawBar->setChecked(true);
            break;
        case OccView::InteractionMode::DrawBeam:
            if (m_actionDrawBeam) m_actionDrawBeam->setChecked(true);
            break;
        case OccView::InteractionMode::DrawColumn:
            if (m_actionDrawColumn) m_actionDrawColumn->setChecked(true);
            break;
        case OccView::InteractionMode::DrawSlab:
            if (m_actionDrawSlab) m_actionDrawSlab->setChecked(true);
            break;
        case OccView::InteractionMode::DrawWall:
            if (m_actionDrawWall) m_actionDrawWall->setChecked(true);
            break;
        case OccView::InteractionMode::Move3D:
            if (m_actionMove3D) m_actionMove3D->setChecked(true);
            break;
        case OccView::InteractionMode::Copy3D:
            if (m_actionCopy3D) m_actionCopy3D->setChecked(true);
            break;
        case OccView::InteractionMode::Rotate3D:
            if (m_actionRotate3D) m_actionRotate3D->setChecked(true);
            break;
        case OccView::InteractionMode::MoveOrigin3D:
            if (m_actionMoveOrigin) m_actionMoveOrigin->setChecked(true);
            break;
        case OccView::InteractionMode::DrawCable:
        case OccView::InteractionMode::DrawStayCable:
        case OccView::InteractionMode::DrawSuspensionCable:
        case OccView::InteractionMode::DrawHanger:
            if (m_actionDrawCable) m_actionDrawCable->setChecked(true);
            break;
        case OccView::InteractionMode::DrawFoundation:
        case OccView::InteractionMode::DrawTruss:
        case OccView::InteractionMode::Paste3D:
            // Pas de QAction checkable dédiée pour ces modes
            break;
        default:
            break;
        }
    });

    connect(m_occView, &OccView::pointToPointMoveRequested, this, &MainWindow::onPointToPointMoveRequested);
    connect(m_occView, &OccView::pointToPointRotateRequested, this, &MainWindow::onPointToPointRotateRequested);
    connect(m_occView, &OccView::originMoveRequested, this, &MainWindow::onOriginMoveRequested);
    connect(m_occView, &OccView::pasteAtPointRequested, this, &MainWindow::onPasteAtPointRequested);

    connect(m_occView, &OccView::drawingPromptChanged, this, [this](const QString& prompt) {
        if (m_statusInfo)
        {
            m_statusInfo->setText(prompt);
        }
    });
}
