#include "LevelDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QCheckBox>

namespace TSA::UI
{

LevelDialog::LevelDialog(TSA::Coordinate::LevelManager* levelManager, QWidget* parent)
    : QDialog(parent)
    , m_levelManager(levelManager)
{
    setWindowTitle(tr("Level & Story Manager"));
    resize(640, 420);

    setupUi();
    refreshTable();

    if (m_levelManager)
    {
        connect(m_levelManager, &TSA::Coordinate::LevelManager::levelsChanged, this, &LevelDialog::refreshTable);
    }
}

void LevelDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* headerLabel = new QLabel(tr("Manage project stories, elevations and vertical levels:"), this);
    mainLayout->addWidget(headerLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({ tr("ID"), tr("Name"), tr("Elevation Z (m)"), tr("Spacing \xCE\x94Z (m)"), tr("Visible") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);

    mainLayout->addWidget(m_table);

    // Boutons d'action
    auto* btnLayout = new QHBoxLayout();

    m_btnAdd = new QPushButton(tr("Ajouter un niveau"), this);
    m_btnAdd->setIcon(QIcon(":/icons/node_add.svg"));
    m_btnRemove = new QPushButton(tr("Supprimer le niveau"), this);
    m_btnRemove->setIcon(QIcon(":/icons/delete.svg"));
    m_btnGenerate = new QPushButton(tr("Générer des étages..."), this);
    m_btnGenerate->setIcon(QIcon(":/icons/geom_cube.svg"));

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnRemove);
    btnLayout->addWidget(m_btnGenerate);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    m_chkLiveSync = new QCheckBox(tr("Synchronisation en direct (temps réel)"), this);
    m_chkLiveSync->setChecked(true);
    m_chkLiveSync->setToolTip(tr("Coché : applique immédiatement les modifications de niveaux dans la vue 3D.\nDécoché : attend un clic sur 'Appliquer'."));
    m_chkLiveSync->setStyleSheet("font-weight: bold; color: #58A6FF; margin-top: 4px;");
    mainLayout->addWidget(m_chkLiveSync);

    auto* bottomLayout = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Appliquer"), this);
    m_btnApply->setIcon(QIcon(":/icons/apply.svg"));
    m_btnApply->setStyleSheet("QPushButton { border: 1.5px solid #1E70BF; background: #EDF5FC; font-weight: bold; color: #104C90; }");
    m_btnApply->setFixedHeight(26);

    auto* closeBtn = new QPushButton(tr("Fermer"), this);
    closeBtn->setIcon(QIcon(":/icons/cancel.svg"));
    closeBtn->setFixedHeight(26);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottomLayout->addStretch();
    bottomLayout->addWidget(m_btnApply);
    bottomLayout->addWidget(closeBtn);
    mainLayout->addLayout(bottomLayout);

    connect(m_chkLiveSync, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) onApply();
    });
    connect(m_btnApply, &QPushButton::clicked, this, &LevelDialog::onApply);
    connect(m_btnAdd, &QPushButton::clicked, this, &LevelDialog::onAddLevel);
    connect(m_btnRemove, &QPushButton::clicked, this, &LevelDialog::onRemoveSelectedLevel);
    connect(m_btnGenerate, &QPushButton::clicked, this, &LevelDialog::onGenerateStories);
    connect(m_table, &QTableWidget::cellChanged, this, &LevelDialog::onTableCellChanged);
}

void LevelDialog::refreshTable()
{
    if (!m_levelManager)
        return;

    m_isUpdatingTable = true;
    m_table->setRowCount(0);

    const auto& levels = m_levelManager->levels();
    m_table->setRowCount(static_cast<int>(levels.size()));

    for (int i = 0; i < static_cast<int>(levels.size()); ++i)
    {
        const auto& lvl = levels[i];

        // Col 0: ID
        auto* idItem = new QTableWidgetItem(QString::fromStdString(lvl.id));
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, idItem);

        // Col 1: Name
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(lvl.name));
        m_table->setItem(i, 1, nameItem);

        // Col 2: Elevation
        auto* elevItem = new QTableWidgetItem(QString::number(lvl.elevation, 'f', 3));
        m_table->setItem(i, 2, elevItem);

        // Col 3: Spacing Delta Z
        double spacing = (i == 0) ? 0.0 : (lvl.elevation - levels[i - 1].elevation);
        auto* spacingItem = new QTableWidgetItem(QString::number(spacing, 'f', 3));
        spacingItem->setFlags(spacingItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 3, spacingItem);

        // Col 4: Visible
        auto* visibleItem = new QTableWidgetItem();
        visibleItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        visibleItem->setCheckState(lvl.visible ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(i, 4, visibleItem);
    }

    m_isUpdatingTable = false;
}

void LevelDialog::onAddLevel()
{
    if (!m_levelManager)
        return;

    double defaultZ = 0.0;
    if (!m_levelManager->isEmpty())
    {
        defaultZ = m_levelManager->levels().back().elevation + 3.0;
    }

    bool ok = false;
    double newZ = QInputDialog::getDouble(this, tr("New Level"),
                                          tr("Level Elevation Z (m):"),
                                          defaultZ, -1000.0, 10000.0, 3, &ok);
    if (ok)
    {
        m_levelManager->addLevel("", newZ);
    }
}

void LevelDialog::onRemoveSelectedLevel()
{
    if (!m_levelManager)
        return;

    int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= static_cast<int>(m_levelManager->levelCount()))
    {
        QMessageBox::information(this, tr("Selection"), tr("Please select a level to delete."));
        return;
    }

    std::string id = m_table->item(currentRow, 0)->text().toStdString();
    auto confirm = QMessageBox::question(this, tr("Confirm Deletion"),
                                         tr("Delete level '%1'?").arg(QString::fromStdString(id)),
                                         QMessageBox::Yes | QMessageBox::No);
    if (confirm == QMessageBox::Yes)
    {
        m_levelManager->removeLevel(id);
    }
}

void LevelDialog::onGenerateStories()
{
    if (!m_levelManager)
        return;

    bool ok = false;
    int numStories = QInputDialog::getInt(this, tr("Generate Regular Stories"),
                                         tr("Number of stories:"), 3, 1, 100, 1, &ok);
    if (!ok) return;

    double storyHeight = QInputDialog::getDouble(this, tr("Story Height"),
                                                tr("Height per story (m):"), 3.0, 0.5, 50.0, 2, &ok);
    if (!ok) return;

    std::vector<double> spacings(numStories, storyHeight);
    m_levelManager->setFromSpacings(0.0, spacings);
}

void LevelDialog::onTableCellChanged(int row, int column)
{
    if (m_isUpdatingTable || !m_levelManager)
        return;

    if (row < 0 || row >= static_cast<int>(m_levelManager->levelCount()))
        return;

    if (m_chkLiveSync && m_chkLiveSync->isChecked())
    {
        std::string id = m_table->item(row, 0)->text().toStdString();

        if (column == 1) // Name
        {
            QString newName = m_table->item(row, column)->text();
            m_levelManager->setLevelName(id, newName.toStdString());
        }
        else if (column == 2) // Elevation
        {
            bool ok = false;
            double newElev = m_table->item(row, column)->text().toDouble(&ok);
            if (ok)
            {
                m_levelManager->setLevelElevation(id, newElev);
            }
        }
        else if (column == 4) // Visible
        {
            bool isVisible = (m_table->item(row, column)->checkState() == Qt::Checked);
            m_levelManager->setLevelVisible(id, isVisible);
        }
    }
}

void LevelDialog::onApply()
{
    if (!m_levelManager)
        return;

    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        auto* idItem = m_table->item(r, 0);
        auto* nameItem = m_table->item(r, 1);
        auto* elevItem = m_table->item(r, 2);
        auto* visItem = m_table->item(r, 4);
        if (!idItem || !nameItem || !elevItem)
            continue;

        std::string id = idItem->text().toStdString();
        m_levelManager->setLevelName(id, nameItem->text().toStdString());

        bool ok = false;
        double newElev = elevItem->text().toDouble(&ok);
        if (ok)
        {
            m_levelManager->setLevelElevation(id, newElev);
        }

        if (visItem)
        {
            m_levelManager->setLevelVisible(id, visItem->checkState() == Qt::Checked);
        }
    }
}

} // namespace TSA::UI
