#pragma once

#include <QDockWidget>
#include <memory>
#include "../../Viewer/ViewManager.h"
#include "../../Viewer/ProjectionManager.h"
#include "../../Coordinate/WorkPlane.h"

class QRadioButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QButtonGroup;

namespace TSA::Model
{
    class Model;
}

namespace TSA::Coordinate
{
    class WorkPlaneManager;
}

namespace TSA::UI
{

/**
 * @brief Panneau dédié "Projection & Vue" (Section 8).
 * Centralise :
 * - Le choix de la VUE (3D, Face, Arrière, Dessus, Dessous, Gauche, Droite, Normal au plan)
 * - Le mode de PROJECTION (3D vs 2D sur le plan de travail)
 * - Le contrôle du WORKPLANE (Plan actif, repère local Xwp/Ywp/Zwp, grille, visibilité, gizmo et taille)
 */
class ProjectionViewDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProjectionViewDock(QWidget* parent = nullptr);
    ~ProjectionViewDock() override = default;

    void setModel(TSA::Model::Model* model);
    void updateWorkPlaneList(const TSA::Coordinate::WorkPlaneManager* wpm);
    void syncFromWorkPlane(const TSA::Coordinate::WorkPlane& wp);
    void syncProjectionMode(TSA::Viewer::ProjectionMode mode);
    void syncCameraView(TSA::Viewer::StandardCameraView view, bool isOrtho);
    void setGizmoSize(double size);

signals:
    void standardViewRequested(TSA::Viewer::StandardCameraView view);
    void cameraOrthographicChanged(bool ortho);
    void projectionModeRequested(TSA::Viewer::ProjectionMode mode);
    void projectionDirectionRequested(TSA::Viewer::ProjectionDirection dir);
    void activeWorkPlaneSelected(int workPlaneId);
    void workPlaneVisibleToggled(bool visible);
    void workPlaneAxesVisibleToggled(bool visible);
    void workPlaneManipulatorToggled(bool active);
    void gizmoSizeChanged(double size);
    void alignViewToWorkPlaneRequested();
    void createNewWorkPlaneRequested();

private slots:
    void onViewRadioToggled();
    void onProjectionModeRadioToggled();
    void onDirectionComboChanged(int index);
    void onWorkPlaneComboChanged(int index);
    void onGizmoSliderChanged(int value);
    void onGizmoSpinChanged(double value);

private:
    void setupUi();

private:
    TSA::Model::Model* m_model = nullptr;
    bool m_isUpdating = false;

    // --- 1. Groupe VUE ---
    QButtonGroup* m_viewGroup = nullptr;
    QRadioButton* m_rbView3D = nullptr;
    QRadioButton* m_rbViewFront = nullptr;
    QRadioButton* m_rbViewBack = nullptr;
    QRadioButton* m_rbViewLeft = nullptr;
    QRadioButton* m_rbViewRight = nullptr;
    QRadioButton* m_rbViewTop = nullptr;
    QRadioButton* m_rbViewBottom = nullptr;
    QRadioButton* m_rbViewNormal = nullptr;
    QRadioButton* m_rbViewOpposite = nullptr;
    QCheckBox* m_chkOrtho = nullptr;

    // --- 2. Groupe PROJECTION ---
    QButtonGroup* m_projGroup = nullptr;
    QRadioButton* m_rbProj3D = nullptr;
    QRadioButton* m_rbProj2D = nullptr;
    QComboBox* m_comboDirection = nullptr;

    // --- 3. Groupe WORKPLANE ---
    QComboBox* m_comboWorkPlanes = nullptr;
    QLabel* m_lblPlaneStatus = nullptr;
    QLabel* m_lblOriginInfo = nullptr;
    QLabel* m_lblAxesInfo = nullptr;

    QCheckBox* m_chkPlaneVisible = nullptr;
    QCheckBox* m_chkAxesVisible = nullptr;
    QCheckBox* m_chkGridVisible = nullptr;

    // --- 4. GIZMO & MANIPULATION ---
    QCheckBox* m_chkGizmoActive = nullptr;
    QSlider* m_sliderGizmoSize = nullptr;
    QDoubleSpinBox* m_spinGizmoSize = nullptr;

    QPushButton* m_btnAlignView = nullptr;
    QPushButton* m_btnNewPlane = nullptr;
};

} // namespace TSA::UI
