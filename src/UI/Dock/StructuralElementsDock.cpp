#include "StructuralElementsDock.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QIcon>

namespace TSA::UI
{

StructuralElementsDock::StructuralElementsDock(QWidget* parent)
    : QDockWidget(tr("ÉLÉMENTS STRUCTURAUX"), parent)
{
    setObjectName("StructuralElementsDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUi();
}

void StructuralElementsDock::setupUi()
{
    auto* container = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 1. Barre de recherche rapide
    m_searchEdit = new QLineEdit(container);
    m_searchEdit->setPlaceholderText(tr("Rechercher un élément (Câble, Poutre, Dalle...)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &StructuralElementsDock::onFilterChanged);
    mainLayout->addWidget(m_searchEdit);

    // 2. Boutons de raccourci rapide (Quick buttons)
    auto* quickBtnLayout = new QGridLayout();
    quickBtnLayout->setSpacing(4);

    m_btnQuickBeam = new QPushButton(QIcon(":/icons/draw_beam.svg"), tr("Poutre"), container);
    m_btnQuickBeam->setToolTip(tr("Mode dessin : Poutre (B)"));
    connect(m_btnQuickBeam, &QPushButton::clicked, this, &StructuralElementsDock::drawBeamTriggered);
    quickBtnLayout->addWidget(m_btnQuickBeam, 0, 0);

    m_btnQuickColumn = new QPushButton(QIcon(":/icons/draw_column.svg"), tr("Poteau"), container);
    m_btnQuickColumn->setToolTip(tr("Mode dessin : Poteau (C)"));
    connect(m_btnQuickColumn, &QPushButton::clicked, this, &StructuralElementsDock::drawColumnTriggered);
    quickBtnLayout->addWidget(m_btnQuickColumn, 0, 1);

    m_btnQuickCable = new QPushButton(QIcon(":/icons/draw_cable.svg"), tr("Cable"), container);
    m_btnQuickCable->setToolTip(tr("Mode dessin : Cable (C) - Élément filaire tendu"));
    m_btnQuickCable->setStyleSheet("QPushButton { font-weight: bold; border: 1px solid #EA580C; background: rgba(234, 88, 12, 0.12); border-radius: 4px; padding: 4px; }"
                                   "QPushButton:hover { background: rgba(234, 88, 12, 0.25); border-color: #FB923C; }");
    connect(m_btnQuickCable, &QPushButton::clicked, this, &StructuralElementsDock::drawCableTriggered);
    quickBtnLayout->addWidget(m_btnQuickCable, 0, 2);

    m_btnQuickSlab = new QPushButton(QIcon(":/icons/draw_slab.svg"), tr("Dalle"), container);
    m_btnQuickSlab->setToolTip(tr("Mode dessin : Dalle (L)"));
    connect(m_btnQuickSlab, &QPushButton::clicked, this, &StructuralElementsDock::drawSlabTriggered);
    quickBtnLayout->addWidget(m_btnQuickSlab, 1, 0);

    m_btnQuickWall = new QPushButton(QIcon(":/icons/draw_wall.svg"), tr("Voile"), container);
    m_btnQuickWall->setToolTip(tr("Mode dessin : Voile (W)"));
    connect(m_btnQuickWall, &QPushButton::clicked, this, &StructuralElementsDock::drawWallTriggered);
    quickBtnLayout->addWidget(m_btnQuickWall, 1, 1);

    auto* btnQuickBar = new QPushButton(QIcon(":/icons/draw_bar.svg"), tr("Barre"), container);
    btnQuickBar->setToolTip(tr("Mode dessin : Barre filaire"));
    connect(btnQuickBar, &QPushButton::clicked, this, &StructuralElementsDock::drawBarTriggered);
    quickBtnLayout->addWidget(btnQuickBar, 1, 2);

    mainLayout->addLayout(quickBtnLayout);

    // 3. Arborescence conceptuelle de navigation
    m_tree = new QTreeWidget(container);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(18);
    m_tree->setIconSize(QSize(18, 18));
    m_tree->setAlternatingRowColors(true);
    m_tree->setStyleSheet("QTreeWidget { border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 4px; }"
                          "QTreeWidget::item { padding: 4px 2px; }"
                          "QTreeWidget::item:hover { background: rgba(56, 189, 248, 0.15); }"
                          "QTreeWidget::item:selected { background: rgba(56, 189, 248, 0.3); font-weight: bold; }");

    connect(m_tree, &QTreeWidget::itemClicked, this, &StructuralElementsDock::onItemClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &StructuralElementsDock::onItemDoubleClicked);

    buildElementTree();
    mainLayout->addWidget(m_tree, 1);

    setWidget(container);
}

void StructuralElementsDock::buildElementTree()
{
    m_tree->clear();

    // Racine : Éléments structuraux
    m_rootItem = new QTreeWidgetItem(m_tree, { tr("Éléments structuraux") });
    m_rootItem->setExpanded(true);
    QFont f = m_rootItem->font(0);
    f.setBold(true);
    m_rootItem->setFont(0, f);

    // 1. Éléments filaires (1D)
    m_linearCategory = new QTreeWidgetItem(m_rootItem, { tr("Éléments filaires (1D)") });
    m_linearCategory->setExpanded(true);

    auto* itemBeam = new QTreeWidgetItem(m_linearCategory, { tr("Poutre") });
    itemBeam->setIcon(0, QIcon(":/icons/draw_beam.svg"));
    itemBeam->setData(0, Qt::UserRole, "Beam");

    auto* itemColumn = new QTreeWidgetItem(m_linearCategory, { tr("Poteau") });
    itemColumn->setIcon(0, QIcon(":/icons/draw_column.svg"));
    itemColumn->setData(0, Qt::UserRole, "Column");

    auto* itemBar = new QTreeWidgetItem(m_linearCategory, { tr("Barre") });
    itemBar->setIcon(0, QIcon(":/icons/draw_bar.svg"));
    itemBar->setData(0, Qt::UserRole, "Bar");

    auto* itemCable = new QTreeWidgetItem(m_linearCategory, { tr("Cable") });
    itemCable->setIcon(0, QIcon(":/icons/draw_cable.svg"));
    itemCable->setData(0, Qt::UserRole, "Cable");
    itemCable->setToolTip(0, tr("Câble structural - Élément tendu (mode dessin direct)"));
    QFont fCable = itemCable->font(0);
    fCable.setBold(true);
    itemCable->setFont(0, fCable);
    itemCable->setForeground(0, QColor("#FB923C"));

    auto* itemTruss = new QTreeWidgetItem(m_linearCategory, { tr("Treillis") });
    itemTruss->setIcon(0, QIcon(":/icons/struct_truss.svg"));
    itemTruss->setData(0, Qt::UserRole, "Truss");

    // 2. Éléments surfaciques (2D)
    m_surfaceCategory = new QTreeWidgetItem(m_rootItem, { tr("Éléments surfaciques (2D)") });
    m_surfaceCategory->setExpanded(true);

    auto* itemSlab = new QTreeWidgetItem(m_surfaceCategory, { tr("Dalle") });
    itemSlab->setIcon(0, QIcon(":/icons/draw_slab.svg"));
    itemSlab->setData(0, Qt::UserRole, "Slab");

    auto* itemWall = new QTreeWidgetItem(m_surfaceCategory, { tr("Voile") });
    itemWall->setIcon(0, QIcon(":/icons/draw_wall.svg"));
    itemWall->setData(0, Qt::UserRole, "Wall");

    auto* itemPanel = new QTreeWidgetItem(m_surfaceCategory, { tr("Panneau") });
    itemPanel->setIcon(0, QIcon(":/icons/draw_slab.svg"));
    itemPanel->setData(0, Qt::UserRole, "Panel");

    // 3. Fondations & Appuis
    m_foundationCategory = new QTreeWidgetItem(m_rootItem, { tr("Fondations & Appuis") });
    m_foundationCategory->setExpanded(true);

    auto* itemFooting = new QTreeWidgetItem(m_foundationCategory, { tr("Semelle / Fondation") });
    itemFooting->setIcon(0, QIcon(":/icons/struct_foundation.svg"));
    itemFooting->setData(0, Qt::UserRole, "Footing");
}

void StructuralElementsDock::triggerActionForRole(const QString& role)
{
    if (role == "Beam") emit drawBeamTriggered();
    else if (role == "Column") emit drawColumnTriggered();
    else if (role == "Bar") emit drawBarTriggered();
    else if (role == "Cable") emit drawCableTriggered();
    else if (role == "Truss") emit drawTrussTriggered();
    else if (role == "Slab") emit drawSlabTriggered();
    else if (role == "Wall") emit drawWallTriggered();
    else if (role == "Panel") emit drawPanelTriggered();
    else if (role == "Footing") emit drawFootingTriggered();
}

void StructuralElementsDock::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QString role = item->data(0, Qt::UserRole).toString();
    if (!role.isEmpty())
    {
        triggerActionForRole(role);
    }
}

void StructuralElementsDock::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QString role = item->data(0, Qt::UserRole).toString();
    if (!role.isEmpty())
    {
        triggerActionForRole(role);
    }
}

void StructuralElementsDock::onFilterChanged(const QString& filter)
{
    QString query = filter.trimmed();
    if (query.isEmpty())
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        {
            auto* top = m_tree->topLevelItem(i);
            top->setHidden(false);
            for (int j = 0; j < top->childCount(); ++j)
            {
                auto* cat = top->child(j);
                cat->setHidden(false);
                for (int k = 0; k < cat->childCount(); ++k)
                {
                    cat->child(k)->setHidden(false);
                }
            }
        }
        return;
    }

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        auto* top = m_tree->topLevelItem(i);
        bool anyTopMatch = false;
        for (int j = 0; j < top->childCount(); ++j)
        {
            auto* cat = top->child(j);
            bool anyCatMatch = false;
            for (int k = 0; k < cat->childCount(); ++k)
            {
                auto* leaf = cat->child(k);
                bool match = leaf->text(0).contains(query, Qt::CaseInsensitive);
                leaf->setHidden(!match);
                if (match) anyCatMatch = true;
            }
            cat->setHidden(!anyCatMatch);
            if (anyCatMatch) anyTopMatch = true;
        }
        top->setHidden(!anyTopMatch);
    }
}

} // namespace TSA::UI
