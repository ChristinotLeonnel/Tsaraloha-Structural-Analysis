#include "RecentProjects.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QCoreApplication>

#include <memory>

namespace TSA::Project
{

namespace
{
// Même identité que l'application (Application.cpp) même si l'hôte ne l'a pas définie (tests, outils).
std::unique_ptr<QSettings> openSettings()
{
    if (QCoreApplication::organizationName().isEmpty())
        return std::make_unique<QSettings>(QStringLiteral("TSA Engineering"), QStringLiteral("TSA"));
    return std::make_unique<QSettings>();
}
} // namespace

RecentProjects::RecentProjects(const QString& settingsGroup)
    : m_group(settingsGroup)
{
}

QString RecentProjects::normalize(const QString& path)
{
    const QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? fi.absoluteFilePath() : canonical);
}

QVector<RecentProject> RecentProjects::load() const
{
    auto settings = openSettings();
    QSettings& s = *settings;
    QVector<RecentProject> entries;
    const int n = s.beginReadArray(m_group);
    for (int i = 0; i < n; ++i)
    {
        s.setArrayIndex(i);
        RecentProject p;
        p.path = s.value("path").toString();
        p.lastOpened = s.value("lastOpened").toDateTime();
        if (!p.path.isEmpty()) entries.push_back(p);
    }
    s.endArray();
    return entries;
}

void RecentProjects::save(const QVector<RecentProject>& entries) const
{
    auto settings = openSettings();
    QSettings& s = *settings;
    s.remove(m_group);
    s.beginWriteArray(m_group, static_cast<int>(entries.size()));
    for (int i = 0; i < entries.size(); ++i)
    {
        s.setArrayIndex(i);
        s.setValue("path", entries[i].path);
        s.setValue("lastOpened", entries[i].lastOpened);
    }
    s.endArray();
}

QVector<RecentProject> RecentProjects::list(bool includeMissing) const
{
    QVector<RecentProject> result;
    for (RecentProject p : load())
    {
        p.exists = QFileInfo::exists(p.path);
        if (p.exists || includeMissing) result.push_back(p);
    }
    return result;
}

void RecentProjects::touch(const QString& path, const QDateTime& when)
{
    if (path.isEmpty()) return;
    const QString key = normalize(path);
    QVector<RecentProject> entries = load();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const RecentProject& p) { return normalize(p.path).compare(key, Qt::CaseInsensitive) == 0; }),
                  entries.end());
    entries.prepend({ key, when, true });
    while (entries.size() > kMaxEntries) entries.removeLast();
    save(entries);
}

void RecentProjects::remove(const QString& path)
{
    const QString key = normalize(path);
    QVector<RecentProject> entries = load();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const RecentProject& p) { return normalize(p.path).compare(key, Qt::CaseInsensitive) == 0; }),
                  entries.end());
    save(entries);
}

void RecentProjects::rename(const QString& oldPath, const QString& newPath)
{
    QVector<RecentProject> entries = load();
    const QString key = normalize(oldPath);
    for (auto& p : entries)
        if (normalize(p.path).compare(key, Qt::CaseInsensitive) == 0) p.path = normalize(newPath);
    save(entries);
}

void RecentProjects::clear()
{
    openSettings()->remove(m_group);
}

QString RecentProjects::relativeTime(const QDateTime& when, const QDateTime& now)
{
    if (!when.isValid()) return QString();
    const qint64 secs = when.secsTo(now);
    if (secs < 60) return QStringLiteral("à l'instant");
    if (secs < 3600) return QStringLiteral("il y a %1 min").arg(secs / 60);
    const int days = static_cast<int>(when.date().daysTo(now.date()));
    if (days == 0) return QStringLiteral("il y a %1 h").arg(secs / 3600);
    if (days == 1) return QStringLiteral("hier, %1").arg(when.toString("HH:mm"));
    if (days < 7) return QStringLiteral("il y a %1 jours").arg(days);
    return when.toString("dd/MM/yyyy");
}

} // namespace TSA::Project
