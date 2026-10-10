#include "TemplateRepository.h"

#include "../Core/AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>

namespace TSA::Templates
{

QString templateOriginName(TemplateOrigin o)
{
    switch (o)
    {
    case TemplateOrigin::BuiltIn: return QStringLiteral("intégré");
    case TemplateOrigin::Module: return QStringLiteral("module");
    case TemplateOrigin::User: return QStringLiteral("utilisateur");
    case TemplateOrigin::Imported: return QStringLiteral("importé");
    case TemplateOrigin::Customized: return QStringLiteral("personnalisé");
    case TemplateOrigin::Archived: return QStringLiteral("ancienne version");
    }
    return {};
}

QString TemplateEntry::key() const
{
    return templateOriginName(origin) + QLatin1Char(':') + path;
}

TemplateRepository::TemplateRepository()
    : m_builtInRoot(QStringLiteral(":/templates/builtin"))
{
}

TemplateRepository& TemplateRepository::instance()
{
    static TemplateRepository r;
    static const bool loaded = (r.reload(), true);
    Q_UNUSED(loaded);
    return r;
}

void TemplateRepository::setBuiltInRoot(const QString& root) { m_builtInRoot = root; }
void TemplateRepository::setUserRoot(const QString& root) { m_userRoot = root; }
QString TemplateRepository::userRoot() const { return m_userRoot.isEmpty() ? TSA::Core::AppPaths::userTemplatesDir() : m_userRoot; }
QString TemplateRepository::importIndexPath() const { return userRoot() + QStringLiteral("/imported/index.json"); }

void TemplateRepository::scanDir(const QString& dir, TemplateOrigin origin)
{
    QJsonObject index;
    if (origin == TemplateOrigin::Imported)
    {
        QFile f(importIndexPath());
        if (f.open(QIODevice::ReadOnly)) index = QJsonDocument::fromJson(f.readAll()).object();
    }
    for (const QFileInfo& fi : QDir(dir).entryInfoList({ QStringLiteral("*") + QLatin1String(kPackageExtension) }, QDir::Files, QDir::Name))
    {
        TemplateEntry e;
        QStringList errors;
        if (!TemplatePackage::loadFile(fi.absoluteFilePath(), &e.package, &errors))
        {
            m_problems.push_back({ fi.absoluteFilePath(), errors });
            continue;
        }
        e.origin = origin;
        e.path = fi.absoluteFilePath();
        if (origin == TemplateOrigin::Customized) e.modified = !e.package.manifest().baseHash.isEmpty() && e.package.contentHash() != e.package.manifest().baseHash;
        if (origin == TemplateOrigin::Imported) e.modified = index.value(fi.fileName()).toObject().value("hash").toString() != e.package.contentHash();
        m_entries.push_back(std::move(e));
    }
}

void TemplateRepository::reload()
{
    m_entries.clear();
    m_problems.clear();
    for (const QFileInfo& d : QDir(m_builtInRoot).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        TemplateEntry e;
        QStringList errors;
        if (!TemplatePackage::loadDirectory(d.absoluteFilePath(), &e.package, &errors))
        {
            m_problems.push_back({ d.absoluteFilePath(), errors });
            continue;
        }
        e.origin = TemplateOrigin::BuiltIn;
        e.path = d.absoluteFilePath();
        m_entries.push_back(std::move(e));
    }
    const QString root = userRoot();
    scanDir(root, TemplateOrigin::User);
    scanDir(root + QStringLiteral("/imported"), TemplateOrigin::Imported);
    scanDir(root + QStringLiteral("/custom"), TemplateOrigin::Customized);
    scanDir(root + QStringLiteral("/versions"), TemplateOrigin::Archived);
    m_entries.insert(m_entries.end(), m_moduleEntries.begin(), m_moduleEntries.end());
}

bool TemplateRepository::registerModulePackage(const QString& path, const QString& moduleId, QString* error)
{
    TemplateEntry e;
    QStringList errors;
    const bool ok = QFileInfo(path).isDir() ? TemplatePackage::loadDirectory(path, &e.package, &errors) : TemplatePackage::loadFile(path, &e.package, &errors);
    if (!ok)
    {
        if (error) *error = errors.join(QStringLiteral(" ; "));
        m_problems.push_back({ path, errors });
        return false;
    }
    e.origin = TemplateOrigin::Module;
    e.path = QFileInfo(path).absoluteFilePath();
    e.moduleId = moduleId;
    m_moduleEntries.push_back(e);
    m_entries.push_back(e);
    return true;
}

void TemplateRepository::unregisterModule(const QString& moduleId)
{
    auto drop = [&](std::vector<TemplateEntry>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](const TemplateEntry& e) { return e.origin == TemplateOrigin::Module && e.moduleId == moduleId; }), v.end());
    };
    drop(m_moduleEntries);
    drop(m_entries);
}

const TemplateEntry* TemplateRepository::entryByKey(const QString& key) const
{
    for (const auto& e : m_entries)
        if (e.key() == key) return &e;
    return nullptr;
}

const TemplateEntry* TemplateRepository::effective(const QString& id) const
{
    static const TemplateOrigin priority[] = { TemplateOrigin::Customized, TemplateOrigin::Imported, TemplateOrigin::User, TemplateOrigin::Module,
                                               TemplateOrigin::BuiltIn };
    for (TemplateOrigin o : priority)
        for (const auto& e : m_entries)
            if (e.origin == o && e.package.manifest().id == id) return &e;
    return nullptr;
}

std::vector<const TemplateEntry*> TemplateRepository::effectiveEntries(const QString& dataSchema) const
{
    std::vector<const TemplateEntry*> out;
    QStringList seen;
    for (const auto& e : m_entries)
    {
        const QString& id = e.package.manifest().id;
        if (e.origin == TemplateOrigin::Archived || seen.contains(id)) continue;
        seen << id;
        const auto* eff = effective(id);
        if (eff && (dataSchema.isEmpty() || eff->package.manifest().dataSchema == dataSchema)) out.push_back(eff);
    }
    return out;
}

bool TemplateRepository::archive(const QString& file)
{
    const QString dir = userRoot() + QStringLiteral("/versions");
    QDir().mkpath(dir);
    const QString target = QStringLiteral("%1/%2-%3%4")
                               .arg(dir, QFileInfo(file).completeBaseName(), QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")),
                                    QLatin1String(kPackageExtension));
    return QFile::rename(file, target);
}

bool TemplateRepository::importPackage(const QString& path, QString* key, QStringList* errors)
{
    TemplatePackage pkg;
    if (!TemplatePackage::loadFile(path, &pkg, errors)) return false;   // validation complète (sécurité comprise)
    const QString dir = userRoot() + QStringLiteral("/imported");
    QDir().mkpath(dir);
    // Version(s) déjà importée(s) du même identifiant : archivées, jamais écrasées.
    for (const auto& e : m_entries)
        if (e.origin == TemplateOrigin::Imported && e.package.manifest().id == pkg.manifest().id) archive(e.path);
    const QString target = QStringLiteral("%1/%2-%3%4").arg(dir, pkg.manifest().id, pkg.manifest().version, QLatin1String(kPackageExtension));
    if (QFileInfo::exists(target)) archive(target);
    QString error;
    if (!pkg.saveFile(target, &error))
    {
        if (errors) *errors = { error };
        return false;
    }
    QJsonObject index;
    {
        QFile f(importIndexPath());
        if (f.open(QIODevice::ReadOnly)) index = QJsonDocument::fromJson(f.readAll()).object();
    }
    index.insert(QFileInfo(target).fileName(), QJsonObject { { "hash", pkg.contentHash() },
                                                             { "source", QFileInfo(path).absoluteFilePath() },
                                                             { "imported", QDateTime::currentDateTime().toString(Qt::ISODate) } });
    QSaveFile f(importIndexPath());
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(index).toJson()), f.commit();
    reload();
    if (key) *key = templateOriginName(TemplateOrigin::Imported) + QLatin1Char(':') + QFileInfo(target).absoluteFilePath();
    return true;
}

bool TemplateRepository::exportPackage(const QString& key, const QString& path, QString* error) const
{
    const auto* e = entryByKey(key);
    if (!e)
    {
        if (error) *error = QStringLiteral("template introuvable");
        return false;
    }
    return e->package.saveFile(path, error);
}

bool TemplateRepository::saveCustomized(const QString& baseKey, const TemplatePackage& edited, QString* key, QStringList* errors)
{
    const auto* base = entryByKey(baseKey);
    if (!base)
    {
        if (errors) *errors = { QStringLiteral("template d'origine introuvable") };
        return false;
    }
    TemplatePackage pkg = edited;
    const auto& bm = base->package.manifest();
    pkg.manifest().id = bm.id;   // la personnalisation remplace le paquet d'origine pour ce même identifiant
    pkg.manifest().basedOn = bm.basedOn.isEmpty() ? bm.id + QLatin1Char('@') + bm.version : bm.basedOn;
    pkg.manifest().baseHash = bm.baseHash.isEmpty() ? base->package.contentHash() : bm.baseHash;
    const QStringList errs = pkg.validate();
    if (!errs.isEmpty())
    {
        if (errors) *errors = errs;
        return false;
    }
    const QString target = userRoot() + QStringLiteral("/custom/") + bm.id + QLatin1String(kPackageExtension);
    QDir().mkpath(QFileInfo(target).absolutePath());
    if (QFileInfo::exists(target)) archive(target);
    QString error;
    if (!pkg.saveFile(target, &error))
    {
        if (errors) *errors = { error };
        return false;
    }
    reload();
    if (key) *key = templateOriginName(TemplateOrigin::Customized) + QLatin1Char(':') + QFileInfo(target).absoluteFilePath();
    return true;
}

bool TemplateRepository::remove(const QString& key, QString* error)
{
    const auto* e = entryByKey(key);
    if (!e || e->origin == TemplateOrigin::BuiltIn || e->origin == TemplateOrigin::Module)
    {
        if (error) *error = e ? QStringLiteral("un template %1 ne se supprime pas").arg(templateOriginName(e->origin)) : QStringLiteral("template introuvable");
        return false;
    }
    if (!QFile::remove(e->path))
    {
        if (error) *error = QStringLiteral("suppression impossible : %1").arg(e->path);
        return false;
    }
    reload();
    return true;
}

} // namespace TSA::Templates
