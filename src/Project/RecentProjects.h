#pragma once

// Liste persistante des projets récemment ouverts ou enregistrés (QSettings « RecentProjects »).
// Les chemins sont normalisés : un même fichier n'apparaît qu'une fois, en tête s'il est rouvert.

#include <QDateTime>
#include <QString>
#include <QVector>

namespace TSA::Project
{

struct RecentProject
{
    QString path;
    QDateTime lastOpened;
    bool exists = true;
};

class RecentProjects
{
public:
    static constexpr int kMaxEntries = 16;

    /// Groupe QSettings utilisé (modifiable pour les tests).
    explicit RecentProjects(const QString& settingsGroup = QStringLiteral("RecentProjects"));

    QVector<RecentProject> list(bool includeMissing = false) const;
    void touch(const QString& path, const QDateTime& when = QDateTime::currentDateTime());
    void remove(const QString& path);
    void rename(const QString& oldPath, const QString& newPath);
    void clear();

    static QString normalize(const QString& path);
    /// « à l'instant », « il y a 5 min », « il y a 2 h », « hier », « il y a 3 jours », date.
    static QString relativeTime(const QDateTime& when, const QDateTime& now = QDateTime::currentDateTime());

private:
    QVector<RecentProject> load() const;
    void save(const QVector<RecentProject>& entries) const;

    QString m_group;
};

} // namespace TSA::Project
