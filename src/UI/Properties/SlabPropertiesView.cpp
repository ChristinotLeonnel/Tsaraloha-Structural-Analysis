#include "SlabPropertiesView.h"
#include "ElementColorField.h"
#include "../../Model/Model.h"
#include "../../Model/Slab.h"
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

SlabPropertiesView::SlabPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void SlabPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void SlabPropertiesView::setElementId(int id)
{
    m_slabId = id;
    refreshView();
}

void SlabPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grp = new QGroupBox(tr("Propriétés de la Dalle / Plancher"), this);
    auto* form = new QFormLayout(grp);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_editName = new QLineEdit(grp);
    connect(m_editName, &QLineEdit::editingFinished, this, &SlabPropertiesView::onWidgetChanged);
    form->addRow(tr("Désignation :"), m_editName);

    m_spinThickness = new QDoubleSpinBox(grp);
    m_spinThickness->setRange(0.01, 10.0);
    m_spinThickness->setDecimals(3);
    m_spinThickness->setSingleStep(0.02);
    m_spinThickness->setSuffix(" m");
    connect(m_spinThickness, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SlabPropertiesView::onWidgetChanged);
    form->addRow(tr("Épaisseur (h) :"), m_spinThickness);

    m_comboType = new QComboBox(grp);
    m_comboType->addItem(tr("Portance Bidirectionnelle (Two-Way)"), static_cast<int>(TSA::Model::SlabType::TwoWay));
    m_comboType->addItem(tr("Portance Unidirectionnelle (One-Way)"), static_cast<int>(TSA::Model::SlabType::OneWay));
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SlabPropertiesView::onWidgetChanged);
    form->addRow(tr("Comportement Mécanique :"), m_comboType);

    m_comboMaterial = new QComboBox(grp);
    refreshLibraries();
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SlabPropertiesView::onWidgetChanged);
    form->addRow(tr("Matériau :"), m_comboMaterial);

    m_lblNodes = new QLabel(grp);
    m_lblNodes->setStyleSheet("color: #64748B; font-weight: bold;");
    form->addRow(tr("Nœuds de contour :"), m_lblNodes);

    m_btnColor = new QPushButton(grp);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &SlabPropertiesView::pickColor);
    form->addRow(tr("Couleur surfacique :"), TSA::UI::ElementColor::row(m_btnColor, this, [this]() {
        m_colorHex.clear();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }));

    mainLayout->addWidget(grp);
    mainLayout->addStretch();
}

void SlabPropertiesView::refreshLibraries()
{
    if (!m_comboMaterial) return;
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void SlabPropertiesView::refreshView()
{
    if (!m_model || m_slabId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* slab = m_model->getSlab(m_slabId);
    if (!slab)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(slab->name()));
    m_spinThickness->setValue(slab->thickness());

    int typeIdx = m_comboType->findData(static_cast<int>(slab->slabType()));
    if (typeIdx >= 0) m_comboType->setCurrentIndex(typeIdx);

    int matIdx = m_comboMaterial->findData(slab->material().id);
    if (matIdx >= 0) m_comboMaterial->setCurrentIndex(matIdx);

    QString nodesStr = "";
    for (int nid : slab->nodeIds())
    {
        if (!nodesStr.isEmpty()) nodesStr += " → ";
        nodesStr += QString("N%1").arg(nid);
    }
    m_lblNodes->setText(nodesStr);

    m_colorHex = QString::fromStdString(slab->color()); // vide : apparence du matériau
    TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, slab->material());
    m_colorMaterial = slab->material();

    m_isLoading = false;
}

void SlabPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(TSA::UI::ElementColor::initial(m_btnColor), this, tr("Couleur de la Dalle"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }
}

void SlabPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void SlabPropertiesView::applyChanges()
{
    if (!m_model || m_slabId <= 0) return;
    auto* slab = m_model->getSlab(m_slabId);
    if (!slab) return;

    const std::string undoName = tr("Modification Dalle %1").arg(m_slabId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    m_model->pushUndoState(undoName, undoName);

    slab->setName(m_editName->text().toStdString());
    slab->setThickness(m_spinThickness->value());
    slab->setSlabType(static_cast<TSA::Model::SlabType>(m_comboType->currentData().toInt()));

    int matCode = m_comboMaterial->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        slab->setMaterial(*pMat);
    }
    slab->setColor(m_colorHex.toStdString());

    m_model->notifySlabModified(m_slabId);
    emit elementModified();
}

} // namespace TSA::UI
