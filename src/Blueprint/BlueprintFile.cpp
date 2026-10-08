#include "BlueprintFile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

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
    else return QJsonValue();
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
} // namespace

QByteArray toJson(const Graph& g)
{
    QJsonObject root;
    root["format"] = "tsbp";
    root["version"] = kFileVersion;
    root["name"] = QString::fromStdString(g.name);
    root["description"] = QString::fromStdString(g.description);
    QJsonArray nodes;
    for (const auto& [id, n] : g.nodes())
    {
        QJsonObject jn;
        jn["id"] = id;
        jn["type"] = QString::fromStdString(n.type);
        jn["x"] = n.x;
        jn["y"] = n.y;
        QJsonObject values;
        for (const auto& [pin, v] : n.values)
        {
            const QJsonValue e = encode(v);
            if (!e.isNull()) values[QString::fromStdString(pin)] = e;
        }
        if (!values.isEmpty()) jn["values"] = values;
        nodes.append(jn);
    }
    root["nodes"] = nodes;
    QJsonArray links;
    for (const auto& l : g.links())
        links.append(QJsonObject { { "from", l.fromNode }, { "out", QString::fromStdString(l.fromPin) },
                                   { "to", l.toNode }, { "in", QString::fromStdString(l.toPin) } });
    root["links"] = links;
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
        const QJsonObject jn = x.toObject();
        NodeInstance n;
        n.id = jn["id"].toInt();
        n.type = jn["type"].toString().toStdString();
        n.x = jn["x"].toDouble();
        n.y = jn["y"].toDouble();
        const QJsonObject values = jn["values"].toObject();
        for (auto it = values.begin(); it != values.end(); ++it) n.values[it.key().toStdString()] = decode(it.value());
        if (!loaded.insertNode(n)) return fail(QStringLiteral("identifiant de nœud invalide ou en double : %1").arg(n.id));
    }
    for (const auto& x : root["links"].toArray())
    {
        const QJsonObject jl = x.toObject();
        loaded.addLink({ jl["from"].toInt(), jl["out"].toString().toStdString(), jl["to"].toInt(), jl["in"].toString().toStdString() });
    }
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

} // namespace TSA::Blueprint
