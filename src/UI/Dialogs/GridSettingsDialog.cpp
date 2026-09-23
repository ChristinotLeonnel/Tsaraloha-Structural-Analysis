#include "GridSettingsDialog.h"
#include "GridDialog.h"
#include "../../Viewer/OccView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>

namespace TSA::UI
{

GridSettingsDialog::GridSettingsDialog(TSA::Grid::GridManager* gridManager,
                                       TSA::Grid::GridSnapManager* snapManager,
                                       OccView* occView,
                                       QWidget* parent)
    : QDialog(parent)
    , m_gridManager(gridManager)
    , m_snapManager(snapManager)
    , m_occView(occView)
{
    setupUi();
    refreshGridList();
}

void GridSettingsDialog::setupUi()
{
    setWindowTitle(tr("Gestionnaire des Grilles 3D & Accrochage"));
    resize(580, 480);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Liste des systèmes de grille
    auto* listGroup = new QGroupBox(tr("Systèmes de Grille du Projet"), this);
    auto* listLayout = new QHBoxLayout(listGroup);

    m_gridList = new QListWidget(listGroup);
    listLayout->addWidget(m_gridList, 1);

    auto* btnCol = new QVBoxLayout();
    m_addBtn = new QPushButton(tr("Ajouter..."), listGroup);
    m_editBtn = new QPushButton(tr("Modifier..."), listGroup);
    m_deleteBtn = new QPushButton(tr("Supprimer"), listGroup);
    m_setActiveBtn = new QPushButton(tr("Définir comme Active"), listGroup);

    btnCol->addWidget(m_addBtn);
    btnCol->addWidget(m_editBtn);
    btnCol->addWidget(m_deleteBtn);
    btnCol->addWidget(m_setActiveBtn);
    btnCol->addStretch();
    listLayout->addLayout(btnCol);

    mainLayout->addWidget(listGroup);

    // 2. Options d'affichage et d'accrochage
    auto* optionsGroup = new QGroupBox(tr("Propriétés de la Grille Sélectionnée & Accrochage"), this);
    auto* optGrid = new QGridLayout(optionsGroup);

    m_visibleCheck = new QCheckBox(tr("Grille Visible"), optionsGroup);
    m_snapCheck = new QCheckBox(tr("Accrochage Magnétique (Snap)"), optionsGroup);
    m_labelsCheck = new QCheckBox(tr("Étiquettes & Bulles d'Axes Visibles"), optionsGroup);
    m_intersectionsCheck = new QCheckBox(tr("Intersections Visibles"), optionsGroup);

    optGrid->addWidget(m_visibleCheck, 0, 0);
    optGrid->addWidget(m_snapCheck, 0, 1);
    optGrid->addWidget(m_labelsCheck, 1, 0);
    optGrid->addWidget(m_intersectionsCheck, 1, 1);

    auto* tolLayout = new QHBoxLayout();
    tolLayout->addWidget(new QLabel(tr("Rayon d'accrochage (m) :"), optionsGroup));
    m_snapToleranceSpin = new QDoubleSpinBox(optionsGroup);
    m_snapToleranceSpin->setRange(0.01, 5.0);
    m_snapToleranceSpin->setSingleStep(0.05);
    m_snapToleranceSpin->setDecimals(2);
    if (m_snapManager)
    {
        m_snapToleranceSpin->setValue(m_snapManager->snapTolerance());
    }
    tolLayout->addWidget(m_snapToleranceSpin);
    tolLayout->addStretch();

    optGrid->addLayout(tolLayout, 2, 0, 1, 2);

    m_infoLabel = new QLabel(optionsGroup);
    m_infoLabel->setStyleSheet("color: #4a90e2; font-style: italic;");
    optGrid->addWidget(m_infoLabel, 3, 0, 1, 2);

    mainLayout->addWidget(optionsGroup);

    // 3. Bouton Fermer
    auto* closeBtnLayout = new QHBoxLayout();
    closeBtnLayout->addStretch();
    auto* closeBtn = new QPushButton(tr("Fermer"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    closeBtnLayout->addWidget(closeBtn);
    mainLayout->addLayout(closeBtnLayout);

    // Connexions
    connect(m_addBtn, &QPushButton::clicked, this, &GridSettingsDialog::onAddGrid);
    connect(m_editBtn, &QPushButton::clicked, this, &GridSettingsDialog::onEditGrid);
    connect(m_deleteBtn, &QPushButton::clicked, this, &GridSettingsDialog::onDeleteGrid);
    connect(m_setActiveBtn, &QPushButton::clicked, this, &GridSettingsDialog::onSetActiveGrid);
    connect(m_gridList, &QListWidget::currentRowChanged, this, &GridSettingsDialog::onSelectedGridChanged);

    connect(m_visibleCheck, &QCheckBox::toggled, this, &GridSettingsDialog::onToggleVisibility);
    connect(m_snapCheck, &QCheckBox::toggled, this, &GridSettingsDialog::onToggleSnap);
    connect(m_labelsCheck, &QCheckBox::toggled, this, &GridSettingsDialog::onToggleLabels);
    connect(m_intersectionsCheck, &QCheckBox::toggled, this, &GridSettingsDialog::onToggleIntersections);
    connect(m_snapToleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GridSettingsDialog::onSnapToleranceChanged);
}

void GridSettingsDialog::refreshGridList()
{
    m_gridList->clear();
    if (!m_gridManager)
        return;

    const auto& grids = m_gridManager->grids();
    int activeRow = -1;

    for (size_t i = 0; i < grids.size(); ++i)
    {
        const auto& g = grids[i];
        QString status = g->isActive() ? tr(" [ACTIVE]") : "";
        QString typeStr = (g->type() == TSA::Grid::GridType::Cartesian) ? tr("Cartésienne") : tr("Cylindrique");
        QString itemText = QString("%1 (%2)%3").arg(QString::fromStdString(g->name())).arg(typeStr).arg(status);

        auto* item = new QListWidgetItem(itemText, m_gridList);
        item->setData(Qt::UserRole, QString::fromStdString(g->id()));

        if (g->isActive())
        {
            activeRow = static_cast<int>(i);
        }
    }

    if (activeRow >= 0)
    {
        m_gridList->setCurrentRow(activeRow);
    }
    else if (m_gridList->count() > 0)
    {
        m_gridList->setCurrentRow(0);
    }

    onSelectedGridChanged();
}

void GridSettingsDialog::onSelectedGridChanged()
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
    {
        m_editBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        m_setActiveBtn->setEnabled(false);
        m_infoLabel->setText("");
        return;
    }

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    auto* grid = m_gridManager->getGrid(id);
    if (!grid)
        return;

    m_editBtn->setEnabled(true);
    m_deleteBtn->setEnabled(m_gridManager->grids().size() > 1);
    m_setActiveBtn->setEnabled(!grid->isActive());

    m_visibleCheck->blockSignals(true);
    m_visibleCheck->setChecked(grid->isVisible());
    m_visibleCheck->blockSignals(false);

    if (m_snapManager)
    {
        m_snapCheck->blockSignals(true);
        m_snapCheck->setChecked(m_snapManager->isSnapEnabled());
        m_snapCheck->blockSignals(false);
    }

    m_labelsCheck->blockSignals(true);
    m_labelsCheck->setChecked(grid->showLabels());
    m_labelsCheck->blockSignals(false);

    m_intersectionsCheck->blockSignals(true);
    m_intersectionsCheck->setChecked(grid->showIntersections());
    m_intersectionsCheck->blockSignals(false);

    QString info;
    if (grid->type() == TSA::Grid::GridType::Cartesian && grid->cartesian())
    {
        info = tr("Cartésienne : %1 axes X, %2 axes Y, %3 niveaux Z (%4 intersections)")
            .arg(grid->definition().xPositions().size())
            .arg(grid->definition().yPositions().size())
            .arg(grid->definition().zLevels().size())
            .arg(grid->cartesian()->intersections().size());
    }
    else if (grid->type() == TSA::Grid::GridType::Cylindrical && grid->cylindrical())
    {
        info = tr("Cylindrique : %1 rayons, %2 angles, %3 niveaux Z (%4 intersections)")
            .arg(grid->definition().radii().size())
            .arg(grid->definition().angles().size())
            .arg(grid->definition().zLevels().size())
            .arg(grid->cylindrical()->intersections().size());
    }
    m_infoLabel->setText(info);
}

void GridSettingsDialog::onAddGrid()
{
    GridDialog dlg(m_gridManager, nullptr, m_occView, this);
    connect(&dlg, &GridDialog::gridDefinitionApplied, this, [this](const TSA::Grid::GridDefinition& /*def*/) {
        refreshGridList();
        if (m_occView)
        {
            m_occView->rebuildGrid();
        }
    });

    if (dlg.exec() == QDialog::Accepted)
    {
        refreshGridList();
        if (m_occView)
        {
            m_occView->rebuildGrid();
        }
    }
}

void GridSettingsDialog::onEditGrid()
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    auto* grid = m_gridManager->getGrid(id);
    if (!grid)
        return;

    GridDialog dlg(grid->definition(), m_gridManager, nullptr, m_occView, this);
    connect(&dlg, &GridDialog::gridDefinitionApplied, this, [this, id](const TSA::Grid::GridDefinition& def) {
        if (m_gridManager)
        {
            m_gridManager->updateGrid(id, def);
            refreshGridList();
        }
        if (m_occView)
        {
            m_occView->rebuildGrid();
        }
    });

    if (dlg.exec() == QDialog::Accepted)
    {
        TSA::Grid::GridDefinition def = dlg.getDefinition();
        m_gridManager->updateGrid(id, def);
        refreshGridList();
        if (m_occView)
        {
            m_occView->rebuildGrid();
        }
    }
}

void GridSettingsDialog::onDeleteGrid()
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    if (m_gridManager->grids().size() <= 1)
    {
        QMessageBox::warning(this, tr("Suppression impossible"), tr("Le projet doit contenir au moins un système de grille."));
        return;
    }

    if (QMessageBox::question(this, tr("Confirmer la suppression"),
                              tr("Voulez-vous vraiment supprimer cette grille ?")) == QMessageBox::Yes)
    {
        m_gridManager->removeGrid(id);
        refreshGridList();
    }
}

void GridSettingsDialog::onSetActiveGrid()
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    m_gridManager->setActiveGridId(id);
    refreshGridList();
}

void GridSettingsDialog::onToggleVisibility(bool checked)
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    m_gridManager->setGridVisible(id, checked);
}

void GridSettingsDialog::onToggleSnap(bool checked)
{
    if (m_snapManager)
    {
        m_snapManager->setSnapEnabled(checked);
    }
}

void GridSettingsDialog::onToggleLabels(bool checked)
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    auto* grid = m_gridManager->getGrid(id);
    if (grid)
    {
        grid->setShowLabels(checked);
        m_gridManager->updateGrid(id, grid->definition());
    }
}

void GridSettingsDialog::onToggleIntersections(bool checked)
{
    auto* item = m_gridList->currentItem();
    if (!item || !m_gridManager)
        return;

    std::string id = item->data(Qt::UserRole).toString().toStdString();
    auto* grid = m_gridManager->getGrid(id);
    if (grid)
    {
        grid->setShowIntersections(checked);
        m_gridManager->updateGrid(id, grid->definition());
    }
}

void GridSettingsDialog::onSnapToleranceChanged(double val)
{
    if (m_snapManager)
    {
        m_snapManager->setSnapTolerance(val);
    }
}

} // namespace TSA::UI
