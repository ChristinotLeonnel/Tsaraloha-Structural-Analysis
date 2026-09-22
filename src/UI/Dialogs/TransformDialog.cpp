#include "TransformDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QLabel>

namespace TSA::UI
{

TransformDialog::TransformDialog(TransformMode mode, QWidget* parent)
    : QDialog(parent)
    , m_initialMode(mode)
{
    setupUi();
}

void TransformDialog::setupUi()
{
    setWindowTitle(tr("Transform Elements (Move / Copy)"));
    resize(380, 320);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    // 1. Mode de transformation
    auto* modeGroup = new QGroupBox(tr("Operation Mode"), this);
    auto* modeLayout = new QHBoxLayout(modeGroup);
    m_radioMove = new QRadioButton(tr("Move (Translation)"), modeGroup);
    m_radioCopy = new QRadioButton(tr("Copy / Duplicate"), modeGroup);

    if (m_initialMode == TransformMode::Move)
    {
        m_radioMove->setChecked(true);
    }
    else
    {
        m_radioCopy->setChecked(true);
    }

    modeLayout->addWidget(m_radioMove);
    modeLayout->addWidget(m_radioCopy);
    mainLayout->addWidget(modeGroup);

    // 2. Vecteur de translation
    auto* vectorGroup = new QGroupBox(tr("Translation Vector"), this);
    auto* formLayout = new QFormLayout(vectorGroup);

    m_dxSpin = new QDoubleSpinBox(vectorGroup);
    m_dySpin = new QDoubleSpinBox(vectorGroup);
    m_dzSpin = new QDoubleSpinBox(vectorGroup);

    for (auto* spin : { m_dxSpin, m_dySpin, m_dzSpin })
    {
        spin->setRange(-10000.0, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(1.0);
        spin->setSuffix(" m");
    }

    formLayout->addRow(tr("dX:"), m_dxSpin);
    formLayout->addRow(tr("dY:"), m_dySpin);
    formLayout->addRow(tr("dZ:"), m_dzSpin);

    m_repetitionsSpin = new QSpinBox(vectorGroup);
    m_repetitionsSpin->setRange(1, 100);
    m_repetitionsSpin->setValue(1);
    formLayout->addRow(tr("Repetitions:"), m_repetitionsSpin);

    mainLayout->addWidget(vectorGroup);

    // Activer / désactiver les répétitions selon le mode
    auto updateRepetitionsState = [this]() {
        m_repetitionsSpin->setEnabled(m_radioCopy->isChecked());
    };
    connect(m_radioMove, &QRadioButton::toggled, this, updateRepetitionsState);
    connect(m_radioCopy, &QRadioButton::toggled, this, updateRepetitionsState);
    updateRepetitionsState();

    // 3. Boutons OK / Cancel
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

TransformMode TransformDialog::mode() const
{
    return m_radioCopy->isChecked() ? TransformMode::Copy : TransformMode::Move;
}

double TransformDialog::deltaX() const
{
    return m_dxSpin->value();
}

double TransformDialog::deltaY() const
{
    return m_dySpin->value();
}

double TransformDialog::deltaZ() const
{
    return m_dzSpin->value();
}

int TransformDialog::repetitions() const
{
    return m_repetitionsSpin->value();
}

} // namespace TSA::UI
