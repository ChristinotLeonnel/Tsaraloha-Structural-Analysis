#include "TrussMemberPropertiesView.h"
#include "../../Model/Model.h"
#include "../../Model/TrussMember.h"
#include "../../Model/MaterialLibrary.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>

namespace TSA::UI
{

TrussMemberPropertiesView::TrussMemberPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void TrussMemberPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void TrussMemberPropertiesView::setElementId(int id)
{
    m_memberId = id;
    refreshView();
}

void TrussMemberPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grp = new QGroupBox(tr("Propriétés de la Barre de Treillis"), this);
    auto* form = new QFormLayout(grp);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_editName = new QLineEdit(grp);
    connect(m_editName, &QLineEdit::editingFinished, this, &TrussMemberPropertiesView::onWidgetChanged);
    form->addRow(tr("Désignation :"), m_editName);

    m_comboRole = new QComboBox(grp);
    m_comboRole->addItem(tr("Diagonale de contreventement"), static_cast<int>(TSA::Model::TrussMemberRole::Diagonal));
    m_comboRole->addItem(tr("Membrure Supérieure / Inférieure"), static_cast<int>(TSA::Model::TrussMemberRole::TopChord));
    m_comboRole->addItem(tr("Montant Vertical"), static_cast<int>(TSA::Model::TrussMemberRole::Vertical));
    connect(m_comboRole, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrussMemberPropertiesView::onWidgetChanged);
    form->addRow(tr("Rôle structural :"), m_comboRole);

    m_spinDiameter = new QDoubleSpinBox(grp);
    m_spinDiameter->setRange(0.005, 1.0);
    m_spinDiameter->setDecimals(3);
    m_spinDiameter->setSingleStep(0.01);
    m_spinDiameter->setSuffix(" m");
    connect(m_spinDiameter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TrussMemberPropertiesView::onWidgetChanged);
    form->addRow(tr("Diamètre / Dimension :"), m_spinDiameter);

    m_comboMaterial = new QComboBox(grp);
    refreshLibraries();
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrussMemberPropertiesView::onWidgetChanged);
    form->addRow(tr("Matériau :"), m_comboMaterial);

    m_lblNodes = new QLabel(grp);
    m_lblNodes->setStyleSheet("color: #64748B; font-weight: bold;");
    form->addRow(tr("Nœuds Début / Fin :"), m_lblNodes);

    m_btnColor = new QPushButton(grp);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &TrussMemberPropertiesView::pickColor);
    form->addRow(tr("Couleur d'affichage :"), m_btnColor);

    mainLayout->addWidget(grp);
    mainLayout->addStretch();
}

void TrussMemberPropertiesView::refreshLibraries()
{
    if (!m_comboMaterial) return;
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void TrussMemberPropertiesView::refreshView()
{
    if (!m_model || m_memberId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* trm = m_model->getTrussMember(m_memberId);
    if (!trm)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(trm->name()));

    int rIdx = m_comboRole->findData(static_cast<int>(trm->role()));
    if (rIdx >= 0) m_comboRole->setCurrentIndex(rIdx);

    m_spinDiameter->setValue(trm->section().diameter);

    int mIdx = m_comboMaterial->findData(trm->material().id);
    if (mIdx >= 0) m_comboMaterial->setCurrentIndex(mIdx);

    m_lblNodes->setText(QString("N%1 → N%2").arg(trm->startNodeId()).arg(trm->endNodeId()));

    m_colorHex = QString::fromStdString(trm->color().empty() ? "#EF4444" : trm->color());
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));

    m_isLoading = false;
}

void TrussMemberPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Couleur de la Barre de Treillis"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));
        applyChanges();
    }
}

void TrussMemberPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void TrussMemberPropertiesView::applyChanges()
{
    if (!m_model || m_memberId <= 0) return;
    auto* trm = m_model->getTrussMember(m_memberId);
    if (!trm) return;

    m_model->pushUndoState(tr("Modification Treillis %1").arg(m_memberId).toStdString());

    trm->setName(m_editName->text().toStdString());
    trm->setRole(static_cast<TSA::Model::TrussMemberRole>(m_comboRole->currentData().toInt()));
    trm->setSection(TSA::Model::Section::circular(m_spinDiameter->value()));

    int matCode = m_comboMaterial->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        trm->setMaterial(*pMat);
    }
    trm->setColor(m_colorHex.toStdString());

    m_model->notifyTrussMemberModified(m_memberId);
    emit elementModified();
}

} // namespace TSA::UI
