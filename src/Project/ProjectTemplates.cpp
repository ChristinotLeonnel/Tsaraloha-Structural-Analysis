#include "ProjectTemplates.h"

#include "../Core/AppPaths.h"
#include "../Grid/GridCoordinateSync.h"
#include "../Grid/GridManager.h"
#include "../IO/TSAFile.h"
#include "../Model/Model.h"
#include "App/ProductInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>

namespace TSA::Project
{

namespace
{
const char* kFormat = "tsa-project-template/1";

QString normalized(const QString& name)
{
    return name.trimmed().toCaseFolded();
}

QString metadataPath(const QString& tsaPath)
{
    const QFileInfo fi(tsaPath);
    return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".json"));
}

ProjectTemplateInfo readInfo(const QString& tsaPath, QStringList* problems)
{
    ProjectTemplateInfo info;
    info.path = tsaPath;
    info.name = QFileInfo(tsaPath).completeBaseName();
    info.created = QFileInfo(tsaPath).birthTime().isValid() ? QFileInfo(tsaPath).birthTime() : QFileInfo(tsaPath).lastModified();
    QFile f(metadataPath(tsaPath));
    if (!f.exists())
    {
        if (problems) problems->append(ProjectTemplateStore::tr("%1 : fiche du template absente (nom du fichier utilisé)").arg(QFileInfo(tsaPath).fileName()));
        return info;
    }
    QJsonParseError err;
    const QJsonDocument doc = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll(), &err) : QJsonDocument();
    const QJsonObject o = doc.object();
    if (!doc.isObject() || o.value(QStringLiteral("format")).toString() != QLatin1String(kFormat))
    {
        if (problems) problems->append(ProjectTemplateStore::tr("%1 : fiche du template illisible (nom du fichier utilisé)").arg(QFileInfo(tsaPath).fileName()));
        return info;
    }
    if (const QString n = o.value(QStringLiteral("name")).toString().trimmed(); !n.isEmpty()) info.name = n;
    info.description = o.value(QStringLiteral("description")).toString();
    if (const QDateTime d = QDateTime::fromString(o.value(QStringLiteral("created")).toString(), Qt::ISODate); d.isValid()) info.created = d;
    return info;
}
} // namespace

ProjectTemplateStore::ProjectTemplateStore(const QString& directory)
    : m_directory(directory.isEmpty() ? TSA::Core::AppPaths::userProjectTemplatesDir() : directory)
{
}

QString ProjectTemplateStore::fileBaseName(const QString& name)
{
    static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    QString kept = name.trimmed();
    kept.remove(invalid);
    while (kept.endsWith(QLatin1Char('.')) || kept.endsWith(QLatin1Char(' '))) kept.chop(1);
    if (kept.trimmed().isEmpty()) return QString();   // vide ou composé uniquement de caractères interdits
    QString base = name.trimmed();
    base.replace(invalid, QStringLiteral("_"));
    while (base.endsWith(QLatin1Char('.')) || base.endsWith(QLatin1Char(' '))) base.chop(1);
    return base.left(120);
}

std::vector<ProjectTemplateInfo> ProjectTemplateStore::list(QStringList* problems) const
{
    std::vector<ProjectTemplateInfo> out;
    const QDir dir(m_directory);
    const QString pattern = QStringLiteral("*") + TSA::Product::projectExtension();
    for (const QFileInfo& fi : dir.entryInfoList({ pattern }, QDir::Files, QDir::Name))
        out.push_back(readInfo(fi.absoluteFilePath(), problems));
    std::sort(out.begin(), out.end(), [](const ProjectTemplateInfo& a, const ProjectTemplateInfo& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return out;
}

std::optional<ProjectTemplateInfo> ProjectTemplateStore::find(const QString& name) const
{
    const QString key = normalized(name);
    const QString fileKey = normalized(fileBaseName(name));
    for (const auto& t : list())
        if (normalized(t.name) == key || normalized(QFileInfo(t.path).completeBaseName()) == fileKey) return t;
    return std::nullopt;
}

bool ProjectTemplateStore::save(const QString& name, const QString& description, const TSA::Model::Model& model,
                                const TSA::Grid::GridManager* grids, Overwrite overwrite, QString* error, ProjectTemplateInfo* saved) const
{
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const QString displayName = name.trimmed();
    const QString base = fileBaseName(displayName);
    if (displayName.isEmpty() || base.isEmpty()) return fail(tr("Nom de template vide ou composé uniquement de caractères interdits."));
    if (!QDir().mkpath(m_directory)) return fail(tr("Impossible de créer le dossier des templates %1").arg(QDir::toNativeSeparators(m_directory)));

    const std::optional<ProjectTemplateInfo> existing = find(displayName);
    if (existing && overwrite == Overwrite::Refuse)
        return fail(tr("Un template « %1 » existe déjà.").arg(existing->name));

    const QString path = existing ? existing->path : QDir(m_directory).filePath(base + TSA::Product::projectExtension());
    QString ioError;
    // Écriture dans un fichier temporaire puis remplacement : un échec laisse l'éventuel template précédent intact
    const QString tmp = path + QStringLiteral(".part");
    QFile::remove(tmp);
    if (!TSA::IO::TSAProjectIO::saveProject(tmp, model, grids, displayName, QString(), true, QImage(), &ioError))
    {
        QFile::remove(tmp);
        return fail(tr("Écriture du template impossible : %1").arg(ioError));
    }
    if (QFile::exists(path) && !QFile::remove(path))
    {
        QFile::remove(tmp);
        return fail(tr("Le template existant %1 ne peut pas être remplacé (fichier ouvert ou protégé).").arg(QDir::toNativeSeparators(path)));
    }
    if (!QFile::rename(tmp, path))
    {
        QFile::remove(tmp);
        return fail(tr("Écriture du template impossible : %1").arg(QDir::toNativeSeparators(path)));
    }

    ProjectTemplateInfo info;
    info.name = displayName;
    info.description = description.trimmed();
    info.created = QDateTime::currentDateTime();
    info.path = path;
    QJsonObject meta;
    meta.insert(QStringLiteral("format"), QString::fromLatin1(kFormat));
    meta.insert(QStringLiteral("name"), info.name);
    meta.insert(QStringLiteral("description"), info.description);
    meta.insert(QStringLiteral("created"), info.created.toString(Qt::ISODate));
    QSaveFile mf(metadataPath(path));
    if (!mf.open(QIODevice::WriteOnly) || mf.write(QJsonDocument(meta).toJson()) < 0 || !mf.commit())
        return fail(tr("Template enregistré, mais sa fiche (nom, description) n'a pas pu être écrite : %1").arg(QDir::toNativeSeparators(metadataPath(path))));
    if (saved) *saved = info;
    return true;
}

bool ProjectTemplateStore::load(const ProjectTemplateInfo& info, TSA::Model::Model& model, TSA::Grid::GridManager* grids, QString* error) const
{
    if (!QFileInfo::exists(info.path))
    {
        if (error) *error = tr("Template introuvable : %1").arg(QDir::toNativeSeparators(info.path));
        return false;
    }
    QString ioError;
    {
        // Vérification complète dans un modèle à part : un template corrompu ne touche pas au projet cible
        TSA::Model::Model probe;
        TSA::Grid::GridManager probeGrids;
        if (!TSA::IO::TSAProjectIO::loadProject(info.path, probe, &probeGrids, nullptr, nullptr, nullptr, &ioError))
        {
            if (error) *error = tr("Template « %1 » illisible : %2").arg(info.name, ioError);
            return false;
        }
    }
    if (!TSA::IO::TSAProjectIO::loadProject(info.path, model, grids, nullptr, nullptr, nullptr, &ioError))
    {
        if (error) *error = tr("Template « %1 » illisible : %2").arg(info.name, ioError);
        return false;
    }
    TSA::Grid::reconcileProject(model, grids);
    return true;
}

bool ProjectTemplateStore::createProject(const ProjectTemplateInfo& info, const QString& projectPath, const QString& projectName,
                                         QString* error) const
{
    if (QFileInfo::exists(projectPath))
    {
        if (error) *error = tr("Un fichier existe déjà : %1").arg(QDir::toNativeSeparators(projectPath));
        return false;
    }
    TSA::Model::Model model;
    TSA::Grid::GridManager grids;
    if (!load(info, model, &grids, error)) return false;
    QString ioError;
    if (!TSA::IO::TSAProjectIO::saveProject(projectPath, model, &grids, projectName, QString(), true, QImage(), &ioError))
    {
        if (error) *error = tr("Création du projet impossible : %1").arg(ioError);
        return false;
    }
    return true;
}

} // namespace TSA::Project
