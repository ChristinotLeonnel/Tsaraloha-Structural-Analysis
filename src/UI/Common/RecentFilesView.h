#pragma once

// Liste détaillée des projets récents (page Fichiers récents de l'accueil) : miniature, nom, fichier, emplacement, date de
// dernière ouverture, type, boutons Ouvrir et Retirer. Double-clic ou Entrée : ouvre. Les actions sont des signaux.

#include "ProjectCardGrid.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QTableWidget;

namespace TSA::UI
{

class RecentFilesView : public QWidget
{
    Q_OBJECT

public:
    explicit RecentFilesView(QWidget* parent = nullptr);

    void setProjects(const QVector<ProjectCardGrid::Entry>& entries);
    void setFilter(const QString& filter);
    void setSortKey(ProjectSortKey key);

    int count() const { return m_entries.size(); }
    int visibleCount() const;

    void updatePreview(const QString& path, const QImage& image);
    void updateSummary(const QString& path, const TSA::Project::ProjectFileSummary& summary);

signals:
    void openRequested(const QString& path);
    void removeRequested(const QString& path);
    void contextMenuRequested(const QString& path, const QPoint& globalPos);

private:
    void rebuild();
    void applyFilter();
    QString displayName(const QString& path) const;
    int rowOf(const QString& path) const;

private:
    QTableWidget* m_table = nullptr;
    QVector<ProjectCardGrid::Entry> m_entries;
    QHash<QString, TSA::Project::ProjectFileSummary> m_summaries;
    QHash<QString, QImage> m_previews;
    QString m_filter;
    ProjectSortKey m_sort = ProjectSortKey::LastOpened;
};

} // namespace TSA::UI
