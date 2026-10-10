#include "BeamPropertiesView.h"
#include "ElementColorField.h"
#include "../../UndoRedo/UndoManager.h"
#include "../Widgets/SectionPreviewWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Beam.h"
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

BeamPropertiesView::BeamPropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void BeamPropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void BeamPropertiesView::setElementId(int id)
{
    m_beamId = id;
    refreshView();
}

void BeamPropertiesView::setupUi()
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

    m_lblCadType = new QLabel(tr("Poutre"), grpCad);
    m_lblCadType->setStyleSheet("font-weight: bold; color: #f8fafc;");
    formCad->addRow(tr("Type :"), m_lblCadType);

    m_lblCadId = new QLabel("B-0", grpCad);
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
    m_endNodes = new MemberEndNodesWidget(this);
    connect(m_endNodes, &MemberEndNodesWidget::nodeRequested, this, &IElementPropertyView::nodeRequested);
    mainLayout->addWidget(m_endNodes);

    // 1. Général & Rôle
    auto* grpGen = new QGroupBox(tr("Général & Rôle"), this);
    auto* formGen = new QFormLayout(grpGen);
    formGen->setContentsMargins(8, 8, 8, 8);
    formGen->setSpacing(6);

    m_editName = new QLineEdit(grpGen);
    connect(m_editName, &QLineEdit::editingFinished, this, &BeamPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Désignation :"), m_editName);

    m_comboRole = new QComboBox(grpGen);
    m_comboRole->addItem(tr("Poutre (Beam)"), static_cast<int>(TSA::Model::BarRole::Beam));
    m_comboRole->addItem(tr("Poteau (Column)"), static_cast<int>(TSA::Model::BarRole::Column));
    m_comboRole->addItem(tr("Barre générique"), static_cast<int>(TSA::Model::BarRole::Generic));
    connect(m_comboRole, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Rôle structural :"), m_comboRole);

    m_comboMaterial = new QComboBox(grpGen);
    refreshLibraries();
    connect(m_comboMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Matériau :"), m_comboMaterial);

    m_spinRotation = new QDoubleSpinBox(grpGen);
    m_spinRotation->setRange(-360.0, 360.0);
    m_spinRotation->setDecimals(1);
    m_spinRotation->setSingleStep(5.0);
    m_spinRotation->setSuffix(" °");
    connect(m_spinRotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formGen->addRow(tr("Angle Gamma (Rotation) :"), m_spinRotation);

    mainLayout->addWidget(grpGen);

    // 2. Section Transversale
    auto* grpSec = new QGroupBox(tr("Section Transversale"), this);
    auto* formSec = new QFormLayout(grpSec);
    formSec->setContentsMargins(8, 8, 8, 8);
    formSec->setSpacing(6);

    m_comboSectionType = new QComboBox(grpSec);
    m_comboSectionType->addItem(tr("Rectangulaire"), static_cast<int>(TSA::Model::SectionShape::Rectangular));
    m_comboSectionType->addItem(tr("Circulaire Pleine"), static_cast<int>(TSA::Model::SectionShape::Circular));
    m_comboSectionType->addItem(tr("Profilé en I (IPE / HEA)"), static_cast<int>(TSA::Model::SectionShape::IShape));
    m_comboSectionType->addItem(tr("Tube Rectangulaire (RHS)"), static_cast<int>(TSA::Model::SectionShape::BoxHollow));
    connect(m_comboSectionType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamPropertiesView::onSectionTypeChanged);
    formSec->addRow(tr("Type de Profilé :"), m_comboSectionType);

    m_spinWidth = new QDoubleSpinBox(grpSec);
    m_spinWidth->setRange(0.01, 10.0);
    m_spinWidth->setDecimals(3);
    m_spinWidth->setSingleStep(0.05);
    m_spinWidth->setSuffix(" m");
    connect(m_spinWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Largeur (b) :"), m_spinWidth);

    m_spinHeight = new QDoubleSpinBox(grpSec);
    m_spinHeight->setRange(0.01, 10.0);
    m_spinHeight->setDecimals(3);
    m_spinHeight->setSingleStep(0.05);
    m_spinHeight->setSuffix(" m");
    connect(m_spinHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Hauteur (h) :"), m_spinHeight);

    m_spinRadius = new QDoubleSpinBox(grpSec);
    m_spinRadius->setRange(0.005, 5.0);
    m_spinRadius->setDecimals(3);
    m_spinRadius->setSingleStep(0.02);
    m_spinRadius->setSuffix(" m");
    connect(m_spinRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Rayon (r) :"), m_spinRadius);

    m_spinFlangeWidth = new QDoubleSpinBox(grpSec);
    m_spinFlangeWidth->setRange(0.01, 2.0);
    m_spinFlangeWidth->setDecimals(3);
    m_spinFlangeWidth->setSingleStep(0.01);
    m_spinFlangeWidth->setSuffix(" m");
    connect(m_spinFlangeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Largeur Semelle (bf) :"), m_spinFlangeWidth);

    m_spinFlangeThick = new QDoubleSpinBox(grpSec);
    m_spinFlangeThick->setRange(0.001, 0.2);
    m_spinFlangeThick->setDecimals(4);
    m_spinFlangeThick->setSingleStep(0.002);
    m_spinFlangeThick->setSuffix(" m");
    connect(m_spinFlangeThick, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Épaisseur Semelle (tf) :"), m_spinFlangeThick);

    m_spinWebThick = new QDoubleSpinBox(grpSec);
    m_spinWebThick->setRange(0.001, 0.2);
    m_spinWebThick->setDecimals(4);
    m_spinWebThick->setSingleStep(0.002);
    m_spinWebThick->setSuffix(" m");
    connect(m_spinWebThick, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamPropertiesView::onWidgetChanged);
    formSec->addRow(tr("Épaisseur Âme (tw) :"), m_spinWebThick);

    m_previewWidget = new SectionPreviewWidget(grpSec);
    m_previewWidget->setFixedHeight(120);
    formSec->addRow(m_previewWidget);

    m_lblArea = new QLabel(grpSec);
    m_lblArea->setStyleSheet("color: #64748B; font-size: 11px;");
    formSec->addRow(tr("Aire (A) :"), m_lblArea);

    mainLayout->addWidget(grpSec);

    // 3. Couleur
    auto* grpCol = new QGroupBox(tr("Affichage"), this);
    auto* formCol = new QFormLayout(grpCol);
    m_btnColor = new QPushButton(grpCol);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &BeamPropertiesView::pickColor);
    formCol->addRow(tr("Couleur de barre :"), TSA::UI::ElementColor::row(m_btnColor, this, [this]() {
        m_colorHex.clear();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }));
    mainLayout->addWidget(grpCol);

    mainLayout->addStretch();
}

void BeamPropertiesView::refreshLibraries()
{
    if (!m_comboMaterial) return;
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void BeamPropertiesView::updateSectionVisibility(int secType)
{
    auto t = static_cast<TSA::Model::SectionShape>(secType);
    bool isRect = (t == TSA::Model::SectionShape::Rectangular);
    bool isCirc = (t == TSA::Model::SectionShape::Circular);
    bool isI    = (t == TSA::Model::SectionShape::IShape);
    bool isTube = (t == TSA::Model::SectionShape::BoxHollow);

    m_spinWidth->setVisible(isRect || isTube);
    m_spinHeight->setVisible(isRect || isTube || isI);
    m_spinRadius->setVisible(isCirc);
    m_spinFlangeWidth->setVisible(isI);
    m_spinFlangeThick->setVisible(isI);
    m_spinWebThick->setVisible(isI || isTube);
}

void BeamPropertiesView::onSectionTypeChanged(int index)
{
    updateSectionVisibility(m_comboSectionType->itemData(index).toInt());
    if (!m_isLoading) onWidgetChanged();
}

TSA::Model::Section BeamPropertiesView::getSectionFromUi() const
{
    auto t = static_cast<TSA::Model::SectionShape>(m_comboSectionType->currentData().toInt());
    switch (t)
    {
    case TSA::Model::SectionShape::Circular:
        return TSA::Model::Section::circular(m_spinRadius->value() * 2.0, "Circulaire");
    case TSA::Model::SectionShape::IShape:
    {
        TSA::Model::Section sec;
        sec.shape = TSA::Model::SectionShape::IShape;
        sec.height = m_spinHeight->value();
        sec.width = m_spinFlangeWidth->value();
        sec.tw = m_spinWebThick->value();
        sec.tf = m_spinFlangeThick->value();
        sec.name = "Profilé I";
        return sec;
    }
    case TSA::Model::SectionShape::BoxHollow:
        return TSA::Model::Section::boxHollow(m_spinWidth->value(), m_spinHeight->value(),
                                              m_spinWebThick->value(), m_spinFlangeThick->value(), "Tube Rectangulaire");
    case TSA::Model::SectionShape::Rectangular:
    default:
        return TSA::Model::Section::rectangular(m_spinWidth->value(), m_spinHeight->value(), "Rectangulaire");
    }
}

void BeamPropertiesView::updateCalculatedProperties(const TSA::Model::Section& sec)
{
    m_lblArea->setText(QString("%1 cm²").arg(sec.area() * 10000.0, 0, 'f', 1));
}

void BeamPropertiesView::refreshView()
{
    if (!m_model || m_beamId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* beam = m_model->getBeam(m_beamId);
    if (!beam)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    // Mise à jour de l'en-tête CAO (propriétés réelles du modèle)
    m_lblCadId->setText(QString("B-%1").arg(m_beamId));
    m_lblCadType->setText(beam->role() == TSA::Model::BarRole::Column ? tr("Poteau") : tr("Poutre"));

    const auto* startN = m_model->getNode(beam->startNodeId());
    const auto* endN = m_model->getNode(beam->endNodeId());
    const bool vertical = beam->role() == TSA::Model::BarRole::Column;
    m_endNodes->setModel(m_model);
    m_endNodes->setEndLabels(vertical ? tr("Pied :") : tr("Début :"), vertical ? tr("Tête :") : tr("Fin :"));
    m_endNodes->setNodes(beam->startNodeId(), beam->endNodeId());
    if (startN && endN)
    {
        m_lblCadNodes->setText(QString("N%1 → N%2").arg(beam->startNodeId()).arg(beam->endNodeId()));
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
        m_lblCadNodes->setText(QString("N%1 → N%2").arg(beam->startNodeId()).arg(beam->endNodeId()));
        m_lblCadLength->setText(QString("%1 m").arg(beam->length(*m_model), 0, 'f', 2));
        m_lblCadLevel->setText("-");
    }

    const auto& sec = beam->section();
    m_lblCadSection->setText(QString::fromStdString(sec.name.empty() ? "Section personnalisée" : sec.name));
    m_lblCadMaterial->setText(QString::fromStdString(beam->material().name.empty() ? "S355" : beam->material().name));

    m_editName->setText(QString::fromStdString(beam->name()));

    int rIdx = m_comboRole->findData(static_cast<int>(beam->role()));
    if (rIdx >= 0) m_comboRole->setCurrentIndex(rIdx);
    int sIdx = m_comboSectionType->findData(static_cast<int>(sec.shape));
    if (sIdx >= 0) m_comboSectionType->setCurrentIndex(sIdx);
    updateSectionVisibility(static_cast<int>(sec.shape));

    m_spinWidth->setValue(sec.width);
    m_spinHeight->setValue(sec.height);
    m_spinRadius->setValue(sec.diameter / 2.0);
    m_spinFlangeWidth->setValue(sec.width);
    m_spinFlangeThick->setValue(sec.tf);
    m_spinWebThick->setValue(sec.tw);
    m_spinRotation->setValue(beam->rotation());

    int mIdx = m_comboMaterial->findData(beam->material().id);
    if (mIdx >= 0) m_comboMaterial->setCurrentIndex(mIdx);

    if (m_previewWidget)
    {
        m_previewWidget->setSection(sec);
        m_previewWidget->setRotation(beam->rotation());
    }
    updateCalculatedProperties(sec);

    m_colorHex = QString::fromStdString(beam->color()); // vide : apparence du matériau
    TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, beam->material());
    m_colorMaterial = beam->material();

    m_isLoading = false;
}

void BeamPropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(TSA::UI::ElementColor::initial(m_btnColor), this, tr("Couleur de la Poutre"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }
}

void BeamPropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void BeamPropertiesView::applyChanges()
{
    if (!m_model || m_beamId <= 0) return;
    auto* beam = m_model->getBeam(m_beamId);
    if (!beam) return;

    const std::string undoName = tr("Modification Barre %1").arg(m_beamId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    const std::string oldSection = beam->section().name;
    const std::string oldMaterial = beam->material().name;
    m_model->pushUndoState(undoName, undoName);

    beam->setName(m_editName->text().toStdString());
    beam->setRole(static_cast<TSA::Model::BarRole>(m_comboRole->currentData().toInt()));

    auto sec = getSectionFromUi();
    beam->setSection(sec);

    int matCode = m_comboMaterial->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        beam->setMaterial(*pMat);
    }
    beam->setRotation(m_spinRotation->value());

    // Relâchements d'extrémité : retirés du panneau, ceux de la barre (fichier, import IFC) sont conservés.

    beam->setColor(m_colorHex.toStdString());

    if (m_previewWidget)
    {
        m_previewWidget->setSection(sec);
        m_previewWidget->setRotation(beam->rotation());
    }
    updateCalculatedProperties(sec);

    if (auto* um = m_model->undoManager())
    {
        if (beam->section().name != oldSection)
            um->addRecord({ "modify_property", "Beam", m_beamId, "section", oldSection, beam->section().name,
                            { "geometry", "stiffness", "self_weight", "results_invalidated" } });
        if (beam->material().name != oldMaterial)
            um->addRecord({ "modify_property", "Beam", m_beamId, "material", oldMaterial, beam->material().name,
                            { "stiffness", "self_weight", "results_invalidated" } });
    }
    m_model->notifyBeamModified(m_beamId);
    emit elementModified();
}

} // namespace TSA::UI
