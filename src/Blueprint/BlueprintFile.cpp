#include "BlueprintFile.h"

#include "BlueprintRuntime.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <map>

namespace TSA::Blueprint
{

namespace
{
QJsonValue encode(const Value& v)
{
    QJsonObject o;
    if (const auto* b = std::get_if<bool>(&v)) o["bool"] = *b;
    else if (const auto* i = std::get_if<long long>(&v)) o["int"] = static_cast<double>(*i);
    else if (const auto* d = std::get_if<double>(&v)) o["real"] = *d;
    else if (const auto* t = std::get_if<std::string>(&v)) o["text"] = QString::fromStdString(*t);
    else if (const auto* p = std::get_if<Point3>(&v)) o["point"] = QJsonArray { (*p)[0], (*p)[1], (*p)[2] };
    else if (const auto* ids = std::get_if<std::vector<int>>(&v))
    {
        QJsonArray a;
        for (int id : *ids) a.append(id);
        o["ids"] = a;
    }
    else return QJsonValue();   // objet ou valeur vide : jamais enregistré
    return o;
}

Value decode(const QJsonValue& j)
{
    const QJsonObject o = j.toObject();
    if (o.contains("bool")) return o["bool"].toBool();
    if (o.contains("int")) return static_cast<long long>(o["int"].toDouble());
    if (o.contains("real")) return o["real"].toDouble();
    if (o.contains("text")) return o["text"].toString().toStdString();
    if (o.contains("point"))
    {
        const QJsonArray a = o["point"].toArray();
        return Point3 { a.at(0).toDouble(), a.at(1).toDouble(), a.at(2).toDouble() };
    }
    if (o.contains("ids"))
    {
        std::vector<int> ids;
        for (const auto& x : o["ids"].toArray()) ids.push_back(x.toInt());
        return ids;
    }
    return {};
}

/// Version enregistrée d'un nœud : la sienne si connue, sinon celle de la bibliothèque (version courante).
int stampedVersion(const NodeInstance& n)
{
    if (n.version > 0) return n.version;
    const auto* d = NodeLibrary::standard().find(n.type);
    return d ? d->version : 1;
}

QJsonObject nodeJson(const NodeInstance& n)
{
    QJsonObject jn;
    jn["id"] = n.id;
    jn["type"] = QString::fromStdString(n.type);
    jn["v"] = stampedVersion(n);
    jn["x"] = n.x;
    jn["y"] = n.y;
    QJsonObject values;
    for (const auto& [pin, v] : n.values)
    {
        const QJsonValue e = encode(v);
        if (!e.isNull()) values[QString::fromStdString(pin)] = e;
    }
    if (!values.isEmpty()) jn["values"] = values;
    return jn;
}

NodeInstance nodeFromJson(const QJsonObject& jn)
{
    NodeInstance n;
    n.id = jn["id"].toInt();
    n.type = jn["type"].toString().toStdString();
    n.version = jn.contains("v") ? jn["v"].toInt() : 1;
    n.x = jn["x"].toDouble();
    n.y = jn["y"].toDouble();
    const QJsonObject values = jn["values"].toObject();
    for (auto it = values.begin(); it != values.end(); ++it) n.values[it.key().toStdString()] = decode(it.value());
    return n;
}

QJsonObject linkJson(const Link& l)
{
    return QJsonObject { { "from", l.fromNode }, { "out", QString::fromStdString(l.fromPin) }, { "to", l.toNode }, { "in", QString::fromStdString(l.toPin) } };
}

Link linkFromJson(const QJsonObject& jl)
{
    return { jl["from"].toInt(), jl["out"].toString().toStdString(), jl["to"].toInt(), jl["in"].toString().toStdString() };
}

QJsonObject commentJson(const Comment& c)
{
    return QJsonObject { { "id", c.id }, { "x", c.x }, { "y", c.y }, { "w", c.width }, { "h", c.height },
                         { "text", QString::fromStdString(c.text) }, { "color", QString::fromStdString(c.color) } };
}

Comment commentFromJson(const QJsonObject& jc)
{
    Comment c;
    c.id = jc["id"].toInt();
    c.x = jc["x"].toDouble();
    c.y = jc["y"].toDouble();
    c.width = jc["w"].toDouble(400.0);
    c.height = jc["h"].toDouble(240.0);
    c.text = jc["text"].toString().toStdString();
    c.color = jc["color"].toString(QStringLiteral("#546E7A")).toStdString();
    return c;
}
} // namespace

QByteArray toJson(const Graph& g)
{
    QJsonObject root;
    root["format"] = "tsbp";
    root["version"] = kFileVersion;
    root["name"] = QString::fromStdString(g.name);
    root["description"] = QString::fromStdString(g.description);
    QJsonArray nodes;
    for (const auto& [id, n] : g.nodes()) nodes.append(nodeJson(n));
    root["nodes"] = nodes;
    QJsonArray links;
    for (const auto& l : g.links()) links.append(linkJson(l));
    root["links"] = links;
    QJsonArray comments;
    for (const auto& [id, c] : g.comments()) comments.append(commentJson(c));
    if (!comments.isEmpty()) root["comments"] = comments;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool fromJson(const QByteArray& json, Graph& g, QString* error)
{
    auto fail = [error](const QString& m) {
        if (error) *error = m;
        return false;
    };
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) return fail(QStringLiteral("JSON invalide : %1").arg(pe.errorString()));
    const QJsonObject root = doc.object();
    if (root["format"].toString() != QLatin1String("tsbp")) return fail(QStringLiteral("ce fichier n'est pas un Blueprint (.tsbp)"));
    const int version = root["version"].toInt();
    if (version < 1 || version > kFileVersion)
        return fail(QStringLiteral("version de Blueprint %1 non prise en charge (maximum %2)").arg(version).arg(kFileVersion));

    Graph loaded;
    loaded.name = root["name"].toString().toStdString();
    loaded.description = root["description"].toString().toStdString();
    for (const auto& x : root["nodes"].toArray())
    {
        const NodeInstance n = nodeFromJson(x.toObject());
        if (!loaded.insertNode(n)) return fail(QStringLiteral("identifiant de nœud invalide ou en double : %1").arg(n.id));
    }
    for (const auto& x : root["links"].toArray()) loaded.addLink(linkFromJson(x.toObject()));
    for (const auto& x : root["comments"].toArray()) loaded.addComment(commentFromJson(x.toObject()));
    g = std::move(loaded);
    return true;
}

bool saveFile(const Graph& g, const QString& path, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(toJson(g)) < 0 || !f.commit())
    {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool loadFile(const QString& path, Graph& g, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = f.errorString();
        return false;
    }
    return fromJson(f.readAll(), g, error);
}

QByteArray fragmentToJson(const Graph& g, const std::set<int>& nodes, const std::set<int>& comments)
{
    QJsonObject root;
    root["format"] = "tsbp-fragment";
    root["version"] = kFileVersion;
    QJsonArray jn, jl, jc;
    for (int id : nodes)
        if (const auto* n = g.node(id)) jn.append(nodeJson(*n));
    for (const auto& l : g.links())
        if (nodes.count(l.fromNode) && nodes.count(l.toNode)) jl.append(linkJson(l));
    for (int id : comments)
        if (const auto* c = g.comment(id)) jc.append(commentJson(*c));
    root["nodes"] = jn;
    root["links"] = jl;
    root["comments"] = jc;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool pasteFragment(Graph& g, const QByteArray& json, double dx, double dy, std::vector<int>* newNodes, std::vector<int>* newComments,
                   QString* error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();
    if (!doc.isObject() || root["format"].toString() != QLatin1String("tsbp-fragment"))
    {
        if (error) *error = QStringLiteral("le presse-papiers ne contient pas de nœuds Blueprint");
        return false;
    }
    std::map<int, int> remap;
    for (const auto& x : root["nodes"].toArray())
    {
        NodeInstance n = nodeFromJson(x.toObject());
        const int id = g.addNode(n.type, n.x + dx, n.y + dy);
        NodeInstance* created = g.node(id);
        created->values = n.values;
        created->version = n.version;
        remap[n.id] = id;
        if (newNodes) newNodes->push_back(id);
    }
    for (const auto& x : root["links"].toArray())
    {
        Link l = linkFromJson(x.toObject());
        if (!remap.count(l.fromNode) || !remap.count(l.toNode)) continue;
        l.fromNode = remap[l.fromNode];
        l.toNode = remap[l.toNode];
        g.addLink(l);
    }
    for (const auto& x : root["comments"].toArray())
    {
        Comment c = commentFromJson(x.toObject());
        c.id = 0;
        c.x += dx, c.y += dy;
        const int id = g.addComment(c);
        if (newComments) newComments->push_back(id);
    }
    return true;
}

} // namespace TSA::Blueprint
