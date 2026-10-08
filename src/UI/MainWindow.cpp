#include "../Analysis/Engine/AnalysisManager.h"
#include "App/ProductInfo.h"
#include "Analysis/AnalysisEngineOptions.h"
#include "Dock/AnalysisDataDock.h"
#include "MainWindow.h"
#include "../Viewer/OccView.h"
#include "../Viewer/SelectionManager.h"
#include "../Model/Model.h"
#include "../Coordinate/WorkPlane.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Properties/PropertyPanel.h"
#include "Dialogs/GridDialog.h"
#include "Dialogs/GridSettingsDialog.h"
#include "Dialogs/LevelDialog.h"
#include "Dialogs/WorkPlaneDialog.h"
#include "Dialogs/SectionCutDialog.h"
#include "Ruler/ViewportContainer.h"
#include "Diagrams/Diagram2DWidget.h"
#include "../NDC/NDCViewerWidget.h"
#include "../Analysis/OpenSeesSolver.h"
#include "../Analysis/ResultsModel.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Viewer/ResultsVisualManager.h"
#include "Ribbon/RibbonBar.h"
#include "Ribbon/RibbonBuilder.h"
#include "Dock/VisibilityDock.h"
#include "Dock/StructuralElementsDock.h"
#include "Dock/LogConsoleDock.h"
#include "Dock/ResultsDockWidget.h"
#include "Dock/ProjectionViewDock.h"
#include "WindowManager/WindowManager.h"
#include "../Diagnostics/Logger.h"
#include "../Analysis/ResultsValidityGuard.h"
#include "../Diagnostics/DiagnosticReport.h"
#include "Theme/ThemeManager.h"
#include "Dialogs/HelpDialog.h"
#include "Widgets/ProjectStatusOverlay.h"
#include "Dialogs/StructurePresetDialog.h"
#include "Dialogs/BarCreationDialog.h"
#include "Dialogs/CableCreationDialog.h"
#include "Dialogs/SurfaceCreationDialog.h"
#include "Dialogs/LibraryDialog.h"
#include "Dialogs/ExtensionManagerDialog.h"
#include "Dialogs/NewNodeDialog.h"
#include "Dialogs/NodeSelectionDialog.h"
#include "../Library/LibraryManager.h"
#include "../Project/ProjectManager.h"
#include "../Project/ProjectSession.h"
#include "../IO/TSAFile.h"
#include "../UndoRedo/CommandManager.h"
#include "../UndoRedo/EditTransaction.h"
#include "../Commands/CreateElementCommands.h"
#include "../Commands/ModifyCommands.h"
#include "../Commands/CommandCatalog.h"
#include "Home/NewProjectDialog.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QEvent>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QTimer>
#include <QSignalBlocker>
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
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace
{
static inline QIcon makeThemeIcon(bool dark) { return QIcon(dark ? ":/icons/common/theme_dark.svg" : ":/icons/common/theme_light.svg"); }
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_session(std::make_unique<TSA::Project::ProjectSession>(this))
    , m_model(&m_session->model())
    , m_commandManager(&m_session->commands())
    , m_selectionManager(std::make_unique<TSA::Viewer::SelectionManager>(this))
    , m_gridManager(&m_session->grids())
    , m_gridSnapManager(&m_session->gridSnap())
    , m_projectManager(&m_session->project())
    , m_windowManager(std::make_unique<TSA::UI::WindowManager>(this, this))
{
    // QMainWindow est toujours créée comme fenêtre : embarquée dans AppShell, c'est une page.
    if (parent) setWindowFlags(Qt::Widget);

    m_resultsGuard = std::make_unique<TSA::Analysis::ResultsValidityGuard>(m_model);
    m_resultsGuard->setStaleCallback([this]() {
        // Différé : on est au milieu d'une notification du modèle ; ne pas toucher aux vues ici.
        QTimer::singleShot(0, this, &MainWindow::onResultsBecameStale);
    });

    // Grilles rattachées au modèle : leurs définitions entrent dans l'historique Annuler (BUG-003).
    m_model->setGridManager(m_gridManager);

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

    m_occView->setModel(m_model);
    m_occView->setGridManager(m_gridManager, m_gridSnapManager);
    m_occView->setCreationPresets(m_presets);

    m_openSeesSolver = std::make_unique<TSA::Analysis::OpenSeesSolver>(this);
    m_engineRegistry = std::make_unique<TSA::Analysis::AnalysisEngineRegistry>();
    TSA::Analysis::registerBuiltInEngines(*m_engineRegistry);
    m_analysisManager = std::make_unique<TSA::Analysis::AnalysisManager>(*m_engineRegistry);
    m_engineOptions = std::make_unique<TSA::UI::AnalysisEngineOptionsRegistry>();
    TSA::UI::registerBuiltInEngineOptions(*m_engineOptions);
    if (!m_engineRegistry->ids().empty()) m_analysisContext.engineId = m_engineRegistry->ids().front();
    if (m_diagramWidget)
    {
        m_diagramWidget->setModel(m_model);
    }
    if (m_ndcWidget)
    {
        m_ndcWidget->setModel(m_model);
    }

    connect(m_gridManager, &TSA::Grid::GridManager::gridAdded, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager, &TSA::Grid::GridManager::gridRemoved, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager, &TSA::Grid::GridManager::gridModified, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager, &TSA::Grid::GridManager::activeGridChanged, this, [this]() {
        m_occView->rebuildGrid();
    });
    connect(m_gridManager, &TSA::Grid::GridManager::gridVisibilityChanged, this, [this]() {
        m_occView->rebuildGrid();
    });

    m_modelTree->setGridManager(m_gridManager);
    m_modelTree->refreshAll();

    m_sectionCutDialog = new TSA::UI::SectionCutDialog(this);
    connect(m_sectionCutDialog, &TSA::UI::SectionCutDialog::clippingChanged, this, [this](bool enabled, int axis, double pos, bool flip) {
        if (m_occView)
        {
            m_occView->setClippingEnabled(enabled);
            m_occView->setClipPlane(axis, pos, flip);
        }
    });
    connect(m_sectionCutDialog, &TSA::UI::SectionCutDialog::sectionPlaneDisplayChanged, this, [this](bool visible) {
        if (m_occView)
            m_occView->setSectionPlaneVisible(visible);
    });

    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Model: %1 nodes, %2 beams, %3 columns, %4 slabs | Ready")
            .arg(m_model->nodes().size())
            .arg(m_model->beams().size())
            .arg(m_model->columns().size())
            .arg(m_model->slabs().size()));
    }

    connect(&TSA::UI::ThemeManager::instance(), &TSA::UI::ThemeManager::themeChanged, this, &MainWindow::applyTheme);
    applyTheme(TSA::UI::ThemeManager::instance().isDarkMode());

    setAcceptDrops(true);
    if (m_projectManager)
    {
        connect(m_projectManager, &TSA::Project::ProjectManager::projectTitleChanged, this, &MainWindow::setWindowTitle);
    }
    if (m_occView)
    {
        connect(m_occView, &OccView::fileDropped, this, [this](const QString& filePath) {
            if (maybeSave())
            {
                loadFile(filePath);
            }
        });
    }

    if (m_windowManager)
    {
        m_windowManager->restoreLayout();
    }

    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    // Les enfants QObject (orchestrateur IA, docks, vue…) sont détruits APRÈS ce destructeur, par
    // ~QObject : un signal qu'ils émettent alors vers un slot ou une lambda de MainWindow
    // s'exécuterait sur une fenêtre déjà détruite (assertion Qt « Called object is not of the
    // correct type »). Constaté : ~AIOrchestrator arrête llama-server → deux stateChanged →
    // statusChanged → updateAIStatusWidget. Toutes ces connexions sont coupées ici.
    for (QObject* child : findChildren<QObject*>())
        QObject::disconnect(child, nullptr, this, nullptr);
    if (m_model) m_model->setGridManager(nullptr);
}

void MainWindow::onResultsBecameStale()
{
    if (!m_resultsModel || m_resultsModel->isValid())
        return;

    // Les consommateurs vérifient déjà ResultsModel::isValid() : il suffit de les rafraîchir.
    if (m_occView && m_occView->resultsVisual())
    {
        m_occView->resultsVisual()->clearAllVisuals();
        m_occView->update();
    }
    if (m_diagramWidget) m_diagramWidget->setResultsModel(m_resultsModel);
    if (m_ndcWidget) m_ndcWidget->setResultsModel(m_resultsModel);
    if (m_propertyPanel) m_propertyPanel->setResultsModel(m_resultsModel);
    if (m_analysisDataDock) m_analysisDataDock->setResultsModel(m_resultsModel);
    if (m_resultsDock) m_resultsDock->setResultsModel(m_resultsModel);

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Modèle modifié depuis le dernier calcul : résultats obsolètes, relancez l'analyse."), "WARN");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Résultats obsolètes — recalcul nécessaire"));
    }
}

void MainWindow::setupUi()
{
    setWindowTitle(QString::fromUtf8(TSA::Product::kMainWindowTitle));
    setDockNestingEnabled(true);

    // Widget central : Viewport OpenCASCADE entouré des règles graduées (style Robot)
    m_occView = new OccView(this);
    m_occView->setSelectionManager(m_selectionManager.get());
    m_viewportContainer = new TSA::UI::ViewportContainer(m_occView, this);
    m_viewportContainer->setModel(m_model);
    setCentralWidget(m_viewportContainer);
    createPreviewCapture();

    createActions();
    createDockWindows();
    createMenus();
    createRibbon();
    createStatusBar();
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

void MainWindow::openBarCreationDialog(TSA::Model::BarRole role)
{
    if (!m_occView) return;
    if (!m_barDialog)
    {
        m_barDialog = new TSA::UI::BarCreationDialog(m_model, m_occView, this);
        connect(m_barDialog, &TSA::UI::BarCreationDialog::barPropertiesChanged, this, [this](const TSA::Model::BarProperties& p) {
            if (m_occView) m_occView->setCurrentBarProperties(p);
        });
        connect(m_occView, &OccView::barFirstPointPicked, m_barDialog, &TSA::UI::BarCreationDialog::onFirstPointPicked);
        connect(m_occView, &OccView::barSecondPointPicked, m_barDialog, &TSA::UI::BarCreationDialog::onSecondPointPicked);
        connect(m_occView, &OccView::barDrawingCancelled, m_barDialog, &TSA::UI::BarCreationDialog::onDrawingCancelled);
    }

    m_barDialog->setRole(role);
    m_occView->setCurrentBarProperties(m_barDialog->currentProperties());
    if (role == TSA::Model::BarRole::Beam)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawBeam);
        if (m_actionDrawBeam) m_actionDrawBeam->setChecked(true);
    }
    else if (role == TSA::Model::BarRole::Column)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawColumn);
        if (m_actionDrawColumn) m_actionDrawColumn->setChecked(true);
    }
    else
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawBar);
        if (m_actionDrawBar) m_actionDrawBar->setChecked(true);
    }

    m_barDialog->showNormal();
    m_barDialog->raise();
    m_barDialog->activateWindow();

    // Positionner le dialogue de manière bien visible au premier plan, au centre-droit du viewport 3D
    if (m_viewportContainer)
    {
        QPoint vpGlobal = m_viewportContainer->mapToGlobal(QPoint(0, 0));
        int targetX = vpGlobal.x() + m_viewportContainer->width() - m_barDialog->width() - 40;
        int targetY = vpGlobal.y() + 40;
        if (targetX < vpGlobal.x() + 20) targetX = vpGlobal.x() + 20;
        if (targetY < vpGlobal.y() + 20) targetY = vpGlobal.y() + 20;
        m_barDialog->move(targetX, targetY);
    }
}

void MainWindow::openCableCreationDialog()
{
    if (!m_occView) return;
    if (!m_cableDialog)
    {
        m_cableDialog = new TSA::UI::CableCreationDialog(m_model, m_occView, this);
        connect(m_occView, &OccView::cableFirstPointPicked, m_cableDialog, &TSA::UI::CableCreationDialog::onFirstPointPicked);
        connect(m_occView, &OccView::cableSecondPointPicked, m_cableDialog, &TSA::UI::CableCreationDialog::onSecondPointPicked);
        connect(m_occView, &OccView::cableDrawingCancelled, m_cableDialog, &TSA::UI::CableCreationDialog::onDrawingCancelled);
        connect(m_cableDialog, &TSA::UI::CableCreationDialog::cableCreated, this, [this](int cableId) {
            if (m_statusInfo) m_statusInfo->setText(tr("Câble C%1 créé avec succès").arg(cableId));
            updateUndoRedoActions();
        });
    }

    m_occView->setInteractionMode(OccView::InteractionMode::DrawCable);
    if (m_actionDrawCable) m_actionDrawCable->setChecked(true);

    m_cableDialog->showNormal();
    m_cableDialog->raise();
    m_cableDialog->activateWindow();

    // Positionner le dialogue de manière bien visible au premier plan, au centre-droit du viewport 3D
    if (m_viewportContainer)
    {
        QPoint vpGlobal = m_viewportContainer->mapToGlobal(QPoint(0, 0));
        int targetX = vpGlobal.x() + m_viewportContainer->width() - m_cableDialog->width() - 40;
        int targetY = vpGlobal.y() + 40;
        if (targetX < vpGlobal.x() + 20) targetX = vpGlobal.x() + 20;
        if (targetY < vpGlobal.y() + 20) targetY = vpGlobal.y() + 20;
        m_cableDialog->move(targetX, targetY);
    }
}

void MainWindow::onModeDrawWire()
{
    openBarCreationDialog(TSA::Model::BarRole::Beam);
}

void MainWindow::onModeDrawBar()
{
    openBarCreationDialog(TSA::Model::BarRole::Generic);
}

void MainWindow::onModeDrawBeam()
{
    openBarCreationDialog(TSA::Model::BarRole::Beam);
}

void MainWindow::onModeDrawColumn()
{
    openBarCreationDialog(TSA::Model::BarRole::Column);
}

void MainWindow::onModeDrawCable()
{
    openCableCreationDialog();
}

void MainWindow::openSurfaceCreationDialog(int surfaceType)
{
    if (!m_occView) return;
    if (!m_surfaceDialog)
    {
        m_surfaceDialog = new TSA::UI::SurfaceCreationDialog(m_model, m_occView, this);
        connect(m_occView, &OccView::slabNodePicked, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onSlabNodePicked);
        connect(m_occView, &OccView::slabCreated, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onSlabCreated);
        connect(m_occView, &OccView::slabDrawingCancelled, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onSlabDrawingCancelled);
        connect(m_occView, &OccView::wallFirstPointPicked, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onWallFirstPointPicked);
        connect(m_occView, &OccView::wallSecondPointPicked, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onWallSecondPointPicked);
        connect(m_occView, &OccView::wallDrawingCancelled, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onWallDrawingCancelled);
        connect(m_occView, &OccView::wallCreated, m_surfaceDialog, &TSA::UI::SurfaceCreationDialog::onWallCreated);
    }

    m_surfaceDialog->setSurfaceType(static_cast<TSA::UI::SurfaceCreationDialog::SurfaceType>(surfaceType));
    if (surfaceType == 0)
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawSlab);
        if (m_actionDrawSlab) m_actionDrawSlab->setChecked(true);
    }
    else
    {
        m_occView->setInteractionMode(OccView::InteractionMode::DrawWall);
        if (m_actionDrawWall) m_actionDrawWall->setChecked(true);
    }

    m_surfaceDialog->showNormal();
    m_surfaceDialog->raise();
    m_surfaceDialog->activateWindow();

    if (m_viewportContainer)
    {
        QPoint vpGlobal = m_viewportContainer->mapToGlobal(QPoint(0, 0));
        int targetX = vpGlobal.x() + m_viewportContainer->width() - m_surfaceDialog->width() - 40;
        int targetY = vpGlobal.y() + 40;
        if (targetX < vpGlobal.x() + 20) targetX = vpGlobal.x() + 20;
        if (targetY < vpGlobal.y() + 20) targetY = vpGlobal.y() + 20;
        m_surfaceDialog->move(targetX, targetY);
    }
}

void MainWindow::onModeDrawSurface()
{
    openSurfaceCreationDialog(0);
}

void MainWindow::onModeDrawSlab()
{
    openSurfaceCreationDialog(0);
}

void MainWindow::onModeDrawWall()
{
    openSurfaceCreationDialog(1);
}

void MainWindow::onActionStructurePresets()
{
    TSA::UI::StructurePresetDialog dlg(m_presets, TSA::UI::PresetTarget::Wall, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_presets = dlg.presets();
        if (m_occView)
        {
            m_occView->setCreationPresets(m_presets);
        }
    }
}

void MainWindow::onActionLibrary(int tabIndex)
{
    TSA::UI::LibraryDialog dlg(m_model, this);
    dlg.selectTab(tabIndex);
    connect(&dlg, &TSA::UI::LibraryDialog::sectionLibraryUpdated, this, [this]() {
        if (m_propertyPanel) m_propertyPanel->refreshLibraryLists();
    });
    connect(&dlg, &TSA::UI::LibraryDialog::materialLibraryUpdated, this, [this]() {
        if (m_propertyPanel) m_propertyPanel->refreshLibraryLists();
    });
    connect(&dlg, &TSA::UI::LibraryDialog::colorLibraryUpdated, this, [this]() {
        if (m_propertyPanel) m_propertyPanel->refreshLibraryLists();
    });
    dlg.exec();
}

void MainWindow::onActionExtensionManager()
{
    TSA::UI::ExtensionManagerDialog dlg(this);
    connect(&dlg, &TSA::UI::ExtensionManagerDialog::extensionsReloaded, this, [this]() {
        if (m_propertyPanel) m_propertyPanel->refreshLibraryLists();
        if (m_occView) m_occView->rebuildAllShapes();
    });
    dlg.exec();
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
    if (m_gridDialog)
    {
        m_gridDialog->show();
        m_gridDialog->raise();
        m_gridDialog->activateWindow();
        return;
    }

    m_gridDialog = new TSA::UI::GridDialog(m_gridManager, m_model, m_occView, this);
    m_gridDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_gridDialog, &TSA::UI::GridDialog::gridDefinitionApplied, this, [this](const TSA::Grid::GridDefinition& /*def*/) {
        m_occView->rebuildGrid();
        m_modelTree->refreshGrids();
        m_modelTree->refreshLevels();
        if (m_viewportContainer)
        {
            m_viewportContainer->updateRulers();
        }
    });
    connect(m_gridDialog, &TSA::UI::GridDialog::manageGridsRequested, this, &MainWindow::onGridManagerDialog);
    connect(m_gridDialog, &TSA::UI::GridDialog::destroyed, this, [this]() {
        m_occView->rebuildGrid();
        m_modelTree->refreshGrids();
        m_modelTree->refreshLevels();
        if (m_viewportContainer)
        {
            m_viewportContainer->updateRulers();
        }
    });

    m_gridDialog->show();
    m_gridDialog->raise();
    m_gridDialog->activateWindow();
}

void MainWindow::onGridManagerDialog()
{
    if (m_gridManager && m_gridSnapManager && m_occView)
    {
        if (m_gridSettingsDialog)
        {
            m_gridSettingsDialog->show();
            m_gridSettingsDialog->raise();
            m_gridSettingsDialog->activateWindow();
            return;
        }

        m_gridSettingsDialog = new TSA::UI::GridSettingsDialog(m_gridManager, m_gridSnapManager, m_occView, this);
        m_gridSettingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_gridSettingsDialog->show();
        m_gridSettingsDialog->raise();
        m_gridSettingsDialog->activateWindow();
    }
}

void MainWindow::onManageLevels()
{
    if (!m_model || !m_model->levelManager())
        return;

    // Une seule entrée Annuler pour tout ce qui est fait dans la fenêtre (niveaux + nœuds rattachés
    // déplacés), aucune si rien n'a changé (BUG-003).
    const std::string before = m_model->coordinateSystem() ? m_model->coordinateSystem()->serializeToJson() : std::string();
    TSA::UndoRedo::EditTransaction tx(*m_model, tr("Modifier les niveaux").toStdString());
    TSA::UI::LevelDialog dlg(m_model->levelManager(), this);
    dlg.exec();
    if (m_model->coordinateSystem() && m_model->coordinateSystem()->serializeToJson() != before)
        tx.commit();
    else
        tx.discardUnchanged();
    updateUndoRedoActions();
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
            m_statusInfo->setText(checked ? tr("Accrochage magnétique à la grille activé (S)") : tr("Accrochage grille désactivé (S)"));
        }
    }
}

void MainWindow::onToggleObjectSnap(bool checked)
{
    if (m_occView)
    {
        m_occView->setObjectSnapEnabled(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Accrochage intelligent aux objets (OSNAP) activé (F3)")
                                          : tr("Accrochage objets (OSNAP) désactivé (F3)"));
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

void MainWindow::onToggleNodesVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setNodesVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Nœuds structurels affichés en 3D") : tr("Nœuds structurels masqués"));
        }
    }
}

void MainWindow::onToggleNodeLabelsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setNodeLabelsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Numéros et étiquettes de nœuds affichés en 3D") : tr("Numéros de nœuds masqués"));
        }
    }
}

void MainWindow::onToggleSupportsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setSupportsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Appuis structuraux affichés en 3D") : tr("Appuis structuraux masqués"));
        }
    }
}

void MainWindow::onToggleSupportLabelsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setSupportLabelsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Étiquettes des appuis affichées en 3D") : tr("Étiquettes des appuis masquées"));
        }
    }
}

void MainWindow::onToggleLoadsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setLoadsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Charges & actions affichées en 3D") : tr("Charges masquées"));
        }
    }
}

void MainWindow::onToggleForcesVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setForcesVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Forces 3D affichées") : tr("Forces 3D masquées"));
        }
    }
}

void MainWindow::onToggleMomentsVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setMomentsVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Moments 3D affichés") : tr("Moments 3D masqués"));
        }
    }
}

void MainWindow::onToggleLoadValuesVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setLoadValuesVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Valeurs des charges affichées en 3D") : tr("Valeurs des charges masquées"));
        }
    }
}

void MainWindow::onActionNewNode()
{
    TSA::UI::NewNodeDialog dlg(m_model, m_occView, m_selectionManager.get(), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        int newId = dlg.createdNodeId();
        if (m_statusInfo && newId > 0 && m_model)
        {
            const auto* n = m_model->getNode(newId);
            if (n)
            {
                m_statusInfo->setText(tr("Nœud N%1 créé à (%2, %3, %4)")
                    .arg(newId).arg(n->x(), 0, 'f', 2).arg(n->y(), 0, 'f', 2).arg(n->z(), 0, 'f', 2));
            }
        }
    }
}

void MainWindow::onActionNewBeam()
{
    openBarCreationDialog(TSA::Model::BarRole::Beam);
}

void MainWindow::onActionNewColumn()
{
    openBarCreationDialog(TSA::Model::BarRole::Column);
}

void MainWindow::onActionNewSlab()
{
    openSurfaceCreationDialog(0);
}

void MainWindow::onActionMove()
{
    startModelingTool("move", m_toolInputInViewport);   // « numérique » : toujours par fenêtre
}

void MainWindow::onActionCopy()
{
    startModelingTool("copy", m_toolInputInViewport);   // « numérique » : toujours par fenêtre
}

void MainWindow::onActionDeleteSelected()
{
    if (!m_selectionManager || !m_model)
        return;

    size_t total = m_selectionManager->totalSelectedCount();
    if (total == 0)
        return;

    auto cmd = std::make_unique<TSA::Commands::DeleteElementsCommand>(
        *m_model,
        m_selectionManager->selectedNodes(),
        m_selectionManager->selectedBeams(),
        m_selectionManager->selectedColumns(),
        m_selectionManager->selectedSlabs(),
        m_selectionManager->selectedWalls(),
        m_selectionManager->selectedFoundations(),
        m_selectionManager->selectedTrussMembers(),
        m_selectionManager->selectedCables()
    );

    if (m_commandManager)
    {
        m_commandManager->executeCommand(std::move(cmd));
    }
    else
    {
        cmd->execute();
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

void MainWindow::onActionSelectAll()
{
    if (!m_model)
        return;
    applyElementSelection(TSA::Model::SelectionQuery::all(*m_model), tr("Tout sélectionné"));
}

void MainWindow::applyElementSelection(const TSA::Model::ElementSet& elements, const QString& description)
{
    if (!m_selectionManager)
        return;
    // Une seule mise à jour : auparavant « Tout sélectionner » appelait selectX() par élément,
    // chacun émettant ses signaux et redessinant le viewport (gel sur les grands modèles), et
    // seul le dernier élément restait en surbrillance.
    m_selectionManager->selectElements(elements);
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("%1 : %2 élément(s)").arg(description).arg(m_selectionManager->totalSelectedCount()));
    }
}


void MainWindow::onActionAddCube()
{
    if (!m_model)
        return;

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
}

void MainWindow::onFitSelection()
{
    if (m_occView) m_occView->fitSelection();
}

void MainWindow::onZoomIn()
{
    if (m_occView) m_occView->zoomIn(1.25);
}

void MainWindow::onZoomOut()
{
    if (m_occView) m_occView->zoomOut(1.25);
}

void MainWindow::onZoomWindow()
{
    if (m_occView) m_occView->startInteractiveZoomWindow();
}

void MainWindow::onPreviousView()
{
    if (m_occView) m_occView->previousView();
}

void MainWindow::onNextView()
{
    if (m_occView) m_occView->nextView();
}

void MainWindow::onActionViewHome()
{
    if (m_occView) m_occView->viewHome();
    if (m_statusView) m_statusView->setText(tr("Vue ISO"));
}

void MainWindow::onActionViewTop()
{
    if (m_occView) m_occView->viewTop();
    if (m_statusView) m_statusView->setText(tr("Vue Dessus (XY)"));
}

void MainWindow::onActionViewBottom()
{
    if (m_occView) m_occView->viewBottom();
    if (m_statusView) m_statusView->setText(tr("Vue Dessous"));
}

void MainWindow::onActionViewFront()
{
    if (m_occView) m_occView->viewFront();
    if (m_statusView) m_statusView->setText(tr("Vue Face (XZ)"));
}

void MainWindow::onActionViewBack()
{
    if (m_occView) m_occView->viewBack();
    if (m_statusView) m_statusView->setText(tr("Vue Arrière"));
}

void MainWindow::onActionViewLeft()
{
    if (m_occView) m_occView->viewLeft();
    if (m_statusView) m_statusView->setText(tr("Vue Gauche (YZ)"));
}

void MainWindow::onActionViewRight()
{
    if (m_occView) m_occView->viewRight();
    if (m_statusView) m_statusView->setText(tr("Vue Droite"));
}

void MainWindow::onActionViewIsometric()
{
    if (m_occView) m_occView->viewIsometric();
    if (m_statusView) m_statusView->setText(tr("Vue ISO"));
}

void MainWindow::onRotate2DLeft()
{
    if (m_occView) m_occView->rotate2D(-15.0);
}

void MainWindow::onRotate2DRight()
{
    if (m_occView) m_occView->rotate2D(15.0);
}

void MainWindow::onWorkPlaneXY()
{
    if (m_occView) m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalXY, 0.0);
}

void MainWindow::onWorkPlaneXZ()
{
    if (m_occView) m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalXZ, 0.0);
}

void MainWindow::onWorkPlaneYZ()
{
    if (m_occView) m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalYZ, 0.0);
}

void MainWindow::onWorkPlaneLevel()
{
    if (m_occView)
    {
        double currentZ = m_occView->activeLevelElevation();
        m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::ElevationZ, currentZ);
    }
}

void MainWindow::onActionWorkPlaneCustom()
{
    if (!m_occView)
        return;
    TSA::Coordinate::LevelManager* lm = (m_model && m_model->coordinateSystem()) ? m_model->coordinateSystem()->levelManager() : nullptr;
    TSA::UI::WorkPlaneDialog dlg(m_occView, lm, this);
    dlg.exec();
}

void MainWindow::onActionToggleWorkPlaneVisible(bool checked)
{
    if (m_occView)
    {
        m_occView->setWorkPlaneVisible(checked);
        if (m_statusInfo)
        {
            m_statusInfo->setText(checked ? tr("Plan de travail 3D affiché") : tr("Plan de travail 3D masqué"));
        }
    }
}

void MainWindow::onActionViewNormalToPlane()
{
    if (m_occView)
    {
        m_occView->viewNormalToWorkPlane();
        if (m_statusInfo)
        {
            m_statusInfo->setText(tr("Vue orientée perpendiculairement au plan de travail actif"));
        }
    }
}

void MainWindow::onWorkPlaneChanged(const TSA::Coordinate::WorkPlane& wp)
{
    // 1. Synchronisation de la coche exclusive dans m_workPlaneGroup
    if (m_actionWorkPlaneXY) m_actionWorkPlaneXY->setChecked(wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXY);
    if (m_actionWorkPlaneLevel) m_actionWorkPlaneLevel->setChecked(wp.type() == TSA::Coordinate::WorkPlaneType::ElevationZ);
    if (m_actionWorkPlaneXZ) m_actionWorkPlaneXZ->setChecked(wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXZ);
    if (m_actionWorkPlaneYZ) m_actionWorkPlaneYZ->setChecked(wp.type() == TSA::Coordinate::WorkPlaneType::GlobalYZ);

    // 2. Synchronisation de la barre d'état avec affichage précis
    if (m_statusWorkPlane)
    {
        QString desc;
        switch (wp.type())
        {
        case TSA::Coordinate::WorkPlaneType::GlobalXY:
            desc = tr("Plan: XY (Z = %1 m)").arg(wp.offset(), 0, 'f', 2);
            break;
        case TSA::Coordinate::WorkPlaneType::ElevationZ:
            desc = tr("Plan: Étage (Z = %1 m)").arg(wp.offset(), 0, 'f', 2);
            break;
        case TSA::Coordinate::WorkPlaneType::GlobalXZ:
            desc = tr("Plan: XZ Façade (Y = %1 m)").arg(wp.offset(), 0, 'f', 2);
            break;
        case TSA::Coordinate::WorkPlaneType::GlobalYZ:
            desc = tr("Plan: YZ Pignon (X = %1 m)").arg(wp.offset(), 0, 'f', 2);
            break;
        case TSA::Coordinate::WorkPlaneType::ThreePoints:
            desc = tr("Plan: Incliné 3P");
            break;
        default:
            desc = tr("Plan: %1").arg(QString::fromStdString(wp.name()));
            break;
        }
        m_statusWorkPlane->setText(desc);
    }

    // 3. Synchronisation avec le sélecteur de niveau du viewport si plan horizontal
    if (m_viewportContainer && m_occView && m_occView->syncWorkPlaneWithLevel() &&
        wp.isHorizontal())
    {
        m_viewportContainer->setActiveLevelElevation(wp.offset());
    }

    // 4. Synchronisation bidirectionnelle immédiate avec le panneau de propriétés et le gestionnaire
    if (m_propertyPanel && m_selectionManager && m_selectionManager->isWorkPlaneSelected())
    {
        m_propertyPanel->setWorkPlane(wp);
    }
    if (m_model && m_model->workPlaneManager())
    {
        m_model->workPlaneManager()->updateWorkPlane(wp);
    }

    // 5. Synchronisation avec le volet Projection & Vue
    if (m_projectionViewDock)
    {
        m_projectionViewDock->syncFromWorkPlane(wp);
        if (m_model && m_model->workPlaneManager())
        {
            m_projectionViewDock->updateWorkPlaneList(m_model->workPlaneManager());
        }
    }

    // 6. Log console
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Plan de travail actif : %1 (Origine: %2, %3, %4 m)")
            .arg(QString::fromStdString(wp.name()))
            .arg(wp.origin().X(), 0, 'f', 2)
            .arg(wp.origin().Y(), 0, 'f', 2)
            .arg(wp.origin().Z(), 0, 'f', 2), "INFO");
    }
}

void MainWindow::onActionViewXY()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXY);
    if (m_statusView) m_statusView->setText(tr("Plan XY"));
}

void MainWindow::onActionViewYZ()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanYZ);
    if (m_statusView) m_statusView->setText(tr("Plan YZ"));
}

void MainWindow::onActionViewXZ()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXZ);
    if (m_statusView) m_statusView->setText(tr("Plan XZ"));
}

void MainWindow::onActionView3D()
{
    if (m_occView)
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::Perspective3D);
    if (m_statusView) m_statusView->setText(tr("Vue 3D"));
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

void MainWindow::updateWindowTitle()
{
    if (m_projectManager)
    {
        bool modified = m_session->hasUnsavedChanges();
        m_projectManager->setModified(modified);
        setWindowTitle(m_projectManager->windowTitle());
    }
}

bool MainWindow::maybeSave()
{
    if (!m_session->hasUnsavedChanges())
        return true;

    const QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        tr("%1 - Enregistrer les modifications").arg(TSA::Product::name()),
        tr("Le projet actuel a été modifié.\nVoulez-vous enregistrer les modifications avant de continuer ?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (ret == QMessageBox::Save)
    {
        return saveFile(m_projectManager ? m_projectManager->currentFilePath() : QString());
    }
    else if (ret == QMessageBox::Cancel)
    {
        return false;
    }
    return true; // Discard
}

bool MainWindow::prepareToClose()
{
    if (m_analysisRunning)
    {
        QMessageBox::information(this, tr("Calcul en cours"), tr("Un calcul est en cours : annulez-le ou attendez sa fin avant de quitter."));
        return false;
    }
    if (!maybeSave())
        return false;
    capturePreview(true); // dernier état du modèle pour le Start Center
    if (m_windowManager)
    {
        m_windowManager->saveLayout();
    }
    return true;
}

void MainWindow::onActionNew()
{
    emit newProjectRequested(); // boîte « Nouveau projet » gérée par AppShell
}

bool MainWindow::createProject(const TSA::UI::NewProjectSettings& settings)
{
    resetWorkspace(settings.projectTemplate);
    if (m_projectManager) m_projectManager->setProjectName(settings.name);
    const QString path = settings.filePath();
    if (!saveFile(path))
        return false;

    const QString fileName = QFileInfo(path).fileName();
    if (m_modelTree) m_modelTree->setProjectName(fileName);
    if (m_statusProject) m_statusProject->setText(fileName);
    if (m_projectStatusWidget) m_projectStatusWidget->setProjectInfo(settings.name, path);
    if (m_consoleDock) m_consoleDock->appendLog(tr("Nouveau projet créé : %1").arg(path), "SYS");
    return true;
}

bool MainWindow::closeProject()
{
    if (m_analysisRunning)
    {
        QMessageBox::information(this, tr("Calcul en cours"), tr("Un calcul est en cours : annulez-le ou attendez sa fin avant de fermer le projet."));
        return false;
    }
    if (!maybeSave())
        return false;
    capturePreview(true); // dernier état du projet pour sa carte du Start Center
    resetWorkspace(TSA::UI::ProjectTemplate::GeneralStructure);
    if (m_consoleDock) m_consoleDock->appendLog(tr("Projet fermé."), "SYS");
    return true;
}

void MainWindow::resetWorkspace(TSA::UI::ProjectTemplate projectTemplate)
{
    if (m_occView)
        m_occView->setInteractionMode(OccView::InteractionMode::Select);

    if (m_projectManager && m_model)
    {
        m_projectManager->newProject(*m_model, m_gridManager);
    }
    else if (m_model)
    {
        m_model->clear();
        m_model->clearUndoRedo();
    }
    if (projectTemplate == TSA::UI::ProjectTemplate::Empty && m_gridManager)
        m_gridManager->clearAllGrids();
    restoreAnalysisContextFromModel();   // nouveau projet : réglages d'analyse par défaut

    // Résultats du projet précédent : jamais affichés sur un autre modèle.
    m_resultsModel.reset();
    emit resultsChanged();
    if (m_occView) m_occView->setResultsModel(nullptr);
    if (m_resultsDock) m_resultsDock->setResultsModel(nullptr);
    if (m_propertyPanel) m_propertyPanel->setResultsModel(nullptr);

    if (m_selectionManager)
        m_selectionManager->clearSelection();
    updateUndoRedoActions();

    if (m_projectStatusWidget)
    {
        m_projectStatusWidget->setModel(m_model);
        m_projectStatusWidget->setProjectInfo(tr("Nouveau Projet"), "");
        m_projectStatusWidget->refreshStatus();
    }

    if (m_occView)
    {
        m_occView->rebuildGrid();
        m_occView->viewIsometric();
    }

    if (m_viewportContainer && m_model && m_model->levelManager())
    {
        m_viewportContainer->updateLevelsList(
            m_model->levelManager()->elevationList(),
            m_model->levelManager()->levelNames()
        );
    }

    if (m_modelTree)
    {
        m_modelTree->setProjectName(tr("Nouveau projet") + TSA::Product::projectExtension());
        m_modelTree->refreshAll();
    }

    if (m_diagramWidget)
    {
        m_diagramWidget->setModel(m_model);
        m_diagramWidget->setResultsModel(nullptr);
    }
    if (m_ndcWidget)
    {
        m_ndcWidget->setModel(m_model);
        m_ndcWidget->setResultsModel(nullptr);
    }
    if (m_analysisDataDock) m_analysisDataDock->setResultsModel(nullptr);

    if (m_statusProject)
    {
        m_statusProject->setText(tr("Nouveau projet") + TSA::Product::projectExtension());
    }
    if (m_statusView)
    {
        m_statusView->setText(tr("Vue ISO"));
    }
    if (m_statusUnits)
    {
        m_statusUnits->setText(tr("kN, m"));
    }
    if (m_statusLevel)
    {
        m_statusLevel->setText(tr("Niveau : Tous"));
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Nouveau projet initialisé."), "SYS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Nouveau projet"));
    }
}

void MainWindow::onActionOpen()
{
    emit openProjectRequested(); // même parcours que depuis le Start Center (AppShell)
}

void MainWindow::onActionSave()
{
    saveFile(m_projectManager ? m_projectManager->currentFilePath() : QString());
}

void MainWindow::onActionSaveAs()
{
    saveFile(QString());
}

bool MainWindow::saveFile(const QString& path)
{
    QString targetPath = path;
    // Un fichier d'un format importé (ex. modèle .tsa ouvert dans TSALab) n'est jamais réécrit :
    // l'enregistrement passe toujours par un fichier au format natif du produit.
    if (!targetPath.isEmpty() && !TSA::Product::isNativeProjectFile(targetPath))
        targetPath.clear();
    if (targetPath.isEmpty())
    {
        QString defaultName = (m_projectManager && m_projectManager->hasFilePath())
            ? TSA::Product::withProjectExtension(m_projectManager->currentFilePath())
            : tr("Projet") + TSA::Product::projectExtension();
        targetPath = QFileDialog::getSaveFileName(
            this,
            tr("Enregistrer le projet %1").arg(TSA::Product::name()),
            defaultName,
            TSA::Product::saveFileFilter()
        );
        if (targetPath.isEmpty())
            return false;

        targetPath = TSA::Product::withProjectExtension(targetPath);
    }

    if (!m_model)
        return false;

    // Capture de la vue 3D pour la miniature Windows Explorer
    QImage thumbnail;
    if (m_occView)
    {
        thumbnail = m_occView->captureViewImage(640, 480); // viewport réel, 4:3
    }

    QString errorMsg;
    bool ok = m_projectManager ? m_projectManager->saveProject(targetPath, *m_model, m_gridManager, thumbnail, &errorMsg)
                               : false;
    if (!ok)
    {
        QMessageBox::critical(this, tr("Erreur de sauvegarde"),
            tr("Échec de l'enregistrement du projet %1 :\n%2").arg(TSA::Product::name(), errorMsg));
        return false;
    }

    updateWindowTitle();
    onProjectFileSaved(targetPath);

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Projet enregistré : %1").arg(targetPath), "SYS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Enregistré : %1").arg(QFileInfo(targetPath).fileName()));
    }
    return true;
}

bool MainWindow::loadFile(const QString& path)
{
    if (!m_model)
        return false;
    capturePreview(true); // dernier état du projet que l'on quitte

    QElapsedTimer loadTimer;
    loadTimer.start();

    QString errorMsg;
    bool ok = m_projectManager ? m_projectManager->openProject(path, *m_model, m_gridManager, &errorMsg)
                               : false;
    const qint64 openMs = loadTimer.elapsed();
    qint64 lastLap = openMs;
    std::string laps;
    if (!ok)
    {
        QMessageBox::critical(this, tr("Erreur de chargement"),
            tr("Échec de l'ouverture du projet :\n%1").arg(errorMsg));
        return false;
    }

    m_model->clearUndoRedo();
    restoreAnalysisContextFromModel();
    updateWindowTitle();

    if (m_selectionManager)
    {
        m_selectionManager->clearSelection();
    }

    QString fileName = QFileInfo(path).fileName();
    QString projName = m_projectManager ? m_projectManager->projectName() : QString();
    if (projName.isEmpty())
    {
        projName = QFileInfo(path).baseName();
    }

    if (m_projectStatusWidget)
    {
        m_projectStatusWidget->setModel(m_model);
        m_projectStatusWidget->setProjectInfo(projName, path);
        m_projectStatusWidget->refreshStatus();
    }

    laps += " | sélection+état=" + std::to_string(loadTimer.elapsed() - lastLap) + " ms"; lastLap = loadTimer.elapsed();
    if (m_occView)
    {
        m_occView->rebuildGrid();
        m_occView->fitModel();
    }

    laps += " | grilles+cadrage=" + std::to_string(loadTimer.elapsed() - lastLap) + " ms"; lastLap = loadTimer.elapsed();
    if (m_viewportContainer && m_model->levelManager())
    {
        m_viewportContainer->updateLevelsList(
            m_model->levelManager()->elevationList(),
            m_model->levelManager()->levelNames()
        );
    }

    laps += " | niveaux/plans=" + std::to_string(loadTimer.elapsed() - lastLap) + " ms"; lastLap = loadTimer.elapsed();
    if (m_modelTree)
    {
        m_modelTree->setProjectName(fileName);
        m_modelTree->refreshAll();
    }

    laps += " | arbre=" + std::to_string(loadTimer.elapsed() - lastLap) + " ms"; lastLap = loadTimer.elapsed();
    if (m_diagramWidget)
    {
        m_diagramWidget->setModel(m_model);
        m_diagramWidget->setResultsModel(nullptr);
    }
    if (m_ndcWidget)
    {
        m_ndcWidget->setModel(m_model);
        m_ndcWidget->setResultsModel(nullptr);
    }
    if (m_analysisDataDock) m_analysisDataDock->setResultsModel(nullptr);

    laps += " | diagrammes+NDC=" + std::to_string(loadTimer.elapsed() - lastLap) + " ms"; lastLap = loadTimer.elapsed();
    if (m_statusProject)
    {
        m_statusProject->setText(fileName);
    }
    if (m_statusView)
    {
        m_statusView->setText(tr("Vue ISO"));
    }
    if (m_statusUnits)
    {
        m_statusUnits->setText(tr("kN, m"));
    }
    if (m_statusLevel)
    {
        m_statusLevel->setText(tr("Niveau : Tous"));
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Projet chargé avec succès : %1").arg(path), "SYS");
    }
    if (m_statusInfo)
    {
        m_statusInfo->setText(tr("Modèle : %1 nœuds, %2 poutres, %3 poteaux, %4 dalles | %5")
            .arg(m_model->nodes().size())
            .arg(m_model->beams().size())
            .arg(m_model->columns().size())
            .arg(m_model->slabs().size())
            .arg(fileName));
    }

    onProjectFileOpened(path);

    TSA_LOG_INFO("MainWindow", "ProjectLoadTiming",
                 "Ouverture " + fileName.toStdString() + " : lecture+modèle+3D = " + std::to_string(openMs) +
                 " ms, total (arbre, grilles, UI inclus) = " + std::to_string(loadTimer.elapsed()) + " ms" + laps + " (" +
                 std::to_string(m_model->nodes().size()) + " nœuds, " +
                 std::to_string(m_model->beams().size() + m_model->columns().size()) + " barres)");
    TSA::Diagnostics::Logger::instance().flush(); // événement ponctuel : rendre la mesure lisible immédiatement
    return true;
}

void MainWindow::onToggleTheme()
{
    TSA::UI::ThemeManager::instance().toggleTheme();
}

void MainWindow::applyTheme(bool dark)
{
    if (m_actionToggleTheme)
    {
        m_actionToggleTheme->setChecked(dark);
        m_actionToggleTheme->setIcon(makeThemeIcon(dark));
        m_actionToggleTheme->setText(dark ? tr("Mode Sombre (Actif)") : tr("Mode Clair (Actif)"));
    }

    if (m_viewportContainer)
    {
        m_viewportContainer->setDarkMode(dark);
    }

    if (m_occView)
    {
        m_occView->setDarkMode(dark);
    }

    if (m_projectStatusWidget)
    {
        m_projectStatusWidget->setDarkMode(dark);
    }

    if (m_statusLogo)
    {
        m_statusLogo->setDarkMode(dark);
    }

    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("Thème basculé : %1")
            .arg(dark ? tr("Mode Sombre AutoCAD") : tr("Mode Clair")), "SYS");
    }
}

void MainWindow::onActionHelp()
{
    if (!m_helpDialog)
    {
        m_helpDialog = new TSA::UI::HelpDialog(this);
    }
    m_helpDialog->selectTopic(0);
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

void MainWindow::onActionShortcuts()
{
    if (!m_helpDialog)
    {
        m_helpDialog = new TSA::UI::HelpDialog(this);
    }
    m_helpDialog->selectTopic(4); // Raccourcis & Console
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

void MainWindow::onActionAbout()
{
    if (!m_helpDialog)
    {
        m_helpDialog = new TSA::UI::HelpDialog(this);
    }
    m_helpDialog->selectTopic(6); // À Propos
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}


void MainWindow::onActionUndo()
{
    std::string actionName;
    if (m_session->undo(&actionName))
    {
        updateUndoRedoActions();
        statusBar()->showMessage(tr("Action annulée : %1 (Ctrl+Z)").arg(QString::fromStdString(actionName)), 3000);
    }
}

void MainWindow::onActionRedo()
{
    std::string actionName;
    if (m_session->redo(&actionName))
    {
        updateUndoRedoActions();
        statusBar()->showMessage(tr("Action rétablie : %1 (Ctrl+Y)").arg(QString::fromStdString(actionName)), 3000);
    }
}

void MainWindow::updateUndoRedoActions()
{
    if (!m_model) return;
    if (m_actionUndo)
    {
        bool canU = m_session->canUndo();
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
        bool canR = m_session->canRedo();
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
    updateWindowTitle();
}

void MainWindow::onToggleDarkMode(bool checked)
{
    if (TSA::UI::ThemeManager::instance().isDarkMode() != checked)
    {
        onToggleTheme();
    }
}

void MainWindow::onToggleFullScreen(bool checked)
{
    // La fenêtre est AppShell : c'est elle qui passe en plein écran (puis appelle setFullScreenState).
    emit fullScreenRequested(checked);
    if (statusBar())
        statusBar()->showMessage(checked ? tr("Mode plein écran activé (F11 pour quitter)") : tr("Mode fenêtre rétabli"), 3000);
}

void MainWindow::setFullScreenState(bool fullScreen)
{
    if (!m_actionFullScreen) return;
    if (m_actionFullScreen->isChecked() != fullScreen)
    {
        QSignalBlocker blocker(m_actionFullScreen);
        m_actionFullScreen->setChecked(fullScreen);
    }
    m_actionFullScreen->setText(fullScreen ? tr("&Quitter le plein écran") : tr("Mode &Plein écran"));
    m_actionFullScreen->setToolTip(fullScreen ? tr("Quitter le mode plein écran (F11)") : tr("Basculer en mode plein écran (F11)"));
}


void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl& url : event->mimeData()->urls())
        {
            if (TSA::Product::isOpenableProjectFile(url.toLocalFile()))
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl& url : event->mimeData()->urls())
        {
            QString path = url.toLocalFile();
            if (TSA::Product::isOpenableProjectFile(path))
            {
                event->acceptProposedAction();
                if (maybeSave())
                {
                    loadFile(path);
                }
                return;
            }
        }
    }
    QMainWindow::dropEvent(event);
}

void MainWindow::onActionExportDiagnosticReport()
{
    TSA_LOG_INFO("UI", "DiagnosticReportExportInitiated", "Export manuel du rapport de diagnostic demandé");
    std::string reportPath = TSA::Diagnostics::DiagnosticReport::exportReport(m_model);
    if (!reportPath.empty())
    {
        QMessageBox::information(this, tr("Rapport de Diagnostic %1").arg(TSA::Product::name()),
            tr("Le rapport de diagnostic a été exporté avec succès :\n\n%1").arg(QString::fromStdString(reportPath)));
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Rapport de diagnostic généré : %1").arg(QString::fromStdString(reportPath)), "INFO");
        }
    }
    else
    {
        QMessageBox::warning(this, tr("Erreur Diagnostic"),
            tr("Impossible de générer le rapport de diagnostic."));
    }
}

