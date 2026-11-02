#include "LoadStyleDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace TSA::UI
{
using TSA::Viewer::LoadCaseFilter;
using TSA::Viewer::LoadColorMode;
using TSA::Viewer::LoadDisplaySettings;
using TSA::Viewer::LoadDisplayStyle;
using TSA::Viewer::LoadTextSize;

LoadStyleDialog::LoadStyleDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Style des charges"));
    setObjectName(QStringLiteral("LoadStyleDialog"));
    m_original = LoadDisplayStyle::instance().settings();
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Affichage des charges dans la vue 3D. Les valeurs des charges ne sont jamais modifiées."), this));

    // 1. Cas affichés
    auto* cases = new QGroupBox(tr("Cas affichés"), this);
    auto* casesLayout = new QVBoxLayout(cases);
    m_activeCase = new QRadioButton(tr("Cas actif seulement (choix : Charges › Cas affichés)"), cases);
    m_allCases = new QRadioButton(tr("Tous les cas superposés"), cases);
    auto* casesGroup = new QButtonGroup(cases);
    casesGroup->addButton(m_activeCase);
    casesGroup->addButton(m_allCases);
    casesLayout->addWidget(m_activeCase);
    casesLayout->addWidget(m_allCases);
    layout->addWidget(cases);

    // 2. Couleurs
    auto* colors = new QGroupBox(tr("Couleurs"), this);
    auto* colorsLayout = new QHBoxLayout(colors);
    m_colorMode = new QComboBox(colors);
    m_colorMode->addItem(tr("Par cas de charge"), int(LoadColorMode::ByCase));
    m_colorMode->addItem(tr("Par intensité (bleu → rouge)"), int(LoadColorMode::ByIntensity));
    m_colorMode->addItem(tr("Par famille (forces, moments, linéiques, surfaciques)"), int(LoadColorMode::ByFamily));
    m_colorMode->addItem(tr("Couleur unique"), int(LoadColorMode::Single));
    m_colorButton = new QPushButton(tr("Couleur…"), colors);
    colorsLayout->addWidget(m_colorMode, 1);
    colorsLayout->addWidget(m_colorButton);
    layout->addWidget(colors);

    // 3. Texte des valeurs
    auto* text = new QGroupBox(tr("Texte des valeurs"), this);
    auto* form = new QFormLayout(text);
    m_textModel = new QRadioButton(tr("Hauteur liée au modèle, comme une cote (grandit en zoomant, bornée à l'écran)"), text);
    m_textScreen = new QRadioButton(tr("Hauteur constante à l'écran"), text);
    auto* sizeGroup = new QButtonGroup(text);
    sizeGroup->addButton(m_textModel);
    sizeGroup->addButton(m_textScreen);
    form->addRow(m_textModel);
    auto* heightRow = new QHBoxLayout;
    m_autoHeight = new QCheckBox(tr("Automatique (d'après la structure)"), text);
    m_textHeight = new QDoubleSpinBox(text);
    m_textHeight->setRange(0.01, 5.0);
    m_textHeight->setDecimals(2);
    m_textHeight->setSingleStep(0.05);
    m_textHeight->setSuffix(QStringLiteral(" m"));
    heightRow->addWidget(m_autoHeight);
    heightRow->addWidget(m_textHeight);
    form->addRow(tr("Hauteur :"), heightRow);
    // Bornes à l'écran : jamais illisible en vue d'ensemble, jamais démesuré en vue rapprochée
    auto* boundsRow = new QHBoxLayout;
    m_textMinPixels = new QSpinBox(text);
    m_textMaxPixels = new QSpinBox(text);
    for (QSpinBox* b : { m_textMinPixels, m_textMaxPixels })
    {
        b->setRange(int(LoadDisplayStyle::kMinTextPixels), int(LoadDisplayStyle::kMaxTextPixels));
        b->setSuffix(QStringLiteral(" px"));
    }
    boundsRow->addWidget(new QLabel(tr("de"), text));
    boundsRow->addWidget(m_textMinPixels);
    boundsRow->addWidget(new QLabel(tr("à"), text));
    boundsRow->addWidget(m_textMaxPixels);
    boundsRow->addStretch(1);
    form->addRow(tr("À l'écran :"), boundsRow);
    form->addRow(m_textScreen);
    m_textPixels = new QSpinBox(text);
    m_textPixels->setRange(int(LoadDisplayStyle::kMinTextPixels), int(LoadDisplayStyle::kMaxTextPixels));
    m_textPixels->setSuffix(QStringLiteral(" px"));
    form->addRow(tr("Taille à l'écran :"), m_textPixels);
    m_decimals = new QSpinBox(text);
    m_decimals->setRange(0, 4);
    form->addRow(tr("Décimales :"), m_decimals);
    m_symbol = new QCheckBox(tr("Symbole (« q = », « P = », « Fz = »)"), text);
    m_units = new QCheckBox(tr("Unité (kN, kN/m, kN/m²)"), text);
    m_caseName = new QCheckBox(tr("Nom du cas (« [Q] »)"), text);
    m_background = new QCheckBox(tr("Contour contrasté (lisible devant la structure)"), text);
    m_declutter = new QCheckBox(tr("Masquer les valeurs qui se chevauchent (la plus forte reste ; zoomer pour lire les autres)"), text);
    for (QCheckBox* c : { m_symbol, m_units, m_caseName, m_background, m_declutter }) form->addRow(c);
    layout->addWidget(text);

    // 4. Flèches
    auto* arrows = new QGroupBox(tr("Flèches"), this);
    auto* arrowsForm = new QFormLayout(arrows);
    m_arrowWidth = new QDoubleSpinBox(arrows);
    m_arrowWidth->setRange(LoadDisplayStyle::kMinArrowWidth, LoadDisplayStyle::kMaxArrowWidth);
    m_arrowWidth->setSingleStep(0.25);
    m_arrowWidth->setDecimals(2);
    m_arrowWidth->setPrefix(QStringLiteral("× "));
    arrowsForm->addRow(tr("Épaisseur :"), m_arrowWidth);
    arrowsForm->addRow(new QLabel(tr("Longueur : d'après la structure (hauteur d'étage, longueur des barres), jamais le zoom — "
                                     "réglage du facteur : Affichage › Échelle des charges."),
                                  arrows));
    layout->addWidget(arrows);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        loadIntoUi(LoadDisplaySettings {});
        applyPreview();
    });

    loadIntoUi(m_original);

    // Aperçu immédiat : chaque changement est appliqué au style (les vues redessinent les charges)
    for (QRadioButton* r : { m_activeCase, m_allCases, m_textModel, m_textScreen })
        connect(r, &QRadioButton::toggled, this, [this] { applyPreview(); });
    for (QCheckBox* c : { m_autoHeight, m_symbol, m_units, m_caseName, m_background, m_declutter })
        connect(c, &QCheckBox::toggled, this, [this] { applyPreview(); });
    connect(m_colorMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { applyPreview(); });
    connect(m_textHeight, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { applyPreview(); });
    connect(m_arrowWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { applyPreview(); });
    for (QSpinBox* b : { m_textPixels, m_textMinPixels, m_textMaxPixels })
        connect(b, qOverload<int>(&QSpinBox::valueChanged), this, [this] { applyPreview(); });
    connect(m_decimals, qOverload<int>(&QSpinBox::valueChanged), this, [this] { applyPreview(); });
    connect(m_colorButton, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_singleColor, this, tr("Couleur des charges"));
        if (!c.isValid()) return;
        m_singleColor = c;
        applyPreview();
    });
}

void LoadStyleDialog::loadIntoUi(const LoadDisplaySettings& s)
{
    m_loading = true;
    (s.cases == LoadCaseFilter::AllCases ? m_allCases : m_activeCase)->setChecked(true);
    m_colorMode->setCurrentIndex(std::max(0, m_colorMode->findData(int(s.colors))));
    m_singleColor = s.singleColor;
    (s.textSize == LoadTextSize::ScreenConstant ? m_textScreen : m_textModel)->setChecked(true);
    m_autoHeight->setChecked(!(s.textHeight > 0.0));
    m_textHeight->setValue(s.textHeight > 0.0 ? s.textHeight : 0.25);
    m_textPixels->setValue(int(std::lround(s.textPixels)));
    m_textMinPixels->setValue(int(std::lround(s.textMinPixels)));
    m_textMaxPixels->setValue(int(std::lround(s.textMaxPixels)));
    m_declutter->setChecked(s.declutter);
    m_decimals->setValue(s.decimals);
    m_symbol->setChecked(s.showSymbol);
    m_units->setChecked(s.showUnits);
    m_caseName->setChecked(s.showCaseName);
    m_background->setChecked(s.textBackground);
    m_arrowWidth->setValue(s.arrowWidth);
    m_loading = false;
    updateEnabledStates();
}

LoadDisplaySettings LoadStyleDialog::settingsFromUi() const
{
    LoadDisplaySettings s;
    s.cases = m_allCases->isChecked() ? LoadCaseFilter::AllCases : LoadCaseFilter::ActiveCase;
    s.colors = static_cast<LoadColorMode>(m_colorMode->currentData().toInt());
    s.singleColor = m_singleColor;
    s.textSize = m_textScreen->isChecked() ? LoadTextSize::ScreenConstant : LoadTextSize::ModelLinked;
    s.textHeight = m_autoHeight->isChecked() ? 0.0 : m_textHeight->value();
    s.textPixels = m_textPixels->value();
    s.textMinPixels = m_textMinPixels->value();
    s.textMaxPixels = std::max(m_textMinPixels->value(), m_textMaxPixels->value());
    s.declutter = m_declutter->isChecked();
    s.decimals = m_decimals->value();
    s.showSymbol = m_symbol->isChecked();
    s.showUnits = m_units->isChecked();
    s.showCaseName = m_caseName->isChecked();
    s.textBackground = m_background->isChecked();
    s.arrowWidth = m_arrowWidth->value();
    return s;
}

void LoadStyleDialog::updateEnabledStates()
{
    const bool model = m_textModel->isChecked();
    m_autoHeight->setEnabled(model);
    m_textHeight->setEnabled(model && !m_autoHeight->isChecked());
    m_textPixels->setEnabled(!model);
    m_textMinPixels->setEnabled(model);
    m_textMaxPixels->setEnabled(model);
    m_colorButton->setEnabled(static_cast<LoadColorMode>(m_colorMode->currentData().toInt()) == LoadColorMode::Single);
}

void LoadStyleDialog::applyPreview()
{
    if (m_loading) return;
    updateEnabledStates();
    LoadDisplayStyle::instance().setSettings(settingsFromUi());
}

void LoadStyleDialog::reject()
{
    LoadDisplayStyle::instance().setSettings(m_original);   // Annuler : style d'origine rétabli
    QDialog::reject();
}

} // namespace TSA::UI
