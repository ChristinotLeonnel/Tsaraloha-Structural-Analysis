#include "GridDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <sstream>

namespace TSA::UI
{

static std::vector<double> parseDoubleList(const QString& text)
{
    std::vector<double> res;
    QStringList parts = text.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        bool ok = false;
        double val = part.toDouble(&ok);
        if (ok)
        {
            res.push_back(val);
        }
    }
    return res;
}

static QString formatDoubleList(const std::vector<double>& vec)
{
    QStringList parts;
    for (double val : vec)
    {
        parts << QString::number(val, 'f', 2);
    }
    return parts.join("; ");
}

GridDialog::GridDialog(QWidget* parent)
    : QDialog(parent)
    , m_isEditMode(false)
{
    setupUi();
}

GridDialog::GridDialog(const TSA::Grid::GridDefinition& existingDef, QWidget* parent)
    : QDialog(parent)
    , m_isEditMode(true)
    , m_gridId(existingDef.id())
{
    setupUi();
    loadFromDefinition(existingDef);
}

void GridDialog::setupUi()
{
    setWindowTitle(m_isEditMode ? tr("Modifier la Grille") : tr("Créer une Nouvelle Grille"));
    resize(520, 560);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Informations générales (Nom, Type, Origine)
    auto* generalGroup = new QGroupBox(tr("Informations Générales"), this);
    auto* generalForm = new QFormLayout(generalGroup);

    m_nameEdit = new QLineEdit(m_isEditMode ? tr("Grille") : tr("Nouvelle Grille"), generalGroup);
    m_typeCombo = new QComboBox(generalGroup);
    m_typeCombo->addItem(tr("Cartésienne (X, Y, Z)"), static_cast<int>(TSA::Grid::GridType::Cartesian));
    m_typeCombo->addItem(tr("Cylindrique / Polaire (R, θ, Z)"), static_cast<int>(TSA::Grid::GridType::Cylindrical));

    generalForm->addRow(tr("Nom de la grille :"), m_nameEdit);
    generalForm->addRow(tr("Système de coordonnées :"), m_typeCombo);

    // Origine
    auto* originLayout = new QHBoxLayout();
    m_originXSpin = new QDoubleSpinBox(generalGroup);
    m_originYSpin = new QDoubleSpinBox(generalGroup);
    m_originZSpin = new QDoubleSpinBox(generalGroup);

    for (auto* spin : { m_originXSpin, m_originYSpin, m_originZSpin })
    {
        spin->setRange(-10000.0, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(1.0);
    }

    originLayout->addWidget(new QLabel("X:"));
    originLayout->addWidget(m_originXSpin);
    originLayout->addWidget(new QLabel("Y:"));
    originLayout->addWidget(m_originYSpin);
    originLayout->addWidget(new QLabel("Z:"));
    originLayout->addWidget(m_originZSpin);

    generalForm->addRow(tr("Point d'origine (m) :"), originLayout);
    mainLayout->addWidget(generalGroup);

    // 2. Onglets Paramètres (Cartésien vs Cylindrique)
    m_paramTabs = new QTabWidget(this);

    // --- Onglet Cartésien ---
    m_cartesianTab = new QWidget();
    auto* cartLayout = new QVBoxLayout(m_cartesianTab);

    auto* cartModeGroup = new QGroupBox(tr("Mode de génération des axes"), m_cartesianTab);
    auto* cartModeLayout = new QHBoxLayout(cartModeGroup);
    m_radioCartRegular = new QRadioButton(tr("Entraxes réguliers (Nombre × Espacement)"), cartModeGroup);
    m_radioCartCustom = new QRadioButton(tr("Positions irrégulières explicites"), cartModeGroup);
    m_radioCartRegular->setChecked(true);
    cartModeLayout->addWidget(m_radioCartRegular);
    cartModeLayout->addWidget(m_radioCartCustom);
    cartLayout->addWidget(cartModeGroup);

    // Formulaire cartésien régulier
    auto* cartRegGroup = new QGroupBox(tr("Définition régulière"), m_cartesianTab);
    auto* cartRegForm = new QFormLayout(cartRegGroup);

    auto* xRegLayout = new QHBoxLayout();
    m_xCountSpin = new QSpinBox(cartRegGroup);
    m_xCountSpin->setRange(1, 100);
    m_xCountSpin->setValue(3);
    m_xSpacingSpin = new QDoubleSpinBox(cartRegGroup);
    m_xSpacingSpin->setRange(0.1, 1000.0);
    m_xSpacingSpin->setValue(5.0);
    xRegLayout->addWidget(new QLabel(tr("Nb traves :")));
    xRegLayout->addWidget(m_xCountSpin);
    xRegLayout->addWidget(new QLabel(tr("Pas (m) :")));
    xRegLayout->addWidget(m_xSpacingSpin);
    cartRegForm->addRow(tr("Axes X :"), xRegLayout);

    auto* yRegLayout = new QHBoxLayout();
    m_yCountSpin = new QSpinBox(cartRegGroup);
    m_yCountSpin->setRange(1, 100);
    m_yCountSpin->setValue(2);
    m_ySpacingSpin = new QDoubleSpinBox(cartRegGroup);
    m_ySpacingSpin->setRange(0.1, 1000.0);
    m_ySpacingSpin->setValue(4.0);
    yRegLayout->addWidget(new QLabel(tr("Nb traves :")));
    yRegLayout->addWidget(m_yCountSpin);
    yRegLayout->addWidget(new QLabel(tr("Pas (m) :")));
    yRegLayout->addWidget(m_ySpacingSpin);
    cartRegForm->addRow(tr("Axes Y :"), yRegLayout);

    auto* zRegLayout = new QHBoxLayout();
    m_zCountSpin = new QSpinBox(cartRegGroup);
    m_zCountSpin->setRange(0, 100);
    m_zCountSpin->setValue(2);
    m_zSpacingSpin = new QDoubleSpinBox(cartRegGroup);
    m_zSpacingSpin->setRange(0.1, 1000.0);
    m_zSpacingSpin->setValue(3.0);
    zRegLayout->addWidget(new QLabel(tr("Nb étages :")));
    zRegLayout->addWidget(m_zCountSpin);
    zRegLayout->addWidget(new QLabel(tr("Hauteur (m) :")));
    zRegLayout->addWidget(m_zSpacingSpin);
    cartRegForm->addRow(tr("Niveaux Z :"), zRegLayout);

    cartLayout->addWidget(cartRegGroup);

    // Formulaire cartésien personnalisé (irrégulier)
    auto* cartCustomGroup = new QGroupBox(tr("Positions explicites (séparées par des points-virgules)"), m_cartesianTab);
    auto* cartCustomForm = new QFormLayout(cartCustomGroup);
    m_xCustomEdit = new QLineEdit("0.0; 5.0; 10.0; 15.0", cartCustomGroup);
    m_yCustomEdit = new QLineEdit("0.0; 4.0; 8.0", cartCustomGroup);
    m_zCustomEdit = new QLineEdit("0.0; 3.0; 6.0", cartCustomGroup);
    cartCustomForm->addRow(tr("X (m) :"), m_xCustomEdit);
    cartCustomForm->addRow(tr("Y (m) :"), m_yCustomEdit);
    cartCustomForm->addRow(tr("Z (m) :"), m_zCustomEdit);
    cartLayout->addWidget(cartCustomGroup);

    m_paramTabs->addTab(m_cartesianTab, tr("Cartésien"));

    // --- Onglet Cylindrique ---
    m_cylindricalTab = new QWidget();
    auto* cylLayout = new QVBoxLayout(m_cylindricalTab);

    auto* cylModeGroup = new QGroupBox(tr("Mode de génération polaire"), m_cylindricalTab);
    auto* cylModeLayout = new QHBoxLayout(cylModeGroup);
    m_radioCylRegular = new QRadioButton(tr("Régulier (Rayons & Angles réguliers)"), cylModeGroup);
    m_radioCylCustom = new QRadioButton(tr("Rayons & Angles explicites"), cylModeGroup);
    m_radioCylRegular->setChecked(true);
    cylModeLayout->addWidget(m_radioCylRegular);
    cylModeLayout->addWidget(m_radioCylCustom);
    cylLayout->addWidget(cylModeGroup);

    auto* cylRegGroup = new QGroupBox(tr("Définition régulière"), m_cylindricalTab);
    auto* cylRegForm = new QFormLayout(cylRegGroup);

    auto* rRegLayout = new QHBoxLayout();
    m_radiusCountSpin = new QSpinBox(cylRegGroup);
    m_radiusCountSpin->setRange(1, 100);
    m_radiusCountSpin->setValue(4);
    m_radiusSpacingSpin = new QDoubleSpinBox(cylRegGroup);
    m_radiusSpacingSpin->setRange(0.1, 1000.0);
    m_radiusSpacingSpin->setValue(2.0);
    rRegLayout->addWidget(new QLabel(tr("Nb rayons :")));
    rRegLayout->addWidget(m_radiusCountSpin);
    rRegLayout->addWidget(new QLabel(tr("Pas R (m) :")));
    rRegLayout->addWidget(m_radiusSpacingSpin);
    cylRegForm->addRow(tr("Rayons R :"), rRegLayout);

    auto* angRegLayout = new QHBoxLayout();
    m_angleCountSpin = new QSpinBox(cylRegGroup);
    m_angleCountSpin->setRange(1, 360);
    m_angleCountSpin->setValue(12);
    m_angleSpacingSpin = new QDoubleSpinBox(cylRegGroup);
    m_angleSpacingSpin->setRange(1.0, 360.0);
    m_angleSpacingSpin->setValue(30.0);
    angRegLayout->addWidget(new QLabel(tr("Nb rayons :")));
    angRegLayout->addWidget(m_angleCountSpin);
    angRegLayout->addWidget(new QLabel(tr("Pas θ (°) :")));
    angRegLayout->addWidget(m_angleSpacingSpin);
    cylRegForm->addRow(tr("Angles θ :"), angRegLayout);

    auto* cylZRegLayout = new QHBoxLayout();
    m_cylZCountSpin = new QSpinBox(cylRegGroup);
    m_cylZCountSpin->setRange(0, 100);
    m_cylZCountSpin->setValue(2);
    m_cylZSpacingSpin = new QDoubleSpinBox(cylRegGroup);
    m_cylZSpacingSpin->setRange(0.1, 1000.0);
    m_cylZSpacingSpin->setValue(3.0);
    cylZRegLayout->addWidget(new QLabel(tr("Nb étages :")));
    cylZRegLayout->addWidget(m_cylZCountSpin);
    cylZRegLayout->addWidget(new QLabel(tr("Hauteur (m) :")));
    cylZRegLayout->addWidget(m_cylZSpacingSpin);
    cylRegForm->addRow(tr("Niveaux Z :"), cylZRegLayout);

    cylLayout->addWidget(cylRegGroup);

    auto* cylCustomGroup = new QGroupBox(tr("Valeurs explicites"), m_cylindricalTab);
    auto* cylCustomForm = new QFormLayout(cylCustomGroup);
    m_radiiCustomEdit = new QLineEdit("2.0; 4.0; 6.0; 8.0", cylCustomGroup);
    m_anglesCustomEdit = new QLineEdit("0; 30; 60; 90; 120; 150; 180; 210; 240; 270; 300; 330", cylCustomGroup);
    m_cylZCustomEdit = new QLineEdit("0.0; 3.0; 6.0", cylCustomGroup);
    cylCustomForm->addRow(tr("Rayons R (m) :"), m_radiiCustomEdit);
    cylCustomForm->addRow(tr("Angles θ (°) :"), m_anglesCustomEdit);
    cylCustomForm->addRow(tr("Niveaux Z (m) :"), m_cylZCustomEdit);
    cylLayout->addWidget(cylCustomGroup);

    m_paramTabs->addTab(m_cylindricalTab, tr("Cylindrique"));
    mainLayout->addWidget(m_paramTabs);

    // 3. Boutons standard (OK / Annuler)
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Connexions
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GridDialog::onTypeChanged);
    connect(m_radioCartRegular, &QRadioButton::toggled, this, &GridDialog::onCartesianModeToggled);
    connect(m_radioCartCustom, &QRadioButton::toggled, this, &GridDialog::onCartesianModeToggled);
    connect(m_radioCylRegular, &QRadioButton::toggled, this, &GridDialog::onCylindricalModeToggled);
    connect(m_radioCylCustom, &QRadioButton::toggled, this, &GridDialog::onCylindricalModeToggled);

    onTypeChanged(m_typeCombo->currentIndex());
    onCartesianModeToggled();
    onCylindricalModeToggled();
}

void GridDialog::onTypeChanged(int index)
{
    m_paramTabs->setCurrentIndex(index);
}

void GridDialog::onCartesianModeToggled()
{
    bool isReg = m_radioCartRegular->isChecked();
    m_xCountSpin->setEnabled(isReg);
    m_xSpacingSpin->setEnabled(isReg);
    m_yCountSpin->setEnabled(isReg);
    m_ySpacingSpin->setEnabled(isReg);
    m_zCountSpin->setEnabled(isReg);
    m_zSpacingSpin->setEnabled(isReg);

    m_xCustomEdit->setEnabled(!isReg);
    m_yCustomEdit->setEnabled(!isReg);
    m_zCustomEdit->setEnabled(!isReg);
}

void GridDialog::onCylindricalModeToggled()
{
    bool isReg = m_radioCylRegular->isChecked();
    m_radiusCountSpin->setEnabled(isReg);
    m_radiusSpacingSpin->setEnabled(isReg);
    m_angleCountSpin->setEnabled(isReg);
    m_angleSpacingSpin->setEnabled(isReg);
    m_cylZCountSpin->setEnabled(isReg);
    m_cylZSpacingSpin->setEnabled(isReg);

    m_radiiCustomEdit->setEnabled(!isReg);
    m_anglesCustomEdit->setEnabled(!isReg);
    m_cylZCustomEdit->setEnabled(!isReg);
}

void GridDialog::loadFromDefinition(const TSA::Grid::GridDefinition& def)
{
    m_nameEdit->setText(QString::fromStdString(def.name()));
    m_typeCombo->setCurrentIndex(def.type() == TSA::Grid::GridType::Cartesian ? 0 : 1);
    m_originXSpin->setValue(def.origin().X());
    m_originYSpin->setValue(def.origin().Y());
    m_originZSpin->setValue(def.origin().Z());

    if (def.type() == TSA::Grid::GridType::Cartesian)
    {
        m_radioCartCustom->setChecked(true);
        m_xCustomEdit->setText(formatDoubleList(def.xPositions()));
        m_yCustomEdit->setText(formatDoubleList(def.yPositions()));
        m_zCustomEdit->setText(formatDoubleList(def.zLevels()));
    }
    else
    {
        m_radioCylCustom->setChecked(true);
        m_radiiCustomEdit->setText(formatDoubleList(def.radii()));
        m_anglesCustomEdit->setText(formatDoubleList(def.angles()));
        m_cylZCustomEdit->setText(formatDoubleList(def.zLevels()));
    }
}

TSA::Grid::GridDefinition GridDialog::getDefinition() const
{
    auto type = static_cast<TSA::Grid::GridType>(m_typeCombo->currentData().toInt());
    std::string name = m_nameEdit->text().trimmed().toStdString();
    if (name.empty()) name = (type == TSA::Grid::GridType::Cartesian) ? "Main Grid" : "Circular Grid";

    TSA::Grid::GridDefinition def(name, type);
    if (m_isEditMode && !m_gridId.empty())
    {
        def.setId(m_gridId);
    }
    def.setOrigin(m_originXSpin->value(), m_originYSpin->value(), m_originZSpin->value());

    if (type == TSA::Grid::GridType::Cartesian)
    {
        if (m_radioCartRegular->isChecked())
        {
            def.generateCartesian(m_xCountSpin->value(), m_xSpacingSpin->value(),
                                  m_yCountSpin->value(), m_ySpacingSpin->value(),
                                  m_zCountSpin->value(), m_zSpacingSpin->value());
        }
        else
        {
            def.setXPositions(parseDoubleList(m_xCustomEdit->text()));
            def.setYPositions(parseDoubleList(m_yCustomEdit->text()));
            def.setZLevels(parseDoubleList(m_zCustomEdit->text()));
        }
    }
    else
    {
        if (m_radioCylRegular->isChecked())
        {
            def.generateCylindrical(m_radiusCountSpin->value(), m_radiusSpacingSpin->value(),
                                    m_angleCountSpin->value(), m_angleSpacingSpin->value(),
                                    m_cylZCountSpin->value(), m_cylZSpacingSpin->value());
        }
        else
        {
            def.setRadii(parseDoubleList(m_radiiCustomEdit->text()));
            def.setAngles(parseDoubleList(m_anglesCustomEdit->text()));
            def.setZLevels(parseDoubleList(m_cylZCustomEdit->text()));
        }
    }

    return def;
}

} // namespace TSA::UI
