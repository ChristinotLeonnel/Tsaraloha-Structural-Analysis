#include "SectionCutDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>

namespace TSA::UI
{

SectionCutDialog::SectionCutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Coupes de la structure (Section 3D)"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(360, 260);

    setupUi();
}

void SectionCutDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // GroupBox Paramètres de Coupe
    auto* group = new QGroupBox(tr("Plan de Coupe 3D"), this);
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setSpacing(10);

    // 1. Activer la coupe
    m_enableCheck = new QCheckBox(tr("Activer la coupe de la structure"), this);
    m_enableCheck->setStyleSheet("font-weight: bold;");
    groupLayout->addWidget(m_enableCheck);

    // 2. Choix du plan de coupe
    auto* planeLayout = new QHBoxLayout();
    planeLayout->addWidget(new QLabel(tr("Plan de coupe :"), this));
    m_axisCombo = new QComboBox(this);
    m_axisCombo->addItem(tr("Plan XY (Coupe horizontale / Étage)"), 0);
    m_axisCombo->addItem(tr("Plan XZ (Coupe longitudinale / Façade)"), 1);
    m_axisCombo->addItem(tr("Plan YZ (Coupe transversale / Pignon)"), 2);
    planeLayout->addWidget(m_axisCombo, 1);
    groupLayout->addLayout(planeLayout);

    // 3. Position métrique avec Slider + SpinBox
    auto* posLayout = new QHBoxLayout();
    posLayout->addWidget(new QLabel(tr("Position (m) :"), this));

    m_posSpin = new QDoubleSpinBox(this);
    m_posSpin->setRange(m_minPos, m_maxPos);
    m_posSpin->setDecimals(2);
    m_posSpin->setSingleStep(0.20);
    m_posSpin->setValue(0.0);
    m_posSpin->setSuffix(" m");
    posLayout->addWidget(m_posSpin);
    groupLayout->addLayout(posLayout);

    m_posSlider = new QSlider(Qt::Horizontal, this);
    m_posSlider->setRange(0, 1000);
    m_posSlider->setValue(500);
    groupLayout->addWidget(m_posSlider);

    // 4. Inverser la direction de coupe
    m_flipCheck = new QCheckBox(tr("Inverser le sens de coupe (Vue opposée)"), this);
    groupLayout->addWidget(m_flipCheck);

    // 5. Bouton coupe rapide à l'étage actif
    m_btnCutAtLevel = new QPushButton(tr("Couper à la hauteur de l'étage sélectionné"), this);
    m_btnCutAtLevel->setStyleSheet("padding: 4px 8px; font-weight: 500;");
    groupLayout->addWidget(m_btnCutAtLevel);

    mainLayout->addWidget(group);

    // Boutons Fermer
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(btnBox);

    // Connects
    connect(m_enableCheck, &QCheckBox::toggled, this, &SectionCutDialog::onEnabledToggled);
    connect(m_axisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SectionCutDialog::onAxisChanged);
    connect(m_posSlider, &QSlider::valueChanged, this, &SectionCutDialog::onSliderValueChanged);
    connect(m_posSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SectionCutDialog::onSpinBoxValueChanged);
    connect(m_flipCheck, &QCheckBox::toggled, this, &SectionCutDialog::onFlipToggled);

    connect(m_btnCutAtLevel, &QPushButton::clicked, this, [this]() {
        // Active le plan XY et coupe à la position courante
        m_axisCombo->setCurrentIndex(0);
        emitChange();
    });
}

bool SectionCutDialog::isCutEnabled() const
{
    return m_enableCheck->isChecked();
}

int SectionCutDialog::selectedAxis() const
{
    return m_axisCombo->currentData().toInt();
}

double SectionCutDialog::cutPosition() const
{
    return m_posSpin->value();
}

bool SectionCutDialog::isFlipped() const
{
    return m_flipCheck->isChecked();
}

void SectionCutDialog::setCutEnabled(bool enabled)
{
    m_enableCheck->setChecked(enabled);
}

void SectionCutDialog::setSelectedAxis(int axis)
{
    int idx = m_axisCombo->findData(axis);
    if (idx >= 0)
    {
        m_axisCombo->setCurrentIndex(idx);
    }
}

void SectionCutDialog::setCutPosition(double pos)
{
    m_updating = true;
    m_posSpin->setValue(pos);
    double factor = (pos - m_minPos) / (m_maxPos - m_minPos);
    factor = std::clamp(factor, 0.0, 1.0);
    m_posSlider->setValue(static_cast<int>(factor * 1000.0));
    m_updating = false;
}

void SectionCutDialog::setCutLimits(double minVal, double maxVal)
{
    m_minPos = minVal;
    m_maxPos = maxVal;
    m_posSpin->setRange(minVal, maxVal);
}

void SectionCutDialog::setFlipped(bool flip)
{
    m_flipCheck->setChecked(flip);
}

void SectionCutDialog::onActiveLevelCutRequested(double zLevel)
{
    setSelectedAxis(0);
    setCutPosition(zLevel + 1.20); // 1.2m au dessus de la dalle pour coupe d'étage classique
    setCutEnabled(true);
    emitChange();
}

void SectionCutDialog::onEnabledToggled(bool /*checked*/)
{
    emitChange();
}

void SectionCutDialog::onAxisChanged(int /*index*/)
{
    emitChange();
}

void SectionCutDialog::onSliderValueChanged(int value)
{
    if (m_updating)
        return;
    m_updating = true;
    double pos = m_minPos + (static_cast<double>(value) / 1000.0) * (m_maxPos - m_minPos);
    m_posSpin->setValue(pos);
    m_updating = false;
    emitChange();
}

void SectionCutDialog::onSpinBoxValueChanged(double value)
{
    if (m_updating)
        return;
    m_updating = true;
    double factor = (value - m_minPos) / (m_maxPos - m_minPos);
    factor = std::clamp(factor, 0.0, 1.0);
    m_posSlider->setValue(static_cast<int>(factor * 1000.0));
    m_updating = false;
    emitChange();
}

void SectionCutDialog::onFlipToggled(bool /*checked*/)
{
    emitChange();
}

void SectionCutDialog::emitChange()
{
    emit clippingChanged(
        m_enableCheck->isChecked(),
        selectedAxis(),
        m_posSpin->value(),
        m_flipCheck->isChecked()
    );
}

} // namespace TSA::UI
