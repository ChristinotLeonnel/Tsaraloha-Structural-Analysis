#include "LoadPropertiesView.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Model/Load/NodalLoad.h"
#include "../../Model/Load/MemberLoad.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>

namespace TSA::UI
{

LoadPropertiesView::LoadPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    populateLoadCases();
}

void LoadPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    populateLoadCases();
    refreshView();
}

void LoadPropertiesView::setNodalLoadId(int loadId)
{
    m_loadId = loadId;
    m_mode = DisplayMode::Nodal;
    refreshView();
}

void LoadPropertiesView::setMemberLoadId(int loadId)
{
    m_loadId = loadId;
    m_mode = DisplayMode::Member;
    refreshView();
}

void LoadPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(10);

    // En-tête
    m_lblHeader = new QLabel(this);
    m_lblHeader->setStyleSheet("font-weight: bold; font-size: 14px; color: #3B82F6; padding: 4px;");
    mainLayout->addWidget(m_lblHeader);

    // Informations générales
    auto* groupGen = new QGroupBox(tr("Identification & Système"), this);
    auto* formGen = new QFormLayout(groupGen);

    m_editName = new QLineEdit(this);
    formGen->addRow(tr("Libellé :"), m_editName);

    m_lblTarget = new QLabel(this);
    m_lblTarget->setStyleSheet("color: #00adb5; font-weight: bold;");
    formGen->addRow(tr("Élément cible :"), m_lblTarget);

    m_comboLoadCase = new QComboBox(this);
    formGen->addRow(tr("Cas de charge :"), m_comboLoadCase);

    m_comboCoordSys = new QComboBox(this);
    m_comboCoordSys->addItem(tr("Global (WCS)"), static_cast<int>(TSA::Model::LoadCoordSystem::Global));
    m_comboCoordSys->addItem(tr("Local (LCS)"), static_cast<int>(TSA::Model::LoadCoordSystem::Local));
    formGen->addRow(tr("Système de coordonnées :"), m_comboCoordSys);

    mainLayout->addWidget(groupGen);

    // Forces nodales (kN)
    m_groupForces = new QGroupBox(tr("Forces Appliquées (kN)"), this);
    auto* gridForces = new QGridLayout(m_groupForces);

    m_spinFx = new QDoubleSpinBox(this);
    m_spinFx->setRange(-1000000.0, 1000000.0);
    m_spinFx->setDecimals(2);
    m_spinFx->setSuffix(" kN");

    m_spinFy = new QDoubleSpinBox(this);
    m_spinFy->setRange(-1000000.0, 1000000.0);
    m_spinFy->setDecimals(2);
    m_spinFy->setSuffix(" kN");

    m_spinFz = new QDoubleSpinBox(this);
    m_spinFz->setRange(-1000000.0, 1000000.0);
    m_spinFz->setDecimals(2);
    m_spinFz->setSuffix(" kN");

    gridForces->addWidget(new QLabel(tr("Fx :"), this), 0, 0);
    gridForces->addWidget(m_spinFx, 0, 1);
    gridForces->addWidget(new QLabel(tr("Fy :"), this), 1, 0);
    gridForces->addWidget(m_spinFy, 1, 1);
    gridForces->addWidget(new QLabel(tr("Fz :"), this), 2, 0);
    gridForces->addWidget(m_spinFz, 2, 1);

    mainLayout->addWidget(m_groupForces);

    // Moments nodaux (kNm)
    m_groupMoments = new QGroupBox(tr("Moments Appliqués (kNm)"), this);
    auto* gridMoments = new QGridLayout(m_groupMoments);

    m_spinMx = new QDoubleSpinBox(this);
    m_spinMx->setRange(-1000000.0, 1000000.0);
    m_spinMx->setDecimals(2);
    m_spinMx->setSuffix(" kNm");

    m_spinMy = new QDoubleSpinBox(this);
    m_spinMy->setRange(-1000000.0, 1000000.0);
    m_spinMy->setDecimals(2);
    m_spinMy->setSuffix(" kNm");

    m_spinMz = new QDoubleSpinBox(this);
    m_spinMz->setRange(-1000000.0, 1000000.0);
    m_spinMz->setDecimals(2);
    m_spinMz->setSuffix(" kNm");

    gridMoments->addWidget(new QLabel(tr("Mx :"), this), 0, 0);
    gridMoments->addWidget(m_spinMx, 0, 1);
    gridMoments->addWidget(new QLabel(tr("My :"), this), 1, 0);
    gridMoments->addWidget(m_spinMy, 1, 1);
    gridMoments->addWidget(new QLabel(tr("Mz :"), this), 2, 0);
    gridMoments->addWidget(m_spinMz, 2, 1);

    mainLayout->addWidget(m_groupMoments);

    // Charges sur barre
    m_groupMember = new QGroupBox(tr("Charges sur Élément Linéaire"), this);
    auto* gridMember = new QGridLayout(m_groupMember);

    m_comboDirection = new QComboBox(this);
    m_comboDirection->addItem(tr("Gravité (-Z)"), static_cast<int>(TSA::Model::LoadDirection::Gravity));
    m_comboDirection->addItem(tr("Global X"), static_cast<int>(TSA::Model::LoadDirection::GlobalX));
    m_comboDirection->addItem(tr("Global Y"), static_cast<int>(TSA::Model::LoadDirection::GlobalY));
    m_comboDirection->addItem(tr("Global Z"), static_cast<int>(TSA::Model::LoadDirection::GlobalZ));
    m_comboDirection->addItem(tr("Local x (axial)"), static_cast<int>(TSA::Model::LoadDirection::LocalX));
    m_comboDirection->addItem(tr("Local y (transversal)"), static_cast<int>(TSA::Model::LoadDirection::LocalY));
    m_comboDirection->addItem(tr("Local z (transversal)"), static_cast<int>(TSA::Model::LoadDirection::LocalZ));
    gridMember->addWidget(new QLabel(tr("Direction :"), this), 0, 0);
    gridMember->addWidget(m_comboDirection, 0, 1, 1, 3);

    m_lblQ1 = new QLabel(tr("Intensité q1 :"), this);
    m_spinQ1 = new QDoubleSpinBox(this);
    m_spinQ1->setRange(-100000.0, 100000.0);
    m_spinQ1->setDecimals(2);
    m_spinQ1->setSuffix(" kN/m");
    gridMember->addWidget(m_lblQ1, 1, 0);
    gridMember->addWidget(m_spinQ1, 1, 1);

    m_lblQ2 = new QLabel(tr("Intensité q2 :"), this);
    m_spinQ2 = new QDoubleSpinBox(this);
    m_spinQ2->setRange(-100000.0, 100000.0);
    m_spinQ2->setDecimals(2);
    m_spinQ2->setSuffix(" kN/m");
    gridMember->addWidget(m_lblQ2, 1, 2);
    gridMember->addWidget(m_spinQ2, 1, 3);

    m_lblX1 = new QLabel(tr("Position x1 :"), this);
    m_spinX1 = new QDoubleSpinBox(this);
    m_spinX1->setRange(0.0, 1000.0);
    m_spinX1->setDecimals(2);
    m_spinX1->setSuffix(" m");
    gridMember->addWidget(m_lblX1, 2, 0);
    gridMember->addWidget(m_spinX1, 2, 1);

    m_lblX2 = new QLabel(tr("Position x2 :"), this);
    m_spinX2 = new QDoubleSpinBox(this);
    m_spinX2->setRange(0.0, 1000.0);
    m_spinX2->setDecimals(2);
    m_spinX2->setSuffix(" m");
    gridMember->addWidget(m_lblX2, 2, 2);
    gridMember->addWidget(m_spinX2, 2, 3);

    mainLayout->addWidget(m_groupMember);
    mainLayout->addStretch();

    // Connexions
    connect(m_editName, &QLineEdit::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_comboLoadCase, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LoadPropertiesView::onFieldChanged);
    connect(m_comboCoordSys, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinFx, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinFy, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinFz, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinMx, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinMy, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinMz, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_comboDirection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinQ1, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinQ2, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinX1, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
    connect(m_spinX2, &QDoubleSpinBox::editingFinished, this, &LoadPropertiesView::onFieldChanged);
}

void LoadPropertiesView::populateLoadCases()
{
    m_comboLoadCase->blockSignals(true);
    m_comboLoadCase->clear();

    if (m_model)
    {
        for (const auto& [id, lc] : m_model->loadManager().loadCases())
        {
            QString txt = QString("[%1] %2 (%3)")
                .arg(id)
                .arg(QString::fromStdString(lc.name()))
                .arg(QString::fromStdString(TSA::Model::loadCategoryToString(lc.category())));
            m_comboLoadCase->addItem(txt, id);
        }
    }
    m_comboLoadCase->blockSignals(false);
}

void LoadPropertiesView::refreshView()
{
    if (!m_model || m_loadId <= 0 || m_mode == DisplayMode::None)
    {
        m_lblHeader->setText(tr("Aucune charge sélectionnée"));
        m_groupForces->setVisible(false);
        m_groupMoments->setVisible(false);
        m_groupMember->setVisible(false);
        return;
    }

    m_isLoading = true;

    if (m_mode == DisplayMode::Nodal)
    {
        const auto* nl = m_model->loadManager().getNodalLoad(m_loadId);
        if (!nl)
        {
            m_isLoading = false;
            return;
        }

        m_lblHeader->setText(tr("Charge Nodale #%1").arg(m_loadId));
        m_editName->setText(QString::fromStdString(nl->name()));
        m_lblTarget->setText(tr("Nœud N%1").arg(nl->nodeId()));

        int lcIdx = m_comboLoadCase->findData(nl->loadCaseId());
        if (lcIdx >= 0) m_comboLoadCase->setCurrentIndex(lcIdx);

        int csIdx = m_comboCoordSys->findData(static_cast<int>(nl->coordSystem()));
        if (csIdx >= 0) m_comboCoordSys->setCurrentIndex(csIdx);

        m_spinFx->setValue(nl->fx());
        m_spinFy->setValue(nl->fy());
        m_spinFz->setValue(nl->fz());

        m_spinMx->setValue(nl->mx());
        m_spinMy->setValue(nl->my());
        m_spinMz->setValue(nl->mz());

        m_groupForces->setVisible(true);
        m_groupMoments->setVisible(true);
        m_groupMember->setVisible(false);
    }
    else if (m_mode == DisplayMode::Member)
    {
        const auto* ml = m_model->loadManager().getMemberLoad(m_loadId);
        if (!ml)
        {
            m_isLoading = false;
            return;
        }

        m_lblHeader->setText(tr("Charge sur Barre #%1").arg(m_loadId));
        m_editName->setText(QString::fromStdString(ml->name()));
        m_lblTarget->setText(tr("Élément #%1").arg(ml->elementId()));

        int lcIdx = m_comboLoadCase->findData(ml->loadCaseId());
        if (lcIdx >= 0) m_comboLoadCase->setCurrentIndex(lcIdx);

        int csIdx = m_comboCoordSys->findData(static_cast<int>(ml->coordSystem()));
        if (csIdx >= 0) m_comboCoordSys->setCurrentIndex(csIdx);

        int dirIdx = m_comboDirection->findData(static_cast<int>(ml->direction()));
        if (dirIdx >= 0) m_comboDirection->setCurrentIndex(dirIdx);

        m_spinQ1->setValue(ml->q1());
        m_spinQ2->setValue(ml->q2());
        m_spinX1->setValue(ml->x1());
        m_spinX2->setValue(ml->x2());

        m_groupForces->setVisible(false);
        m_groupMoments->setVisible(false);
        m_groupMember->setVisible(true);
    }

    m_isLoading = false;
}

void LoadPropertiesView::onFieldChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void LoadPropertiesView::applyChanges()
{
    if (!m_model || m_loadId <= 0 || m_mode == DisplayMode::None)
        return;

    if (m_mode == DisplayMode::Nodal)
    {
        auto* nl = m_model->loadManager().getNodalLoad(m_loadId);
        if (!nl) return;

        m_model->pushUndoState(tr("Modifier Charge Nodale #%1").arg(m_loadId).toStdString());

        nl->setName(m_editName->text().trimmed().toStdString());
        nl->setLoadCaseId(m_comboLoadCase->currentData().toInt());
        nl->setCoordSystem(static_cast<TSA::Model::LoadCoordSystem>(m_comboCoordSys->currentData().toInt()));
        nl->setForces(m_spinFx->value(), m_spinFy->value(), m_spinFz->value());
        nl->setMoments(m_spinMx->value(), m_spinMy->value(), m_spinMz->value());

        m_model->notifyNodalLoadModified(m_loadId);
    }
    else if (m_mode == DisplayMode::Member)
    {
        auto* ml = m_model->loadManager().getMemberLoad(m_loadId);
        if (!ml) return;

        m_model->pushUndoState(tr("Modifier Charge sur Barre #%1").arg(m_loadId).toStdString());

        ml->setName(m_editName->text().trimmed().toStdString());
        ml->setLoadCaseId(m_comboLoadCase->currentData().toInt());
        ml->setCoordSystem(static_cast<TSA::Model::LoadCoordSystem>(m_comboCoordSys->currentData().toInt()));
        ml->setDirection(static_cast<TSA::Model::LoadDirection>(m_comboDirection->currentData().toInt()));
        ml->setQ1(m_spinQ1->value());
        ml->setQ2(m_spinQ2->value());
        ml->setX1(m_spinX1->value());
        ml->setX2(m_spinX2->value());

        m_model->notifyMemberLoadModified(m_loadId);
    }

    emit loadModified();
}

} // namespace TSA::UI
