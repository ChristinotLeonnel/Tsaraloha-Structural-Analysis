#include "PropertyPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QPushButton>

namespace TSA::UI
{

PropertyPanel::PropertyPanel(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    clearProperties();
}

void PropertyPanel::setupMaterialCombo(QComboBox* combo)
{
    combo->clear();
    combo->addItem("Concrete C25/30", 1);
    combo->addItem("Concrete C30/37", 2);
    combo->addItem("Steel S235", 3);
    combo->addItem("Steel S355", 4);
    combo->addItem("Timber C24", 5);
}

void PropertyPanel::setupSectionTypeCombo(QComboBox* combo)
{
    combo->clear();
    combo->addItem(tr("Rectangulaire"), 0);
    combo->addItem(tr("Circulaire"), 1);
    combo->addItem("IPE 160", 160);
    combo->addItem("IPE 200", 200);
    combo->addItem("IPE 240", 240);
    combo->addItem("IPE 300", 300);
    combo->addItem("HEA 200", 2000);
    combo->addItem("HEA 240", 2400);
    combo->addItem("HEB 200", 2001);
    combo->addItem("HEB 300", 3001);
}

void PropertyPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(8);

    m_titleLabel = new QLabel(tr("PROPRIÉTÉS STRUCTURALES"), container);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 11pt; padding: 4px; background: rgba(0,0,0,0.1); border-radius: 3px;");
    containerLayout->addWidget(m_titleLabel);

    m_emptyLabel = new QLabel(tr("Aucun élément sélectionné.\nCliquez sur un élément dans le Viewport 3D ou dans l'Arbre du Modèle."), container);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: gray; padding: 20px; font-style: italic;");
    containerLayout->addWidget(m_emptyLabel);

    // ==========================================
    // 1. PANNEAU NŒUD
    // ==========================================
    m_nodeGroup = new QGroupBox(tr("Propriétés du Nœud"), container);
    auto* nodeForm = new QFormLayout(m_nodeGroup);
    m_nodeNameEdit = new QLineEdit(m_nodeGroup);
    m_nodeIdLabel = new QLabel(m_nodeGroup);
    m_nodeLevelLabel = new QLabel(m_nodeGroup);

    m_nodeXSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeXSpin->setRange(-10000.0, 10000.0);
    m_nodeXSpin->setDecimals(3);
    m_nodeXSpin->setSuffix(" m");

    m_nodeYSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeYSpin->setRange(-10000.0, 10000.0);
    m_nodeYSpin->setDecimals(3);
    m_nodeYSpin->setSuffix(" m");

    m_nodeZSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeZSpin->setRange(-10000.0, 10000.0);
    m_nodeZSpin->setDecimals(3);
    m_nodeZSpin->setSuffix(" m");

    m_nodeSupportCombo = new QComboBox(m_nodeGroup);
    m_nodeSupportCombo->addItem(tr("Libre (Aucun appui)"), static_cast<int>(TSA::Model::SupportType::Free));
    m_nodeSupportCombo->addItem(tr("Encastrement (6 DDL bloqués)"), static_cast<int>(TSA::Model::SupportType::Fixed));
    m_nodeSupportCombo->addItem(tr("Articulation / Rotule 3D"), static_cast<int>(TSA::Model::SupportType::Pinned));
    m_nodeSupportCombo->addItem(tr("Appui Simple (Rouleau Tz)"), static_cast<int>(TSA::Model::SupportType::Roller));

    nodeForm->addRow(tr("Nom / Repère :"), m_nodeNameEdit);
    nodeForm->addRow(tr("ID Interne :"), m_nodeIdLabel);
    nodeForm->addRow(tr("Niveau :"), m_nodeLevelLabel);
    nodeForm->addRow(tr("X (m) :"), m_nodeXSpin);
    nodeForm->addRow(tr("Y (m) :"), m_nodeYSpin);
    nodeForm->addRow(tr("Z (m) :"), m_nodeZSpin);
    nodeForm->addRow(tr("Liaison / Appui :"), m_nodeSupportCombo);

    auto* nodeBtnLayout = new QHBoxLayout();
    auto* btnApplyNode = new QPushButton(tr("Appliquer"), m_nodeGroup);
    btnApplyNode->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyNode, &QPushButton::clicked, this, &PropertyPanel::onApplyNode);
    auto* btnCancelNode = new QPushButton(tr("Annuler"), m_nodeGroup);
    connect(btnCancelNode, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    nodeBtnLayout->addWidget(btnApplyNode);
    nodeBtnLayout->addWidget(btnCancelNode);
    nodeForm->addRow(nodeBtnLayout);

    containerLayout->addWidget(m_nodeGroup);

    // ==========================================
    // 2. PANNEAU POUTRE
    // ==========================================
    m_beamGroup = new QGroupBox(tr("Propriétés de la Poutre"), container);
    auto* beamForm = new QFormLayout(m_beamGroup);
    m_beamNameEdit = new QLineEdit(m_beamGroup);
    m_beamIdLabel = new QLabel(m_beamGroup);
    m_beamStartNodeLabel = new QLabel(m_beamGroup);
    m_beamEndNodeLabel = new QLabel(m_beamGroup);
    m_beamLengthLabel = new QLabel(m_beamGroup);

    m_beamSectionTypeCombo = new QComboBox(m_beamGroup);
    setupSectionTypeCombo(m_beamSectionTypeCombo);

    m_beamWidthSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamWidthSpin->setRange(0.01, 10.0);
    m_beamWidthSpin->setSingleStep(0.05);
    m_beamWidthSpin->setSuffix(" m");

    m_beamHeightSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamHeightSpin->setRange(0.01, 10.0);
    m_beamHeightSpin->setSingleStep(0.05);
    m_beamHeightSpin->setSuffix(" m");

    m_beamMaterialCombo = new QComboBox(m_beamGroup);
    setupMaterialCombo(m_beamMaterialCombo);

    m_beamRotationSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamRotationSpin->setRange(0.0, 360.0);
    m_beamRotationSpin->setSingleStep(15.0);
    m_beamRotationSpin->setSuffix(" °");

    beamForm->addRow(tr("Nom / Repère :"), m_beamNameEdit);
    beamForm->addRow(tr("ID Interne :"), m_beamIdLabel);
    beamForm->addRow(tr("Nœud Départ :"), m_beamStartNodeLabel);
    beamForm->addRow(tr("Nœud Arrivée :"), m_beamEndNodeLabel);
    beamForm->addRow(tr("Longueur :"), m_beamLengthLabel);
    beamForm->addRow(tr("Section :"), m_beamSectionTypeCombo);
    beamForm->addRow(tr("Largeur b :"), m_beamWidthSpin);
    beamForm->addRow(tr("Hauteur h :"), m_beamHeightSpin);
    beamForm->addRow(tr("Matériau :"), m_beamMaterialCombo);
    beamForm->addRow(tr("Rotation β :"), m_beamRotationSpin);

    auto* beamBtnLayout = new QHBoxLayout();
    auto* btnApplyBeam = new QPushButton(tr("Appliquer"), m_beamGroup);
    btnApplyBeam->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyBeam, &QPushButton::clicked, this, &PropertyPanel::onApplyBeam);
    auto* btnCancelBeam = new QPushButton(tr("Annuler"), m_beamGroup);
    connect(btnCancelBeam, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    beamBtnLayout->addWidget(btnApplyBeam);
    beamBtnLayout->addWidget(btnCancelBeam);
    beamForm->addRow(beamBtnLayout);

    containerLayout->addWidget(m_beamGroup);

    // ==========================================
    // 3. PANNEAU POTEAU
    // ==========================================
    m_columnGroup = new QGroupBox(tr("Propriétés du Poteau"), container);
    auto* colForm = new QFormLayout(m_columnGroup);
    m_columnNameEdit = new QLineEdit(m_columnGroup);
    m_columnIdLabel = new QLabel(m_columnGroup);
    m_columnStartNodeLabel = new QLabel(m_columnGroup);
    m_columnEndNodeLabel = new QLabel(m_columnGroup);
    m_columnHeightLabel = new QLabel(m_columnGroup);

    m_columnSectionTypeCombo = new QComboBox(m_columnGroup);
    setupSectionTypeCombo(m_columnSectionTypeCombo);

    m_columnWidthSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnWidthSpin->setRange(0.01, 10.0);
    m_columnWidthSpin->setSingleStep(0.05);
    m_columnWidthSpin->setSuffix(" m");

    m_columnDepthSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnDepthSpin->setRange(0.01, 10.0);
    m_columnDepthSpin->setSingleStep(0.05);
    m_columnDepthSpin->setSuffix(" m");

    m_columnMaterialCombo = new QComboBox(m_columnGroup);
    setupMaterialCombo(m_columnMaterialCombo);

    m_columnRotationSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnRotationSpin->setRange(0.0, 360.0);
    m_columnRotationSpin->setSingleStep(15.0);
    m_columnRotationSpin->setSuffix(" °");

    colForm->addRow(tr("Nom / Repère :"), m_columnNameEdit);
    colForm->addRow(tr("ID Interne :"), m_columnIdLabel);
    colForm->addRow(tr("Nœud Base :"), m_columnStartNodeLabel);
    colForm->addRow(tr("Nœud Sommet :"), m_columnEndNodeLabel);
    colForm->addRow(tr("Hauteur :"), m_columnHeightLabel);
    colForm->addRow(tr("Forme Section :"), m_columnSectionTypeCombo);
    colForm->addRow(tr("Largeur b :"), m_columnWidthSpin);
    colForm->addRow(tr("Profondeur h :"), m_columnDepthSpin);
    colForm->addRow(tr("Matériau :"), m_columnMaterialCombo);
    colForm->addRow(tr("Rotation β :"), m_columnRotationSpin);

    auto* colBtnLayout = new QHBoxLayout();
    auto* btnApplyCol = new QPushButton(tr("Appliquer"), m_columnGroup);
    btnApplyCol->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyCol, &QPushButton::clicked, this, &PropertyPanel::onApplyColumn);
    auto* btnCancelCol = new QPushButton(tr("Annuler"), m_columnGroup);
    connect(btnCancelCol, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    colBtnLayout->addWidget(btnApplyCol);
    colBtnLayout->addWidget(btnCancelCol);
    colForm->addRow(colBtnLayout);

    containerLayout->addWidget(m_columnGroup);

    // ==========================================
    // 4. PANNEAU DALLE
    // ==========================================
    m_slabGroup = new QGroupBox(tr("Propriétés de la Dalle"), container);
    auto* slabForm = new QFormLayout(m_slabGroup);
    m_slabNameEdit = new QLineEdit(m_slabGroup);
    m_slabIdLabel = new QLabel(m_slabGroup);
    m_slabNodesLabel = new QLabel(m_slabGroup);
    m_slabAreaLabel = new QLabel(m_slabGroup);

    m_slabThicknessSpin = new QDoubleSpinBox(m_slabGroup);
    m_slabThicknessSpin->setRange(0.05, 2.0);
    m_slabThicknessSpin->setSingleStep(0.02);
    m_slabThicknessSpin->setSuffix(" m");

    m_slabMaterialCombo = new QComboBox(m_slabGroup);
    setupMaterialCombo(m_slabMaterialCombo);

    auto* typeLayout = new QVBoxLayout();
    m_slabRadioTwoWay = new QRadioButton(tr("Portance Bidirectionnelle (Two-way)"), m_slabGroup);
    m_slabRadioOneWay = new QRadioButton(tr("Portance Unidirectionnelle (One-way)"), m_slabGroup);
    m_slabRadioFlat   = new QRadioButton(tr("Plancher-Dalle (Flat slab)"), m_slabGroup);
    m_slabRadioTwoWay->setChecked(true);
    typeLayout->addWidget(m_slabRadioTwoWay);
    typeLayout->addWidget(m_slabRadioOneWay);
    typeLayout->addWidget(m_slabRadioFlat);

    slabForm->addRow(tr("Nom / Repère :"), m_slabNameEdit);
    slabForm->addRow(tr("ID Interne :"), m_slabIdLabel);
    slabForm->addRow(tr("Nœuds Contour :"), m_slabNodesLabel);
    slabForm->addRow(tr("Surface 3D :"), m_slabAreaLabel);
    slabForm->addRow(tr("Épaisseur e :"), m_slabThicknessSpin);
    slabForm->addRow(tr("Matériau :"), m_slabMaterialCombo);
    slabForm->addRow(tr("Typologie :"), typeLayout);

    auto* slabBtnLayout = new QHBoxLayout();
    auto* btnApplySlab = new QPushButton(tr("Appliquer"), m_slabGroup);
    btnApplySlab->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplySlab, &QPushButton::clicked, this, &PropertyPanel::onApplySlab);
    auto* btnCancelSlab = new QPushButton(tr("Annuler"), m_slabGroup);
    connect(btnCancelSlab, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    slabBtnLayout->addWidget(btnApplySlab);
    slabBtnLayout->addWidget(btnCancelSlab);
    slabForm->addRow(slabBtnLayout);

    containerLayout->addWidget(m_slabGroup);

    // ==========================================
    // 5. PANNEAU VOILE (WALL)
    // ==========================================
    m_wallGroup = new QGroupBox(tr("Propriétés du Voile"), container);
    auto* wallForm = new QFormLayout(m_wallGroup);
    m_wallNameEdit = new QLineEdit(m_wallGroup);
    m_wallIdLabel = new QLabel(m_wallGroup);
    m_wallStartNodeLabel = new QLabel(m_wallGroup);
    m_wallEndNodeLabel = new QLabel(m_wallGroup);
    m_wallLengthLabel = new QLabel(m_wallGroup);

    m_wallHeightSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallHeightSpin->setRange(0.5, 100.0);
    m_wallHeightSpin->setSingleStep(0.5);
    m_wallHeightSpin->setSuffix(" m");

    m_wallThicknessSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallThicknessSpin->setRange(0.05, 2.0);
    m_wallThicknessSpin->setSingleStep(0.05);
    m_wallThicknessSpin->setSuffix(" m");

    m_wallMaterialCombo = new QComboBox(m_wallGroup);
    setupMaterialCombo(m_wallMaterialCombo);

    m_wallOffsetSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallOffsetSpin->setRange(-10.0, 10.0);
    m_wallOffsetSpin->setSingleStep(0.05);
    m_wallOffsetSpin->setSuffix(" m");

    wallForm->addRow(tr("Nom / Repère :"), m_wallNameEdit);
    wallForm->addRow(tr("ID Interne :"), m_wallIdLabel);
    wallForm->addRow(tr("Nœud 1 :"), m_wallStartNodeLabel);
    wallForm->addRow(tr("Nœud 2 :"), m_wallEndNodeLabel);
    wallForm->addRow(tr("Longueur :"), m_wallLengthLabel);
    wallForm->addRow(tr("Hauteur H :"), m_wallHeightSpin);
    wallForm->addRow(tr("Épaisseur e :"), m_wallThicknessSpin);
    wallForm->addRow(tr("Matériau :"), m_wallMaterialCombo);
    wallForm->addRow(tr("Décalage :"), m_wallOffsetSpin);

    auto* wallBtnLayout = new QHBoxLayout();
    auto* btnApplyWall = new QPushButton(tr("Appliquer"), m_wallGroup);
    btnApplyWall->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyWall, &QPushButton::clicked, this, &PropertyPanel::onApplyWall);
    auto* btnCancelWall = new QPushButton(tr("Annuler"), m_wallGroup);
    connect(btnCancelWall, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    wallBtnLayout->addWidget(btnApplyWall);
    wallBtnLayout->addWidget(btnCancelWall);
    wallForm->addRow(wallBtnLayout);

    containerLayout->addWidget(m_wallGroup);

    // ==========================================
    // 6. PANNEAU FONDATION
    // ==========================================
    m_foundationGroup = new QGroupBox(tr("Propriétés de la Fondation"), container);
    auto* fForm = new QFormLayout(m_foundationGroup);
    m_foundationNameEdit = new QLineEdit(m_foundationGroup);
    m_foundationIdLabel = new QLabel(m_foundationGroup);
    m_foundationNodeLabel = new QLabel(m_foundationGroup);

    m_foundationTypeCombo = new QComboBox(m_foundationGroup);
    m_foundationTypeCombo->addItem(tr("Semelle Isolée (Poteau)"), static_cast<int>(TSA::Model::FoundationType::IsolatedFooting));
    m_foundationTypeCombo->addItem(tr("Semelle Filante (Voile)"), static_cast<int>(TSA::Model::FoundationType::StripFooting));
    m_foundationTypeCombo->addItem(tr("Radier Général"), static_cast<int>(TSA::Model::FoundationType::Raft));
    m_foundationTypeCombo->addItem(tr("Pieu"), static_cast<int>(TSA::Model::FoundationType::Pile));

    m_foundationWidthASpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationWidthASpin->setRange(0.2, 50.0);
    m_foundationWidthASpin->setSingleStep(0.1);
    m_foundationWidthASpin->setSuffix(" m");

    m_foundationLengthBSpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationLengthBSpin->setRange(0.2, 50.0);
    m_foundationLengthBSpin->setSingleStep(0.1);
    m_foundationLengthBSpin->setSuffix(" m");

    m_foundationHeightHSpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationHeightHSpin->setRange(0.1, 10.0);
    m_foundationHeightHSpin->setSingleStep(0.05);
    m_foundationHeightHSpin->setSuffix(" m");

    m_foundationMaterialCombo = new QComboBox(m_foundationGroup);
    setupMaterialCombo(m_foundationMaterialCombo);

    m_foundationSoilCapacitySpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationSoilCapacitySpin->setRange(10.0, 5000.0);
    m_foundationSoilCapacitySpin->setSingleStep(50.0);
    m_foundationSoilCapacitySpin->setSuffix(" kPa");

    fForm->addRow(tr("Nom / Repère :"), m_foundationNameEdit);
    fForm->addRow(tr("ID Interne :"), m_foundationIdLabel);
    fForm->addRow(tr("Nœud Support :"), m_foundationNodeLabel);
    fForm->addRow(tr("Type Fondation :"), m_foundationTypeCombo);
    fForm->addRow(tr("Largeur A :"), m_foundationWidthASpin);
    fForm->addRow(tr("Longueur B :"), m_foundationLengthBSpin);
    fForm->addRow(tr("Hauteur H :"), m_foundationHeightHSpin);
    fForm->addRow(tr("Matériau :"), m_foundationMaterialCombo);
    fForm->addRow(tr("Capacité Sol :"), m_foundationSoilCapacitySpin);

    auto* fBtnLayout = new QHBoxLayout();
    auto* btnApplyF = new QPushButton(tr("Appliquer"), m_foundationGroup);
    btnApplyF->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyF, &QPushButton::clicked, this, &PropertyPanel::onApplyFoundation);
    auto* btnCancelF = new QPushButton(tr("Annuler"), m_foundationGroup);
    connect(btnCancelF, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    fBtnLayout->addWidget(btnApplyF);
    fBtnLayout->addWidget(btnCancelF);
    fForm->addRow(fBtnLayout);

    containerLayout->addWidget(m_foundationGroup);

    // ==========================================
    // 7. PANNEAU TREILLIS / CONTREVENTEMENT
    // ==========================================
    m_trussGroup = new QGroupBox(tr("Propriétés de la Barre de Treillis"), container);
    auto* trForm = new QFormLayout(m_trussGroup);
    m_trussNameEdit = new QLineEdit(m_trussGroup);
    m_trussIdLabel = new QLabel(m_trussGroup);
    m_trussStartNodeLabel = new QLabel(m_trussGroup);
    m_trussEndNodeLabel = new QLabel(m_trussGroup);
    m_trussLengthLabel = new QLabel(m_trussGroup);

    m_trussRoleCombo = new QComboBox(m_trussGroup);
    m_trussRoleCombo->addItem(tr("Diagonale"), static_cast<int>(TSA::Model::TrussMemberRole::Diagonal));
    m_trussRoleCombo->addItem(tr("Membrure Supérieure"), static_cast<int>(TSA::Model::TrussMemberRole::TopChord));
    m_trussRoleCombo->addItem(tr("Membrure Inférieure"), static_cast<int>(TSA::Model::TrussMemberRole::BottomChord));
    m_trussRoleCombo->addItem(tr("Montant Vertical"), static_cast<int>(TSA::Model::TrussMemberRole::Vertical));
    m_trussRoleCombo->addItem(tr("Contreventement (Brace)"), static_cast<int>(TSA::Model::TrussMemberRole::Brace));

    m_trussDimensionSpin = new QDoubleSpinBox(m_trussGroup);
    m_trussDimensionSpin->setRange(0.02, 1.0);
    m_trussDimensionSpin->setSingleStep(0.01);
    m_trussDimensionSpin->setSuffix(" m");

    m_trussMaterialCombo = new QComboBox(m_trussGroup);
    setupMaterialCombo(m_trussMaterialCombo);

    trForm->addRow(tr("Nom / Repère :"), m_trussNameEdit);
    trForm->addRow(tr("ID Interne :"), m_trussIdLabel);
    trForm->addRow(tr("Nœud 1 :"), m_trussStartNodeLabel);
    trForm->addRow(tr("Nœud 2 :"), m_trussEndNodeLabel);
    trForm->addRow(tr("Longueur :"), m_trussLengthLabel);
    trForm->addRow(tr("Rôle :"), m_trussRoleCombo);
    trForm->addRow(tr("Diamètre / Section :"), m_trussDimensionSpin);
    trForm->addRow(tr("Matériau :"), m_trussMaterialCombo);

    auto* trBtnLayout = new QHBoxLayout();
    auto* btnApplyTr = new QPushButton(tr("Appliquer"), m_trussGroup);
    btnApplyTr->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 4px 10px;");
    connect(btnApplyTr, &QPushButton::clicked, this, &PropertyPanel::onApplyTruss);
    auto* btnCancelTr = new QPushButton(tr("Annuler"), m_trussGroup);
    connect(btnCancelTr, &QPushButton::clicked, this, &PropertyPanel::onCancelCurrent);
    trBtnLayout->addWidget(btnApplyTr);
    trBtnLayout->addWidget(btnCancelTr);
    trForm->addRow(trBtnLayout);

    containerLayout->addWidget(m_trussGroup);

    containerLayout->addStretch();
    scrollArea->setWidget(container);
    mainLayout->addWidget(scrollArea);
}

void PropertyPanel::hideAllGroups()
{
    m_nodeGroup->setVisible(false);
    m_beamGroup->setVisible(false);
    m_columnGroup->setVisible(false);
    m_slabGroup->setVisible(false);
    m_wallGroup->setVisible(false);
    m_foundationGroup->setVisible(false);
    m_trussGroup->setVisible(false);
}

void PropertyPanel::clearProperties()
{
    m_currentType = CurrentType::None;
    m_currentNodeId = -1;
    m_currentBeamId = -1;
    m_currentColumnId = -1;
    m_currentSlabId = -1;
    m_currentWallId = -1;
    m_currentFoundationId = -1;
    m_currentTrussId = -1;

    m_titleLabel->setText(tr("PROPRIÉTÉS STRUCTURALES"));
    m_emptyLabel->setVisible(true);
    hideAllGroups();
}

void PropertyPanel::showLevelProperties(const QString& levelId)
{
    clearProperties();
    m_currentType = CurrentType::Level;
    m_currentLevelId = levelId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NIVEAU : %1").arg(levelId));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    if (!m_model) return;
    const auto* node = m_model->getNode(nodeId);
    if (!node) return;

    clearProperties();
    m_currentType = CurrentType::Node;
    m_currentNodeId = nodeId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NŒUD"));

    m_nodeNameEdit->setText(QString::fromStdString(node->formattedName()));
    m_nodeIdLabel->setText(QString::number(node->id()));
    m_nodeLevelLabel->setText(node->levelId().empty() ? tr("Aucun") : QString::fromStdString(node->levelId()));
    m_nodeXSpin->setValue(node->x());
    m_nodeYSpin->setValue(node->y());
    m_nodeZSpin->setValue(node->z());

    int idx = m_nodeSupportCombo->findData(static_cast<int>(node->supportType()));
    if (idx >= 0) m_nodeSupportCombo->setCurrentIndex(idx);

    m_nodeGroup->setVisible(true);
}

void PropertyPanel::showBeamProperties(int beamId)
{
    if (!m_model) return;
    const auto* beam = m_model->getBeam(beamId);
    if (!beam) return;

    clearProperties();
    m_currentType = CurrentType::Beam;
    m_currentBeamId = beamId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA POUTRE"));

    m_beamNameEdit->setText(QString::fromStdString(beam->formattedName()));
    m_beamIdLabel->setText(QString::number(beam->id()));
    m_beamStartNodeLabel->setText(QString("N%1").arg(beam->startNodeId()));
    m_beamEndNodeLabel->setText(QString("N%1").arg(beam->endNodeId()));
    m_beamLengthLabel->setText(QString("%1 m").arg(beam->length(*m_model), 0, 'f', 2));
    m_beamWidthSpin->setValue(beam->width());
    m_beamHeightSpin->setValue(beam->height());
    m_beamRotationSpin->setValue(beam->rotation());

    m_beamGroup->setVisible(true);
}

void PropertyPanel::showColumnProperties(int columnId)
{
    if (!m_model) return;
    const auto* col = m_model->getColumn(columnId);
    if (!col) return;

    clearProperties();
    m_currentType = CurrentType::Column;
    m_currentColumnId = columnId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU POTEAU"));

    m_columnNameEdit->setText(QString::fromStdString(col->formattedName()));
    m_columnIdLabel->setText(QString::number(col->id()));
    m_columnStartNodeLabel->setText(QString("N%1").arg(col->startNodeId()));
    m_columnEndNodeLabel->setText(QString("N%1").arg(col->endNodeId()));
    m_columnHeightLabel->setText(QString("%1 m (%2)").arg(col->length(*m_model), 0, 'f', 2).arg(QString::fromStdString(col->direction(*m_model))));
    m_columnWidthSpin->setValue(col->width());
    m_columnDepthSpin->setValue(col->height());
    m_columnRotationSpin->setValue(col->rotation());

    m_columnGroup->setVisible(true);
}

void PropertyPanel::showSlabProperties(int slabId)
{
    if (!m_model) return;
    const auto* slab = m_model->getSlab(slabId);
    if (!slab) return;

    clearProperties();
    m_currentType = CurrentType::Slab;
    m_currentSlabId = slabId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA DALLE"));

    m_slabNameEdit->setText(QString::fromStdString(slab->formattedName()));
    m_slabIdLabel->setText(QString::number(slab->id()));

    QStringList nodeStrs;
    for (int nid : slab->nodeIds()) nodeStrs << QString("N%1").arg(nid);
    m_slabNodesLabel->setText(nodeStrs.join(" - "));
    m_slabAreaLabel->setText(QString("%1 m²").arg(slab->area(*m_model), 0, 'f', 2));
    m_slabThicknessSpin->setValue(slab->thickness());

    if (slab->slabType() == TSA::Model::SlabType::OneWay) m_slabRadioOneWay->setChecked(true);
    else if (slab->slabType() == TSA::Model::SlabType::FlatSlab) m_slabRadioFlat->setChecked(true);
    else m_slabRadioTwoWay->setChecked(true);

    m_slabGroup->setVisible(true);
}

void PropertyPanel::showWallProperties(int wallId)
{
    if (!m_model) return;
    const auto* wall = m_model->getWall(wallId);
    if (!wall) return;

    clearProperties();
    m_currentType = CurrentType::Wall;
    m_currentWallId = wallId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU VOILE"));

    m_wallNameEdit->setText(QString::fromStdString(wall->formattedName()));
    m_wallIdLabel->setText(QString::number(wall->id()));
    m_wallStartNodeLabel->setText(QString("N%1").arg(wall->startNodeId()));
    m_wallEndNodeLabel->setText(QString("N%1").arg(wall->endNodeId()));
    m_wallLengthLabel->setText(QString("%1 m (Aire = %2 m²)").arg(wall->length(*m_model), 0, 'f', 2).arg(wall->area(*m_model), 0, 'f', 2));
    m_wallHeightSpin->setValue(wall->height());
    m_wallThicknessSpin->setValue(wall->thickness());
    m_wallOffsetSpin->setValue(wall->offset());

    m_wallGroup->setVisible(true);
}

void PropertyPanel::showFoundationProperties(int foundationId)
{
    if (!m_model) return;
    const auto* f = m_model->getFoundation(foundationId);
    if (!f) return;

    clearProperties();
    m_currentType = CurrentType::Foundation;
    m_currentFoundationId = foundationId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA FONDATION"));

    m_foundationNameEdit->setText(QString::fromStdString(f->formattedName()));
    m_foundationIdLabel->setText(QString::number(f->id()));
    m_foundationNodeLabel->setText(QString("N%1").arg(f->nodeId()));

    int idx = m_foundationTypeCombo->findData(static_cast<int>(f->foundationType()));
    if (idx >= 0) m_foundationTypeCombo->setCurrentIndex(idx);

    m_foundationWidthASpin->setValue(f->widthA());
    m_foundationLengthBSpin->setValue(f->lengthB());
    m_foundationHeightHSpin->setValue(f->heightH());
    m_foundationSoilCapacitySpin->setValue(f->soilBearingCapacity());

    m_foundationGroup->setVisible(true);
}

void PropertyPanel::showTrussMemberProperties(int memberId)
{
    if (!m_model) return;
    const auto* truss = m_model->getTrussMember(memberId);
    if (!truss) return;

    clearProperties();
    m_currentType = CurrentType::Truss;
    m_currentTrussId = memberId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU TREILLIS"));

    m_trussNameEdit->setText(QString::fromStdString(truss->formattedName()));
    m_trussIdLabel->setText(QString::number(truss->id()));
    m_trussStartNodeLabel->setText(QString("N%1").arg(truss->startNodeId()));
    m_trussEndNodeLabel->setText(QString("N%1").arg(truss->endNodeId()));
    m_trussLengthLabel->setText(QString("%1 m").arg(truss->length(*m_model), 0, 'f', 2));

    int idx = m_trussRoleCombo->findData(static_cast<int>(truss->role()));
    if (idx >= 0) m_trussRoleCombo->setCurrentIndex(idx);

    m_trussDimensionSpin->setValue(truss->section().width);

    m_trussGroup->setVisible(true);
}

void PropertyPanel::onApplyNode()
{
    if (!m_model || m_currentNodeId < 0) return;
    auto* node = m_model->getNode(m_currentNodeId);
    if (!node) return;

    m_model->pushUndoState(tr("Modification Nœud %1").arg(m_currentNodeId).toStdString());

    node->setName(m_nodeNameEdit->text().toStdString());
    node->setCoordinates(m_nodeXSpin->value(), m_nodeYSpin->value(), m_nodeZSpin->value());
    node->setSupportType(static_cast<TSA::Model::SupportType>(m_nodeSupportCombo->currentData().toInt()));

    m_model->notifyNodeModified(m_currentNodeId);
    emit elementModified();
}

void PropertyPanel::onApplyBeam()
{
    if (!m_model || m_currentBeamId < 0) return;
    auto* beam = m_model->getBeam(m_currentBeamId);
    if (!beam) return;

    m_model->pushUndoState(tr("Modification Poutre %1").arg(m_currentBeamId).toStdString());

    beam->setName(m_beamNameEdit->text().toStdString());
    beam->setWidth(m_beamWidthSpin->value());
    beam->setHeight(m_beamHeightSpin->value());
    beam->setRotation(m_beamRotationSpin->value());

    m_model->notifyBeamModified(m_currentBeamId);
    emit elementModified();
}

void PropertyPanel::onApplyColumn()
{
    if (!m_model || m_currentColumnId < 0) return;
    auto* col = m_model->getColumn(m_currentColumnId);
    if (!col) return;

    m_model->pushUndoState(tr("Modification Poteau %1").arg(m_currentColumnId).toStdString());

    col->setName(m_columnNameEdit->text().toStdString());
    col->setWidth(m_columnWidthSpin->value());
    col->setHeight(m_columnDepthSpin->value());
    col->setRotation(m_columnRotationSpin->value());

    m_model->notifyColumnModified(m_currentColumnId);
    emit elementModified();
}

void PropertyPanel::onApplySlab()
{
    if (!m_model || m_currentSlabId < 0) return;
    auto* slab = m_model->getSlab(m_currentSlabId);
    if (!slab) return;

    m_model->pushUndoState(tr("Modification Dalle %1").arg(m_currentSlabId).toStdString());

    slab->setName(m_slabNameEdit->text().toStdString());
    slab->setThickness(m_slabThicknessSpin->value());

    if (m_slabRadioOneWay->isChecked()) slab->setSlabType(TSA::Model::SlabType::OneWay);
    else if (m_slabRadioFlat->isChecked()) slab->setSlabType(TSA::Model::SlabType::FlatSlab);
    else slab->setSlabType(TSA::Model::SlabType::TwoWay);

    m_model->notifySlabModified(m_currentSlabId);
    emit elementModified();
}

void PropertyPanel::onApplyWall()
{
    if (!m_model || m_currentWallId < 0) return;
    auto* wall = m_model->getWall(m_currentWallId);
    if (!wall) return;

    m_model->pushUndoState(tr("Modification Voile %1").arg(m_currentWallId).toStdString());

    wall->setName(m_wallNameEdit->text().toStdString());
    wall->setHeight(m_wallHeightSpin->value());
    wall->setThickness(m_wallThicknessSpin->value());
    wall->setOffset(m_wallOffsetSpin->value());

    m_model->notifyWallModified(m_currentWallId);
    emit elementModified();
}

void PropertyPanel::onApplyFoundation()
{
    if (!m_model || m_currentFoundationId < 0) return;
    auto* f = m_model->getFoundation(m_currentFoundationId);
    if (!f) return;

    m_model->pushUndoState(tr("Modification Fondation %1").arg(m_currentFoundationId).toStdString());

    f->setName(m_foundationNameEdit->text().toStdString());
    f->setFoundationType(static_cast<TSA::Model::FoundationType>(m_foundationTypeCombo->currentData().toInt()));
    f->setWidthA(m_foundationWidthASpin->value());
    f->setLengthB(m_foundationLengthBSpin->value());
    f->setHeightH(m_foundationHeightHSpin->value());
    f->setSoilBearingCapacity(m_foundationSoilCapacitySpin->value());

    m_model->notifyFoundationModified(m_currentFoundationId);
    emit elementModified();
}

void PropertyPanel::onApplyTruss()
{
    if (!m_model || m_currentTrussId < 0) return;
    auto* truss = m_model->getTrussMember(m_currentTrussId);
    if (!truss) return;

    m_model->pushUndoState(tr("Modification Treillis %1").arg(m_currentTrussId).toStdString());

    truss->setName(m_trussNameEdit->text().toStdString());
    truss->setRole(static_cast<TSA::Model::TrussMemberRole>(m_trussRoleCombo->currentData().toInt()));
    truss->section().width = m_trussDimensionSpin->value();
    truss->section().diameter = m_trussDimensionSpin->value();

    m_model->notifyTrussMemberModified(m_currentTrussId);
    emit elementModified();
}

void PropertyPanel::onCancelCurrent()
{
    switch (m_currentType)
    {
    case CurrentType::Node:       showNodeProperties(m_currentNodeId); break;
    case CurrentType::Beam:       showBeamProperties(m_currentBeamId); break;
    case CurrentType::Column:     showColumnProperties(m_currentColumnId); break;
    case CurrentType::Slab:       showSlabProperties(m_currentSlabId); break;
    case CurrentType::Wall:       showWallProperties(m_currentWallId); break;
    case CurrentType::Foundation: showFoundationProperties(m_currentFoundationId); break;
    case CurrentType::Truss:      showTrussMemberProperties(m_currentTrussId); break;
    default:                      clearProperties(); break;
    }
}

} // namespace TSA::UI
