#include "GridSettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

namespace TSA::UI
{

GridSettingsDialog::GridSettingsDialog(OccView* occView, QWidget* parent)
    : QDialog(parent)
    , m_occView(occView)
{
    setupUi();
}

void GridSettingsDialog::setupUi()
{
    setWindowTitle(tr("3D Grid Settings (Cartesian & Cylindrical)"));
    resize(420, 440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Sélection du type de grille active
    auto* typeGroup = new QGroupBox(tr("Active Grid Display"), this);
    auto* typeLayout = new QHBoxLayout(typeGroup);
    m_radioCartesian = new QRadioButton(tr("Cartesian (3D Rectangular)"), typeGroup);
    m_radioCylindrical = new QRadioButton(tr("Cylindrical (Polar / Radial)"), typeGroup);
    m_radioNone = new QRadioButton(tr("Hidden"), typeGroup);

    if (m_occView)
    {
        if (m_occView->currentGridType() == OccView::GridType::Cylindrical)
        {
            m_radioCylindrical->setChecked(true);
        }
        else if (m_occView->currentGridType() == OccView::GridType::Cartesian)
        {
            m_radioCartesian->setChecked(true);
        }
        else
        {
            m_radioNone->setChecked(true);
        }
    }
    else
    {
        m_radioCartesian->setChecked(true);
    }

    typeLayout->addWidget(m_radioCartesian);
    typeLayout->addWidget(m_radioCylindrical);
    typeLayout->addWidget(m_radioNone);
    mainLayout->addWidget(typeGroup);

    // 2. Onglets de configuration
    m_tabWidget = new QTabWidget(this);

    // --- Onglet Cartésien ---
    auto* cartWidget = new QWidget(m_tabWidget);
    auto* cartForm = new QFormLayout(cartWidget);

    m_cartXStep = new QDoubleSpinBox(cartWidget);
    m_cartXStep->setRange(0.05, 50.0);
    m_cartXStep->setValue(1.0);
    m_cartXStep->setSingleStep(0.5);
    m_cartXStep->setSuffix(" m");

    m_cartYStep = new QDoubleSpinBox(cartWidget);
    m_cartYStep->setRange(0.05, 50.0);
    m_cartYStep->setValue(1.0);
    m_cartYStep->setSingleStep(0.5);
    m_cartYStep->setSuffix(" m");

    m_cartXSize = new QDoubleSpinBox(cartWidget);
    m_cartXSize->setRange(1.0, 500.0);
    m_cartXSize->setValue(20.0);
    m_cartXSize->setSingleStep(5.0);
    m_cartXSize->setSuffix(" m");

    m_cartYSize = new QDoubleSpinBox(cartWidget);
    m_cartYSize->setRange(1.0, 500.0);
    m_cartYSize->setValue(20.0);
    m_cartYSize->setSingleStep(5.0);
    m_cartYSize->setSuffix(" m");

    m_cartZOffset = new QDoubleSpinBox(cartWidget);
    m_cartZOffset->setRange(-1000.0, 1000.0);
    m_cartZOffset->setValue(0.0);
    m_cartZOffset->setSingleStep(1.0);
    m_cartZOffset->setSuffix(" m");

    m_cartModeCombo = new QComboBox(cartWidget);
    m_cartModeCombo->addItems({ tr("Solid Lines"), tr("Points") });

    cartForm->addRow(tr("Step X (dX):"), m_cartXStep);
    cartForm->addRow(tr("Step Y (dY):"), m_cartYStep);
    cartForm->addRow(tr("Grid Width (Size X):"), m_cartXSize);
    cartForm->addRow(tr("Grid Length (Size Y):"), m_cartYSize);
    cartForm->addRow(tr("Elevation (Z Level):"), m_cartZOffset);
    cartForm->addRow(tr("Display Mode:"), m_cartModeCombo);

    m_tabWidget->addTab(cartWidget, tr("Cartesian Grid"));

    // --- Onglet Cylindrique ---
    auto* cylWidget = new QWidget(m_tabWidget);
    auto* cylForm = new QFormLayout(cylWidget);

    m_cylRadiusStep = new QDoubleSpinBox(cylWidget);
    m_cylRadiusStep->setRange(0.1, 50.0);
    m_cylRadiusStep->setValue(1.0);
    m_cylRadiusStep->setSingleStep(0.5);
    m_cylRadiusStep->setSuffix(" m");

    m_cylDivisions = new QSpinBox(cylWidget);
    m_cylDivisions->setRange(2, 72);
    m_cylDivisions->setValue(12); // 12 divisions per half-circle = 15° sectors
    m_cylDivisions->setSingleStep(2);
    m_cylDivisions->setSuffix(tr(" (half-circle)"));

    m_cylMaxRadius = new QDoubleSpinBox(cylWidget);
    m_cylMaxRadius->setRange(1.0, 500.0);
    m_cylMaxRadius->setValue(15.0);
    m_cylMaxRadius->setSingleStep(5.0);
    m_cylMaxRadius->setSuffix(" m");

    m_cylZOffset = new QDoubleSpinBox(cylWidget);
    m_cylZOffset->setRange(-1000.0, 1000.0);
    m_cylZOffset->setValue(0.0);
    m_cylZOffset->setSingleStep(1.0);
    m_cylZOffset->setSuffix(" m");

    m_cylModeCombo = new QComboBox(cylWidget);
    m_cylModeCombo->addItems({ tr("Solid Lines & Circles"), tr("Points") });

    cylForm->addRow(tr("Radial Step (dR):"), m_cylRadiusStep);
    cylForm->addRow(tr("Angular Divisions:"), m_cylDivisions);
    cylForm->addRow(tr("Maximum Radius (R):"), m_cylMaxRadius);
    cylForm->addRow(tr("Elevation (Z Level):"), m_cylZOffset);
    cylForm->addRow(tr("Display Mode:"), m_cylModeCombo);

    m_tabWidget->addTab(cylWidget, tr("Cylindrical Grid"));

    mainLayout->addWidget(m_tabWidget);

    // 3. Option d'accrochage (Snapping)
    m_snapCheck = new QCheckBox(tr("Snap cursor to grid intersections (Magnetic Grid)"), this);
    if (m_occView)
    {
        m_snapCheck->setChecked(m_occView->isSnapToGridEnabled());
    }
    mainLayout->addWidget(m_snapCheck);

    // 4. Boutons OK / Appliquer / Cancel
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
        this
    );

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]() {
        applySettings();
        accept();
    });

    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &GridSettingsDialog::applySettings);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void GridSettingsDialog::applySettings()
{
    if (!m_occView)
        return;

    m_occView->setSnapToGridEnabled(m_snapCheck->isChecked());

    if (m_radioNone->isChecked())
    {
        m_occView->hideGrid();
    }
    else if (m_radioCylindrical->isChecked())
    {
        bool pointsMode = (m_cylModeCombo->currentIndex() == 1);
        m_occView->showCylindricalGrid(
            m_cylRadiusStep->value(),
            m_cylDivisions->value(),
            m_cylMaxRadius->value(),
            m_cylZOffset->value(),
            pointsMode
        );
    }
    else
    {
        bool pointsMode = (m_cartModeCombo->currentIndex() == 1);
        m_occView->showCartesianGrid(
            m_cartXStep->value(),
            m_cartYStep->value(),
            m_cartXSize->value(),
            m_cartYSize->value(),
            m_cartZOffset->value(),
            pointsMode
        );
    }
}

} // namespace TSA::UI
