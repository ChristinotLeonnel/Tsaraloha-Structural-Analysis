#include "NodeSelectionDialog.h"
#include "../../Model/Model.h"
#include "../../Model/Node.h"
#include "../../Viewer/OccView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

namespace TSA::UI
{

NodeSelectionDialog::NodeSelectionDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_occView(occView)
{
    setWindowTitle(tr("Sélection d'un Nœud"));
    setMinimumSize(640, 420);
    resize(700, 480);
    setupUi();
    populateTable();
}

void NodeSelectionDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Barre supérieure : Filtre / Recherche
    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    auto* lblSearch = new QLabel(tr("Rechercher :"), this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filtrer par numéro (ex: 25 ou N25) ou par nom..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &NodeSelectionDialog::onFilterTextChanged);

    topLayout->addWidget(lblSearch);
    topLayout->addWidget(m_filterEdit, 1);
    mainLayout->addLayout(topLayout);

    // Table des nœuds
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(7);
    QStringList headers = {
        tr("ID"),
        tr("Nom"),
        tr("X (m)"),
        tr("Y (m)"),
        tr("Z (m)"),
        tr("Appui"),
        tr("Étage")
    };
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSortingEnabled(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &NodeSelectionDialog::onTableRowDoubleClicked);
    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &NodeSelectionDialog::onTableSelectionChanged);

    mainLayout->addWidget(m_tableWidget, 1);

    // Barre inférieure : Statut et Boutons
    auto* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");

    bottomLayout->addWidget(m_statusLabel, 1);

    m_btnSelect = new QPushButton(tr("Sélectionner"), this);
    m_btnSelect->setDefault(true);
    m_btnSelect->setEnabled(false);
    connect(m_btnSelect, &QPushButton::clicked, this, &NodeSelectionDialog::onAcceptClicked);

    m_btnCancel = new QPushButton(tr("Annuler"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    bottomLayout->addWidget(m_btnSelect);
    bottomLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(bottomLayout);
}

void NodeSelectionDialog::populateTable()
{
    m_tableWidget->setSortingEnabled(false);
    m_tableWidget->setRowCount(0);

    if (!m_model)
    {
        m_statusLabel->setText(tr("Aucun modèle associé."));
        return;
    }

    const auto& nodesMap = m_model->nodes();
    m_tableWidget->setRowCount(static_cast<int>(nodesMap.size()));

    int row = 0;
    for (const auto& [nodeId, node] : nodesMap)
    {
        // Colonne 0 : ID numérique pour tri correct
        auto* itemId = new QTableWidgetItem();
        itemId->setData(Qt::DisplayRole, nodeId);
        itemId->setText(QString("N%1").arg(nodeId));
        itemId->setData(Qt::UserRole, nodeId);
        m_tableWidget->setItem(row, 0, itemId);

        // Colonne 1 : Nom
        QString name = QString::fromStdString(node.name());
        if (name.isEmpty())
        {
            name = QString::fromStdString(node.formattedName());
        }
        auto* itemName = new QTableWidgetItem(name);
        m_tableWidget->setItem(row, 1, itemName);

        // Colonne 2 : X
        auto* itemX = new QTableWidgetItem();
        itemX->setData(Qt::DisplayRole, node.x());
        itemX->setText(QString::number(node.x(), 'f', 3));
        itemX->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(row, 2, itemX);

        // Colonne 3 : Y
        auto* itemY = new QTableWidgetItem();
        itemY->setData(Qt::DisplayRole, node.y());
        itemY->setText(QString::number(node.y(), 'f', 3));
        itemY->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(row, 3, itemY);

        // Colonne 4 : Z
        auto* itemZ = new QTableWidgetItem();
        itemZ->setData(Qt::DisplayRole, node.z());
        itemZ->setText(QString::number(node.z(), 'f', 3));
        itemZ->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(row, 4, itemZ);

        // Colonne 5 : Support
        QString suppStr = tr("Libre");
        switch (node.supportType())
        {
        case TSA::Model::SupportType::Fixed:
            suppStr = tr("Encastrement");
            break;
        case TSA::Model::SupportType::Pinned:
            suppStr = tr("Articulation");
            break;
        case TSA::Model::SupportType::Roller:
            suppStr = tr("Simple");
            break;
        case TSA::Model::SupportType::Free:
        default:
            suppStr = tr("Libre");
            break;
        }
        auto* itemSupp = new QTableWidgetItem(suppStr);
        itemSupp->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(row, 5, itemSupp);

        // Colonne 6 : Étage
        QString levelStr = QString::fromStdString(node.levelId());
        if (levelStr.isEmpty())
        {
            levelStr = "-";
        }
        auto* itemLevel = new QTableWidgetItem(levelStr);
        m_tableWidget->setItem(row, 6, itemLevel);

        ++row;
    }

    m_tableWidget->setSortingEnabled(true);
    m_tableWidget->sortByColumn(0, Qt::AscendingOrder);

    m_statusLabel->setText(tr("%1 nœud(s) au total.").arg(nodesMap.size()));
}

void NodeSelectionDialog::setSelectedNodeId(int nodeId)
{
    if (nodeId <= 0) return;

    for (int r = 0; r < m_tableWidget->rowCount(); ++r)
    {
        auto* item = m_tableWidget->item(r, 0);
        if (item && item->data(Qt::UserRole).toInt() == nodeId)
        {
            m_tableWidget->selectRow(r);
            m_tableWidget->scrollToItem(item);
            break;
        }
    }
}

void NodeSelectionDialog::onFilterTextChanged(const QString& filter)
{
    QString q = filter.trimmed();
    int visibleCount = 0;

    for (int r = 0; r < m_tableWidget->rowCount(); ++r)
    {
        bool match = false;
        if (q.isEmpty())
        {
            match = true;
        }
        else
        {
            for (int c = 0; c < m_tableWidget->columnCount(); ++c)
            {
                auto* item = m_tableWidget->item(r, c);
                if (item && item->text().contains(q, Qt::CaseInsensitive))
                {
                    match = true;
                    break;
                }
            }
        }
        m_tableWidget->setRowHidden(r, !match);
        if (match) ++visibleCount;
    }

    m_statusLabel->setText(tr("%1 nœud(s) affiché(s).").arg(visibleCount));
}

void NodeSelectionDialog::onTableRowDoubleClicked(int row, int /*col*/)
{
    auto* item = m_tableWidget->item(row, 0);
    if (!item) return;

    int nid = item->data(Qt::UserRole).toInt();
    if (m_model)
    {
        const auto* node = m_model->getNode(nid);
        if (node)
        {
            m_selectedNodeId = nid;
            m_selectedPoint = gp_Pnt(node->x(), node->y(), node->z());
            accept();
        }
    }
}

void NodeSelectionDialog::onTableSelectionChanged()
{
    auto selItems = m_tableWidget->selectedItems();
    if (selItems.isEmpty())
    {
        m_btnSelect->setEnabled(false);
        return;
    }

    int row = selItems.first()->row();
    auto* item = m_tableWidget->item(row, 0);
    if (!item) return;

    int nid = item->data(Qt::UserRole).toInt();
    if (m_model)
    {
        const auto* node = m_model->getNode(nid);
        if (node)
        {
            m_selectedNodeId = nid;
            m_selectedPoint = gp_Pnt(node->x(), node->y(), node->z());
            m_btnSelect->setEnabled(true);

            if (m_occView)
            {
                m_occView->highlightNode(nid);
            }
        }
    }
}

void NodeSelectionDialog::onAcceptClicked()
{
    if (m_selectedNodeId > 0)
    {
        accept();
    }
}

} // namespace TSA::UI
