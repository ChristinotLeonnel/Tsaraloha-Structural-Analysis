#include "PropertyPanel.h"
#include "../Widgets/SectionPreviewWidget.h"
#include "../../Library/LibraryManager.h"
#include "../../Model/ModelDiff.h"
#include "../../Model/MaterialLibrary.h"
#include "../../Model/Cable/Cable.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QPushButton>
#include <QColorDialog>
#include <QCheckBox>
#include <QIcon>

namespace
{
struct LoadingGuard
{
    bool& flag;
    explicit LoadingGuard(bool& f) : flag(f) { flag = true; }
    ~LoadingGuard() { flag = false; }
};

struct SelfUpdateGuard
{
    bool& flag;
    explicit SelfUpdateGuard(bool& f) : flag(f) { flag = true; }
    ~SelfUpdateGuard() { flag = false; }
};
}

namespace TSA::UI
{

PropertyPanel::PropertyPanel(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    if (m_model)
    {
        m_model->addObserver(this);
    }
    clearProperties();
}

PropertyPanel::~PropertyPanel()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void PropertyPanel::setModel(TSA::Model::Model* model)
{
    if (m_model == model) return;
    if (m_model)
    {
        m_model->removeObserver(this);
    }
    m_model = model;
    if (m_model)
    {
        m_model->addObserver(this);
    }
    clearProperties();
}

void PropertyPanel::refreshLibraryLists()
{
    if (m_beamMaterialCombo) setupMaterialCombo(m_beamMaterialCombo);
    if (m_columnMaterialCombo) setupMaterialCombo(m_columnMaterialCombo);
    if (m_slabMaterialCombo) setupMaterialCombo(m_slabMaterialCombo);
    if (m_wallMaterialCombo) setupMaterialCombo(m_wallMaterialCombo);
    if (m_foundationMaterialCombo) setupMaterialCombo(m_foundationMaterialCombo);
    if (m_trussMaterialCombo) setupMaterialCombo(m_trussMaterialCombo);
    if (m_beamSectionTypeCombo) setupSectionTypeCombo(m_beamSectionTypeCombo);
    if (m_columnSectionTypeCombo) setupSectionTypeCombo(m_columnSectionTypeCombo);
}

void PropertyPanel::setupMaterialCombo(QComboBox* combo)
{
    combo->clear();
    for (const auto& mat : TSA::Model::MaterialLibrary::instance().allMaterials())
    {
        combo->addItem(QString::fromStdString(mat.name), mat.id);
    }
}

void PropertyPanel::setupSectionTypeCombo(QComboBox* combo)
{
    combo->clear();
    combo->addItem(tr("Rectangulaire"), 0);
    combo->addItem(tr("Circulaire"), 1);
    combo->addItem("IPE 100", 100);
    combo->addItem("IPE 160", 160);
    combo->addItem("IPE 200", 200);
    combo->addItem("IPE 240", 240);
    combo->addItem("IPE 300", 300);
    combo->addItem("HEA 160", 1600);
    combo->addItem("HEA 200", 2000);
    combo->addItem("HEA 240", 2400);
    combo->addItem("HEB 160", 1601);
    combo->addItem("HEB 200", 2001);
    combo->addItem("HEB 300", 3001);
    combo->addItem("UPN 100", 5100);
    combo->addItem("UPN 160", 5160);
    combo->addItem("UPN 200", 5200);
    combo->addItem("L 60x60x6", 6060);
    combo->addItem("L 80x80x8", 6080);
    combo->addItem("Tube 100x100x5", 7100);
    combo->addItem("Tube D114x5", 8114);
}

void PropertyPanel::setupColorButton(QPushButton* btn, const QString& hexColor)
{
    if (!btn) return;
    QColor c(hexColor);
    if (!c.isValid()) c = QColor("#007ACC");

    double luminance = (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
    QString textColor = luminance > 0.5 ? "#111111" : "#FFFFFF";

    btn->setText(c.name(QColor::HexRgb).toUpper());
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-weight: bold;"
        "  border: 1px solid #555555;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  text-align: center;"
        "}"
        "QPushButton:hover {"
        "  border: 1px solid #ffffff;"
        "}"
    ).arg(c.name(QColor::HexRgb)).arg(textColor));
}

void PropertyPanel::pickColor(QString& targetColor, QPushButton* targetBtn, const QString& title)
{
    QColor initial(targetColor);
    QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : Qt::white, this, title);
    if (chosen.isValid())
    {
        targetColor = chosen.name(QColor::HexRgb).toUpper();
        setupColorButton(targetBtn, targetColor);
        if (m_chkLiveSync && m_chkLiveSync->isChecked())
        {
            onWidgetChanged();
        }
    }
}

void PropertyPanel::updateBeamSectionVisibility(int secData)
{
    if (secData == 1) // Circulaire
    {
        if (m_beamWidthLabel) m_beamWidthLabel->setText(tr("Diamètre Ø :"));
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamWidthLabel) m_beamWidthLabel->setVisible(true);
        if (m_beamHeightLabel) m_beamHeightLabel->setVisible(false);
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(false);
        if (m_beamTwLabel) m_beamTwLabel->setVisible(false);
        if (m_beamTwSpin) m_beamTwSpin->setVisible(false);
        if (m_beamTfLabel) m_beamTfLabel->setVisible(false);
        if (m_beamTfSpin) m_beamTfSpin->setVisible(false);
    }
    else if (secData == 8114) // Tube circulaire (Pipe)
    {
        if (m_beamWidthLabel) m_beamWidthLabel->setText(tr("Diamètre ext. Ø :"));
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamWidthLabel) m_beamWidthLabel->setVisible(true);
        if (m_beamHeightLabel) m_beamHeightLabel->setVisible(false);
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(false);
        if (m_beamTwLabel) { m_beamTwLabel->setText(tr("Épaisseur t :")); m_beamTwLabel->setVisible(true); }
        if (m_beamTwSpin) m_beamTwSpin->setVisible(true);
        if (m_beamTfLabel) m_beamTfLabel->setVisible(false);
        if (m_beamTfSpin) m_beamTfSpin->setVisible(false);
    }
    else if (secData >= 100 && secData <= 400) // IPE
    {
        if (m_beamWidthLabel) { m_beamWidthLabel->setText(tr("Largeur b :")); m_beamWidthLabel->setVisible(true); }
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamHeightLabel) { m_beamHeightLabel->setText(tr("Hauteur h :")); m_beamHeightLabel->setVisible(true); }
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(true);
        if (m_beamTwLabel) { m_beamTwLabel->setText(tr("Ép. âme tw :")); m_beamTwLabel->setVisible(true); }
        if (m_beamTwSpin) m_beamTwSpin->setVisible(true);
        if (m_beamTfLabel) { m_beamTfLabel->setText(tr("Ép. aile tf :")); m_beamTfLabel->setVisible(true); }
        if (m_beamTfSpin) m_beamTfSpin->setVisible(true);
    }
    else if ((secData >= 1600 && secData <= 2400) || (secData >= 1601 && secData <= 3001) || (secData >= 5100 && secData <= 5300)) // HEA, HEB, UPN
    {
        if (m_beamWidthLabel) { m_beamWidthLabel->setText(tr("Largeur b :")); m_beamWidthLabel->setVisible(true); }
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamHeightLabel) { m_beamHeightLabel->setText(tr("Hauteur h :")); m_beamHeightLabel->setVisible(true); }
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(true);
        if (m_beamTwLabel) { m_beamTwLabel->setText(tr("Ép. âme tw :")); m_beamTwLabel->setVisible(true); }
        if (m_beamTwSpin) m_beamTwSpin->setVisible(true);
        if (m_beamTfLabel) { m_beamTfLabel->setText(tr("Ép. aile tf :")); m_beamTfLabel->setVisible(true); }
        if (m_beamTfSpin) m_beamTfSpin->setVisible(true);
    }
    else if (secData == 6060 || secData == 6080) // Cornière
    {
        if (m_beamWidthLabel) { m_beamWidthLabel->setText(tr("Largeur b :")); m_beamWidthLabel->setVisible(true); }
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamHeightLabel) { m_beamHeightLabel->setText(tr("Hauteur h :")); m_beamHeightLabel->setVisible(true); }
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(true);
        if (m_beamTwLabel) { m_beamTwLabel->setText(tr("Épaisseur t :")); m_beamTwLabel->setVisible(true); }
        if (m_beamTwSpin) m_beamTwSpin->setVisible(true);
        if (m_beamTfLabel) m_beamTfLabel->setVisible(false);
        if (m_beamTfSpin) m_beamTfSpin->setVisible(false);
    }
    else // Rectangulaire par défaut
    {
        if (m_beamWidthLabel) { m_beamWidthLabel->setText(tr("Largeur b :")); m_beamWidthLabel->setVisible(true); }
        if (m_beamWidthSpin) m_beamWidthSpin->setVisible(true);
        if (m_beamHeightLabel) { m_beamHeightLabel->setText(tr("Hauteur h :")); m_beamHeightLabel->setVisible(true); }
        if (m_beamHeightSpin) m_beamHeightSpin->setVisible(true);
        if (m_beamTwLabel) m_beamTwLabel->setVisible(false);
        if (m_beamTwSpin) m_beamTwSpin->setVisible(false);
        if (m_beamTfLabel) m_beamTfLabel->setVisible(false);
        if (m_beamTfSpin) m_beamTfSpin->setVisible(false);
    }
}

void PropertyPanel::updateColumnSectionVisibility(int secData)
{
    if (secData == 1) // Circulaire
    {
        if (m_columnWidthLabel) m_columnWidthLabel->setText(tr("Diamètre Ø :"));
        if (m_columnWidthSpin) m_columnWidthSpin->setVisible(true);
        if (m_columnWidthLabel) m_columnWidthLabel->setVisible(true);
        if (m_columnDepthLabel) m_columnDepthLabel->setVisible(false);
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(false);
        if (m_columnTwLabel) m_columnTwLabel->setVisible(false);
        if (m_columnTwSpin) m_columnTwSpin->setVisible(false);
        if (m_columnTfLabel) m_columnTfLabel->setVisible(false);
        if (m_columnTfSpin) m_columnTfSpin->setVisible(false);
    }
    else if (secData >= 160 && secData <= 400) // IPE
    {
        if (m_columnWidthLabel) { m_columnWidthLabel->setText(tr("Largeur b :")); m_columnWidthLabel->setVisible(true); }
        if (m_columnWidthSpin) m_columnWidthSpin->setVisible(true);
        if (m_columnDepthLabel) { m_columnDepthLabel->setText(tr("Hauteur h :")); m_columnDepthLabel->setVisible(true); }
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(true);
        if (m_columnTwLabel) { m_columnTwLabel->setText(tr("Ép. âme tw :")); m_columnTwLabel->setVisible(true); }
        if (m_columnTwSpin) m_columnTwSpin->setVisible(true);
        if (m_columnTfLabel) { m_columnTfLabel->setText(tr("Ép. aile tf :")); m_columnTfLabel->setVisible(true); }
        if (m_columnTfSpin) m_columnTfSpin->setVisible(true);
    }
    else if (secData == 2000 || secData == 2400 || secData == 2001 || secData == 3001) // HEA / HEB
    {
        if (m_columnWidthLabel) { m_columnWidthLabel->setText(tr("Largeur b :")); m_columnWidthLabel->setVisible(true); }
        if (m_columnWidthSpin) m_columnWidthSpin->setVisible(true);
        if (m_columnDepthLabel) { m_columnDepthLabel->setText(tr("Hauteur h :")); m_columnDepthLabel->setVisible(true); }
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(true);
        if (m_columnTwLabel) { m_columnTwLabel->setText(tr("Ép. âme tw :")); m_columnTwLabel->setVisible(true); }
        if (m_columnTwSpin) m_columnTwSpin->setVisible(true);
        if (m_columnTfLabel) { m_columnTfLabel->setText(tr("Ép. aile tf :")); m_columnTfLabel->setVisible(true); }
        if (m_columnTfSpin) m_columnTfSpin->setVisible(true);
    }
    else // Rectangulaire
    {
        if (m_columnWidthLabel) { m_columnWidthLabel->setText(tr("Largeur b :")); m_columnWidthLabel->setVisible(true); }
        if (m_columnWidthSpin) m_columnWidthSpin->setVisible(true);
        if (m_columnDepthLabel) { m_columnDepthLabel->setText(tr("Profondeur h :")); m_columnDepthLabel->setVisible(true); }
        if (m_columnDepthSpin) m_columnDepthSpin->setVisible(true);
        if (m_columnTwLabel) m_columnTwLabel->setVisible(false);
        if (m_columnTwSpin) m_columnTwSpin->setVisible(false);
        if (m_columnTfLabel) m_columnTfLabel->setVisible(false);
        if (m_columnTfSpin) m_columnTfSpin->setVisible(false);
    }
}

void PropertyPanel::updateBeamCalculatedProperties(const TSA::Model::Section& sec)
{
    if (!m_beamPropArea) return;
    double aCm2 = sec.area() * 10000.0;
    double iyCm4 = sec.iy() * 100000000.0;
    double izCm4 = sec.iz() * 100000000.0;
    double itCm4 = sec.it() * 100000000.0;
    double wyCm3 = sec.wy() * 1000000.0;
    double wzCm3 = sec.wz() * 1000000.0;

    m_beamPropArea->setText(QString("Aire <b>A</b> = %1 cm² (%2 m²)").arg(aCm2, 0, 'f', 1).arg(sec.area(), 0, 'f', 4));
    m_beamPropIy->setText(QString("Inertie <b>Iy</b> = %1 cm⁴ (axe fort)").arg(iyCm4, 0, 'f', 1));
    m_beamPropIz->setText(QString("Inertie <b>Iz</b> = %1 cm⁴ (axe faible)").arg(izCm4, 0, 'f', 1));
    m_beamPropIt->setText(QString("Torsion <b>J</b> = %1 cm⁴").arg(itCm4, 0, 'f', 1));
    m_beamPropW->setText(QString("Module <b>Wel,y</b> = %1 cm³ | <b>Wel,z</b> = %2 cm³").arg(wyCm3, 0, 'f', 1).arg(wzCm3, 0, 'f', 1));

    if (m_beamSectionPreview)
    {
        m_beamSectionPreview->setSection(sec);
    }
}

void PropertyPanel::updateColumnCalculatedProperties(const TSA::Model::Section& sec)
{
    if (!m_columnPropArea) return;
    double aCm2 = sec.area() * 10000.0;
    double iyCm4 = sec.iy() * 100000000.0;
    double izCm4 = sec.iz() * 100000000.0;
    double itCm4 = sec.it() * 100000000.0;
    double wyCm3 = sec.wy() * 1000000.0;
    double wzCm3 = sec.wz() * 1000000.0;

    m_columnPropArea->setText(QString("Aire <b>A</b> = %1 cm² (%2 m²)").arg(aCm2, 0, 'f', 1).arg(sec.area(), 0, 'f', 4));
    m_columnPropIy->setText(QString("Inertie <b>Iy</b> = %1 cm⁴ (axe fort)").arg(iyCm4, 0, 'f', 1));
    m_columnPropIz->setText(QString("Inertie <b>Iz</b> = %1 cm⁴ (axe faible)").arg(izCm4, 0, 'f', 1));
    m_columnPropIt->setText(QString("Torsion <b>J</b> = %1 cm⁴").arg(itCm4, 0, 'f', 1));
    m_columnPropW->setText(QString("Module <b>Wel,y</b> = %1 cm³ | <b>Wel,z</b> = %2 cm³").arg(wyCm3, 0, 'f', 1).arg(wzCm3, 0, 'f', 1));

    if (m_columnSectionPreview)
    {
        m_columnSectionPreview->setSection(sec);
    }
}

TSA::Model::Section PropertyPanel::getBeamSectionFromUi() const
{
    int secData = m_beamSectionTypeCombo ? m_beamSectionTypeCombo->currentData().toInt() : 0;
    double w = m_beamWidthSpin ? m_beamWidthSpin->value() : 0.30;
    double h = m_beamHeightSpin ? m_beamHeightSpin->value() : 0.50;
    double tw = m_beamTwSpin ? m_beamTwSpin->value() : 0.01;
    double tf = m_beamTfSpin ? m_beamTfSpin->value() : 0.015;

    if (secData == 0) // Rectangulaire
    {
        return TSA::Model::Section::rectangular(w, h);
    }
    else if (secData == 1) // Circulaire
    {
        return TSA::Model::Section::circular(w);
    }
    else if (secData >= 100 && secData <= 400) // IPE
    {
        auto s = TSA::Model::Section::ipe(secData);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData >= 1600 && secData <= 2400) // HEA
    {
        auto s = TSA::Model::Section::hea(secData / 10);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData >= 1601 && secData <= 3001) // HEB
    {
        auto s = TSA::Model::Section::heb((secData - 1) / 10);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData >= 5100 && secData <= 5300) // UPN
    {
        auto s = TSA::Model::Section::upn(secData - 5000);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData == 6060 || secData == 6080) // Cornière
    {
        return TSA::Model::Section::angle(h, w, tw);
    }
    else if (secData == 7100) // Tube rectangulaire
    {
        return TSA::Model::Section::boxHollow(w, h, tw, tf);
    }
    else if (secData == 8114) // Tube circulaire
    {
        return TSA::Model::Section::pipe(w, tw);
    }
    return TSA::Model::Section::rectangular(w, h);
}

TSA::Model::Section PropertyPanel::getColumnSectionFromUi() const
{
    int secData = m_columnSectionTypeCombo ? m_columnSectionTypeCombo->currentData().toInt() : 0;
    double w = m_columnWidthSpin ? m_columnWidthSpin->value() : 0.30;
    double h = m_columnDepthSpin ? m_columnDepthSpin->value() : 0.30;
    double tw = m_columnTwSpin ? m_columnTwSpin->value() : 0.01;
    double tf = m_columnTfSpin ? m_columnTfSpin->value() : 0.015;

    if (secData == 0) // Rectangulaire
    {
        return TSA::Model::Section::rectangular(w, h);
    }
    else if (secData == 1) // Circulaire
    {
        return TSA::Model::Section::circular(w);
    }
    else if (secData >= 160 && secData <= 400) // IPE
    {
        auto s = TSA::Model::Section::ipe(secData);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData == 2000 || secData == 2400) // HEA
    {
        auto s = TSA::Model::Section::hea(secData / 10);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    else if (secData == 2001 || secData == 3001) // HEB
    {
        auto s = TSA::Model::Section::heb((secData - 1) / 10);
        s.width = w;
        s.height = h;
        s.tw = tw;
        s.tf = tf;
        return s;
    }
    return TSA::Model::Section::rectangular(w, h);
}

void PropertyPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(8);

    m_titleLabel = new QLabel(tr("PROPRIÉTÉS STRUCTURALES"), container);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 11pt; padding: 4px; background: rgba(0,0,0,0.1); border-radius: 3px;");
    containerLayout->addWidget(m_titleLabel);

    m_chkLiveSync = new QCheckBox(tr("Synchronisation en direct (temps réel)"), container);
    m_chkLiveSync->setChecked(true);
    m_chkLiveSync->setToolTip(tr("Si coché, toute modification de valeur est répercutée immédiatement sur la structure 3D.\nSinon, effectuez vos modifications puis cliquez sur 'Appliquer les modifications'."));
    m_chkLiveSync->setStyleSheet("QCheckBox { font-weight: bold; color: #1E70BF; padding: 4px 6px; background: rgba(30,112,191,0.08); border-radius: 4px; }");
    containerLayout->addWidget(m_chkLiveSync);

    connect(m_chkLiveSync, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            onWidgetChanged();
        }
    });

    m_emptyLabel = new QLabel(tr("Aucun élément sélectionné.\nCliquez sur un élément dans le Viewport 3D ou dans l'Arbre du Modèle."), container);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: gray; padding: 20px; font-style: italic;");
    containerLayout->addWidget(m_emptyLabel);

    // ==========================================
    // 1. PANNEAU NŒUD
    // ==========================================
    m_nodeGroup = new QGroupBox(tr("Propriétés du Nœud"), container);
    auto* nodeForm = new QFormLayout(m_nodeGroup);
    m_nodeNameEdit = new QLineEdit(m_nodeGroup);
    m_nodeIdLabel = new QLabel(m_nodeGroup);
    m_nodeLevelLabel = new QLabel(m_nodeGroup);

    m_nodeXSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeXSpin->setRange(-10000.0, 10000.0);
    m_nodeXSpin->setDecimals(3);
    m_nodeXSpin->setSuffix(" m");

    m_nodeYSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeYSpin->setRange(-10000.0, 10000.0);
    m_nodeYSpin->setDecimals(3);
    m_nodeYSpin->setSuffix(" m");

    m_nodeZSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeZSpin->setRange(-10000.0, 10000.0);
    m_nodeZSpin->setDecimals(3);
    m_nodeZSpin->setSuffix(" m");

    m_nodeSupportCombo = new QComboBox(m_nodeGroup);
    m_nodeSupportCombo->addItem(tr("Libre (Aucun appui)"), static_cast<int>(TSA::Model::SupportType::Free));
    m_nodeSupportCombo->addItem(tr("Encastrement (6 DDL bloqués)"), static_cast<int>(TSA::Model::SupportType::Fixed));
    m_nodeSupportCombo->addItem(tr("Articulation / Rotule 3D"), static_cast<int>(TSA::Model::SupportType::Pinned));
    m_nodeSupportCombo->addItem(tr("Appui Simple (Rouleau Tz)"), static_cast<int>(TSA::Model::SupportType::Roller));

    m_nodeColorBtn = new QPushButton(m_nodeGroup);
    m_nodeColor = "#FFD700";
    setupColorButton(m_nodeColorBtn, m_nodeColor);
    connect(m_nodeColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_nodeColor, m_nodeColorBtn, tr("Couleur du Nœud"));
    });

    nodeForm->addRow(tr("Nom / Repère :"), m_nodeNameEdit);
    nodeForm->addRow(tr("ID Interne :"), m_nodeIdLabel);
    nodeForm->addRow(tr("Niveau :"), m_nodeLevelLabel);
    nodeForm->addRow(tr("X (m) :"), m_nodeXSpin);
    nodeForm->addRow(tr("Y (m) :"), m_nodeYSpin);
    nodeForm->addRow(tr("Z (m) :"), m_nodeZSpin);
    nodeForm->addRow(tr("Liaison / Appui :"), m_nodeSupportCombo);
    nodeForm->addRow(tr("Couleur 3D :"), m_nodeColorBtn);

    auto* btnApplyNode = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_nodeGroup);
    btnApplyNode->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyNode, &QPushButton::clicked, this, &PropertyPanel::onApplyNode);
    nodeForm->addRow(btnApplyNode);

    connect(m_nodeNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_nodeSupportCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_nodeGroup);

    // ==========================================
    // 2. PANNEAU POUTRE / BARRE
    // ==========================================
    m_beamGroup = new QGroupBox(tr("Propriétés de la Barre"), container);
    auto* beamMainLayout = new QVBoxLayout(m_beamGroup);
    beamMainLayout->setContentsMargins(6, 8, 6, 8);
    beamMainLayout->setSpacing(8);

    // 2.1 Identification
    auto* beamIdentBox = new QGroupBox(tr("Identification"), m_beamGroup);
    auto* beamIdentForm = new QFormLayout(beamIdentBox);
    beamIdentForm->setContentsMargins(6, 6, 6, 6);
    beamIdentForm->setSpacing(4);

    m_beamNameEdit = new QLineEdit(beamIdentBox);
    m_beamIdLabel = new QLabel(beamIdentBox);
    m_beamStartNodeLabel = new QLabel(beamIdentBox);
    m_beamEndNodeLabel = new QLabel(beamIdentBox);
    m_beamLengthLabel = new QLabel(beamIdentBox);

    m_beamRoleCombo = new QComboBox(beamIdentBox);
    m_beamRoleCombo->addItem(tr("Barre"), static_cast<int>(TSA::Model::BarRole::Generic));
    m_beamRoleCombo->addItem(tr("Poutre"), static_cast<int>(TSA::Model::BarRole::Beam));
    m_beamRoleCombo->addItem(tr("Poteau"), static_cast<int>(TSA::Model::BarRole::Column));
    m_beamRoleCombo->addItem(tr("Diagonale / Contreventement"), static_cast<int>(TSA::Model::BarRole::Brace));
    m_beamRoleCombo->addItem(tr("Tirant"), static_cast<int>(TSA::Model::BarRole::Tie));
    m_beamRoleCombo->addItem(tr("Barre acier"), static_cast<int>(TSA::Model::BarRole::SteelMember));
    m_beamRoleCombo->addItem(tr("Treillis"), static_cast<int>(TSA::Model::BarRole::Truss));

    beamIdentForm->addRow(tr("Nom / Repère :"), m_beamNameEdit);
    beamIdentForm->addRow(tr("ID Interne :"), m_beamIdLabel);
    beamIdentForm->addRow(tr("Rôle / Type :"), m_beamRoleCombo);
    beamIdentForm->addRow(tr("Nœud Départ :"), m_beamStartNodeLabel);
    beamIdentForm->addRow(tr("Nœud Arrivée :"), m_beamEndNodeLabel);
    beamIdentForm->addRow(tr("Longueur :"), m_beamLengthLabel);
    beamMainLayout->addWidget(beamIdentBox);

    // 2.2 Éditeur Graphique de Section
    auto* beamSecBox = new QGroupBox(tr("Éditeur Graphique de Section"), m_beamGroup);
    beamSecBox->setStyleSheet("QGroupBox { font-weight: bold; }");
    auto* beamSecLayout = new QVBoxLayout(beamSecBox);
    beamSecLayout->setContentsMargins(6, 6, 6, 6);
    beamSecLayout->setSpacing(6);

    // Aperçu interactif 2D
    m_beamSectionPreview = new SectionPreviewWidget(beamSecBox);
    m_beamSectionPreview->setMinimumHeight(190);
    beamSecLayout->addWidget(m_beamSectionPreview);

    // Paramètres géométriques de la section
    auto* beamSecForm = new QFormLayout();
    beamSecForm->setContentsMargins(0, 4, 0, 4);
    beamSecForm->setSpacing(4);

    m_beamSectionTypeCombo = new QComboBox(beamSecBox);
    setupSectionTypeCombo(m_beamSectionTypeCombo);
    beamSecForm->addRow(tr("Profil / Forme :"), m_beamSectionTypeCombo);

    m_beamWidthLabel = new QLabel(tr("Largeur b :"), beamSecBox);
    m_beamWidthSpin = new QDoubleSpinBox(beamSecBox);
    m_beamWidthSpin->setRange(0.01, 10.0);
    m_beamWidthSpin->setSingleStep(0.02);
    m_beamWidthSpin->setDecimals(3);
    m_beamWidthSpin->setSuffix(" m");
    beamSecForm->addRow(m_beamWidthLabel, m_beamWidthSpin);

    m_beamHeightLabel = new QLabel(tr("Hauteur h :"), beamSecBox);
    m_beamHeightSpin = new QDoubleSpinBox(beamSecBox);
    m_beamHeightSpin->setRange(0.01, 10.0);
    m_beamHeightSpin->setSingleStep(0.02);
    m_beamHeightSpin->setDecimals(3);
    m_beamHeightSpin->setSuffix(" m");
    beamSecForm->addRow(m_beamHeightLabel, m_beamHeightSpin);

    m_beamTwLabel = new QLabel(tr("Ép. âme tw :"), beamSecBox);
    m_beamTwSpin = new QDoubleSpinBox(beamSecBox);
    m_beamTwSpin->setRange(0.001, 1.0);
    m_beamTwSpin->setSingleStep(0.002);
    m_beamTwSpin->setDecimals(3);
    m_beamTwSpin->setSuffix(" m");
    beamSecForm->addRow(m_beamTwLabel, m_beamTwSpin);

    m_beamTfLabel = new QLabel(tr("Ép. aile tf :"), beamSecBox);
    m_beamTfSpin = new QDoubleSpinBox(beamSecBox);
    m_beamTfSpin->setRange(0.001, 1.0);
    m_beamTfSpin->setSingleStep(0.002);
    m_beamTfSpin->setDecimals(3);
    m_beamTfSpin->setSuffix(" m");
    beamSecForm->addRow(m_beamTfLabel, m_beamTfSpin);

    beamSecLayout->addLayout(beamSecForm);

    // Caractéristiques mécaniques calculées
    auto* beamCalcFrame = new QFrame(beamSecBox);
    beamCalcFrame->setStyleSheet("QFrame { background: rgba(30, 45, 60, 0.05); border: 1px solid rgba(0,0,0,0.12); border-radius: 4px; padding: 4px; }");
    auto* beamCalcLayout = new QVBoxLayout(beamCalcFrame);
    beamCalcLayout->setContentsMargins(6, 4, 6, 4);
    beamCalcLayout->setSpacing(2);

    auto* calcHdr = new QLabel(tr("<b>Caractéristiques Mécaniques Calculées</b>"), beamCalcFrame);
    calcHdr->setStyleSheet("color: #1976D2; font-size: 8.5pt;");
    beamCalcLayout->addWidget(calcHdr);

    m_beamPropArea = new QLabel(beamCalcFrame);
    m_beamPropArea->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_beamPropIy = new QLabel(beamCalcFrame);
    m_beamPropIy->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_beamPropIz = new QLabel(beamCalcFrame);
    m_beamPropIz->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_beamPropIt = new QLabel(beamCalcFrame);
    m_beamPropIt->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_beamPropW = new QLabel(beamCalcFrame);
    m_beamPropW->setStyleSheet("font-size: 8.5pt; font-family: monospace;");

    beamCalcLayout->addWidget(m_beamPropArea);
    beamCalcLayout->addWidget(m_beamPropIy);
    beamCalcLayout->addWidget(m_beamPropIz);
    beamCalcLayout->addWidget(m_beamPropIt);
    beamCalcLayout->addWidget(m_beamPropW);

    beamSecLayout->addWidget(beamCalcFrame);
    beamMainLayout->addWidget(beamSecBox);

    // 2.3 Matériau & Orientation
    auto* beamMatBox = new QGroupBox(tr("Matériau & Orientation"), m_beamGroup);
    auto* beamMatForm = new QFormLayout(beamMatBox);
    beamMatForm->setContentsMargins(6, 6, 6, 6);
    beamMatForm->setSpacing(4);

    m_beamMaterialCombo = new QComboBox(beamMatBox);
    setupMaterialCombo(m_beamMaterialCombo);
    beamMatForm->addRow(tr("Matériau :"), m_beamMaterialCombo);

    m_beamRotationSpin = new QDoubleSpinBox(beamMatBox);
    m_beamRotationSpin->setRange(0.0, 360.0);
    m_beamRotationSpin->setSingleStep(15.0);
    m_beamRotationSpin->setSuffix(" °");
    beamMatForm->addRow(tr("Rotation γ :"), m_beamRotationSpin);
    beamMainLayout->addWidget(beamMatBox);

    // 2.4 Excentrement & Positionnement
    auto* beamEccBox = new QGroupBox(tr("Excentrement & Alignement"), m_beamGroup);
    auto* beamEccForm = new QFormLayout(beamEccBox);
    beamEccForm->setContentsMargins(6, 6, 6, 6);
    beamEccForm->setSpacing(4);

    m_beamEccentricityCombo = new QComboBox(beamEccBox);
    m_beamEccentricityCombo->addItem(tr("Axe neutre (Axe de barre)"), static_cast<int>(TSA::Model::BarEccentricity::None));
    m_beamEccentricityCombo->addItem(tr("Fibre supérieure (Top)"), static_cast<int>(TSA::Model::BarEccentricity::TopFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre inférieure (Bottom)"), static_cast<int>(TSA::Model::BarEccentricity::BottomFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre gauche (Left)"), static_cast<int>(TSA::Model::BarEccentricity::LeftFlange));
    m_beamEccentricityCombo->addItem(tr("Fibre droite (Right)"), static_cast<int>(TSA::Model::BarEccentricity::RightFlange));
    beamEccForm->addRow(tr("Fibre d'alignement :"), m_beamEccentricityCombo);

    m_beamEySpin = new QDoubleSpinBox(beamEccBox);
    m_beamEySpin->setRange(-2.0, 2.0);
    m_beamEySpin->setSingleStep(0.01);
    m_beamEySpin->setDecimals(3);
    m_beamEySpin->setSuffix(" m");
    beamEccForm->addRow(tr("Décalage ey :"), m_beamEySpin);

    m_beamEzSpin = new QDoubleSpinBox(beamEccBox);
    m_beamEzSpin->setRange(-2.0, 2.0);
    m_beamEzSpin->setSingleStep(0.01);
    m_beamEzSpin->setDecimals(3);
    m_beamEzSpin->setSuffix(" m");
    beamEccForm->addRow(tr("Décalage ez :"), m_beamEzSpin);
    beamMainLayout->addWidget(beamEccBox);

    // 2.5 Relâchements aux extrémités (End Releases)
    auto* beamRelBox = new QGroupBox(tr("Relâchements aux Extrémités (Articulations)"), m_beamGroup);
    auto* beamRelLayout = new QVBoxLayout(beamRelBox);
    beamRelLayout->setContentsMargins(6, 6, 6, 6);
    beamRelLayout->setSpacing(4);

    auto* relGrid = new QGridLayout();
    relGrid->addWidget(new QLabel(tr("<b>DDL</b>")), 0, 0);
    relGrid->addWidget(new QLabel(tr("<b>Nœud Départ</b>")), 0, 1);
    relGrid->addWidget(new QLabel(tr("<b>Nœud Fin</b>")), 0, 2);

    m_beamStartUx = new QCheckBox(tr("Ux (Fx)"));
    m_beamEndUx = new QCheckBox(tr("Ux (Fx)"));
    relGrid->addWidget(new QLabel(tr("T axial :")), 1, 0);
    relGrid->addWidget(m_beamStartUx, 1, 1);
    relGrid->addWidget(m_beamEndUx, 1, 2);

    m_beamStartUy = new QCheckBox(tr("Uy (Fy)"));
    m_beamEndUy = new QCheckBox(tr("Uy (Fy)"));
    relGrid->addWidget(new QLabel(tr("V cisail. y :")), 2, 0);
    relGrid->addWidget(m_beamStartUy, 2, 1);
    relGrid->addWidget(m_beamEndUy, 2, 2);

    m_beamStartUz = new QCheckBox(tr("Uz (Fz)"));
    m_beamEndUz = new QCheckBox(tr("Uz (Fz)"));
    relGrid->addWidget(new QLabel(tr("V cisail. z :")), 3, 0);
    relGrid->addWidget(m_beamStartUz, 3, 1);
    relGrid->addWidget(m_beamEndUz, 3, 2);

    m_beamStartRx = new QCheckBox(tr("Rx (Mx)"));
    m_beamEndRx = new QCheckBox(tr("Rx (Mx)"));
    relGrid->addWidget(new QLabel(tr("M torsion :")), 4, 0);
    relGrid->addWidget(m_beamStartRx, 4, 1);
    relGrid->addWidget(m_beamEndRx, 4, 2);

    m_beamStartRy = new QCheckBox(tr("Ry (My)"));
    m_beamEndRy = new QCheckBox(tr("Ry (My)"));
    relGrid->addWidget(new QLabel(tr("M flex. y :")), 5, 0);
    relGrid->addWidget(m_beamStartRy, 5, 1);
    relGrid->addWidget(m_beamEndRy, 5, 2);

    m_beamStartRz = new QCheckBox(tr("Rz (Mz)"));
    m_beamEndRz = new QCheckBox(tr("Rz (Mz)"));
    relGrid->addWidget(new QLabel(tr("M flex. z :")), 6, 0);
    relGrid->addWidget(m_beamStartRz, 6, 1);
    relGrid->addWidget(m_beamEndRz, 6, 2);

    beamRelLayout->addLayout(relGrid);

    auto* relBtnLayout = new QHBoxLayout();
    auto* btnFixedFixed = new QPushButton(tr("Encastré"), beamRelBox);
    btnFixedFixed->setToolTip(tr("Bloquer toutes les rotations et translations aux deux extrémités"));
    auto* btnPinnedPinned = new QPushButton(tr("Articulé"), beamRelBox);
    btnPinnedPinned->setToolTip(tr("Relâcher les rotations de flexion (Ry, Rz) aux deux extrémités"));
    relBtnLayout->addWidget(btnFixedFixed);
    relBtnLayout->addWidget(btnPinnedPinned);
    beamRelLayout->addLayout(relBtnLayout);

    connect(btnFixedFixed, &QPushButton::clicked, this, [this]() {
        m_beamStartUx->setChecked(false); m_beamStartUy->setChecked(false); m_beamStartUz->setChecked(false);
        m_beamStartRx->setChecked(false); m_beamStartRy->setChecked(false); m_beamStartRz->setChecked(false);
        m_beamEndUx->setChecked(false); m_beamEndUy->setChecked(false); m_beamEndUz->setChecked(false);
        m_beamEndRx->setChecked(false); m_beamEndRy->setChecked(false); m_beamEndRz->setChecked(false);
        onWidgetChanged();
    });
    connect(btnPinnedPinned, &QPushButton::clicked, this, [this]() {
        m_beamStartUx->setChecked(false); m_beamStartUy->setChecked(false); m_beamStartUz->setChecked(false);
        m_beamStartRx->setChecked(false); m_beamStartRy->setChecked(true);  m_beamStartRz->setChecked(true);
        m_beamEndUx->setChecked(false); m_beamEndUy->setChecked(false); m_beamEndUz->setChecked(false);
        m_beamEndRx->setChecked(false); m_beamEndRy->setChecked(true);  m_beamEndRz->setChecked(true);
        onWidgetChanged();
    });

    beamMainLayout->addWidget(beamRelBox);

    // 2.6 Couleur & Bouton Appliquer
    auto* beamColorForm = new QFormLayout();
    m_beamColorBtn = new QPushButton(m_beamGroup);
    m_beamColor = "#4682B4";
    setupColorButton(m_beamColorBtn, m_beamColor);
    connect(m_beamColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_beamColor, m_beamColorBtn, tr("Couleur de la Barre"));
    });
    beamColorForm->addRow(tr("Couleur 3D :"), m_beamColorBtn);

    auto* btnApplyBeam = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_beamGroup);
    btnApplyBeam->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyBeam, &QPushButton::clicked, this, &PropertyPanel::onApplyBeam);
    beamColorForm->addRow(btnApplyBeam);

    beamMainLayout->addLayout(beamColorForm);

    // Signaux Poutre
    connect(m_beamNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_beamRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

    connect(m_beamSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_isLoading) return;
        int secData = m_beamSectionTypeCombo->currentData().toInt();
        updateBeamSectionVisibility(secData);
        if (secData >= 100 && secData <= 400) {
            auto s = TSA::Model::Section::ipe(secData);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
            m_beamTfSpin->setValue(s.tf);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 1600 && secData <= 2400) {
            auto s = TSA::Model::Section::hea(secData / 10);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
            m_beamTfSpin->setValue(s.tf);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 1601 && secData <= 3001) {
            auto s = TSA::Model::Section::heb((secData - 1) / 10);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
            m_beamTfSpin->setValue(s.tf);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData >= 5100 && secData <= 5300) {
            auto s = TSA::Model::Section::upn(secData - 5000);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
            m_beamTfSpin->setValue(s.tf);
            int stIdx = m_beamMaterialCombo->findData(4);
            if (stIdx >= 0) m_beamMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 6060) {
            auto s = TSA::Model::Section::angle(0.060, 0.060, 0.006);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
        } else if (secData == 6080) {
            auto s = TSA::Model::Section::angle(0.080, 0.080, 0.008);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
        } else if (secData == 7100) {
            auto s = TSA::Model::Section::boxHollow(0.100, 0.100, 0.005);
            m_beamWidthSpin->setValue(s.width);
            m_beamHeightSpin->setValue(s.height);
            m_beamTwSpin->setValue(s.tw);
            m_beamTfSpin->setValue(s.tf);
        } else if (secData == 8114) {
            auto s = TSA::Model::Section::pipe(0.114, 0.005);
            m_beamWidthSpin->setValue(s.width);
            m_beamTwSpin->setValue(s.tw);
        }
        auto sec = getBeamSectionFromUi();
        updateBeamCalculatedProperties(sec);
        onWidgetChanged();
    });

    auto onBeamDimChanged = [this]() {
        if (m_isLoading) return;
        auto sec = getBeamSectionFromUi();
        updateBeamCalculatedProperties(sec);
        onWidgetChanged();
    };
    connect(m_beamWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamDimChanged);
    connect(m_beamHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamDimChanged);
    connect(m_beamTwSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamDimChanged);
    connect(m_beamTfSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamDimChanged);
    connect(m_beamMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

    connect(m_beamRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isLoading) return;
        if (m_beamSectionPreview) m_beamSectionPreview->setRotation(val);
        onWidgetChanged();
    });

    auto onBeamEccChanged = [this]() {
        if (m_isLoading) return;
        if (m_beamSectionPreview) {
            m_beamSectionPreview->setEccentricity(
                static_cast<TSA::Model::BarEccentricity>(m_beamEccentricityCombo->currentData().toInt()),
                m_beamEySpin->value(),
                m_beamEzSpin->value()
            );
        }
        onWidgetChanged();
    };
    connect(m_beamEccentricityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onBeamEccChanged);
    connect(m_beamEySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamEccChanged);
    connect(m_beamEzSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onBeamEccChanged);

    auto onReleaseToggle = [this](bool) {
        if (m_isLoading) return;
        onWidgetChanged();
    };
    connect(m_beamStartUx, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamStartUy, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamStartUz, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamStartRx, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamStartRy, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamStartRz, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndUx, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndUy, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndUz, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndRx, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndRy, &QCheckBox::toggled, this, onReleaseToggle);
    connect(m_beamEndRz, &QCheckBox::toggled, this, onReleaseToggle);

    containerLayout->addWidget(m_beamGroup);

    // ==========================================
    // 3. PANNEAU POTEAU
    // ==========================================
    m_columnGroup = new QGroupBox(tr("Propriétés du Poteau"), container);
    auto* colMainLayout = new QVBoxLayout(m_columnGroup);
    colMainLayout->setContentsMargins(6, 8, 6, 8);
    colMainLayout->setSpacing(8);

    // 3.1 Identification
    auto* colIdentBox = new QGroupBox(tr("Identification"), m_columnGroup);
    auto* colIdentForm = new QFormLayout(colIdentBox);
    colIdentForm->setContentsMargins(6, 6, 6, 6);
    colIdentForm->setSpacing(4);

    m_columnNameEdit = new QLineEdit(colIdentBox);
    m_columnIdLabel = new QLabel(colIdentBox);
    m_columnStartNodeLabel = new QLabel(colIdentBox);
    m_columnEndNodeLabel = new QLabel(colIdentBox);
    m_columnHeightLabel = new QLabel(colIdentBox);

    colIdentForm->addRow(tr("Nom / Repère :"), m_columnNameEdit);
    colIdentForm->addRow(tr("ID Interne :"), m_columnIdLabel);
    colIdentForm->addRow(tr("Nœud Base :"), m_columnStartNodeLabel);
    colIdentForm->addRow(tr("Nœud Sommet :"), m_columnEndNodeLabel);
    colIdentForm->addRow(tr("Hauteur :"), m_columnHeightLabel);
    colMainLayout->addWidget(colIdentBox);

    // 3.2 Éditeur Graphique de Section
    auto* colSecBox = new QGroupBox(tr("Éditeur Graphique de Section"), m_columnGroup);
    colSecBox->setStyleSheet("QGroupBox { font-weight: bold; }");
    auto* colSecLayout = new QVBoxLayout(colSecBox);
    colSecLayout->setContentsMargins(6, 6, 6, 6);
    colSecLayout->setSpacing(6);

    m_columnSectionPreview = new SectionPreviewWidget(colSecBox);
    m_columnSectionPreview->setMinimumHeight(190);
    colSecLayout->addWidget(m_columnSectionPreview);

    auto* colSecForm = new QFormLayout();
    colSecForm->setContentsMargins(0, 4, 0, 4);
    colSecForm->setSpacing(4);

    m_columnSectionTypeCombo = new QComboBox(colSecBox);
    setupSectionTypeCombo(m_columnSectionTypeCombo);
    colSecForm->addRow(tr("Profil / Forme :"), m_columnSectionTypeCombo);

    m_columnWidthLabel = new QLabel(tr("Largeur b :"), colSecBox);
    m_columnWidthSpin = new QDoubleSpinBox(colSecBox);
    m_columnWidthSpin->setRange(0.01, 10.0);
    m_columnWidthSpin->setSingleStep(0.02);
    m_columnWidthSpin->setDecimals(3);
    m_columnWidthSpin->setSuffix(" m");
    colSecForm->addRow(m_columnWidthLabel, m_columnWidthSpin);

    m_columnDepthLabel = new QLabel(tr("Profondeur h :"), colSecBox);
    m_columnDepthSpin = new QDoubleSpinBox(colSecBox);
    m_columnDepthSpin->setRange(0.01, 10.0);
    m_columnDepthSpin->setSingleStep(0.02);
    m_columnDepthSpin->setDecimals(3);
    m_columnDepthSpin->setSuffix(" m");
    colSecForm->addRow(m_columnDepthLabel, m_columnDepthSpin);

    m_columnTwLabel = new QLabel(tr("Ép. âme tw :"), colSecBox);
    m_columnTwSpin = new QDoubleSpinBox(colSecBox);
    m_columnTwSpin->setRange(0.001, 1.0);
    m_columnTwSpin->setSingleStep(0.002);
    m_columnTwSpin->setDecimals(3);
    m_columnTwSpin->setSuffix(" m");
    colSecForm->addRow(m_columnTwLabel, m_columnTwSpin);

    m_columnTfLabel = new QLabel(tr("Ép. aile tf :"), colSecBox);
    m_columnTfSpin = new QDoubleSpinBox(colSecBox);
    m_columnTfSpin->setRange(0.001, 1.0);
    m_columnTfSpin->setSingleStep(0.002);
    m_columnTfSpin->setDecimals(3);
    m_columnTfSpin->setSuffix(" m");
    colSecForm->addRow(m_columnTfLabel, m_columnTfSpin);

    colSecLayout->addLayout(colSecForm);

    // Caractéristiques calculées
    auto* colCalcFrame = new QFrame(colSecBox);
    colCalcFrame->setStyleSheet("QFrame { background: rgba(30, 45, 60, 0.05); border: 1px solid rgba(0,0,0,0.12); border-radius: 4px; padding: 4px; }");
    auto* colCalcLayout = new QVBoxLayout(colCalcFrame);
    colCalcLayout->setContentsMargins(6, 4, 6, 4);
    colCalcLayout->setSpacing(2);

    auto* colCalcHdr = new QLabel(tr("<b>Caractéristiques Mécaniques Calculées</b>"), colCalcFrame);
    colCalcHdr->setStyleSheet("color: #1976D2; font-size: 8.5pt;");
    colCalcLayout->addWidget(colCalcHdr);

    m_columnPropArea = new QLabel(colCalcFrame);
    m_columnPropArea->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_columnPropIy = new QLabel(colCalcFrame);
    m_columnPropIy->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_columnPropIz = new QLabel(colCalcFrame);
    m_columnPropIz->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_columnPropIt = new QLabel(colCalcFrame);
    m_columnPropIt->setStyleSheet("font-size: 8.5pt; font-family: monospace;");
    m_columnPropW = new QLabel(colCalcFrame);
    m_columnPropW->setStyleSheet("font-size: 8.5pt; font-family: monospace;");

    colCalcLayout->addWidget(m_columnPropArea);
    colCalcLayout->addWidget(m_columnPropIy);
    colCalcLayout->addWidget(m_columnPropIz);
    colCalcLayout->addWidget(m_columnPropIt);
    colCalcLayout->addWidget(m_columnPropW);

    colSecLayout->addWidget(colCalcFrame);
    colMainLayout->addWidget(colSecBox);

    // 3.3 Matériau & Orientation
    auto* colMatBox = new QGroupBox(tr("Matériau & Orientation"), m_columnGroup);
    auto* colMatForm = new QFormLayout(colMatBox);
    colMatForm->setContentsMargins(6, 6, 6, 6);
    colMatForm->setSpacing(4);

    m_columnMaterialCombo = new QComboBox(colMatBox);
    setupMaterialCombo(m_columnMaterialCombo);
    colMatForm->addRow(tr("Matériau :"), m_columnMaterialCombo);

    m_columnRotationSpin = new QDoubleSpinBox(colMatBox);
    m_columnRotationSpin->setRange(0.0, 360.0);
    m_columnRotationSpin->setSingleStep(15.0);
    m_columnRotationSpin->setSuffix(" °");
    colMatForm->addRow(tr("Rotation β :"), m_columnRotationSpin);
    colMainLayout->addWidget(colMatBox);

    // 3.4 Couleur & Bouton Appliquer
    auto* colColorForm = new QFormLayout();
    m_columnColorBtn = new QPushButton(m_columnGroup);
    m_columnColor = "#6A5ACD";
    setupColorButton(m_columnColorBtn, m_columnColor);
    connect(m_columnColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_columnColor, m_columnColorBtn, tr("Couleur du Poteau"));
    });
    colColorForm->addRow(tr("Couleur 3D :"), m_columnColorBtn);

    auto* btnApplyCol = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_columnGroup);
    btnApplyCol->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyCol, &QPushButton::clicked, this, &PropertyPanel::onApplyColumn);
    colColorForm->addRow(btnApplyCol);

    colMainLayout->addLayout(colColorForm);

    // Signaux Poteau
    connect(m_columnNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);

    connect(m_columnSectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_isLoading) return;
        int secData = m_columnSectionTypeCombo->currentData().toInt();
        updateColumnSectionVisibility(secData);
        if (secData >= 160 && secData <= 400) {
            auto s = TSA::Model::Section::ipe(secData);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            m_columnTwSpin->setValue(s.tw);
            m_columnTfSpin->setValue(s.tf);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 2000 || secData == 2400) {
            auto s = TSA::Model::Section::hea(secData / 10);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            m_columnTwSpin->setValue(s.tw);
            m_columnTfSpin->setValue(s.tf);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        } else if (secData == 2001 || secData == 3001) {
            auto s = TSA::Model::Section::heb((secData - 1) / 10);
            m_columnWidthSpin->setValue(s.width);
            m_columnDepthSpin->setValue(s.height);
            m_columnTwSpin->setValue(s.tw);
            m_columnTfSpin->setValue(s.tf);
            int stIdx = m_columnMaterialCombo->findData(4);
            if (stIdx >= 0) m_columnMaterialCombo->setCurrentIndex(stIdx);
        }
        auto sec = getColumnSectionFromUi();
        updateColumnCalculatedProperties(sec);
        onWidgetChanged();
    });

    auto onColDimChanged = [this]() {
        if (m_isLoading) return;
        auto sec = getColumnSectionFromUi();
        updateColumnCalculatedProperties(sec);
        onWidgetChanged();
    };
    connect(m_columnWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onColDimChanged);
    connect(m_columnDepthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onColDimChanged);
    connect(m_columnTwSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onColDimChanged);
    connect(m_columnTfSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onColDimChanged);
    connect(m_columnMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

    connect(m_columnRotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (m_isLoading) return;
        if (m_columnSectionPreview) m_columnSectionPreview->setRotation(val);
        onWidgetChanged();
    });

    containerLayout->addWidget(m_columnGroup);

    // ==========================================
    // 4. PANNEAU DALLE
    // ==========================================
    m_slabGroup = new QGroupBox(tr("Propriétés de la Dalle"), container);
    auto* slabForm = new QFormLayout(m_slabGroup);
    m_slabNameEdit = new QLineEdit(m_slabGroup);
    m_slabIdLabel = new QLabel(m_slabGroup);
    m_slabNodesLabel = new QLabel(m_slabGroup);
    m_slabAreaLabel = new QLabel(m_slabGroup);

    m_slabThicknessSpin = new QDoubleSpinBox(m_slabGroup);
    m_slabThicknessSpin->setRange(0.05, 2.0);
    m_slabThicknessSpin->setSingleStep(0.02);
    m_slabThicknessSpin->setSuffix(" m");

    m_slabMaterialCombo = new QComboBox(m_slabGroup);
    setupMaterialCombo(m_slabMaterialCombo);

    auto* typeLayout = new QVBoxLayout();
    m_slabRadioTwoWay = new QRadioButton(tr("Portance Bidirectionnelle (Two-way)"), m_slabGroup);
    m_slabRadioOneWay = new QRadioButton(tr("Portance Unidirectionnelle (One-way)"), m_slabGroup);
    m_slabRadioFlat   = new QRadioButton(tr("Plancher-Dalle (Flat slab)"), m_slabGroup);
    m_slabRadioTwoWay->setChecked(true);
    typeLayout->addWidget(m_slabRadioTwoWay);
    typeLayout->addWidget(m_slabRadioOneWay);
    typeLayout->addWidget(m_slabRadioFlat);

    m_slabColorBtn = new QPushButton(m_slabGroup);
    m_slabColor = "#B0C4DE";
    setupColorButton(m_slabColorBtn, m_slabColor);
    connect(m_slabColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_slabColor, m_slabColorBtn, tr("Couleur de la Dalle"));
    });

    slabForm->addRow(tr("Nom / Repère :"), m_slabNameEdit);
    slabForm->addRow(tr("ID Interne :"), m_slabIdLabel);
    slabForm->addRow(tr("Nœuds Contour :"), m_slabNodesLabel);
    slabForm->addRow(tr("Surface 3D :"), m_slabAreaLabel);
    slabForm->addRow(tr("Épaisseur e :"), m_slabThicknessSpin);
    slabForm->addRow(tr("Matériau :"), m_slabMaterialCombo);
    slabForm->addRow(tr("Typologie :"), typeLayout);
    slabForm->addRow(tr("Couleur 3D :"), m_slabColorBtn);

    auto* btnApplySlab = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_slabGroup);
    btnApplySlab->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplySlab, &QPushButton::clicked, this, &PropertyPanel::onApplySlab);
    slabForm->addRow(btnApplySlab);

    connect(m_slabNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabThicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_slabMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioOneWay, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioTwoWay, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);
    connect(m_slabRadioFlat, &QRadioButton::toggled, this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_slabGroup);

    // ==========================================
    // 5. PANNEAU VOILE (WALL)
    // ==========================================
    m_wallGroup = new QGroupBox(tr("Propriétés du Voile"), container);
    auto* wallForm = new QFormLayout(m_wallGroup);
    m_wallNameEdit = new QLineEdit(m_wallGroup);
    m_wallIdLabel = new QLabel(m_wallGroup);
    m_wallStartNodeLabel = new QLabel(m_wallGroup);
    m_wallEndNodeLabel = new QLabel(m_wallGroup);
    m_wallLengthLabel = new QLabel(m_wallGroup);

    m_wallHeightSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallHeightSpin->setRange(0.5, 100.0);
    m_wallHeightSpin->setSingleStep(0.5);
    m_wallHeightSpin->setSuffix(" m");

    m_wallThicknessSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallThicknessSpin->setRange(0.05, 2.0);
    m_wallThicknessSpin->setSingleStep(0.05);
    m_wallThicknessSpin->setSuffix(" m");

    m_wallMaterialCombo = new QComboBox(m_wallGroup);
    setupMaterialCombo(m_wallMaterialCombo);

    m_wallOffsetSpin = new QDoubleSpinBox(m_wallGroup);
    m_wallOffsetSpin->setRange(-10.0, 10.0);
    m_wallOffsetSpin->setSingleStep(0.05);
    m_wallOffsetSpin->setSuffix(" m");

    m_wallColorBtn = new QPushButton(m_wallGroup);
    m_wallColor = "#808080";
    setupColorButton(m_wallColorBtn, m_wallColor);
    connect(m_wallColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_wallColor, m_wallColorBtn, tr("Couleur du Voile"));
    });

    wallForm->addRow(tr("Nom / Repère :"), m_wallNameEdit);
    wallForm->addRow(tr("ID Interne :"), m_wallIdLabel);
    wallForm->addRow(tr("Nœud 1 :"), m_wallStartNodeLabel);
    wallForm->addRow(tr("Nœud 2 :"), m_wallEndNodeLabel);
    wallForm->addRow(tr("Longueur :"), m_wallLengthLabel);
    wallForm->addRow(tr("Hauteur H :"), m_wallHeightSpin);
    wallForm->addRow(tr("Épaisseur e :"), m_wallThicknessSpin);
    wallForm->addRow(tr("Matériau :"), m_wallMaterialCombo);
    wallForm->addRow(tr("Décalage :"), m_wallOffsetSpin);
    wallForm->addRow(tr("Couleur 3D :"), m_wallColorBtn);

    auto* btnApplyWall = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_wallGroup);
    btnApplyWall->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyWall, &QPushButton::clicked, this, &PropertyPanel::onApplyWall);
    wallForm->addRow(btnApplyWall);

    connect(m_wallNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_wallHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallThicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_wallOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_wallGroup);

    // ==========================================
    // 6. PANNEAU FONDATION
    // ==========================================
    m_foundationGroup = new QGroupBox(tr("Propriétés de la Fondation"), container);
    auto* fForm = new QFormLayout(m_foundationGroup);
    m_foundationNameEdit = new QLineEdit(m_foundationGroup);
    m_foundationIdLabel = new QLabel(m_foundationGroup);
    m_foundationNodeLabel = new QLabel(m_foundationGroup);

    m_foundationTypeCombo = new QComboBox(m_foundationGroup);
    m_foundationTypeCombo->addItem(tr("Semelle Isolée (Poteau)"), static_cast<int>(TSA::Model::FoundationType::IsolatedFooting));
    m_foundationTypeCombo->addItem(tr("Semelle Filante (Voile)"), static_cast<int>(TSA::Model::FoundationType::StripFooting));
    m_foundationTypeCombo->addItem(tr("Radier Général"), static_cast<int>(TSA::Model::FoundationType::Raft));
    m_foundationTypeCombo->addItem(tr("Pieu"), static_cast<int>(TSA::Model::FoundationType::Pile));

    m_foundationWidthASpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationWidthASpin->setRange(0.2, 50.0);
    m_foundationWidthASpin->setSingleStep(0.1);
    m_foundationWidthASpin->setSuffix(" m");

    m_foundationLengthBSpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationLengthBSpin->setRange(0.2, 50.0);
    m_foundationLengthBSpin->setSingleStep(0.1);
    m_foundationLengthBSpin->setSuffix(" m");

    m_foundationHeightHSpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationHeightHSpin->setRange(0.1, 10.0);
    m_foundationHeightHSpin->setSingleStep(0.05);
    m_foundationHeightHSpin->setSuffix(" m");

    m_foundationMaterialCombo = new QComboBox(m_foundationGroup);
    setupMaterialCombo(m_foundationMaterialCombo);

    m_foundationSoilCapacitySpin = new QDoubleSpinBox(m_foundationGroup);
    m_foundationSoilCapacitySpin->setRange(10.0, 5000.0);
    m_foundationSoilCapacitySpin->setSingleStep(50.0);
    m_foundationSoilCapacitySpin->setSuffix(" kPa");

    m_foundationColorBtn = new QPushButton(m_foundationGroup);
    m_foundationColor = "#B8860B";
    setupColorButton(m_foundationColorBtn, m_foundationColor);
    connect(m_foundationColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_foundationColor, m_foundationColorBtn, tr("Couleur de la Fondation"));
    });

    fForm->addRow(tr("Nom / Repère :"), m_foundationNameEdit);
    fForm->addRow(tr("ID Interne :"), m_foundationIdLabel);
    fForm->addRow(tr("Nœud Support :"), m_foundationNodeLabel);
    fForm->addRow(tr("Type Fondation :"), m_foundationTypeCombo);
    fForm->addRow(tr("Largeur A :"), m_foundationWidthASpin);
    fForm->addRow(tr("Longueur B :"), m_foundationLengthBSpin);
    fForm->addRow(tr("Hauteur H :"), m_foundationHeightHSpin);
    fForm->addRow(tr("Matériau :"), m_foundationMaterialCombo);
    fForm->addRow(tr("Capacité Sol :"), m_foundationSoilCapacitySpin);
    fForm->addRow(tr("Couleur 3D :"), m_foundationColorBtn);

    auto* btnApplyF = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_foundationGroup);
    btnApplyF->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyF, &QPushButton::clicked, this, &PropertyPanel::onApplyFoundation);
    fForm->addRow(btnApplyF);

    connect(m_foundationNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationWidthASpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationLengthBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationHeightHSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_foundationSoilCapacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_foundationGroup);

    // ==========================================
    // 7. PANNEAU TREILLIS / CONTREVENTEMENT
    // ==========================================
    m_trussGroup = new QGroupBox(tr("Propriétés de la Barre de Treillis"), container);
    auto* trForm = new QFormLayout(m_trussGroup);
    m_trussNameEdit = new QLineEdit(m_trussGroup);
    m_trussIdLabel = new QLabel(m_trussGroup);
    m_trussStartNodeLabel = new QLabel(m_trussGroup);
    m_trussEndNodeLabel = new QLabel(m_trussGroup);
    m_trussLengthLabel = new QLabel(m_trussGroup);

    m_trussRoleCombo = new QComboBox(m_trussGroup);
    m_trussRoleCombo->addItem(tr("Diagonale"), static_cast<int>(TSA::Model::TrussMemberRole::Diagonal));
    m_trussRoleCombo->addItem(tr("Membrure Supérieure"), static_cast<int>(TSA::Model::TrussMemberRole::TopChord));
    m_trussRoleCombo->addItem(tr("Membrure Inférieure"), static_cast<int>(TSA::Model::TrussMemberRole::BottomChord));
    m_trussRoleCombo->addItem(tr("Montant Vertical"), static_cast<int>(TSA::Model::TrussMemberRole::Vertical));
    m_trussRoleCombo->addItem(tr("Contreventement (Brace)"), static_cast<int>(TSA::Model::TrussMemberRole::Brace));

    m_trussDimensionSpin = new QDoubleSpinBox(m_trussGroup);
    m_trussDimensionSpin->setRange(0.02, 1.0);
    m_trussDimensionSpin->setSingleStep(0.01);
    m_trussDimensionSpin->setSuffix(" m");

    m_trussMaterialCombo = new QComboBox(m_trussGroup);
    setupMaterialCombo(m_trussMaterialCombo);

    m_trussColorBtn = new QPushButton(m_trussGroup);
    m_trussColor = "#DAA520";
    setupColorButton(m_trussColorBtn, m_trussColor);
    connect(m_trussColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_trussColor, m_trussColorBtn, tr("Couleur du Treillis"));
    });

    trForm->addRow(tr("Nom / Repère :"), m_trussNameEdit);
    trForm->addRow(tr("ID Interne :"), m_trussIdLabel);
    trForm->addRow(tr("Nœud 1 :"), m_trussStartNodeLabel);
    trForm->addRow(tr("Nœud 2 :"), m_trussEndNodeLabel);
    trForm->addRow(tr("Longueur :"), m_trussLengthLabel);
    trForm->addRow(tr("Rôle :"), m_trussRoleCombo);
    trForm->addRow(tr("Diamètre / Section :"), m_trussDimensionSpin);
    trForm->addRow(tr("Matériau :"), m_trussMaterialCombo);
    trForm->addRow(tr("Couleur 3D :"), m_trussColorBtn);

    auto* btnApplyTr = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_trussGroup);
    btnApplyTr->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyTr, &QPushButton::clicked, this, &PropertyPanel::onApplyTruss);
    trForm->addRow(btnApplyTr);

    connect(m_trussNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_trussRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_trussDimensionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_trussMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_trussGroup);

    // --- GROUPE CÂBLE & TENSION SYSTEM ---
    m_cableGroup = new QGroupBox(tr("Câble / Système de Tension"), container);
    auto* cabForm = new QFormLayout(m_cableGroup);

    m_cableNameEdit = new QLineEdit(m_cableGroup);
    m_cableIdLabel = new QLabel(m_cableGroup);
    m_cableStartNodeLabel = new QLabel(m_cableGroup);
    m_cableEndNodeLabel = new QLabel(m_cableGroup);
    m_cableLengthLabel = new QLabel(m_cableGroup);

    m_cableTypeCombo = new QComboBox(m_cableGroup);
    m_cableTypeCombo->addItem(tr("Générique"), static_cast<int>(TSA::Model::CableType::Generic));
    m_cableTypeCombo->addItem(tr("Toron (Strand)"), static_cast<int>(TSA::Model::CableType::Strand));
    m_cableTypeCombo->addItem(tr("Fil tréfilé (Wire)"), static_cast<int>(TSA::Model::CableType::Wire));
    m_cableTypeCombo->addItem(tr("Barre précontrainte"), static_cast<int>(TSA::Model::CableType::PrestressingBar));
    m_cableTypeCombo->addItem(tr("Hauban (Stay Cable)"), static_cast<int>(TSA::Model::CableType::StayCable));
    m_cableTypeCombo->addItem(tr("Câble porteur suspendu"), static_cast<int>(TSA::Model::CableType::SuspensionCable));
    m_cableTypeCombo->addItem(tr("Suspente verticale"), static_cast<int>(TSA::Model::CableType::Hanger));
    m_cableTypeCombo->addItem(tr("Précontrainte extérieure"), static_cast<int>(TSA::Model::CableType::ExternalPrestressing));
    m_cableTypeCombo->addItem(tr("Tirant d'ancrage"), static_cast<int>(TSA::Model::CableType::GroundAnchor));

    m_cableGeomModeCombo = new QComboBox(m_cableGroup);
    m_cableGeomModeCombo->addItem(tr("Droit (Straight chord)"), static_cast<int>(TSA::Model::CableGeometryMode::Straight));
    m_cableGeomModeCombo->addItem(tr("Parabolique (Tension & flèche)"), static_cast<int>(TSA::Model::CableGeometryMode::Parabolic));
    m_cableGeomModeCombo->addItem(tr("Caténaire (Catenary exact)"), static_cast<int>(TSA::Model::CableGeometryMode::Catenary));

    m_cableDiaSpin = new QDoubleSpinBox(m_cableGroup);
    m_cableDiaSpin->setRange(1.0, 500.0);
    m_cableDiaSpin->setSingleStep(1.0);
    m_cableDiaSpin->setSuffix(" mm");

    m_cableAreaSpin = new QDoubleSpinBox(m_cableGroup);
    m_cableAreaSpin->setRange(1.0, 100000.0);
    m_cableAreaSpin->setSingleStep(10.0);
    m_cableAreaSpin->setSuffix(" mm²");

    m_cableModulusSpin = new QDoubleSpinBox(m_cableGroup);
    m_cableModulusSpin->setRange(50.0, 300.0);
    m_cableModulusSpin->setSingleStep(5.0);
    m_cableModulusSpin->setSuffix(" GPa");

    m_cableInitialTensionSpin = new QDoubleSpinBox(m_cableGroup);
    m_cableInitialTensionSpin->setRange(0.0, 100000.0);
    m_cableInitialTensionSpin->setSingleStep(10.0);
    m_cableInitialTensionSpin->setSuffix(" kN");

    m_cableSagSpin = new QDoubleSpinBox(m_cableGroup);
    m_cableSagSpin->setRange(0.0, 100.0);
    m_cableSagSpin->setSingleStep(0.1);
    m_cableSagSpin->setSuffix(" m");

    m_cableStartAnchorCombo = new QComboBox(m_cableGroup);
    m_cableStartAnchorCombo->addItem(tr("Encastré"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_cableStartAnchorCombo->addItem(tr("Articulé"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_cableStartAnchorCombo->addItem(tr("Tête de précontrainte"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_cableStartAnchorCombo->addItem(tr("Culot / Trompette de hauban"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));

    m_cableEndAnchorCombo = new QComboBox(m_cableGroup);
    m_cableEndAnchorCombo->addItem(tr("Encastré"), static_cast<int>(TSA::Model::AnchorType::Fixed));
    m_cableEndAnchorCombo->addItem(tr("Articulé"), static_cast<int>(TSA::Model::AnchorType::Pinned));
    m_cableEndAnchorCombo->addItem(tr("Tête de précontrainte"), static_cast<int>(TSA::Model::AnchorType::PrestressingAnchor));
    m_cableEndAnchorCombo->addItem(tr("Culot / Trompette de hauban"), static_cast<int>(TSA::Model::AnchorType::StructuralAnchor));

    m_cableTensionOnlyCheck = new QCheckBox(tr("Traction seule (Tension-only)"), m_cableGroup);
    m_cableTensionOnlyCheck->setChecked(true);

    m_cableErnstModulusLabel = new QLabel(m_cableGroup);

    m_cableColorBtn = new QPushButton(m_cableGroup);
    m_cableColor = "#3296DC";
    setupColorButton(m_cableColorBtn, m_cableColor);
    connect(m_cableColorBtn, &QPushButton::clicked, this, [this]() {
        pickColor(m_cableColor, m_cableColorBtn, tr("Couleur du Câble"));
    });

    cabForm->addRow(tr("Nom / Repère :"), m_cableNameEdit);
    cabForm->addRow(tr("ID Interne :"), m_cableIdLabel);
    cabForm->addRow(tr("Nœud Début :"), m_cableStartNodeLabel);
    cabForm->addRow(tr("Nœud Fin :"), m_cableEndNodeLabel);
    cabForm->addRow(tr("Longueur de corde :"), m_cableLengthLabel);
    cabForm->addRow(tr("Type de Câble :"), m_cableTypeCombo);
    cabForm->addRow(tr("Profil Géométrique :"), m_cableGeomModeCombo);
    cabForm->addRow(tr("Diamètre Nominal :"), m_cableDiaSpin);
    cabForm->addRow(tr("Section d'acier :"), m_cableAreaSpin);
    cabForm->addRow(tr("Module Élastique E :"), m_cableModulusSpin);
    cabForm->addRow(tr("Tension Initiale N0 :"), m_cableInitialTensionSpin);
    cabForm->addRow(tr("Flèche / Sag :"), m_cableSagSpin);
    cabForm->addRow(tr("Ancrage Début :"), m_cableStartAnchorCombo);
    cabForm->addRow(tr("Ancrage Fin :"), m_cableEndAnchorCombo);
    cabForm->addRow(tr("Comportement EF :"), m_cableTensionOnlyCheck);
    cabForm->addRow(tr("Module Ernst E_eq :"), m_cableErnstModulusLabel);
    cabForm->addRow(tr("Couleur 3D :"), m_cableColorBtn);

    auto* btnApplyCab = new QPushButton(QIcon(":/icons/apply.svg"), tr("Appliquer les modifications"), m_cableGroup);
    btnApplyCab->setStyleSheet("font-weight: bold; background: #007acc; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(btnApplyCab, &QPushButton::clicked, this, &PropertyPanel::onApplyCable);
    cabForm->addRow(btnApplyCab);

    connect(m_cableNameEdit, &QLineEdit::editingFinished, this, &PropertyPanel::onWidgetChanged);
    connect(m_cableTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableGeomModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableDiaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableAreaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableModulusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableInitialTensionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableSagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableStartAnchorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableEndAnchorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertyPanel::onWidgetChanged);
    connect(m_cableTensionOnlyCheck, &QCheckBox::toggled, this, &PropertyPanel::onWidgetChanged);

    containerLayout->addWidget(m_cableGroup);

    containerLayout->addStretch();
    scrollArea->setWidget(container);
    mainLayout->addWidget(scrollArea);
}

void PropertyPanel::hideAllGroups()
{
    m_nodeGroup->setVisible(false);
    m_beamGroup->setVisible(false);
    m_columnGroup->setVisible(false);
    m_slabGroup->setVisible(false);
    m_wallGroup->setVisible(false);
    m_foundationGroup->setVisible(false);
    m_trussGroup->setVisible(false);
    m_cableGroup->setVisible(false);
}

void PropertyPanel::clearProperties()
{
    m_currentType = CurrentType::None;
    m_currentLevelId.clear();
    m_currentNodeId = -1;
    m_currentBeamId = -1;
    m_currentColumnId = -1;
    m_currentSlabId = -1;
    m_currentWallId = -1;
    m_currentFoundationId = -1;
    m_currentTrussId = -1;
    m_currentCableId = -1;

    m_titleLabel->setText(tr("PROPRIÉTÉS STRUCTURALES"));
    m_emptyLabel->setVisible(true);
    hideAllGroups();
}

void PropertyPanel::showLevelProperties(const QString& levelId)
{
    LoadingGuard guard(m_isLoading);
    clearProperties();
    m_currentType = CurrentType::Level;
    m_currentLevelId = levelId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NIVEAU : %1").arg(levelId));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* node = m_model->getNode(nodeId);
    if (!node) return;

    clearProperties();
    m_currentType = CurrentType::Node;
    m_currentNodeId = nodeId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NŒUD"));

    m_nodeNameEdit->setText(QString::fromStdString(node->formattedName()));
    m_nodeIdLabel->setText(QString::number(node->id()));
    m_nodeLevelLabel->setText(node->levelId().empty() ? tr("Aucun") : QString::fromStdString(node->levelId()));
    m_nodeXSpin->setValue(node->x());
    m_nodeYSpin->setValue(node->y());
    m_nodeZSpin->setValue(node->z());

    int idx = m_nodeSupportCombo->findData(static_cast<int>(node->supportType()));
    if (idx >= 0) m_nodeSupportCombo->setCurrentIndex(idx);

    m_nodeColor = QString::fromStdString(node->color());
    setupColorButton(m_nodeColorBtn, m_nodeColor);

    m_nodeGroup->setVisible(true);
}

void PropertyPanel::showBeamProperties(int beamId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* beam = m_model->getBeam(beamId);
    if (!beam) return;

    clearProperties();
    m_currentType = CurrentType::Beam;
    m_currentBeamId = beamId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA BARRE / POUTRE"));

    m_beamNameEdit->setText(QString::fromStdString(beam->formattedName()));
    m_beamIdLabel->setText(QString::number(beam->id()));
    m_beamStartNodeLabel->setText(QString("N%1").arg(beam->startNodeId()));
    m_beamEndNodeLabel->setText(QString("N%1").arg(beam->endNodeId()));
    m_beamLengthLabel->setText(QString("%1 m").arg(beam->length(*m_model), 0, 'f', 2));

    int roleIdx = m_beamRoleCombo->findData(static_cast<int>(beam->role()));
    if (roleIdx >= 0) m_beamRoleCombo->setCurrentIndex(roleIdx);

    const auto& sec = beam->section();
    double wVal = (sec.shape == TSA::Model::SectionShape::Circular || sec.shape == TSA::Model::SectionShape::Pipe)
                  ? ((sec.diameter > 0.0) ? sec.diameter : sec.width)
                  : sec.width;
    double hVal = (sec.shape == TSA::Model::SectionShape::Circular || sec.shape == TSA::Model::SectionShape::Pipe)
                  ? ((sec.diameter > 0.0) ? sec.diameter : sec.height)
                  : sec.height;

    m_beamWidthSpin->setValue(wVal > 0.0 ? wVal : 0.30);
    m_beamHeightSpin->setValue(hVal > 0.0 ? hVal : 0.50);
    m_beamTwSpin->setValue(sec.tw > 0.0 ? sec.tw : 0.010);
    m_beamTfSpin->setValue(sec.tf > 0.0 ? sec.tf : 0.015);
    m_beamRotationSpin->setValue(beam->rotation());

    int eccIdx = m_beamEccentricityCombo->findData(static_cast<int>(beam->eccentricity()));
    if (eccIdx >= 0) m_beamEccentricityCombo->setCurrentIndex(eccIdx);

    // Relâchements aux extrémités
    const auto& sr = beam->startRelease();
    m_beamStartUx->setChecked(sr.fx);
    m_beamStartUy->setChecked(sr.fy);
    m_beamStartUz->setChecked(sr.fz);
    m_beamStartRx->setChecked(sr.mx);
    m_beamStartRy->setChecked(sr.my);
    m_beamStartRz->setChecked(sr.mz);

    const auto& er = beam->endRelease();
    m_beamEndUx->setChecked(er.fx);
    m_beamEndUy->setChecked(er.fy);
    m_beamEndUz->setChecked(er.fz);
    m_beamEndRx->setChecked(er.mx);
    m_beamEndRy->setChecked(er.my);
    m_beamEndRz->setChecked(er.mz);

    m_beamSectionTypeCombo->blockSignals(true);
    int activeSecData = 0;
    if (sec.shape == TSA::Model::SectionShape::Circular)
    {
        activeSecData = 1;
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(1));
    }
    else if (sec.shape == TSA::Model::SectionShape::Pipe)
    {
        activeSecData = 8114;
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(8114));
    }
    else if (sec.shape == TSA::Model::SectionShape::BoxHollow)
    {
        activeSecData = 7100;
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(7100));
    }
    else if (sec.shape == TSA::Model::SectionShape::Angle)
    {
        activeSecData = 6060;
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(6060));
    }
    else if (sec.shape == TSA::Model::SectionShape::IShape || sec.shape == TSA::Model::SectionShape::UPN)
    {
        int idx = m_beamSectionTypeCombo->findText(QString::fromStdString(sec.name));
        if (idx < 0)
        {
            int hMm = static_cast<int>(std::round(sec.height * 1000.0));
            idx = m_beamSectionTypeCombo->findData(hMm);
        }
        if (idx >= 0)
        {
            m_beamSectionTypeCombo->setCurrentIndex(idx);
            activeSecData = m_beamSectionTypeCombo->currentData().toInt();
        }
        else
        {
            m_beamSectionTypeCombo->setCurrentIndex(0);
            activeSecData = 0;
        }
    }
    else
    {
        m_beamSectionTypeCombo->setCurrentIndex(m_beamSectionTypeCombo->findData(0));
        activeSecData = 0;
    }
    m_beamSectionTypeCombo->blockSignals(false);
    updateBeamSectionVisibility(activeSecData);

    updateBeamCalculatedProperties(sec);
    if (m_beamSectionPreview)
    {
        m_beamSectionPreview->setSection(sec);
        m_beamSectionPreview->setRotation(beam->rotation());
        m_beamSectionPreview->setEccentricity(beam->eccentricity(), m_beamEySpin->value(), m_beamEzSpin->value());
    }

    int bMatIdx = m_beamMaterialCombo->findData(beam->material().id);
    if (bMatIdx < 0) bMatIdx = m_beamMaterialCombo->findText(QString::fromStdString(beam->material().name));
    if (bMatIdx >= 0) m_beamMaterialCombo->setCurrentIndex(bMatIdx);

    m_beamColor = QString::fromStdString(beam->color());
    setupColorButton(m_beamColorBtn, m_beamColor);

    m_beamGroup->setVisible(true);
}

void PropertyPanel::showColumnProperties(int columnId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* col = m_model->getColumn(columnId);
    if (!col) return;

    clearProperties();
    m_currentType = CurrentType::Column;
    m_currentColumnId = columnId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU POTEAU"));

    m_columnNameEdit->setText(QString::fromStdString(col->formattedName()));
    m_columnIdLabel->setText(QString::number(col->id()));
    m_columnStartNodeLabel->setText(QString("N%1").arg(col->startNodeId()));
    m_columnEndNodeLabel->setText(QString("N%1").arg(col->endNodeId()));
    m_columnHeightLabel->setText(QString("%1 m (%2)").arg(col->length(*m_model), 0, 'f', 2).arg(QString::fromStdString(col->direction(*m_model))));

    const auto& sec = col->section();
    double wVal = (sec.shape == TSA::Model::SectionShape::Circular)
                  ? ((sec.diameter > 0.0) ? sec.diameter : sec.width)
                  : sec.width;
    double dVal = (sec.shape == TSA::Model::SectionShape::Circular)
                  ? ((sec.diameter > 0.0) ? sec.diameter : sec.height)
                  : sec.height;

    m_columnWidthSpin->setValue(wVal > 0.0 ? wVal : 0.30);
    m_columnDepthSpin->setValue(dVal > 0.0 ? dVal : 0.30);
    m_columnTwSpin->setValue(sec.tw > 0.0 ? sec.tw : 0.010);
    m_columnTfSpin->setValue(sec.tf > 0.0 ? sec.tf : 0.015);
    m_columnRotationSpin->setValue(col->rotation());

    m_columnSectionTypeCombo->blockSignals(true);
    int activeSecData = 0;
    if (sec.shape == TSA::Model::SectionShape::Circular)
    {
        activeSecData = 1;
        m_columnSectionTypeCombo->setCurrentIndex(m_columnSectionTypeCombo->findData(1));
    }
    else if (sec.shape == TSA::Model::SectionShape::IShape)
    {
        int idx = m_columnSectionTypeCombo->findText(QString::fromStdString(sec.name));
        if (idx < 0)
        {
            int hMm = static_cast<int>(std::round(sec.height * 1000.0));
            idx = m_columnSectionTypeCombo->findData(hMm);
        }
        if (idx >= 0)
        {
            m_columnSectionTypeCombo->setCurrentIndex(idx);
            activeSecData = m_columnSectionTypeCombo->currentData().toInt();
        }
        else
        {
            m_columnSectionTypeCombo->setCurrentIndex(0);
            activeSecData = 0;
        }
    }
    else
    {
        m_columnSectionTypeCombo->setCurrentIndex(m_columnSectionTypeCombo->findData(0));
        activeSecData = 0;
    }
    m_columnSectionTypeCombo->blockSignals(false);
    updateColumnSectionVisibility(activeSecData);

    updateColumnCalculatedProperties(sec);
    if (m_columnSectionPreview)
    {
        m_columnSectionPreview->setSection(sec);
        m_columnSectionPreview->setRotation(col->rotation());
    }

    int cMatIdx = m_columnMaterialCombo->findData(col->material().id);
    if (cMatIdx < 0) cMatIdx = m_columnMaterialCombo->findText(QString::fromStdString(col->material().name));
    if (cMatIdx >= 0) m_columnMaterialCombo->setCurrentIndex(cMatIdx);

    m_columnColor = QString::fromStdString(col->color());
    setupColorButton(m_columnColorBtn, m_columnColor);

    m_columnGroup->setVisible(true);
}

void PropertyPanel::showSlabProperties(int slabId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* slab = m_model->getSlab(slabId);
    if (!slab) return;

    clearProperties();
    m_currentType = CurrentType::Slab;
    m_currentSlabId = slabId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA DALLE"));

    m_slabNameEdit->setText(QString::fromStdString(slab->formattedName()));
    m_slabIdLabel->setText(QString::number(slab->id()));

    QStringList nodeStrs;
    for (int nid : slab->nodeIds()) nodeStrs << QString("N%1").arg(nid);
    m_slabNodesLabel->setText(nodeStrs.join(" - "));
    m_slabAreaLabel->setText(QString("%1 m²").arg(slab->area(*m_model), 0, 'f', 2));
    m_slabThicknessSpin->setValue(slab->thickness());

    if (slab->slabType() == TSA::Model::SlabType::OneWay) m_slabRadioOneWay->setChecked(true);
    else if (slab->slabType() == TSA::Model::SlabType::FlatSlab) m_slabRadioFlat->setChecked(true);
    else m_slabRadioTwoWay->setChecked(true);

    int sMatIdx = m_slabMaterialCombo->findData(slab->material().id);
    if (sMatIdx < 0) sMatIdx = m_slabMaterialCombo->findText(QString::fromStdString(slab->material().name));
    if (sMatIdx >= 0) m_slabMaterialCombo->setCurrentIndex(sMatIdx);

    m_slabColor = QString::fromStdString(slab->color());
    setupColorButton(m_slabColorBtn, m_slabColor);

    m_slabGroup->setVisible(true);
}

void PropertyPanel::showWallProperties(int wallId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* wall = m_model->getWall(wallId);
    if (!wall) return;

    clearProperties();
    m_currentType = CurrentType::Wall;
    m_currentWallId = wallId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU VOILE"));

    m_wallNameEdit->setText(QString::fromStdString(wall->formattedName()));
    m_wallIdLabel->setText(QString::number(wall->id()));
    m_wallStartNodeLabel->setText(QString("N%1").arg(wall->startNodeId()));
    m_wallEndNodeLabel->setText(QString("N%1").arg(wall->endNodeId()));
    m_wallLengthLabel->setText(QString("%1 m (Aire = %2 m²)").arg(wall->length(*m_model), 0, 'f', 2).arg(wall->area(*m_model), 0, 'f', 2));
    m_wallHeightSpin->setValue(wall->height());
    m_wallThicknessSpin->setValue(wall->thickness());
    m_wallOffsetSpin->setValue(wall->offset());

    int wMatIdx = m_wallMaterialCombo->findData(wall->material().id);
    if (wMatIdx < 0) wMatIdx = m_wallMaterialCombo->findText(QString::fromStdString(wall->material().name));
    if (wMatIdx >= 0) m_wallMaterialCombo->setCurrentIndex(wMatIdx);

    m_wallColor = QString::fromStdString(wall->color());
    setupColorButton(m_wallColorBtn, m_wallColor);

    m_wallGroup->setVisible(true);
}

void PropertyPanel::showFoundationProperties(int foundationId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* f = m_model->getFoundation(foundationId);
    if (!f) return;

    clearProperties();
    m_currentType = CurrentType::Foundation;
    m_currentFoundationId = foundationId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA FONDATION"));

    m_foundationNameEdit->setText(QString::fromStdString(f->formattedName()));
    m_foundationIdLabel->setText(QString::number(f->id()));
    m_foundationNodeLabel->setText(QString("N%1").arg(f->nodeId()));

    int idx = m_foundationTypeCombo->findData(static_cast<int>(f->foundationType()));
    if (idx >= 0) m_foundationTypeCombo->setCurrentIndex(idx);

    m_foundationWidthASpin->setValue(f->widthA());
    m_foundationLengthBSpin->setValue(f->lengthB());
    m_foundationHeightHSpin->setValue(f->heightH());
    m_foundationSoilCapacitySpin->setValue(f->soilBearingCapacity());

    int fMatIdx = m_foundationMaterialCombo->findData(f->material().id);
    if (fMatIdx < 0) fMatIdx = m_foundationMaterialCombo->findText(QString::fromStdString(f->material().name));
    if (fMatIdx >= 0) m_foundationMaterialCombo->setCurrentIndex(fMatIdx);

    m_foundationColor = QString::fromStdString(f->color());
    setupColorButton(m_foundationColorBtn, m_foundationColor);

    m_foundationGroup->setVisible(true);
}

void PropertyPanel::showTrussMemberProperties(int memberId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* truss = m_model->getTrussMember(memberId);
    if (!truss) return;

    clearProperties();
    m_currentType = CurrentType::Truss;
    m_currentTrussId = memberId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU TREILLIS"));

    m_trussNameEdit->setText(QString::fromStdString(truss->formattedName()));
    m_trussIdLabel->setText(QString::number(truss->id()));
    m_trussStartNodeLabel->setText(QString("N%1").arg(truss->startNodeId()));
    m_trussEndNodeLabel->setText(QString("N%1").arg(truss->endNodeId()));
    m_trussLengthLabel->setText(QString("%1 m").arg(truss->length(*m_model), 0, 'f', 2));

    int idx = m_trussRoleCombo->findData(static_cast<int>(truss->role()));
    if (idx >= 0) m_trussRoleCombo->setCurrentIndex(idx);

    m_trussDimensionSpin->setValue(truss->section().width);

    int tMatIdx = m_trussMaterialCombo->findData(truss->material().id);
    if (tMatIdx < 0) tMatIdx = m_trussMaterialCombo->findText(QString::fromStdString(truss->material().name));
    if (tMatIdx >= 0) m_trussMaterialCombo->setCurrentIndex(tMatIdx);

    m_trussColor = QString::fromStdString(truss->color());
    setupColorButton(m_trussColorBtn, m_trussColor);

    m_trussGroup->setVisible(true);
}

void PropertyPanel::showCableProperties(int cableId)
{
    LoadingGuard guard(m_isLoading);
    if (!m_model) return;
    const auto* cable = m_model->getCable(cableId);
    if (!cable) return;

    clearProperties();
    m_currentType = CurrentType::Cable;
    m_currentCableId = cableId;
    m_emptyLabel->setVisible(false);
    m_titleLabel->setText(tr("PROPRIÉTÉS DU CÂBLE / SYSTÈME DE TENSION"));

    m_cableNameEdit->setText(QString::fromStdString(cable->formattedName()));
    m_cableIdLabel->setText(QString::number(cable->id()));
    m_cableStartNodeLabel->setText(QString("N%1").arg(cable->startNodeId()));
    m_cableEndNodeLabel->setText(QString("N%1").arg(cable->endNodeId()));

    double chordLen = cable->chordLength(*m_model);
    m_cableLengthLabel->setText(QString("%1 m").arg(chordLen, 0, 'f', 3));

    int typeIdx = m_cableTypeCombo->findData(static_cast<int>(cable->type()));
    if (typeIdx >= 0) m_cableTypeCombo->setCurrentIndex(typeIdx);

    int geomIdx = m_cableGeomModeCombo->findData(static_cast<int>(cable->geometryMode()));
    if (geomIdx >= 0) m_cableGeomModeCombo->setCurrentIndex(geomIdx);

    // Diameter in mm
    m_cableDiaSpin->setValue(cable->definition().nominalDiameter() * 1000.0);
    // Area in mm2
    m_cableAreaSpin->setValue(cable->definition().area() * 1e6);
    // Modulus in GPa
    m_cableModulusSpin->setValue(cable->definition().elasticModulus() / 1e9);
    // Initial tension in kN
    m_cableInitialTensionSpin->setValue(cable->prestress().initialTension / 1000.0);
    // Sag in m
    m_cableSagSpin->setValue(cable->geometry().sag());

    int startAncIdx = m_cableStartAnchorCombo->findData(static_cast<int>(cable->startAnchor().type()));
    if (startAncIdx >= 0) m_cableStartAnchorCombo->setCurrentIndex(startAncIdx);

    int endAncIdx = m_cableEndAnchorCombo->findData(static_cast<int>(cable->endAnchor().type()));
    if (endAncIdx >= 0) m_cableEndAnchorCombo->setCurrentIndex(endAncIdx);

    m_cableTensionOnlyCheck->setChecked(cable->analysisProperties().tensionOnly);

    // Ernst equivalent modulus
    double E_eq = cable->equivalentElasticModulus(*m_model);
    m_cableErnstModulusLabel->setText(QString("%1 GPa").arg(E_eq / 1e9, 0, 'f', 2));

    m_cableColor = "#3296DC";
    setupColorButton(m_cableColorBtn, m_cableColor);

    m_cableGroup->setVisible(true);
}

void PropertyPanel::onApplyNode()
{
    if (!m_model || m_currentNodeId < 0) return;
    auto* node = m_model->getNode(m_currentNodeId);
    if (!node) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Nœud %1").arg(m_currentNodeId).toStdString());

    node->setName(m_nodeNameEdit->text().toStdString());
    node->setCoordinates(m_nodeXSpin->value(), m_nodeYSpin->value(), m_nodeZSpin->value());
    node->setSupportType(static_cast<TSA::Model::SupportType>(m_nodeSupportCombo->currentData().toInt()));
    node->setColor(m_nodeColor.toStdString());

    m_model->notifyNodeModified(m_currentNodeId);
    emit elementModified();
}

void PropertyPanel::onApplyBeam()
{
    if (!m_model || m_currentBeamId < 0) return;
    auto* beam = m_model->getBeam(m_currentBeamId);
    if (!beam) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Barre %1").arg(m_currentBeamId).toStdString());

    beam->setName(m_beamNameEdit->text().toStdString());
    beam->setRole(static_cast<TSA::Model::BarRole>(m_beamRoleCombo->currentData().toInt()));

    // 1. Mise à jour de la section depuis l'interface
    auto sec = getBeamSectionFromUi();
    beam->setSection(sec);

    // 2. Mise à jour du matériau
    int matCode = m_beamMaterialCombo->currentData().toInt();
    const auto* pMat = TSA::Model::MaterialLibrary::instance().findById(matCode);
    if (pMat)
    {
        beam->setMaterial(*pMat);
    }
    else
    {
        beam->setMaterial(TSA::Model::Material::findByName(m_beamMaterialCombo->currentText().toStdString()));
    }

    // 3. Mise à jour de l'orientation gamma
    beam->setRotation(m_beamRotationSpin->value());

    // 4. Excentrement
    beam->setEccentricity(static_cast<TSA::Model::BarEccentricity>(m_beamEccentricityCombo->currentData().toInt()));

    // 5. Relâchements aux extrémités
    TSA::Model::EndRelease sr;
    sr.fx = m_beamStartUx ? m_beamStartUx->isChecked() : false;
    sr.fy = m_beamStartUy ? m_beamStartUy->isChecked() : false;
    sr.fz = m_beamStartUz ? m_beamStartUz->isChecked() : false;
    sr.mx = m_beamStartRx ? m_beamStartRx->isChecked() : false;
    sr.my = m_beamStartRy ? m_beamStartRy->isChecked() : false;
    sr.mz = m_beamStartRz ? m_beamStartRz->isChecked() : false;
    beam->setStartRelease(sr);

    TSA::Model::EndRelease er;
    er.fx = m_beamEndUx ? m_beamEndUx->isChecked() : false;
    er.fy = m_beamEndUy ? m_beamEndUy->isChecked() : false;
    er.fz = m_beamEndUz ? m_beamEndUz->isChecked() : false;
    er.mx = m_beamEndRx ? m_beamEndRx->isChecked() : false;
    er.my = m_beamEndRy ? m_beamEndRy->isChecked() : false;
    er.mz = m_beamEndRz ? m_beamEndRz->isChecked() : false;
    beam->setEndRelease(er);

    // 6. Mise à jour de la couleur
    beam->setColor(m_beamColor.toStdString());

    // 7. Synchronisation aperçu graphique & calculs
    updateBeamCalculatedProperties(sec);
    if (m_beamSectionPreview)
    {
        m_beamSectionPreview->setSection(sec);
        m_beamSectionPreview->setRotation(beam->rotation());
        m_beamSectionPreview->setEccentricity(beam->eccentricity(), m_beamEySpin ? m_beamEySpin->value() : 0.0, m_beamEzSpin ? m_beamEzSpin->value() : 0.0);
    }

    // 8. Notification immédiate -> reconstruction 3D automatique
    m_model->notifyBeamModified(m_currentBeamId);
    emit elementModified();
}

void PropertyPanel::onApplyColumn()
{
    if (!m_model || m_currentColumnId < 0) return;
    auto* col = m_model->getColumn(m_currentColumnId);
    if (!col) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Poteau %1").arg(m_currentColumnId).toStdString());

    col->setName(m_columnNameEdit->text().toStdString());

    // 1. Mise à jour de la section depuis l'interface
    auto sec = getColumnSectionFromUi();
    col->setSection(sec);

    // 2. Mise à jour du matériau
    int colMatCode = m_columnMaterialCombo->currentData().toInt();
    const auto* pColMat = TSA::Model::MaterialLibrary::instance().findById(colMatCode);
    if (pColMat)
    {
        col->setMaterial(*pColMat);
    }
    else
    {
        col->setMaterial(TSA::Model::Material::findByName(m_columnMaterialCombo->currentText().toStdString()));
    }

    // 3. Mise à jour de l'orientation bêta
    col->setRotation(m_columnRotationSpin->value());

    // 4. Mise à jour de la couleur
    col->setColor(m_columnColor.toStdString());

    // 5. Synchronisation aperçu graphique & calculs
    updateColumnCalculatedProperties(sec);
    if (m_columnSectionPreview)
    {
        m_columnSectionPreview->setSection(sec);
        m_columnSectionPreview->setRotation(col->rotation());
    }

    // 6. Notification immédiate -> reconstruction 3D automatique
    m_model->notifyColumnModified(m_currentColumnId);
    emit elementModified();
}

void PropertyPanel::onApplySlab()
{
    if (!m_model || m_currentSlabId < 0) return;
    auto* slab = m_model->getSlab(m_currentSlabId);
    if (!slab) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Dalle %1").arg(m_currentSlabId).toStdString());

    slab->setName(m_slabNameEdit->text().toStdString());
    slab->setThickness(m_slabThicknessSpin->value());

    if (m_slabRadioOneWay->isChecked()) slab->setSlabType(TSA::Model::SlabType::OneWay);
    else if (m_slabRadioFlat->isChecked()) slab->setSlabType(TSA::Model::SlabType::FlatSlab);
    else slab->setSlabType(TSA::Model::SlabType::TwoWay);

    int slabMatCode = m_slabMaterialCombo->currentData().toInt();
    const auto* pSlabMat = TSA::Model::MaterialLibrary::instance().findById(slabMatCode);
    if (pSlabMat)
    {
        slab->setMaterial(*pSlabMat);
    }
    else
    {
        slab->setMaterial(TSA::Model::Material::findByName(m_slabMaterialCombo->currentText().toStdString()));
    }

    slab->setColor(m_slabColor.toStdString());

    m_model->notifySlabModified(m_currentSlabId);
    emit elementModified();
}

void PropertyPanel::onApplyWall()
{
    if (!m_model || m_currentWallId < 0) return;
    auto* wall = m_model->getWall(m_currentWallId);
    if (!wall) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Voile %1").arg(m_currentWallId).toStdString());

    wall->setName(m_wallNameEdit->text().toStdString());
    wall->setHeight(m_wallHeightSpin->value());
    wall->setThickness(m_wallThicknessSpin->value());
    wall->setOffset(m_wallOffsetSpin->value());

    int wallMatCode = m_wallMaterialCombo->currentData().toInt();
    const auto* pWallMat = TSA::Model::MaterialLibrary::instance().findById(wallMatCode);
    if (pWallMat)
    {
        wall->setMaterial(*pWallMat);
    }
    else
    {
        wall->setMaterial(TSA::Model::Material::findByName(m_wallMaterialCombo->currentText().toStdString()));
    }

    wall->setColor(m_wallColor.toStdString());

    m_model->notifyWallModified(m_currentWallId);
    emit elementModified();
}

void PropertyPanel::onApplyFoundation()
{
    if (!m_model || m_currentFoundationId < 0) return;
    auto* f = m_model->getFoundation(m_currentFoundationId);
    if (!f) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Fondation %1").arg(m_currentFoundationId).toStdString());

    f->setName(m_foundationNameEdit->text().toStdString());
    f->setFoundationType(static_cast<TSA::Model::FoundationType>(m_foundationTypeCombo->currentData().toInt()));
    f->setWidthA(m_foundationWidthASpin->value());
    f->setLengthB(m_foundationLengthBSpin->value());
    f->setHeightH(m_foundationHeightHSpin->value());
    f->setSoilBearingCapacity(m_foundationSoilCapacitySpin->value());

    int fMatCode = m_foundationMaterialCombo->currentData().toInt();
    const auto* pFMat = TSA::Model::MaterialLibrary::instance().findById(fMatCode);
    if (pFMat)
    {
        f->setMaterial(*pFMat);
    }
    else
    {
        f->setMaterial(TSA::Model::Material::findByName(m_foundationMaterialCombo->currentText().toStdString()));
    }

    f->setColor(m_foundationColor.toStdString());

    m_model->notifyFoundationModified(m_currentFoundationId);
    emit elementModified();
}

void PropertyPanel::onApplyTruss()
{
    if (!m_model || m_currentTrussId < 0) return;
    auto* truss = m_model->getTrussMember(m_currentTrussId);
    if (!truss) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Treillis %1").arg(m_currentTrussId).toStdString());

    truss->setName(m_trussNameEdit->text().toStdString());
    truss->setRole(static_cast<TSA::Model::TrussMemberRole>(m_trussRoleCombo->currentData().toInt()));
    truss->section().width = m_trussDimensionSpin->value();
    truss->section().height = m_trussDimensionSpin->value();
    truss->section().diameter = m_trussDimensionSpin->value();

    int trMatCode = m_trussMaterialCombo->currentData().toInt();
    const auto* pTrMat = TSA::Model::MaterialLibrary::instance().findById(trMatCode);
    if (pTrMat)
    {
        truss->setMaterial(*pTrMat);
    }
    else
    {
        truss->setMaterial(TSA::Model::Material::findByName(m_trussMaterialCombo->currentText().toStdString()));
    }

    truss->setColor(m_trussColor.toStdString());

    m_model->notifyTrussMemberModified(m_currentTrussId);
    emit elementModified();
}

void PropertyPanel::onApplyCable()
{
    if (!m_model || m_currentCableId < 0) return;
    auto* cable = m_model->getCable(m_currentCableId);
    if (!cable) return;

    SelfUpdateGuard selfGuard(m_isUpdatingFromSelf);
    m_model->pushUndoState(tr("Modification Câble %1").arg(m_currentCableId).toStdString());

    cable->setName(m_cableNameEdit->text().toStdString());
    cable->setType(static_cast<TSA::Model::CableType>(m_cableTypeCombo->currentData().toInt()));
    cable->setGeometryMode(static_cast<TSA::Model::CableGeometryMode>(m_cableGeomModeCombo->currentData().toInt()));

    cable->definition().setNominalDiameter(m_cableDiaSpin->value() / 1000.0);
    cable->definition().setArea(m_cableAreaSpin->value() / 1e6);
    cable->definition().setElasticModulus(m_cableModulusSpin->value() * 1e9);
    cable->prestress().initialTension = m_cableInitialTensionSpin->value() * 1000.0;
    cable->definition().setInitialTension(cable->prestress().initialTension);
    cable->geometry().setSag(m_cableSagSpin->value());

    cable->startAnchor().setType(static_cast<TSA::Model::AnchorType>(m_cableStartAnchorCombo->currentData().toInt()));
    cable->endAnchor().setType(static_cast<TSA::Model::AnchorType>(m_cableEndAnchorCombo->currentData().toInt()));
    cable->analysisProperties().tensionOnly = m_cableTensionOnlyCheck->isChecked();

    m_model->notifyCableModified(m_currentCableId);
    emit elementModified();
}

void PropertyPanel::onWidgetChanged()
{
    if (m_isLoading)
        return;
    if (!m_chkLiveSync || !m_chkLiveSync->isChecked())
        return;

    switch (m_currentType)
    {
    case CurrentType::Node:
        onApplyNode();
        break;
    case CurrentType::Beam:
        onApplyBeam();
        break;
    case CurrentType::Column:
        onApplyColumn();
        break;
    case CurrentType::Slab:
        onApplySlab();
        break;
    case CurrentType::Wall:
        onApplyWall();
        break;
    case CurrentType::Foundation:
        onApplyFoundation();
        break;
    case CurrentType::Truss:
        onApplyTruss();
        break;
    case CurrentType::Cable:
        onApplyCable();
        break;
    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// Implémentation IModelObserver pour la synchronisation bidirectionnelle
// -----------------------------------------------------------------------------

void PropertyPanel::onNodeModified(const TSA::Model::Node& node)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Node && m_currentNodeId == node.id())
    {
        showNodeProperties(node.id());
    }
}

void PropertyPanel::onNodeRemoved(int nodeId)
{
    if (m_currentType == CurrentType::Node && m_currentNodeId == nodeId)
    {
        clearProperties();
    }
}

void PropertyPanel::onBeamModified(const TSA::Model::Beam& beam)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Beam && m_currentBeamId == beam.id())
    {
        showBeamProperties(beam.id());
    }
}

void PropertyPanel::onBeamRemoved(int beamId)
{
    if (m_currentType == CurrentType::Beam && m_currentBeamId == beamId)
    {
        clearProperties();
    }
}

void PropertyPanel::onColumnModified(const TSA::Model::Column& column)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Column && m_currentColumnId == column.id())
    {
        showColumnProperties(column.id());
    }
}

void PropertyPanel::onColumnRemoved(int columnId)
{
    if (m_currentType == CurrentType::Column && m_currentColumnId == columnId)
    {
        clearProperties();
    }
}

void PropertyPanel::onSlabModified(const TSA::Model::Slab& slab)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Slab && m_currentSlabId == slab.id())
    {
        showSlabProperties(slab.id());
    }
}

void PropertyPanel::onSlabRemoved(int slabId)
{
    if (m_currentType == CurrentType::Slab && m_currentSlabId == slabId)
    {
        clearProperties();
    }
}

void PropertyPanel::onWallModified(const TSA::Model::Wall& wall)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Wall && m_currentWallId == wall.id())
    {
        showWallProperties(wall.id());
    }
}

void PropertyPanel::onWallRemoved(int wallId)
{
    if (m_currentType == CurrentType::Wall && m_currentWallId == wallId)
    {
        clearProperties();
    }
}

void PropertyPanel::onFoundationModified(const TSA::Model::Foundation& foundation)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Foundation && m_currentFoundationId == foundation.id())
    {
        showFoundationProperties(foundation.id());
    }
}

void PropertyPanel::onFoundationRemoved(int foundationId)
{
    if (m_currentType == CurrentType::Foundation && m_currentFoundationId == foundationId)
    {
        clearProperties();
    }
}

void PropertyPanel::onTrussMemberModified(const TSA::Model::TrussMember& member)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Truss && m_currentTrussId == member.id())
    {
        showTrussMemberProperties(member.id());
    }
}

void PropertyPanel::onTrussMemberRemoved(int memberId)
{
    if (m_currentType == CurrentType::Truss && m_currentTrussId == memberId)
    {
        clearProperties();
    }
}

void PropertyPanel::onCableModified(const TSA::Model::Cable& cable)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Cable && m_currentCableId == cable.id())
    {
        showCableProperties(cable.id());
    }
}

void PropertyPanel::onCableRemoved(int cableId)
{
    if (m_currentType == CurrentType::Cable && m_currentCableId == cableId)
    {
        clearProperties();
    }
}

void PropertyPanel::onModelDiffApplied(const TSA::Model::ModelDiff& diff)
{
    if (m_isUpdatingFromSelf) return;
    if (m_currentType == CurrentType::Node && m_currentNodeId >= 0)
    {
        for (int id : diff.modifiedNodeIds) { if (id == m_currentNodeId) { showNodeProperties(id); break; } }
        for (int id : diff.deletedNodeIds) { if (id == m_currentNodeId) { clearProperties(); break; } }
    }
    else if (m_currentType == CurrentType::Beam && m_currentBeamId >= 0)
    {
        for (int id : diff.modifiedBeamIds) { if (id == m_currentBeamId) { showBeamProperties(id); break; } }
        for (int id : diff.deletedBeamIds) { if (id == m_currentBeamId) { clearProperties(); break; } }
    }
    else if (m_currentType == CurrentType::Column && m_currentColumnId >= 0)
    {
        for (int id : diff.modifiedColumnIds) { if (id == m_currentColumnId) { showColumnProperties(id); break; } }
        for (int id : diff.deletedColumnIds) { if (id == m_currentColumnId) { clearProperties(); break; } }
    }
    else if (m_currentType == CurrentType::Slab && m_currentSlabId >= 0)
    {
        for (int id : diff.slabs.modified) { if (id == m_currentSlabId) { showSlabProperties(id); break; } }
        for (int id : diff.slabs.deleted) { if (id == m_currentSlabId) { clearProperties(); break; } }
    }
    else if (m_currentType == CurrentType::Cable && m_currentCableId >= 0)
    {
        for (int id : diff.modifiedCableIds) { if (id == m_currentCableId) { showCableProperties(id); break; } }
        for (int id : diff.deletedCableIds) { if (id == m_currentCableId) { clearProperties(); break; } }
    }
}

void PropertyPanel::onModelCleared()
{
    clearProperties();
}

} // namespace TSA::UI
