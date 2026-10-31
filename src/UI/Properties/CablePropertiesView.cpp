#include "CablePropertiesView.h"
#include "ElementColorField.h"
#include "../../UndoRedo/EditTransaction.h"
#include "../../Model/Model.h"
#include "../../Model/Cable/Cable.h"
#include "../../Library/CableLibrary.h"

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

CablePropertiesView::CablePropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void CablePropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void CablePropertiesView::setElementId(int id)
{
    m_cableId = id;
    refreshView();
}

void CablePropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // 1. Général & Type de Câble
    auto* grpGen = new QGroupBox(tr("Identification et Type de Câble"), this);
    auto* formGen = new QFormLayout(grpGen);
    formGen->setContentsMargins(8, 8, 8, 8);
    formGen->setSpacing(6);

    m_editName = new QLineEdit(grpGen);
    connect(m_editName, &QLineEdit::editingFinished, this, &CablePropertiesView::onWidgetChanged);
    formGen->addRow(tr("Désignation :"), m_editName);

    m_comboType = new QComboBox(grpGen);
    m_comboType->addItem(tr("Hauban de structure (Stay Cable)"), static_cast<int>(TSA::Model::CableType::StayCable));
    m_comboType->addItem(tr("Toron Élémentaire (Strand)"), static_cast<int>(TSA::Model::CableType::Strand));
    m_comboType->addItem(tr("Câble Porteur Suspendu (Suspension Cable)"), static_cast<int>(TSA::Model::CableType::SuspensionCable));
    m_comboType->addItem(tr("Suspente (Hanger)"), static_cast<int>(TSA::Model::CableType::Hanger));
    m_comboType->addItem(tr("Tirant d'ancrage (Ground Anchor)"), static_cast<int>(TSA::Model::CableType::GroundAnchor));
    m_comboType->addItem(tr("Câble Générique (Generic)"), static_cast<int>(TSA::Model::CableType::Generic));
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CablePropertiesView::onCableTypeChanged);
    formGen->addRow(tr("Norme / Type :"), m_comboType);

    m_comboGeomMode = new QComboBox(grpGen);
    m_comboGeomMode->addItem(tr("Corde Droite (Traction Pure)"), static_cast<int>(TSA::Model::CableGeometryMode::Straight));
    m_comboGeomMode->addItem(tr("Chaînette Parabole (Poids Propre et Flèche)"), static_cast<int>(TSA::Model::CableGeometryMode::Catenary));
    connect(m_comboGeomMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CablePropertiesView::onWidgetChanged);
    formGen->addRow(tr("Modèle Géométrique :"), m_comboGeomMode);

    m_lblStandardInfo = new QLabel(tr("Spécification : EN 12385-4 / Eurocode 3-1-11"), grpGen);
    m_lblStandardInfo->setStyleSheet("color: #64748B; font-size: 11px;");
    formGen->addRow(m_lblStandardInfo);

    mainLayout->addWidget(grpGen);

    // 2. Section & Paramètres Mécaniques
    auto* grpMech = new QGroupBox(tr("Caractéristiques Mécaniques et Pré-Tension"), this);
    auto* formMech = new QFormLayout(grpMech);
    formMech->setContentsMargins(8, 8, 8, 8);
    formMech->setSpacing(6);

    m_spinDiameter = new QDoubleSpinBox(grpMech);
    m_spinDiameter->setRange(1.0, 500.0);
    m_spinDiameter->setDecimals(1);
    m_spinDiameter->setSuffix(" mm");
    connect(m_spinDiameter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CablePropertiesView::onWidgetChanged);
    formMech->addRow(tr("Diamètre nominal (Ø) :"), m_spinDiameter);

    m_spinArea = new QDoubleSpinBox(grpMech);
    m_spinArea->setRange(0.1, 1000000.0);
    m_spinArea->setDecimals(1);
    m_spinArea->setSuffix(" mm²");
    connect(m_spinArea, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CablePropertiesView::onWidgetChanged);
    formMech->addRow(tr("Section métallique (Am) :"), m_spinArea);

    m_spinModulus = new QDoubleSpinBox(grpMech);
    m_spinModulus->setRange(10.0, 500.0);
    m_spinModulus->setDecimals(1);
    m_spinModulus->setSuffix(" GPa");
    connect(m_spinModulus, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CablePropertiesView::onWidgetChanged);
    formMech->addRow(tr("Module de Young (E) :"), m_spinModulus);

    m_spinPrestress = new QDoubleSpinBox(grpMech);
    m_spinPrestress->setRange(0.0, 100000.0);
    m_spinPrestress->setDecimals(2);
    m_spinPrestress->setSuffix(" kN");
    connect(m_spinPrestress, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CablePropertiesView::onWidgetChanged);
    formMech->addRow(tr("Tension initiale (T0) :"), m_spinPrestress);

    m_spinSag = new QDoubleSpinBox(grpMech);
    m_spinSag->setRange(0.0, 100.0);
    m_spinSag->setDecimals(3);
    m_spinSag->setSuffix(" m");
    connect(m_spinSag, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CablePropertiesView::onWidgetChanged);
    formMech->addRow(tr("Flèche maximale (f) :"), m_spinSag);

    mainLayout->addWidget(grpMech);

    // 3. Extrémités & Ancrages
    auto* grpAnchor = new QGroupBox(tr("Ancrages et Rendu Visuel"), this);
    auto* formAnchor = new QFormLayout(grpAnchor);
    formAnchor->setContentsMargins(8, 8, 8, 8);
    formAnchor->setSpacing(6);

    m_comboStartAnchor = new QComboBox(grpAnchor);
    m_comboStartAnchor->addItem(tr("Ancrage Fixe / Butée"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_comboStartAnchor->addItem(tr("Ancrage Articulé (Chape / Œil)"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_comboStartAnchor->addItem(tr("Ancrage Actif Précontrainte"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_comboStartAnchor->addItem(tr("Tirant / Ancrage Terrain"), static_cast<int>(TSA::Model::AnchorType::GroundAnchor));
    m_comboStartAnchor->addItem(tr("Ancrage Solidaire"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));
    connect(m_comboStartAnchor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CablePropertiesView::onWidgetChanged);
    formAnchor->addRow(tr("Ancrage Début :"), m_comboStartAnchor);

    m_comboEndAnchor = new QComboBox(grpAnchor);
    m_comboEndAnchor->addItem(tr("Ancrage Fixe / Butée"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_comboEndAnchor->addItem(tr("Ancrage Articulé (Chape / Œil)"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_comboEndAnchor->addItem(tr("Ancrage Actif Précontrainte"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_comboEndAnchor->addItem(tr("Tirant / Ancrage Terrain"), static_cast<int>(TSA::Model::AnchorType::GroundAnchor));
    m_comboEndAnchor->addItem(tr("Ancrage Solidaire"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));
    connect(m_comboEndAnchor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CablePropertiesView::onWidgetChanged);
    formAnchor->addRow(tr("Ancrage Fin :"), m_comboEndAnchor);

    m_btnColor = new QPushButton(grpAnchor);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &CablePropertiesView::pickColor);
    formAnchor->addRow(tr("Couleur d'affichage :"), TSA::UI::ElementColor::row(m_btnColor, this, [this]() {
        m_colorHex.clear();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }));

    mainLayout->addWidget(grpAnchor);
    mainLayout->addStretch();
}

void CablePropertiesView::refreshLibraries()
{
    // Synchronisation avec TSALib si des catalogues externes sont rechargés
}

void CablePropertiesView::refreshView()
{
    if (!m_model || m_cableId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* cable = m_model->getCable(m_cableId);
    if (!cable)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(cable->name()));

    int typeIdx = m_comboType->findData(static_cast<int>(cable->type()));
    if (typeIdx >= 0) m_comboType->setCurrentIndex(typeIdx);

    int modeIdx = m_comboGeomMode->findData(static_cast<int>(cable->geometryMode()));
    if (modeIdx >= 0) m_comboGeomMode->setCurrentIndex(modeIdx);

    m_spinDiameter->setValue(cable->diameter() * 1000.0);
    m_spinArea->setValue(cable->definition().area() * 1e6);
    m_spinModulus->setValue(cable->definition().elasticModulus() / 1e9);
    m_spinPrestress->setValue(cable->prestress().initialTension / 1000.0);
    m_spinSag->setValue(cable->geometry().sag());

    int saIdx = m_comboStartAnchor->findData(static_cast<int>(cable->startAnchor().type()));
    if (saIdx >= 0) m_comboStartAnchor->setCurrentIndex(saIdx);

    int eaIdx = m_comboEndAnchor->findData(static_cast<int>(cable->endAnchor().type()));
    if (eaIdx >= 0) m_comboEndAnchor->setCurrentIndex(eaIdx);

    m_colorHex = QString::fromStdString(cable->color()); // vide : apparence du matériau
    TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, cable->material());
    m_colorMaterial = cable->material();
    captureEditedFields();

    m_isLoading = false;
}

void CablePropertiesView::onCableTypeChanged(int /*index*/)
{
    if (m_isLoading) return;
    auto t = static_cast<TSA::Model::CableType>(m_comboType->currentData().toInt());
    switch (t)
    {
    case TSA::Model::CableType::Strand:
        m_lblStandardInfo->setText(tr("Spécification : Toron 7 fils EN 10138-3 / E ≈ 195 GPa"));
        m_spinModulus->setValue(195.0);
        break;
    case TSA::Model::CableType::StayCable:
        m_lblStandardInfo->setText(tr("Spécification : Hauban de pont Eurocode 3-1-11 / E ≈ 205 GPa"));
        m_spinModulus->setValue(205.0);
        break;
    case TSA::Model::CableType::SuspensionCable:
        m_lblStandardInfo->setText(tr("Spécification : Câble porteur principal suspendu / E ≈ 200 GPa"));
        m_spinModulus->setValue(200.0);
        break;
    case TSA::Model::CableType::Hanger:
        m_lblStandardInfo->setText(tr("Spécification : Suspente de pont suspendu / E ≈ 160 GPa"));
        m_spinModulus->setValue(160.0);
        break;
    default:
        m_lblStandardInfo->setText(tr("Spécification : Câble structural générique / E ≈ 160 GPa"));
        m_spinModulus->setValue(160.0);
        break;
    }
    applyChanges();
}

void CablePropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(TSA::UI::ElementColor::initial(m_btnColor), this, tr("Couleur du Câble"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        TSA::UI::ElementColor::showOn(m_btnColor, m_colorHex, m_colorMaterial);
        applyChanges();
    }
}

void CablePropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void CablePropertiesView::applyChanges()
{
    if (!m_model || m_cableId <= 0) return;
    auto* cable = m_model->getCable(m_cableId);
    if (!cable) return;

    const std::string undoName = tr("Modification Câble %1").arg(m_cableId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    TSA::UndoRedo::EditTransaction tx(*m_model, undoName, undoName);

    cable->setName(m_editName->text().toStdString());
    if (m_edited.changed(m_comboType)) cable->setType(static_cast<TSA::Model::CableType>(m_comboType->currentData().toInt()));
    if (m_edited.changed(m_comboGeomMode))
        cable->setGeometryMode(static_cast<TSA::Model::CableGeometryMode>(m_comboGeomMode->currentData().toInt()));

    // Seuls les champs modifiés sont réécrits : les valeurs affichées sont arrondies (1 décimale en mm, mm², GPa) et
    // les réécrire changerait silencieusement l'aire ou le module de la définition (BUG-125).
    if (m_edited.changed(m_spinDiameter)) cable->setDiameter(m_spinDiameter->value() / 1000.0);
    if (m_edited.changed(m_spinArea)) cable->definition().setArea(m_spinArea->value() / 1e6);
    if (m_edited.changed(m_spinModulus)) cable->definition().setElasticModulus(m_spinModulus->value() * 1e9);
    if (m_edited.changed(m_spinPrestress))
    {
        cable->prestress().initialTension = m_spinPrestress->value() * 1000.0;
        cable->definition().setInitialTension(cable->prestress().initialTension);
    }
    if (m_edited.changed(m_spinSag)) cable->geometry().setSag(m_spinSag->value());

    if (m_edited.changed(m_comboStartAnchor))
        cable->startAnchor().setType(static_cast<TSA::Model::AnchorType>(m_comboStartAnchor->currentData().toInt()));
    if (m_edited.changed(m_comboEndAnchor))
        cable->endAnchor().setType(static_cast<TSA::Model::AnchorType>(m_comboEndAnchor->currentData().toInt()));
    cable->setColor(m_colorHex.toStdString());
    captureEditedFields();

    // Rien de changé : ni entrée Annuler, ni notification, résultats conservés (BUG-130).
    if (!tx.hasChanges())
    {
        tx.discardUnchanged();
        return;
    }
    m_model->notifyCableModified(m_cableId);   // dans la transaction : une exception d'observateur annule tout (BUG-129)
    tx.commit();
    emit elementModified();
}

void CablePropertiesView::captureEditedFields()
{
    m_edited.capture({ m_comboType, m_comboGeomMode, m_spinDiameter, m_spinArea, m_spinModulus, m_spinPrestress, m_spinSag,
                       m_comboStartAnchor, m_comboEndAnchor });
}

} // namespace TSA::UI
