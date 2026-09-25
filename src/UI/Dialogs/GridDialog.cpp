#include "GridDialog.h"
#include "../../Grid/GridManager.h"
#include "../../Model/Model.h"
#include "../../Viewer/OccView.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QMessageBox>
#include <QCheckBox>

namespace TSA::UI
{

GridDialog::GridDialog(QWidget* parent)
    : GridDialog(nullptr, nullptr, nullptr, parent)
{
}

GridDialog::GridDialog(TSA::Grid::GridManager* gridManager,
                       TSA::Model::Model* model,
                       OccView* occView,
                       QWidget* parent)
    : QDialog(parent)
    , m_gridManager(gridManager)
    , m_model(model)
    , m_occView(occView)
{
    setupUi();

    if (m_model && m_model->coordinateSystem())
    {
        // Charger les coordonnées actuelles du modèle
        m_axes[0].positions = m_model->coordinateSystem()->xPositions();
        m_axes[1].positions = m_model->coordinateSystem()->yPositions();
        if (m_model->levelManager())
        {
            m_axes[2].positions = m_model->levelManager()->elevationList();
        }

        m_axes[0].labelStyle = 0; // 1 2 3...
        m_axes[1].labelStyle = 1; // A B C...
        m_axes[2].labelStyle = 2; // Niveau 0 1 2...
    }
    else
    {
        // Valeurs par défaut
        m_axes[0].positions = { 0.0, 6.0, 12.0, 18.0 };
        m_axes[1].positions = { 0.0, 4.0, 8.0 };
        m_axes[2].positions = { 0.0, 3.0, 6.0 };
        m_axes[0].labelStyle = 0;
        m_axes[1].labelStyle = 1;
        m_axes[2].labelStyle = 2;
    }

    if (m_model && m_gridManager && m_gridManager->activeGrid())
    {
        m_isEditMode = true;
        m_gridId = m_gridManager->activeGrid()->id();
        m_nameCombo->setCurrentText(QString::fromStdString(m_gridManager->activeGrid()->name()));
    }
    else
    {
        m_isEditMode = false;
        m_gridId.clear();
        int nextNum = m_gridManager ? static_cast<int>(m_gridManager->grids().size() + 1) : 1;
        m_nameCombo->setCurrentText(tr("Grille %1").arg(nextNum));
    }

    for (int i = 0; i < 3; ++i)
    {
        applyLabels(i);
        if (!m_axes[i].positions.empty())
        {
            m_axes[i].currentPosition = m_axes[i].positions.back() + 3.0;
        }
    }

    updateTableForCurrentTab();
}

GridDialog::GridDialog(const TSA::Grid::GridDefinition& existingDef, QWidget* parent)
    : GridDialog(existingDef, nullptr, nullptr, nullptr, parent)
{
}

GridDialog::GridDialog(const TSA::Grid::GridDefinition& existingDef,
                       TSA::Grid::GridManager* gridManager,
                       TSA::Model::Model* model,
                       OccView* occView,
                       QWidget* parent)
    : QDialog(parent)
    , m_gridManager(gridManager)
    , m_model(model)
    , m_occView(occView)
    , m_isEditMode(true)
    , m_gridId(existingDef.id())
{
    setupUi();
    loadFromDefinition(existingDef);
}

void GridDialog::setupUi()
{
    setWindowTitle(tr("Lignes de construction..."));
    setFixedSize(430, 520);
    const bool isDark = ThemeManager::instance().isDarkMode();
    if (isDark)
    {
        setStyleSheet(
            "QDialog { background-color: #1E2328; font-family: Segoe UI, sans-serif; font-size: 12px; color: #E6EDF3; }"
            "QPushButton { background: #212830; border: 1px solid #30363D; border-radius: 2px; padding: 4px 10px; color: #E6EDF3; font-size: 11px; }"
            "QPushButton:hover { background: #30363D; border-color: #58A6FF; color: #58A6FF; }"
            "QPushButton:pressed { background: #1F3A5A; }"
            "QPushButton:checked { background: #1F3A5A; border-color: #1F6FEB; font-weight: bold; color: #58A6FF; }"
            "QTableWidget { background: #161B22; border: 1px solid #30363D; gridline-color: #30363D; color: #E6EDF3; selection-background-color: #1F3A5A; selection-color: #58A6FF; font-size: 11px; }"
            "QHeaderView::section { background: #212830; border: 1px solid #30363D; padding: 3px; font-weight: 600; font-size: 11px; color: #8B949E; }"
            "QDoubleSpinBox, QSpinBox, QLineEdit, QComboBox { background: #161B22; color: #E6EDF3; border: 1px solid #30363D; border-radius: 2px; padding: 2px 4px; font-size: 11px; }"
            "QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus { border: 1px solid #2EA043; background: #12261A; }"
            "QTabBar::tab { background: #161B22; border: 1px solid #30363D; border-bottom: none; padding: 5px 16px; margin-right: 2px; border-top-left-radius: 2px; border-top-right-radius: 2px; font-weight: 600; color: #8B949E; }"
            "QTabBar::tab:selected { background: #212830; border-bottom: 1px solid #212830; color: #58A6FF; }"
        );
    }
    else
    {
        setStyleSheet(
            "QDialog { background-color: #F0F3F7; font-family: Segoe UI, sans-serif; font-size: 12px; color: #202630; }"
            "QPushButton { background: #FFFFFF; border: 1px solid #B4C0CE; border-radius: 2px; padding: 4px 10px; color: #1E2D3D; font-size: 11px; }"
            "QPushButton:hover { background: #E8F0FA; border-color: #3884D8; }"
            "QPushButton:pressed { background: #D0E2F6; }"
            "QPushButton:checked { background: #D6E7FA; border-color: #2475D0; font-weight: bold; color: #104C90; }"
            "QTableWidget { background: #FFFFFF; border: 1px solid #B4C0CE; gridline-color: #E2E8F0; selection-background-color: #D6E7FA; selection-color: #104C90; font-size: 11px; }"
            "QHeaderView::section { background: #EEF2F6; border: 1px solid #CCD3DC; padding: 3px; font-weight: 600; font-size: 11px; color: #334455; }"
            "QDoubleSpinBox, QSpinBox, QLineEdit, QComboBox { background: #FFFFFF; border: 1px solid #B4C0CE; border-radius: 2px; padding: 2px 4px; font-size: 11px; }"
            "QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus { border: 1px solid #28A745; background: #F4FDF6; }"
            "QTabBar::tab { background: #E4E9F0; border: 1px solid #B4C0CE; border-bottom: none; padding: 5px 16px; margin-right: 2px; border-top-left-radius: 2px; border-top-right-radius: 2px; font-weight: 600; color: #405060; }"
            "QTabBar::tab:selected { background: #FFFFFF; border-bottom: 1px solid #FFFFFF; color: #104C90; }"
        );
    }

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. En-tête : Nom
    auto* nameLayout = new QHBoxLayout();
    auto* lblName = new QLabel(tr("Nom:"), this);
    lblName->setFixedWidth(35);
    nameLayout->addWidget(lblName);

    m_nameCombo = new QComboBox(this);
    m_nameCombo->setEditable(true);
    m_nameCombo->addItem(tr("Lignes de construction"));
    m_nameCombo->addItem(tr("Grille Bâtiment Principale"));
    m_nameCombo->addItem(tr("Grille Secondaire"));
    nameLayout->addWidget(m_nameCombo);
    mainLayout->addLayout(nameLayout);

    // 2. Boutons de mode : Cartésien / Cylindrique / Lignes arbitraires
    auto* modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(4);

    m_btnCartesian = new QPushButton(tr("Cartésien"), this);
    m_btnCartesian->setIcon(QIcon(":/icons/grid_cartesian.svg"));
    m_btnCartesian->setCheckable(true);
    m_btnCartesian->setChecked(true);

    m_btnCylindrical = new QPushButton(tr("Cylindrique"), this);
    m_btnCylindrical->setIcon(QIcon(":/icons/grid_cylindrical.svg"));
    m_btnCylindrical->setCheckable(true);

    m_btnArbitrary = new QPushButton(tr("Lignes arbitraires"), this);
    m_btnArbitrary->setIcon(QIcon(":/icons/geom_polyline.svg"));
    m_btnArbitrary->setCheckable(true);
    // Non implémenté côté moteur (GridType ne connaît que Cartésien/Cylindrique) :
    // on désactive plutôt que de laisser un bouton qui ne fait rien.
    m_btnArbitrary->setEnabled(false);
    m_btnArbitrary->setToolTip(tr("Fonctionnalité pas encore disponible"));

    modeLayout->addWidget(m_btnCartesian);
    modeLayout->addWidget(m_btnCylindrical);
    modeLayout->addWidget(m_btnArbitrary);
    mainLayout->addLayout(modeLayout);

    connect(m_btnCartesian, &QPushButton::clicked, this, &GridDialog::onModeCartesian);
    connect(m_btnCylindrical, &QPushButton::clicked, this, &GridDialog::onModeCylindrical);
    connect(m_btnArbitrary, &QPushButton::clicked, this, &GridDialog::onModeArbitrary);

    // 3. Bouton Paramètres avancés
    m_btnAdvanced = new QPushButton(tr("Paramètres avancés"), this);
    m_btnAdvanced->setIcon(QIcon(":/icons/settings.svg"));
    mainLayout->addWidget(m_btnAdvanced);

    // 4. Sous-onglets d'axes : X / Y / Z
    m_axisTabs = new QTabWidget(this);
    m_axisTabs->addTab(new QWidget(), tr("X"));
    m_axisTabs->addTab(new QWidget(), tr("Y"));
    m_axisTabs->addTab(new QWidget(), tr("Z"));
    mainLayout->addWidget(m_axisTabs);

    connect(m_axisTabs, &QTabWidget::currentChanged, this, &GridDialog::onTabChanged);

    // 5. Zone de saisie rapide (Position / Répéter / Espacement)
    auto* inputGrid = new QGridLayout();
    inputGrid->setContentsMargins(4, 4, 4, 4);
    inputGrid->setHorizontalSpacing(8);
    inputGrid->setVerticalSpacing(4);

    inputGrid->addWidget(m_posLabel = new QLabel(tr("Position:"), this), 0, 0);
    inputGrid->addWidget(m_repeatLabel = new QLabel(tr("Répéter x:"), this), 0, 1);
    inputGrid->addWidget(m_spacingLabel = new QLabel(tr("Espacement:"), this), 0, 2);

    auto* posLayout = new QHBoxLayout();
    m_posSpin = new QDoubleSpinBox(this);
    m_posSpin->setRange(-10000.0, 10000.0);
    m_posSpin->setDecimals(2);
    m_posSpin->setSingleStep(1.0);
    m_posSpin->setValue(0.0);
    m_posSpin->setStyleSheet(isDark
        ? "border: 1.5px solid #2EA043; background-color: #12261A; color: #7EE787; font-weight: bold;"
        : "border: 1.5px solid #28A745; background-color: #E8F8EE; font-weight: bold;");
    posLayout->addWidget(m_posSpin);
    posLayout->addWidget(m_posUnitLabel = new QLabel(tr("(m)"), this));
    inputGrid->addLayout(posLayout, 1, 0);

    m_repeatSpin = new QSpinBox(this);
    m_repeatSpin->setRange(1, 100);
    m_repeatSpin->setValue(2); // 2 par défaut pour éviter tout bug de grille
    inputGrid->addWidget(m_repeatSpin, 1, 1);

    auto* spaceLayout = new QHBoxLayout();
    m_spacingSpin = new QDoubleSpinBox(this);
    m_spacingSpin->setRange(0.01, 1000.0);
    m_spacingSpin->setDecimals(2);
    m_spacingSpin->setSingleStep(1.0);
    m_spacingSpin->setValue(3.0);
    spaceLayout->addWidget(m_spacingSpin);
    spaceLayout->addWidget(m_spacingUnitLabel = new QLabel(tr("(m)"), this));
    inputGrid->addLayout(spaceLayout, 1, 2);

    mainLayout->addLayout(inputGrid);

    // 6. Tableau central + Boutons d'action latéraux (Ajouter, Supprimer, Supprimer tout, Gras)
    auto* centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({ tr("Libellé"), tr("Position") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(0, 80);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    centerLayout->addWidget(m_table, 1);

    auto* sideBtnLayout = new QVBoxLayout();
    sideBtnLayout->setSpacing(6);

    m_btnAdd = new QPushButton(tr("Ajouter"), this);
    m_btnAdd->setIcon(QIcon(":/icons/node_add.svg"));
    m_btnAdd->setStyleSheet(isDark
        ? "QPushButton { border: 1.5px solid #1F6FEB; background: #1F3A5A; font-weight: bold; color: #58A6FF; } QPushButton:hover { background: #234975; }"
        : "QPushButton { border: 1.5px solid #1E70BF; background: #EDF5FC; font-weight: bold; color: #104C90; } QPushButton:hover { background: #D9ECFC; }");
    m_btnAdd->setFixedHeight(28);

    m_btnDelete = new QPushButton(tr("Supprimer"), this);
    m_btnDelete->setIcon(QIcon(":/icons/delete.svg"));
    m_btnClearAll = new QPushButton(tr("Supprimer tout"), this);
    m_btnClearAll->setIcon(QIcon(":/icons/delete.svg"));
    m_btnBold = new QPushButton(tr("Gras"), this);

    sideBtnLayout->addWidget(m_btnAdd);
    sideBtnLayout->addWidget(m_btnDelete);
    sideBtnLayout->addWidget(m_btnClearAll);
    sideBtnLayout->addWidget(m_btnBold);
    sideBtnLayout->addStretch();

    centerLayout->addLayout(sideBtnLayout);
    mainLayout->addLayout(centerLayout);

    connect(m_btnAdd, &QPushButton::clicked, this, &GridDialog::onAddLines);
    connect(m_btnDelete, &QPushButton::clicked, this, &GridDialog::onRemoveLine);
    connect(m_btnClearAll, &QPushButton::clicked, this, &GridDialog::onClearLines);

    // 7. Format de Libellé
    auto* labelFormatLayout = new QHBoxLayout();
    labelFormatLayout->addWidget(new QLabel(tr("Libellé:"), this));

    m_labelStyleCombo = new QComboBox(this);
    m_labelStyleCombo->addItem(tr("1 2 3 ..."));
    m_labelStyleCombo->addItem(tr("A B C ..."));
    m_labelStyleCombo->addItem(tr("Niveau 1 2 3 ..."));
    m_labelStyleCombo->addItem(tr("Défini par l'utilisateur"));
    labelFormatLayout->addWidget(m_labelStyleCombo, 1);

    m_customLabelEdit = new QLineEdit(this);
    m_customLabelEdit->setPlaceholderText(tr("Texte perso..."));
    m_customLabelEdit->setEnabled(false);
    labelFormatLayout->addWidget(m_customLabelEdit, 1);

    mainLayout->addLayout(labelFormatLayout);

    connect(m_labelStyleCombo, &QComboBox::currentIndexChanged, this, &GridDialog::onLabelStyleChanged);

    // 8. Barre d'actions inférieure (Nouveau, Gestionnaire, Appliquer, Fermer, Aide)
    m_chkLiveSync = new QCheckBox(tr("Synchronisation en direct (temps réel)"), this);
    m_chkLiveSync->setChecked(true);
    m_chkLiveSync->setToolTip(tr("Coché : applique immédiatement les modifications dans la vue 3D.\nDécoché : conserve les modifications en mémoire et attend un clic sur 'Appliquer'."));
    m_chkLiveSync->setStyleSheet("font-weight: bold; color: #58A6FF; margin-top: 4px;");
    mainLayout->addWidget(m_chkLiveSync);

    auto* bottomLayout1 = new QHBoxLayout();
    m_btnNew = new QPushButton(tr("Nouveau"), this);
    m_btnNew->setIcon(QIcon(":/icons/file_new.svg"));
    m_btnManage = new QPushButton(tr("Gestionnaire de lignes"), this);
    m_btnManage->setIcon(QIcon(":/icons/settings.svg"));
    bottomLayout1->addWidget(m_btnNew);
    bottomLayout1->addWidget(m_btnManage);
    mainLayout->addLayout(bottomLayout1);

    auto* bottomLayout2 = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Appliquer"), this);
    m_btnApply->setIcon(QIcon(":/icons/apply.svg"));
    m_btnApply->setStyleSheet("QPushButton { border: 1.5px solid #1E70BF; background: #EDF5FC; font-weight: bold; color: #104C90; }");
    m_btnApply->setFixedHeight(26);

    m_btnClose = new QPushButton(tr("Fermer"), this);
    m_btnClose->setIcon(QIcon(":/icons/cancel.svg"));
    m_btnClose->setFixedHeight(26);

    m_btnHelp = new QPushButton(tr("Aide"), this);
    m_btnHelp->setIcon(QIcon(":/icons/TSA.svg"));
    m_btnHelp->setFixedHeight(26);

    bottomLayout2->addWidget(m_btnApply);
    bottomLayout2->addWidget(m_btnClose);
    bottomLayout2->addWidget(m_btnHelp);
    mainLayout->addLayout(bottomLayout2);

    connect(m_chkLiveSync, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) onApply();
    });
    connect(m_btnNew, &QPushButton::clicked, this, &GridDialog::onNewGrid);
    connect(m_btnApply, &QPushButton::clicked, this, &GridDialog::onApply);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnManage, &QPushButton::clicked, this, &GridDialog::manageGridsRequested);
}

void GridDialog::onModeCartesian()
{
    const bool changed = (m_currentType != TSA::Grid::GridType::Cartesian);

    m_btnCartesian->setChecked(true);
    m_btnCylindrical->setChecked(false);
    m_btnArbitrary->setChecked(false);
    m_currentType = TSA::Grid::GridType::Cartesian;

    m_axisTabs->setTabText(0, tr("X"));
    m_axisTabs->setTabText(1, tr("Y"));
    m_axisTabs->setTabText(2, tr("Z"));

    if (changed)
    {
        m_axes[0].positions = { 0.0, 6.0, 12.0, 18.0 };
        m_axes[1].positions = { 0.0, 4.0, 8.0 };
        m_axes[0].spacing = 3.0;
        m_axes[1].spacing = 3.0;
        m_axes[0].repeatCount = 2;
        m_axes[1].repeatCount = 2;
        for (int i = 0; i < 2; ++i)
        {
            applyLabels(i);
            m_axes[i].currentPosition = m_axes[i].positions.back() + 3.0;
        }
    }

    updateTableForCurrentTab();
    if (m_chkLiveSync && m_chkLiveSync->isChecked())
    {
        onApply();
    }
}

void GridDialog::onModeCylindrical()
{
    const bool changed = (m_currentType != TSA::Grid::GridType::Cylindrical);

    m_btnCartesian->setChecked(false);
    m_btnCylindrical->setChecked(true);
    m_btnArbitrary->setChecked(false);
    m_currentType = TSA::Grid::GridType::Cylindrical;

    m_axisTabs->setTabText(0, tr("R (m)"));
    m_axisTabs->setTabText(1, tr("Thêta (°)"));
    m_axisTabs->setTabText(2, tr("Z (m)"));

    if (changed)
    {
        m_axes[0].positions = { 2.0, 4.0, 6.0, 8.0 };
        m_axes[1].positions = { 0.0, 30.0, 60.0, 90.0, 120.0, 150.0, 180.0, 210.0, 240.0, 270.0, 300.0, 330.0 };
        m_axes[0].spacing = 2.0;
        m_axes[1].spacing = 30.0;
        m_axes[0].repeatCount = 2;
        m_axes[1].repeatCount = 2;
        applyLabels(0);
        applyLabels(1);
        m_axes[0].currentPosition = m_axes[0].positions.back() + 2.0;
        m_axes[1].currentPosition = 0.0;
    }

    updateTableForCurrentTab();
    if (m_chkLiveSync && m_chkLiveSync->isChecked())
    {
        onApply();
    }
}

void GridDialog::onModeArbitrary()
{
    m_btnCartesian->setChecked(false);
    m_btnCylindrical->setChecked(false);
    m_btnArbitrary->setChecked(true);
}

void GridDialog::onTabChanged(int index)
{
    if (index < 0 || index >= 3) return;

    // Sauvegarder l'état actuel
    m_axes[m_currentAxisIndex].currentPosition = m_posSpin->value();
    m_axes[m_currentAxisIndex].repeatCount = m_repeatSpin->value();
    m_axes[m_currentAxisIndex].spacing = m_spacingSpin->value();

    m_currentAxisIndex = index;

    // Restaurer l'état du nouvel onglet
    m_isUpdating = true;
    m_posSpin->setValue(m_axes[m_currentAxisIndex].currentPosition);
    m_repeatSpin->setValue(std::max(1, m_axes[m_currentAxisIndex].repeatCount));
    m_spacingSpin->setValue(m_axes[m_currentAxisIndex].spacing);
    m_labelStyleCombo->setCurrentIndex(m_axes[m_currentAxisIndex].labelStyle);
    m_isUpdating = false;

    updateTableForCurrentTab();
}

void GridDialog::onAddLines()
{
    double startPos = m_posSpin->value();
    int repeat = m_repeatSpin->value();
    if (repeat < 1)
    {
        repeat = 1;
        m_repeatSpin->setValue(1);
    }
    double spacing = m_spacingSpin->value();

    auto& axis = m_axes[m_currentAxisIndex];

    for (int i = 0; i < repeat; ++i)
    {
        double p = startPos + i * spacing;

        // Normalisation d'angle pour grille cylindrique (axe Thêta)
        if (m_currentType == TSA::Grid::GridType::Cylindrical && m_currentAxisIndex == 1)
        {
            while (p >= 360.0) p -= 360.0;
            while (p < 0.0) p += 360.0;
        }

        // Éviter les doublons stricts
        bool exists = false;
        for (double existing : axis.positions)
        {
            if (std::abs(existing - p) < 1e-4)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            axis.positions.push_back(p);
        }
    }

    std::sort(axis.positions.begin(), axis.positions.end());
    applyLabels(m_currentAxisIndex);

    // Calculer la prochaine position suggérée
    double nextPos = startPos + repeat * spacing;
    if (m_currentType == TSA::Grid::GridType::Cylindrical && m_currentAxisIndex == 1)
    {
        while (nextPos >= 360.0) nextPos -= 360.0;
    }
    m_posSpin->setValue(nextPos);
    axis.currentPosition = nextPos;

    updateTableForCurrentTab();
    if (m_chkLiveSync && m_chkLiveSync->isChecked())
    {
        onApply();
    }
}

void GridDialog::onRemoveLine()
{
    int row = m_table->currentRow();
    auto& axis = m_axes[m_currentAxisIndex];
    if (row >= 0 && row < static_cast<int>(axis.positions.size()))
    {
        axis.positions.erase(axis.positions.begin() + row);
        applyLabels(m_currentAxisIndex);
        updateTableForCurrentTab();
        if (m_chkLiveSync && m_chkLiveSync->isChecked())
        {
            onApply();
        }
    }
}

void GridDialog::onClearLines()
{
    auto& axis = m_axes[m_currentAxisIndex];
    axis.positions.clear();
    axis.labels.clear();
    axis.currentPosition = 0.0;
    m_posSpin->setValue(0.0);
    updateTableForCurrentTab();
    if (m_chkLiveSync && m_chkLiveSync->isChecked())
    {
        onApply();
    }
}

void GridDialog::onLabelStyleChanged(int index)
{
    if (m_isUpdating) return;

    m_axes[m_currentAxisIndex].labelStyle = index;
    m_customLabelEdit->setEnabled(index == 3);

    applyLabels(m_currentAxisIndex);
    updateTableForCurrentTab();
}

void GridDialog::applyLabels(int tabIdx)
{
    if (tabIdx < 0 || tabIdx >= 3) return;
    auto& axis = m_axes[tabIdx];
    axis.labels.clear();

    for (size_t i = 0; i < axis.positions.size(); ++i)
    {
        std::string lbl;
        if (axis.labelStyle == 0) // 1 2 3...
        {
            lbl = std::to_string(i + 1);
        }
        else if (axis.labelStyle == 1) // A B C...
        {
            char c = static_cast<char>('A' + (i % 26));
            lbl = std::string(1, c);
            if (i >= 26) lbl += std::to_string(i / 26);
        }
        else if (axis.labelStyle == 2) // Niveau 0 1 2...
        {
            lbl = "Niveau " + std::to_string(i);
        }
        else
        {
            lbl = axis.customLabel.empty() ? std::to_string(i + 1) : (axis.customLabel + std::to_string(i + 1));
        }
        axis.labels.push_back(lbl);
    }
}

void GridDialog::updateTableForCurrentTab()
{
    // Mettre à jour les libellés et unités des champs selon le type et l'onglet actif
    QString col1Header;
    if (m_currentType == TSA::Grid::GridType::Cylindrical)
    {
        if (m_currentAxisIndex == 0) // Rayon
        {
            m_posLabel->setText(tr("Rayon (R):"));
            m_posUnitLabel->setText(tr("(m)"));
            m_spacingLabel->setText(tr("Pas radial:"));
            m_spacingUnitLabel->setText(tr("(m)"));
            m_posSpin->setRange(0.0, 10000.0);
            m_posSpin->setSingleStep(1.0);
            m_spacingSpin->setRange(0.01, 1000.0);
            m_spacingSpin->setSingleStep(1.0);
            col1Header = tr("Rayon R (m)");
        }
        else if (m_currentAxisIndex == 1) // Angle Thêta
        {
            m_posLabel->setText(tr("Angle (θ):"));
            m_posUnitLabel->setText(tr("(°)"));
            m_spacingLabel->setText(tr("Pas angulaire:"));
            m_spacingUnitLabel->setText(tr("(°)"));
            m_posSpin->setRange(-360.0, 360.0);
            m_posSpin->setSingleStep(15.0);
            m_spacingSpin->setRange(0.1, 360.0);
            m_spacingSpin->setSingleStep(15.0);
            col1Header = tr("Angle θ (°)");
        }
        else // Élévation Z
        {
            m_posLabel->setText(tr("Position (Z):"));
            m_posUnitLabel->setText(tr("(m)"));
            m_spacingLabel->setText(tr("Espacement:"));
            m_spacingUnitLabel->setText(tr("(m)"));
            m_posSpin->setRange(-10000.0, 10000.0);
            m_posSpin->setSingleStep(1.0);
            m_spacingSpin->setRange(0.01, 1000.0);
            m_spacingSpin->setSingleStep(1.0);
            col1Header = tr("Élévation Z (m)");
        }
    }
    else // Cartésien
    {
        m_posUnitLabel->setText(tr("(m)"));
        m_spacingUnitLabel->setText(tr("(m)"));
        m_posSpin->setRange(-10000.0, 10000.0);
        m_posSpin->setSingleStep(1.0);
        m_spacingSpin->setRange(0.01, 1000.0);
        m_spacingSpin->setSingleStep(1.0);

        if (m_currentAxisIndex == 0)
        {
            m_posLabel->setText(tr("Position (X):"));
            col1Header = tr("Position X (m)");
        }
        else if (m_currentAxisIndex == 1)
        {
            m_posLabel->setText(tr("Position (Y):"));
            col1Header = tr("Position Y (m)");
        }
        else
        {
            m_posLabel->setText(tr("Position (Z):"));
            col1Header = tr("Élévation Z (m)");
        }
    }

    m_table->setHorizontalHeaderLabels({ tr("Libellé"), col1Header });

    m_table->setRowCount(0);
    const auto& axis = m_axes[m_currentAxisIndex];

    m_table->setRowCount(static_cast<int>(axis.positions.size()));
    for (int i = 0; i < static_cast<int>(axis.positions.size()); ++i)
    {
        QString labelStr = (i < static_cast<int>(axis.labels.size())) ? QString::fromStdString(axis.labels[i]) : QString::number(i + 1);
        auto* itemLabel = new QTableWidgetItem(labelStr);
        itemLabel->setTextAlignment(Qt::AlignCenter);

        QString posStr;
        if (m_currentType == TSA::Grid::GridType::Cylindrical && m_currentAxisIndex == 1)
        {
            posStr = QString("%1°").arg(QString::number(axis.positions[i], 'f', 2));
        }
        else
        {
            posStr = QString::number(axis.positions[i], 'f', 2);
        }

        auto* itemPos = new QTableWidgetItem(posStr);
        itemPos->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_table->setItem(i, 0, itemLabel);
        m_table->setItem(i, 1, itemPos);
    }

    if (m_table->rowCount() > 0)
    {
        m_table->selectRow(m_table->rowCount() - 1);
        m_table->scrollToBottom();
    }
}

void GridDialog::onNewGrid()
{
    // Passer en mode création d'une nouvelle grille (sans toucher ni écraser la grille existante)
    m_isEditMode = false;
    m_gridId.clear();

    int nextNum = m_gridManager ? static_cast<int>(m_gridManager->grids().size() + 1) : 1;
    m_nameCombo->setCurrentText(tr("Grille %1").arg(nextNum));

    for (int i = 0; i < 3; ++i)
    {
        m_axes[i].positions.clear();
        m_axes[i].labels.clear();
        m_axes[i].currentPosition = 0.0;
    }
    m_posSpin->setValue(0.0);
    updateTableForCurrentTab();

    // IMPORTANT : Ne PAS appeler onApply() ici !
    // La grille 3D existante dans le viewport reste intacte. La nouvelle grille
    // sera créée et affichée dès que l'utilisateur aura défini des lignes et cliqué sur Appliquer.
}

void GridDialog::onApply()
{
    TSA::Grid::GridDefinition def = getDefinition();

    // Vérifier si la définition contient au minimum des lignes en X et Y (ou R et Thêta)
    bool hasLines = false;
    if (m_currentType == TSA::Grid::GridType::Cartesian)
    {
        hasLines = !m_axes[0].positions.empty() && !m_axes[1].positions.empty();
    }
    else // Cylindrique
    {
        hasLines = !m_axes[0].positions.empty() && !m_axes[1].positions.empty();
    }

    if (!hasLines)
    {
        if (sender() == m_btnApply)
        {
            QMessageBox::warning(this, tr("Grille incomplète"),
                (m_currentType == TSA::Grid::GridType::Cartesian)
                ? tr("Veuillez définir au moins une ligne sur l'axe X et sur l'axe Y.")
                : tr("Veuillez définir au moins un rayon (R) et un angle (θ)."));
        }
        return;
    }

    // Mettre à jour la grille existante ou ajouter une nouvelle grille dans le GridManager
    if (m_gridManager)
    {
        if (m_isEditMode && !m_gridId.empty() && m_gridManager->getGrid(m_gridId))
        {
            m_gridManager->updateGrid(m_gridId, def);
        }
        else
        {
            // Nouvelle grille : ne JAMAIS écraser la grille active existante
            auto* newGrid = m_gridManager->addGrid(def);
            if (newGrid)
            {
                m_gridId = newGrid->id();
                m_isEditMode = true; // Pour que les modifications ultérieures dans cette boîte mettent à jour cette grille
                m_gridManager->setActiveGridId(newGrid->id());
            }
        }
    }

    // Mettre à jour directement le système de coordonnées du modèle UNIQUEMENT si cette grille est la grille active
    if (m_model && m_model->coordinateSystem())
    {
        bool isActive = false;
        if (m_gridManager)
        {
            auto* active = m_gridManager->activeGrid();
            isActive = (active && active->id() == m_gridId);
        }
        else
        {
            isActive = true;
        }

        if (isActive)
        {
            if (m_currentType == TSA::Grid::GridType::Cartesian)
            {
                m_model->coordinateSystem()->setXPositions(m_axes[0].positions);
                m_model->coordinateSystem()->setYPositions(m_axes[1].positions);
            }

            if (m_model->levelManager() && !m_axes[2].positions.empty())
            {
                m_model->levelManager()->setFromElevations(m_axes[2].positions, m_axes[2].labels);
            }
        }
    }

    // Reconstruire immédiatement la vue 3D
    if (m_occView)
    {
        m_occView->rebuildGrid();
    }

    emit gridDefinitionApplied(def);
}

TSA::Grid::GridDefinition GridDialog::getDefinition() const
{
    std::string name = m_nameCombo->currentText().toStdString();
    TSA::Grid::GridDefinition def(name, m_currentType);

    if (m_isEditMode)
    {
        def.setId(m_gridId);
    }

    if (m_currentType == TSA::Grid::GridType::Cartesian)
    {
        def.setXPositions(m_axes[0].positions);
        def.setXLabels(m_axes[0].labels);

        def.setYPositions(m_axes[1].positions);
        def.setYLabels(m_axes[1].labels);
    }
    else // Cylindrique : l'onglet 0 contient les rayons, l'onglet 1 les angles
    {
        def.setRadii(m_axes[0].positions);
        def.setRadiusLabels(m_axes[0].labels);

        def.setAngles(m_axes[1].positions);
        def.setAngleLabels(m_axes[1].labels);
    }

    def.setZLevels(m_axes[2].positions);
    def.setZLabels(m_axes[2].labels);

    return def;
}

void GridDialog::loadFromDefinition(const TSA::Grid::GridDefinition& def)
{
    m_nameCombo->setCurrentText(QString::fromStdString(def.name()));
    m_currentType = def.type();

    if (def.type() == TSA::Grid::GridType::Cartesian)
    {
        onModeCartesian();
    }
    else
    {
        onModeCylindrical();
    }

    if (def.type() == TSA::Grid::GridType::Cartesian)
    {
        m_axes[0].positions = def.xPositions();
        m_axes[0].labels = def.xLabels();

        m_axes[1].positions = def.yPositions();
        m_axes[1].labels = def.yLabels();
    }
    else // Cylindrique : recharger rayons/angles, pas X/Y (toujours vides pour ce type)
    {
        m_axes[0].positions = def.radii();
        m_axes[0].labels = def.radiusLabels();

        m_axes[1].positions = def.angles();
        m_axes[1].labels = def.angleLabels();
    }

    m_axes[2].positions = def.zLevels();
    m_axes[2].labels = def.zLabels();

    for (int i = 0; i < 3; ++i)
    {
        if (m_axes[i].labels.empty())
        {
            applyLabels(i);
        }
        if (!m_axes[i].positions.empty())
        {
            m_axes[i].currentPosition = m_axes[i].positions.back() + 3.0;
        }
    }

    updateTableForCurrentTab();
}

} // namespace TSA::UI
