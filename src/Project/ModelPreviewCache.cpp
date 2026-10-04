#include "ModelPreviewCache.h"

#include "RecentProjects.h"
#include "../IO/TSAFile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>

namespace TSA::Project
{

QJsonObject ProjectPreviewMetadata::toJson() const
{
    return QJsonObject{
        { "projectId", projectId }, { "projectPath", projectPath }, { "previewPath", previewPath },
        { "sourceFormat", sourceFormat }, { "fileLastModified", fileLastModified.toString(Qt::ISODateWithMs) },
        { "capturedAt", capturedAt.toString(Qt::ISODateWithMs) }, { "modelRevision", static_cast<double>(modelRevision) },
        { "cameraState", cameraState }, { "viewState", viewState }, { "previewVersion", previewVersion },
        { "nodeCount", nodeCount }, { "elementCount", elementCount } };
}

ProjectPreviewMetadata ProjectPreviewMetadata::fromJson(const QJsonObject& o)
{
    ProjectPreviewMetadata m;
    m.projectId = o["projectId"].toString();
    m.projectPath = o["projectPath"].toString();
    m.previewPath = o["previewPath"].toString();
    m.sourceFormat = o["sourceFormat"].toString("TSA");
    m.fileLastModified = QDateTime::fromString(o["fileLastModified"].toString(), Qt::ISODateWithMs);
    m.capturedAt = QDateTime::fromString(o["capturedAt"].toString(), Qt::ISODateWithMs);
    m.modelRevision = static_cast<quint64>(o["modelRevision"].toDouble());
    m.cameraState = o["cameraState"].toObject();
    m.viewState = o["viewState"].toObject();
    m.previewVersion = o["previewVersion"].toInt(0);
    m.nodeCount = o["nodeCount"].toInt();
    m.elementCount = o["elementCount"].toInt();
    return m;
}

ModelPreviewCache::ModelPreviewCache(const QString& directory)
    : m_dir(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/previews" : directory)
{
}

QString ModelPreviewCache::projectId(const QString& projectPath)
{
    const QString key = RecentProjects::normalize(projectPath).toLower();
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
}

std::optional<ProjectPreviewMetadata> ModelPreviewCache::metadata(const QString& projectPath) const
{
    QFile f(metaPath(projectId(projectPath)));
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return std::nullopt;
    auto m = ProjectPreviewMetadata::fromJson(doc.object());
    if (m.previewVersion != ProjectPreviewMetadata::kPreviewVersion) return std::nullopt; // format périmé
    return m;
}

QImage ModelPreviewCache::preview(const QString& projectPath, PreviewSource* source) const
{
    if (source) *source = PreviewSource::None;
    const QFileInfo file(projectPath);
    if (const auto meta = metadata(projectPath))
    {
        // Le cache n'est valable que si le fichier n'a pas été modifié après la capture
        // (par une autre instance, un autre poste…). Tolérance : 2 s (horloges, écriture différée).
        const bool fileNewer = file.exists() && meta->capturedAt.isValid()
                            && file.lastModified() > meta->capturedAt.addSecs(2);
        QImage img(imagePath(meta->projectId));
        if (!img.isNull() && !fileNewer)
        {
            if (source) *source = PreviewSource::Cache;
            return img;
        }
    }
    QImage embedded;
    if (file.exists() && TSA::IO::TSAProjectIO::extractThumbnail(projectPath, embedded) && !embedded.isNull())
    {
        if (source) *source = PreviewSource::Embedded;
        return embedded;
    }
    return {};
}

bool ModelPreviewCache::store(const QImage& image, ProjectPreviewMetadata meta) const
{
    if (image.isNull() || meta.projectPath.isEmpty()) return false;
    QDir().mkpath(m_dir);
    meta.projectId = projectId(meta.projectPath);
    meta.previewPath = imagePath(meta.projectId);
    meta.previewVersion = ProjectPreviewMetadata::kPreviewVersion;

    const QImage scaled = (image.width() > kWidth || image.height() > kHeight)
                              ? image.scaled(kWidth, kHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                              : image;
    QSaveFile png(meta.previewPath);
    if (!png.open(QIODevice::WriteOnly) || !scaled.save(&png, "PNG") || !png.commit()) return false;
    QSaveFile json(metaPath(meta.projectId));
    if (!json.open(QIODevice::WriteOnly)) return false;
    json.write(QJsonDocument(meta.toJson()).toJson(QJsonDocument::Indented));
    return json.commit();
}

void ModelPreviewCache::remove(const QString& projectPath) const
{
    const QString id = projectId(projectPath);
    QFile::remove(imagePath(id));
    QFile::remove(metaPath(id));
}

void ModelPreviewCache::rename(const QString& oldPath, const QString& newPath) const
{
    auto meta = metadata(oldPath);
    const QImage img(imagePath(projectId(oldPath)));
    remove(oldPath);
    if (meta && !img.isNull())
    {
        meta->projectPath = newPath;
        store(img, *meta);
    }
}

bool ModelPreviewCache::needsCapture(const std::optional<ProjectPreviewMetadata>& last, quint64 revision,
                                     const QJsonObject& cameraState)
{
    if (!last) return true;
    if (last->modelRevision != revision) return true;
    // Caméra : petite tolérance pour ne pas recapturer sur des écarts numériques.
    for (const QString& key : { QStringLiteral("eye"), QStringLiteral("center"), QStringLiteral("up") })
    {
        const auto a = last->cameraState[key].toArray();
        const auto b = cameraState[key].toArray();
        if (a.size() != b.size()) return true;
        for (int i = 0; i < a.size(); ++i)
            if (std::abs(a[i].toDouble() - b[i].toDouble()) > 1e-3 * std::max(1.0, std::abs(b[i].toDouble()))) return true;
    }
    if (std::abs(last->cameraState["scale"].toDouble() - cameraState["scale"].toDouble())
        > 1e-3 * std::max(1.0, std::abs(cameraState["scale"].toDouble())))
        return true;
    return last->cameraState["projection"] != cameraState["projection"];
}

} // namespace TSA::Project
