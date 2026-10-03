#include "ColumnPropertiesView.h"
#include "../../UndoRedo/UndoManager.h"
#include "../Widgets/SectionPreviewWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Column.h"
#include "../../Model/MaterialLibrary.h"
#include <cmath>

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

    // 0. Récapitulatif CAO professionnel (conforme Robot SA / Revit)
    auto* grpCad = new QGroupBox(tr("ÉLÉMENT"), this);
    grpCad->setStyleSheet("QGroupBox { font-weight: bold; color: #38bdf8; border: 1px solid #334155; margin-top: 6px; padding-top: 8px; border-radius: 4px; }");
    auto* formCad = new QFormLayout(grpCad);
    formCad->setContentsMargins(8, 8, 8, 8);
    formCad->setSpacing(4);

    m_lblCadType = new QLabel(tr("Poteau"), grpCad);
    m_lblCadType->setStyleSheet("font-weight: bold; color: #f8fafc;");
    formCad->addRow(tr("Type :"), m_lblCadType);

    m_lblCadId = new QLabel("C-0", grpCad);
    m_lblCadId->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; color: #38bdf8;");
    formCad->addRow(tr("ID :"), m_lblCadId);

    m_lblCadNodes = new QLabel("N1 → N2", grpCad);
    m_lblCadNodes->setStyleSheet("font-family: Consolas, monospace; color: #94a3b8;");
    formCad->addRow(tr("Nœuds :"), m_lblCadNodes);

    m_lblCadSection = new QLabel("-", grpCad);
    m_lblCadSection->setStyleSheet("font-weight: bold; color: #f8fafc;");
    formCad->addRow(tr("Section :"), m_lblCadSection);

    m_lblCadMaterial = new QLabel("-", grpCad);
    m_lblCadMaterial->setStyleSheet("font-weight: 500; color: #cbd5e1;");
    formCad->addRow(tr("Matériau :"), m_lblCadMaterial);

    m_lblCadLength = new QLabel("0.00 m", grpCad);
    m_lblCadLength->setStyleSheet("font-family: Consolas, monospace; font-weight: bold; color: #34d399;");
    formCad->addRow(tr("Longueur :"), m_lblCadLength);

    m_lblCadLevel = new QLabel("-", grpCad);
    m_lblCadLevel->setStyleSheet("color: #fbbf24; font-weight: 500;");
    formCad->addRow(tr("Niveau :"), m_lblCadLevel);

    mainLayout->addWidget(grpCad);

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

    // Mise à jour de l'en-tête CAO (propriétés réelles du modèle)
    m_lblCadId->setText(QString("C-%1").arg(m_columnId));
    m_lblCadType->setText(tr("Poteau"));

    const auto* startN = m_model->getNode(col->startNodeId());
    const auto* endN = m_model->getNode(col->endNodeId());
    if (startN && endN)
    {
        m_lblCadNodes->setText(QString("N%1 (Pied) → N%2 (Tête)").arg(col->startNodeId()).arg(col->endNodeId()));
        double dx = endN->x() - startN->x();
        double dy = endN->y() - startN->y();
        double dz = endN->z() - startN->z();
        double len = std::sqrt(dx * dx + dy * dy + dz * dz);
        m_lblCadLength->setText(QString("%1 m").arg(len, 0, 'f', 2));

        if (!startN->levelId().empty())
        {
            m_lblCadLevel->setText(QString::fromStdString(startN->levelId()));
        }
        else if (m_model->levelManager())
        {
            const auto* lvl = m_model->levelManager()->findLevelAtElevation(startN->z());
            m_lblCadLevel->setText(lvl ? QString::fromStdString(lvl->name) : QString("Z = %1 m").arg(startN->z(), 0, 'f', 2));
        }
        else
        {
            m_lblCadLevel->setText(QString("Z = %1 m").arg(startN->z(), 0, 'f', 2));
        }
    }
    else
    {
        m_lblCadNodes->setText(QString("N%1 → N%2").arg(col->startNodeId()).arg(col->endNodeId()));
        m_lblCadLength->setText(QString("%1 m").arg(col->length(*m_model), 0, 'f', 2));
        m_lblCadLevel->setText("-");
    }

    const auto& sec = col->section();
    m_lblCadSection->setText(QString::fromStdString(sec.name.empty() ? "Section personnalisée" : sec.name));
    m_lblCadMaterial->setText(QString::fromStdString(col->material().name.empty() ? "Béton C25/30" : col->material().name));

    m_editName->setText(QString::fromStdString(col->name()));

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

    const std::string undoName = tr("Modification Poteau %1").arg(m_columnId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    const std::string oldSection = col->section().name;
    const std::string oldMaterial = col->material().name;
    m_model->pushUndoState(undoName, undoName);

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

    if (auto* um = m_model->undoManager())
    {
        if (col->section().name != oldSection)
            um->addRecord({ "modify_property", "Column", m_columnId, "section", oldSection, col->section().name,
                            { "geometry", "stiffness", "self_weight", "results_invalidated" } });
        if (col->material().name != oldMaterial)
            um->addRecord({ "modify_property", "Column", m_columnId, "material", oldMaterial, col->material().name,
                            { "stiffness", "self_weight", "results_invalidated" } });
    }
    m_model->notifyColumnModified(m_columnId);
    emit elementModified();
}

} // namespace TSA::UI
