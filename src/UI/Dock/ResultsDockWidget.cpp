#include "ResultsDockWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Node.h"
#include "../../Model/Beam.h"
#include "../../Model/Column.h"
#include "../../Analysis/ResultsModel.h"
#include "../../Standards/Design/ConcreteDesignEC2.h"
#include "../../Standards/Design/SteelDesignEC3.h"

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
#include <QTimer>
#include <algorithm>
#include <cmath>

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
    // Nouveaux résultats : animation arrêtée, déformée complète affichée
    if (m_animTimer && m_animTimer->isActive()) setAnimationRunning(false);
    m_animTime = 0.0;
    emitAnimationPhase(1.0);
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
    m_labelEffectiveScale->setText(tr("Échelle effective : ×%1%2").arg(visualMgr->deformationScale(), 0, 'g', 5).arg(std::abs(visualMgr->deformationScale() - 1.0) < 1e-12 ? tr(" (non amplifiée)") : QString()));
    if (m_labelDiagramScale)
    {
        const QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(visualMgr->diagramType());
        if (visualMgr->diagramType() == TSA::Geometry::DiagramType::None) m_labelDiagramScale->setText(tr("Aucun diagramme affiché"));
        else if (visualMgr->diagramAllZero()) m_labelDiagramScale->setText(tr("Grandeur nulle sur toute la structure"));
        else
            m_labelDiagramScale->setText(tr("1 m de diagramme = %1 %2 (max %3 %2)").arg(1.0 / std::max(visualMgr->diagramScale(), 1e-300), 0, 'g', 4).arg(unit).arg(visualMgr->diagramMaxValue(), 0, 'g', 4));
        m_spinDiagramMultiplier->setValue(visualMgr->diagramScaleMultiplier());
    }

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
    m_chkSectionRotation->setChecked(visualMgr->isSectionRotationVisible());

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
    auto* grpStatus = new QGroupBox(tr("Statut et Synthèse"), container);
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
    auto* grpFamily = new QGroupBox(tr("Grandeur et Diagramme 3D"), container);
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
    m_comboFamily->addItem(tr("🧮 Contrainte normale σ = N/A ± M/W (MPa)"), static_cast<int>(TSA::Geometry::DiagramType::StressSigma));
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
    m_chkSectionRotation = new QCheckBox(tr("Rotation des sections (torsion)"), m_grpDeformedMode);
    m_chkSectionRotation->setChecked(true);
    m_chkSectionRotation->setToolTip(tr("Fait tourner les sections des poutres et poteaux de leur torsion θx calculée "
                                        "(amplifiée comme les déplacements) et trace des repères de section. "
                                        "Une torsion pure ne déplace pas l'axe de la barre : sans cette option, elle reste invisible."));
    connect(m_chkSectionRotation, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_updating) emit sectionRotationToggled(on);
    });
    vDef->addWidget(m_chkSectionRotation);
    rootLayout->addWidget(m_grpDeformedMode);

    // 4. Facteur d'Échelle 3D
    auto* grpScale = new QGroupBox(tr("Amplification de la déformée"), container);
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

    // 4 bis. Échelle des diagrammes : indépendante de la déformée, du zoom et des textes
    auto* grpDiag = new QGroupBox(tr("Échelle des diagrammes"), container);
    auto* formDiag = new QFormLayout(grpDiag);
    formDiag->setContentsMargins(8, 8, 8, 8);
    formDiag->setSpacing(6);
    m_comboDiagramScale = new QComboBox(grpDiag);
    m_comboDiagramScale->addItem(tr("Automatique"), 1.0);
    m_comboDiagramScale->addItem(tr("Automatique ×0,5"), 0.5);
    m_comboDiagramScale->addItem(tr("Automatique ×2"), 2.0);
    m_comboDiagramScale->addItem(tr("Automatique ×5"), 5.0);
    m_comboDiagramScale->addItem(tr("Personnalisé…"), 0.0);
    m_comboDiagramScale->setToolTip(tr("Hauteur des diagrammes à l'écran (valeurs calculées inchangées). Automatique : crête à 8 % de "
                                       "l'envergure, sans dépasser 60 % de la longueur médiane des barres."));
    formDiag->addRow(tr("Échelle :"), m_comboDiagramScale);
    m_spinDiagramMultiplier = new QDoubleSpinBox(grpDiag);
    m_spinDiagramMultiplier->setRange(0.01, 1000.0);
    m_spinDiagramMultiplier->setDecimals(2);
    m_spinDiagramMultiplier->setValue(1.0);
    m_spinDiagramMultiplier->setPrefix(QStringLiteral("× "));
    m_spinDiagramMultiplier->setEnabled(false);
    formDiag->addRow(tr("Facteur :"), m_spinDiagramMultiplier);
    m_labelDiagramScale = new QLabel(tr("1 m de diagramme = —"), grpDiag);
    m_labelDiagramScale->setStyleSheet("font-size: 11px; color: #3b82f6;");
    formDiag->addRow(m_labelDiagramScale);
    connect(m_comboDiagramScale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_spinDiagramMultiplier->setEnabled(m_comboDiagramScale->currentData().toDouble() <= 0.0);
        if (!m_updating) emitDiagramScale();
    });
    connect(m_spinDiagramMultiplier, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!m_updating && m_comboDiagramScale->currentData().toDouble() <= 0.0) emitDiagramScale();
    });
    rootLayout->addWidget(grpDiag);

    // Animation de la déformée : les résultats statiques déjà calculés sont réaffichés entre la géométrie
    // initiale et la déformée amplifiée ; ce n'est pas une analyse dynamique.
    auto* grpAnim = new QGroupBox(tr("Animation de la déformée"), container);
    auto* vAnim = new QVBoxLayout(grpAnim);
    vAnim->setContentsMargins(8, 8, 8, 8);
    vAnim->setSpacing(6);
    auto* hAnim = new QHBoxLayout();
    m_btnAnimPlay = new QPushButton(tr("▶ Lecture"), grpAnim);
    m_btnAnimPlay->setObjectName(QStringLiteral("ResultsAnimPlay"));
    m_btnAnimPlay->setCheckable(true);
    m_btnAnimReset = new QPushButton(tr("⏮ Début"), grpAnim);
    m_btnAnimReset->setObjectName(QStringLiteral("ResultsAnimReset"));
    m_comboAnimSpeed = new QComboBox(grpAnim);
    m_comboAnimSpeed->setObjectName(QStringLiteral("ResultsAnimSpeed"));
    m_comboAnimSpeed->addItem(tr("Vitesse ×0,5"), 0.5);
    m_comboAnimSpeed->addItem(tr("Vitesse ×1"), 1.0);
    m_comboAnimSpeed->addItem(tr("Vitesse ×2"), 2.0);
    m_comboAnimSpeed->setCurrentIndex(1);
    hAnim->addWidget(m_btnAnimPlay);
    hAnim->addWidget(m_btnAnimReset);
    hAnim->addWidget(m_comboAnimSpeed, 1);
    vAnim->addLayout(hAnim);
    m_labelAnim = new QLabel(tr("Interpolation visuelle entre la géométrie initiale et la déformée calculée (pas une analyse dynamique)."), grpAnim);
    m_labelAnim->setWordWrap(true);
    m_labelAnim->setStyleSheet("font-size: 11px; color: #94a3b8;");
    vAnim->addWidget(m_labelAnim);
    rootLayout->addWidget(grpAnim);
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(40);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        const double speed = m_comboAnimSpeed->currentData().toDouble();
        m_animTime += 0.04 * speed;
        // Aller-retour en 3 s à vitesse ×1 : 0 → 1 → 0 (cosinus, départ de la géométrie initiale)
        emitAnimationPhase(0.5 - 0.5 * std::cos(2.0 * M_PI * m_animTime / 3.0));
    });
    connect(m_btnAnimPlay, &QPushButton::toggled, this, &ResultsDockWidget::setAnimationRunning);
    connect(m_btnAnimReset, &QPushButton::clicked, this, [this]() {
        setAnimationRunning(false);
        m_animTime = 0.0;
        emitAnimationPhase(0.0);
    });

    // 5. Modes propres (analyse modale : un pas de résultats par mode)
    m_grpSteps = new QGroupBox(tr("Modes propres"), container);
    auto* vSteps = new QVBoxLayout(m_grpSteps);
    vSteps->setContentsMargins(8, 8, 8, 8);
    vSteps->setSpacing(6);

    m_labelStepInfo = new QLabel(tr("Résultat unique (statique linéaire)"), m_grpSteps);
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
    m_grpNodes = new QGroupBox(tr("Identification et Filtre des Nœuds"), container);
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

    const auto& modal = m_resultsModel->modal();
    if (m_resultsModel->analysisType() == TSA::Analysis::AnalysisType::Modal && !modal.empty())
    {
        m_labelStatus->setText(tr("✓ Analyse modale (%1 modes)").arg(modal.modes.size()));
        m_labelStatus->setStyleSheet("font-weight: bold; color: #10b981;");
        const bool kN = m_resultsModel->units().force == "kN";
        QString txt = tr("Masse totale : %1 %3 (libre %2 %3)\n").arg(modal.totalMass[2], 0, 'f', 3).arg(modal.totalFreeMass[2], 0, 'f', 3).arg(kN ? QStringLiteral("t") : QStringLiteral("kg"));
        txt += tr("Mode   f (Hz)    T (s)    MX %   MY %   MZ %\n");
        for (const auto& md : modal.modes)
            txt += QStringLiteral("%1  %2  %3  %4  %5  %6\n").arg(md.index, 4).arg(md.frequency, 8, 'f', 3).arg(md.period, 8, 'f', 4)
                       .arg(md.massRatio[0], 6, 'f', 1).arg(md.massRatio[1], 6, 'f', 1).arg(md.massRatio[2], 6, 'f', 1);
        const auto& last = modal.modes.back().cumulativeRatio;
        txt += tr("Cumul : MX %1 %, MY %2 %, MZ %3 %\n").arg(last[0], 0, 'f', 1).arg(last[1], 0, 'f', 1).arg(last[2], 0, 'f', 1);
        txt += tr("Formes propres normalisées (translation max = 1) ; masses : %1.").arg(QString::fromStdString(modal.massSource));
        m_labelSummary->setText(txt);
        return;
    }

    const auto& s = m_resultsModel->summary();
    const auto& eq = m_resultsModel->equilibrium();

    m_labelStatus->setText(tr("✓ Calcul disponible"));
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
    if (s.maxDisplacementNodeId <= 0)   // aucun nœud TSA calculé (modèle de surfaces seules) : pas de ligne « barres » vide
        txt = tr("• Réaction totale Rz : %1 kN\n• Équilibre : %2").arg(eq.reactionFz, 0, 'f', 1).arg(eq.isBalanced(0.05) ? tr("CONFORME") : tr("DÉSÉQUILIBRE"));
    if (!m_resultsModel->surfaceNodeDisplacements().empty())
    {
        int meshNode = 0;
        const double ws = m_resultsModel->maxSurfaceDisplacement(&meshNode);
        txt += tr("\n• Surfaces (coques) : déplacement max %1 mm (nœud %2 du maillage)").arg(ws * 1000.0, 0, 'f', 2).arg(meshNode);
    }
    if (s.maxTorsionMoment > 1e-9 || s.maxTwist > 1e-12)
        txt += tr("\n• Torsion max : Mx = %1 kNm (Barre #%2), θx = %3 mrad (Barre #%4)")
                   .arg(s.maxTorsionMoment, 0, 'f', 2).arg(s.maxTorsionMomentElementId)
                   .arg(s.maxTwist * 1000.0, 0, 'g', 4).arg(s.maxTwistElementId);

    // Évaluation des ratios de travail maximaux Eurocodes (EC2 & EC3)
    if (m_model)
    {
        double maxEtaEC2 = 0.0;
        int maxEtaEC2Id = 0;
        double maxEtaEC3 = 0.0;
        int maxEtaEC3Id = 0;
        // Efforts du calcul (kN ou N selon le moteur et ses options) convertis en SI pour EC2 / EC3.
        const double toN = m_resultsModel->units().forceToNewtons();

        for (const auto& [bId, beam] : m_model->beams())
        {
            const auto* elemRes = m_resultsModel->getElementResults(TSA::Analysis::StructuralElementKind::Beam, bId);
            if (!elemRes) continue;

            if (beam.material().type == TSA::Model::MaterialType::Concrete ||
                beam.material().type == TSA::Model::MaterialType::ReinforcedConcrete)
            {
                double Med = elemRes->maxAbsMy() * toN;   // axe fort de la section b × h (EC2, flexion simple)
                auto ec2 = TSA::Standards::Design::ConcreteDesignEC2::calculateFromModel(beam.section(), beam.material(), Med);
                if (ec2.valid && ec2.utilizationRatio > maxEtaEC2)
                {
                    maxEtaEC2 = ec2.utilizationRatio;
                    maxEtaEC2Id = bId;
                }
            }
            else if (beam.material().type == TSA::Model::MaterialType::Steel ||
                     beam.material().type == TSA::Model::MaterialType::GalvanizedSteel)
            {
                double Ned = std::max(std::abs(elemRes->minNormalForce()), std::abs(elemRes->maxNormalForce())) * toN;
                const auto* n1 = m_model->getNode(beam.startNodeId());
                const auto* n2 = m_model->getNode(beam.endNodeId());
                double L = 3.0;
                if (n1 && n2)
                {
                    double dx = n2->x() - n1->x(), dy = n2->y() - n1->y(), dz = n2->z() - n1->z();
                    L = std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                auto ec3 = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(beam.section(), beam.material(), L, 1.0, Ned, false);
                if (ec3.valid && ec3.utilizationRatio > maxEtaEC3)
                {
                    maxEtaEC3 = ec3.utilizationRatio;
                    maxEtaEC3Id = bId;
                }
            }
        }

        for (const auto& [colId, col] : m_model->columns())
        {
            const auto* elemRes = m_resultsModel->getElementResults(TSA::Analysis::StructuralElementKind::Column, colId);
            if (!elemRes) continue;

            if (col.material().type == TSA::Model::MaterialType::Steel ||
                col.material().type == TSA::Model::MaterialType::GalvanizedSteel)
            {
                double Ned = std::max(std::abs(elemRes->minNormalForce()), std::abs(elemRes->maxNormalForce())) * toN;
                const auto* n1 = m_model->getNode(col.startNodeId());
                const auto* n2 = m_model->getNode(col.endNodeId());
                double L = 3.0;
                if (n1 && n2)
                {
                    double dx = n2->x() - n1->x(), dy = n2->y() - n1->y(), dz = n2->z() - n1->z();
                    L = std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                auto ec3 = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(col.section(), col.material(), L, 1.0, Ned, false);
                if (ec3.valid && ec3.utilizationRatio > maxEtaEC3)
                {
                    maxEtaEC3 = ec3.utilizationRatio;
                    maxEtaEC3Id = colId;
                }
            }
        }

        if (maxEtaEC2 > 0.0)
        {
            txt += tr("\n• Ratio max EC2 (Béton) : %1% (#%2)")
                       .arg(maxEtaEC2 * 100.0, 0, 'f', 1)
                       .arg(maxEtaEC2Id);
        }
        if (maxEtaEC3 > 0.0)
        {
            txt += tr("\n• Ratio max EC3 (Acier) : %1% (#%2)")
                       .arg(maxEtaEC3 * 100.0, 0, 'f', 1)
                       .arg(maxEtaEC3Id);
        }
    }

    m_labelSummary->setText(txt);
}

void ResultsDockWidget::updateStepControls()
{
    if (!m_resultsModel || !m_resultsModel->hasResults() || m_resultsModel->stepCount() <= 1)
    {
        m_grpSteps->setEnabled(false);
        m_labelStepInfo->setText(tr("Résultat unique (statique linéaire)"));
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
    const bool modal = m_resultsModel->analysisType() == TSA::Analysis::AnalysisType::Modal && !m_resultsModel->modal().empty();
    int current = modal ? std::max(1, m_resultsModel->activeStep() + 1) : m_resultsModel->activeStep() + 1;

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

    if (modal)
    {
        const auto& md = m_resultsModel->modal().modes[std::size_t(std::clamp(current, 1, count) - 1)];
        m_labelStepInfo->setText(tr("Mode %1 / %2 : f = %3 Hz, T = %4 s").arg(current).arg(count).arg(md.frequency, 0, 'f', 3).arg(md.period, 0, 'f', 4));
        return;
    }
    // Seule l'analyse modale produit plusieurs jeux de résultats : aucun facteur de charge n'est déduit du rang.
    m_labelStepInfo->setText(tr("Résultat %1 / %2").arg(current).arg(count));
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
    emit deformationScalePresetChanged(preset, customVal);   // déformée seulement : les diagrammes ont leur échelle
}

void ResultsDockWidget::emitDiagramScale()
{
    const double preset = m_comboDiagramScale->currentData().toDouble();
    const double multiplier = preset > 0.0 ? preset : m_spinDiagramMultiplier->value();
    emit diagramScalePresetChanged(std::abs(multiplier - 1.0) < 1e-12 ? TSA::Viewer::ScalePreset::Auto : TSA::Viewer::ScalePreset::Custom, multiplier);
}

void ResultsDockWidget::onCustomScaleChanged(double val)
{
    if (m_updating) return;
    auto preset = static_cast<TSA::Viewer::ScalePreset>(m_comboScalePreset->currentData().toInt());
    if (preset == TSA::Viewer::ScalePreset::Custom) emit deformationScalePresetChanged(preset, val);
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

void ResultsDockWidget::setAnimationRunning(bool run)
{
    if (!m_animTimer) return;
    if (m_btnAnimPlay->isChecked() != run)
    {
        const QSignalBlocker block(m_btnAnimPlay);
        m_btnAnimPlay->setChecked(run);
    }
    m_btnAnimPlay->setText(run ? tr("⏸ Pause") : tr("▶ Lecture"));
    if (run) m_animTimer->start();
    else m_animTimer->stop();
}

void ResultsDockWidget::emitAnimationPhase(double phase)
{
    if (m_labelAnim)
        m_labelAnim->setText(tr("Affichage : %1 % de la déformée amplifiée. Interpolation visuelle de la solution statique (pas une analyse dynamique).")
                                 .arg(phase * 100.0, 0, 'f', 0));
    emit animationPhaseChanged(phase);
}

} // namespace TSA::UI
