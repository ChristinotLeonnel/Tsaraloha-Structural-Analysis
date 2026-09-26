#include "SectionCustomizationDialog.h"
#include "../Widgets/SectionPreviewWidget.h"
#include "../../Library/LibraryManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QFrame>
#include <QIcon>
#include <cmath>

namespace TSA::UI
{

SectionCustomizationDialog::SectionCustomizationDialog(const TSA::Model::Section& initialSection, QWidget* parent)
    : QDialog(parent)
    , m_section(initialSection)
{
    setWindowTitle(tr("Personnalisation de la Section Structurale"));
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    resize(420, 560);

    setupUi();

    m_isUpdating = true;
    m_editName->setText(QString::fromStdString(m_section.name));

    int shapeIdx = m_comboShape->findData(static_cast<int>(m_section.shape));
    if (shapeIdx >= 0) m_comboShape->setCurrentIndex(shapeIdx);

    m_spinWidthB->setValue(m_section.width > 0.0 ? m_section.width : 0.30);
    m_spinHeightH->setValue(m_section.height > 0.0 ? m_section.height : 0.50);
    m_spinDiameterD->setValue(m_section.diameter > 0.0 ? m_section.diameter : (m_section.width > 0.0 ? m_section.width : 0.30));
    m_spinTw->setValue(m_section.tw > 0.0 ? m_section.tw : 0.010);
    m_spinTf->setValue(m_section.tf > 0.0 ? m_section.tf : 0.015);
    m_isUpdating = false;

    updateVisibility();
    updateCalculatedProperties();
}

void SectionCustomizationDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Groupe Identification & Forme
    auto* grpType = new QGroupBox(tr("Forme de Section & Bibliothèque"), this);
    auto* formType = new QFormLayout(grpType);
    formType->setContentsMargins(8, 10, 8, 8);
    formType->setSpacing(6);

    m_comboShape = new QComboBox(grpType);
    m_comboShape->addItem(tr("Rectangulaire (B x H)"), static_cast<int>(TSA::Model::SectionShape::Rectangular));
    m_comboShape->addItem(tr("Circulaire (Ø)"), static_cast<int>(TSA::Model::SectionShape::Circular));
    m_comboShape->addItem(tr("Profil en I (IPE / HEA / HEB)"), static_cast<int>(TSA::Model::SectionShape::IShape));
    m_comboShape->addItem(tr("Tube Rectangulaire / Carré"), static_cast<int>(TSA::Model::SectionShape::BoxHollow));
    m_comboShape->addItem(tr("Tube Rond (Pipe)"), static_cast<int>(TSA::Model::SectionShape::Pipe));
    m_comboShape->addItem(tr("Profil en U (UPN)"), static_cast<int>(TSA::Model::SectionShape::UPN));
    m_comboShape->addItem(tr("Cornière (L)"), static_cast<int>(TSA::Model::SectionShape::Angle));
    m_comboShape->addItem(tr("Profil en T"), static_cast<int>(TSA::Model::SectionShape::TSection));
    formType->addRow(tr("Forme :"), m_comboShape);

    m_lblPreset = new QLabel(tr("Profil standard :"), grpType);
    m_comboPreset = new QComboBox(grpType);
    formType->addRow(m_lblPreset, m_comboPreset);

    m_editName = new QLineEdit(grpType);
    formType->addRow(tr("Nom de section :"), m_editName);

    mainLayout->addWidget(grpType);

    // 2. Aperçu Graphique 2D avec Cotes
    auto* grpPreview = new QGroupBox(tr("Schéma & Dimensions"), this);
    auto* prevLayout = new QVBoxLayout(grpPreview);
    prevLayout->setContentsMargins(6, 6, 6, 6);

    m_previewWidget = new SectionPreviewWidget(grpPreview);
    m_previewWidget->setMinimumHeight(180);
    prevLayout->addWidget(m_previewWidget);

    // Champs de dimensions
    auto* gridDim = new QGridLayout();
    gridDim->setContentsMargins(0, 4, 0, 4);
    gridDim->setSpacing(6);

    m_lblWidthB = new QLabel(tr("Largeur B :"), grpPreview);
    m_spinWidthB = new QDoubleSpinBox(grpPreview);
    m_spinWidthB->setRange(0.01, 10.0);
    m_spinWidthB->setSingleStep(0.05);
    m_spinWidthB->setDecimals(3);
    m_spinWidthB->setSuffix(" m");
    gridDim->addWidget(m_lblWidthB, 0, 0);
    gridDim->addWidget(m_spinWidthB, 0, 1);

    m_lblHeightH = new QLabel(tr("Hauteur H :"), grpPreview);
    m_spinHeightH = new QDoubleSpinBox(grpPreview);
    m_spinHeightH->setRange(0.01, 10.0);
    m_spinHeightH->setSingleStep(0.05);
    m_spinHeightH->setDecimals(3);
    m_spinHeightH->setSuffix(" m");
    gridDim->addWidget(m_lblHeightH, 0, 2);
    gridDim->addWidget(m_spinHeightH, 0, 3);

    m_lblDiameterD = new QLabel(tr("Diamètre D :"), grpPreview);
    m_spinDiameterD = new QDoubleSpinBox(grpPreview);
    m_spinDiameterD->setRange(0.01, 10.0);
    m_spinDiameterD->setSingleStep(0.05);
    m_spinDiameterD->setDecimals(3);
    m_spinDiameterD->setSuffix(" m");
    gridDim->addWidget(m_lblDiameterD, 1, 0);
    gridDim->addWidget(m_spinDiameterD, 1, 1);

    m_lblTw = new QLabel(tr("Épaisseur âme tw :"), grpPreview);
    m_spinTw = new QDoubleSpinBox(grpPreview);
    m_spinTw->setRange(0.001, 0.50);
    m_spinTw->setSingleStep(0.002);
    m_spinTw->setDecimals(3);
    m_spinTw->setSuffix(" m");
    gridDim->addWidget(m_lblTw, 2, 0);
    gridDim->addWidget(m_spinTw, 2, 1);

    m_lblTf = new QLabel(tr("Épaisseur aile tf :"), grpPreview);
    m_spinTf = new QDoubleSpinBox(grpPreview);
    m_spinTf->setRange(0.001, 0.50);
    m_spinTf->setSingleStep(0.002);
    m_spinTf->setDecimals(3);
    m_spinTf->setSuffix(" m");
    gridDim->addWidget(m_lblTf, 2, 2);
    gridDim->addWidget(m_spinTf, 2, 3);

    prevLayout->addLayout(gridDim);
    mainLayout->addWidget(grpPreview);

    // 3. Groupe Propriétés Géométriques Calculées
    auto* grpCalc = new QGroupBox(tr("Caractéristiques Mécaniques Calculées"), this);
    auto* calcLayout = new QVBoxLayout(grpCalc);
    calcLayout->setContentsMargins(8, 8, 8, 8);
    calcLayout->setSpacing(2);

    m_lblArea = new QLabel(grpCalc);
    m_lblArea->setStyleSheet("font-family: monospace; font-size: 8.5pt;");
    m_lblIy = new QLabel(grpCalc);
    m_lblIy->setStyleSheet("font-family: monospace; font-size: 8.5pt;");
    m_lblIz = new QLabel(grpCalc);
    m_lblIz->setStyleSheet("font-family: monospace; font-size: 8.5pt;");
    m_lblIt = new QLabel(grpCalc);
    m_lblIt->setStyleSheet("font-family: monospace; font-size: 8.5pt;");
    m_lblW = new QLabel(grpCalc);
    m_lblW->setStyleSheet("font-family: monospace; font-size: 8.5pt;");

    calcLayout->addWidget(m_lblArea);
    calcLayout->addWidget(m_lblIy);
    calcLayout->addWidget(m_lblIz);
    calcLayout->addWidget(m_lblIt);
    calcLayout->addWidget(m_lblW);

    mainLayout->addWidget(grpCalc);

    // 4. Boutons Appliquer / Annuler
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnApply = new QPushButton(tr("Appliquer"), this);
    m_btnApply->setIcon(QIcon(":/icons/apply.svg"));
    m_btnApply->setStyleSheet("font-weight: bold; background-color: #007ACC; color: white; padding: 6px 16px; border-radius: 4px;");

    m_btnCancel = new QPushButton(tr("Annuler"), this);
    m_btnCancel->setIcon(QIcon(":/icons/cancel.svg"));

    btnLayout->addStretch();
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_comboShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SectionCustomizationDialog::onShapeChanged);
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SectionCustomizationDialog::onPresetSelected);

    connect(m_spinWidthB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCustomizationDialog::onDimensionsChanged);
    connect(m_spinHeightH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCustomizationDialog::onDimensionsChanged);
    connect(m_spinDiameterD, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCustomizationDialog::onDimensionsChanged);
    connect(m_spinTw, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCustomizationDialog::onDimensionsChanged);
    connect(m_spinTf, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCustomizationDialog::onDimensionsChanged);

    connect(m_btnApply, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void SectionCustomizationDialog::populatePresetsForShape(TSA::Model::SectionShape shape)
{
    m_comboPreset->blockSignals(true);
    m_comboPreset->clear();
    m_comboPreset->addItem(tr("-- Personnalisée --"), -1);

    if (shape == TSA::Model::SectionShape::IShape)
    {
        m_comboPreset->addItem("IPE 100", 100);
        m_comboPreset->addItem("IPE 120", 120);
        m_comboPreset->addItem("IPE 140", 140);
        m_comboPreset->addItem("IPE 160", 160);
        m_comboPreset->addItem("IPE 180", 180);
        m_comboPreset->addItem("IPE 200", 200);
        m_comboPreset->addItem("IPE 240", 240);
        m_comboPreset->addItem("IPE 300", 300);
        m_comboPreset->addItem("HEA 100", 1100);
        m_comboPreset->addItem("HEA 160", 1160);
        m_comboPreset->addItem("HEA 200", 1200);
        m_comboPreset->addItem("HEA 240", 1240);
        m_comboPreset->addItem("HEB 100", 2100);
        m_comboPreset->addItem("HEB 160", 2160);
        m_comboPreset->addItem("HEB 200", 2200);
        m_comboPreset->addItem("HEB 300", 2300);
    }
    else if (shape == TSA::Model::SectionShape::UPN)
    {
        m_comboPreset->addItem("UPN 80", 80);
        m_comboPreset->addItem("UPN 100", 100);
        m_comboPreset->addItem("UPN 120", 120);
        m_comboPreset->addItem("UPN 160", 160);
        m_comboPreset->addItem("UPN 200", 200);
    }
    else if (shape == TSA::Model::SectionShape::Rectangular)
    {
        m_comboPreset->addItem("300 x 500 mm", 3050);
        m_comboPreset->addItem("400 x 400 mm", 4040);
        m_comboPreset->addItem("600 x 300 mm", 6030);
    }
    else if (shape == TSA::Model::SectionShape::Circular)
    {
        m_comboPreset->addItem("Ø 200 mm", 200);
        m_comboPreset->addItem("Ø 300 mm", 300);
        m_comboPreset->addItem("Ø 400 mm", 400);
        m_comboPreset->addItem("Ø 600 mm", 600);
    }

    m_comboPreset->blockSignals(false);
}

void SectionCustomizationDialog::onShapeChanged(int index)
{
    if (m_isUpdating) return;

    auto shape = static_cast<TSA::Model::SectionShape>(m_comboShape->itemData(index).toInt());
    m_section.shape = shape;

    populatePresetsForShape(shape);
    updateVisibility();
    onDimensionsChanged();
}

void SectionCustomizationDialog::onPresetSelected(int index)
{
    if (m_isUpdating || index <= 0) return;

    int code = m_comboPreset->itemData(index).toInt();
    auto shape = static_cast<TSA::Model::SectionShape>(m_comboShape->currentData().toInt());

    m_isUpdating = true;
    if (shape == TSA::Model::SectionShape::IShape)
    {
        if (code >= 2000) m_section = TSA::Model::Section::heb(code - 2000);
        else if (code >= 1000) m_section = TSA::Model::Section::hea(code - 1000);
        else m_section = TSA::Model::Section::ipe(code);
    }
    else if (shape == TSA::Model::SectionShape::UPN)
    {
        m_section = TSA::Model::Section::upn(code);
    }
    else if (shape == TSA::Model::SectionShape::Rectangular)
    {
        if (code == 3050) m_section = TSA::Model::Section::rectangular(0.30, 0.50);
        else if (code == 4040) m_section = TSA::Model::Section::rectangular(0.40, 0.40);
        else if (code == 6030) m_section = TSA::Model::Section::rectangular(0.60, 0.30);
    }
    else if (shape == TSA::Model::SectionShape::Circular)
    {
        m_section = TSA::Model::Section::circular(code / 1000.0);
    }

    m_editName->setText(QString::fromStdString(m_section.name));
    m_spinWidthB->setValue(m_section.width);
    m_spinHeightH->setValue(m_section.height);
    m_spinDiameterD->setValue(m_section.diameter);
    m_spinTw->setValue(m_section.tw);
    m_spinTf->setValue(m_section.tf);
    m_isUpdating = false;

    updateCalculatedProperties();
}

void SectionCustomizationDialog::updateVisibility()
{
    auto shape = static_cast<TSA::Model::SectionShape>(m_comboShape->currentData().toInt());

    bool isCircular = (shape == TSA::Model::SectionShape::Circular);
    bool isPipe = (shape == TSA::Model::SectionShape::Pipe);
    bool isI = (shape == TSA::Model::SectionShape::IShape || shape == TSA::Model::SectionShape::UPN || shape == TSA::Model::SectionShape::TSection || shape == TSA::Model::SectionShape::BoxHollow);
    bool isAngle = (shape == TSA::Model::SectionShape::Angle);

    m_lblDiameterD->setVisible(isCircular || isPipe);
    m_spinDiameterD->setVisible(isCircular || isPipe);

    m_lblWidthB->setVisible(!isCircular);
    m_spinWidthB->setVisible(!isCircular);

    m_lblHeightH->setVisible(!isCircular && !isPipe);
    m_spinHeightH->setVisible(!isCircular && !isPipe);

    m_lblTw->setVisible(isI || isAngle || isPipe);
    m_spinTw->setVisible(isI || isAngle || isPipe);

    m_lblTf->setVisible(isI && shape != TSA::Model::SectionShape::Pipe);
    m_spinTf->setVisible(isI && shape != TSA::Model::SectionShape::Pipe);

    if (isCircular)
    {
        m_lblDiameterD->setText(tr("Diamètre D :"));
    }
    else if (isPipe)
    {
        m_lblDiameterD->setText(tr("Diamètre ext. D :"));
        m_lblTw->setText(tr("Épaisseur t :"));
    }
    else if (isAngle)
    {
        m_lblTw->setText(tr("Épaisseur t :"));
    }
    else
    {
        m_lblTw->setText(tr("Épaisseur âme tw :"));
        m_lblTf->setText(tr("Épaisseur aile tf :"));
    }
}

void SectionCustomizationDialog::onDimensionsChanged()
{
    if (m_isUpdating) return;

    auto shape = static_cast<TSA::Model::SectionShape>(m_comboShape->currentData().toInt());
    m_section.shape = shape;

    if (shape == TSA::Model::SectionShape::Circular)
    {
        double d = m_spinDiameterD->value();
        m_section.diameter = d;
        m_section.width = d;
        m_section.height = d;
    }
    else if (shape == TSA::Model::SectionShape::Pipe)
    {
        double d = m_spinDiameterD->value();
        m_section.diameter = d;
        m_section.width = d;
        m_section.height = d;
        m_section.tw = m_spinTw->value();
    }
    else
    {
        m_section.width = m_spinWidthB->value();
        m_section.height = m_spinHeightH->value();
        m_section.tw = m_spinTw->value();
        m_section.tf = m_spinTf->value();
    }

    // Auto-génération de nom si non personnalisé par l'utilisateur
    if (m_editName->text().isEmpty() || m_editName->text().startsWith("Rect ") || m_editName->text().startsWith("Circ ") || m_editName->text().startsWith("R"))
    {
        if (shape == TSA::Model::SectionShape::Circular)
        {
            m_section.name = QString("Circ D%1").arg(m_section.diameter * 1000.0, 0, 'f', 0).toStdString();
        }
        else if (shape == TSA::Model::SectionShape::Rectangular)
        {
            m_section.name = QString("R%1x%2").arg(m_section.width * 100.0, 0, 'f', 0).arg(m_section.height * 100.0, 0, 'f', 0).toStdString();
        }
        else
        {
            m_section.name = m_editName->text().toStdString();
        }
        m_editName->setText(QString::fromStdString(m_section.name));
    }
    else
    {
        m_section.name = m_editName->text().toStdString();
    }

    updateCalculatedProperties();
}

void SectionCustomizationDialog::updateCalculatedProperties()
{
    if (m_previewWidget)
    {
        m_previewWidget->setSection(m_section);
    }

    double aCm2 = m_section.area() * 10000.0;
    double iyCm4 = m_section.iy() * 100000000.0;
    double izCm4 = m_section.iz() * 100000000.0;
    double itCm4 = m_section.it() * 100000000.0;
    double wyCm3 = m_section.wy() * 1000000.0;
    double wzCm3 = m_section.wz() * 1000000.0;

    m_lblArea->setText(QString("Aire <b>A</b> = %1 cm² (%2 m²)").arg(aCm2, 0, 'f', 1).arg(m_section.area(), 0, 'f', 4));
    m_lblIy->setText(QString("Inertie <b>Iy</b> = %1 cm⁴ (axe fort)").arg(iyCm4, 0, 'f', 1));
    m_lblIz->setText(QString("Inertie <b>Iz</b> = %1 cm⁴ (axe faible)").arg(izCm4, 0, 'f', 1));
    m_lblIt->setText(QString("Torsion <b>J</b> = %1 cm⁴").arg(itCm4, 0, 'f', 1));
    m_lblW->setText(QString("Module <b>Wel,y</b> = %1 cm³ | <b>Wel,z</b> = %2 cm³").arg(wyCm3, 0, 'f', 1).arg(wzCm3, 0, 'f', 1));
}

TSA::Model::Section SectionCustomizationDialog::customizedSection() const
{
    TSA::Model::Section s = m_section;
    if (m_editName)
    {
        s.name = m_editName->text().toStdString();
    }
    return s;
}

} // namespace TSA::UI
