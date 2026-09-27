#include "WorkPlanePropertiesView.h"
#include "../../Model/Model.h"
#include "../../Coordinate/WorkPlaneManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

namespace TSA::UI
{

WorkPlanePropertiesView::WorkPlanePropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void WorkPlanePropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void WorkPlanePropertiesView::setElementId(int id)
{
    if (m_model && m_model->workPlaneManager())
    {
        if (const auto* wp = m_model->workPlaneManager()->getWorkPlane(id))
        {
            setWorkPlane(*wp);
        }
    }
}

void WorkPlanePropertiesView::setWorkPlane(const TSA::Coordinate::WorkPlane& wp)
{
    m_isLoading = true;
    m_workPlane = wp;

    if (m_editName) m_editName->setText(QString::fromStdString(wp.name()));
    if (m_comboType) m_comboType->setCurrentIndex(static_cast<int>(wp.type()));

    gp_Pnt orig = wp.origin();
    if (m_spinX) m_spinX->setValue(orig.X());
    if (m_spinY) m_spinY->setValue(orig.Y());
    if (m_spinZ) m_spinZ->setValue(orig.Z());

    if (m_spinRotX) m_spinRotX->setValue(wp.rotationX());
    if (m_spinRotY) m_spinRotY->setValue(wp.rotationY());
    if (m_spinRotZ) m_spinRotZ->setValue(wp.rotationZ());

    gp_Dir norm = wp.normal();
    if (m_spinNormX) m_spinNormX->setValue(norm.X());
    if (m_spinNormY) m_spinNormY->setValue(norm.Y());
    if (m_spinNormZ) m_spinNormZ->setValue(norm.Z());

    if (m_spinWidth) m_spinWidth->setValue(wp.width());
    if (m_spinHeight) m_spinHeight->setValue(wp.height());

    if (m_spinSpacingX) m_spinSpacingX->setValue(wp.gridSpacingX());
    if (m_spinSpacingY) m_spinSpacingY->setValue(wp.gridSpacingY());
    if (m_spinSubdivisions) m_spinSubdivisions->setValue(wp.gridSubdivisions());
    if (m_chkGridVisible) m_chkGridVisible->setChecked(wp.isGridVisible());

    if (m_chkVisible) m_chkVisible->setChecked(wp.isVisible());
    if (m_chkActive) m_chkActive->setChecked(wp.isActive());
    if (m_chkLocked) m_chkLocked->setChecked(wp.isLocked());
    if (m_chkIsolated) m_chkIsolated->setChecked(wp.isIsolated());
    if (m_spinIsolationDist) m_spinIsolationDist->setValue(wp.isolationDistance());

    bool isEditable = !wp.isLocked();
    if (m_spinX) m_spinX->setEnabled(isEditable);
    if (m_spinY) m_spinY->setEnabled(isEditable);
    if (m_spinZ) m_spinZ->setEnabled(isEditable);
    if (m_spinRotX) m_spinRotX->setEnabled(isEditable);
    if (m_spinRotY) m_spinRotY->setEnabled(isEditable);
    if (m_spinRotZ) m_spinRotZ->setEnabled(isEditable);

    m_isLoading = false;
}

void WorkPlanePropertiesView::refreshView()
{
    if (m_model && m_model->workPlaneManager())
    {
        if (const auto* wp = m_model->workPlaneManager()->activeWorkPlane())
        {
            setWorkPlane(*wp);
        }
    }
}

void WorkPlanePropertiesView::applyChanges()
{
    if (m_isLoading)
        return;

    m_workPlane.setName(m_editName->text().trimmed().toStdString());
    m_workPlane.setType(static_cast<TSA::Coordinate::WorkPlaneType>(m_comboType->currentIndex()));
    m_workPlane.setDimensions(m_spinWidth->value(), m_spinHeight->value());
    m_workPlane.setGridSettings(m_spinSpacingX->value(), m_spinSpacingY->value(),
                                m_spinSubdivisions->value(), m_chkGridVisible->isChecked());

    m_workPlane.setVisible(m_chkVisible->isChecked());
    m_workPlane.setActive(m_chkActive->isChecked());
    m_workPlane.setLocked(m_chkLocked->isChecked());
    m_workPlane.setIsolated(m_chkIsolated->isChecked());
    m_workPlane.setIsolationDistance(m_spinIsolationDist->value());

    if (!m_workPlane.isLocked())
    {
        m_workPlane.setOrigin(gp_Pnt(m_spinX->value(), m_spinY->value(), m_spinZ->value()));
        m_workPlane.setRotation(m_spinRotX->value(), m_spinRotY->value(), m_spinRotZ->value());
    }

    if (m_model && m_model->workPlaneManager())
    {
        m_model->workPlaneManager()->updateWorkPlane(m_workPlane);
    }

    emit workPlaneModified(m_workPlane);
    emit elementModified();
}

void WorkPlanePropertiesView::onWidgetChanged()
{
    applyChanges();
}

void WorkPlanePropertiesView::onTypeChanged(int index)
{
    if (m_isLoading) return;
    auto newType = static_cast<TSA::Coordinate::WorkPlaneType>(index);
    m_workPlane.setType(newType);

    if (newType == TSA::Coordinate::WorkPlaneType::GlobalXY)
    {
        m_workPlane = TSA::Coordinate::WorkPlane::xy(m_spinZ->value(), m_workPlane.name());
    }
    else if (newType == TSA::Coordinate::WorkPlaneType::GlobalXZ)
    {
        m_workPlane = TSA::Coordinate::WorkPlane::xz(m_spinY->value(), m_workPlane.name());
    }
    else if (newType == TSA::Coordinate::WorkPlaneType::GlobalYZ)
    {
        m_workPlane = TSA::Coordinate::WorkPlane::yz(m_spinX->value(), m_workPlane.name());
    }
    setWorkPlane(m_workPlane);
    applyChanges();
}

void WorkPlanePropertiesView::onViewNormalClicked()
{
    emit viewNormalRequested();
}

void WorkPlanePropertiesView::onResetOriginClicked()
{
    if (m_spinX) m_spinX->setValue(0.0);
    if (m_spinY) m_spinY->setValue(0.0);
    if (m_spinZ) m_spinZ->setValue(0.0);
    applyChanges();
    emit resetOriginRequested();
}

void WorkPlanePropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(8);

    // 1. Groupe Identification
    auto* grpId = new QGroupBox(tr("Identification & Type"), this);
    auto* formId = new QFormLayout(grpId);
    formId->setContentsMargins(8, 8, 8, 8);
    formId->setSpacing(6);

    m_editName = new QLineEdit(this);
    connect(m_editName, &QLineEdit::editingFinished, this, &WorkPlanePropertiesView::onWidgetChanged);
    formId->addRow(tr("Nom :"), m_editName);

    m_comboType = new QComboBox(this);
    m_comboType->addItem(tr("Global XY (Horizontal)"));
    m_comboType->addItem(tr("Global XZ (Façade)"));
    m_comboType->addItem(tr("Global YZ (Pignon)"));
    m_comboType->addItem(tr("Étage (Altitude Z)"));
    m_comboType->addItem(tr("3 Points Arbitraires"));
    m_comboType->addItem(tr("Parallèle à une face"));
    m_comboType->addItem(tr("Personnalisé (Custom)"));
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WorkPlanePropertiesView::onTypeChanged);
    formId->addRow(tr("Type :"), m_comboType);
    mainLayout->addWidget(grpId);

    // 2. Groupe Position 3D (Origine)
    auto* grpPos = new QGroupBox(tr("Position 3D (Origine)"), this);
    auto* formPos = new QFormLayout(grpPos);
    formPos->setContentsMargins(8, 8, 8, 8);
    formPos->setSpacing(6);

    m_spinX = new QDoubleSpinBox(this);
    m_spinX->setRange(-99999.0, 99999.0);
    m_spinX->setDecimals(3);
    m_spinX->setSingleStep(0.5);
    m_spinX->setSuffix(tr(" m"));
    connect(m_spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formPos->addRow(tr("Origine X :"), m_spinX);

    m_spinY = new QDoubleSpinBox(this);
    m_spinY->setRange(-99999.0, 99999.0);
    m_spinY->setDecimals(3);
    m_spinY->setSingleStep(0.5);
    m_spinY->setSuffix(tr(" m"));
    connect(m_spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formPos->addRow(tr("Origine Y :"), m_spinY);

    m_spinZ = new QDoubleSpinBox(this);
    m_spinZ->setRange(-99999.0, 99999.0);
    m_spinZ->setDecimals(3);
    m_spinZ->setSingleStep(0.5);
    m_spinZ->setSuffix(tr(" m"));
    connect(m_spinZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formPos->addRow(tr("Origine Z :"), m_spinZ);
    mainLayout->addWidget(grpPos);

    // 3. Groupe Orientation 3D (Degrés)
    auto* grpRot = new QGroupBox(tr("Orientation (Rotations)"), this);
    auto* formRot = new QFormLayout(grpRot);
    formRot->setContentsMargins(8, 8, 8, 8);
    formRot->setSpacing(6);

    m_spinRotX = new QDoubleSpinBox(this);
    m_spinRotX->setRange(-180.0, 180.0);
    m_spinRotX->setDecimals(1);
    m_spinRotX->setSingleStep(5.0);
    m_spinRotX->setSuffix(tr("°"));
    connect(m_spinRotX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formRot->addRow(tr("Rotation X :"), m_spinRotX);

    m_spinRotY = new QDoubleSpinBox(this);
    m_spinRotY->setRange(-180.0, 180.0);
    m_spinRotY->setDecimals(1);
    m_spinRotY->setSingleStep(5.0);
    m_spinRotY->setSuffix(tr("°"));
    connect(m_spinRotY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formRot->addRow(tr("Rotation Y :"), m_spinRotY);

    m_spinRotZ = new QDoubleSpinBox(this);
    m_spinRotZ->setRange(-180.0, 180.0);
    m_spinRotZ->setDecimals(1);
    m_spinRotZ->setSingleStep(5.0);
    m_spinRotZ->setSuffix(tr("°"));
    connect(m_spinRotZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formRot->addRow(tr("Rotation Z :"), m_spinRotZ);

    m_spinNormX = new QDoubleSpinBox(this);
    m_spinNormX->setRange(-1.0, 1.0);
    m_spinNormX->setDecimals(3);
    m_spinNormX->setReadOnly(true);
    formRot->addRow(tr("Normale Nx :"), m_spinNormX);

    m_spinNormY = new QDoubleSpinBox(this);
    m_spinNormY->setRange(-1.0, 1.0);
    m_spinNormY->setDecimals(3);
    m_spinNormY->setReadOnly(true);
    formRot->addRow(tr("Normale Ny :"), m_spinNormY);

    m_spinNormZ = new QDoubleSpinBox(this);
    m_spinNormZ->setRange(-1.0, 1.0);
    m_spinNormZ->setDecimals(3);
    m_spinNormZ->setReadOnly(true);
    formRot->addRow(tr("Normale Nz :"), m_spinNormZ);
    mainLayout->addWidget(grpRot);

    // 4. Groupe Dimensions
    auto* grpDim = new QGroupBox(tr("Dimensions du Plan Visible"), this);
    auto* formDim = new QFormLayout(grpDim);
    formDim->setContentsMargins(8, 8, 8, 8);
    formDim->setSpacing(6);

    m_spinWidth = new QDoubleSpinBox(this);
    m_spinWidth->setRange(1.0, 5000.0);
    m_spinWidth->setDecimals(1);
    m_spinWidth->setSingleStep(5.0);
    m_spinWidth->setSuffix(tr(" m"));
    connect(m_spinWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formDim->addRow(tr("Largeur U :"), m_spinWidth);

    m_spinHeight = new QDoubleSpinBox(this);
    m_spinHeight->setRange(1.0, 5000.0);
    m_spinHeight->setDecimals(1);
    m_spinHeight->setSingleStep(5.0);
    m_spinHeight->setSuffix(tr(" m"));
    connect(m_spinHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formDim->addRow(tr("Longueur V :"), m_spinHeight);
    mainLayout->addWidget(grpDim);

    // 5. Groupe Grille Locale
    auto* grpGrid = new QGroupBox(tr("Grille Locale du Plan"), this);
    auto* formGrid = new QFormLayout(grpGrid);
    formGrid->setContentsMargins(8, 8, 8, 8);
    formGrid->setSpacing(6);

    m_spinSpacingX = new QDoubleSpinBox(this);
    m_spinSpacingX->setRange(0.05, 50.0);
    m_spinSpacingX->setDecimals(2);
    m_spinSpacingX->setSingleStep(0.5);
    m_spinSpacingX->setSuffix(tr(" m"));
    connect(m_spinSpacingX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formGrid->addRow(tr("Pas U :"), m_spinSpacingX);

    m_spinSpacingY = new QDoubleSpinBox(this);
    m_spinSpacingY->setRange(0.05, 50.0);
    m_spinSpacingY->setDecimals(2);
    m_spinSpacingY->setSingleStep(0.5);
    m_spinSpacingY->setSuffix(tr(" m"));
    connect(m_spinSpacingY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formGrid->addRow(tr("Pas V :"), m_spinSpacingY);

    m_spinSubdivisions = new QSpinBox(this);
    m_spinSubdivisions->setRange(1, 20);
    connect(m_spinSubdivisions, QOverload<int>::of(&QSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    formGrid->addRow(tr("Subdivisions :"), m_spinSubdivisions);

    m_chkGridVisible = new QCheckBox(tr("Afficher lignes UV du plan"), this);
    connect(m_chkGridVisible, &QCheckBox::toggled, this, &WorkPlanePropertiesView::onWidgetChanged);
    formGrid->addRow(m_chkGridVisible);
    mainLayout->addWidget(grpGrid);

    // 6. Groupe États & Contrôles
    auto* grpState = new QGroupBox(tr("États & Isolation"), this);
    auto* layState = new QVBoxLayout(grpState);
    layState->setContentsMargins(8, 8, 8, 8);
    layState->setSpacing(4);

    m_chkVisible = new QCheckBox(tr("Plan visible dans la scène 3D"), this);
    connect(m_chkVisible, &QCheckBox::toggled, this, &WorkPlanePropertiesView::onWidgetChanged);
    layState->addWidget(m_chkVisible);

    m_chkActive = new QCheckBox(tr("Plan actif pour le dessin CAO"), this);
    connect(m_chkActive, &QCheckBox::toggled, this, &WorkPlanePropertiesView::onWidgetChanged);
    layState->addWidget(m_chkActive);

    m_chkLocked = new QCheckBox(tr("Verrouiller la position (Lock)"), this);
    connect(m_chkLocked, &QCheckBox::toggled, this, &WorkPlanePropertiesView::onWidgetChanged);
    layState->addWidget(m_chkLocked);

    m_chkIsolated = new QCheckBox(tr("Isoler le plan (filtre de proximité)"), this);
    connect(m_chkIsolated, &QCheckBox::toggled, this, &WorkPlanePropertiesView::onWidgetChanged);
    layState->addWidget(m_chkIsolated);

    auto* layIso = new QHBoxLayout();
    layIso->addWidget(new QLabel(tr("Épaisseur d'isolation :"), this));
    m_spinIsolationDist = new QDoubleSpinBox(this);
    m_spinIsolationDist->setRange(0.1, 50.0);
    m_spinIsolationDist->setDecimals(2);
    m_spinIsolationDist->setSingleStep(0.5);
    m_spinIsolationDist->setSuffix(tr(" m"));
    connect(m_spinIsolationDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WorkPlanePropertiesView::onWidgetChanged);
    layIso->addWidget(m_spinIsolationDist);
    layState->addLayout(layIso);
    mainLayout->addWidget(grpState);

    // 7. Boutons Actions Rapides
    auto* layActions = new QVBoxLayout();
    layActions->setSpacing(6);

    m_btnViewNormal = new QPushButton(tr("👁 Vue Normale au Plan"), this);
    m_btnViewNormal->setStyleSheet("font-weight: bold; padding: 6px; background-color: #2563EB; color: white; border-radius: 4px;");
    connect(m_btnViewNormal, &QPushButton::clicked, this, &WorkPlanePropertiesView::onViewNormalClicked);
    layActions->addWidget(m_btnViewNormal);

    m_btnResetOrigin = new QPushButton(tr("⟲ Réinitialiser Origine à (0,0,0)"), this);
    connect(m_btnResetOrigin, &QPushButton::clicked, this, &WorkPlanePropertiesView::onResetOriginClicked);
    layActions->addWidget(m_btnResetOrigin);

    mainLayout->addLayout(layActions);
    mainLayout->addStretch();
}

} // namespace TSA::UI
