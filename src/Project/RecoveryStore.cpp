#include "RecoveryStore.h"

#include "../IO/TSAFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace TSA::Project
{

RecoveryStore::RecoveryStore(const QString& directory)
    : m_dir(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery") : directory)
{
}

QString RecoveryStore::path(const QString& key, const char* suffix) const
{
    return QDir(m_dir).filePath(key + QString::fromLatin1(suffix));
}

std::unique_ptr<QLockFile> RecoveryStore::lock(const QString& key) const
{
    if (key.isEmpty() || !QDir().mkpath(m_dir)) return nullptr;
    auto lockFile = std::make_unique<QLockFile>(path(key, ".lock"));
    lockFile->setStaleLockTime(0);   // verrou d'un processus disparu : périmé (fermeture brutale)
    if (!lockFile->tryLock(0)) return nullptr;
    return lockFile;
}

bool RecoveryStore::write(const QString& key, const TSA::Model::Model& model, const TSA::Grid::GridManager* grids, const QString& originalPath,
                          const QString& title, QString* error) const
{
    if (key.isEmpty() || !QDir().mkpath(m_dir))
    {
        if (error) *error = QStringLiteral("dossier de récupération inaccessible : %1").arg(m_dir);
        return false;
    }
    std::string err;
    // Écriture atomique (QSaveFile dans TSAProjectIO) : une copie interrompue ne remplace jamais la précédente
    if (!TSA::IO::TSAProjectIO::saveToFile(path(key, ".tsa"), model, grids, &err))
    {
        if (error) *error = QString::fromStdString(err);
        return false;
    }
    QSaveFile info(path(key, ".json"));
    if (!info.open(QIODevice::WriteOnly)) return false;
    info.write(QJsonDocument(QJsonObject { { "originalPath", originalPath },
                                           { "title", title },
                                           { "savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate) } })
                   .toJson(QJsonDocument::Compact));
    return info.commit();
}

void RecoveryStore::remove(const QString& key) const
{
    if (key.isEmpty()) return;
    QFile::remove(path(key, ".tsa"));
    QFile::remove(path(key, ".json"));
}

std::vector<RecoveryEntry> RecoveryStore::orphans() const
{
    std::vector<RecoveryEntry> out;
    const QDir dir(m_dir);
    for (const QFileInfo& fi : dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files))
    {
        const QString key = fi.completeBaseName();
        if (!QFile::exists(path(key, ".tsa"))) continue;
        // Document encore ouvert (cette instance ou une autre) : verrou tenu par un processus vivant
        QLockFile probe(path(key, ".lock"));
        probe.setStaleLockTime(0);
        if (QFile::exists(path(key, ".lock")) && !probe.tryLock(0)) continue;
        if (probe.isLocked()) probe.unlock();
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        RecoveryEntry e;
        e.key = key;
        e.file = path(key, ".tsa");
        e.originalPath = o.value("originalPath").toString();
        e.title = o.value("title").toString();
        e.savedAt = QDateTime::fromString(o.value("savedAt").toString(), Qt::ISODate);
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const RecoveryEntry& a, const RecoveryEntry& b) { return a.savedAt > b.savedAt; });
    return out;
}

int RecoveryStore::intervalMinutes()
{
    return std::clamp(QSettings().value(QStringLiteral("Project/autosaveMinutes"), 5).toInt(), 0, 120);
}

void RecoveryStore::setIntervalMinutes(int minutes)
{
    QSettings().setValue(QStringLiteral("Project/autosaveMinutes"), std::clamp(minutes, 0, 120));
}

} // namespace TSA::Project
