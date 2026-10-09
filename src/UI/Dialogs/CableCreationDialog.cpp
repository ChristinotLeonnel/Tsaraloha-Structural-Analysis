#include "CableCreationDialog.h"
#include <QEvent>
#include "../../Model/Model.h"
#include "../../Model/MaterialLibrary.h"
#include "../../Viewer/OccView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QMessageBox>
#include <QKeyEvent>
#include <cmath>
#include <sstream>

namespace TSA::UI
{

CableCreationDialog::CableCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_occView(occView)
{
    setWindowTitle(tr("Propriétés du Câble"));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFixedWidth(380);

    setupUi();
    populatePresets();
    populateMaterials();
    updateHighlight(false);

    if (m_model)
    {
        int nextId = m_model->nextCableId();
        m_spinCableId->setValue(nextId);
        m_editName->setText(QString("Cable_%1").arg(nextId));
    }
}

void CableCreationDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Numérotation & Identification
    auto* numLayout = new QHBoxLayout();
    numLayout->setSpacing(6);

    auto* lblCableId = new QLabel(tr("Câble n° :"), this);
    m_spinCableId = new QSpinBox(this);
    m_spinCableId->setRange(1, 999999);
    m_spinCableId->setValue(1);

    auto* lblStep = new QLabel(tr("Pas :"), this);
    lblStep->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_spinStep = new QSpinBox(this);
    m_spinStep->setRange(1, 100);
    m_spinStep->setValue(1);

    numLayout->addWidget(lblCableId);
    numLayout->addWidget(m_spinCableId, 1);
    numLayout->addWidget(lblStep);
    numLayout->addWidget(m_spinStep, 1);
    mainLayout->addLayout(numLayout);

    // 2. Nom
    auto* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(6);
    auto* lblName = new QLabel(tr("Nom :"), this);
    lblName->setFixedWidth(50);
    m_editName = new QLineEdit(this);
    m_editName->setText("Cable_1");
    nameLayout->addWidget(lblName);
    nameLayout->addWidget(m_editName, 1);
    mainLayout->addLayout(nameLayout);

    // 3. Groupe "Section du Câble"
    auto* grpSection = new QGroupBox(tr("Section du Câble"), this);
    auto* secLayout = new QGridLayout(grpSection);
    secLayout->setContentsMargins(8, 12, 8, 8);
    secLayout->setSpacing(6);

    // Type de câble
    secLayout->addWidget(new QLabel(tr("Type :"), grpSection), 0, 0);
    m_comboType = new QComboBox(grpSection);
    m_comboType->addItem(tr("Générique"), static_cast<int>(TSA::Model::CableType::Generic));
    m_comboType->addItem(tr("Toron (Strand)"), static_cast<int>(TSA::Model::CableType::Strand));
    m_comboType->addItem(tr("Fil tréfilé (Wire)"), static_cast<int>(TSA::Model::CableType::Wire));
    m_comboType->addItem(tr("Barre de précontrainte"), static_cast<int>(TSA::Model::CableType::PrestressingBar));
    m_comboType->addItem(tr("Hauban (Stay Cable)"), static_cast<int>(TSA::Model::CableType::StayCable));
    m_comboType->addItem(tr("Câble Porteur (Suspension)"), static_cast<int>(TSA::Model::CableType::SuspensionCable));
    m_comboType->addItem(tr("Suspente (Hanger)"), static_cast<int>(TSA::Model::CableType::Hanger));
    m_comboType->addItem(tr("Précontrainte extérieure"), static_cast<int>(TSA::Model::CableType::ExternalPrestressing));
    m_comboType->addItem(tr("Tirant d'ancrage (Ground Anchor)"), static_cast<int>(TSA::Model::CableType::GroundAnchor));
    secLayout->addWidget(m_comboType, 0, 1, 1, 2);

    // Préréglage de section
    secLayout->addWidget(new QLabel(tr("Gabarit :"), grpSection), 1, 0);
    m_comboPreset = new QComboBox(grpSection);
    secLayout->addWidget(m_comboPreset, 1, 1, 1, 2);

    // Diamètre
    secLayout->addWidget(new QLabel(tr("Diamètre Ø :"), grpSection), 2, 0);
    m_spinDiameter = new QDoubleSpinBox(grpSection);
    m_spinDiameter->setRange(1.0, 500.0);
    m_spinDiameter->setDecimals(1);
    m_spinDiameter->setValue(20.0);
    m_spinDiameter->setSuffix(" mm");
    secLayout->addWidget(m_spinDiameter, 2, 1);

    // Aire métallique
    secLayout->addWidget(new QLabel(tr("Aire A :"), grpSection), 3, 0);
    m_spinArea = new QDoubleSpinBox(grpSection);
    m_spinArea->setRange(0.1, 500000.0);
    m_spinArea->setDecimals(2);
    m_spinArea->setValue(314.16);
    m_spinArea->setSuffix(" mm²");
    secLayout->addWidget(m_spinArea, 3, 1);

    mainLayout->addWidget(grpSection);

    // 4. Groupe "Matériau & Mécanique"
    auto* grpMat = new QGroupBox(tr("Matériau & Caractéristiques Mécaniques"), this);
    auto* matLayout = new QGridLayout(grpMat);
    matLayout->setContentsMargins(8, 12, 8, 8);
    matLayout->setSpacing(6);

    matLayout->addWidget(new QLabel(tr("Matériau :"), grpMat), 0, 0);
    m_comboMaterial = new QComboBox(grpMat);
    matLayout->addWidget(m_comboMaterial, 0, 1, 1, 2);

    matLayout->addWidget(new QLabel(tr("Module E :"), grpMat), 1, 0);
    m_spinModulus = new QDoubleSpinBox(grpMat);
    m_spinModulus->setRange(10.0, 500.0);
    m_spinModulus->setValue(195.0);
    m_spinModulus->setSuffix(" GPa");
    matLayout->addWidget(m_spinModulus, 1, 1);

    matLayout->addWidget(new QLabel(tr("Résistance f_pk :"), grpMat), 2, 0);
    m_spinStrength = new QDoubleSpinBox(grpMat);
    m_spinStrength->setRange(100.0, 3000.0);
    m_spinStrength->setValue(1860.0);
    m_spinStrength->setSuffix(" MPa");
    matLayout->addWidget(m_spinStrength, 2, 1);

    matLayout->addWidget(new QLabel(tr("Rupture F_brk :"), grpMat), 3, 0);
    m_spinBreakingForce = new QDoubleSpinBox(grpMat);
    m_spinBreakingForce->setRange(1.0, 100000.0);
    m_spinBreakingForce->setValue(584.0);
    m_spinBreakingForce->setSuffix(" kN");
    matLayout->addWidget(m_spinBreakingForce, 3, 1);

    mainLayout->addWidget(grpMat);

    // 5. Groupe "Paramètres Spécifiques Câble"
    auto* grpCable = new QGroupBox(tr("Paramètres Spécifiques Câble"), this);
    auto* cableLayout = new QGridLayout(grpCable);
    cableLayout->setContentsMargins(8, 12, 8, 8);
    cableLayout->setSpacing(6);

    cableLayout->addWidget(new QLabel(tr("Tension T₀ :"), grpCable), 0, 0);
    m_spinInitialTension = new QDoubleSpinBox(grpCable);
    m_spinInitialTension->setRange(0.0, 100000.0);
    m_spinInitialTension->setValue(50.0);
    m_spinInitialTension->setSuffix(" kN");
    cableLayout->addWidget(m_spinInitialTension, 0, 1);

    cableLayout->addWidget(new QLabel(tr("Géométrie :"), grpCable), 1, 0);
    m_comboGeomMode = new QComboBox(grpCable);
    m_comboGeomMode->addItem(tr("Droit (Straight)"), static_cast<int>(TSA::Model::CableGeometryMode::Straight));
    m_comboGeomMode->addItem(tr("Parabolique (Sag)"), static_cast<int>(TSA::Model::CableGeometryMode::Parabolic));
    m_comboGeomMode->addItem(tr("Caténaire (Catenary)"), static_cast<int>(TSA::Model::CableGeometryMode::Catenary));
    cableLayout->addWidget(m_comboGeomMode, 1, 1);

    m_lblSag = new QLabel(tr("Flèche f :"), grpCable);
    m_spinSag = new QDoubleSpinBox(grpCable);
    m_spinSag->setRange(0.0, 200.0);
    m_spinSag->setDecimals(3);
    m_spinSag->setValue(0.0);
    m_spinSag->setSuffix(" m");
    m_spinSag->setEnabled(false);
    cableLayout->addWidget(m_lblSag, 2, 0);
    cableLayout->addWidget(m_spinSag, 2, 1);

    cableLayout->addWidget(new QLabel(tr("Ancrage début :"), grpCable), 3, 0);
    m_comboStartAnchor = new QComboBox(grpCable);
    m_comboStartAnchor->addItem(tr("Fixe (Fixed)"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_comboStartAnchor->addItem(tr("Articulé / Chape (Pinned)"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_comboStartAnchor->addItem(tr("Précontrainte / Mors (Prestressing)"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_comboStartAnchor->addItem(tr("Ancrage terrain (Ground)"), static_cast<int>(TSA::Model::AnchorType::GroundAnchor));
    m_comboStartAnchor->addItem(tr("Nœud structurel (Structural)"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));
    cableLayout->addWidget(m_comboStartAnchor, 3, 1);

    cableLayout->addWidget(new QLabel(tr("Ancrage fin :"), grpCable), 4, 0);
    m_comboEndAnchor = new QComboBox(grpCable);
    m_comboEndAnchor->addItem(tr("Fixe (Fixed)"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_comboEndAnchor->addItem(tr("Articulé / Chape (Pinned)"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_comboEndAnchor->addItem(tr("Précontrainte / Mors (Prestressing)"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_comboEndAnchor->addItem(tr("Ancrage terrain (Ground)"), static_cast<int>(TSA::Model::AnchorType::GroundAnchor));
    m_comboEndAnchor->addItem(tr("Nœud structurel (Structural)"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));
    cableLayout->addWidget(m_comboEndAnchor, 4, 1);

    m_chkTensionOnly = new QCheckBox(tr("Élément tendu uniquement (Tension-only)"), grpCable);
    m_chkTensionOnly->setChecked(true);
    cableLayout->addWidget(m_chkTensionOnly, 5, 0, 1, 2);

    mainLayout->addWidget(grpCable);

    // 6. Groupe "Coordonnées des Nœuds"
    auto* grpCoords = new QGroupBox(tr("Coordonnées des Nœuds"), this);
    auto* coordsLayout = new QGridLayout(grpCoords);
    coordsLayout->setContentsMargins(8, 12, 8, 8);
    coordsLayout->setSpacing(6);

    coordsLayout->addWidget(new QLabel(tr("Origine :"), grpCoords), 0, 0);
    m_editOrigin = new QLineEdit(grpCoords);
    m_editOrigin->setPlaceholderText(tr("Cliquez ou tapez 'N1' ou 'x y z'"));
    coordsLayout->addWidget(m_editOrigin, 0, 1);

    coordsLayout->addWidget(new QLabel(tr("Fin :"), grpCoords), 1, 0);
    m_editEnd = new QLineEdit(grpCoords);
    m_editEnd->setPlaceholderText(tr("Cliquez ou tapez 'N2' ou 'x y z'"));
    coordsLayout->addWidget(m_editEnd, 1, 1);

    m_chkChain = new QCheckBox(tr("Étirer (Mode Continu)"), grpCoords);
    m_chkChain->setChecked(true);
    coordsLayout->addWidget(m_chkChain, 2, 0, 1, 2);

    mainLayout->addWidget(grpCoords);

    // 7. Boutons d'Action
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);

    m_btnHelp = new QPushButton(tr("Aide"), this);
    m_btnHelp->setFixedWidth(60);

    m_btnAdd = new QPushButton(tr("Ajouter"), this);
    m_btnAdd->setStyleSheet("font-weight: bold; background-color: #2D68C4; color: white;");

    m_btnClose = new QPushButton(tr("Fermer"), this);

    btnLayout->addWidget(m_btnHelp);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnClose);
    mainLayout->addLayout(btnLayout);

    // Connexions de signaux
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CableCreationDialog::onCableTypeChanged);
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CableCreationDialog::onPresetSectionChanged);
    connect(m_spinDiameter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CableCreationDialog::onDiameterChanged);
    connect(m_spinArea, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CableCreationDialog::onAreaChanged);
    connect(m_comboGeomMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CableCreationDialog::onGeometryModeChanged);

    connect(m_editOrigin, &QLineEdit::returnPressed, this, &CableCreationDialog::onOriginReturnPressed);
    connect(m_editEnd, &QLineEdit::returnPressed, this, &CableCreationDialog::onEndReturnPressed);

    connect(m_btnAdd, &QPushButton::clicked, this, &CableCreationDialog::onAddClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &CableCreationDialog::close);
    connect(m_btnHelp, &QPushButton::clicked, this, &CableCreationDialog::onHelpClicked);
}

void CableCreationDialog::populatePresets()
{
    m_comboPreset->clear();
    m_comboPreset->addItem(tr("Personnalisé..."), 0.0);
    m_comboPreset->addItem(tr("Toron Y1860S7 Ø15.7 mm (150 mm²)"), 15.7);
    m_comboPreset->addItem(tr("Toron Y1860S7 Ø15.2 mm (140 mm²)"), 15.2);
    m_comboPreset->addItem(tr("Toron Y1860S7 Ø12.7 mm (100 mm²)"), 12.7);
    m_comboPreset->addItem(tr("Hauban PSS 19x15.7 Ø90 mm (2850 mm²)"), 90.0);
    m_comboPreset->addItem(tr("Hauban PSS 37x15.7 Ø125 mm (5550 mm²)"), 125.0);
    m_comboPreset->addItem(tr("Câble Clos FLC 50 Ø50 mm (1530 mm²)"), 50.0);
    m_comboPreset->addItem(tr("Câble Clos FLC 80 Ø80 mm (3920 mm²)") , 80.0);
    m_comboPreset->addItem(tr("Câble Clos FLC 120 Ø120 mm (8820 mm²)"), 120.0);
    m_comboPreset->addItem(tr("Suspente 30 Ø30 mm (550 mm²)"), 30.0);
    m_comboPreset->addItem(tr("Barre Y1030 Ø32 mm (804 mm²)"), 32.0);
    m_comboPreset->addItem(tr("Tirant Ø20 mm (314.16 mm²)"), 20.0);
    m_comboPreset->setCurrentIndex(11); // Tirant 20mm par défaut
}

void CableCreationDialog::populateMaterials()
{
    m_comboMaterial->clear();
    const auto& lib = TSA::Model::MaterialLibrary::instance();
    for (const auto& mat : lib.allMaterials())
    {
        m_comboMaterial->addItem(QString::fromStdString(mat.name), mat.id);
    }

    // Sélectionner un acier ou matériau de câble adapté en priorité
    int idx = m_comboMaterial->findText("Acier S355");
    if (idx >= 0) m_comboMaterial->setCurrentIndex(idx);
    else if (m_comboMaterial->count() > 0) m_comboMaterial->setCurrentIndex(0);
}

void CableCreationDialog::updateCalculatedArea()
{
    if (m_isInternalUpdate) return;
    double dMm = m_spinDiameter->value();
    double aMm2 = M_PI * dMm * dMm / 4.0;
    m_isInternalUpdate = true;
    m_spinArea->setValue(aMm2);

    double fpkMpa = m_spinStrength->value();
    double fbrkKn = (aMm2 * fpkMpa) / 1000.0;
    m_spinBreakingForce->setValue(fbrkKn);
    m_isInternalUpdate = false;
}

void CableCreationDialog::onDiameterChanged(double valMm)
{
    (void)valMm;
    updateCalculatedArea();
}

void CableCreationDialog::onAreaChanged(double valMm2)
{
    if (m_isInternalUpdate) return;
    double fpkMpa = m_spinStrength->value();
    double fbrkKn = (valMm2 * fpkMpa) / 1000.0;
    m_isInternalUpdate = true;
    m_spinBreakingForce->setValue(fbrkKn);
    m_isInternalUpdate = false;
}

void CableCreationDialog::onCableTypeChanged(int index)
{
    auto type = static_cast<TSA::Model::CableType>(m_comboType->itemData(index).toInt());
    if (type == TSA::Model::CableType::StayCable)
    {
        m_spinDiameter->setValue(90.0);
        m_spinInitialTension->setValue(200.0);
    }
    else if (type == TSA::Model::CableType::SuspensionCable)
    {
        m_spinDiameter->setValue(120.0);
        m_spinInitialTension->setValue(500.0);
        m_comboGeomMode->setCurrentIndex(1); // Parabolique
    }
    else if (type == TSA::Model::CableType::Hanger)
    {
        m_spinDiameter->setValue(30.0);
        m_spinInitialTension->setValue(30.0);
    }
    else if (type == TSA::Model::CableType::Strand || type == TSA::Model::CableType::PrestressingBar)
    {
        m_spinDiameter->setValue(15.7);
        m_spinInitialTension->setValue(190.0);
    }
    updateCalculatedArea();
}

void CableCreationDialog::onPresetSectionChanged(int index)
{
    double dia = m_comboPreset->itemData(index).toDouble();
    if (dia > 0.1)
    {
        m_spinDiameter->setValue(dia);
        updateCalculatedArea();
    }
}

void CableCreationDialog::onGeometryModeChanged(int index)
{
    auto mode = static_cast<TSA::Model::CableGeometryMode>(m_comboGeomMode->itemData(index).toInt());
    bool isCurved = (mode != TSA::Model::CableGeometryMode::Straight);
    m_spinSag->setEnabled(isCurved);
    if (isCurved && m_spinSag->value() < 1e-4)
    {
        m_spinSag->setValue(1.0);
    }
}

TSA::Model::CableType CableCreationDialog::cableType() const
{
    return static_cast<TSA::Model::CableType>(m_comboType->currentData().toInt());
}

double CableCreationDialog::diameter() const
{
    return m_spinDiameter->value() / 1000.0; // mm -> m
}

double CableCreationDialog::area() const
{
    return m_spinArea->value() / 1e6; // mm² -> m²
}

double CableCreationDialog::elasticModulus() const
{
    return m_spinModulus->value() * 1e9; // GPa -> Pa
}

double CableCreationDialog::characteristicStrength() const
{
    return m_spinStrength->value() * 1e6; // MPa -> Pa
}

double CableCreationDialog::minimumBreakingForce() const
{
    return m_spinBreakingForce->value() * 1000.0; // kN -> N
}

double CableCreationDialog::initialTension() const
{
    return m_spinInitialTension->value() * 1000.0; // kN -> N
}

TSA::Model::CableGeometryMode CableCreationDialog::geometryMode() const
{
    return static_cast<TSA::Model::CableGeometryMode>(m_comboGeomMode->currentData().toInt());
}

double CableCreationDialog::sag() const
{
    return m_spinSag->value();
}

TSA::Model::AnchorType CableCreationDialog::startAnchorType() const
{
    return static_cast<TSA::Model::AnchorType>(m_comboStartAnchor->currentData().toInt());
}

TSA::Model::AnchorType CableCreationDialog::endAnchorType() const
{
    return static_cast<TSA::Model::AnchorType>(m_comboEndAnchor->currentData().toInt());
}

bool CableCreationDialog::isTensionOnly() const
{
    return m_chkTensionOnly->isChecked();
}

bool CableCreationDialog::isChainMode() const
{
    return m_chkChain->isChecked();
}

void CableCreationDialog::onFirstPointPicked(const gp_Pnt& pt, int nodeId)
{
    m_originPt = pt;
    m_originNodeId = nodeId;
    m_hasOrigin = true;

    if (nodeId > 0)
        m_editOrigin->setText(QString("N%1").arg(nodeId));
    else
        m_editOrigin->setText(formatPoint(pt));

    updateHighlight(true);
}

void CableCreationDialog::onSecondPointPicked(const gp_Pnt& pt, int nodeId)
{
    m_endPt = pt;
    m_endNodeId = nodeId;
    m_hasEnd = true;

    if (nodeId > 0)
        m_editEnd->setText(QString("N%1").arg(nodeId));
    else
        m_editEnd->setText(formatPoint(pt));

    onAddClicked();

    if (isChainMode() && m_endNodeId > 0)
    {
        // Chaînage continu : la fin devient la nouvelle origine
        m_originNodeId = m_endNodeId;
        m_originPt = pt;
        m_hasOrigin = true;
        m_hasEnd = false;
        m_editOrigin->setText(QString("N%1").arg(m_originNodeId));
        m_editEnd->clear();
        updateHighlight(true);
    }
    else
    {
        m_hasOrigin = false;
        m_hasEnd = false;
        m_editOrigin->clear();
        m_editEnd->clear();
        updateHighlight(false);
    }
}

void CableCreationDialog::onDrawingCancelled()
{
    m_hasOrigin = false;
    m_hasEnd = false;
    m_editOrigin->clear();
    m_editEnd->clear();
    updateHighlight(false);
}

void CableCreationDialog::onOriginReturnPressed()
{
    double x = 0, y = 0, z = 0;
    if (parseCoordinates(m_editOrigin->text(), x, y, z))
    {
        m_originPt = gp_Pnt(x, y, z);
        m_originNodeId = m_model ? m_model->addNode(x, y, z) : -1;
        m_hasOrigin = true;
        updateHighlight(true);
        m_editEnd->setFocus();
    }
}

void CableCreationDialog::onEndReturnPressed()
{
    double x = 0, y = 0, z = 0;
    if (parseCoordinates(m_editEnd->text(), x, y, z))
    {
        m_endPt = gp_Pnt(x, y, z);
        m_endNodeId = m_model ? m_model->addNode(x, y, z) : -1;
        m_hasEnd = true;
        onAddClicked();
    }
}

void CableCreationDialog::onAddClicked()
{
    if (!m_model) return;

    // Résolution du nœud d'origine
    int startId = m_originNodeId;
    if (startId <= 0)
    {
        double x = 0, y = 0, z = 0;
        if (parseCoordinates(m_editOrigin->text(), x, y, z))
        {
            startId = m_model->addNode(x, y, z);
        }
    }

    // Résolution du nœud de fin
    int endId = m_endNodeId;
    if (endId <= 0)
    {
        double x = 0, y = 0, z = 0;
        if (parseCoordinates(m_editEnd->text(), x, y, z))
        {
            endId = m_model->addNode(x, y, z);
        }
    }

    if (startId <= 0 || endId <= 0)
    {
        QMessageBox::warning(this, tr("Erreur"), tr("Veuillez sélectionner ou saisir les nœuds de départ et de fin du câble."));
        return;
    }

    if (startId == endId)
    {
        QMessageBox::warning(this, tr("Erreur"), tr("Le nœud d'origine et le nœud d'extrémité doivent être distincts."));
        return;
    }

    m_model->pushUndoState(tr("Création Câble %1").arg(m_spinCableId->value()).toStdString());

    int newId = m_model->addCable(startId, endId, cableType());
    auto* cable = m_model->getCable(newId);
    if (cable)
    {
        cable->setName(m_editName->text().toStdString());
        cable->setDiameter(diameter());
        cable->definition().setArea(area());
        cable->definition().setElasticModulus(elasticModulus());
        cable->definition().setCharacteristicStrength(characteristicStrength());
        cable->definition().setMinimumBreakingForce(minimumBreakingForce());

        cable->prestress().initialTension = initialTension();
        cable->definition().setInitialTension(initialTension());

        cable->setGeometryMode(geometryMode());
        cable->geometry().setSag(sag());

        cable->startAnchor().setType(startAnchorType());
        cable->endAnchor().setType(endAnchorType());
        cable->analysisProperties().tensionOnly = isTensionOnly();

        // Matériau
        int matId = m_comboMaterial->currentData().toInt();
        const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matId);
        if (pMat) cable->setMaterial(*pMat);

        m_model->notifyCableModified(newId);
        if (m_occView) m_occView->updateCableShape(newId);
    }

    emit cableCreated(newId);

    // Incrémenter pour le prochain câble
    m_spinCableId->setValue(m_spinCableId->value() + m_spinStep->value());
    m_editName->setText(QString("Cable_%1").arg(m_spinCableId->value()));
}

void CableCreationDialog::loadFromCable(const TSA::Model::Cable& cable)
{
    m_spinCableId->setValue(cable.id());
    m_editName->setText(QString::fromStdString(cable.name()));

    int tIdx = m_comboType->findData(static_cast<int>(cable.type()));
    if (tIdx >= 0) m_comboType->setCurrentIndex(tIdx);

    m_spinDiameter->setValue(cable.diameter() * 1000.0);
    m_spinArea->setValue(cable.metallicArea() * 1e6);
    m_spinModulus->setValue(cable.definition().elasticModulus() / 1e9);
    m_spinStrength->setValue(cable.definition().characteristicStrength() / 1e6);
    m_spinBreakingForce->setValue(cable.definition().minimumBreakingForce() / 1000.0);
    m_spinInitialTension->setValue(cable.initialTension() / 1000.0);

    int gIdx = m_comboGeomMode->findData(static_cast<int>(cable.geometryMode()));
    if (gIdx >= 0) m_comboGeomMode->setCurrentIndex(gIdx);
    m_spinSag->setValue(cable.sag());

    int saIdx = m_comboStartAnchor->findData(static_cast<int>(cable.startAnchor().type()));
    if (saIdx >= 0) m_comboStartAnchor->setCurrentIndex(saIdx);

    int eaIdx = m_comboEndAnchor->findData(static_cast<int>(cable.endAnchor().type()));
    if (eaIdx >= 0) m_comboEndAnchor->setCurrentIndex(eaIdx);

    m_chkTensionOnly->setChecked(cable.tensionOnly());

    m_editOrigin->setText(QString("N%1").arg(cable.startNodeId()));
    m_editEnd->setText(QString("N%1").arg(cable.endNodeId()));
}

void CableCreationDialog::onHelpClicked()
{
    QMessageBox::information(this, tr("Aide - Câbles & Systèmes de Tension"),
        tr("<b>Élément Câble (1D) :</b><br><br>"
           "• <b>Section :</b> Section circulaire définie par son diamètre nominal Ø et son aire métallique A.<br>"
           "• <b>Tension initiale T₀ :</b> Précontrainte axiale appliquée à l'élément.<br>"
           "• <b>Géométrie :</b> Droit (cylindre direct) ou Parabolique/Caténaire selon la flèche f.<br>"
           "• <b>Tension-only :</b> L'élément ne reprend que des efforts de traction.<br>"
           "• <b>Tracé 3D :</b> Cliquez sur deux nœuds successifs dans le viewport pour créer le câble.<br>"
           "• <b>Étirer :</b> Permet de tracer en chaîne plusieurs câbles d'affilée."));
}

void CableCreationDialog::updateHighlight(bool waitingForSecondPoint)
{
    if (waitingForSecondPoint)
    {
        m_editOrigin->setStyleSheet("background-color: #E8F5E9; border: 1px solid #4CAF50;");
        m_editEnd->setStyleSheet("background-color: #FFF9C4; border: 2px solid #FBC02D; font-weight: bold;");
    }
    else
    {
        m_editOrigin->setStyleSheet("background-color: #FFF9C4; border: 2px solid #FBC02D; font-weight: bold;");
        m_editEnd->setStyleSheet("");
    }
}

bool CableCreationDialog::parseCoordinates(const QString& text, double& x, double& y, double& z) const
{
    QString t = text.trimmed();
    if (t.startsWith("N", Qt::CaseInsensitive))
    {
        bool ok = false;
        int nId = t.mid(1).toInt(&ok);
        if (ok && m_model)
        {
            const auto* n = m_model->getNode(nId);
            if (n) { x = n->x(); y = n->y(); z = n->z(); return true; }
        }
    }

    t.replace(',', ' ');
    std::stringstream ss(t.toStdString());
    if (ss >> x >> y >> z) return true;
    return false;
}

QString CableCreationDialog::formatPoint(const gp_Pnt& pt) const
{
    return QString("%1 %2 %3").arg(pt.X(), 0, 'f', 2).arg(pt.Y(), 0, 'f', 2).arg(pt.Z(), 0, 'f', 2);
}

bool CableCreationDialog::event(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate) emit drawingResumeRequested();
    return QDialog::event(event);
}

void CableCreationDialog::closeEvent(QCloseEvent* event)
{
    if (m_occView) m_occView->setInteractionMode(OccView::InteractionMode::Select);
    QDialog::closeEvent(event);
}

void CableCreationDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        onDrawingCancelled();
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace TSA::UI
