#pragma once

#include <QMainWindow>
#include <memory>

namespace TSA::Model { class Model; }
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

    // Modes d'interaction (Dessin 3D)
    void onModeSelect();
    void onModeDrawNode();
    void onModeDrawBeam();
    void onModeDrawColumn();
    void onModeDrawSlab();

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

private:
    void setupUi();
    void createMenus();
    void createToolBars();
    void createDockWindows();
    void createStatusBar();

private:
    std::unique_ptr<TSA::Model::Model> m_model;
    std::unique_ptr<TSA::Viewer::SelectionManager> m_selectionManager;
    std::unique_ptr<TSA::Grid::GridManager> m_gridManager;
    std::unique_ptr<TSA::Grid::GridSnapManager> m_gridSnapManager;

    OccView* m_occView = nullptr;
    TSA::UI::ViewportContainer* m_viewportContainer = nullptr;
    TSA::UI::ModelTreeWidget* m_modelTree = nullptr;
    TSA::UI::PropertyPanel*   m_propertyPanel = nullptr;

    QDockWidget* m_modelTreeDock = nullptr;
    QDockWidget* m_propertiesDock = nullptr;

    QLabel*  m_statusCoordinates = nullptr;
    QLabel*  m_statusInfo = nullptr;

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

    // Actions Modes d'interaction / Dessin 3D
    QActionGroup* m_drawModeGroup = nullptr;
    QAction* m_actionSelectMode = nullptr;
    QAction* m_actionDrawNode = nullptr;
    QAction* m_actionDrawBeam = nullptr;
    QAction* m_actionDrawColumn = nullptr;
    QAction* m_actionDrawSlab = nullptr;

    QAction* m_actionNewNode = nullptr;
    QAction* m_actionNewBeam = nullptr;
    QAction* m_actionNewColumn = nullptr;
    QAction* m_actionNewSlab = nullptr;
    QAction* m_actionAddCube = nullptr;
    QAction* m_actionDelete = nullptr;

    QAction* m_actionMove = nullptr;
    QAction* m_actionCopy = nullptr;

    // Actions Barre "Vue" (Robot SA style)
    QAction* m_actionViewXY = nullptr;
    QAction* m_actionViewYZ = nullptr;
    QAction* m_actionViewXZ = nullptr;
    QAction* m_actionView3D = nullptr;
    QAction* m_actionCoordSystem = nullptr;
    QAction* m_actionSectionCut = nullptr;

    TSA::UI::SectionCutDialog* m_sectionCutDialog = nullptr;

private slots:
    void onActionViewXY();
    void onActionViewYZ();
    void onActionViewXZ();
    void onActionView3D();
    void onActionCoordSystem();
    void onActionSectionCut();
};
