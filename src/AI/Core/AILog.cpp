#include "AILog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QStandardPaths>

namespace TSA::AI
{

namespace
{
QMutex& logMutex()
{
    static QMutex m;
    return m;
}
}

QJsonObject AILogEntry::toJson() const
{
    QJsonObject o{ { "ts", timestamp }, { "event", event }, { "provider", provider }, { "model", model },
                   { "backend", backend } };
    if (latencyMs >= 0) o["latencyMs"] = latencyMs;
    if (promptTokens >= 0) o["promptTokens"] = promptTokens;
    if (completionTokens >= 0) o["completionTokens"] = completionTokens;
    if (tokensPerSecond > 0) o["tokensPerSecond"] = tokensPerSecond;
    if (!detail.isEmpty()) o["detail"] = detail;
    return o;
}

QString AILog::directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ai/logs";
}

void AILog::append(AILogEntry entry)
{
    if (entry.timestamp.isEmpty()) entry.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    QMutexLocker lock(&logMutex());
    QDir().mkpath(directory());
    QFile f(directory() + "/ai-" + QDate::currentDate().toString("yyyyMMdd") + ".jsonl");
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write(QJsonDocument(entry.toJson()).toJson(QJsonDocument::Compact) + "\n");
}

QVector<AILogEntry> AILog::recent(int maxEntries)
{
    QMutexLocker lock(&logMutex());
    QDir dir(directory());
    QStringList files = dir.entryList({ "ai-*.jsonl" }, QDir::Files, QDir::Name | QDir::Reversed);
    QVector<AILogEntry> entries;
    for (const QString& name : files)
    {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QList<QByteArray> lines = f.readAll().split('\n');
        for (auto it = lines.rbegin(); it != lines.rend() && entries.size() < maxEntries; ++it)
        {
            const auto o = QJsonDocument::fromJson(*it).object();
            if (o.isEmpty()) continue;
            AILogEntry e;
            e.timestamp = o["ts"].toString();
            e.event = o["event"].toString();
            e.provider = o["provider"].toString();
            e.model = o["model"].toString();
            e.backend = o["backend"].toString();
            e.latencyMs = static_cast<qint64>(o["latencyMs"].toDouble(-1));
            e.promptTokens = o["promptTokens"].toInt(-1);
            e.completionTokens = o["completionTokens"].toInt(-1);
            e.tokensPerSecond = o["tokensPerSecond"].toDouble();
            e.detail = o["detail"].toString();
            entries.push_back(e);
        }
        if (entries.size() >= maxEntries) break;
    }
    return entries;
}

} // namespace TSA::AI
