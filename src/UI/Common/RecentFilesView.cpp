#include "RecentFilesView.h"
#include "UI/Theme/ThemedIcon.h"
#include "App/ProductInfo.h"

#include "../../Project/RecentProjects.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLocale>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{
namespace
{
enum Column { ColName, ColFile, ColFolder, ColOpened, ColType, ColActions, ColCount };
constexpr int kThumbWidth = 64;
constexpr int kThumbHeight = 36;
constexpr int kPathRole = Qt::UserRole + 1;

QString key(const QString& path)
{
    return TSA::Project::RecentProjects::normalize(path).toLower();
}
} // namespace

RecentFilesView::RecentFilesView(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setObjectName("RecentFilesTable");
    m_table->setHorizontalHeaderLabels({ tr("Nom du projet"), tr("Fichier"), tr("Emplacement"), tr("Dernière ouverture"), tr("Type"), QString() });
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(kThumbHeight + 10);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setIconSize(QSize(kThumbWidth, kThumbHeight));
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setTextElideMode(Qt::ElideMiddle);
    m_table->setWordWrap(false);
    auto* header = m_table->horizontalHeader();
    header->setHighlightSections(false);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(ColFolder, QHeaderView::Stretch);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(ColActions, QHeaderView::Fixed);   // boutons : un widget de cellule ne compte pas dans « ajuster au contenu »
    m_table->setColumnWidth(ColActions, 120);
    m_table->setColumnWidth(ColName, 280);
    m_table->setColumnWidth(ColFile, 190);
    m_table->setColumnWidth(ColOpened, 170);
    m_table->setColumnWidth(ColType, 170);
    layout->addWidget(m_table);

    auto pathOfRow = [this](int row) {
        const QTableWidgetItem* item = row >= 0 ? m_table->item(row, ColName) : nullptr;
        return item ? item->data(kPathRole).toString() : QString();
    };
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this, pathOfRow](int row, int) {
        const QString path = pathOfRow(row);
        if (!path.isEmpty()) emit openRequested(path);
    });
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this, pathOfRow](const QPoint& pos) {
        const QString path = pathOfRow(m_table->rowAt(pos.y()));
        if (!path.isEmpty()) emit contextMenuRequested(path, m_table->viewport()->mapToGlobal(pos));
    });
}

QString RecentFilesView::displayName(const QString& path) const
{
    const auto it = m_summaries.constFind(key(path));
    if (it != m_summaries.constEnd() && !it->projectName.trimmed().isEmpty() && it->projectName != TSA::Product::kDefaultProjectName)
        return it->projectName.trimmed();
    return QFileInfo(path).completeBaseName();
}

void RecentFilesView::setProjects(const QVector<ProjectCardGrid::Entry>& entries)
{
    m_entries = entries;
    rebuild();
}

void RecentFilesView::setSortKey(ProjectSortKey sortKey)
{
    m_sort = sortKey;
    rebuild();
}

void RecentFilesView::setFilter(const QString& filter)
{
    m_filter = filter;
    applyFilter();
}

void RecentFilesView::rebuild()
{
    QVector<ProjectCardGrid::Entry> sorted = m_entries;
    std::stable_sort(sorted.begin(), sorted.end(), [this](const ProjectCardGrid::Entry& a, const ProjectCardGrid::Entry& b) {
        if (m_sort == ProjectSortKey::Name) return displayName(a.path).localeAwareCompare(displayName(b.path)) < 0;
        if (m_sort == ProjectSortKey::Modified) return QFileInfo(a.path).lastModified() > QFileInfo(b.path).lastModified();
        return a.lastOpened > b.lastOpened;
    });

    m_table->setRowCount(0);
    m_table->setRowCount(sorted.size());
    for (int row = 0; row < sorted.size(); ++row)
    {
        const QString path = sorted[row].path;
        const QFileInfo fi(path);
        auto* name = new QTableWidgetItem(displayName(path));
        name->setData(kPathRole, path);
        name->setToolTip(tr("%1\nDouble-clic pour ouvrir").arg(QDir::toNativeSeparators(path)));
        if (const auto preview = m_previews.constFind(key(path)); preview != m_previews.constEnd())
            name->setIcon(QIcon(cropToFill(*preview, kThumbWidth, kThumbHeight)));
        m_table->setItem(row, ColName, name);
        m_table->setItem(row, ColFile, new QTableWidgetItem(fi.fileName()));
        auto* folder = new QTableWidgetItem(QDir::toNativeSeparators(fi.absolutePath()));
        folder->setToolTip(folder->text());
        m_table->setItem(row, ColFolder, folder);
        const QDateTime opened = sorted[row].lastOpened;
        m_table->setItem(row, ColOpened, new QTableWidgetItem(opened.isValid() ? QLocale().toString(opened, QLocale::ShortFormat) : QString()));
        m_table->setItem(row, ColType, new QTableWidgetItem(projectTypeText(m_summaries.value(key(path)))));

        auto* actions = new QWidget(m_table);
        auto* box = new QHBoxLayout(actions);
        box->setContentsMargins(6, 0, 6, 0);
        box->setSpacing(4);
        auto* open = new QToolButton(actions);
        open->setObjectName("ProjectCardOpen");
        open->setText(tr("Ouvrir"));
        open->setCursor(Qt::PointingHandCursor);
        open->setToolTip(tr("Ouvrir ce projet"));
        connect(open, &QToolButton::clicked, this, [this, path] { emit openRequested(path); });
        auto* remove = new QToolButton(actions);
        remove->setIcon(TSA::UI::themedIcon(QStringLiteral(":/icons/common/remove.svg")));
        remove->setIconSize(QSize(14, 14));
        remove->setAutoRaise(true);
        remove->setCursor(Qt::PointingHandCursor);
        remove->setToolTip(tr("Retirer de la liste (le fichier n'est pas supprimé)"));
        remove->setAccessibleName(tr("Retirer de la liste"));
        connect(remove, &QToolButton::clicked, this, [this, path] { emit removeRequested(path); });
        box->addWidget(open);
        box->addWidget(remove);
        m_table->setCellWidget(row, ColActions, actions);
    }
    applyFilter();
}

void RecentFilesView::applyFilter()
{
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        const QString path = m_table->item(row, ColName)->data(kPathRole).toString();
        const bool match = m_filter.isEmpty() || displayName(path).contains(m_filter, Qt::CaseInsensitive) || path.contains(m_filter, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !match);
    }
}

int RecentFilesView::visibleCount() const
{
    int n = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) n += m_table->isRowHidden(row) ? 0 : 1;
    return n;
}

int RecentFilesView::rowOf(const QString& path) const
{
    const QString wanted = key(path);
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (key(m_table->item(row, ColName)->data(kPathRole).toString()) == wanted) return row;
    return -1;
}

void RecentFilesView::updatePreview(const QString& path, const QImage& image)
{
    if (image.isNull()) return;
    m_previews.insert(key(path), image);
    if (const int row = rowOf(path); row >= 0)
        m_table->item(row, ColName)->setIcon(QIcon(cropToFill(image, kThumbWidth, kThumbHeight)));
}

void RecentFilesView::updateSummary(const QString& path, const TSA::Project::ProjectFileSummary& summary)
{
    m_summaries.insert(key(path), summary);
    if (const int row = rowOf(path); row >= 0)
    {
        m_table->item(row, ColName)->setText(displayName(path));
        m_table->item(row, ColType)->setText(projectTypeText(summary));
    }
}

} // namespace TSA::UI
