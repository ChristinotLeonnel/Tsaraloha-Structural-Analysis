#include "TopologySettings.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace TSA::Topology
{

namespace
{
constexpr std::array<EntityFamily, 8> kFamilies{ EntityFamily::Node,  EntityFamily::Beam, EntityFamily::Column,
                                                 EntityFamily::Truss, EntityFamily::Cable, EntityFamily::Slab,
                                                 EntityFamily::Wall,  EntityFamily::Foundation };

void addWarning(std::string* w, const std::string& text)
{
    if (!w) return;
    if (!w->empty()) *w += "\n";
    *w += text;
}

QJsonObject formatToJson(const LabelFormat& f)
{
    QJsonObject o;
    o["prefix"] = QString::fromStdString(f.prefix);
    o["pattern"] = QString::fromStdString(f.pattern);
    o["width"] = f.width;
    o["start"] = f.start;
    o["increment"] = f.increment;
    return o;
}

LabelFormat formatFromJson(const QJsonObject& o, const LabelFormat& def)
{
    LabelFormat f = def;
    if (o.contains("prefix")) f.prefix = o["prefix"].toString().toStdString();
    if (o.contains("pattern")) f.pattern = o["pattern"].toString().toStdString();
    f.width = std::clamp(o["width"].toInt(def.width), 0, 9);
    f.start = std::max(0, o["start"].toInt(def.start));
    f.increment = std::max(1, o["increment"].toInt(def.increment));
    return f;
}

QJsonObject orderToJson(const AxisOrder& a)
{
    QJsonObject o;
    o["axes"] = QString::fromStdString(a.code());
    o["descX"] = a.descending[0];
    o["descY"] = a.descending[1];
    o["descZ"] = a.descending[2];
    return o;
}

AxisOrder orderFromJson(const QJsonObject& o, const AxisOrder& def, std::string* warnings)
{
    AxisOrder a = def;
    if (o.contains("axes") && !AxisOrder::fromCode(o["axes"].toString().toStdString(), a))
    {
        a = def;
        addWarning(warnings, "Ordre des axes invalide : valeur par défaut utilisée.");
    }
    a.descending = { o["descX"].toBool(def.descending[0]), o["descY"].toBool(def.descending[1]),
                     o["descZ"].toBool(def.descending[2]) };
    return a;
}
} // namespace

const char* familyKey(EntityFamily f)
{
    switch (f)
    {
    case EntityFamily::Node: return "node";
    case EntityFamily::Beam: return "beam";
    case EntityFamily::Column: return "column";
    case EntityFamily::Truss: return "truss";
    case EntityFamily::Cable: return "cable";
    case EntityFamily::Slab: return "slab";
    case EntityFamily::Wall: return "wall";
    case EntityFamily::Foundation: return "foundation";
    }
    return "node";
}

std::string familyDisplayName(EntityFamily f)
{
    switch (f)
    {
    case EntityFamily::Node: return "Nœud";
    case EntityFamily::Beam: return "Poutre / barre";
    case EntityFamily::Column: return "Poteau";
    case EntityFamily::Truss: return "Treillis";
    case EntityFamily::Cable: return "Câble";
    case EntityFamily::Slab: return "Dalle";
    case EntityFamily::Wall: return "Voile";
    case EntityFamily::Foundation: return "Fondation";
    }
    return {};
}

bool familyFromKey(const std::string& key, EntityFamily& out)
{
    for (EntityFamily f : kFamilies)
    {
        if (key == familyKey(f))
        {
            out = f;
            return true;
        }
    }
    return false;
}

bool AxisOrder::fromCode(const std::string& code, AxisOrder& out)
{
    if (code.size() != 3) return false;
    std::string sorted = code;
    std::sort(sorted.begin(), sorted.end());
    if (sorted != "XYZ") return false;
    out.axes = { code[0], code[1], code[2] };
    return true;
}

TopologySettings TopologySettings::defaults()
{
    TopologySettings s;
    // Préfixes historiques des formattedName() du modèle.
    s.elements.prefixes = { { EntityFamily::Beam, "B" },  { EntityFamily::Column, "C" }, { EntityFamily::Truss, "TR" },
                            { EntityFamily::Cable, "K" }, { EntityFamily::Slab, "S" },   { EntityFamily::Wall, "W" },
                            { EntityFamily::Foundation, "F" } };
    return s;
}

std::string TopologySettings::elementPrefix(EntityFamily f) const
{
    auto it = elements.prefixes.find(f);
    if (it != elements.prefixes.end()) return it->second;
    const TopologySettings d = defaults();
    auto dit = d.elements.prefixes.find(f);
    return dit != d.elements.prefixes.end() ? dit->second : std::string();
}

std::string TopologySettings::toJson() const
{
    QJsonObject root;
    root["schema"] = kSchemaVersion;
    root["scope"] = scope == NumberingScope::Selection ? "selection" : "project";
    root["showNodeLabels"] = showNodeLabels;

    QJsonObject n;
    n["strategy"] = QString::fromStdString(nodes.strategy);
    n["order"] = orderToJson(nodes.order);
    n["tolerance"] = nodes.tolerance;
    n["startNodeId"] = nodes.startNodeId;
    n["restartPerLayer"] = nodes.restartPerLayer;
    n["preserveCustomNames"] = nodes.preserveCustomNames;
    n["format"] = formatToJson(nodes.format);
    root["nodes"] = n;

    QJsonObject e;
    e["strategy"] = QString::fromStdString(elements.strategy);
    e["sharedSequence"] = elements.sharedSequence;
    e["order"] = orderToJson(elements.order);
    e["tolerance"] = elements.tolerance;
    e["preserveCustomNames"] = elements.preserveCustomNames;
    e["format"] = formatToJson(elements.format);
    QJsonObject prefixes;
    for (const auto& [family, prefix] : elements.prefixes) prefixes[familyKey(family)] = QString::fromStdString(prefix);
    e["prefixes"] = prefixes;
    root["elements"] = e;

    return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

TopologySettings TopologySettings::fromJson(const std::string& json, std::string* warnings)
{
    TopologySettings s = defaults();
    if (json.empty()) return s; // ancien projet : aucun chunk TOPO
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        addWarning(warnings, "Paramètres de topologie illisibles : valeurs par défaut utilisées.");
        return s;
    }
    const QJsonObject root = doc.object();
    if (root["schema"].toInt(0) > kSchemaVersion)
        addWarning(warnings, "Paramètres de topologie d'une version plus récente de TSA : champs inconnus ignorés.");

    s.scope = root["scope"].toString() == "selection" ? NumberingScope::Selection : NumberingScope::Project;
    s.showNodeLabels = root["showNodeLabels"].toBool(false);

    const QJsonObject n = root["nodes"].toObject();
    if (n.contains("strategy")) s.nodes.strategy = n["strategy"].toString().toStdString();
    s.nodes.order = orderFromJson(n["order"].toObject(), s.nodes.order, warnings);
    s.nodes.tolerance = std::max(1e-9, n["tolerance"].toDouble(s.nodes.tolerance));
    s.nodes.startNodeId = std::max(0, n["startNodeId"].toInt(0));
    s.nodes.restartPerLayer = n["restartPerLayer"].toBool(false);
    s.nodes.preserveCustomNames = n["preserveCustomNames"].toBool(true);
    s.nodes.format = formatFromJson(n["format"].toObject(), s.nodes.format);

    const QJsonObject e = root["elements"].toObject();
    if (e.contains("strategy")) s.elements.strategy = e["strategy"].toString().toStdString();
    s.elements.sharedSequence = e["sharedSequence"].toBool(false);
    s.elements.order = orderFromJson(e["order"].toObject(), s.elements.order, warnings);
    s.elements.tolerance = std::max(1e-9, e["tolerance"].toDouble(s.elements.tolerance));
    s.elements.preserveCustomNames = e["preserveCustomNames"].toBool(true);
    s.elements.format = formatFromJson(e["format"].toObject(), s.elements.format);
    const QJsonObject prefixes = e["prefixes"].toObject();
    for (auto it = prefixes.begin(); it != prefixes.end(); ++it)
    {
        EntityFamily f;
        if (familyFromKey(it.key().toStdString(), f) && f != EntityFamily::Node)
            s.elements.prefixes[f] = it.value().toString().toStdString();
    }
    return s;
}

} // namespace TSA::Topology
