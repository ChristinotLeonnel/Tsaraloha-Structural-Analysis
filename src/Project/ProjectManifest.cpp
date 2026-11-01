#include "ProjectManifest.h"

#include "../Model/Model.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <map>
#include <set>

namespace TSA::Project
{
namespace
{
QString q(const std::string& s) { return QString::fromStdString(s); }
std::string s(const QJsonValue& v) { return v.toString().toStdString(); }

/// SHA-256 hexadécimal du fichier (lecture par blocs) ; vide si illisible.
std::string fileSha256(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return hash.result().toHex().toStdString();
}

std::string relativeTo(const std::string& projectFile, const QString& absolute)
{
    if (projectFile.empty()) return {};
    const QDir dir = QFileInfo(q(projectFile)).absoluteDir();
    const QString rel = dir.relativeFilePath(absolute);
    // Autre lecteur (Windows) : relativeFilePath rend le chemin absolu, pas un chemin relatif.
    return QDir::isRelativePath(rel) ? QDir::fromNativeSeparators(rel).toStdString() : std::string();
}

bool samePath(const LinkedFile& l, const std::string& path)
{
    const QString p = QDir::cleanPath(QFileInfo(q(path)).absoluteFilePath());
    return (!l.absolutePath.empty() && QDir::cleanPath(q(l.absolutePath)).compare(p, Qt::CaseInsensitive) == 0) || l.relativePath == path;
}
} // namespace

std::string linkRoleLabel(const std::string& role)
{
    static const std::map<std::string, std::string> labels {
        { "import-ifc", "Source de l'import IFC" }, { "import-tsa3d", "Source de l'import TSA3D" },
        { "export-ifc", "Export IFC" },             { "export-tsa3d", "Export TSA3D" },
        { "texture", "Texture de matériau" },       { "attachment", "Document joint" } };
    const auto it = labels.find(role);
    return it != labels.end() ? it->second : role;
}

bool ProjectManifest::registerLink(const std::string& role, const std::string& absolutePath, const std::string& projectFile)
{
    const QFileInfo info(q(absolutePath));
    if (!info.exists() || !info.isFile()) return false;
    LinkedFile l;
    l.role = role;
    l.absolutePath = QDir::cleanPath(info.absoluteFilePath()).toStdString();
    l.relativePath = relativeTo(projectFile, info.absoluteFilePath());
    l.size = info.size();
    l.sha256 = fileSha256(info.absoluteFilePath());
    l.modified = info.lastModified().toUTC().toString(Qt::ISODate).toStdString();
    for (auto& existing : m_links)
        if (existing.role == role && samePath(existing, absolutePath))
        {
            existing = l;
            return true;
        }
    m_links.push_back(std::move(l));
    return true;
}

bool ProjectManifest::removeLink(const std::string& role, const std::string& path)
{
    const auto before = m_links.size();
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                 [&](const LinkedFile& l) { return l.role == role && samePath(l, path); }),
                  m_links.end());
    return m_links.size() != before;
}

void ProjectManifest::updateLinkPaths(const std::string& projectFile, bool keepAbsolutePaths)
{
    const std::vector<LinkCheck> checks = checkLinks(projectFile);   // emplacement réel avant de changer les chemins
    for (std::size_t i = 0; i < m_links.size(); ++i)
    {
        LinkedFile& l = m_links[i];
        const std::string located = checks[i].status != LinkStatus::Missing ? checks[i].resolvedPath : l.absolutePath;
        if (!located.empty())
        {
            const std::string rel = relativeTo(projectFile, QFileInfo(q(located)).absoluteFilePath());
            if (!rel.empty()) l.relativePath = rel;
            if (keepAbsolutePaths) l.absolutePath = QDir::cleanPath(QFileInfo(q(located)).absoluteFilePath()).toStdString();
        }
        if (!keepAbsolutePaths) l.absolutePath.clear();
    }
}

std::vector<LinkCheck> ProjectManifest::checkLinks(const std::string& projectFile) const
{
    std::vector<LinkCheck> out;
    const QDir dir = projectFile.empty() ? QDir() : QFileInfo(q(projectFile)).absoluteDir();
    for (const LinkedFile& l : m_links)
    {
        LinkCheck c;
        c.link = l;
        QString found;
        if (!projectFile.empty() && !l.relativePath.empty() && QFileInfo(dir.filePath(q(l.relativePath))).isFile())
            found = dir.filePath(q(l.relativePath));
        else if (!l.absolutePath.empty() && QFileInfo(q(l.absolutePath)).isFile())
            found = q(l.absolutePath);
        if (found.isEmpty())
            c.status = LinkStatus::Missing;
        else
        {
            c.resolvedPath = QDir::cleanPath(QFileInfo(found).absoluteFilePath()).toStdString();
            const QFileInfo info(found);
            c.status = (info.size() == l.size && fileSha256(found) == l.sha256) ? LinkStatus::Present : LinkStatus::Modified;
        }
        out.push_back(std::move(c));
    }
    return out;
}

void ProjectManifest::collectLibraries(const TSA::Model::Model& model)
{
    std::map<std::pair<std::string, std::string>, std::set<std::string>> used;
    for (const auto& [key, ref] : model.definitionReferences())
        if (ref.isValid()) used[{ ref.libraryId, ref.libraryVersion.toString() }].insert(ref.definitionId);
    m_libraries.clear();
    for (const auto& [lib, defs] : used)
        m_libraries.push_back({ lib.first, lib.second, std::vector<std::string>(defs.begin(), defs.end()) });
}

std::vector<LibraryCheck> ProjectManifest::checkLibraries(const std::vector<std::pair<std::string, std::string>>& installed) const
{
    std::vector<LibraryCheck> out;
    for (const auto& req : m_libraries)
    {
        LibraryCheck c;
        c.requirement = req;
        for (const auto& [id, version] : installed)
            if (id == req.id)
            {
                c.installed = true;
                c.installedVersion = version;
                if (version == req.version) break;
            }
        out.push_back(std::move(c));
    }
    return out;
}

void ProjectManifest::recordSave(const SaveRecord& record)
{
    m_saves.push_back(record);
    while (m_saves.size() > kMaxSaves) m_saves.pop_front();
}

bool ProjectManifest::empty() const noexcept
{
    return m_links.empty() && m_libraries.empty() && m_saves.empty() && m_properties == ProjectProperties {};
}

void ProjectManifest::clear()
{
    *this = ProjectManifest {};
}

std::string ProjectManifest::toJson() const
{
    QJsonArray links, libraries, saves;
    for (const auto& l : m_links)
        links.append(QJsonObject { { "role", q(l.role) }, { "relativePath", q(l.relativePath) }, { "absolutePath", q(l.absolutePath) },
                                   { "size", static_cast<double>(l.size) }, { "sha256", q(l.sha256) }, { "modified", q(l.modified) } });
    for (const auto& lib : m_libraries)
    {
        QJsonArray defs;
        for (const auto& d : lib.definitions) defs.append(q(d));
        libraries.append(QJsonObject { { "id", q(lib.id) }, { "version", q(lib.version) }, { "definitions", defs } });
    }
    for (const auto& r : m_saves)
        saves.append(QJsonObject { { "timestamp", q(r.timestamp) }, { "author", q(r.author) }, { "application", q(r.application) },
                                   { "format", q(r.format) } });
    const QJsonObject props { { "description", q(m_properties.description) }, { "client", q(m_properties.client) },
                              { "reference", q(m_properties.reference) }, { "phase", q(m_properties.phase) } };
    return QJsonDocument(QJsonObject { { "schema", 1 }, { "properties", props }, { "libraries", libraries }, { "links", links },
                                       { "saves", saves } })
        .toJson(QJsonDocument::Compact)
        .toStdString();
}

bool ProjectManifest::fromJson(const std::string& json, std::string* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parseError);
    if (!doc.isObject())
    {
        if (error) *error = "manifeste illisible : " + parseError.errorString().toStdString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value("schema").toInt(0) != 1)
    {
        if (error) *error = "manifeste : version " + std::to_string(root.value("schema").toInt(0)) + " inconnue";
        return false;
    }
    ProjectManifest m;
    const QJsonObject p = root.value("properties").toObject();
    m.m_properties = { s(p.value("description")), s(p.value("client")), s(p.value("reference")), s(p.value("phase")) };
    for (const QJsonValue& v : root.value("libraries").toArray())
    {
        const QJsonObject o = v.toObject();
        LibraryRequirement lib { s(o.value("id")), s(o.value("version")), {} };
        for (const QJsonValue& d : o.value("definitions").toArray()) lib.definitions.push_back(s(d));
        m.m_libraries.push_back(std::move(lib));
    }
    for (const QJsonValue& v : root.value("links").toArray())
    {
        const QJsonObject o = v.toObject();
        m.m_links.push_back({ s(o.value("role")), s(o.value("relativePath")), s(o.value("absolutePath")),
                              static_cast<std::int64_t>(o.value("size").toDouble()), s(o.value("sha256")), s(o.value("modified")) });
    }
    for (const QJsonValue& v : root.value("saves").toArray())
    {
        const QJsonObject o = v.toObject();
        m.m_saves.push_back({ s(o.value("timestamp")), s(o.value("author")), s(o.value("application")), s(o.value("format")) });
    }
    while (m.m_saves.size() > kMaxSaves) m.m_saves.pop_front();
    *this = std::move(m);
    return true;
}

} // namespace TSA::Project
