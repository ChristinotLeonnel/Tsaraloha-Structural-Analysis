#include "ResultsDockWidget.h"
#include "../../Model/Model.h"
#include "../../Analysis/ResultsModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

namespace TSA::UI
{

ResultsDockWidget::ResultsDockWidget(QWidget* parent)
    : QDockWidget(tr("RÉSULTATS STRUCTURAUX 3D"), parent)
{
    setObjectName("ResultsDockWidget");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    setupUi();
    refreshUi();
}

void ResultsDockWidget::setModel(TSA::Model::Model* model)
{
    m_model = model;
    updateNodeStats();
}

void ResultsDockWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_resultsModel = results;
    refreshUi();
}

void ResultsDockWidget::syncFromVisualManager(TSA::Viewer::ResultsVisualManager* visualMgr)
{
    if (!visualMgr) return;
    m_updating = true;

    // Échelle
    int presetIdx = static_cast<int>(visualMgr->deformationScalePreset());
    if (presetIdx >= 0 && presetIdx < m_comboScalePreset->count())
    {
        m_comboScalePreset->setCurrentIndex(presetIdx);
    }
    m_spinCustomScale->setValue(visualMgr->deformationScale());
    m_labelEffectiveScale->setText(tr("Échelle effective : ×%1").arg(visualMgr->deformationScale(), 0, 'f', 1));

    // Mode d'affichage déformée
    switch (visualMgr->deformedDisplayMode())
    {
    case TSA::Viewer::DeformedDisplayMode::UndeformedOnly:
        m_radioInitialOnly->setChecked(true);
        break;
    case TSA::Viewer::DeformedDisplayMode::DeformedOnly:
        m_radioDeformedOnly->setChecked(true);
        break;
    case TSA::Viewer::DeformedDisplayMode::Both:
        m_radioBoth->setChecked(true);
        break;
    }

    // Réactions & Légende
    m_chkReactions->setChecked(visualMgr->areReactionsVisible());
    m_chkDiagramLabels->setChecked(visualMgr->areDiagramLabelsVisible());
    m_chkLegend->setChecked(visualMgr->isLegendVisible());

    m_updating = false;
}

void ResultsDockWidget::setupUi()
{
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* rootLayout = new QVBoxLayout(container);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // 1. Synthèse & Statut
    auto* grpStatus = new QGroupBox(tr("Statut & Synthèse"), container);
    auto* vStatus = new QVBoxLayout(grpStatus);
    vStatus->setContentsMargins(8, 8, 8, 8);
    vStatus->setSpacing(4);

    m_labelStatus = new QLabel(tr("Aucun résultat calculé"), grpStatus);
    m_labelStatus->setStyleSheet("font-weight: bold; color: #94a3b8;");
    vStatus->addWidget(m_labelStatus);

    m_labelSummary = new QLabel(grpStatus);
    m_labelSummary->setWordWrap(true);
    m_labelSummary->setStyleSheet("font-size: 11px; color: #64748b;");
    vStatus->addWidget(m_labelSummary);
    rootLayout->addWidget(grpStatus);

    // 2. Famille de Résultats & Diagrammes 3D
    auto* grpFamily = new QGroupBox(tr("Grandeur & Diagramme 3D"), container);
    auto* formFamily = new QFormLayout(grpFamily);
    formFamily->setContentsMargins(8, 8, 8, 8);
    formFamily->setSpacing(6);

    m_comboFamily = new QComboBox(grpFamily);
    m_comboFamily->addItem(tr("🌀 Déformée 3D"), static_cast<int>(TSA::Geometry::DiagramType::None));
    m_comboFamily->addItem(tr("📈 Moment fléchissant Mz (Principal)"), static_cast<int>(TSA::Geometry::DiagramType::BendingMz));
    m_comboFamily->addItem(tr("📈 Moment fléchissant My (Secondaire)"), static_cast<int>(TSA::Geometry::DiagramType::BendingMy));
    m_comboFamily->addItem(tr("🔄 Moment de torsion Mx"), static_cast<int>(TSA::Geometry::DiagramType::TorsionMx));
    m_comboFamily->addItem(tr("⚡ Effort tranchant Vz"), static_cast<int>(TSA::Geometry::DiagramType::ShearForceVz));
    m_comboFamily->addItem(tr("⚡ Effort tranchant Vy"), static_cast<int>(TSA::Geometry::DiagramType::ShearForceVy));
    m_comboFamily->addItem(tr("↔️ Effort normal N (Traction/Compr.)"), static_cast<int>(TSA::Geometry::DiagramType::AxialForceN));
    m_comboFamily->addItem(tr("➡️ Flèche UX"), static_cast<int>(TSA::Geometry::DiagramType::DeflectionUx));
    m_comboFamily->addItem(tr("↗️ Flèche UY"), static_cast<int>(TSA::Geometry::DiagramType::DeflectionUy));
    m_comboFamily->addItem(tr("⬇️ Flèche UZ (Verticale)"), static_cast<int>(TSA::Geometry::DiagramType::DeflectionUz));
    m_comboFamily->addItem(tr("🔀 Flèche résultante Ures"), static_cast<int>(TSA::Geometry::DiagramType::DeflectionUres));
    m_comboFamily->addItem(tr("🔄 Rotation RX"), static_cast<int>(TSA::Geometry::DiagramType::RotationRx));
    m_comboFamily->addItem(tr("🔄 Rotation RY"), static_cast<int>(TSA::Geometry::DiagramType::RotationRy));
    m_comboFamily->addItem(tr("🔄 Rotation RZ"), static_cast<int>(TSA::Geometry::DiagramType::RotationRz));
    m_comboFamily->addItem(tr("🚫 Aucun diagramme"), static_cast<int>(TSA::Geometry::DiagramType::None));
    connect(m_comboFamily, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultsDockWidget::onFamilyChanged);
    formFamily->addRow(tr("Affichage :"), m_comboFamily);

    // Options diagrammes
    m_chkDiagramLabels = new QCheckBox(tr("Afficher valeurs sur diagrammes"), grpFamily);
    m_chkDiagramLabels->setChecked(true);
    connect(m_chkDiagramLabels, &QCheckBox::toggled, this, &ResultsDockWidget::onDiagramLabelsCheckChanged);
    formFamily->addRow(m_chkDiagramLabels);

    m_chkReactions = new QCheckBox(tr("Afficher réactions d'appui 3D"), grpFamily);
    m_chkReactions->setChecked(true);
    connect(m_chkReactions, &QCheckBox::toggled, this, &ResultsDockWidget::onReactionsCheckChanged);
    formFamily->addRow(m_chkReactions);

    m_chkLegend = new QCheckBox(tr("Afficher légende à l'écran"), grpFamily);
    m_chkLegend->setChecked(true);
    connect(m_chkLegend, &QCheckBox::toggled, this, &ResultsDockWidget::onLegendCheckChanged);
    formFamily->addRow(m_chkLegend);

    rootLayout->addWidget(grpFamily);

    // 3. Mode d'affichage de la déformée
    m_grpDeformedMode = new QGroupBox(tr("Affichage Déformée 3D"), container);
    auto* vDef = new QVBoxLayout(m_grpDeformedMode);
    vDef->setContentsMargins(8, 8, 8, 8);
    vDef->setSpacing(4);

    m_radioBoth = new QRadioButton(tr("Initial + Déformé (Superposés)"), m_grpDeformedMode);
    m_radioDeformedOnly = new QRadioButton(tr("Déformé seul"), m_grpDeformedMode);
    m_radioInitialOnly = new QRadioButton(tr("Initial seul (Non déformé)"), m_grpDeformedMode);
    m_radioBoth->setChecked(true);

    auto* defGroup = new QButtonGroup(this);
    defGroup->addButton(m_radioBoth);
    defGroup->addButton(m_radioDeformedOnly);
    defGroup->addButton(m_radioInitialOnly);

    connect(m_radioBoth, &QRadioButton::toggled, this, &ResultsDockWidget::onDeformedModeChanged);
    connect(m_radioDeformedOnly, &QRadioButton::toggled, this, &ResultsDockWidget::onDeformedModeChanged);
    connect(m_radioInitialOnly, &QRadioButton::toggled, this, &ResultsDockWidget::onDeformedModeChanged);

    vDef->addWidget(m_radioBoth);
    vDef->addWidget(m_radioDeformedOnly);
    vDef->addWidget(m_radioInitialOnly);
    rootLayout->addWidget(m_grpDeformedMode);

    // 4. Facteur d'Échelle 3D
    auto* grpScale = new QGroupBox(tr("Amplification / Facteur d'Échelle"), container);
    auto* formScale = new QFormLayout(grpScale);
    formScale->setContentsMargins(8, 8, 8, 8);
    formScale->setSpacing(6);

    m_comboScalePreset = new QComboBox(grpScale);
    m_comboScalePreset->addItem(tr("Auto (Adapté au modèle)"), static_cast<int>(TSA::Viewer::ScalePreset::Auto));
    m_comboScalePreset->addItem(tr("×1 (Vraie échelle 1:1)"), static_cast<int>(TSA::Viewer::ScalePreset::X1));
    m_comboScalePreset->addItem(tr("×10"), static_cast<int>(TSA::Viewer::ScalePreset::X10));
    m_comboScalePreset->addItem(tr("×100"), static_cast<int>(TSA::Viewer::ScalePreset::X100));
    m_comboScalePreset->addItem(tr("×1 000"), static_cast<int>(TSA::Viewer::ScalePreset::X1000));
    m_comboScalePreset->addItem(tr("×10 000"), static_cast<int>(TSA::Viewer::ScalePreset::X10000));
    m_comboScalePreset->addItem(tr("Personnalisé..."), static_cast<int>(TSA::Viewer::ScalePreset::Custom));
    connect(m_comboScalePreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultsDockWidget::onScalePresetChanged);
    formScale->addRow(tr("Préréglage :"), m_comboScalePreset);

    m_spinCustomScale = new QDoubleSpinBox(grpScale);
    m_spinCustomScale->setRange(0.001, 1000000.0);
    m_spinCustomScale->setValue(100.0);
    m_spinCustomScale->setDecimals(2);
    m_spinCustomScale->setEnabled(false);
    connect(m_spinCustomScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ResultsDockWidget::onCustomScaleChanged);
    formScale->addRow(tr("Échelle manuelle :"), m_spinCustomScale);

    m_labelEffectiveScale = new QLabel(tr("Échelle effective : Auto"), grpScale);
    m_labelEffectiveScale->setStyleSheet("font-size: 11px; color: #3b82f6;");
    formScale->addRow(m_labelEffectiveScale);
    rootLayout->addWidget(grpScale);

    // 5. Multi-pas / Incréments (Non-linéaire & Dynamique)
    m_grpSteps = new QGroupBox(tr("Pas de Calcul / Incréments"), container);
    auto* vSteps = new QVBoxLayout(m_grpSteps);
    vSteps->setContentsMargins(8, 8, 8, 8);
    vSteps->setSpacing(6);

    m_labelStepInfo = new QLabel(tr("Pas 1 / 1 (Charge 100%)"), m_grpSteps);
    m_labelStepInfo->setStyleSheet("font-weight: bold; font-size: 11px;");
    vSteps->addWidget(m_labelStepInfo);

    auto* hStepCtrl = new QHBoxLayout();
    m_btnPrevStep = new QPushButton(tr("◀"), m_grpSteps);
    m_btnPrevStep->setFixedWidth(32);
    connect(m_btnPrevStep, &QPushButton::clicked, this, &ResultsDockWidget::onPrevStepClicked);

    m_sliderStep = new QSlider(Qt::Horizontal, m_grpSteps);
    m_sliderStep->setRange(1, 1);
    m_sliderStep->setValue(1);
    connect(m_sliderStep, &QSlider::valueChanged, this, &ResultsDockWidget::onStepSliderChanged);

    m_spinStep = new QSpinBox(m_grpSteps);
    m_spinStep->setRange(1, 1);
    m_spinStep->setValue(1);
    m_spinStep->setFixedWidth(50);
    connect(m_spinStep, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResultsDockWidget::onStepSpinChanged);

    m_btnNextStep = new QPushButton(tr("▶"), m_grpSteps);
    m_btnNextStep->setFixedWidth(32);
    connect(m_btnNextStep, &QPushButton::clicked, this, &ResultsDockWidget::onNextStepClicked);

    hStepCtrl->addWidget(m_btnPrevStep);
    hStepCtrl->addWidget(m_sliderStep);
    hStepCtrl->addWidget(m_spinStep);
    hStepCtrl->addWidget(m_btnNextStep);
    vSteps->addLayout(hStepCtrl);
    rootLayout->addWidget(m_grpSteps);

    // 6. Identification & Filtrage des Nœuds
    m_grpNodes = new QGroupBox(tr("Identification & Filtre des Nœuds"), container);
    auto* formNodes = new QFormLayout(m_grpNodes);
    formNodes->setContentsMargins(8, 8, 8, 8);
    formNodes->setSpacing(6);

    m_chkShowNodes = new QCheckBox(tr("Afficher les sphères des nœuds"), m_grpNodes);
    m_chkShowNodes->setChecked(true);
    connect(m_chkShowNodes, &QCheckBox::toggled, this, &ResultsDockWidget::onNodesVisibleCheckChanged);
    formNodes->addRow(m_chkShowNodes);

    m_chkShowNodeLabels = new QCheckBox(tr("Afficher numéros / labels des nœuds"), m_grpNodes);
    m_chkShowNodeLabels->setChecked(false);
    connect(m_chkShowNodeLabels, &QCheckBox::toggled, this, &ResultsDockWidget::onNodeLabelsCheckChanged);
    formNodes->addRow(m_chkShowNodeLabels);

    m_comboNodeFilter = new QComboBox(m_grpNodes);
    m_comboNodeFilter->addItem(tr("Tous les nœuds"), static_cast<int>(OccView::NodeDisplayFilter::All));
    m_comboNodeFilter->addItem(tr("⚠️ Nœuds libres uniquement"), static_cast<int>(OccView::NodeDisplayFilter::FreeOnly));
    m_comboNodeFilter->addItem(tr("⚓ Nœuds avec appuis uniquement"), static_cast<int>(OccView::NodeDisplayFilter::SupportedOnly));
    m_comboNodeFilter->addItem(tr("🔍 Nœuds sélectionnés uniquement"), static_cast<int>(OccView::NodeDisplayFilter::SelectedOnly));
    connect(m_comboNodeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultsDockWidget::onNodeFilterChanged);
    formNodes->addRow(tr("Filtrer :"), m_comboNodeFilter);

    m_labelNodeStats = new QLabel(m_grpNodes);
    m_labelNodeStats->setStyleSheet("font-size: 11px; color: #64748b;");
    formNodes->addRow(m_labelNodeStats);
    rootLayout->addWidget(m_grpNodes);

    // 7. Cadrage Caméra 3D (Fit)
    auto* grpFit = new QGroupBox(tr("Cadrage Caméra 3D (Fit)"), container);
    auto* gridFit = new QGridLayout(grpFit);
    gridFit->setContentsMargins(8, 8, 8, 8);
    gridFit->setSpacing(6);

    m_btnFitModel = new QPushButton(tr("🎯 Modèle"), grpFit);
    m_btnFitResults = new QPushButton(tr("📈 Résultats"), grpFit);
    m_btnFitDeformed = new QPushButton(tr("🌀 Déformée"), grpFit);
    m_btnFitSelection = new QPushButton(tr("🔍 Sélection"), grpFit);

    connect(m_btnFitModel, &QPushButton::clicked, this, &ResultsDockWidget::fitModelRequested);
    connect(m_btnFitResults, &QPushButton::clicked, this, &ResultsDockWidget::fitResultsRequested);
    connect(m_btnFitDeformed, &QPushButton::clicked, this, &ResultsDockWidget::fitDeformedRequested);
    connect(m_btnFitSelection, &QPushButton::clicked, this, &ResultsDockWidget::fitSelectionRequested);

    gridFit->addWidget(m_btnFitModel, 0, 0);
    gridFit->addWidget(m_btnFitResults, 0, 1);
    gridFit->addWidget(m_btnFitDeformed, 1, 0);
    gridFit->addWidget(m_btnFitSelection, 1, 1);
    rootLayout->addWidget(grpFit);

    rootLayout->addStretch();
    scrollArea->setWidget(container);
    setWidget(scrollArea);
}

void ResultsDockWidget::refreshUi()
{
    updateSummaryText();
    updateStepControls();
    updateNodeStats();
}

void ResultsDockWidget::updateSummaryText()
{
    if (!m_resultsModel || !m_resultsModel->hasResults())
    {
        m_labelStatus->setText(tr("Aucun résultat disponible"));
        m_labelStatus->setStyleSheet("font-weight: bold; color: #94a3b8;");
        m_labelSummary->setText(tr("Lancez un calcul (F5) pour inspecter la structure."));
        m_grpSteps->setEnabled(false);
        return;
    }

    const auto& s = m_resultsModel->summary();
    const auto& eq = m_resultsModel->equilibrium();

    m_labelStatus->setText(tr("✓ Calcul disponible (%1 pas)").arg(m_resultsModel->stepCount()));
    m_labelStatus->setStyleSheet("font-weight: bold; color: #10b981;");

    QString txt = tr(
        "• Flèche max : %1 mm (Nœud #%2)\n"
        "• Moment max : %3 kNm (Barre #%4)\n"
        "• Traction max : %5 kN\n"
        "• Réaction totale Rz : %6 kN\n"
        "• Équilibre : %7"
    )
    .arg(s.maxDisplacement * 1000.0, 0, 'f', 2)
    .arg(s.maxDisplacementNodeId)
    .arg(s.maxBendingMoment, 0, 'f', 1)
    .arg(s.maxBendingMomentElementId)
    .arg(s.maxTension, 0, 'f', 1)
    .arg(eq.reactionFz, 0, 'f', 1)
    .arg(eq.isBalanced(0.05) ? tr("CONFORME") : tr("DÉSÉQUILIBRE"));

    m_labelSummary->setText(txt);
}

void ResultsDockWidget::updateStepControls()
{
    if (!m_resultsModel || !m_resultsModel->hasResults() || m_resultsModel->stepCount() <= 1)
    {
        m_grpSteps->setEnabled(false);
        m_labelStepInfo->setText(tr("Pas unique (Statique linéaire)"));
        m_sliderStep->setRange(1, 1);
        m_sliderStep->setValue(1);
        m_spinStep->setRange(1, 1);
        m_spinStep->setValue(1);
        m_btnPrevStep->setEnabled(false);
        m_btnNextStep->setEnabled(false);
        return;
    }

    m_grpSteps->setEnabled(true);
    int count = m_resultsModel->stepCount();
    int current = m_resultsModel->activeStep() + 1;

    m_sliderStep->blockSignals(true);
    m_spinStep->blockSignals(true);
    m_sliderStep->setRange(1, count);
    m_spinStep->setRange(1, count);
    m_sliderStep->setValue(current);
    m_spinStep->setValue(current);
    m_sliderStep->blockSignals(false);
    m_spinStep->blockSignals(false);

    m_btnPrevStep->setEnabled(current > 1);
    m_btnNextStep->setEnabled(current < count);

    double lambda = (count > 1) ? (static_cast<double>(current) / static_cast<double>(count) * 100.0) : 100.0;
    m_labelStepInfo->setText(tr("Pas %1 / %2 (Facteur de charge λ = %3%)").arg(current).arg(count).arg(lambda, 0, 'f', 1));
}

void ResultsDockWidget::updateNodeStats()
{
    if (!m_model)
    {
        m_labelNodeStats->setText(tr("Modèle non initialisé"));
        return;
    }

    int totalNodes = static_cast<int>(m_model->nodes().size());
    int freeNodes = static_cast<int>(m_model->freeNodeIds().size());
    int supportedNodes = static_cast<int>(m_model->supportedNodeIds().size());

    m_labelNodeStats->setText(tr("Nœuds : %1 | Libres : %2 | Appuis : %3")
                                  .arg(totalNodes)
                                  .arg(freeNodes)
                                  .arg(supportedNodes));
}

void ResultsDockWidget::onFamilyChanged(int index)
{
    if (m_updating) return;
    auto type = static_cast<TSA::Geometry::DiagramType>(m_comboFamily->itemData(index).toInt());

    // Si on choisit Déformée seule (index 0), on n'affiche aucun diagramme
    if (index == 0)
    {
        emit deformedToggled(true);
        emit diagramTypeChanged(TSA::Geometry::DiagramType::None);
    }
    else
    {
        emit diagramTypeChanged(type);
    }
}

void ResultsDockWidget::onDeformedModeChanged()
{
    if (m_updating) return;
    if (m_radioInitialOnly->isChecked())
    {
        emit deformedDisplayModeChanged(TSA::Viewer::DeformedDisplayMode::UndeformedOnly);
    }
    else if (m_radioDeformedOnly->isChecked())
    {
        emit deformedDisplayModeChanged(TSA::Viewer::DeformedDisplayMode::DeformedOnly);
    }
    else
    {
        emit deformedDisplayModeChanged(TSA::Viewer::DeformedDisplayMode::Both);
    }
}

void ResultsDockWidget::onScalePresetChanged(int index)
{
    if (m_updating) return;
    auto preset = static_cast<TSA::Viewer::ScalePreset>(m_comboScalePreset->itemData(index).toInt());
    bool isCustom = (preset == TSA::Viewer::ScalePreset::Custom);
    m_spinCustomScale->setEnabled(isCustom);

    double customVal = m_spinCustomScale->value();
    emit deformationScalePresetChanged(preset, customVal);
    emit diagramScalePresetChanged(preset, customVal);
}

void ResultsDockWidget::onCustomScaleChanged(double val)
{
    if (m_updating) return;
    auto preset = static_cast<TSA::Viewer::ScalePreset>(m_comboScalePreset->currentData().toInt());
    if (preset == TSA::Viewer::ScalePreset::Custom)
    {
        emit deformationScalePresetChanged(preset, val);
        emit diagramScalePresetChanged(preset, val);
    }
}

void ResultsDockWidget::onStepSliderChanged(int val)
{
    if (m_updating) return;
    m_spinStep->blockSignals(true);
    m_spinStep->setValue(val);
    m_spinStep->blockSignals(false);

    int stepIdx = val - 1;
    if (m_resultsModel)
    {
        m_resultsModel->setActiveStep(stepIdx);
    }
    emit activeStepChanged(stepIdx);
    updateStepControls();
    updateSummaryText();
}

void ResultsDockWidget::onStepSpinChanged(int val)
{
    if (m_updating) return;
    m_sliderStep->blockSignals(true);
    m_sliderStep->setValue(val);
    m_sliderStep->blockSignals(false);

    int stepIdx = val - 1;
    if (m_resultsModel)
    {
        m_resultsModel->setActiveStep(stepIdx);
    }
    emit activeStepChanged(stepIdx);
    updateStepControls();
    updateSummaryText();
}

void ResultsDockWidget::onPrevStepClicked()
{
    int current = m_sliderStep->value();
    if (current > 1)
    {
        m_sliderStep->setValue(current - 1);
    }
}

void ResultsDockWidget::onNextStepClicked()
{
    int current = m_sliderStep->value();
    if (current < m_sliderStep->maximum())
    {
        m_sliderStep->setValue(current + 1);
    }
}

void ResultsDockWidget::onNodeFilterChanged(int index)
{
    if (m_updating) return;
    auto filter = static_cast<OccView::NodeDisplayFilter>(m_comboNodeFilter->itemData(index).toInt());
    emit nodeFilterChanged(filter);
}

void ResultsDockWidget::onReactionsCheckChanged(bool checked)
{
    if (m_updating) return;
    emit reactionsToggled(checked);
}

void ResultsDockWidget::onDiagramLabelsCheckChanged(bool checked)
{
    if (m_updating) return;
    emit diagramLabelsToggled(checked);
}

void ResultsDockWidget::onLegendCheckChanged(bool checked)
{
    if (m_updating) return;
    emit legendToggled(checked);
}

void ResultsDockWidget::onNodesVisibleCheckChanged(bool checked)
{
    if (m_updating) return;
    emit nodesVisibleToggled(checked);
}

void ResultsDockWidget::onNodeLabelsCheckChanged(bool checked)
{
    if (m_updating) return;
    emit nodeLabelsToggled(checked);
}

} // namespace TSA::UI
