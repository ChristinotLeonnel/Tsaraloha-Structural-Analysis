#pragma once

#include <QDockWidget>
#include <memory>

#include "../../Viewer/ResultsVisualManager.h"
#include "../../Viewer/OccView.h"
#include "../../Geometry/DiagramGeometry.h"

class QComboBox;
class QRadioButton;
class QCheckBox;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QGroupBox;
class QTextEdit;

namespace TSA::Model { class Model; }
namespace TSA::Analysis { class ResultsModel; }

namespace TSA::UI
{

/**
 * @brief Panneau professionnel d'inspection et de visualisation 3D des résultats structuraux.
 * Permet le pilotage complet de :
 * - Famille de résultats (Déformée, Moments M, Tranchant V, Normal N, Flèches U, Rotations R, Réactions)
 * - Facteurs d'échelle 3D (Auto, ×1, ×10, ×100, ×1000, ×10000, Personnalisé)
 * - Modes d'affichage déformée (Initial, Déformé, Initial + Déformé)
 * - Navigation multi-pas / incréments de calcul
 * - Filtrage et identification des nœuds (Tous, Libres, Appuis, Sélectionnés)
 * - Cadrage caméra adapté (Modèle, Résultats, Déformée, Sélection)
 * - Légende dynamique et bilan d'équilibre
 */
class ResultsDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit ResultsDockWidget(QWidget* parent = nullptr);
    ~ResultsDockWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
    void syncFromVisualManager(TSA::Viewer::ResultsVisualManager* visualMgr);

signals:
    void deformedToggled(bool visible);
    void deformedDisplayModeChanged(TSA::Viewer::DeformedDisplayMode mode);
    void deformationScalePresetChanged(TSA::Viewer::ScalePreset preset, double customVal);
    void diagramTypeChanged(TSA::Geometry::DiagramType type);
    void diagramScalePresetChanged(TSA::Viewer::ScalePreset preset, double customVal);
    void diagramLabelsToggled(bool visible);
    void reactionsToggled(bool visible);
    void activeStepChanged(int step);
    void legendToggled(bool visible);
    void nodesVisibleToggled(bool visible);
    void nodeLabelsToggled(bool visible);
    void nodeFilterChanged(OccView::NodeDisplayFilter filter);
    void fitModelRequested();
    void fitResultsRequested();
    void fitDeformedRequested();
    void fitSelectionRequested();

public slots:
    void refreshUi();

private slots:
    void onFamilyChanged(int index);
    void onDeformedModeChanged();
    void onScalePresetChanged(int index);
    void onCustomScaleChanged(double val);
    void onStepSliderChanged(int val);
    void onStepSpinChanged(int val);
    void onPrevStepClicked();
    void onNextStepClicked();
    void onNodeFilterChanged(int index);
    void onReactionsCheckChanged(bool checked);
    void onDiagramLabelsCheckChanged(bool checked);
    void onLegendCheckChanged(bool checked);
    void onNodesVisibleCheckChanged(bool checked);
    void onNodeLabelsCheckChanged(bool checked);

private:
    void setupUi();
    void updateSummaryText();
    void updateStepControls();
    void updateNodeStats();

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_resultsModel;
    bool m_updating = false;

    // Statut & Synthèse
    QLabel* m_labelStatus = nullptr;
    QLabel* m_labelSummary = nullptr;

    // Famille & Composante
    QComboBox* m_comboFamily = nullptr;
    QGroupBox* m_grpDeformedMode = nullptr;
    QRadioButton* m_radioInitialOnly = nullptr;
    QRadioButton* m_radioDeformedOnly = nullptr;
    QRadioButton* m_radioBoth = nullptr;

    // Facteur d'échelle
    QComboBox* m_comboScalePreset = nullptr;
    QDoubleSpinBox* m_spinCustomScale = nullptr;
    QLabel* m_labelEffectiveScale = nullptr;

    // Multi-pas
    QGroupBox* m_grpSteps = nullptr;
    QLabel* m_labelStepInfo = nullptr;
    QSlider* m_sliderStep = nullptr;
    QSpinBox* m_spinStep = nullptr;
    QPushButton* m_btnPrevStep = nullptr;
    QPushButton* m_btnNextStep = nullptr;

    // Options d'affichage
    QCheckBox* m_chkReactions = nullptr;
    QCheckBox* m_chkDiagramLabels = nullptr;
    QCheckBox* m_chkLegend = nullptr;

    // Identification et filtrage des nœuds
    QGroupBox* m_grpNodes = nullptr;
    QCheckBox* m_chkShowNodes = nullptr;
    QCheckBox* m_chkShowNodeLabels = nullptr;
    QComboBox* m_comboNodeFilter = nullptr;
    QLabel* m_labelNodeStats = nullptr;

    // Cadrage Caméra (Fit)
    QPushButton* m_btnFitModel = nullptr;
    QPushButton* m_btnFitResults = nullptr;
    QPushButton* m_btnFitDeformed = nullptr;
    QPushButton* m_btnFitSelection = nullptr;
};

} // namespace TSA::UI
