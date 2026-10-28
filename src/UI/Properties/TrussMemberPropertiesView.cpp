#include "TrussMemberPropertiesView.h"
#include "../../UndoRedo/EditTransaction.h"
#include "ElementColorField.h"
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
    form->addRow(tr("Couleur d'affichage :"), TSA::UI::ElementColor::row(m_btnColor, this, [this]() {
        m_colorHex.clear();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }));

    mainLayout->addWidget(grp);
    m_endNodes = new MemberEndNodesWidget(this);
    connect(m_endNodes, &MemberEndNodesWidget::nodeRequested, this, &IElementPropertyView::nodeRequested);
    mainLayout->addWidget(m_endNodes);
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
    // Le champ ne représente qu'un rond plein : une cornière, un tube ou un profilé n'est pas modifiable ici.
    m_sectionEditable = trm->section().shape == TSA::Model::SectionShape::Circular;
    m_spinDiameter->setEnabled(m_sectionEditable);
    m_spinDiameter->setToolTip(m_sectionEditable ? QString() : tr("Section %1 : non modifiable dans ce panneau (dialogue des sections).").arg(QString::fromStdString(trm->section().name)));

    // Matériau absent de la liste (importé, personnalisé) : aucun choix affiché, jamais remplacé par un autre.
    m_comboMaterial->setCurrentIndex(m_comboMaterial->findData(trm->material().id));

    m_lblNodes->setText(QString("N%1 → N%2").arg(trm->startNodeId()).arg(trm->endNodeId()));
    m_endNodes->setModel(m_model);
    m_endNodes->setNodes(trm->startNodeId(), trm->endNodeId());

    m_colorHex = QString::fromStdString(trm->color()); // vide : apparence du matériau
    TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, trm->material());
    m_colorMaterial = trm->material();
    captureEditedFields();

    m_isLoading = false;
}

void TrussMemberPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(TSA::UI::ElementColor::initial(m_btnColor), this, tr("Couleur de la Barre de Treillis"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
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

    const std::string undoName = tr("Modification Treillis %1").arg(m_memberId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    TSA::UndoRedo::EditTransaction tx(*m_model, undoName, undoName);

    trm->setName(m_editName->text().toStdString());
    if (m_edited.changed(m_comboRole)) trm->setRole(static_cast<TSA::Model::TrussMemberRole>(m_comboRole->currentData().toInt()));
    // Seuls les champs modifiés sont réécrits : renommer une barre en cornière ne la remplace plus par un rond plein.
    if (m_sectionEditable && m_edited.changed(m_spinDiameter)) trm->setSection(TSA::Model::Section::circular(m_spinDiameter->value()));
    if (m_edited.changed(m_comboMaterial))
    {
        if (const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(m_comboMaterial->currentData().toInt()))
            trm->setMaterial(*pMat);
    }
    trm->setColor(m_colorHex.toStdString());

    captureEditedFields();
    // Rien de changé : ni entrée Annuler, ni notification, résultats conservés (BUG-130).
    if (!tx.hasChanges())
    {
        tx.discardUnchanged();
        return;
    }
    m_model->notifyTrussMemberModified(m_memberId);   // dans la transaction : une exception d'observateur annule tout (BUG-129)
    tx.commit();
    emit elementModified();
}

void TrussMemberPropertiesView::captureEditedFields()
{
    m_edited.capture({ m_comboRole, m_spinDiameter, m_comboMaterial });
}

} // namespace TSA::UI
