#include "WorkPlaneDialog.h"
#include "../../Viewer/OccView.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Coordinate/CoordinateTransformationService.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>

namespace TSA::UI
{

WorkPlaneDialog::WorkPlaneDialog(OccView* occView,
                                 TSA::Coordinate::LevelManager* levelManager,
                                 QWidget* parent)
    : QDialog(parent)
    , m_occView(occView)
    , m_levelManager(levelManager)
{
    setWindowTitle(tr("Gestionnaire des Plans de Travail (Work Planes)"));
    resize(480, 420);

    setupUi();
    syncFromOccView();
}

void WorkPlaneDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Groupe Type de Plan
    auto* grpType = new QGroupBox(tr("Orientation du Plan de Travail"), this);
    auto* typeLayout = new QVBoxLayout(grpType);
    typeLayout->setSpacing(8);

    m_rbXY = new QRadioButton(tr("Plan Horizontal Global XY (Z = 0.0 m)"), grpType);
    m_rbLevel = new QRadioButton(tr("Plan d'Étage / Niveau altimétrique (Z = cote d'étage)"), grpType);
    m_rbXZ = new QRadioButton(tr("Plan Vertical Frontal XZ (Façade / Portique)"), grpType);
    m_rbYZ = new QRadioButton(tr("Plan Vertical Latéral YZ (Pignon / Contreventement)"), grpType);
    m_rbThreePoints = new QRadioButton(tr("Plan Incliné Personnalisé (défini par 3 points)"), grpType);

    m_rbXY->setChecked(true);

    typeLayout->addWidget(m_rbXY);
    typeLayout->addWidget(m_rbLevel);
    typeLayout->addWidget(m_rbXZ);
    typeLayout->addWidget(m_rbYZ);
    typeLayout->addWidget(m_rbThreePoints);

    connect(m_rbXY, &QRadioButton::toggled, this, &WorkPlaneDialog::onModeChanged);
    connect(m_rbLevel, &QRadioButton::toggled, this, &WorkPlaneDialog::onModeChanged);
    connect(m_rbXZ, &QRadioButton::toggled, this, &WorkPlaneDialog::onModeChanged);
    connect(m_rbYZ, &QRadioButton::toggled, this, &WorkPlaneDialog::onModeChanged);
    connect(m_rbThreePoints, &QRadioButton::toggled, this, &WorkPlaneDialog::onModeChanged);

    mainLayout->addWidget(grpType);

    // 2. Paramètres numériques (Décalage / Étage)
    auto* grpParams = new QGroupBox(tr("Position et Décalage"), this);
    auto* formParams = new QFormLayout(grpParams);
    formParams->setSpacing(8);

    m_comboLevels = new QComboBox(grpParams);
    if (m_levelManager)
    {
        for (const auto& lvl : m_levelManager->levels())
        {
            m_comboLevels->addItem(
                tr("%1 (Z = %2 m)").arg(QString::fromStdString(lvl.name)).arg(lvl.elevation, 0, 'f', 2),
                lvl.elevation
            );
        }
    }
    if (m_comboLevels->count() == 0)
    {
        m_comboLevels->addItem(tr("Niveau 0 (Z = 0.00 m)"), 0.0);
        m_comboLevels->addItem(tr("Étage 1 (Z = 3.00 m)"), 3.0);
        m_comboLevels->addItem(tr("Étage 2 (Z = 6.00 m)"), 6.0);
    }
    connect(m_comboLevels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WorkPlaneDialog::onLevelSelected);
    formParams->addRow(tr("Étage de référence :"), m_comboLevels);

    m_spinOffset = new QDoubleSpinBox(grpParams);
    m_spinOffset->setRange(-10000.0, 10000.0);
    m_spinOffset->setDecimals(3);
    m_spinOffset->setSingleStep(0.50);
    m_spinOffset->setSuffix(" m");
    m_spinOffset->setValue(0.0);
    formParams->addRow(tr("Décalage normal (Offset) :"), m_spinOffset);

    mainLayout->addWidget(grpParams);

    // 3. Définition par 3 points (incliné)
    m_grpThreePoints = new QGroupBox(tr("Points de Référence (Plan Incliné)"), this);
    auto* grid3P = new QGridLayout(m_grpThreePoints);
    grid3P->setSpacing(6);

    grid3P->addWidget(new QLabel(tr("P1 (Origine) :")), 0, 0);
    m_p1X = new QDoubleSpinBox(m_grpThreePoints); m_p1X->setRange(-1000, 1000); m_p1X->setValue(0.0);
    m_p1Y = new QDoubleSpinBox(m_grpThreePoints); m_p1Y->setRange(-1000, 1000); m_p1Y->setValue(0.0);
    m_p1Z = new QDoubleSpinBox(m_grpThreePoints); m_p1Z->setRange(-1000, 1000); m_p1Z->setValue(0.0);
    grid3P->addWidget(m_p1X, 0, 1); grid3P->addWidget(m_p1Y, 0, 2); grid3P->addWidget(m_p1Z, 0, 3);

    grid3P->addWidget(new QLabel(tr("P2 (Axe U) :")), 1, 0);
    m_p2X = new QDoubleSpinBox(m_grpThreePoints); m_p2X->setRange(-1000, 1000); m_p2X->setValue(5.0);
    m_p2Y = new QDoubleSpinBox(m_grpThreePoints); m_p2Y->setRange(-1000, 1000); m_p2Y->setValue(0.0);
    m_p2Z = new QDoubleSpinBox(m_grpThreePoints); m_p2Z->setRange(-1000, 1000); m_p2Z->setValue(0.0);
    grid3P->addWidget(m_p2X, 1, 1); grid3P->addWidget(m_p2Y, 1, 2); grid3P->addWidget(m_p2Z, 1, 3);

    grid3P->addWidget(new QLabel(tr("P3 (Plan UV) :")), 2, 0);
    m_p3X = new QDoubleSpinBox(m_grpThreePoints); m_p3X->setRange(-1000, 1000); m_p3X->setValue(0.0);
    m_p3Y = new QDoubleSpinBox(m_grpThreePoints); m_p3Y->setRange(-1000, 1000); m_p3Y->setValue(5.0);
    m_p3Z = new QDoubleSpinBox(m_grpThreePoints); m_p3Z->setRange(-1000, 1000); m_p3Z->setValue(2.0);
    grid3P->addWidget(m_p3X, 2, 1); grid3P->addWidget(m_p3Y, 2, 2); grid3P->addWidget(m_p3Z, 2, 3);

    mainLayout->addWidget(m_grpThreePoints);
    m_grpThreePoints->setVisible(false);

    // 4. Options
    auto* grpOpt = new QGroupBox(tr("Options d'Affichage & Navigation"), this);
    auto* optLayout = new QVBoxLayout(grpOpt);
    optLayout->setSpacing(6);

    m_chkVisible = new QCheckBox(tr("Afficher la trame 3D du plan dans le viewport"), grpOpt);
    m_chkVisible->setChecked(m_occView ? m_occView->isWorkPlaneVisible() : true);
    optLayout->addWidget(m_chkVisible);

    m_chkAlignCamera = new QCheckBox(tr("Aligner immédiatement la vue caméra face au plan (Vue normale)"), grpOpt);
    m_chkAlignCamera->setChecked(false);
    optLayout->addWidget(m_chkAlignCamera);

    mainLayout->addWidget(grpOpt);

    // 5. Boutons standards
    auto* btnBox = new QHBoxLayout();
    auto* btnReset = new QPushButton(tr("Réinitialiser (XY Sol)"), this);
    connect(btnReset, &QPushButton::clicked, this, &WorkPlaneDialog::resetToDefault);
    btnBox->addWidget(btnReset);

    btnBox->addStretch();

    auto* btnApply = new QPushButton(tr("Appliquer"), this);
    btnApply->setDefault(true);
    connect(btnApply, &QPushButton::clicked, this, &WorkPlaneDialog::applySettings);
    btnBox->addWidget(btnApply);

    auto* btnClose = new QPushButton(tr("Fermer"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnBox->addWidget(btnClose);

    mainLayout->addLayout(btnBox);
}

void WorkPlaneDialog::syncFromOccView()
{
    if (!m_occView) return;

    const auto& wp = m_occView->activeWorkPlane();
    m_spinOffset->setValue(wp.offset());
    m_chkVisible->setChecked(m_occView->isWorkPlaneVisible());

    switch (wp.type())
    {
    case TSA::Coordinate::WorkPlaneType::GlobalXY:
        m_rbXY->setChecked(true);
        break;
    case TSA::Coordinate::WorkPlaneType::ElevationZ:
        m_rbLevel->setChecked(true);
        break;
    case TSA::Coordinate::WorkPlaneType::GlobalXZ:
        m_rbXZ->setChecked(true);
        break;
    case TSA::Coordinate::WorkPlaneType::GlobalYZ:
        m_rbYZ->setChecked(true);
        break;
    case TSA::Coordinate::WorkPlaneType::ThreePoints:
    case TSA::Coordinate::WorkPlaneType::Custom:
        m_rbThreePoints->setChecked(true);
        break;
    default:
        m_rbXY->setChecked(true);
        break;
    }

    onModeChanged();
}

void WorkPlaneDialog::onModeChanged()
{
    bool isLevel = m_rbLevel->isChecked();
    bool isThreeP = m_rbThreePoints->isChecked();

    m_comboLevels->setEnabled(isLevel);
    m_grpThreePoints->setVisible(isThreeP);

    if (m_rbXY->isChecked())
    {
        m_spinOffset->setEnabled(false);
        m_spinOffset->setValue(0.0);
    }
    else if (isLevel)
    {
        m_spinOffset->setEnabled(false);
        onLevelSelected(m_comboLevels->currentIndex());
    }
    else if (isThreeP)
    {
        m_spinOffset->setEnabled(false);
    }
    else
    {
        m_spinOffset->setEnabled(true);
    }
}

void WorkPlaneDialog::onLevelSelected(int index)
{
    if (index >= 0 && index < m_comboLevels->count())
    {
        double z = m_comboLevels->itemData(index).toDouble();
        m_spinOffset->setValue(z);
    }
}

void WorkPlaneDialog::applySettings()
{
    if (!m_occView) return;

    if (m_rbXY->isChecked())
    {
        m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalXY, 0.0);
    }
    else if (m_rbLevel->isChecked())
    {
        double z = m_spinOffset->value();
        m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::ElevationZ, z);
    }
    else if (m_rbXZ->isChecked())
    {
        double yOff = m_spinOffset->value();
        m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalXZ, yOff);
    }
    else if (m_rbYZ->isChecked())
    {
        double xOff = m_spinOffset->value();
        m_occView->setWorkPlaneType(TSA::Coordinate::WorkPlaneType::GlobalYZ, xOff);
    }
    else if (m_rbThreePoints->isChecked())
    {
        gp_Pnt p1(m_p1X->value(), m_p1Y->value(), m_p1Z->value());
        gp_Pnt p2(m_p2X->value(), m_p2Y->value(), m_p2Z->value());
        gp_Pnt p3(m_p3X->value(), m_p3Y->value(), m_p3Z->value());

        auto wp = TSA::Coordinate::WorkPlane::fromThreePoints(p1, p2, p3, "Plan 3 Points");
        m_occView->setActiveWorkPlane(wp);
    }

    m_occView->setWorkPlaneVisible(m_chkVisible->isChecked());

    if (m_chkAlignCamera->isChecked())
    {
        m_occView->viewNormalToWorkPlane();
    }
}

void WorkPlaneDialog::resetToDefault()
{
    m_rbXY->setChecked(true);
    m_spinOffset->setValue(0.0);
    m_chkVisible->setChecked(true);
    applySettings();
}

} // namespace TSA::UI
