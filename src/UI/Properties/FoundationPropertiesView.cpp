#include "FoundationPropertiesView.h"
#include "../../Model/Model.h"
#include "../../Model/Foundation.h"

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

FoundationPropertiesView::FoundationPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void FoundationPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void FoundationPropertiesView::setElementId(int id)
{
    m_foundationId = id;
    refreshView();
}

void FoundationPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grp = new QGroupBox(tr("Propriétés de la Fondation / Semelle"), this);
    auto* form = new QFormLayout(grp);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_editName = new QLineEdit(grp);
    connect(m_editName, &QLineEdit::editingFinished, this, &FoundationPropertiesView::onWidgetChanged);
    form->addRow(tr("Désignation :"), m_editName);

    m_comboType = new QComboBox(grp);
    m_comboType->addItem(tr("Semelle Isolée Rectangulaire"), static_cast<int>(TSA::Model::FoundationType::IsolatedFooting));
    m_comboType->addItem(tr("Semelle Filante Continue"), static_cast<int>(TSA::Model::FoundationType::StripFooting));
    m_comboType->addItem(tr("Radier Général"), static_cast<int>(TSA::Model::FoundationType::Raft));
    m_comboType->addItem(tr("Massif sur Pieux"), static_cast<int>(TSA::Model::FoundationType::Pile));
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FoundationPropertiesView::onWidgetChanged);
    form->addRow(tr("Type de Fondation :"), m_comboType);

    m_spinWidthA = new QDoubleSpinBox(grp);
    m_spinWidthA->setRange(0.2, 50.0);
    m_spinWidthA->setDecimals(2);
    m_spinWidthA->setSingleStep(0.1);
    m_spinWidthA->setSuffix(" m");
    connect(m_spinWidthA, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FoundationPropertiesView::onWidgetChanged);
    form->addRow(tr("Largeur (A) :"), m_spinWidthA);

    m_spinLengthB = new QDoubleSpinBox(grp);
    m_spinLengthB->setRange(0.2, 50.0);
    m_spinLengthB->setDecimals(2);
    m_spinLengthB->setSingleStep(0.1);
    m_spinLengthB->setSuffix(" m");
    connect(m_spinLengthB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FoundationPropertiesView::onWidgetChanged);
    form->addRow(tr("Longueur (B) :"), m_spinLengthB);

    m_spinHeightH = new QDoubleSpinBox(grp);
    m_spinHeightH->setRange(0.1, 10.0);
    m_spinHeightH->setDecimals(2);
    m_spinHeightH->setSingleStep(0.1);
    m_spinHeightH->setSuffix(" m");
    connect(m_spinHeightH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FoundationPropertiesView::onWidgetChanged);
    form->addRow(tr("Épaisseur / Hauteur (H) :"), m_spinHeightH);

    m_lblNode = new QLabel(grp);
    m_lblNode->setStyleSheet("color: #64748B; font-weight: bold;");
    form->addRow(tr("Nœud porteur :"), m_lblNode);

    m_btnColor = new QPushButton(grp);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &FoundationPropertiesView::pickColor);
    form->addRow(tr("Couleur d'affichage :"), m_btnColor);

    mainLayout->addWidget(grp);
    mainLayout->addStretch();
}

void FoundationPropertiesView::refreshView()
{
    if (!m_model || m_foundationId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* f = m_model->getFoundation(m_foundationId);
    if (!f)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(f->name()));

    int tIdx = m_comboType->findData(static_cast<int>(f->foundationType()));
    if (tIdx >= 0) m_comboType->setCurrentIndex(tIdx);

    m_spinWidthA->setValue(f->widthA());
    m_spinLengthB->setValue(f->lengthB());
    m_spinHeightH->setValue(f->heightH());

    m_lblNode->setText(QString("N%1").arg(f->nodeId()));

    m_colorHex = QString::fromStdString(f->color().empty() ? "#64748B" : f->color());
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));

    m_isLoading = false;
}

void FoundationPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Couleur de la Fondation"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));
        applyChanges();
    }
}

void FoundationPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void FoundationPropertiesView::applyChanges()
{
    if (!m_model || m_foundationId <= 0) return;
    auto* f = m_model->getFoundation(m_foundationId);
    if (!f) return;

    const std::string undoName = tr("Modification Fondation %1").arg(m_foundationId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    m_model->pushUndoState(undoName, undoName);

    f->setName(m_editName->text().toStdString());
    f->setFoundationType(static_cast<TSA::Model::FoundationType>(m_comboType->currentData().toInt()));
    f->setWidthA(m_spinWidthA->value());
    f->setLengthB(m_spinLengthB->value());
    f->setHeightH(m_spinHeightH->value());
    f->setColor(m_colorHex.toStdString());

    m_model->notifyFoundationModified(m_foundationId);
    emit elementModified();
}

} // namespace TSA::UI
