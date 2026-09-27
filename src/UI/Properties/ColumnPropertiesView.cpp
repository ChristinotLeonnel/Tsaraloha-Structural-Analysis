#include "ColumnPropertiesView.h"
#include "../Widgets/SectionPreviewWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Column.h"
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

ColumnPropertiesView::ColumnPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void ColumnPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void ColumnPropertiesView::setElementId(int id)
{
    m_columnId = id;
    refreshView();
}

void ColumnPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // 1. Général
    auto* grpGen = new QGroupBox(tr("Général & Matériau"), this);
    auto* formGen = new QFormLayout(grpGen);
    formGen->setContentsMargins(8, 8, 8, 8);
    formGen->setSpacing(6);

    m_editName = new QLineEdit(grpGen);
    connect(m_editName, &QLineEdit::editingFinished, this, &ColumnPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Désignation :"), m_editName);

    m_comboMaterial = new QComboBox(grpGen);
    refreshLibraries();
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Matériau :"), m_comboMaterial);

    m_spinRotation = new QDoubleSpinBox(grpGen);
    m_spinRotation->setRange(-360.0, 360.0);
    m_spinRotation->setDecimals(1);
    m_spinRotation->setSingleStep(5.0);
    m_spinRotation->setSuffix(" °");
    connect(m_spinRotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColumnPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Angle Gamma (Rotation) :"), m_spinRotation);

    m_lblNodes = new QLabel(grpGen);
    m_lblNodes->setStyleSheet("color: #64748B; font-weight: bold;");
    formGen->addRow(tr("Implantation :"), m_lblNodes);

    mainLayout->addWidget(grpGen);

    // 2. Section Transversale
    auto* grpSec = new QGroupBox(tr("Section Transversale"), this);
    auto* formSec = new QFormLayout(grpSec);
    formSec->setContentsMargins(8, 8, 8, 8);
    formSec->setSpacing(6);

    m_comboSectionType = new QComboBox(grpSec);
    m_comboSectionType->addItem(tr("Rectangulaire / Carrée"), static_cast<int>(TSA::Model::SectionShape::Rectangular));
    m_comboSectionType->addItem(tr("Circulaire Pleine"), static_cast<int>(TSA::Model::SectionShape::Circular));
    m_comboSectionType->addItem(tr("Tube Rectangulaire / Creux"), static_cast<int>(TSA::Model::SectionShape::BoxHollow));
    connect(m_comboSectionType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnPropertiesView::onSectionTypeChanged);
    formSec->addRow(tr("Type de Section :"), m_comboSectionType);

    m_spinWidth = new QDoubleSpinBox(grpSec);
    m_spinWidth->setRange(0.05, 5.0);
    m_spinWidth->setDecimals(3);
    m_spinWidth->setSingleStep(0.05);
    m_spinWidth->setSuffix(" m");
    connect(m_spinWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColumnPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Largeur (b) :"), m_spinWidth);

    m_spinHeight = new QDoubleSpinBox(grpSec);
    m_spinHeight->setRange(0.05, 5.0);
    m_spinHeight->setDecimals(3);
    m_spinHeight->setSingleStep(0.05);
    m_spinHeight->setSuffix(" m");
    connect(m_spinHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColumnPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Hauteur (h) :"), m_spinHeight);

    m_spinRadius = new QDoubleSpinBox(grpSec);
    m_spinRadius->setRange(0.02, 3.0);
    m_spinRadius->setDecimals(3);
    m_spinRadius->setSingleStep(0.02);
    m_spinRadius->setSuffix(" m");
    connect(m_spinRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ColumnPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Rayon (r) :"), m_spinRadius);

    m_previewWidget = new SectionPreviewWidget(grpSec);
    m_previewWidget->setFixedHeight(120);
    formSec->addRow(m_previewWidget);

    mainLayout->addWidget(grpSec);

    // 3. Couleur
    auto* grpCol = new QGroupBox(tr("Affichage"), this);
    auto* formCol = new QFormLayout(grpCol);
    m_btnColor = new QPushButton(grpCol);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &ColumnPropertiesView::pickColor);
    formCol->addRow(tr("Couleur du poteau :"), m_btnColor);
    mainLayout->addWidget(grpCol);

    mainLayout->addStretch();
}

void ColumnPropertiesView::refreshLibraries()
{
    if (!m_comboMaterial) return;
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void ColumnPropertiesView::updateSectionVisibility(int secType)
{
    auto t = static_cast<TSA::Model::SectionShape>(secType);
    bool isRect = (t == TSA::Model::SectionShape::Rectangular || t == TSA::Model::SectionShape::BoxHollow);
    bool isCirc = (t == TSA::Model::SectionShape::Circular);

    m_spinWidth->setVisible(isRect);
    m_spinHeight->setVisible(isRect);
    m_spinRadius->setVisible(isCirc);
}

void ColumnPropertiesView::onSectionTypeChanged(int index)
{
    updateSectionVisibility(m_comboSectionType->itemData(index).toInt());
    if (!m_isLoading) onWidgetChanged();
}

TSA::Model::Section ColumnPropertiesView::getSectionFromUi() const
{
    auto t = static_cast<TSA::Model::SectionShape>(m_comboSectionType->currentData().toInt());
    if (t == TSA::Model::SectionShape::Circular)
    {
        return TSA::Model::Section::circular(m_spinRadius->value() * 2.0, "Poteau Circulaire");
    }
    return TSA::Model::Section::rectangular(m_spinWidth->value(), m_spinHeight->value(), "Poteau Rectangulaire");
}

void ColumnPropertiesView::refreshView()
{
    if (!m_model || m_columnId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* col = m_model->getColumn(m_columnId);
    if (!col)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(col->name()));

    const auto& sec = col->section();
    int sIdx = m_comboSectionType->findData(static_cast<int>(sec.shape));
    if (sIdx >= 0) m_comboSectionType->setCurrentIndex(sIdx);
    updateSectionVisibility(static_cast<int>(sec.shape));

    m_spinWidth->setValue(sec.width);
    m_spinHeight->setValue(sec.height);
    m_spinRadius->setValue(sec.diameter / 2.0);
    m_spinRotation->setValue(col->rotation());

    int mIdx = m_comboMaterial->findData(col->material().id);
    if (mIdx >= 0) m_comboMaterial->setCurrentIndex(mIdx);

    m_lblNodes->setText(QString("N%1 (Pied) → N%2 (Tête)").arg(col->startNodeId()).arg(col->endNodeId()));

    if (m_previewWidget)
    {
        m_previewWidget->setSection(sec);
        m_previewWidget->setRotation(col->rotation());
    }

    m_colorHex = QString::fromStdString(col->color().empty() ? "#D97706" : col->color());
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));

    m_isLoading = false;
}

void ColumnPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Couleur du Poteau"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));
        applyChanges();
    }
}

void ColumnPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void ColumnPropertiesView::applyChanges()
{
    if (!m_model || m_columnId <= 0) return;
    auto* col = m_model->getColumn(m_columnId);
    if (!col) return;

    m_model->pushUndoState(tr("Modification Poteau %1").arg(m_columnId).toStdString());

    col->setName(m_editName->text().toStdString());

    auto sec = getSectionFromUi();
    col->setSection(sec);

    int matCode = m_comboMaterial->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        col->setMaterial(*pMat);
    }
    col->setRotation(m_spinRotation->value());
    col->setColor(m_colorHex.toStdString());

    if (m_previewWidget)
    {
        m_previewWidget->setSection(sec);
        m_previewWidget->setRotation(col->rotation());
    }

    m_model->notifyColumnModified(m_columnId);
    emit elementModified();
}

} // namespace TSA::UI
