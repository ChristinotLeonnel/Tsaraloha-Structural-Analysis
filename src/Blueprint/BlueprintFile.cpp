#include "BlueprintFile.h"

#include "BlueprintRuntime.h"
#include "BlueprintSubgraph.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
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

/// Version enregistrée d'un nœud : la sienne si connue, sinon celle de la définition de sous-graphe (instance) ou de la
/// bibliothèque (version courante).
int stampedVersion(const NodeInstance& n, const Graph* owner)
{
    if (n.version > 0) return n.version;
    if (const std::string sub = subgraphOfType(n.type); !sub.empty() && owner)
        if (const SubgraphDefinition* d = owner->subgraph(sub)) return d->version;
    const auto* d = NodeLibrary::standard().find(n.type);
    return d ? d->version : 1;
}

QJsonObject nodeJson(const NodeInstance& n, const Graph* owner)
{
    QJsonObject jn;
    jn["id"] = n.id;
    jn["type"] = QString::fromStdString(n.type);
    jn["v"] = stampedVersion(n, owner);
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

QJsonObject pinJson(const PinSpec& p)
{
    QJsonObject j;
    j["name"] = QString::fromStdString(p.name);
    j["label"] = QString::fromStdString(p.label);
    j["type"] = p.any ? QStringLiteral("any") : QString::fromLatin1(valueTypeId(p.type));
    if (!p.objectType.empty()) j["object"] = QString::fromStdString(p.objectType);
    if (p.quantity != Quantity::None) j["quantity"] = static_cast<int>(p.quantity);
    if (const QJsonValue d = encode(p.defaultValue); !d.isNull()) j["default"] = d;
    if (p.required) j["required"] = true;
    return j;
}

bool pinFromJson(const QJsonObject& j, PinSpec& p, QString* error)
{
    p = PinSpec {};
    p.kind = PinKind::Data;
    p.name = j["name"].toString().toStdString();
    p.label = j["label"].toString().toStdString();
    const std::string type = j["type"].toString().toStdString();
    if (type == "any")
        p.any = true;
    else if (!valueTypeFromId(type, p.type))
    {
        if (error) *error = QStringLiteral("broche « %1 » : type « %2 » inconnu").arg(QString::fromStdString(p.name), QString::fromStdString(type));
        return false;
    }
    p.objectType = j["object"].toString().toStdString();
    p.quantity = static_cast<Quantity>(j["quantity"].toInt(0));
    p.defaultValue = decode(j["default"]);
    p.required = j["required"].toBool(false);
    return true;
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
/// Nœuds, liens, commentaires (et nom, description) d'un graphe ; owner : graphe qui définit les sous-graphes utilisés.
void writeContent(QJsonObject& root, const Graph& g, const Graph* owner)
{
    root["name"] = QString::fromStdString(g.name);
    root["description"] = QString::fromStdString(g.description);
    QJsonArray nodes;
    for (const auto& [id, n] : g.nodes()) nodes.append(nodeJson(n, owner));
    root["nodes"] = nodes;
    QJsonArray links;
    for (const auto& l : g.links()) links.append(linkJson(l));
    root["links"] = links;
    QJsonArray comments;
    for (const auto& [id, c] : g.comments()) comments.append(commentJson(c));
    if (!comments.isEmpty()) root["comments"] = comments;
}

bool readContent(const QJsonObject& root, Graph& g, QString* error)
{
    g.name = root["name"].toString().toStdString();
    g.description = root["description"].toString().toStdString();
    for (const auto& x : root["nodes"].toArray())
    {
        const NodeInstance n = nodeFromJson(x.toObject());
        if (!g.insertNode(n))
        {
            if (error) *error = QStringLiteral("identifiant de nœud invalide ou en double : %1").arg(n.id);
            return false;
        }
    }
    for (const auto& x : root["links"].toArray()) g.addLink(linkFromJson(x.toObject()));
    for (const auto& x : root["comments"].toArray()) g.addComment(commentFromJson(x.toObject()));
    return true;
}

QJsonObject subgraphJson(const SubgraphDefinition& d, const Graph& owner)
{
    QJsonObject j;
    j["id"] = QString::fromStdString(d.id);
    j["name"] = QString::fromStdString(d.name);
    j["description"] = QString::fromStdString(d.description);
    j["category"] = QString::fromStdString(d.category);
    if (!d.documentation.empty()) j["documentation"] = QString::fromStdString(d.documentation);
    j["version"] = d.version;
    QJsonArray in, out;
    for (const auto& p : d.inputs) in.append(pinJson(p));
    for (const auto& p : d.outputs) out.append(pinJson(p));
    j["inputs"] = in;
    j["outputs"] = out;
    QJsonObject body;
    if (d.body) writeContent(body, *d.body, &owner);
    j["graph"] = body;
    return j;
}

bool subgraphFromJson(const QJsonObject& j, SubgraphDefinition& d, QString* error)
{
    d = SubgraphDefinition {};
    d.id = j["id"].toString().toStdString();
    d.name = j["name"].toString().toStdString();
    d.description = j["description"].toString().toStdString();
    d.category = j["category"].toString(QStringLiteral("Sous-graphes")).toStdString();
    d.documentation = j["documentation"].toString().toStdString();
    d.version = j["version"].toInt(1);
    const QString label = QStringLiteral("sous-graphe « %1 »").arg(QString::fromStdString(d.id));
    if (!isValidSubgraphId(d.id))
    {
        if (error) *error = label + QStringLiteral(" : identifiant invalide");
        return false;
    }
    for (int side = 0; side < 2; ++side)
        for (const auto& x : j[side == 0 ? "inputs" : "outputs"].toArray())
        {
            PinSpec p;
            QString why;
            if (!pinFromJson(x.toObject(), p, &why))
            {
                if (error) *error = label + QStringLiteral(" : ") + why;
                return false;
            }
            (side == 0 ? d.inputs : d.outputs).push_back(p);
        }
    auto body = std::make_shared<Graph>();
    QString why;
    if (!readContent(j["graph"].toObject(), *body, &why))
    {
        if (error) *error = label + QStringLiteral(" : ") + why;
        return false;
    }
    d.body = body;
    return true;
}

bool readSubgraphs(const QJsonArray& list, std::vector<SubgraphDefinition>& out, QString* error)
{
    for (const auto& x : list)
    {
        SubgraphDefinition d;
        if (!subgraphFromJson(x.toObject(), d, error)) return false;
        out.push_back(std::move(d));
    }
    return true;
}
} // namespace

QByteArray toJson(const Graph& g)
{
    QJsonObject root;
    root["format"] = "tsbp";
    root["version"] = g.subgraphs.empty() ? 2 : kFileVersion;   // sans sous-graphe : lisible par les versions antérieures
    writeContent(root, g, &g);
    if (!g.subgraphs.empty())
    {
        QJsonArray subs;
        for (const auto& d : g.subgraphs) subs.append(subgraphJson(d, g));
        root["subgraphs"] = subs;
    }
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
    QString why;
    if (!readContent(root, loaded, &why)) return fail(why);
    if (!readSubgraphs(root["subgraphs"].toArray(), loaded.subgraphs, &why)) return fail(why);
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
        if (const auto* n = g.node(id)) jn.append(nodeJson(*n, &g));
    QJsonArray subs;
    for (const auto& sub : usedSubgraphs(g, nodes))
        if (const SubgraphDefinition* d = g.subgraph(sub)) subs.append(subgraphJson(*d, g));
    if (!subs.isEmpty()) root["subgraphs"] = subs;
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
                   QString* error, std::vector<std::string>* notes)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();
    if (!doc.isObject() || root["format"].toString() != QLatin1String("tsbp-fragment"))
    {
        if (error) *error = QStringLiteral("le presse-papiers ne contient pas de nœuds Blueprint");
        return false;
    }
    // Définitions des sous-graphes utilisés par le fragment : ajoutées, ou remplacées par une version plus récente
    std::vector<SubgraphDefinition> defs;
    if (!readSubgraphs(root["subgraphs"].toArray(), defs, error)) return false;
    for (auto& d : defs)
    {
        auto it = std::find_if(g.subgraphs.begin(), g.subgraphs.end(), [&](const SubgraphDefinition& x) { return x.id == d.id; });
        if (it == g.subgraphs.end())
        {
            if (notes) notes->push_back("sous-graphe « " + d.id + " » (version " + std::to_string(d.version) + ") ajouté");
            g.subgraphs.push_back(std::move(d));
        }
        else if (d.version > it->version)
        {
            if (notes)
                notes->push_back("sous-graphe « " + d.id + " » mis à jour de la version " + std::to_string(it->version) + " à la version " + std::to_string(d.version));
            *it = std::move(d);
        }
        else if (!d.sameContent(*it) && notes)
            notes->push_back("sous-graphe « " + d.id + " » : version " + std::to_string(it->version) + " du graphe conservée (fragment en version "
                             + std::to_string(d.version) + ")");
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

bool saveSubgraphFile(const Graph& owner, const std::string& id, const QString& path, QString* error)
{
    const SubgraphDefinition* main = owner.subgraph(id);
    if (!main)
    {
        if (error) *error = QStringLiteral("sous-graphe « %1 » inexistant").arg(QString::fromStdString(id));
        return false;
    }
    // Dépendances (sous-graphes appelés par le corps) d'abord, définition principale en dernier
    std::vector<std::string> order;
    if (main->body)
    {
        Graph probe;
        probe.subgraphs = owner.subgraphs;
        std::set<int> nodes;
        for (const auto& [nid, n] : main->body->nodes()) nodes.insert(probe.addNode(n.type));
        order = usedSubgraphs(probe, nodes);
    }
    order.erase(std::remove(order.begin(), order.end(), id), order.end());
    order.push_back(id);
    QJsonObject root;
    root["format"] = "tsbp-subgraph";
    root["version"] = kSubgraphFileVersion;
    QJsonArray subs;
    for (const auto& sub : order)
        if (const SubgraphDefinition* d = owner.subgraph(sub)) subs.append(subgraphJson(*d, owner));
    root["subgraphs"] = subs;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !f.commit())
    {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool loadSubgraphFile(const QString& path, std::vector<SubgraphDefinition>& definitions, QString* error)
{
    definitions.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = f.errorString();
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    const QJsonObject root = doc.object();
    if (doc.isNull() || !doc.isObject() || root["format"].toString() != QLatin1String("tsbp-subgraph"))
    {
        if (error) *error = QStringLiteral("ce fichier n'est pas un sous-graphe Blueprint (.tsbps)");
        return false;
    }
    const int version = root["version"].toInt();
    if (version < 1 || version > kSubgraphFileVersion)
    {
        if (error) *error = QStringLiteral("version de sous-graphe %1 non prise en charge (maximum %2)").arg(version).arg(kSubgraphFileVersion);
        return false;
    }
    if (!readSubgraphs(root["subgraphs"].toArray(), definitions, error)) return false;
    if (definitions.empty())
    {
        if (error) *error = QStringLiteral("fichier de sous-graphe vide");
        return false;
    }
    return true;
}

} // namespace TSA::Blueprint
