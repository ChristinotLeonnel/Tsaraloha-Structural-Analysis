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
#include <QColorDialog>
#include <QCheckBox>
#include <QIcon>

namespace
{
struct LoadingGuard
{
    bool& flag;
    explicit LoadingGuard(bool& f) : flag(f) { flag = true; }
    ~LoadingGuard() { flag = false; }
};
}

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
    combo->addItem("IPE 100", 100);
    combo->addItem("IPE 160", 160);
    combo->addItem("IPE 200", 200);
    combo->addItem("IPE 240", 240);
    combo->addItem("IPE 300", 300);
    combo->addItem("HEA 160", 1600);
    combo->addItem("HEA 200", 2000);
    combo->addItem("HEA 240", 2400);
    combo->addItem("HEB 160", 1601);
    combo->addItem("HEB 200", 2001);
    combo->addItem("HEB 300", 3001);
    combo->addItem("UPN 100", 5100);
    combo->addItem("UPN 160", 5160);
    combo->addItem("UPN 200", 5200);
    combo->addItem("L 60x60x6", 6060);
    combo->addItem("L 80x80x8", 6080);
    combo->addItem("Tube 100x100x5", 7100);
    combo->addItem("Tube D114x5", 8114);
}

void PropertyPanel::setupColorButton(QPushButton* btn, const QString& hexColor)
{
    if (!btn) return;
    QColor c(hexColor);
    if (!c.isValid()) c = QColor("#007ACC");

    double luminance = (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
    QString textColor = luminance > 0.5 ? "#111111" : "#FFFFFF";

    btn->setText(c.name(QColor::HexRgb).toUpper());
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-weight: bold;"
        "  border: 1px solid #555555;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  text-align: center;"
        "}"
        "QPushButton:hover {"
        "  border: 1px solid #ffffff;"
        "}"
    ).arg(c.name(QColor::HexRgb)).arg(textColor));
}

void PropertyPanel::pickColor(QString& targetColor, QPushButton* targetBtn, const QString& title)
{
    QColor initial(targetColor);
    QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : Qt::white, this, title);
    if (chosen.isValid())
    {
        targetColor = chosen.name(QColor::HexRgb).toUpper();
        setupColorButton(targetBtn, targetColor);
        if (m_chkLiveSync && m_chkLiveSync->isChecked())
        {
            onWidgetChanged();
        }
    }
}

void PropertyPanel::updateBeamSectionVisibility(int secData)
{
    if (secData == 1) // Circulaire
    {
        if (m_beamWidthLabel) m_beamWidthLabel->setText(tr("Diamètre D :"));
        if (m_beamHeightLabel) m_beamHeightLabel->setVisible(false);
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(false);
    }
    else
    {
        if (m_beamWidthLabel) m_beamWidthLabel->setText(tr("Largeur b :"));
        if (m_beamHeightLabel)
        {
            m_beamHeightLabel->setText(tr("Hauteur h :"));
            m_beamHeightLabel->setVisible(true);
        }
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(true);
    }
}

void PropertyPanel::updateColumnSectionVisibility(int secData)
{
    if (secData == 1) // Circulaire
    {
        if (m_columnWidthLabel) m_columnWidthLabel->setText(tr("Diamètre D :"));
        if (m_columnDepthLabel) m_columnDepthLabel->setVisible(false);
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(false);
    }
    else
    {
        if (m_columnWidthLabel) m_columnWidthLabel->setText(tr("Largeur b :"));
        if (m_columnDepthLabel)
        {
            m_columnDepthLabel->setText(tr("Profondeur h :"));
            m_columnDepthLabel->setVisible(true);
        }
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(true);
    }
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

    m_chkLiveSync = new QCheckBox(tr("Synchronisation en direct (temps réel)"), container);
    m_chkLiveSync->setChecked(true);
    m_chkLiveSync->setToolTip(tr("Si coché, toute modification de valeur est répercutée immédiatement sur la structure 3D.\nSinon, effectuez vos modifications puis cliquez sur 'Appliquer les modifications'."));
    m_chkLiveSync->setStyleSheet("QCheckBox { font-weight: bold; color: #1E70BF; padding: 4px 6px; background: rgba(30,112,191,0.08); border-radius: 4px; }");
    containerLayout->addWidget(m_chkLiveSync);

    connect(m_chkLiveSync, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            onWidgetChanged();
        }
    });

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

    m_nodeColorBtn = new QPushButton(m_nodeGroup);
    m_nodeColor = "#FFD700";
    setupColorButton(m_nodeColorBtn, m_nodeColor);
    connect(m_nodeColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_nodeColor, m_nodeColorBtn, tr("Couleur du Nœud"));
    });

    nodeForm->addRow(tr("Nom / Repère :"), m_nodeNameEdit);
    nodeForm->addRow(tr("ID Interne :"), m_nodeIdLabel);
    nodeForm->addRow(tr("Niveau :"), m_nodeLevelLabel);
    nodeForm->addRow(tr("X (m) :"), m_nodeXSpin);
    nodeForm->addRow(tr("Y (m) :"), m_nodeYSpin);
    nodeForm->addRow(tr("Z (m) :"), m_nodeZSpin);
    nodeForm->addRow(tr("Liaison / Appui :"), m_nodeSupportCombo);
    nodeForm->addRow(tr("Couleur 3D :"), m_nodeColorBtn);

    auto* btnApplyNode = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_nodeGroup);
    btnApplyNode->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyNode, &QPushButton::clicked, this, &PropertyPanel::onApplyNode);
    nodeForm->addRow(btnApplyNode);

    connect(m_nodeNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeSupportCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

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

    m_beamRoleCombo = new QComboBox(m_beamGroup);
    m_beamRoleCombo->addItem(tr("Barre"), static_cast<int>(TSA::Model::BarRole::Generic));
    m_beamRoleCombo->addItem(tr("Poutre"), static_cast<int>(TSA::Model::BarRole::Beam));
    m_beamRoleCombo->addItem(tr("Poteau"), static_cast<int>(TSA::Model::BarRole::Column));
    m_beamRoleCombo->addItem(tr("Diagonale / Contreventement"), static_cast<int>(TSA::Model::BarRole::Brace));
    m_beamRoleCombo->addItem(tr("Tirant"), static_cast<int>(TSA::Model::BarRole::Tie));
    m_beamRoleCombo->addItem(tr("Barre acier"), static_cast<int>(TSA::Model::BarRole::SteelMember));
    m_beamRoleCombo->addItem(tr("Treillis"), static_cast<int>(TSA::Model::BarRole::Truss));

    m_beamSectionTypeCombo = new QComboBox(m_beamGroup);
    setupSectionTypeCombo(m_beamSectionTypeCombo);

    m_beamWidthLabel = new QLabel(tr("Largeur b :"), m_beamGroup);
    m_beamWidthSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamWidthSpin->setRange(0.01, 10.0);
    m_beamWidthSpin->setSingleStep(0.05);
    m_beamWidthSpin->setSuffix(" m");

    m_beamHeightLabel = new QLabel(tr("Hauteur h :"), m_beamGroup);
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

    m_beamEccentricityCombo = new QComboBox(m_beamGroup);
    m_beamEccentricityCombo->addItem(tr("inexistant"), static_cast<int>(TSA::Model::BarEccentricity::None));
    m_beamEccentricityCombo->addItem(tr("Fibre supérieure"), static_cast<int>(TSA::Model::BarEccentricity::TopFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre inférieure"), static_cast<int>(TSA::Model::BarEccentricity::BottomFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre gauche"), static_cast<int>(TSA::Model::BarEccentricity::LeftFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre droite"), static_cast<int>(TSA::Model::BarEccentricity::RightFlange));

    m_beamColorBtn = new QPushButton(m_beamGroup);
    m_beamColor = "#4682B4";
    setupColorButton(m_beamColorBtn, m_beamColor);
    connect(m_beamColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_beamColor, m_beamColorBtn, tr("Couleur de la Poutre"));
    });

    connect(m_beamSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        int secData = m_beamSectionTypeCombo->currentData().toInt();
        updateBeamSectionVisibility(secData);
        if (secData >= 100 && secData <= 400) {
            auto s = TSA::Model::Section::ipe(secData);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 1600 && secData <= 2400) {
            auto s = TSA::Model::Section::hea(secData / 10);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 1601 && secData <= 3001) {
            auto s = TSA::Model::Section::heb((secData - 1) / 10);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 5100 && secData <= 5300) {
            auto s = TSA::Model::Section::upn(secData - 5000);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 6060) {
            auto s = TSA::Model::Section::angle(0.060, 0.060, 0.006);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
        } else if (secData == 7100) {
            auto s = TSA::Model::Section::boxHollow(0.100, 0.100, 0.005);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
        } else if (secData == 8114) {
            auto s = TSA::Model::Section::pipe(0.114, 0.005);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
        }
    });

    beamForm->addRow(tr("Nom / Repère :"), m_beamNameEdit);
    beamForm->addRow(tr("ID Interne :"), m_beamIdLabel);
    beamForm->addRow(tr("Rôle / Type :"), m_beamRoleCombo);
    beamForm->addRow(tr("Nœud Départ :"), m_beamStartNodeLabel);
    beamForm->addRow(tr("Nœud Arrivée :"), m_beamEndNodeLabel);
    beamForm->addRow(tr("Longueur :"), m_beamLengthLabel);
    beamForm->addRow(tr("Section :"), m_beamSectionTypeCombo);
    beamForm->addRow(m_beamWidthLabel, m_beamWidthSpin);
    beamForm->addRow(m_beamHeightLabel, m_beamHeightSpin);
    beamForm->addRow(tr("Matériau :"), m_beamMaterialCombo);
    beamForm->addRow(tr("Rotation γ :"), m_beamRotationSpin);
    beamForm->addRow(tr("Excentrement :"), m_beamEccentricityCombo);
    beamForm->addRow(tr("Couleur 3D :"), m_beamColorBtn);

    auto* btnApplyBeam = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_beamGroup);
    btnApplyBeam->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyBeam, &QPushButton::clicked, this, &PropertyPanel::onApplyBeam);
    beamForm->addRow(btnApplyBeam);

    connect(m_beamNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_beamRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_beamEccentricityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

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

    m_columnWidthLabel = new QLabel(tr("Largeur b :"), m_columnGroup);
    m_columnWidthSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnWidthSpin->setRange(0.01, 10.0);
    m_columnWidthSpin->setSingleStep(0.05);
    m_columnWidthSpin->setSuffix(" m");

    m_columnDepthLabel = new QLabel(tr("Profondeur h :"), m_columnGroup);
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

    m_columnColorBtn = new QPushButton(m_columnGroup);
    m_columnColor = "#6A5ACD";
    setupColorButton(m_columnColorBtn, m_columnColor);
    connect(m_columnColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_columnColor, m_columnColorBtn, tr("Couleur du Poteau"));
    });

    connect(m_columnSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        int secData = m_columnSectionTypeCombo->currentData().toInt();
        updateColumnSectionVisibility(secData);
        if (secData >= 160 && secData <= 400) {
            auto s = TSA::Model::Section::ipe(secData);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 2000 || secData == 2400) {
            auto s = TSA::Model::Section::hea(secData / 10);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 2001 || secData == 3001) {
            auto s = TSA::Model::Section::heb((secData - 1) / 10);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        }
    });

    colForm->addRow(tr("Nom / Repère :"), m_columnNameEdit);
    colForm->addRow(tr("ID Interne :"), m_columnIdLabel);
    colForm->addRow(tr("Nœud Base :"), m_columnStartNodeLabel);
    colForm->addRow(tr("Nœud Sommet :"), m_columnEndNodeLabel);
    colForm->addRow(tr("Hauteur :"), m_columnHeightLabel);
    colForm->addRow(tr("Forme Section :"), m_columnSectionTypeCombo);
    colForm->addRow(m_columnWidthLabel, m_columnWidthSpin);
    colForm->addRow(m_columnDepthLabel, m_columnDepthSpin);
    colForm->addRow(tr("Matériau :"), m_columnMaterialCombo);
    colForm->addRow(tr("Rotation β :"), m_columnRotationSpin);
    colForm->addRow(tr("Couleur 3D :"), m_columnColorBtn);

    auto* btnApplyCol = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_columnGroup);
    btnApplyCol->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyCol, &QPushButton::clicked, this, &PropertyPanel::onApplyColumn);
    colForm->addRow(btnApplyCol);

    connect(m_columnNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_columnSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_columnWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_columnDepthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_columnMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_columnRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);

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

    m_slabColorBtn = new QPushButton(m_slabGroup);
    m_slabColor = "#B0C4DE";
    setupColorButton(m_slabColorBtn, m_slabColor);
    connect(m_slabColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_slabColor, m_slabColorBtn, tr("Couleur de la Dalle"));
    });

    slabForm->addRow(tr("Nom / Repère :"), m_slabNameEdit);
    slabForm->addRow(tr("ID Interne :"), m_slabIdLabel);
    slabForm->addRow(tr("Nœuds Contour :"), m_slabNodesLabel);
    slabForm->addRow(tr("Surface 3D :"), m_slabAreaLabel);
    slabForm->addRow(tr("Épaisseur e :"), m_slabThicknessSpin);
    slabForm->addRow(tr("Matériau :"), m_slabMaterialCombo);
    slabForm->addRow(tr("Typologie :"), typeLayout);
    slabForm->addRow(tr("Couleur 3D :"), m_slabColorBtn);

    auto* btnApplySlab = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_slabGroup);
    btnApplySlab->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplySlab, &QPushButton::clicked, this, &PropertyPanel::onApplySlab);
    slabForm->addRow(btnApplySlab);

    connect(m_slabNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabThicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_slabMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioOneWay, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioTwoWay, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioFlat, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);

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

    m_wallColorBtn = new QPushButton(m_wallGroup);
    m_wallColor = "#808080";
    setupColorButton(m_wallColorBtn, m_wallColor);
    connect(m_wallColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_wallColor, m_wallColorBtn, tr("Couleur du Voile"));
    });

    wallForm->addRow(tr("Nom / Repère :"), m_wallNameEdit);
    wallForm->addRow(tr("ID Interne :"), m_wallIdLabel);
    wallForm->addRow(tr("Nœud 1 :"), m_wallStartNodeLabel);
    wallForm->addRow(tr("Nœud 2 :"), m_wallEndNodeLabel);
    wallForm->addRow(tr("Longueur :"), m_wallLengthLabel);
    wallForm->addRow(tr("Hauteur H :"), m_wallHeightSpin);
    wallForm->addRow(tr("Épaisseur e :"), m_wallThicknessSpin);
    wallForm->addRow(tr("Matériau :"), m_wallMaterialCombo);
    wallForm->addRow(tr("Décalage :"), m_wallOffsetSpin);
    wallForm->addRow(tr("Couleur 3D :"), m_wallColorBtn);

    auto* btnApplyWall = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_wallGroup);
    btnApplyWall->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyWall, &QPushButton::clicked, this, &PropertyPanel::onApplyWall);
    wallForm->addRow(btnApplyWall);

    connect(m_wallNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_wallHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallThicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);

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

    m_foundationColorBtn = new QPushButton(m_foundationGroup);
    m_foundationColor = "#B8860B";
    setupColorButton(m_foundationColorBtn, m_foundationColor);
    connect(m_foundationColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_foundationColor, m_foundationColorBtn, tr("Couleur de la Fondation"));
    });

    fForm->addRow(tr("Nom / Repère :"), m_foundationNameEdit);
    fForm->addRow(tr("ID Interne :"), m_foundationIdLabel);
    fForm->addRow(tr("Nœud Support :"), m_foundationNodeLabel);
    fForm->addRow(tr("Type Fondation :"), m_foundationTypeCombo);
    fForm->addRow(tr("Largeur A :"), m_foundationWidthASpin);
    fForm->addRow(tr("Longueur B :"), m_foundationLengthBSpin);
    fForm->addRow(tr("Hauteur H :"), m_foundationHeightHSpin);
    fForm->addRow(tr("Matériau :"), m_foundationMaterialCombo);
    fForm->addRow(tr("Capacité Sol :"), m_foundationSoilCapacitySpin);
    fForm->addRow(tr("Couleur 3D :"), m_foundationColorBtn);

    auto* btnApplyF = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_foundationGroup);
    btnApplyF->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyF, &QPushButton::clicked, this, &PropertyPanel::onApplyFoundation);
    fForm->addRow(btnApplyF);

    connect(m_foundationNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationWidthASpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationLengthBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationHeightHSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationSoilCapacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);

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

    m_trussColorBtn = new QPushButton(m_trussGroup);
    m_trussColor = "#DAA520";
    setupColorButton(m_trussColorBtn, m_trussColor);
    connect(m_trussColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_trussColor, m_trussColorBtn, tr("Couleur du Treillis"));
    });

    trForm->addRow(tr("Nom / Repère :"), m_trussNameEdit);
    trForm->addRow(tr("ID Interne :"), m_trussIdLabel);
    trForm->addRow(tr("Nœud 1 :"), m_trussStartNodeLabel);
    trForm->addRow(tr("Nœud 2 :"), m_trussEndNodeLabel);
    trForm->addRow(tr("Longueur :"), m_trussLengthLabel);
    trForm->addRow(tr("Rôle :"), m_trussRoleCombo);
    trForm->addRow(tr("Diamètre / Section :"), m_trussDimensionSpin);
    trForm->addRow(tr("Matériau :"), m_trussMaterialCombo);
    trForm->addRow(tr("Couleur 3D :"), m_trussColorBtn);

    auto* btnApplyTr = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_trussGroup);
    btnApplyTr->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyTr, &QPushButton::clicked, this, &PropertyPanel::onApplyTruss);
    trForm->addRow(btnApplyTr);

    connect(m_trussNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_trussRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_trussDimensionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_trussMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

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
    m_currentLevelId.clear();
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
    LoadingGuard guard(m_isLoading);
    clearProperties();
    m_currentType = CurrentType::Level;
    m_currentLevelId = levelId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NIVEAU : %1").arg(levelId));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    LoadingGuard guard(m_isLoading);
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

    m_nodeColor = QString::fromStdString(node->color());
    setupColorButton(m_nodeColorBtn, m_nodeColor);

    m_nodeGroup->setVisible(true);
}

void PropertyPanel::showBeamProperties(int beamId)
{
    LoadingGuard guard(m_isLoading);
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

    int roleIdx = m_beamRoleCombo->findData(static_cast<int>(beam->role()));
    if (roleIdx >= 0) m_beamRoleCombo->setCurrentIndex(roleIdx);

    int eccIdx = m_beamEccentricityCombo->findData(static_cast<int>(beam->eccentricity()));
    if (eccIdx >= 0) m_beamEccentricityCombo->setCurrentIndex(eccIdx);

    m_beamSectionTypeCombo->blockSignals(true);
    const auto& sec = beam->section();
    int activeSecData = 0;
    if (sec.shape == TSA::Model::SectionShape::Circular)
    {
        activeSecData = 1;
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(1));
    }
    else if (sec.shape == TSA::Model::SectionShape::IShape)
    {
        int idx = m_beamSectionTypeCombo->findText(QString::fromStdString(sec.name));
        if (idx < 0)
        {
            int hMm = static_cast<int>(std::round(sec.height * 1000.0));
            idx = m_beamSectionTypeCombo->findData(hMm);
        }
        if (idx >= 0)
        {
            m_beamSectionTypeCombo->setCurrentIndex(idx);
            activeSecData = m_beamSectionTypeCombo->currentData().toInt();
        }
        else
        {
            m_beamSectionTypeCombo->setCurrentIndex(0);
            activeSecData = 0;
        }
    }
    else
    {
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(0));
        activeSecData = 0;
    }
    m_beamSectionTypeCombo->blockSignals(false);
    updateBeamSectionVisibility(activeSecData);

    int matCode = 1;
    if (beam->material().type == TSA::Model::MaterialType::Steel)
        matCode = (beam->material().fk > 300e6) ? 4 : 3;
    else if (beam->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (beam->material().fk > 28e6) ? 2 : 1;
    m_beamMaterialCombo->setCurrentIndex(m_beamMaterialCombo->findData(matCode));

    m_beamColor = QString::fromStdString(beam->color());
    setupColorButton(m_beamColorBtn, m_beamColor);

    m_beamGroup->setVisible(true);
}

void PropertyPanel::showColumnProperties(int columnId)
{
    LoadingGuard guard(m_isLoading);
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

    m_columnSectionTypeCombo->blockSignals(true);
    const auto& sec = col->section();
    int activeSecData = 0;
    if (sec.shape == TSA::Model::SectionShape::Circular)
    {
        activeSecData = 1;
        m_columnSectionTypeCombo->setCurrentIndex(m_columnSectionTypeCombo->findData(1));
    }
    else if (sec.shape == TSA::Model::SectionShape::IShape)
    {
        int idx = m_columnSectionTypeCombo->findText(QString::fromStdString(sec.name));
        if (idx < 0)
        {
            int hMm = static_cast<int>(std::round(sec.height * 1000.0));
            idx = m_columnSectionTypeCombo->findData(hMm);
        }
        if (idx >= 0)
        {
            m_columnSectionTypeCombo->setCurrentIndex(idx);
            activeSecData = m_columnSectionTypeCombo->currentData().toInt();
        }
        else
        {
            m_columnSectionTypeCombo->setCurrentIndex(0);
            activeSecData = 0;
        }
    }
    else
    {
        m_columnSectionTypeCombo->setCurrentIndex(m_columnSectionTypeCombo->findData(0));
        activeSecData = 0;
    }
    m_columnSectionTypeCombo->blockSignals(false);
    updateColumnSectionVisibility(activeSecData);

    int matCode = 1;
    if (col->material().type == TSA::Model::MaterialType::Steel)
        matCode = (col->material().fk > 300e6) ? 4 : 3;
    else if (col->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (col->material().fk > 28e6) ? 2 : 1;
    m_columnMaterialCombo->setCurrentIndex(m_columnMaterialCombo->findData(matCode));

    m_columnColor = QString::fromStdString(col->color());
    setupColorButton(m_columnColorBtn, m_columnColor);

    m_columnGroup->setVisible(true);
}

void PropertyPanel::showSlabProperties(int slabId)
{
    LoadingGuard guard(m_isLoading);
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

    int matCode = 1;
    if (slab->material().type == TSA::Model::MaterialType::Steel)
        matCode = (slab->material().fk > 300e6) ? 4 : 3;
    else if (slab->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (slab->material().fk > 28e6) ? 2 : 1;
    m_slabMaterialCombo->setCurrentIndex(m_slabMaterialCombo->findData(matCode));

    m_slabColor = QString::fromStdString(slab->color());
    setupColorButton(m_slabColorBtn, m_slabColor);

    m_slabGroup->setVisible(true);
}

void PropertyPanel::showWallProperties(int wallId)
{
    LoadingGuard guard(m_isLoading);
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

    int matCode = 1;
    if (wall->material().type == TSA::Model::MaterialType::Steel)
        matCode = (wall->material().fk > 300e6) ? 4 : 3;
    else if (wall->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (wall->material().fk > 28e6) ? 2 : 1;
    m_wallMaterialCombo->setCurrentIndex(m_wallMaterialCombo->findData(matCode));

    m_wallColor = QString::fromStdString(wall->color());
    setupColorButton(m_wallColorBtn, m_wallColor);

    m_wallGroup->setVisible(true);
}

void PropertyPanel::showFoundationProperties(int foundationId)
{
    LoadingGuard guard(m_isLoading);
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

    int matCode = 1;
    if (f->material().type == TSA::Model::MaterialType::Steel)
        matCode = (f->material().fk > 300e6) ? 4 : 3;
    else if (f->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (f->material().fk > 28e6) ? 2 : 1;
    m_foundationMaterialCombo->setCurrentIndex(m_foundationMaterialCombo->findData(matCode));

    m_foundationColor = QString::fromStdString(f->color());
    setupColorButton(m_foundationColorBtn, m_foundationColor);

    m_foundationGroup->setVisible(true);
}

void PropertyPanel::showTrussMemberProperties(int memberId)
{
    LoadingGuard guard(m_isLoading);
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

    int matCode = 3;
    if (truss->material().type == TSA::Model::MaterialType::Steel)
        matCode = (truss->material().fk > 300e6) ? 4 : 3;
    else if (truss->material().type == TSA::Model::MaterialType::Timber)
        matCode = 5;
    else
        matCode = (truss->material().fk > 28e6) ? 2 : 1;
    m_trussMaterialCombo->setCurrentIndex(m_trussMaterialCombo->findData(matCode));

    m_trussColor = QString::fromStdString(truss->color());
    setupColorButton(m_trussColorBtn, m_trussColor);

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
    node->setColor(m_nodeColor.toStdString());

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
    beam->setRole(static_cast<TSA::Model::BarRole>(m_beamRoleCombo->currentData().toInt()));
    beam->setEccentricity(static_cast<TSA::Model::BarEccentricity>(m_beamEccentricityCombo->currentData().toInt()));

    // 1. Mise à jour de la section
    int secData = m_beamSectionTypeCombo->currentData().toInt();
    if (secData == 0) // Rectangulaire
    {
        beam->setSection(TSA::Model::Section::rectangular(m_beamWidthSpin->value(), m_beamHeightSpin->value()));
    }
    else if (secData == 1) // Circulaire
    {
        beam->setSection(TSA::Model::Section::circular(m_beamWidthSpin->value()));
    }
    else if (secData >= 100 && secData <= 400) // IPE
    {
        beam->setSection(TSA::Model::Section::ipe(secData));
    }
    else if (secData >= 1600 && secData <= 2400) // HEA
    {
        beam->setSection(TSA::Model::Section::hea(secData / 10));
    }
    else if (secData >= 1601 && secData <= 3001) // HEB
    {
        beam->setSection(TSA::Model::Section::heb((secData - 1) / 10));
    }
    else if (secData >= 5100 && secData <= 5300) // UPN
    {
        beam->setSection(TSA::Model::Section::upn(secData - 5000));
    }
    else if (secData == 6060)
    {
        beam->setSection(TSA::Model::Section::angle(0.060, 0.060, 0.006));
    }
    else if (secData == 6080)
    {
        beam->setSection(TSA::Model::Section::angle(0.080, 0.080, 0.008));
    }
    else if (secData == 7100)
    {
        beam->setSection(TSA::Model::Section::boxHollow(0.100, 0.100, 0.005));
    }
    else if (secData == 8114)
    {
        beam->setSection(TSA::Model::Section::pipe(0.114, 0.005));
    }

    // 2. Mise à jour du matériau
    int matCode = m_beamMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: beam->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: beam->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: beam->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: beam->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: beam->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    // 3. Mise à jour de l'orientation gamma
    beam->setRotation(m_beamRotationSpin->value());

    // 4. Mise à jour de la couleur
    beam->setColor(m_beamColor.toStdString());

    // 5. Notification immédiate -> reconstruction 3D automatique
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

    // 1. Mise à jour de la section
    int secData = m_columnSectionTypeCombo->currentData().toInt();
    if (secData == 0) // Rectangulaire
    {
        col->setSection(TSA::Model::Section::rectangular(m_columnWidthSpin->value(), m_columnDepthSpin->value()));
    }
    else if (secData == 1) // Circulaire
    {
        col->setSection(TSA::Model::Section::circular(m_columnWidthSpin->value()));
    }
    else if (secData >= 160 && secData <= 400) // IPE
    {
        col->setSection(TSA::Model::Section::ipe(secData));
    }
    else if (secData == 2000 || secData == 2400) // HEA
    {
        col->setSection(TSA::Model::Section::hea(secData / 10));
    }
    else if (secData == 2001 || secData == 3001) // HEB
    {
        col->setSection(TSA::Model::Section::heb((secData - 1) / 10));
    }

    // 2. Mise à jour du matériau
    int matCode = m_columnMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: col->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: col->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: col->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: col->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: col->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    // 3. Mise à jour de l'orientation bêta
    col->setRotation(m_columnRotationSpin->value());

    // 4. Mise à jour de la couleur
    col->setColor(m_columnColor.toStdString());

    // 5. Notification immédiate -> reconstruction 3D automatique
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

    int matCode = m_slabMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: slab->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: slab->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: slab->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: slab->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: slab->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    slab->setColor(m_slabColor.toStdString());

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

    int matCode = m_wallMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: wall->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: wall->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: wall->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: wall->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: wall->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    wall->setColor(m_wallColor.toStdString());

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

    int matCode = m_foundationMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: f->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: f->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: f->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: f->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: f->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    f->setColor(m_foundationColor.toStdString());

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
    truss->section().height = m_trussDimensionSpin->value();
    truss->section().diameter = m_trussDimensionSpin->value();

    int matCode = m_trussMaterialCombo->currentData().toInt();
    switch (matCode)
    {
    case 1: truss->setMaterial(TSA::Model::Material::concreteC25_30()); break;
    case 2: truss->setMaterial(TSA::Model::Material::concreteC30_37()); break;
    case 3: truss->setMaterial(TSA::Model::Material::steelS235()); break;
    case 4: truss->setMaterial(TSA::Model::Material::steelS355()); break;
    case 5: truss->setMaterial(TSA::Model::Material::timberC24()); break;
    }

    truss->setColor(m_trussColor.toStdString());

    m_model->notifyTrussMemberModified(m_currentTrussId);
    emit elementModified();
}

void PropertyPanel::onWidgetChanged()
{
    if (m_isLoading)
        return;
    if (!m_chkLiveSync || !m_chkLiveSync->isChecked())
        return;

    switch (m_currentType)
    {
    case CurrentType::Node:
        onApplyNode();
        break;
    case CurrentType::Beam:
        onApplyBeam();
        break;
    case CurrentType::Column:
        onApplyColumn();
        break;
    case CurrentType::Slab:
        onApplySlab();
        break;
    case CurrentType::Wall:
        onApplyWall();
        break;
    case CurrentType::Foundation:
        onApplyFoundation();
        break;
    case CurrentType::Truss:
        onApplyTruss();
        break;
    default:
        break;
    }
}

} // namespace TSA::UI
