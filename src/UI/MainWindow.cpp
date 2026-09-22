#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Properties/PropertyPanel.h"
#include "Dialogs/TransformDialog.h"
#include "Dialogs/GridSettingsDialog.h"

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
#include <sstream>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_model(std::make_unique<TSA::Model::Model>())
    , m_selectionManager(std::make_unique<TSA::Viewer::SelectionManager>(this))
{
    setupUi();

    // Modèle initial 3D complet :
    // Bâtiment structural à 4 poteaux, 4 poutres et 1 dalle supérieure
    int n1 = m_model->addNode(0.0, 0.0, 0.0);
    int n2 = m_model->addNode(0.0, 0.0, 3.0);
    int n3 = m_model->addNode(6.0, 0.0, 0.0);
    int n4 = m_model->addNode(6.0, 0.0, 3.0);
    int n5 = m_model->addNode(0.0, 4.0, 0.0);
    int n6 = m_model->addNode(0.0, 4.0, 3.0);
    int n7 = m_model->addNode(6.0, 4.0, 0.0);
    int n8 = m_model->addNode(6.0, 4.0, 3.0);

    // 4 Poteaux verticaux
    m_model->addColumn(n1, n2, 0.35, 0.35);
    m_model->addColumn(n3, n4, 0.35, 0.35);
    m_model->addColumn(n5, n6, 0.35, 0.35);
    m_model->addColumn(n7, n8, 0.35, 0.35);

    // 4 Poutres horizontales de toiture
    m_model->addBeam(n2, n4, 0.30, 0.50);
    m_model->addBeam(n4, n8, 0.30, 0.50);
    m_model->addBeam(n8, n6, 0.30, 0.50);
    m_model->addBeam(n6, n2, 0.30, 0.50);

    // 1 Dalle surfacique supérieure reliant les 4 têtes de poteaux
    m_model->addSlab({ n2, n4, n8, n6 }, 0.20);

    m_occView->setModel(m_model.get());
    m_modelTree->refreshAll();

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
    resize(1440, 880);

    setDockNestingEnabled(true);

    // Widget central : Viewport OpenCASCADE
    m_occView = new OccView(this);
    m_occView->setSelectionManager(m_selectionManager.get());
    setCentralWidget(m_occView);

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
    m_actionMove = editMenu->addAction(tr("&Move Elements..."), this, &MainWindow::onActionMove);
    m_actionMove->setIcon(QIcon(":/icons/move.svg"));
    m_actionMove->setToolTip(tr("Move Elements (M)..."));
    m_actionMove->setShortcut(QKeySequence(Qt::Key_M));

    m_actionCopy = editMenu->addAction(tr("&Copy / Repeat..."), this, &MainWindow::onActionCopy);
    m_actionCopy->setIcon(QIcon(":/icons/copy.svg"));
    m_actionCopy->setToolTip(tr("Copy / Repeat Elements (Ctrl+D)..."));
    m_actionCopy->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));

    editMenu->addSeparator();

    m_actionDelete = editMenu->addAction(tr("&Delete Selected"), this, &MainWindow::onActionDeleteSelected);
    m_actionDelete->setIcon(QIcon(":/icons/delete.svg"));
    m_actionDelete->setToolTip(tr("Delete Selected Elements (Del)"));
    m_actionDelete->setShortcut(QKeySequence::Delete);

    // Menu Vue
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    m_actionFitAll = viewMenu->addAction(tr("&Fit All"), this, &MainWindow::onFitAll);
    m_actionFitAll->setIcon(QIcon(":/icons/fit_all.svg"));
    m_actionFitAll->setToolTip(tr("Fit All (F)"));
    m_actionFitAll->setShortcut(QKeySequence(Qt::Key_F));

    m_actionResetView = viewMenu->addAction(tr("&Reset View (Isometric)"), this, &MainWindow::onResetView);
    m_actionResetView->setIcon(QIcon(":/icons/view_iso.svg"));
    m_actionResetView->setToolTip(tr("Reset View - Isometric (R)"));
    m_actionResetView->setShortcut(QKeySequence(Qt::Key_R));

    viewMenu->addSeparator();

    // Sous-menu Grille 3D
    QMenu* gridMenu = viewMenu->addMenu(tr("&3D Grid"));
    auto* gridGroup = new QActionGroup(this);

    m_actionGridCartesian = gridMenu->addAction(tr("&Cartesian Grid (Rectangular)"), this, &MainWindow::onGridCartesian);
    m_actionGridCartesian->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_actionGridCartesian->setToolTip(tr("Cartesian Grid (Rectangular)"));
    m_actionGridCartesian->setCheckable(true);
    m_actionGridCartesian->setChecked(true);
    gridGroup->addAction(m_actionGridCartesian);

    m_actionGridCylindrical = gridMenu->addAction(tr("C&ylindrical Grid (Polar / Radial)"), this, &MainWindow::onGridCylindrical);
    m_actionGridCylindrical->setIcon(QIcon(":/icons/grid_cylindrical.svg"));
    m_actionGridCylindrical->setToolTip(tr("Cylindrical Grid (Polar / Radial)"));
    m_actionGridCylindrical->setCheckable(true);
    gridGroup->addAction(m_actionGridCylindrical);

    m_actionGridHide = gridMenu->addAction(tr("&Hide Grid"), this, &MainWindow::onGridHide);
    m_actionGridHide->setCheckable(true);
    gridGroup->addAction(m_actionGridHide);

    gridMenu->addSeparator();

    m_actionGridSnap = gridMenu->addAction(tr("&Snap Cursor to Grid"), this, &MainWindow::onToggleGridSnap);
    m_actionGridSnap->setIcon(QIcon(":/icons/snap.svg"));
    m_actionGridSnap->setToolTip(tr("Snap Cursor to Grid (S)"));
    m_actionGridSnap->setCheckable(true);
    m_actionGridSnap->setChecked(false);
    m_actionGridSnap->setShortcut(QKeySequence(Qt::Key_S));

    gridMenu->addSeparator();
    m_actionGridSettings = gridMenu->addAction(tr("Grid &Settings..."), this, &MainWindow::onGridSettings);
    m_actionGridSettings->setIcon(QIcon(":/icons/settings.svg"));
    m_actionGridSettings->setToolTip(tr("Grid Settings..."));

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

    // Menu Modèle (Création d'éléments structuraux)
    QMenu* modelMenu = menuBar()->addMenu(tr("&Model"));

    QMenu* drawMenu = modelMenu->addMenu(tr("3D &Drawing Modes"));
    drawMenu->addAction(m_actionSelectMode);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actionDrawNode);
    drawMenu->addAction(m_actionDrawBeam);
    drawMenu->addAction(m_actionDrawColumn);
    drawMenu->addAction(m_actionDrawSlab);

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

    // Barre d'outils Modélisation
    QToolBar* modelToolBar = addToolBar(tr("Model"));
    modelToolBar->setObjectName("ModelToolBar");
    modelToolBar->setIconSize(QSize(22, 22));
    modelToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    modelToolBar->addAction(m_actionNewNode);
    modelToolBar->addAction(m_actionNewBeam);
    modelToolBar->addAction(m_actionNewColumn);
    modelToolBar->addAction(m_actionNewSlab);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionMove);
    modelToolBar->addAction(m_actionCopy);
    modelToolBar->addSeparator();
    modelToolBar->addAction(m_actionDelete);

    // Barre d'outils Vue
    QToolBar* viewToolBar = addToolBar(tr("View"));
    viewToolBar->setObjectName("ViewToolBar");
    viewToolBar->setIconSize(QSize(22, 22));
    viewToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    viewToolBar->addAction(m_actionFitAll);
    viewToolBar->addAction(m_actionResetView);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_actionGridCartesian);
    viewToolBar->addAction(m_actionGridCylindrical);
    viewToolBar->addAction(m_actionGridSnap);
    viewToolBar->addAction(m_actionGridSettings);
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

    connect(m_occView, &OccView::gridTypeChanged, this, [this](OccView::GridType type) {
        if (type == OccView::GridType::Cartesian)
        {
            if (m_actionGridCartesian) m_actionGridCartesian->setChecked(true);
        }
        else if (type == OccView::GridType::Cylindrical)
        {
            if (m_actionGridCylindrical) m_actionGridCylindrical->setChecked(true);
        }
        else
        {
            if (m_actionGridHide) m_actionGridHide->setChecked(true);
        }
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

void MainWindow::onGridCartesian()
{
    if (m_occView)
    {
        m_occView->showCartesianGrid();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("3D Cartesian Grid active"));
        }
    }
}

void MainWindow::onGridCylindrical()
{
    if (m_occView)
    {
        m_occView->showCylindricalGrid();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("3D Cylindrical Grid active"));
        }
    }
}

void MainWindow::onGridHide()
{
    if (m_occView)
    {
        m_occView->hideGrid();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Grid hidden"));
        }
    }
}

void MainWindow::onToggleGridSnap(bool checked)
{
    if (m_occView)
    {
        m_occView->setSnapToGridEnabled(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Magnetic Grid Snapping enabled") : tr("Grid Snapping disabled"));
        }
    }
}

void MainWindow::onGridSettings()
{
    if (m_occView)
    {
        TSA::UI::GridSettingsDialog dlg(m_occView, this);
        dlg.exec();
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

    int newId = m_model->addNode(x, y, z);
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

    int beamId = m_model->addBeam(startNode, endNode, 0.30, 0.50);
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

    int colId = m_model->addColumn(startNode, endNode, 0.35, 0.35);
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

    int slabId = m_model->addSlab(nodeIds, thickness);
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

    if (m_model->moveNodes(nodesToMove, dx, dy, dz))
    {
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

    auto newIds = m_model->copyElements(
        m_selectionManager->selectedNodes(),
        m_selectionManager->selectedBeams(),
        m_selectionManager->selectedColumns(),
        m_selectionManager->selectedSlabs(),
        dx, dy, dz, reps
    );

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
}
