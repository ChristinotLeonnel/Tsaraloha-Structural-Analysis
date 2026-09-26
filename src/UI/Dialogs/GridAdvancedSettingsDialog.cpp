#include "GridAdvancedSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QColorDialog>

namespace TSA::UI
{

GridAdvancedSettingsDialog::GridAdvancedSettingsDialog(
    const gp_Pnt& origin,
    double rotationDeg,
    const TSA::Grid::GridDisplaySettings& displaySettings,
    QWidget* parent)
    : QDialog(parent)
    , m_selectedColor(QString::fromStdString(displaySettings.lineColor))
{
    setWindowTitle(tr("Paramètres avancés de la grille"));
    resize(420, 460);
    setupUi();

    m_spnOriginX->setValue(origin.X());
    m_spnOriginY->setValue(origin.Y());
    m_spnOriginZ->setValue(origin.Z());
    m_spnRotation->setValue(rotationDeg);

    m_spnExtension->setValue(displaySettings.extension > 0.0 ? displaySettings.extension : 1.2);
    m_spnBubbleRadius->setValue(displaySettings.bubbleRadius > 0.0 ? displaySettings.bubbleRadius : 0.40);
    m_chkShowBubbles->setChecked(displaySettings.showBubbles);

    int styleIdx = m_cmbLineStyle->findData(QString::fromStdString(displaySettings.lineStyle));
    if (styleIdx >= 0)
    {
        m_cmbLineStyle->setCurrentIndex(styleIdx);
    }
    m_spnLineWidth->setValue(displaySettings.lineWidth > 0.0 ? displaySettings.lineWidth : 1.2);

    if (m_selectedColor.isEmpty())
    {
        m_selectedColor = "#808080";
    }
    m_btnColor->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold; border-radius: 4px; padding: 4px;")
                                  .arg(m_selectedColor));
}

void GridAdvancedSettingsDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Groupe Origine et Rotation
    auto* grpCoord = new QGroupBox(tr("Système de coordonnées local"), this);
    auto* formCoord = new QFormLayout(grpCoord);

    m_spnOriginX = new QDoubleSpinBox(this);
    m_spnOriginX->setRange(-100000.0, 100000.0);
    m_spnOriginX->setDecimals(3);
    m_spnOriginX->setSuffix(" m");

    m_spnOriginY = new QDoubleSpinBox(this);
    m_spnOriginY->setRange(-100000.0, 100000.0);
    m_spnOriginY->setDecimals(3);
    m_spnOriginY->setSuffix(" m");

    m_spnOriginZ = new QDoubleSpinBox(this);
    m_spnOriginZ->setRange(-100000.0, 100000.0);
    m_spnOriginZ->setDecimals(3);
    m_spnOriginZ->setSuffix(" m");

    m_spnRotation = new QDoubleSpinBox(this);
    m_spnRotation->setRange(-360.0, 360.0);
    m_spnRotation->setDecimals(2);
    m_spnRotation->setSuffix(" °");

    formCoord->addRow(tr("Origine X :"), m_spnOriginX);
    formCoord->addRow(tr("Origine Y :"), m_spnOriginY);
    formCoord->addRow(tr("Origine Z :"), m_spnOriginZ);
    formCoord->addRow(tr("Rotation (autour de Z) :"), m_spnRotation);
    mainLayout->addWidget(grpCoord);

    // Groupe Géométrie et Bulles
    auto* grpGeom = new QGroupBox(tr("Géométrie et bulles d'extrémités"), this);
    auto* formGeom = new QFormLayout(grpGeom);

    m_spnExtension = new QDoubleSpinBox(this);
    m_spnExtension->setRange(0.0, 100.0);
    m_spnExtension->setDecimals(2);
    m_spnExtension->setSingleStep(0.2);
    m_spnExtension->setSuffix(" m");

    m_spnBubbleRadius = new QDoubleSpinBox(this);
    m_spnBubbleRadius->setRange(0.05, 10.0);
    m_spnBubbleRadius->setDecimals(2);
    m_spnBubbleRadius->setSingleStep(0.05);
    m_spnBubbleRadius->setSuffix(" m");

    m_chkShowBubbles = new QCheckBox(tr("Afficher les bulles d'axes"), this);

    formGeom->addRow(tr("Débord / Extension des lignes :"), m_spnExtension);
    formGeom->addRow(tr("Rayon des bulles :"), m_spnBubbleRadius);
    formGeom->addRow("", m_chkShowBubbles);
    mainLayout->addWidget(grpGeom);

    // Groupe Affichage / Style
    auto* grpStyle = new QGroupBox(tr("Style graphique des lignes"), this);
    auto* formStyle = new QFormLayout(grpStyle);

    m_cmbLineStyle = new QComboBox(this);
    m_cmbLineStyle->addItem(tr("Tireté (Dashed)"), "dash");
    m_cmbLineStyle->addItem(tr("Continu (Solid)"), "solid");
    m_cmbLineStyle->addItem(tr("Pointillé (Dot)"), "dot");
    m_cmbLineStyle->addItem(tr("Trait mixte (Dash-Dot)"), "dashdot");

    m_spnLineWidth = new QDoubleSpinBox(this);
    m_spnLineWidth->setRange(0.5, 10.0);
    m_spnLineWidth->setDecimals(1);
    m_spnLineWidth->setSingleStep(0.5);

    m_btnColor = new QPushButton(tr("Choisir la couleur..."), this);
    connect(m_btnColor, &QPushButton::clicked, this, &GridAdvancedSettingsDialog::onPickColor);

    formStyle->addRow(tr("Style de trait :"), m_cmbLineStyle);
    formStyle->addRow(tr("Épaisseur :"), m_spnLineWidth);
    formStyle->addRow(tr("Couleur :"), m_btnColor);
    mainLayout->addWidget(grpStyle);

    // Boutons de validation
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void GridAdvancedSettingsDialog::onPickColor()
{
    QColor cur = QColor(m_selectedColor.isEmpty() ? "#808080" : m_selectedColor);
    QColor col = QColorDialog::getColor(cur, this, tr("Sélectionner la couleur des lignes de grille"));
    if (col.isValid())
    {
        m_selectedColor = col.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold; border-radius: 4px; padding: 4px;")
                                      .arg(m_selectedColor));
    }
}

gp_Pnt GridAdvancedSettingsDialog::origin() const
{
    return gp_Pnt(m_spnOriginX->value(), m_spnOriginY->value(), m_spnOriginZ->value());
}

double GridAdvancedSettingsDialog::rotationDeg() const
{
    return m_spnRotation->value();
}

TSA::Grid::GridDisplaySettings GridAdvancedSettingsDialog::displaySettings() const
{
    TSA::Grid::GridDisplaySettings ds;
    ds.extension = m_spnExtension->value();
    ds.bubbleRadius = m_spnBubbleRadius->value();
    ds.showBubbles = m_chkShowBubbles->isChecked();
    ds.lineStyle = m_cmbLineStyle->currentData().toString().toStdString();
    ds.lineWidth = m_spnLineWidth->value();
    ds.lineColor = m_selectedColor.toStdString();
    return ds;
}

} // namespace TSA::UI
