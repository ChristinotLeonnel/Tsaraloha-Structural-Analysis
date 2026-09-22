#pragma once

#include <QMainWindow>
#include <memory>

namespace TSA::Model { class Model; }
namespace TSA::Viewer { class SelectionManager; }
namespace TSA::UI
{
    class ModelTreeWidget;
    class PropertyPanel;
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

private slots:
    void onFitAll();
    void onResetView();

    // Grille 3D
    void onGridCartesian();
    void onGridCylindrical();
    void onGridHide();
    void onToggleGridSnap(bool checked);
    void onGridSettings();

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

    OccView* m_occView = nullptr;
    TSA::UI::ModelTreeWidget* m_modelTree = nullptr;
    TSA::UI::PropertyPanel*   m_propertyPanel = nullptr;

    QDockWidget* m_modelTreeDock = nullptr;
    QDockWidget* m_propertiesDock = nullptr;

    QLabel*  m_statusCoordinates = nullptr;
    QLabel*  m_statusInfo = nullptr;

    QAction* m_actionFitAll = nullptr;
    QAction* m_actionResetView = nullptr;

    // Actions Grille
    QAction* m_actionGridCartesian = nullptr;
    QAction* m_actionGridCylindrical = nullptr;
    QAction* m_actionGridHide = nullptr;
    QAction* m_actionGridSnap = nullptr;
    QAction* m_actionGridSettings = nullptr;

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
    QAction* m_actionDelete = nullptr;

    QAction* m_actionMove = nullptr;
    QAction* m_actionCopy = nullptr;
};
