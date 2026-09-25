#include "StructurePresetDialog.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QColorDialog>
#include <QStackedWidget>
#include <QDialogButtonBox>

namespace TSA::UI
{

StructurePresetDialog::StructurePresetDialog(
    TSA::Model::StructurePresets& presets,
    PresetTarget initialTarget,
    QWidget* parent)
    : QDialog(parent)
    , m_presets(presets)
    , m_initialTarget(initialTarget)
{
    setupUi();
    loadFromPresets();

    switch (m_initialTarget)
    {
    case PresetTarget::Wall:
        m_tabWidget->setCurrentIndex(0);
        setWindowTitle(tr("Propriétés du Voile (Mur Porteur) - Pré-dessin 3D"));
        break;
    case PresetTarget::Slab:
        m_tabWidget->setCurrentIndex(1);
        setWindowTitle(tr("Propriétés de la Dalle - Pré-dessin 3D"));
        break;
    case PresetTarget::Beam:
        m_tabWidget->setCurrentIndex(2);
        setWindowTitle(tr("Propriétés de la Poutre - Pré-dessin 3D"));
        break;
    case PresetTarget::Column:
        m_tabWidget->setCurrentIndex(3);
        setWindowTitle(tr("Propriétés du Poteau - Pré-dessin 3D"));
        break;
    }
}

TSA::Model::StructurePresets StructurePresetDialog::presets() const
{
    return m_presets;
}

void StructurePresetDialog::setupUi()
{
    resize(480, 420);
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    // En-tête informatif
    auto* headerWidget = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto* lblHeader = new QLabel(
        tr("<b>Paramètres de modélisation initiale</b><br>"
           "<span style='color:#8B949E; font-size:11px;'>"
           "Définissez les propriétés appliquées automatiquement aux structures lors du tracé 3D."
           "</span>"), headerWidget);
    headerLayout->addWidget(lblHeader);
    mainLayout->addWidget(headerWidget);

    // Onglets par type d'élément
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createWallTab(), tr("Voile / Mur"));
    m_tabWidget->setTabIcon(0, QIcon(":/icons/struct_wall.svg"));
    m_tabWidget->addTab(createSlabTab(), tr("Dalle"));
    m_tabWidget->setTabIcon(1, QIcon(":/icons/draw_slab.svg"));
    m_tabWidget->addTab(createBeamTab(), tr("Poutre"));
    m_tabWidget->setTabIcon(2, QIcon(":/icons/draw_beam.svg"));
    m_tabWidget->addTab(createColumnTab(), tr("Poteau"));
    m_tabWidget->setTabIcon(3, QIcon(":/icons/draw_column.svg"));
    m_tabWidget->setIconSize(QSize(18, 18));
    mainLayout->addWidget(m_tabWidget, 1);

    // Option : afficher avant chaque dessin
    m_chkShowEveryTime = new QCheckBox(tr("Afficher cette boîte avant chaque dessin d'élément"), this);
    m_chkShowEveryTime->setChecked(m_presets.showDialogBeforeDrawing);
    mainLayout->addWidget(m_chkShowEveryTime);

    // Boutons de validation
    auto* btnBox = new QDialogButtonBox(this);
    auto* btnOk = btnBox->addButton(tr("Démarrer le Dessin 3D"), QDialogButtonBox::AcceptRole);
    btnOk->setIcon(QIcon(":/icons/apply.svg"));
    auto* btnCancel = btnBox->addButton(tr("Annuler"), QDialogButtonBox::RejectRole);
    btnCancel->setIcon(QIcon(":/icons/cancel.svg"));

    btnOk->setDefault(true);
    btnOk->setStyleSheet(
        "QPushButton { background-color: #1F6FEB; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #388BFD; }"
        "QPushButton:pressed { background-color: #1158C7; }"
    );

    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToPresets();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);
}

void StructurePresetDialog::updateColorButton(QPushButton* btn, const QString& hexColor, const QString& defaultHex)
{
    QString c = hexColor.isEmpty() ? defaultHex : hexColor;
    btn->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #6E7681; border-radius: 3px; min-width: 60px; min-height: 22px; }").arg(c));
    btn->setText(hexColor.isEmpty() ? tr("Par défaut") : hexColor.toUpper());
}

QWidget* StructurePresetDialog::createWallTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QFormLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_wallThickness = new QDoubleSpinBox(w);
    m_wallThickness->setRange(0.05, 2.00);
    m_wallThickness->setSingleStep(0.02);
    m_wallThickness->setDecimals(2);
    m_wallThickness->setSuffix(" m");
    m_wallThickness->setValue(m_presets.wall.thickness);
    layout->addRow(tr("Épaisseur e :"), m_wallThickness);

    m_wallHeight = new QDoubleSpinBox(w);
    m_wallHeight->setRange(0.50, 50.00);
    m_wallHeight->setSingleStep(0.10);
    m_wallHeight->setDecimals(2);
    m_wallHeight->setSuffix(" m");
    m_wallHeight->setValue(m_presets.wall.height);
    layout->addRow(tr("Hauteur H :"), m_wallHeight);

    m_wallOffset = new QDoubleSpinBox(w);
    m_wallOffset->setRange(-2.00, 2.00);
    m_wallOffset->setSingleStep(0.05);
    m_wallOffset->setDecimals(2);
    m_wallOffset->setSuffix(" m");
    m_wallOffset->setValue(m_presets.wall.offset);
    layout->addRow(tr("Décalage d'axe :"), m_wallOffset);

    m_wallMaterial = new QComboBox(w);
    m_wallMaterial->addItems({ "Béton C25/30", "Béton C30/37", "Béton C20/25", "Acier S355", "Acier S235" });
    layout->addRow(tr("Matériau :"), m_wallMaterial);

    auto* colorLayout = new QHBoxLayout();
    m_wallColorBtn = new QPushButton(w);
    colorLayout->addWidget(m_wallColorBtn);

    auto* resetColor = new QPushButton(tr("Défaut"), w);
    resetColor->setFixedWidth(55);
    colorLayout->addWidget(resetColor);
    layout->addRow(tr("Couleur 3D :"), colorLayout);

    connect(m_wallColorBtn, &QPushButton::clicked, this, [this]() {
        QColor current(m_wallColor.isEmpty() ? "#9E9E9E" : m_wallColor);
        QColor chosen = QColorDialog::getColor(current, this, tr("Couleur du Voile"));
        if (chosen.isValid())
        {
            m_wallColor = chosen.name();
            updateColorButton(m_wallColorBtn, m_wallColor, "#9E9E9E");
        }
    });

    connect(resetColor, &QPushButton::clicked, this, [this]() {
        m_wallColor = "";
        updateColorButton(m_wallColorBtn, m_wallColor, "#9E9E9E");
    });

    updateColorButton(m_wallColorBtn, m_presets.wall.color.c_str(), "#9E9E9E");
    return w;
}

QWidget* StructurePresetDialog::createSlabTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QFormLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_slabThickness = new QDoubleSpinBox(w);
    m_slabThickness->setRange(0.05, 2.00);
    m_slabThickness->setSingleStep(0.02);
    m_slabThickness->setDecimals(2);
    m_slabThickness->setSuffix(" m");
    m_slabThickness->setValue(m_presets.slab.thickness);
    layout->addRow(tr("Épaisseur e :"), m_slabThickness);

    m_slabMaterial = new QComboBox(w);
    m_slabMaterial->addItems({ "Béton C25/30", "Béton C30/37", "Béton C20/25", "Acier S355" });
    layout->addRow(tr("Matériau :"), m_slabMaterial);

    auto* colorLayout = new QHBoxLayout();
    m_slabColorBtn = new QPushButton(w);
    colorLayout->addWidget(m_slabColorBtn);

    auto* resetColor = new QPushButton(tr("Défaut"), w);
    resetColor->setFixedWidth(55);
    colorLayout->addWidget(resetColor);
    layout->addRow(tr("Couleur 3D :"), colorLayout);

    connect(m_slabColorBtn, &QPushButton::clicked, this, [this]() {
        QColor current(m_slabColor.isEmpty() ? "#78909C" : m_slabColor);
        QColor chosen = QColorDialog::getColor(current, this, tr("Couleur de la Dalle"));
        if (chosen.isValid())
        {
            m_slabColor = chosen.name();
            updateColorButton(m_slabColorBtn, m_slabColor, "#78909C");
        }
    });

    connect(resetColor, &QPushButton::clicked, this, [this]() {
        m_slabColor = "";
        updateColorButton(m_slabColorBtn, m_slabColor, "#78909C");
    });

    updateColorButton(m_slabColorBtn, m_presets.slab.color.c_str(), "#78909C");
    return w;
}

QWidget* StructurePresetDialog::createBeamTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QFormLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_beamShapeType = new QComboBox(w);
    m_beamShapeType->addItems({ tr("Rectangulaire"), tr("Circulaire"), tr("Profilé IPE"), tr("Profilé HEA"), tr("Profilé HEB") });
    layout->addRow(tr("Type de Section :"), m_beamShapeType);

    m_beamShapeStack = new QStackedWidget(w);

    // Page 0 : Rectangulaire
    auto* pageRect = new QWidget(m_beamShapeStack);
    auto* layRect = new QFormLayout(pageRect);
    layRect->setContentsMargins(0, 0, 0, 0);
    m_beamRectWidth = new QDoubleSpinBox(pageRect);
    m_beamRectWidth->setRange(0.05, 5.00);
    m_beamRectWidth->setSingleStep(0.05);
    m_beamRectWidth->setDecimals(2);
    m_beamRectWidth->setSuffix(" m");
    m_beamRectWidth->setValue(m_presets.beam.section.width);
    layRect->addRow(tr("Largeur b :"), m_beamRectWidth);

    m_beamRectHeight = new QDoubleSpinBox(pageRect);
    m_beamRectHeight->setRange(0.05, 5.00);
    m_beamRectHeight->setSingleStep(0.05);
    m_beamRectHeight->setDecimals(2);
    m_beamRectHeight->setSuffix(" m");
    m_beamRectHeight->setValue(m_presets.beam.section.height);
    layRect->addRow(tr("Hauteur h :"), m_beamRectHeight);
    m_beamShapeStack->addWidget(pageRect);

    // Page 1 : Circulaire
    auto* pageCirc = new QWidget(m_beamShapeStack);
    auto* layCirc = new QFormLayout(pageCirc);
    layCirc->setContentsMargins(0, 0, 0, 0);
    m_beamCircDiam = new QDoubleSpinBox(pageCirc);
    m_beamCircDiam->setRange(0.05, 5.00);
    m_beamCircDiam->setSingleStep(0.05);
    m_beamCircDiam->setDecimals(2);
    m_beamCircDiam->setSuffix(" m");
    m_beamCircDiam->setValue(m_presets.beam.section.diameter > 0 ? m_presets.beam.section.diameter : 0.30);
    layCirc->addRow(tr("Diamètre D :"), m_beamCircDiam);
    m_beamShapeStack->addWidget(pageCirc);

    // Page 2 : IPE
    auto* pageIpe = new QWidget(m_beamShapeStack);
    auto* layIpe = new QFormLayout(pageIpe);
    layIpe->setContentsMargins(0, 0, 0, 0);
    m_beamIpeNumber = new QComboBox(pageIpe);
    m_beamIpeNumber->addItems({ "IPE 160", "IPE 200", "IPE 240", "IPE 270", "IPE 300", "IPE 330", "IPE 360", "IPE 400" });
    m_beamIpeNumber->setCurrentText("IPE 200");
    layIpe->addRow(tr("Profilé :"), m_beamIpeNumber);
    m_beamShapeStack->addWidget(pageIpe);

    // Page 3 : HEA
    auto* pageHea = new QWidget(m_beamShapeStack);
    auto* layHea = new QFormLayout(pageHea);
    layHea->setContentsMargins(0, 0, 0, 0);
    m_beamHeaNumber = new QComboBox(pageHea);
    m_beamHeaNumber->addItems({ "HEA 160", "HEA 200", "HEA 240", "HEA 300" });
    m_beamHeaNumber->setCurrentText("HEA 200");
    layHea->addRow(tr("Profilé :"), m_beamHeaNumber);
    m_beamShapeStack->addWidget(pageHea);

    // Page 4 : HEB
    auto* pageHeb = new QWidget(m_beamShapeStack);
    auto* layHeb = new QFormLayout(pageHeb);
    layHeb->setContentsMargins(0, 0, 0, 0);
    m_beamHebNumber = new QComboBox(pageHeb);
    m_beamHebNumber->addItems({ "HEB 160", "HEB 200", "HEB 240", "HEB 300" });
    m_beamHebNumber->setCurrentText("HEB 200");
    layHeb->addRow(tr("Profilé :"), m_beamHebNumber);
    m_beamShapeStack->addWidget(pageHeb);

    layout->addRow(m_beamShapeStack);

    connect(m_beamShapeType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_beamShapeStack, &QStackedWidget::setCurrentIndex);

    m_beamBetaAngle = new QDoubleSpinBox(w);
    m_beamBetaAngle->setRange(0.0, 360.0);
    m_beamBetaAngle->setSingleStep(15.0);
    m_beamBetaAngle->setSuffix("°");
    m_beamBetaAngle->setValue(m_presets.beam.betaAngle);
    layout->addRow(tr("Rotation Beta :"), m_beamBetaAngle);

    m_beamMaterial = new QComboBox(w);
    m_beamMaterial->addItems({ "Béton C25/30", "Béton C30/37", "Acier S355", "Acier S235", "Bois C24" });
    layout->addRow(tr("Matériau :"), m_beamMaterial);

    auto* colorLayout = new QHBoxLayout();
    m_beamColorBtn = new QPushButton(w);
    colorLayout->addWidget(m_beamColorBtn);

    auto* resetColor = new QPushButton(tr("Défaut"), w);
    resetColor->setFixedWidth(55);
    colorLayout->addWidget(resetColor);
    layout->addRow(tr("Couleur 3D :"), colorLayout);

    connect(m_beamColorBtn, &QPushButton::clicked, this, [this]() {
        QColor current(m_beamColor.isEmpty() ? "#42A5F5" : m_beamColor);
        QColor chosen = QColorDialog::getColor(current, this, tr("Couleur de la Poutre"));
        if (chosen.isValid())
        {
            m_beamColor = chosen.name();
            updateColorButton(m_beamColorBtn, m_beamColor, "#42A5F5");
        }
    });

    connect(resetColor, &QPushButton::clicked, this, [this]() {
        m_beamColor = "";
        updateColorButton(m_beamColorBtn, m_beamColor, "#42A5F5");
    });

    updateColorButton(m_beamColorBtn, m_presets.beam.color.c_str(), "#42A5F5");
    return w;
}

QWidget* StructurePresetDialog::createColumnTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QFormLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_colShapeType = new QComboBox(w);
    m_colShapeType->addItems({ tr("Rectangulaire"), tr("Circulaire"), tr("Profilé IPE"), tr("Profilé HEA") });
    layout->addRow(tr("Type de Section :"), m_colShapeType);

    m_colShapeStack = new QStackedWidget(w);

    // Page 0 : Rectangulaire
    auto* pageRect = new QWidget(m_colShapeStack);
    auto* layRect = new QFormLayout(pageRect);
    layRect->setContentsMargins(0, 0, 0, 0);
    m_colRectWidth = new QDoubleSpinBox(pageRect);
    m_colRectWidth->setRange(0.05, 5.00);
    m_colRectWidth->setSingleStep(0.05);
    m_colRectWidth->setDecimals(2);
    m_colRectWidth->setSuffix(" m");
    m_colRectWidth->setValue(m_presets.column.section.width);
    layRect->addRow(tr("Largeur b :"), m_colRectWidth);

    m_colRectHeight = new QDoubleSpinBox(pageRect);
    m_colRectHeight->setRange(0.05, 5.00);
    m_colRectHeight->setSingleStep(0.05);
    m_colRectHeight->setDecimals(2);
    m_colRectHeight->setSuffix(" m");
    m_colRectHeight->setValue(m_presets.column.section.height);
    layRect->addRow(tr("Hauteur h :"), m_colRectHeight);
    m_colShapeStack->addWidget(pageRect);

    // Page 1 : Circulaire
    auto* pageCirc = new QWidget(m_colShapeStack);
    auto* layCirc = new QFormLayout(pageCirc);
    layCirc->setContentsMargins(0, 0, 0, 0);
    m_colCircDiam = new QDoubleSpinBox(pageCirc);
    m_colCircDiam->setRange(0.05, 5.00);
    m_colCircDiam->setSingleStep(0.05);
    m_colCircDiam->setDecimals(2);
    m_colCircDiam->setSuffix(" m");
    m_colCircDiam->setValue(m_presets.column.section.diameter > 0 ? m_presets.column.section.diameter : 0.30);
    layCirc->addRow(tr("Diamètre D :"), m_colCircDiam);
    m_colShapeStack->addWidget(pageCirc);

    // Page 2 : IPE
    auto* pageIpe = new QWidget(m_colShapeStack);
    auto* layIpe = new QFormLayout(pageIpe);
    layIpe->setContentsMargins(0, 0, 0, 0);
    m_colIpeNumber = new QComboBox(pageIpe);
    m_colIpeNumber->addItems({ "IPE 160", "IPE 200", "IPE 240", "IPE 300" });
    m_colIpeNumber->setCurrentText("IPE 200");
    layIpe->addRow(tr("Profilé :"), m_colIpeNumber);
    m_colShapeStack->addWidget(pageIpe);

    // Page 3 : HEA
    auto* pageHea = new QWidget(m_colShapeStack);
    auto* layHea = new QFormLayout(pageHea);
    layHea->setContentsMargins(0, 0, 0, 0);
    m_colHeaNumber = new QComboBox(pageHea);
    m_colHeaNumber->addItems({ "HEA 160", "HEA 200", "HEA 240", "HEA 300" });
    m_colHeaNumber->setCurrentText("HEA 200");
    layHea->addRow(tr("Profilé :"), m_colHeaNumber);
    m_colShapeStack->addWidget(pageHea);

    layout->addRow(m_colShapeStack);

    connect(m_colShapeType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_colShapeStack, &QStackedWidget::setCurrentIndex);

    m_colBetaAngle = new QDoubleSpinBox(w);
    m_colBetaAngle->setRange(0.0, 360.0);
    m_colBetaAngle->setSingleStep(15.0);
    m_colBetaAngle->setSuffix("°");
    m_colBetaAngle->setValue(m_presets.column.betaAngle);
    layout->addRow(tr("Rotation Beta :"), m_colBetaAngle);

    m_colMaterial = new QComboBox(w);
    m_colMaterial->addItems({ "Béton C25/30", "Béton C30/37", "Acier S355", "Acier S235" });
    layout->addRow(tr("Matériau :"), m_colMaterial);

    auto* colorLayout = new QHBoxLayout();
    m_colColorBtn = new QPushButton(w);
    colorLayout->addWidget(m_colColorBtn);

    auto* resetColor = new QPushButton(tr("Défaut"), w);
    resetColor->setFixedWidth(55);
    colorLayout->addWidget(resetColor);
    layout->addRow(tr("Couleur 3D :"), colorLayout);

    connect(m_colColorBtn, &QPushButton::clicked, this, [this]() {
        QColor current(m_colColor.isEmpty() ? "#66BB6A" : m_colColor);
        QColor chosen = QColorDialog::getColor(current, this, tr("Couleur du Poteau"));
        if (chosen.isValid())
        {
            m_colColor = chosen.name();
            updateColorButton(m_colColorBtn, m_colColor, "#66BB6A");
        }
    });

    connect(resetColor, &QPushButton::clicked, this, [this]() {
        m_colColor = "";
        updateColorButton(m_colColorBtn, m_colColor, "#66BB6A");
    });

    updateColorButton(m_colColorBtn, m_presets.column.color.c_str(), "#66BB6A");
    return w;
}

void StructurePresetDialog::loadFromPresets()
{
    m_wallColor = m_presets.wall.color.c_str();
    m_slabColor = m_presets.slab.color.c_str();
    m_beamColor = m_presets.beam.color.c_str();
    m_colColor = m_presets.column.color.c_str();

    if (m_wallThickness) m_wallThickness->setValue(m_presets.wall.thickness);
    if (m_wallHeight) m_wallHeight->setValue(m_presets.wall.height);
    if (m_wallOffset) m_wallOffset->setValue(m_presets.wall.offset);
    if (m_wallMaterial) m_wallMaterial->setCurrentText(QString::fromStdString(m_presets.wall.material.name));

    if (m_slabThickness) m_slabThickness->setValue(m_presets.slab.thickness);
    if (m_slabMaterial) m_slabMaterial->setCurrentText(QString::fromStdString(m_presets.slab.material.name));

    if (m_beamRectWidth) m_beamRectWidth->setValue(m_presets.beam.section.width);
    if (m_beamRectHeight) m_beamRectHeight->setValue(m_presets.beam.section.height);
    if (m_beamCircDiam) m_beamCircDiam->setValue(m_presets.beam.section.diameter > 0.0 ? m_presets.beam.section.diameter : (m_presets.beam.section.width > 0.0 ? m_presets.beam.section.width : 0.30));
    if (m_beamBetaAngle) m_beamBetaAngle->setValue(m_presets.beam.betaAngle);
    if (m_beamMaterial) m_beamMaterial->setCurrentText(QString::fromStdString(m_presets.beam.material.name));

    if (m_beamShapeType)
    {
        if (m_presets.beam.section.shape == TSA::Model::SectionShape::Circular)
        {
            m_beamShapeType->setCurrentIndex(1);
            if (m_beamShapeStack) m_beamShapeStack->setCurrentIndex(1);
        }
        else if (m_presets.beam.section.shape == TSA::Model::SectionShape::IShape)
        {
            if (m_presets.beam.section.name.rfind("HEA", 0) == 0)
            {
                m_beamShapeType->setCurrentIndex(3);
                if (m_beamShapeStack) m_beamShapeStack->setCurrentIndex(3);
            }
            else if (m_presets.beam.section.name.rfind("HEB", 0) == 0)
            {
                m_beamShapeType->setCurrentIndex(4);
                if (m_beamShapeStack) m_beamShapeStack->setCurrentIndex(4);
            }
            else
            {
                m_beamShapeType->setCurrentIndex(2);
                if (m_beamShapeStack) m_beamShapeStack->setCurrentIndex(2);
            }
        }
        else
        {
            m_beamShapeType->setCurrentIndex(0);
            if (m_beamShapeStack) m_beamShapeStack->setCurrentIndex(0);
        }
    }

    if (m_colRectWidth) m_colRectWidth->setValue(m_presets.column.section.width);
    if (m_colRectHeight) m_colRectHeight->setValue(m_presets.column.section.height);
    if (m_colCircDiam) m_colCircDiam->setValue(m_presets.column.section.diameter > 0.0 ? m_presets.column.section.diameter : (m_presets.column.section.width > 0.0 ? m_presets.column.section.width : 0.30));
    if (m_colBetaAngle) m_colBetaAngle->setValue(m_presets.column.betaAngle);
    if (m_colMaterial) m_colMaterial->setCurrentText(QString::fromStdString(m_presets.column.material.name));

    if (m_colShapeType)
    {
        if (m_presets.column.section.shape == TSA::Model::SectionShape::Circular)
        {
            m_colShapeType->setCurrentIndex(1);
            if (m_colShapeStack) m_colShapeStack->setCurrentIndex(1);
        }
        else if (m_presets.column.section.shape == TSA::Model::SectionShape::IShape)
        {
            if (m_presets.column.section.name.rfind("HEA", 0) == 0)
            {
                m_colShapeType->setCurrentIndex(3);
                if (m_colShapeStack) m_colShapeStack->setCurrentIndex(3);
            }
            else
            {
                m_colShapeType->setCurrentIndex(2);
                if (m_colShapeStack) m_colShapeStack->setCurrentIndex(2);
            }
        }
        else
        {
            m_colShapeType->setCurrentIndex(0);
            if (m_colShapeStack) m_colShapeStack->setCurrentIndex(0);
        }
    }
}

void StructurePresetDialog::saveToPresets()
{
    m_presets.showDialogBeforeDrawing = m_chkShowEveryTime->isChecked();

    // Voile
    m_presets.wall.thickness = m_wallThickness->value();
    m_presets.wall.height = m_wallHeight->value();
    m_presets.wall.offset = m_wallOffset->value();
    m_presets.wall.material.name = m_wallMaterial->currentText().toStdString();
    m_presets.wall.color = m_wallColor.toStdString();

    // Dalle
    m_presets.slab.thickness = m_slabThickness->value();
    m_presets.slab.material.name = m_slabMaterial->currentText().toStdString();
    m_presets.slab.color = m_slabColor.toStdString();

    // Poutre
    int beamShape = m_beamShapeType->currentIndex();
    if (beamShape == 0) // Rect
    {
        m_presets.beam.section = TSA::Model::Section::rectangular(
            m_beamRectWidth->value(), m_beamRectHeight->value(),
            QString("R%1x%2").arg(m_beamRectWidth->value() * 100, 0, 'f', 0).arg(m_beamRectHeight->value() * 100, 0, 'f', 0).toStdString());
    }
    else if (beamShape == 1) // Circ
    {
        m_presets.beam.section = TSA::Model::Section::circular(
            m_beamCircDiam->value(),
            QString("D%1").arg(m_beamCircDiam->value() * 100, 0, 'f', 0).toStdString());
    }
    else if (beamShape == 2) // IPE
    {
        int num = m_beamIpeNumber->currentText().remove("IPE ").toInt();
        m_presets.beam.section = TSA::Model::Section::ipe(num > 0 ? num : 200);
    }
    else if (beamShape == 3) // HEA
    {
        int num = m_beamHeaNumber->currentText().remove("HEA ").toInt();
        m_presets.beam.section = TSA::Model::Section::hea(num > 0 ? num : 200);
    }
    else if (beamShape == 4) // HEB
    {
        int num = m_beamHebNumber->currentText().remove("HEB ").toInt();
        m_presets.beam.section = TSA::Model::Section::heb(num > 0 ? num : 200);
    }
    m_presets.beam.betaAngle = m_beamBetaAngle->value();
    m_presets.beam.material.name = m_beamMaterial->currentText().toStdString();
    m_presets.beam.color = m_beamColor.toStdString();

    // Poteau
    int colShape = m_colShapeType->currentIndex();
    if (colShape == 0) // Rect
    {
        m_presets.column.section = TSA::Model::Section::rectangular(
            m_colRectWidth->value(), m_colRectHeight->value(),
            QString("R%1x%2").arg(m_colRectWidth->value() * 100, 0, 'f', 0).arg(m_colRectHeight->value() * 100, 0, 'f', 0).toStdString());
    }
    else if (colShape == 1) // Circ
    {
        m_presets.column.section = TSA::Model::Section::circular(
            m_colCircDiam->value(),
            QString("D%1").arg(m_colCircDiam->value() * 100, 0, 'f', 0).toStdString());
    }
    else if (colShape == 2) // IPE
    {
        int num = m_colIpeNumber->currentText().remove("IPE ").toInt();
        m_presets.column.section = TSA::Model::Section::ipe(num > 0 ? num : 200);
    }
    else if (colShape == 3) // HEA
    {
        int num = m_colHeaNumber->currentText().remove("HEA ").toInt();
        m_presets.column.section = TSA::Model::Section::hea(num > 0 ? num : 200);
    }
    m_presets.column.betaAngle = m_colBetaAngle->value();
    m_presets.column.material.name = m_colMaterial->currentText().toStdString();
    m_presets.column.color = m_colColor.toStdString();
}

} // namespace TSA::UI
