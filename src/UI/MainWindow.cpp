#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Properties/PropertyPanel.h"
#include "Dialogs/TransformDialog.h"
#include "Dialogs/GridDialog.h"
#include "Dialogs/GridSettingsDialog.h"
#include "Dialogs/LevelDialog.h"
#include "Dialogs/SectionCutDialog.h"
#include "Ruler/ViewportContainer.h"
#include "Theme/ThemeManager.h"

#include <QSettings>
#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <sstream>

namespace
{
static QIcon makePlanIcon(const QColor& planeColor, const QColor& axis1Color, const QColor& axis2Color, const QString& l1, const QString& l2)
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Fond du plan coloré style Robot SA
    p.setPen(QPen(planeColor.darker(150), 1.2));
    p.setBrush(QBrush(planeColor));
    p.drawRect(7, 4, 15, 15);

    // Axes
    p.setPen(QPen(axis1Color, 2.0));
    p.drawLine(5, 21, 23, 21); // Horizontal
    p.setPen(QPen(axis2Color, 2.0));
    p.drawLine(5, 21, 5, 3);   // Vertical

    // Flèches d'axe
    p.drawLine(23, 21, 20, 19);
    p.drawLine(23, 21, 20, 23);
    p.drawLine(5, 3, 3, 6);
    p.drawLine(5, 3, 7, 6);

    // Libellés d'axes
    QFont f = p.font();
    f.setPixelSize(7);
    f.setBold(true);
    p.setFont(f);
    p.setPen(axis1Color);
    p.drawText(20, 25, l1);
    p.setPen(axis2Color);
    p.drawText(0, 8, l2);

    return QIcon(pix);
}

static QIcon make3DIsoIcon()
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Face supérieure (XY)
    QPolygon topPoly;
    topPoly << QPoint(13, 3) << QPoint(22, 8) << QPoint(13, 13) << QPoint(4, 8);
    p.setPen(QPen(QColor(180, 50, 50), 1.0));
    p.setBrush(QColor(240, 110, 110, 220));
    p.drawPolygon(topPoly);

    // Face gauche (XZ)
    QPolygon leftPoly;
    leftPoly << QPoint(4, 8) << QPoint(13, 13) << QPoint(13, 23) << QPoint(4, 18);
    p.setPen(QPen(QColor(40, 150, 60), 1.0));
    p.setBrush(QColor(100, 210, 120, 220));
    p.drawPolygon(leftPoly);

    // Face droite (YZ)
    QPolygon rightPoly;
    rightPoly << QPoint(13, 13) << QPoint(22, 8) << QPoint(22, 18) << QPoint(13, 23);
    p.setPen(QPen(QColor(40, 80, 200), 1.0));
    p.setBrush(QColor(100, 140, 240, 220));
    p.drawPolygon(rightPoly);

    return QIcon(pix);
}

static QIcon makeCoordSystemIcon()
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Poutre inclinée
    p.setPen(QPen(QColor(50, 60, 80), 2.5));
    p.drawLine(3, 21, 23, 9);

    // Flèche normale (rouge)
    p.setPen(QPen(QColor(220, 30, 30), 2.0));
    p.drawLine(13, 15, 19, 4);
    p.drawLine(19, 4, 16, 5);

    // Flèche tangentielle (bleue)
    p.setPen(QPen(QColor(30, 90, 220), 2.0));
    p.drawLine(13, 15, 23, 9);
    p.drawLine(23, 9, 20, 9);

    return QIcon(pix);
}

static QIcon makeSectionCutIcon()
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Bâtiment filaire
    p.setPen(QPen(QColor(90, 105, 125), 1.2, Qt::DashLine));
    p.drawRect(4, 5, 18, 18);
    p.drawLine(4, 14, 22, 14);

    // Plan de coupe jaune vif
    QPolygon cutPoly;
    cutPoly << QPoint(11, 2) << QPoint(25, 6) << QPoint(15, 25) << QPoint(1, 21);
    p.setPen(QPen(QColor(220, 160, 10), 2.0));
    p.setBrush(QColor(255, 225, 40, 150));
    p.drawPolygon(cutPoly);

    return QIcon(pix);
}

static QIcon makeRotateIcon()
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(30, 140, 240), 2.2));
    p.drawArc(4, 4, 18, 18, 45 * 16, 270 * 16);

    // Flèche de rotation
    p.setBrush(QColor(30, 140, 240));
    QPolygon arrow;
    arrow << QPoint(18, 5) << QPoint(23, 8) << QPoint(19, 12);
    p.drawPolygon(arrow);

    // Point central
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(240, 60, 60));
    p.drawEllipse(11, 11, 4, 4);

    return QIcon(pix);
}

static QIcon makeOriginMoveIcon()
{
    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Triad axes
    p.setPen(QPen(Qt::red, 2.0));
    p.drawLine(13, 13, 23, 13);
    p.setPen(QPen(Qt::green, 2.0));
    p.drawLine(13, 13, 13, 3);
    p.setPen(QPen(Qt::blue, 2.0));
    p.drawLine(13, 13, 6, 20);

    // Center target ring
    p.setPen(QPen(QColor(255, 140, 0), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(8, 8, 10, 10);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 140, 0));
    p.drawEllipse(11, 11, 4, 4);

    return QIcon(pix);
}

static QIcon makeUndoIcon()
{
    QIcon icon(":/icons/undo.svg");
    if (!icon.isNull()) return icon;

    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(50, 70, 95), 2.5));
    p.drawArc(7, 8, 14, 14, 0, 180 * 16);

    QPolygon arrow;
    arrow << QPoint(4, 15) << QPoint(10, 10) << QPoint(10, 20);
    p.setBrush(QColor(50, 70, 95));
    p.setPen(Qt::NoPen);
    p.drawPolygon(arrow);

    return QIcon(pix);
}

static QIcon makeRedoIcon()
{
    QIcon icon(":/icons/redo.svg");
    if (!icon.isNull()) return icon;

    QPixmap pix(26, 26);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(50, 70, 95), 2.5));
    p.drawArc(5, 8, 14, 14, 0, 180 * 16);

    QPolygon arrow;
    arrow << QPoint(22, 15) << QPoint(16, 10) << QPoint(16, 20);
    p.setBrush(QColor(50, 70, 95));
    p.setPen(Qt::NoPen);
    p.drawPolygon(arrow);

    return QIcon(pix);
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_model(std::make_unique<TSA::Model::Model>())
    , m_selectionManager(std::make_unique<TSA::Viewer::SelectionManager>(this))
    , m_gridManager(std::make_unique<TSA::Grid::GridManager>())
    , m_gridSnapManager(std::make_unique<TSA::Grid::GridSnapManager>())
{
    // Grille 3D initiale : synchronisée avec le système de coordonnées et de niveaux unifié
    m_gridManager->clearAllGrids();

    TSA::Grid::GridDefinition def("Grille Bâtiment", TSA::Grid::GridType::Cartesian);
    def.setOrigin(0.0, 0.0, 0.0);
    if (m_model && m_model->coordinateSystem())
    {
        def.setXPositions(m_model->coordinateSystem()->xPositions());
        def.setYPositions(m_model->coordinateSystem()->yPositions());
        if (m_model->levelManager())
        {
            def.setZLevels(m_model->levelManager()->elevationList());
        }
    }
    auto* defaultGrid = m_gridManager->addGrid(def);
    if (defaultGrid)
    {
        m_gridManager->setActiveGridId(defaultGrid->id());
    }

    if (m_model && m_model->levelManager())
    {
        connect(m_model->levelManager(), &TSA::Coordinate::LevelManager::levelsChanged, this, [this]() {
            if (auto* grid = m_gridManager->activeGrid())
            {
                auto gdef = grid->definition();
                gdef.setZLevels(m_model->levelManager()->elevationList());
                grid->updateDefinition(gdef);
                m_occView->rebuildGrid();
            }
            if (m_viewportContainer)
            {
                m_viewportContainer->updateLevelsList(
                    m_model->levelManager()->elevationList(),
                    m_model->levelManager()->levelNames()
                );
            }
            m_modelTree->refreshLevels();
        });
    }

    setupUi();

    if (m_model && m_model->levelManager() && m_viewportContainer)
    {
        m_viewportContainer->updateLevelsList(
            m_model->levelManager()->elevationList(),
            m_model->levelManager()->levelNames()
        );
    }

    m_occView->setModel(m_model.get());
    m_occView->setGridManager(m_gridManager.get(), m_gridSnapManager.get());

    connect(m_gridManager.get(), &TSA::Grid::GridManager::gridAdded, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager.get(), &TSA::Grid::GridManager::gridRemoved, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager.get(), &TSA::Grid::GridManager::gridModified, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager.get(), &TSA::Grid::GridManager::activeGridChanged, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager.get(), &TSA::Grid::GridManager::gridVisibilityChanged, this, [this]() {
        m_occView->rebuildGrid();
    });

    m_modelTree->setGridManager(m_gridManager.get());
    m_modelTree->refreshAll();

    m_sectionCutDialog = new TSA::UI::SectionCutDialog(this);
    connect(m_sectionCutDialog, &TSA::UI::SectionCutDialog::clippingChanged, this, [this](bool enabled, int axis, double pos, bool flip) {
        if (m_occView)
        {
            m_occView->setClippingEnabled(enabled);
            m_occView->setClipPlane(axis, pos, flip);
        }
    });

    connect(m_occView, &OccView::pointToPointMoveRequested, this, &MainWindow::onPointToPointMoveRequested);
    connect(m_occView, &OccView::pointToPointRotateRequested, this, &MainWindow::onPointToPointRotateRequested);
    connect(m_occView, &OccView::originMoveRequested, this, &MainWindow::onOriginMoveRequested);
    connect(m_occView, &OccView::pasteAtPointRequested, this, &MainWindow::onPasteAtPointRequested);
    connect(m_occView, &OccView::elementCreated, this, [this]() {
        updateUndoRedoActions();
        if (m_statusInfo && m_model)
        {
            m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
                .arg(m_model->nodes().size())
                .arg(m_model->beams().size())
                .arg(m_model->columns().size())
                .arg(m_model->slabs().size()));
        }
    });

    updateUndoRedoActions();

    QSettings settings("TSAEngineering", "TSA");
    bool isDark = settings.value("Theme/DarkMode", false).toBool();
    if (m_actionDarkMode)
    {
        m_actionDarkMode->setChecked(isDark);
    }
    if (isDark)
    {
        onToggleDarkMode(true);
    }

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
            .arg(m_model->nodes().size())
            .arg(m_model->beams().size())
            .arg(m_model->columns().size())
            .arg(m_model->slabs().size()));
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("TSA - 3D Structural Modeler"));
    QIcon appIcon;
    appIcon.addFile(":/icons/TSA.ico");
    appIcon.addFile(":/icons/TSA.svg");
    setWindowIcon(appIcon);
    resize(1440, 880);

    setDockNestingEnabled(true);

    // Widget central : Viewport OpenCASCADE entouré des règles graduées (style Robot)
    m_occView = new OccView(this);
    m_occView->setSelectionManager(m_selectionManager.get());
    m_viewportContainer = new TSA::UI::ViewportContainer(m_occView, this);
    setCentralWidget(m_viewportContainer);

    createMenus();
    createToolBars();
    createDockWindows();
    createStatusBar();
}

void MainWindow::createMenus()
{
    // Menu Fichier
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    // Menu Edition
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    m_actionUndo = editMenu->addAction(tr("&Annuler"), this, &MainWindow::onActionUndo);
    m_actionUndo->setIcon(makeUndoIcon());
    m_actionUndo->setToolTip(tr("Annuler la dernière action (Ctrl+Z)"));
    m_actionUndo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    m_actionUndo->setEnabled(false);

    m_actionRedo = editMenu->addAction(tr("&Rétablir"), this, &MainWindow::onActionRedo);
    m_actionRedo->setIcon(makeRedoIcon());
    m_actionRedo->setToolTip(tr("Rétablir la dernière action annulée (Ctrl+Y)"));
    m_actionRedo->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) });
    m_actionRedo->setEnabled(false);

    editMenu->addSeparator();

    m_actionCopyClipboard = editMenu->addAction(tr("&Copier (Presse-papier)"), this, &MainWindow::onActionCopyClipboard);
    m_actionCopyClipboard->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopyClipboard->setToolTip(tr("Copier la sélection dans le presse-papier structural (Ctrl+C)"));
    m_actionCopyClipboard->setShortcut(QKeySequence::Copy);

    m_actionPasteClipboard = editMenu->addAction(tr("C&oller en 3D"), this, &MainWindow::onActionPasteClipboard);
    m_actionPasteClipboard->setIcon(QIcon(":/icons/copy.svg"));
    m_actionPasteClipboard->setToolTip(tr("Coller les éléments copiés dans la vue 3D au clic souris (Ctrl+V)"));
    m_actionPasteClipboard->setShortcut(QKeySequence::Paste);

    editMenu->addSeparator();

    m_actionMove3D = editMenu->addAction(tr("&Déplacement 3D (Point à Point)..."), this, &MainWindow::onActionMove3D);
    m_actionMove3D->setIcon(QIcon(":/icons/move.svg"));
    m_actionMove3D->setToolTip(tr("Déplacer interactivement les éléments dans la vue 3D (M)"));
    m_actionMove3D->setShortcut(QKeySequence(Qt::Key_M));
    m_actionMove3D->setCheckable(true);

    m_actionCopy3D = editMenu->addAction(tr("C&opie 3D (Translation)..."), this, &MainWindow::onActionCopy3D);
    m_actionCopy3D->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopy3D->setToolTip(tr("Copier interactivement les éléments par translation en 3D"));
    m_actionCopy3D->setCheckable(true);

    m_actionRotate3D = editMenu->addAction(tr("&Rotation 3D..."), this, &MainWindow::onActionRotate3D);
    m_actionRotate3D->setIcon(makeRotateIcon());
    m_actionRotate3D->setToolTip(tr("Faire tourner les éléments sélectionnés autour d'un axe 3D (Ctrl+R)"));
    m_actionRotate3D->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actionRotate3D->setCheckable(true);

    m_actionMoveOrigin = editMenu->addAction(tr("Déplacer l'&Origine 3D..."), this, &MainWindow::onActionMoveOrigin);
    m_actionMoveOrigin->setIcon(makeOriginMoveIcon());
    m_actionMoveOrigin->setToolTip(tr("Positionner le repère global / la grille 3D par clic ou snap"));
    m_actionMoveOrigin->setCheckable(true);

    editMenu->addSeparator();

    m_actionMove = editMenu->addAction(tr("Déplacement paramétrique (Dialogue)..."), this, &MainWindow::onActionMove);
    m_actionMove->setIcon(QIcon(":/icons/move.svg"));
    m_actionMove->setToolTip(tr("Déplacer par coordonnées dx, dy, dz"));

    m_actionCopy = editMenu->addAction(tr("Copie paramétrique / Répétition..."), this, &MainWindow::onActionCopy);
    m_actionCopy->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopy->setToolTip(tr("Copier et répéter par coordonnées dx, dy, dz (Ctrl+D)..."));
    m_actionCopy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    editMenu->addSeparator();

    m_actionDelete = editMenu->addAction(tr("&Delete Selected"), this, &MainWindow::onActionDeleteSelected);
    m_actionDelete->setIcon(QIcon(":/icons/delete.svg"));
    m_actionDelete->setToolTip(tr("Delete Selected Elements (Del)"));
    m_actionDelete->setShortcut(QKeySequence::Delete);

    // Menu Vue
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    // Projections en plan (Robot SA style)
    m_actionViewXY = viewMenu->addAction(tr("Plan &XY (Vue d'étage)"), this, &MainWindow::onActionViewXY);
    m_actionViewXY->setIcon(makePlanIcon(QColor(255, 140, 140), Qt::blue, Qt::darkGreen, "X", "Y"));
    m_actionViewXY->setToolTip(tr("Vue en Plan XY (Étage actif)"));

    m_actionViewYZ = viewMenu->addAction(tr("Plan &YZ (Coupe latérale / Pignon)"), this, &MainWindow::onActionViewYZ);
    m_actionViewYZ->setIcon(makePlanIcon(QColor(140, 160, 255), Qt::darkGreen, Qt::red, "Y", "Z"));
    m_actionViewYZ->setToolTip(tr("Vue en Plan YZ (Coupe latérale / Pignon)"));

    m_actionViewXZ = viewMenu->addAction(tr("Plan &XZ (Élévation de face / Portique)"), this, &MainWindow::onActionViewXZ);
    m_actionViewXZ->setIcon(makePlanIcon(QColor(140, 230, 160), Qt::blue, Qt::red, "X", "Z"));
    m_actionViewXZ->setToolTip(tr("Vue en Plan XZ (Élévation de face / Portique)"));

    m_actionView3D = viewMenu->addAction(tr("Vue &3D (Axonométrique)"), this, &MainWindow::onActionView3D);
    m_actionView3D->setIcon(make3DIsoIcon());
    m_actionView3D->setToolTip(tr("Vue 3D Isométrique"));

    viewMenu->addSeparator();

    m_actionCoordSystem = viewMenu->addAction(tr("Repère &Local / Global"), this, &MainWindow::onActionCoordSystem);
    m_actionCoordSystem->setIcon(makeCoordSystemIcon());
    m_actionCoordSystem->setCheckable(true);
    m_actionCoordSystem->setToolTip(tr("Basculer entre Repère Global (GCS) et Repère Local (LCS)"));

    m_actionSectionCut = viewMenu->addAction(tr("&Coupes de la structure (Section 3D)..."), this, &MainWindow::onActionSectionCut);
    m_actionSectionCut->setIcon(makeSectionCutIcon());
    m_actionSectionCut->setToolTip(tr("Définir et activer des plans de coupe 3D (Graphic3d_ClipPlane)"));

    viewMenu->addSeparator();

    m_actionFitAll = viewMenu->addAction(tr("&Fit All"), this, &MainWindow::onFitAll);
    m_actionFitAll->setIcon(QIcon(":/icons/fit_all.svg"));
    m_actionFitAll->setToolTip(tr("Fit All (F)"));
    m_actionFitAll->setShortcut(QKeySequence(Qt::Key_F));

    m_actionResetView = viewMenu->addAction(tr("&Reset View (Isometric)"), this, &MainWindow::onResetView);
    m_actionResetView->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionResetView->setToolTip(tr("Reset View - Isometric (R)"));
    m_actionResetView->setShortcut(QKeySequence(Qt::Key_R));

    viewMenu->addSeparator();

    m_actionDarkMode = viewMenu->addAction(tr("Mode &Sombre (Dark Theme)"), this, &MainWindow::onToggleDarkMode);
    m_actionDarkMode->setIcon(QIcon(":/icons/theme_dark.svg"));
    m_actionDarkMode->setToolTip(tr("Basculer entre le mode sombre et le mode clair (Ctrl+D)"));
    m_actionDarkMode->setCheckable(true);
    m_actionDarkMode->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    m_actionFullScreen = viewMenu->addAction(tr("Mode &Plein écran"), this, &MainWindow::onToggleFullScreen);
    m_actionFullScreen->setIcon(QIcon(":/icons/fullscreen.svg"));
    m_actionFullScreen->setToolTip(tr("Basculer en mode plein écran (F11)"));
    m_actionFullScreen->setCheckable(true);
    m_actionFullScreen->setShortcut(QKeySequence(Qt::Key_F11));

    viewMenu->addSeparator();

    // Menu Grille 3D & Niveaux
    QMenu* gridMenu = menuBar()->addMenu(tr("&Grids && Levels"));

    m_actionNewGrid = gridMenu->addAction(tr("&New Grid..."), this, &MainWindow::onNewGrid);
    m_actionNewGrid->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_actionNewGrid->setToolTip(tr("Define a new parametric 3D grid (Cartesian or Cylindrical)..."));

    m_actionGridManager = gridMenu->addAction(tr("&Grid Manager..."), this, &MainWindow::onGridManagerDialog);
    m_actionGridManager->setIcon(QIcon(":/icons/settings.svg"));
    m_actionGridManager->setToolTip(tr("Manage grids, active workplane, visibility and snapping tolerance..."));

    m_actionManageLevels = gridMenu->addAction(tr("Manage &Levels / Stories..."), this, &MainWindow::onManageLevels);
    m_actionManageLevels->setIcon(QIcon(":/icons/settings.svg"));
    m_actionManageLevels->setToolTip(tr("Manage project stories, elevations, and vertical connections (Ctrl+L)..."));
    m_actionManageLevels->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    gridMenu->addSeparator();

    m_actionGridVisible = gridMenu->addAction(tr("&Show 3D Grid"), this, &MainWindow::onToggleGridVisible);
    m_actionGridVisible->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_actionGridVisible->setToolTip(tr("Toggle 3D Grid Visibility (G)"));
    m_actionGridVisible->setCheckable(true);
    m_actionGridVisible->setChecked(true);
    m_actionGridVisible->setShortcut(QKeySequence(Qt::Key_G));

    m_actionLevelsVisible = gridMenu->addAction(tr("Show Level &Planes && Markers"), this, &MainWindow::onToggleLevelsVisible);
    m_actionLevelsVisible->setToolTip(tr("Toggle 3D story planes and vertical level datum markers"));
    m_actionLevelsVisible->setCheckable(true);
    m_actionLevelsVisible->setChecked(true);

    m_actionGridSnap = gridMenu->addAction(tr("&Snap Cursor to Grid"), this, &MainWindow::onToggleGridSnap);
    m_actionGridSnap->setIcon(QIcon(":/icons/snap.svg"));
    m_actionGridSnap->setToolTip(tr("Magnetic Snap to Grid intersections, axes and model nodes (S)"));
    m_actionGridSnap->setCheckable(true);
    m_actionGridSnap->setChecked(true);
    m_actionGridSnap->setShortcut(QKeySequence(Qt::Key_S));

    m_actionGridLabels = gridMenu->addAction(tr("Show Axis &Labels / Bubbles"), this, &MainWindow::onToggleGridLabels);
    m_actionGridLabels->setToolTip(tr("Show or hide axis bubbles and label texts in 3D"));
    m_actionGridLabels->setCheckable(true);
    m_actionGridLabels->setChecked(true);

    m_actionRulersVisible = gridMenu->addAction(tr("Show Viewport &Rulers"), this, &MainWindow::onToggleRulersVisible);
    m_actionRulersVisible->setToolTip(tr("Afficher ou masquer les règles de projection graduées sur les bords du viewport"));
    m_actionRulersVisible->setCheckable(true);
    m_actionRulersVisible->setChecked(true);

    // Modes d'interaction / Dessin 3D
    m_drawModeGroup = new QActionGroup(this);

    m_actionSelectMode = new QAction(tr("&Select Mode"), this);
    m_actionSelectMode->setIcon(QIcon(":/icons/select.svg"));
    m_actionSelectMode->setToolTip(tr("Select Mode - Box/Crossing selection (Esc)"));
    m_actionSelectMode->setCheckable(true);
    m_actionSelectMode->setChecked(true);
    m_actionSelectMode->setShortcut(QKeySequence(Qt::Key_Escape));
    m_actionSelectMode->setStatusTip(tr("Select and inspect structural elements (Esc)"));
    connect(m_actionSelectMode, &QAction::triggered, this, &MainWindow::onModeSelect);
    m_drawModeGroup->addAction(m_actionSelectMode);

    m_actionDrawNode = new QAction(tr("Draw &Node"), this);
    m_actionDrawNode->setIcon(QIcon(":/icons/draw_node.svg"));
    m_actionDrawNode->setToolTip(tr("Draw Node in 3D (N)"));
    m_actionDrawNode->setCheckable(true);
    m_actionDrawNode->setShortcut(QKeySequence(Qt::Key_N));
    m_actionDrawNode->setStatusTip(tr("Click anywhere in 3D or on grid to create a Node (N)"));
    connect(m_actionDrawNode, &QAction::triggered, this, &MainWindow::onModeDrawNode);
    m_drawModeGroup->addAction(m_actionDrawNode);

    m_actionDrawBeam = new QAction(tr("Draw &Beam"), this);
    m_actionDrawBeam->setIcon(QIcon(":/icons/draw_beam.svg"));
    m_actionDrawBeam->setToolTip(tr("Draw Beam (B)"));
    m_actionDrawBeam->setCheckable(true);
    m_actionDrawBeam->setShortcut(QKeySequence(Qt::Key_B));
    m_actionDrawBeam->setStatusTip(tr("Click two points or nodes to create a Beam (B)"));
    connect(m_actionDrawBeam, &QAction::triggered, this, &MainWindow::onModeDrawBeam);
    m_drawModeGroup->addAction(m_actionDrawBeam);

    m_actionDrawColumn = new QAction(tr("Draw &Column"), this);
    m_actionDrawColumn->setIcon(QIcon(":/icons/draw_column.svg"));
    m_actionDrawColumn->setToolTip(tr("Draw Column (C)"));
    m_actionDrawColumn->setCheckable(true);
    m_actionDrawColumn->setShortcut(QKeySequence(Qt::Key_C));
    m_actionDrawColumn->setStatusTip(tr("Click base and top point to create a Column (C)"));
    connect(m_actionDrawColumn, &QAction::triggered, this, &MainWindow::onModeDrawColumn);
    m_drawModeGroup->addAction(m_actionDrawColumn);

    m_actionDrawSlab = new QAction(tr("Draw &Slab"), this);
    m_actionDrawSlab->setIcon(QIcon(":/icons/draw_slab.svg"));
    m_actionDrawSlab->setToolTip(tr("Draw Slab (L)"));
    m_actionDrawSlab->setCheckable(true);
    m_actionDrawSlab->setShortcut(QKeySequence(Qt::Key_L));
    m_actionDrawSlab->setStatusTip(tr("Click polygon nodes to create a Slab (L)"));
    connect(m_actionDrawSlab, &QAction::triggered, this, &MainWindow::onModeDrawSlab);
    m_drawModeGroup->addAction(m_actionDrawSlab);

    m_drawModeGroup->addAction(m_actionMove3D);
    m_drawModeGroup->addAction(m_actionCopy3D);
    m_drawModeGroup->addAction(m_actionRotate3D);
    m_drawModeGroup->addAction(m_actionMoveOrigin);

    // Menu Modèle (Création d'éléments structuraux)
    QMenu* modelMenu = menuBar()->addMenu(tr("&Model"));

    QMenu* drawMenu = modelMenu->addMenu(tr("3D &Drawing Modes"));
    drawMenu->addAction(m_actionSelectMode);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actionDrawNode);
    drawMenu->addAction(m_actionDrawBeam);
    drawMenu->addAction(m_actionDrawColumn);
    drawMenu->addAction(m_actionDrawSlab);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actionMove3D);
    drawMenu->addAction(m_actionCopy3D);
    drawMenu->addAction(m_actionRotate3D);
    drawMenu->addAction(m_actionMoveOrigin);

    modelMenu->addSeparator();
    m_actionNewNode = modelMenu->addAction(tr("New &Node (Dialog)..."), this, &MainWindow::onActionNewNode);
    m_actionNewNode->setIcon(QIcon(":/icons/node_add.svg"));
    m_actionNewNode->setToolTip(tr("New Node (Dialog)..."));

    m_actionNewBeam = modelMenu->addAction(tr("New &Beam (Dialog)..."), this, &MainWindow::onActionNewBeam);
    m_actionNewBeam->setIcon(QIcon(":/icons/beam_add.svg"));
    m_actionNewBeam->setToolTip(tr("New Beam (Dialog)..."));

    m_actionNewColumn = modelMenu->addAction(tr("New &Column (Dialog)..."), this, &MainWindow::onActionNewColumn);
    m_actionNewColumn->setIcon(QIcon(":/icons/column_add.svg"));
    m_actionNewColumn->setToolTip(tr("New Column (Dialog)..."));

    m_actionNewSlab = modelMenu->addAction(tr("New &Slab (Dialog)..."), this, &MainWindow::onActionNewSlab);
    m_actionNewSlab->setIcon(QIcon(":/icons/slab_add.svg"));
    m_actionNewSlab->setToolTip(tr("New Slab (Dialog)..."));

    modelMenu->addSeparator();
    m_actionAddCube = modelMenu->addAction(tr("Ajouter un &Cube Structurel (3D)..."), this, &MainWindow::onActionAddCube);
    m_actionAddCube->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionAddCube->setToolTip(tr("Générer un cube structurel 3D (8 nœuds, 4 poteaux, 8 poutres, 1 dalle) entre les étages actifs"));

    menuBar()->addMenu(tr("&Tools"));
    menuBar()->addMenu(tr("&Help"));
}

void MainWindow::createToolBars()
{
    // Barre d'outils Dessin 3D Interactif
    QToolBar* drawToolBar = addToolBar(tr("3D Drawing"));
    drawToolBar->setObjectName("DrawToolBar");
    drawToolBar->setIconSize(QSize(22, 22));
    drawToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    drawToolBar->addAction(m_actionSelectMode);
    drawToolBar->addSeparator();
    drawToolBar->addAction(m_actionDrawNode);
    drawToolBar->addAction(m_actionDrawBeam);
    drawToolBar->addAction(m_actionDrawColumn);
    drawToolBar->addAction(m_actionDrawSlab);
    drawToolBar->addSeparator();
    drawToolBar->addAction(m_actionMove3D);
    drawToolBar->addAction(m_actionCopy3D);
    drawToolBar->addAction(m_actionRotate3D);
    drawToolBar->addAction(m_actionMoveOrigin);

    // Barre d'outils Modélisation
    QToolBar* modelToolBar = addToolBar(tr("Model"));
    modelToolBar->setObjectName("ModelToolBar");
    modelToolBar->setIconSize(QSize(22, 22));
    modelToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    modelToolBar->addAction(m_actionUndo);
    modelToolBar->addAction(m_actionRedo);
    modelToolBar->addSeparator();

    modelToolBar->addAction(m_actionNewNode);
    modelToolBar->addAction(m_actionNewBeam);
    modelToolBar->addAction(m_actionNewColumn);
    modelToolBar->addAction(m_actionNewSlab);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionAddCube);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionCopyClipboard);
    modelToolBar->addAction(m_actionPasteClipboard);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionMove);
    modelToolBar->addAction(m_actionCopy);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionDelete);

    // Barre d'outils Vue (Conforme à Robot Structural Analysis)
    QToolBar* viewToolBar = addToolBar(tr("Vue"));
    viewToolBar->setObjectName("ViewToolBar");
    viewToolBar->setIconSize(QSize(24, 24));
    viewToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    viewToolBar->addAction(m_actionViewXY);
    viewToolBar->addAction(m_actionViewYZ);
    viewToolBar->addAction(m_actionViewXZ);
    viewToolBar->addAction(m_actionView3D);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_actionCoordSystem);
    viewToolBar->addAction(m_actionMoveOrigin);
    viewToolBar->addAction(m_actionSectionCut);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_actionFitAll);
    viewToolBar->addAction(m_actionResetView);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_actionGridVisible);
    viewToolBar->addAction(m_actionGridSnap);
    viewToolBar->addAction(m_actionNewGrid);
    viewToolBar->addAction(m_actionGridManager);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_actionDarkMode);
    viewToolBar->addAction(m_actionFullScreen);
}

void MainWindow::createDockWindows()
{
    // Dock gauche : MODEL TREE
    m_modelTreeDock = new QDockWidget(tr("MODEL TREE"), this);
    m_modelTreeDock->setObjectName("ModelTreeDock");
    m_modelTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_modelTree = new TSA::UI::ModelTreeWidget(m_model.get(), m_modelTreeDock);
    m_modelTreeDock->setWidget(m_modelTree);
    m_modelTreeDock->setMinimumWidth(280);
    addDockWidget(Qt::LeftDockWidgetArea, m_modelTreeDock);

    // Dock droit : PROPERTIES
    m_propertiesDock = new QDockWidget(tr("PROPERTIES"), this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_propertyPanel = new TSA::UI::PropertyPanel(m_model.get(), m_propertiesDock);
    m_propertiesDock->setWidget(m_propertyPanel);
    m_propertiesDock->setMinimumWidth(280);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 1. Sélection depuis le MODEL TREE
    connect(m_modelTree, &TSA::UI::ModelTreeWidget::levelSelected, this, [this](const QString& levelId) {
        m_selectionManager->clearSelection();
        m_occView->clearHighlight();
        m_propertyPanel->showLevelProperties(levelId);
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

    connect(m_modelTree, &TSA::UI::ModelTreeWidget::selectionCleared, this, [this]() {
        m_selectionManager->clearSelection();
        m_occView->clearHighlight();
        m_propertyPanel->clearProperties();
    });

    // 2. Sélection depuis le VIEWPORT 3D (clic souris)
    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::nodeSelected, this, [this](int nodeId) {
        m_modelTree->selectNodeItem(nodeId);
        m_occView->highlightNode(nodeId);
        m_propertyPanel->showNodeProperties(nodeId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Node %1").arg(nodeId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::beamSelected, this, [this](int beamId) {
        m_modelTree->selectBeamItem(beamId);
        m_occView->highlightBeam(beamId);
        m_propertyPanel->showBeamProperties(beamId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Beam %1").arg(beamId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::columnSelected, this, [this](int columnId) {
        m_modelTree->selectColumnItem(columnId);
        m_occView->highlightColumn(columnId);
        m_propertyPanel->showColumnProperties(columnId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Column %1").arg(columnId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::slabSelected, this, [this](int slabId) {
        m_modelTree->selectSlabItem(slabId);
        m_occView->highlightSlab(slabId);
        m_propertyPanel->showSlabProperties(slabId);
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Selected Slab %1").arg(slabId));
        }
    });

    connect(m_selectionManager.get(), &TSA::Viewer::SelectionManager::selectionCleared, this, [this]() {
        m_modelTree->clearTreeSelection();
        m_occView->clearHighlight();
        m_propertyPanel->clearProperties();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Ready"));
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
}

void MainWindow::createStatusBar()
{
    QStatusBar* bar = statusBar();

    m_statusCoordinates = new QLabel(tr("X: 0.000 m   Y: 0.000 m   Z: 0.000 m"), this);
    m_statusCoordinates->setMinimumWidth(260);
    m_statusCoordinates->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; padding: 2px 8px;");
    bar->addWidget(m_statusCoordinates);

    m_statusInfo = new QLabel(tr("Ready"), this);
    bar->addPermanentWidget(m_statusInfo);

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

    connect(m_occView, &OccView::interactionModeChanged, this, [this](OccView::InteractionMode mode) {
        switch (mode)
        {
        case OccView::InteractionMode::Select:
            if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
            break;
        case OccView::InteractionMode::DrawNode:
            if (m_actionDrawNode) m_actionDrawNode->setChecked(true);
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
        case OccView::InteractionMode::Paste3D:
            break;
        }
    });

    connect(m_occView, &OccView::drawingPromptChanged, this, [this](const QString& prompt) {
        if (m_statusInfo)
        {
            m_statusInfo->setText(prompt);
        }
    });
}

void MainWindow::onModeSelect()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }
}

void MainWindow::onModeDrawNode()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawNode);
    }
}

void MainWindow::onModeDrawBeam()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawBeam);
    }
}

void MainWindow::onModeDrawColumn()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawColumn);
    }
}

void MainWindow::onModeDrawSlab()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawSlab);
    }
}

void MainWindow::onFitAll()
{
    if (m_occView)
    {
        m_occView->fitAll();
    }
}

void MainWindow::onResetView()
{
    if (m_occView)
    {
        m_occView->resetView();
    }
}

void MainWindow::onNewGrid()
{
    TSA::UI::GridDialog dlg(m_gridManager.get(), m_model.get(), m_occView, this);
    connect(&dlg, &TSA::UI::GridDialog::gridDefinitionApplied, this, [this](const TSA::Grid::GridDefinition& /*def*/) {
        m_occView->rebuildGrid();
        m_modelTree->refreshGrids();
        m_modelTree->refreshLevels();
        if (m_viewportContainer)
        {
            m_viewportContainer->updateRulers();
        }
    });
    connect(&dlg, &TSA::UI::GridDialog::manageGridsRequested, this, &MainWindow::onGridManagerDialog);

    dlg.exec();

    m_occView->rebuildGrid();
    m_modelTree->refreshGrids();
    m_modelTree->refreshLevels();
    if (m_viewportContainer)
    {
        m_viewportContainer->updateRulers();
    }
}

void MainWindow::onGridManagerDialog()
{
    if (m_gridManager && m_gridSnapManager && m_occView)
    {
        TSA::UI::GridSettingsDialog dlg(m_gridManager.get(), m_gridSnapManager.get(), m_occView, this);
        dlg.exec();
    }
}

void MainWindow::onManageLevels()
{
    if (!m_model || !m_model->levelManager())
        return;

    TSA::UI::LevelDialog dlg(m_model->levelManager(), this);
    dlg.exec();
}

void MainWindow::onToggleLevelsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setGridLevelsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Plans d'étages et repères de niveaux affichés") : tr("Plans d'étages masqués"));
        }
    }
}

void MainWindow::onToggleRulersVisible(bool checked)
{
    if (m_viewportContainer)
    {
        m_viewportContainer->setRulersVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Règles de bordure affichées") : tr("Règles de bordure masquées"));
        }
    }
}

void MainWindow::onToggleDarkMode(bool checked)
{
    TSA::UI::ThemeManager::setDarkMode(checked);

    if (checked)
    {
        qApp->setStyleSheet(TSA::UI::ThemeManager::darkStyleSheet());
        if (statusBar()) statusBar()->showMessage(tr("Mode sombre activé"), 2000);
    }
    else
    {
        qApp->setStyleSheet(TSA::UI::ThemeManager::lightStyleSheet());
        if (statusBar()) statusBar()->showMessage(tr("Mode clair activé"), 2000);
    }

    if (m_actionDarkMode && m_actionDarkMode->isChecked() != checked)
    {
        m_actionDarkMode->setChecked(checked);
    }

    if (m_viewportContainer)
    {
        m_viewportContainer->setDarkMode(checked);
    }
}

void MainWindow::onToggleFullScreen(bool checked)
{
    if (checked)
    {
        if (!isFullScreen())
        {
            m_wasMaximizedBeforeFullScreen = isMaximized();
            showFullScreen();
            if (statusBar())
                statusBar()->showMessage(tr("Mode plein écran activé (F11 pour quitter)"), 3000);
        }
    }
    else
    {
        if (isFullScreen())
        {
            if (m_wasMaximizedBeforeFullScreen)
                showMaximized();
            else
                showNormal();
            if (statusBar())
                statusBar()->showMessage(tr("Mode fenêtre rétabli"), 2000);
        }
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
    {
        bool full = isFullScreen();
        if (m_actionFullScreen && m_actionFullScreen->isChecked() != full)
        {
            m_actionFullScreen->blockSignals(true);
            m_actionFullScreen->setChecked(full);
            m_actionFullScreen->blockSignals(false);
        }
        if (m_actionFullScreen)
        {
            if (full)
            {
                m_actionFullScreen->setText(tr("&Quitter le plein écran"));
                m_actionFullScreen->setToolTip(tr("Quitter le mode plein écran (F11)"));
            }
            else
            {
                m_actionFullScreen->setText(tr("Mode &Plein écran"));
                m_actionFullScreen->setToolTip(tr("Basculer en mode plein écran (F11)"));
            }
        }
    }
}

void MainWindow::onToggleGridVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setGridVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Grille 3D affichée") : tr("Grille 3D masquée"));
        }
    }
}

void MainWindow::onToggleGridSnap(bool checked)
{
    if (m_occView)
    {
        m_occView->setGridSnapEnabled(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Accrochage magnétique à la grille activé") : tr("Accrochage désactivé"));
        }
    }
}

void MainWindow::onToggleGridLabels(bool checked)
{
    if (m_occView)
    {
        m_occView->setGridLabelsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Bulles et étiquettes d'axes affichées") : tr("Bulles d'axes masquées"));
        }
    }
}

void MainWindow::onActionNewNode()
{
    bool ok = false;
    double x = QInputDialog::getDouble(this, tr("New Node"), tr("Coordinate X (m):"), 0.0, -10000.0, 10000.0, 3, &ok);
    if (!ok) return;

    double y = QInputDialog::getDouble(this, tr("New Node"), tr("Coordinate Y (m):"), 0.0, -10000.0, 10000.0, 3, &ok);
    if (!ok) return;

    double z = QInputDialog::getDouble(this, tr("New Node"), tr("Coordinate Z (m):"), 0.0, -10000.0, 10000.0, 3, &ok);
    if (!ok) return;

    m_model->pushUndoState(tr("Nouveau Nœud").toStdString());
    int newId = m_model->addNode(x, y, z);
    updateUndoRedoActions();
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Created Node %1 (%2, %3, %4)")
            .arg(newId).arg(x).arg(y).arg(z));
    }
}

void MainWindow::onActionNewBeam()
{
    if (m_model->nodes().size() < 2)
    {
        QMessageBox::warning(this, tr("New Beam"), tr("At least two nodes are required to create a beam."));
        return;
    }

    bool ok = false;
    int startNode = QInputDialog::getInt(this, tr("New Beam"), tr("Start Node ID:"), 1, 1, 100000, 1, &ok);
    if (!ok) return;

    int endNode = QInputDialog::getInt(this, tr("New Beam"), tr("End Node ID:"), 2, 1, 100000, 1, &ok);
    if (!ok) return;

    if (!m_model->getNode(startNode) || !m_model->getNode(endNode))
    {
        QMessageBox::warning(this, tr("New Beam"), tr("One or both specified nodes do not exist."));
        return;
    }

    m_model->pushUndoState(tr("Nouvelle Poutre").toStdString());
    int beamId = m_model->addBeam(startNode, endNode, 0.30, 0.50);
    updateUndoRedoActions();
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Created Beam %1 (Nodes %2 -> %3)").arg(beamId).arg(startNode).arg(endNode));
    }
}

void MainWindow::onActionNewColumn()
{
    if (m_model->nodes().size() < 2)
    {
        QMessageBox::warning(this, tr("New Column"), tr("At least two nodes are required to create a column."));
        return;
    }

    bool ok = false;
    int startNode = QInputDialog::getInt(this, tr("New Column"), tr("Bottom Node ID:"), 1, 1, 100000, 1, &ok);
    if (!ok) return;

    int endNode = QInputDialog::getInt(this, tr("New Column"), tr("Top Node ID:"), 2, 1, 100000, 1, &ok);
    if (!ok) return;

    if (!m_model->getNode(startNode) || !m_model->getNode(endNode))
    {
        QMessageBox::warning(this, tr("New Column"), tr("One or both specified nodes do not exist."));
        return;
    }

    m_model->pushUndoState(tr("Nouveau Poteau").toStdString());
    int colId = m_model->addColumn(startNode, endNode, 0.35, 0.35);
    updateUndoRedoActions();
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Created Column %1 (Nodes %2 -> %3)").arg(colId).arg(startNode).arg(endNode));
    }
}

void MainWindow::onActionNewSlab()
{
    if (m_model->nodes().size() < 3)
    {
        QMessageBox::warning(this, tr("New Slab"), tr("At least 3 nodes are required to create a slab polygon."));
        return;
    }

    bool ok = false;
    QString text = QInputDialog::getText(this, tr("New Slab"),
        tr("Enter contour Node IDs (separated by commas or spaces, e.g. 2, 4, 8, 6):"),
        QLineEdit::Normal, "2, 4, 8, 6", &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    std::vector<int> nodeIds;
    std::string s = text.toStdString();
    for (char& c : s)
    {
        if (c == ',' || c == ';') c = ' ';
    }
    std::istringstream iss(s);
    int nid = 0;
    while (iss >> nid)
    {
        if (!m_model->getNode(nid))
        {
            QMessageBox::warning(this, tr("New Slab"), tr("Node %1 does not exist.").arg(nid));
            return;
        }
        nodeIds.push_back(nid);
    }

    if (nodeIds.size() < 3)
    {
        QMessageBox::warning(this, tr("New Slab"), tr("A slab must contain at least 3 nodes."));
        return;
    }

    double thickness = QInputDialog::getDouble(this, tr("New Slab"), tr("Thickness (m):"), 0.20, 0.01, 5.0, 2, &ok);
    if (!ok) return;

    m_model->pushUndoState(tr("Nouvelle Dalle").toStdString());
    int slabId = m_model->addSlab(nodeIds, thickness);
    updateUndoRedoActions();
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Created Slab %1 (%2 nodes, e = %3 m)").arg(slabId).arg(nodeIds.size()).arg(thickness));
    }
}

void MainWindow::onActionMove()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Move"), tr("Please select at least one element to move."));
        return;
    }

    TSA::UI::TransformDialog dlg(TSA::UI::TransformMode::Move, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    double dx = dlg.deltaX();
    double dy = dlg.deltaY();
    double dz = dlg.deltaZ();

    std::set<int> nodesToMove = m_selectionManager->selectedNodes();
    for (int bId : m_selectionManager->selectedBeams())
    {
        const auto* b = m_model->getBeam(bId);
        if (b) { nodesToMove.insert(b->startNodeId()); nodesToMove.insert(b->endNodeId()); }
    }
    for (int cId : m_selectionManager->selectedColumns())
    {
        const auto* c = m_model->getColumn(cId);
        if (c) { nodesToMove.insert(c->startNodeId()); nodesToMove.insert(c->endNodeId()); }
    }
    for (int sId : m_selectionManager->selectedSlabs())
    {
        const auto* s = m_model->getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) nodesToMove.insert(nid);
        }
    }

    if (m_model)
    {
        m_model->pushUndoState(tr("Déplacement paramétrique").toStdString());
    }

    if (m_model->moveNodes(nodesToMove, dx, dy, dz))
    {
        updateUndoRedoActions();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Moved %1 node(s) by (%2, %3, %4) m")
                .arg(nodesToMove.size()).arg(dx).arg(dy).arg(dz));
        }
    }
}

void MainWindow::onActionCopy()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Copy"), tr("Please select at least one element to copy."));
        return;
    }

    TSA::UI::TransformDialog dlg(TSA::UI::TransformMode::Copy, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    double dx = dlg.deltaX();
    double dy = dlg.deltaY();
    double dz = dlg.deltaZ();
    int reps = dlg.repetitions();

    if (m_model)
    {
        m_model->pushUndoState(tr("Copie paramétrique").toStdString());
    }

    auto newIds = m_model->copyElements(
        m_selectionManager->selectedNodes(),
        m_selectionManager->selectedBeams(),
        m_selectionManager->selectedColumns(),
        m_selectionManager->selectedSlabs(),
        dx, dy, dz, reps
    );

    updateUndoRedoActions();

    if (!newIds.empty() && m_statusInfo)
    {
        m_statusInfo->setText(tr("Duplicated selection: %1 new element(s) created (%2 repetition(s))")
            .arg(newIds.size()).arg(reps));
    }
}

void MainWindow::onActionDeleteSelected()
{
    if (!m_selectionManager || !m_model)
        return;

    size_t total = m_selectionManager->totalSelectedCount();
    if (total == 0)
        return;

    m_model->pushUndoState(tr("Suppression").toStdString());

    // Supprimer dans l'ordre sécurisé : Dalles, Poutres, Poteaux, Nœuds
    auto slabs = m_selectionManager->selectedSlabs();
    for (int id : slabs)
    {
        m_model->removeSlab(id);
    }

    auto beams = m_selectionManager->selectedBeams();
    for (int id : beams)
    {
        m_model->removeBeam(id);
    }

    auto columns = m_selectionManager->selectedColumns();
    for (int id : columns)
    {
        m_model->removeColumn(id);
    }

    auto nodes = m_selectionManager->selectedNodes();
    for (int id : nodes)
    {
        m_model->removeNode(id);
    }

    m_selectionManager->clearSelection();
    if (m_occView) m_occView->clearHighlight();
    if (m_propertyPanel) m_propertyPanel->clearProperties();

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("%1 élément(s) supprimé(s)").arg(total));
    }
    updateUndoRedoActions();
}

void MainWindow::onActionAddCube()
{
    if (!m_model)
        return;

    m_model->pushUndoState(tr("Structure Cube 3D").toStdString());

    // Déterminer les dimensions et élévations du cube depuis le niveau actif et la grille
    double x0 = 0.0, x1 = 6.0;
    double y0 = 0.0, y1 = 4.0;
    double z0 = 0.0, z1 = 3.0;

    if (m_viewportContainer)
    {
        z0 = m_viewportContainer->activeLevelElevation();
    }

    if (m_model->coordinateSystem())
    {
        const auto& xPos = m_model->coordinateSystem()->xPositions();
        const auto& yPos = m_model->coordinateSystem()->yPositions();
        if (xPos.size() >= 2)
        {
            x0 = xPos[0];
            x1 = xPos[1];
        }
        if (yPos.size() >= 2)
        {
            y0 = yPos[0];
            y1 = yPos[1];
        }
    }

    if (m_model->levelManager())
    {
        const auto& levels = m_model->levelManager()->elevationList();
        bool foundNext = false;
        for (double lz : levels)
        {
            if (lz > z0 + 1e-4)
            {
                z1 = lz;
                foundNext = true;
                break;
            }
        }
        if (!foundNext)
        {
            z1 = z0 + 3.0;
        }
    }

    // 1. Création des 8 nœuds géométriques du cube
    // Nœuds de base au niveau z0
    int n1 = m_model->addNode(x0, y0, z0);
    int n2 = m_model->addNode(x1, y0, z0);
    int n3 = m_model->addNode(x1, y1, z0);
    int n4 = m_model->addNode(x0, y1, z0);

    // Nœuds de sommet au niveau supérieur z1
    int n5 = m_model->addNode(x0, y0, z1);
    int n6 = m_model->addNode(x1, y0, z1);
    int n7 = m_model->addNode(x1, y1, z1);
    int n8 = m_model->addNode(x0, y1, z1);

    // 2. Création des 4 poteaux verticaux reliant les étages en hauteur
    m_model->addColumn(n1, n5, 0.40, 0.40);
    m_model->addColumn(n2, n6, 0.40, 0.40);
    m_model->addColumn(n3, n7, 0.40, 0.40);
    m_model->addColumn(n4, n8, 0.40, 0.40);

    // 3. Création des 4 poutres d'encadrement inférieur
    m_model->addBeam(n1, n2, 0.30, 0.50);
    m_model->addBeam(n2, n3, 0.30, 0.50);
    m_model->addBeam(n3, n4, 0.30, 0.50);
    m_model->addBeam(n4, n1, 0.30, 0.50);

    // 4. Création des 4 poutres d'encadrement supérieur
    m_model->addBeam(n5, n6, 0.30, 0.50);
    m_model->addBeam(n6, n7, 0.30, 0.50);
    m_model->addBeam(n7, n8, 0.30, 0.50);
    m_model->addBeam(n8, n5, 0.30, 0.50);

    // 5. Création de la dalle supérieure
    m_model->addSlab({n5, n6, n7, n8}, 0.20);

    if (m_modelTree)
    {
        m_modelTree->refreshAll();
    }

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Structure Cube 3D créée entre Z = %1 m et Z = %2 m (8 nœuds, 4 poteaux, 8 poutres, 1 dalle)")
            .arg(z0, 0, 'f', 2)
            .arg(z1, 0, 'f', 2));
    }

    if (m_occView)
    {
        m_occView->fitAll();
    }
    updateUndoRedoActions();
}

void MainWindow::onActionViewXY()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXY);
}

void MainWindow::onActionViewYZ()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanYZ);
}

void MainWindow::onActionViewXZ()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXZ);
}

void MainWindow::onActionView3D()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::Perspective3D);
}

void MainWindow::onActionCoordSystem()
{
    if (!m_occView)
        return;
    bool isLocal = !m_occView->isLocalCoordinateSystem();
    m_occView->setLocalCoordinateSystem(isLocal);
    m_actionCoordSystem->setChecked(isLocal);
    statusBar()->showMessage(isLocal ? tr("Repère Local (LCS) activé") : tr("Repère Global (GCS) activé"), 3000);
}

void MainWindow::onActionSectionCut()
{
    if (!m_sectionCutDialog)
        return;

    if (m_occView)
    {
        m_sectionCutDialog->setCutLimits(-20.0, 50.0);
        m_sectionCutDialog->setCutPosition(m_occView->activeLevelElevation() + 1.20);
    }
    m_sectionCutDialog->show();
    m_sectionCutDialog->raise();
    m_sectionCutDialog->activateWindow();
}

void MainWindow::onActionMove3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Déplacement 3D"),
            tr("Veuillez d'abord sélectionner les éléments à déplacer (nœuds, barres, poteaux ou dalles)."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Move3D);
    }
}

void MainWindow::onActionCopy3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Copie 3D"),
            tr("Veuillez d'abord sélectionner les éléments à copier (nœuds, barres, poteaux ou dalles)."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Copy3D);
    }
}

void MainWindow::onActionRotate3D()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection())
    {
        QMessageBox::information(this, tr("Rotation 3D"),
            tr("Veuillez d'abord sélectionner les éléments à faire tourner."));
        if (m_actionSelectMode) m_actionSelectMode->setChecked(true);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Rotate3D);
    }
}

void MainWindow::onActionMoveOrigin()
{
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::MoveOrigin3D);
    }
}

void MainWindow::onActionCopyClipboard()
{
    if (!m_selectionManager || !m_selectionManager->hasSelection() || !m_model)
    {
        if (statusBar()) statusBar()->showMessage(tr("Presse-papier : Aucun élément sélectionné."), 3000);
        return;
    }

    std::unordered_set<int> allNodeIds(
        m_selectionManager->selectedNodes().begin(),
        m_selectionManager->selectedNodes().end()
    );
    for (int bId : m_selectionManager->selectedBeams())
    {
        const auto* b = m_model->getBeam(bId);
        if (b) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    }
    for (int cId : m_selectionManager->selectedColumns())
    {
        const auto* c = m_model->getColumn(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }
    for (int sId : m_selectionManager->selectedSlabs())
    {
        const auto* s = m_model->getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) allNodeIds.insert(nid);
        }
    }

    if (allNodeIds.empty())
        return;

    double minX = 1e9, minY = 1e9, minZ = 1e9;
    for (int nid : allNodeIds)
    {
        const auto* node = m_model->getNode(nid);
        if (node)
        {
            minX = std::min(minX, node->x());
            minY = std::min(minY, node->y());
            minZ = std::min(minZ, node->z());
        }
    }

    m_clipboard.hasData = true;
    m_clipboard.refOriginX = minX;
    m_clipboard.refOriginY = minY;
    m_clipboard.refOriginZ = minZ;

    m_clipboard.nodes.clear();
    for (int nid : allNodeIds)
    {
        const auto* node = m_model->getNode(nid);
        if (node)
        {
            ClipboardNode cn;
            cn.originalId = nid;
            cn.relX = node->x() - minX;
            cn.relY = node->y() - minY;
            cn.relZ = node->z() - minZ;
            m_clipboard.nodes.push_back(cn);
        }
    }

    m_clipboard.beams.clear();
    for (int bId : m_selectionManager->selectedBeams())
    {
        const auto* b = m_model->getBeam(bId);
        if (b)
        {
            ClipboardBeam cb;
            cb.originalStartNodeId = b->startNodeId();
            cb.originalEndNodeId = b->endNodeId();
            cb.width = b->width();
            cb.height = b->height();
            m_clipboard.beams.push_back(cb);
        }
    }

    m_clipboard.columns.clear();
    for (int cId : m_selectionManager->selectedColumns())
    {
        const auto* c = m_model->getColumn(cId);
        if (c)
        {
            ClipboardColumn cc;
            cc.originalStartNodeId = c->startNodeId();
            cc.originalEndNodeId = c->endNodeId();
            cc.width = c->width();
            cc.height = c->height();
            m_clipboard.columns.push_back(cc);
        }
    }

    m_clipboard.slabs.clear();
    for (int sId : m_selectionManager->selectedSlabs())
    {
        const auto* s = m_model->getSlab(sId);
        if (s)
        {
            ClipboardSlab cs;
            cs.originalNodeIds = s->nodeIds();
            cs.thickness = s->thickness();
            m_clipboard.slabs.push_back(cs);
        }
    }

    if (statusBar())
    {
        statusBar()->showMessage(tr("Presse-papier : %1 nœud(s), %2 poutre(s), %3 poteau(x), %4 dalle(s) copiés. (Appuyez sur Ctrl+V pour coller)")
            .arg(m_clipboard.nodes.size())
            .arg(m_clipboard.beams.size())
            .arg(m_clipboard.columns.size())
            .arg(m_clipboard.slabs.size()), 4000);
    }
}

void MainWindow::onActionPasteClipboard()
{
    if (!m_clipboard.hasData || m_clipboard.nodes.empty())
    {
        if (statusBar()) statusBar()->showMessage(tr("Presse-papier vide. Sélectionnez des éléments et faites Ctrl+C."), 3000);
        return;
    }
    if (m_occView)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Paste3D);
    }
}

void MainWindow::onPointToPointMoveRequested(const gp_Pnt& base, const gp_Pnt& target, bool isCopy)
{
    if (!m_selectionManager || !m_model)
        return;

    m_model->pushUndoState(isCopy ? tr("Copie 3D").toStdString() : tr("Déplacement 3D").toStdString());

    double dx = target.X() - base.X();
    double dy = target.Y() - base.Y();
    double dz = target.Z() - base.Z();

    if (!isCopy)
    {
        std::set<int> nodesToMove(
            m_selectionManager->selectedNodes().begin(),
            m_selectionManager->selectedNodes().end()
        );
        for (int bId : m_selectionManager->selectedBeams())
        {
            const auto* b = m_model->getBeam(bId);
            if (b) { nodesToMove.insert(b->startNodeId()); nodesToMove.insert(b->endNodeId()); }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            const auto* c = m_model->getColumn(cId);
            if (c) { nodesToMove.insert(c->startNodeId()); nodesToMove.insert(c->endNodeId()); }
        }
        for (int sId : m_selectionManager->selectedSlabs())
        {
            const auto* s = m_model->getSlab(sId);
            if (s)
            {
                for (int nid : s->nodeIds()) nodesToMove.insert(nid);
            }
        }

        if (m_model->moveNodes(nodesToMove, dx, dy, dz))
        {
            if (m_modelTree) m_modelTree->refreshAll();
            if (m_occView) m_occView->update();
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Déplacement 3D : %1 nœud(s) déplacé(s) de (%2, %3, %4) m")
                    .arg(nodesToMove.size())
                    .arg(dx, 0, 'f', 3)
                    .arg(dy, 0, 'f', 3)
                    .arg(dz, 0, 'f', 3));
            }
        }
    }
    else
    {
        auto newIds = m_model->copyElements(
            m_selectionManager->selectedNodes(),
            m_selectionManager->selectedBeams(),
            m_selectionManager->selectedColumns(),
            m_selectionManager->selectedSlabs(),
            dx, dy, dz, 1
        );
        if (!newIds.empty())
        {
            if (m_modelTree) m_modelTree->refreshAll();
            if (m_occView) m_occView->update();
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Copie 3D : %1 élément(s) créé(s) par translation")
                    .arg(newIds.size()));
            }
        }
    }
    updateUndoRedoActions();
}

void MainWindow::onPointToPointRotateRequested(const gp_Pnt& center, double angleRad, bool isCopy)
{
    if (!m_selectionManager || !m_model)
        return;

    m_model->pushUndoState(isCopy ? tr("Copie & Rotation 3D").toStdString() : tr("Rotation 3D").toStdString());

    gp_Dir axis(0.0, 0.0, 1.0);
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    double deg = angleRad * kRadToDeg;

    if (!isCopy)
    {
        std::set<int> nodesToRotate(
            m_selectionManager->selectedNodes().begin(),
            m_selectionManager->selectedNodes().end()
        );
        for (int bId : m_selectionManager->selectedBeams())
        {
            const auto* b = m_model->getBeam(bId);
            if (b) { nodesToRotate.insert(b->startNodeId()); nodesToRotate.insert(b->endNodeId()); }
        }
        for (int cId : m_selectionManager->selectedColumns())
        {
            const auto* c = m_model->getColumn(cId);
            if (c) { nodesToRotate.insert(c->startNodeId()); nodesToRotate.insert(c->endNodeId()); }
        }
        for (int sId : m_selectionManager->selectedSlabs())
        {
            const auto* s = m_model->getSlab(sId);
            if (s)
            {
                for (int nid : s->nodeIds()) nodesToRotate.insert(nid);
            }
        }

        if (m_model->rotateNodes(nodesToRotate, center, axis, angleRad))
        {
            if (m_modelTree) m_modelTree->refreshAll();
            if (m_occView) m_occView->update();
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Rotation 3D : %1 nœud(s) tourné(s) de %2° autour de (%3, %4, %5)")
                    .arg(nodesToRotate.size())
                    .arg(deg, 0, 'f', 1)
                    .arg(center.X(), 0, 'f', 2)
                    .arg(center.Y(), 0, 'f', 2)
                    .arg(center.Z(), 0, 'f', 2));
            }
        }
    }
    else
    {
        auto newIds = m_model->copyAndRotateElements(
            m_selectionManager->selectedNodes(),
            m_selectionManager->selectedBeams(),
            m_selectionManager->selectedColumns(),
            m_selectionManager->selectedSlabs(),
            center, axis, angleRad, 1
        );
        if (!newIds.empty())
        {
            if (m_modelTree) m_modelTree->refreshAll();
            if (m_occView) m_occView->update();
            if (m_statusInfo)
            {
                m_statusInfo->setText(tr("Copie & Rotation 3D : %1 élément(s) créé(s) (angle %2°)")
                    .arg(newIds.size())
                    .arg(deg, 0, 'f', 1));
            }
        }
    }
    updateUndoRedoActions();
}

void MainWindow::onOriginMoveRequested(const gp_Pnt& newOrigin)
{
    if (m_gridManager)
    {
        if (auto* grid = m_gridManager->activeGrid())
        {
            auto gdef = grid->definition();
            gdef.setOrigin(newOrigin.X(), newOrigin.Y(), newOrigin.Z());
            grid->updateDefinition(gdef);
        }
    }
    if (m_occView)
    {
        m_occView->rebuildGrid();
    }
    if (m_viewportContainer)
    {
        m_viewportContainer->updateRulers();
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Repère Global & Grille déplacés en (%1, %2, %3) m")
            .arg(newOrigin.X(), 0, 'f', 3)
            .arg(newOrigin.Y(), 0, 'f', 3)
            .arg(newOrigin.Z(), 0, 'f', 3));
    }
}

void MainWindow::onPasteAtPointRequested(const gp_Pnt& target)
{
    if (!m_model || !m_clipboard.hasData || m_clipboard.nodes.empty())
        return;

    m_model->pushUndoState(tr("Coller").toStdString());

    std::unordered_map<int, int> nodeMap;
    std::vector<int> newNodes;
    for (const auto& cn : m_clipboard.nodes)
    {
        double nx = target.X() + cn.relX;
        double ny = target.Y() + cn.relY;
        double nz = target.Z() + cn.relZ;
        int newId = m_model->addNode(nx, ny, nz);
        nodeMap[cn.originalId] = newId;
        newNodes.push_back(newId);
    }

    std::vector<int> newBeams;
    for (const auto& cb : m_clipboard.beams)
    {
        auto itS = nodeMap.find(cb.originalStartNodeId);
        auto itE = nodeMap.find(cb.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            int bId = m_model->addBeam(itS->second, itE->second, cb.width, cb.height);
            newBeams.push_back(bId);
        }
    }

    std::vector<int> newColumns;
    for (const auto& cc : m_clipboard.columns)
    {
        auto itS = nodeMap.find(cc.originalStartNodeId);
        auto itE = nodeMap.find(cc.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            int cId = m_model->addColumn(itS->second, itE->second, cc.width, cc.height);
            newColumns.push_back(cId);
        }
    }

    std::vector<int> newSlabs;
    for (const auto& cs : m_clipboard.slabs)
    {
        std::vector<int> sNodes;
        for (int onid : cs.originalNodeIds)
        {
            auto it = nodeMap.find(onid);
            if (it != nodeMap.end())
            {
                sNodes.push_back(it->second);
            }
        }
        if (sNodes.size() >= 3)
        {
            int sId = m_model->addSlab(sNodes, cs.thickness);
            newSlabs.push_back(sId);
        }
    }

    // Sélectionner les nouveaux éléments créés
    if (m_selectionManager)
    {
        m_selectionManager->clearSelection();
        for (int nid : newNodes) m_selectionManager->selectNode(nid);
        for (int bid : newBeams) m_selectionManager->selectBeam(bid);
        for (int cid : newColumns) m_selectionManager->selectColumn(cid);
        for (int sid : newSlabs) m_selectionManager->selectSlab(sid);
    }

    if (m_modelTree) m_modelTree->refreshAll();
    if (m_occView)
    {
        m_occView->update();
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Collé en (%1, %2, %3) m : %4 nœud(s), %5 poutre(s), %6 poteau(x), %7 dalle(s)")
            .arg(target.X(), 0, 'f', 2)
            .arg(target.Y(), 0, 'f', 2)
            .arg(target.Z(), 0, 'f', 2)
            .arg(newNodes.size())
            .arg(newBeams.size())
            .arg(newColumns.size())
            .arg(newSlabs.size()));
    }
    updateUndoRedoActions();
}

void MainWindow::onActionUndo()
{
    if (m_model && m_model->canUndo())
    {
        std::string actionName = m_model->lastUndoActionName();
        if (m_model->undo())
        {
            if (m_selectionManager)
            {
                m_selectionManager->clearSelection();
            }
            if (m_modelTree)
            {
                m_modelTree->refreshAll();
            }
            if (m_occView)
            {
                m_occView->rebuildAllShapes();
                m_occView->update();
            }
            updateUndoRedoActions();
            if (m_statusInfo && m_model)
            {
                m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
                    .arg(m_model->nodes().size())
                    .arg(m_model->beams().size())
                    .arg(m_model->columns().size())
                    .arg(m_model->slabs().size()));
            }
            statusBar()->showMessage(tr("Action annulée : %1 (Ctrl+Z)").arg(QString::fromStdString(actionName)), 3000);
        }
    }
}

void MainWindow::onActionRedo()
{
    if (m_model && m_model->canRedo())
    {
        std::string actionName = m_model->lastRedoActionName();
        if (m_model->redo())
        {
            if (m_selectionManager)
            {
                m_selectionManager->clearSelection();
            }
            if (m_modelTree)
            {
                m_modelTree->refreshAll();
            }
            if (m_occView)
            {
                m_occView->rebuildAllShapes();
                m_occView->update();
            }
            updateUndoRedoActions();
            if (m_statusInfo && m_model)
            {
                m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
                    .arg(m_model->nodes().size())
                    .arg(m_model->beams().size())
                    .arg(m_model->columns().size())
                    .arg(m_model->slabs().size()));
            }
            statusBar()->showMessage(tr("Action rétablie : %1 (Ctrl+Y)").arg(QString::fromStdString(actionName)), 3000);
        }
    }
}

void MainWindow::updateUndoRedoActions()
{
    if (!m_model) return;
    if (m_actionUndo)
    {
        bool canU = m_model->canUndo();
        m_actionUndo->setEnabled(canU);
        if (canU && !m_model->lastUndoActionName().empty())
        {
            m_actionUndo->setText(tr("&Annuler %1").arg(QString::fromStdString(m_model->lastUndoActionName())));
            m_actionUndo->setToolTip(tr("Annuler : %1 (Ctrl+Z)").arg(QString::fromStdString(m_model->lastUndoActionName())));
        }
        else
        {
            m_actionUndo->setText(tr("&Annuler"));
            m_actionUndo->setToolTip(tr("Annuler la dernière action (Ctrl+Z)"));
        }
    }
    if (m_actionRedo)
    {
        bool canR = m_model->canRedo();
        m_actionRedo->setEnabled(canR);
        if (canR && !m_model->lastRedoActionName().empty())
        {
            m_actionRedo->setText(tr("&Rétablir %1").arg(QString::fromStdString(m_model->lastRedoActionName())));
            m_actionRedo->setToolTip(tr("Rétablir : %1 (Ctrl+Y)").arg(QString::fromStdString(m_model->lastRedoActionName())));
        }
        else
        {
            m_actionRedo->setText(tr("&Rétablir"));
            m_actionRedo->setToolTip(tr("Rétablir la dernière action annulée (Ctrl+Y)"));
        }
    }
}

