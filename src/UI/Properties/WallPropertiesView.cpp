#include "WallPropertiesView.h"
#include "ElementColorField.h"
#include "../../Model/Model.h"
#include "../../Model/Wall.h"
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

WallPropertiesView::WallPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void WallPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void WallPropertiesView::setElementId(int id)
{
    m_wallId = id;
    refreshView();
}

void WallPropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grp = new QGroupBox(tr("Propriétés du Voile / Mur Porteur"), this);
    auto* form = new QFormLayout(grp);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_editName = new QLineEdit(grp);
    connect(m_editName, &QLineEdit::editingFinished, this, &WallPropertiesView::onWidgetChanged);
    form->addRow(tr("Désignation :"), m_editName);

    m_spinThickness = new QDoubleSpinBox(grp);
    m_spinThickness->setRange(0.05, 5.0);
    m_spinThickness->setDecimals(3);
    m_spinThickness->setSingleStep(0.05);
    m_spinThickness->setSuffix(" m");
    connect(m_spinThickness, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WallPropertiesView::onWidgetChanged);
    form->addRow(tr("Épaisseur (t) :"), m_spinThickness);

    m_spinHeight = new QDoubleSpinBox(grp);
    m_spinHeight->setRange(0.1, 100.0);
    m_spinHeight->setDecimals(2);
    m_spinHeight->setSingleStep(0.5);
    m_spinHeight->setSuffix(" m");
    connect(m_spinHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WallPropertiesView::onWidgetChanged);
    form->addRow(tr("Hauteur (H) :"), m_spinHeight);

    m_comboMaterial = new QComboBox(grp);
    refreshLibraries();
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WallPropertiesView::onWidgetChanged);
    form->addRow(tr("Matériau :"), m_comboMaterial);

    m_lblNodes = new QLabel(grp);
    m_lblNodes->setStyleSheet("color: #64748B; font-weight: bold;");
    form->addRow(tr("Nœuds de base :"), m_lblNodes);

    m_btnColor = new QPushButton(grp);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &WallPropertiesView::pickColor);
    form->addRow(tr("Couleur d'affichage :"), TSA::UI::ElementColor::row(m_btnColor, this, [this]() {
        m_colorHex.clear();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }));

    mainLayout->addWidget(grp);
    mainLayout->addStretch();
}

void WallPropertiesView::refreshLibraries()
{
    if (!m_comboMaterial) return;
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void WallPropertiesView::refreshView()
{
    if (!m_model || m_wallId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* wall = m_model->getWall(m_wallId);
    if (!wall)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(wall->name()));
    m_spinThickness->setValue(wall->thickness());
    m_spinHeight->setValue(wall->height());

    int matIdx = m_comboMaterial->findData(wall->material().id);
    if (matIdx >= 0) m_comboMaterial->setCurrentIndex(matIdx);

    m_lblNodes->setText(QString("N%1 → N%2").arg(wall->startNodeId()).arg(wall->endNodeId()));

    m_colorHex = QString::fromStdString(wall->color()); // vide : apparence du matériau
    TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, wall->material());
    m_colorMaterial = wall->material();

    m_isLoading = false;
}

void WallPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(TSA::UI::ElementColor::initial(m_btnColor), this, tr("Couleur du Voile"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }
}

void WallPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void WallPropertiesView::applyChanges()
{
    if (!m_model || m_wallId <= 0) return;
    auto* wall = m_model->getWall(m_wallId);
    if (!wall) return;

    const std::string undoName = tr("Modification Voile %1").arg(m_wallId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    m_model->pushUndoState(undoName, undoName);

    wall->setName(m_editName->text().toStdString());
    wall->setThickness(m_spinThickness->value());
    wall->setHeight(m_spinHeight->value());

    int matCode = m_comboMaterial->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        wall->setMaterial(*pMat);
    }
    wall->setColor(m_colorHex.toStdString());

    m_model->notifyWallModified(m_wallId);
    emit elementModified();
}

} // namespace TSA::UI
