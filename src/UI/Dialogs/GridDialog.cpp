#include "GridDialog.h"
#include "../../Grid/GridManager.h"
#include "../../Model/Model.h"

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

namespace TSA::UI
{

GridDialog::GridDialog(QWidget* parent)
    : GridDialog(nullptr, nullptr, parent)
{
}

GridDialog::GridDialog(TSA::Grid::GridManager* gridManager,
                       TSA::Model::Model* model,
                       QWidget* parent)
    : QDialog(parent)
    , m_gridManager(gridManager)
    , m_model(model)
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
    : GridDialog(existingDef, nullptr, nullptr, parent)
{
}

GridDialog::GridDialog(const TSA::Grid::GridDefinition& existingDef,
                       TSA::Grid::GridManager* gridManager,
                       TSA::Model::Model* model,
                       QWidget* parent)
    : QDialog(parent)
    , m_gridManager(gridManager)
    , m_model(model)
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
    m_btnCartesian->setCheckable(true);
    m_btnCartesian->setChecked(true);

    m_btnCylindrical = new QPushButton(tr("Cylindrique"), this);
    m_btnCylindrical->setCheckable(true);

    m_btnArbitrary = new QPushButton(tr("Lignes arbitraires"), this);
    m_btnArbitrary->setCheckable(true);

    modeLayout->addWidget(m_btnCartesian);
    modeLayout->addWidget(m_btnCylindrical);
    modeLayout->addWidget(m_btnArbitrary);
    mainLayout->addLayout(modeLayout);

    connect(m_btnCartesian, &QPushButton::clicked, this, &GridDialog::onModeCartesian);
    connect(m_btnCylindrical, &QPushButton::clicked, this, &GridDialog::onModeCylindrical);
    connect(m_btnArbitrary, &QPushButton::clicked, this, &GridDialog::onModeArbitrary);

    // 3. Bouton Paramètres avancés
    m_btnAdvanced = new QPushButton(tr("Paramètres avancés"), this);
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

    inputGrid->addWidget(new QLabel(tr("Position:"), this), 0, 0);
    inputGrid->addWidget(new QLabel(tr("Répéter x:"), this), 0, 1);
    inputGrid->addWidget(new QLabel(tr("Espacement:"), this), 0, 2);

    auto* posLayout = new QHBoxLayout();
    m_posSpin = new QDoubleSpinBox(this);
    m_posSpin->setRange(-10000.0, 10000.0);
    m_posSpin->setDecimals(2);
    m_posSpin->setSingleStep(1.0);
    m_posSpin->setValue(0.0);
    m_posSpin->setStyleSheet("border: 1.5px solid #28A745; background-color: #E8F8EE; font-weight: bold;");
    posLayout->addWidget(m_posSpin);
    posLayout->addWidget(new QLabel(tr("(m)"), this));
    inputGrid->addLayout(posLayout, 1, 0);

    m_repeatSpin = new QSpinBox(this);
    m_repeatSpin->setRange(1, 100);
    m_repeatSpin->setValue(1);
    inputGrid->addWidget(m_repeatSpin, 1, 1);

    auto* spaceLayout = new QHBoxLayout();
    m_spacingSpin = new QDoubleSpinBox(this);
    m_spacingSpin->setRange(0.01, 1000.0);
    m_spacingSpin->setDecimals(2);
    m_spacingSpin->setSingleStep(1.0);
    m_spacingSpin->setValue(3.0);
    spaceLayout->addWidget(m_spacingSpin);
    spaceLayout->addWidget(new QLabel(tr("(m)"), this));
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
    m_btnAdd->setStyleSheet("QPushButton { border: 1.5px solid #1E70BF; background: #EDF5FC; font-weight: bold; color: #104C90; } QPushButton:hover { background: #D9ECFC; }");
    m_btnAdd->setFixedHeight(28);

    m_btnDelete = new QPushButton(tr("Supprimer"), this);
    m_btnClearAll = new QPushButton(tr("Supprimer tout"), this);
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
    auto* bottomLayout1 = new QHBoxLayout();
    m_btnNew = new QPushButton(tr("Nouveau"), this);
    m_btnManage = new QPushButton(tr("Gestionnaire de lignes"), this);
    bottomLayout1->addWidget(m_btnNew);
    bottomLayout1->addWidget(m_btnManage);
    mainLayout->addLayout(bottomLayout1);

    auto* bottomLayout2 = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Appliquer"), this);
    m_btnApply->setStyleSheet("QPushButton { border: 1.5px solid #1E70BF; background: #EDF5FC; font-weight: bold; color: #104C90; }");
    m_btnApply->setFixedHeight(26);

    m_btnClose = new QPushButton(tr("Fermer"), this);
    m_btnClose->setFixedHeight(26);

    m_btnHelp = new QPushButton(tr("Aide"), this);
    m_btnHelp->setFixedHeight(26);

    bottomLayout2->addWidget(m_btnApply);
    bottomLayout2->addWidget(m_btnClose);
    bottomLayout2->addWidget(m_btnHelp);
    mainLayout->addLayout(bottomLayout2);

    connect(m_btnNew, &QPushButton::clicked, this, &GridDialog::onNewGrid);
    connect(m_btnApply, &QPushButton::clicked, this, &GridDialog::onApply);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void GridDialog::onModeCartesian()
{
    m_btnCartesian->setChecked(true);
    m_btnCylindrical->setChecked(false);
    m_btnArbitrary->setChecked(false);
    m_currentType = TSA::Grid::GridType::Cartesian;

    m_axisTabs->setTabText(0, tr("X"));
    m_axisTabs->setTabText(1, tr("Y"));
    m_axisTabs->setTabText(2, tr("Z"));
}

void GridDialog::onModeCylindrical()
{
    m_btnCartesian->setChecked(false);
    m_btnCylindrical->setChecked(true);
    m_btnArbitrary->setChecked(false);
    m_currentType = TSA::Grid::GridType::Cylindrical;

    m_axisTabs->setTabText(0, tr("R (m)"));
    m_axisTabs->setTabText(1, tr("Thêta (°)"));
    m_axisTabs->setTabText(2, tr("Z (m)"));
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
    m_repeatSpin->setValue(m_axes[m_currentAxisIndex].repeatCount);
    m_spacingSpin->setValue(m_axes[m_currentAxisIndex].spacing);
    m_labelStyleCombo->setCurrentIndex(m_axes[m_currentAxisIndex].labelStyle);
    m_isUpdating = false;

    updateTableForCurrentTab();
}

void GridDialog::onAddLines()
{
    double startPos = m_posSpin->value();
    int repeat = m_repeatSpin->value();
    double spacing = m_spacingSpin->value();

    auto& axis = m_axes[m_currentAxisIndex];

    for (int i = 0; i < repeat; ++i)
    {
        double p = startPos + i * spacing;
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
    m_posSpin->setValue(nextPos);
    axis.currentPosition = nextPos;

    updateTableForCurrentTab();

    // Mettre à jour en direct dans le 3D viewport
    onApply();
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
        onApply();
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
    onApply();
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
    m_table->setRowCount(0);
    const auto& axis = m_axes[m_currentAxisIndex];

    m_table->setRowCount(static_cast<int>(axis.positions.size()));
    for (int i = 0; i < static_cast<int>(axis.positions.size()); ++i)
    {
        QString labelStr = (i < static_cast<int>(axis.labels.size())) ? QString::fromStdString(axis.labels[i]) : QString::number(i + 1);
        auto* itemLabel = new QTableWidgetItem(labelStr);
        itemLabel->setTextAlignment(Qt::AlignCenter);

        auto* itemPos = new QTableWidgetItem(QString::number(axis.positions[i], 'f', 2));
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
    for (int i = 0; i < 3; ++i)
    {
        m_axes[i].positions.clear();
        m_axes[i].labels.clear();
        m_axes[i].currentPosition = 0.0;
    }
    m_posSpin->setValue(0.0);
    updateTableForCurrentTab();
    onApply();
}

void GridDialog::onApply()
{
    TSA::Grid::GridDefinition def = getDefinition();

    // Mettre à jour directement le système de coordonnées du modèle
    if (m_model && m_model->coordinateSystem())
    {
        m_model->coordinateSystem()->setXPositions(m_axes[0].positions);
        m_model->coordinateSystem()->setYPositions(m_axes[1].positions);

        if (m_model->levelManager())
        {
            m_model->levelManager()->setFromElevations(m_axes[2].positions, m_axes[2].labels);
        }
    }

    // Mettre à jour la grille active dans le GridManager
    if (m_gridManager)
    {
        if (auto* active = m_gridManager->activeGrid())
        {
            active->updateDefinition(def);
        }
        else
        {
            auto* newGrid = m_gridManager->addGrid(def);
            if (newGrid)
            {
                m_gridManager->setActiveGridId(newGrid->id());
            }
        }
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

    def.setXPositions(m_axes[0].positions);
    def.setXLabels(m_axes[0].labels);

    def.setYPositions(m_axes[1].positions);
    def.setYLabels(m_axes[1].labels);

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

    m_axes[0].positions = def.xPositions();
    m_axes[0].labels = def.xLabels();

    m_axes[1].positions = def.yPositions();
    m_axes[1].labels = def.yLabels();

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
