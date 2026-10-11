#include "MeshProject.h"

#include "../Core/AppPaths.h"
#include "../Model/Model.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace TSA::Mesh
{

namespace
{
template <typename E>
QString enumText(E v, const QStringList& names)
{
    const int i = static_cast<int>(v);
    return i >= 0 && i < names.size() ? names[i] : names.value(0);
}
template <typename E>
E enumFrom(const QJsonValue& v, const QStringList& names, E fallback)
{
    const int i = names.indexOf(v.toString());
    return i >= 0 ? static_cast<E>(i) : fallback;
}
const QStringList kFineness { "veryCoarse", "coarse", "standard", "fine", "veryFine", "custom" };
const QStringList kDistribution { "uniform", "progressive", "symmetric" };
// Énumérations dont la valeur est un code Gmsh : correspondance explicite nom ↔ valeur.
template <typename E>
QString codeText(E v, const std::vector<std::pair<E, const char*>>& table)
{
    for (const auto& [e, n] : table)
        if (e == v) return QLatin1String(n);
    return QLatin1String(table.front().second);
}
template <typename E>
E codeFrom(const QJsonValue& v, const std::vector<std::pair<E, const char*>>& table, E fallback)
{
    for (const auto& [e, n] : table)
        if (v.toString() == QLatin1String(n)) return e;
    return fallback;
}
const std::vector<std::pair<Algorithm2D, const char*>> kAlgo2D { { Algorithm2D::Automatic, "automatic" },
                                                                 { Algorithm2D::MeshAdapt, "meshAdapt" },
                                                                 { Algorithm2D::Delaunay, "delaunay" },
                                                                 { Algorithm2D::FrontalDelaunay, "frontalDelaunay" },
                                                                 { Algorithm2D::FrontalDelaunayQuads, "frontalDelaunayQuads" },
                                                                 { Algorithm2D::PackingParallelograms, "packingParallelograms" },
                                                                 { Algorithm2D::QuasiStructuredQuad, "quasiStructuredQuad" } };
const std::vector<std::pair<Algorithm3D, const char*>> kAlgo3D { { Algorithm3D::Delaunay, "delaunay" }, { Algorithm3D::Frontal, "frontal" }, { Algorithm3D::HXT, "hxt" } };
const std::vector<std::pair<RecombinationAlgorithm, const char*>> kRecomb { { RecombinationAlgorithm::Simple, "simple" },
                                                                            { RecombinationAlgorithm::Blossom, "blossom" },
                                                                            { RecombinationAlgorithm::SimpleFullQuad, "simpleFullQuad" },
                                                                            { RecombinationAlgorithm::BlossomFullQuad, "blossomFullQuad" } };
const QStringList kVolume { "tetrahedra", "hexahedra" };
const QStringList kShape { "triangles", "quadrilaterals" };
const QStringList kZoneKind { "point", "node", "support", "load" };
const QStringList kFeType { "line2", "line3", "tri3", "tri6", "quad4", "quad8", "hex8", "hex20", "tet4", "tet10" };

QJsonObject autoToJson(const AutoRefinement& a) { return { { "enabled", a.enabled }, { "size", a.size }, { "radius", a.radius } }; }
AutoRefinement autoFromJson(const QJsonValue& v, AutoRefinement d)
{
    const QJsonObject o = v.toObject();
    d.enabled = o.value("enabled").toBool(d.enabled);
    d.size = o.value("size").toDouble(d.size);
    d.radius = o.value("radius").toDouble(d.radius);
    return d;
}
QJsonArray strings(const std::vector<std::string>& v)
{
    QJsonArray a;
    for (const auto& s : v) a.append(QString::fromStdString(s));
    return a;
}
std::vector<std::string> stringsFrom(const QJsonValue& v)
{
    std::vector<std::string> out;
    for (const auto& x : v.toArray()) out.push_back(x.toString().toStdString());
    return out;
}
void putOpt(QJsonObject& o, const char* k, const std::optional<double>& v)
{
    if (v) o.insert(QLatin1String(k), *v);
}
void putOpt(QJsonObject& o, const char* k, const std::optional<int>& v)
{
    if (v) o.insert(QLatin1String(k), *v);
}
std::optional<double> optD(const QJsonObject& o, const char* k)
{
    return o.contains(QLatin1String(k)) ? std::optional<double>(o.value(QLatin1String(k)).toDouble()) : std::nullopt;
}
std::optional<int> optI(const QJsonObject& o, const char* k)
{
    return o.contains(QLatin1String(k)) ? std::optional<int>(o.value(QLatin1String(k)).toInt()) : std::nullopt;
}
} // namespace

QJsonObject globalSettingsToJson(const GlobalMeshSettings& g)
{
    return { { "mesh1D", g.mesh1D },
             { "mesh2D", g.mesh2D },
             { "mesh3D", g.mesh3D },
             { "globalSize", g.globalSize },
             { "minSize", g.minSize },
             { "maxSize", g.maxSize },
             { "fineness", enumText(g.fineness, kFineness) },
             { "refinementFactor", g.refinementFactor },
             { "transitionRate", g.transitionRate },
             { "order", static_cast<int>(g.order) },
             { "method", QString::fromStdString(g.method) },
             { "algorithm2D", codeText(g.algorithm2D, kAlgo2D) },
             { "algorithm3D", codeText(g.algorithm3D, kAlgo3D) },
             { "volumeElement", enumText(g.volumeElement, kVolume) },
             { "preferStructured", g.preferStructured },
             { "recombine", g.recombine },
             { "recombination", codeText(g.recombination, kRecomb) },
             { "smoothing", g.smoothing },
             { "optimize", g.optimize },
             { "boundaryElements", g.boundaryElements },
             { "physicalGroups", g.physicalGroups },
             { "geometricTolerance", g.geometricTolerance },
             { "keepCharacteristicPoints", g.keepCharacteristicPoints },
             { "maxElements", g.maxElements },
             { "quality",
               QJsonObject { { "maxAspectRatio", g.quality.maxAspectRatio },
                             { "minAngleDeg", g.quality.minAngleDeg },
                             { "maxAngleDeg", g.quality.maxAngleDeg },
                             { "minScaledJacobian", g.quality.minScaledJacobian },
                             { "maxGrowthRatio", g.quality.maxGrowthRatio },
                             { "minSICN", g.quality.minSICN } } },
             { "nearSupports", autoToJson(g.nearSupports) },
             { "nearPointLoads", autoToJson(g.nearPointLoads) },
             { "nearSectionChanges", autoToJson(g.nearSectionChanges) },
             { "nearOpenings", autoToJson(g.nearOpenings) },
             { "nearConnections", autoToJson(g.nearConnections) } };
}

GlobalMeshSettings globalSettingsFromJson(const QJsonObject& o)
{
    GlobalMeshSettings g;
    g.mesh1D = o.value("mesh1D").toBool(g.mesh1D);
    g.mesh2D = o.value("mesh2D").toBool(g.mesh2D);
    g.mesh3D = o.value("mesh3D").toBool(g.mesh3D);
    g.globalSize = o.value("globalSize").toDouble(g.globalSize);
    g.minSize = o.value("minSize").toDouble(g.minSize);
    g.maxSize = o.value("maxSize").toDouble(g.maxSize);
    g.fineness = enumFrom(o.value("fineness"), kFineness, g.fineness);
    g.refinementFactor = o.value("refinementFactor").toDouble(g.refinementFactor);
    g.transitionRate = o.value("transitionRate").toDouble(g.transitionRate);
    g.order = o.value("order").toInt(1) == 2 ? ElementOrder::Second : ElementOrder::First;
    g.method = o.value("method").toString(QString::fromStdString(g.method)).toStdString();
    g.algorithm2D = codeFrom(o.value("algorithm2D"), kAlgo2D, g.algorithm2D);
    g.algorithm3D = codeFrom(o.value("algorithm3D"), kAlgo3D, g.algorithm3D);
    g.volumeElement = enumFrom(o.value("volumeElement"), kVolume, g.volumeElement);
    g.preferStructured = o.value("preferStructured").toBool(g.preferStructured);
    g.recombine = o.value("recombine").toBool(g.recombine);
    g.recombination = codeFrom(o.value("recombination"), kRecomb, g.recombination);
    g.smoothing = o.value("smoothing").toInt(g.smoothing);
    g.optimize = o.value("optimize").toBool(g.optimize);
    g.boundaryElements = o.value("boundaryElements").toBool(g.boundaryElements);
    g.physicalGroups = o.value("physicalGroups").toBool(g.physicalGroups);
    g.geometricTolerance = o.value("geometricTolerance").toDouble(g.geometricTolerance);
    g.keepCharacteristicPoints = o.value("keepCharacteristicPoints").toBool(g.keepCharacteristicPoints);
    g.maxElements = o.value("maxElements").toInt(g.maxElements);
    const QJsonObject q = o.value("quality").toObject();
    g.quality.maxAspectRatio = q.value("maxAspectRatio").toDouble(g.quality.maxAspectRatio);
    g.quality.minAngleDeg = q.value("minAngleDeg").toDouble(g.quality.minAngleDeg);
    g.quality.maxAngleDeg = q.value("maxAngleDeg").toDouble(g.quality.maxAngleDeg);
    g.quality.minScaledJacobian = q.value("minScaledJacobian").toDouble(g.quality.minScaledJacobian);
    g.quality.maxGrowthRatio = q.value("maxGrowthRatio").toDouble(g.quality.maxGrowthRatio);
    g.quality.minSICN = q.value("minSICN").toDouble(g.quality.minSICN);
    g.nearSupports = autoFromJson(o.value("nearSupports"), g.nearSupports);
    g.nearPointLoads = autoFromJson(o.value("nearPointLoads"), g.nearPointLoads);
    g.nearSectionChanges = autoFromJson(o.value("nearSectionChanges"), g.nearSectionChanges);
    g.nearOpenings = autoFromJson(o.value("nearOpenings"), g.nearOpenings);
    g.nearConnections = autoFromJson(o.value("nearConnections"), g.nearConnections);
    return g;
}

QJsonObject localSettingToJson(const LocalMeshSetting& l)
{
    QJsonObject o { { "name", QString::fromStdString(l.name) }, { "targets", strings(l.targets) }, { "distribution", enumText(l.distribution, kDistribution) },
                    { "progression", l.progression } };
    putOpt(o, "size", l.size);
    putOpt(o, "divisions", l.divisions);
    putOpt(o, "maxLength", l.maxLength);
    putOpt(o, "endSize", l.endSize);
    putOpt(o, "divisionsU", l.divisionsU);
    putOpt(o, "divisionsV", l.divisionsV);
    if (l.shape) o.insert("shape", enumText(*l.shape, kShape));
    if (l.algorithm) o.insert("algorithm", codeText(*l.algorithm, kAlgo2D));
    if (l.structured) o.insert("structured", *l.structured);
    QJsonArray portions;
    for (const auto& p : l.portions) portions.append(QJsonObject { { "from", p.from }, { "to", p.to }, { "size", p.size } });
    if (!portions.isEmpty()) o.insert("portions", portions);
    return o;
}

LocalMeshSetting localSettingFromJson(const QJsonObject& o)
{
    LocalMeshSetting l;
    l.name = o.value("name").toString().toStdString();
    l.targets = stringsFrom(o.value("targets"));
    l.distribution = enumFrom(o.value("distribution"), kDistribution, Distribution::Uniform);
    l.progression = o.value("progression").toDouble(1.2);
    l.size = optD(o, "size");
    l.divisions = optI(o, "divisions");
    l.maxLength = optD(o, "maxLength");
    l.endSize = optD(o, "endSize");
    l.divisionsU = optI(o, "divisionsU");
    l.divisionsV = optI(o, "divisionsV");
    if (o.contains("shape")) l.shape = enumFrom(o.value("shape"), kShape, SurfaceShape::Triangles);
    if (o.contains("algorithm")) l.algorithm = codeFrom(o.value("algorithm"), kAlgo2D, Algorithm2D::Automatic);
    if (o.contains("structured")) l.structured = o.value("structured").toBool();
    for (const auto& v : o.value("portions").toArray())
    {
        const QJsonObject p = v.toObject();
        l.portions.push_back({ p.value("from").toDouble(), p.value("to").toDouble(1), p.value("size").toDouble(0.1) });
    }
    return l;
}

QJsonObject zoneToJson(const RefinementZone& z)
{
    QJsonObject o { { "name", QString::fromStdString(z.name) }, { "kind", enumText(z.kind, kZoneKind) }, { "position", QJsonArray { z.x, z.y, z.z } },
                    { "node", z.nodeId }, { "radius", z.radius }, { "targets", strings(z.targets) } };
    putOpt(o, "size", z.size);
    putOpt(o, "factor", z.factor);
    return o;
}

RefinementZone zoneFromJson(const QJsonObject& o)
{
    RefinementZone z;
    z.name = o.value("name").toString().toStdString();
    z.kind = enumFrom(o.value("kind"), kZoneKind, RefinementZone::Kind::Point);
    const QJsonArray p = o.value("position").toArray();
    z.x = p.at(0).toDouble(), z.y = p.at(1).toDouble(), z.z = p.at(2).toDouble();
    z.nodeId = o.value("node").toInt();
    z.radius = o.value("radius").toDouble(0.5);
    z.size = optD(o, "size");
    z.factor = optD(o, "factor");
    z.targets = stringsFrom(o.value("targets"));
    return z;
}

QJsonObject meshToJson(const FeMesh& m)
{
    // Format compact : nœuds [x, y, z, nœud TSA], éléments [type, source, frontière, n1, n2…].
    QJsonArray nodes, elements, groups, quality;
    QJsonArray sources;
    std::map<std::string, int> sourceIndex;
    auto src = [&](const std::string& s) {
        auto it = sourceIndex.find(s);
        if (it != sourceIndex.end()) return it->second;
        sources.append(QString::fromStdString(s));
        return sourceIndex[s] = int(sourceIndex.size());
    };
    // Correspondance Gmsh (étiquettes des nœuds, éléments, entités d'origine) : tableaux parallèles facultatifs.
    QJsonArray gmshNodes, gmshElements;
    bool anyGmsh = false;
    for (const auto& n : m.nodes)
    {
        nodes.append(QJsonArray { n.x, n.y, n.z, n.tsaNode, src(n.source) });
        gmshNodes.append(double(n.gmshTag));
        anyGmsh |= n.gmshTag != 0;
    }
    for (const auto& e : m.elements)
    {
        QJsonArray a { kFeType.value(static_cast<int>(e.type)), src(e.source), e.boundary ? 1 : 0 };
        for (int id : e.nodes) a.append(id);
        elements.append(a);
        quality.append(e.quality);
        gmshElements.append(QJsonArray { double(e.gmshTag), e.gmshEntityDim, e.gmshEntityTag });
        anyGmsh |= e.gmshTag != 0;
    }
    for (const auto& g : m.groups)
    {
        QJsonArray ids, nids;
        for (int id : g.elements) ids.append(id);
        for (int id : g.nodes) nids.append(id);
        QJsonObject go { { "name", QString::fromStdString(g.name) }, { "dimension", g.dimension }, { "elements", ids } };
        if (!nids.isEmpty()) go.insert("nodes", nids);
        if (g.gmshPhysicalDim >= 0) go.insert("gmshPhysical", QJsonArray { g.gmshPhysicalDim, g.gmshPhysicalTag });
        groups.append(go);
    }
    QJsonObject out { { "sources", sources }, { "nodes", nodes }, { "elements", elements }, { "quality", quality }, { "groups", groups } };
    if (anyGmsh)
    {
        out.insert("gmshNodeTags", gmshNodes);
        out.insert("gmshElements", gmshElements);
    }
    return out;
}

FeMesh meshFromJson(const QJsonObject& o)
{
    FeMesh m;
    std::vector<std::string> sources;
    for (const auto& s : o.value("sources").toArray()) sources.push_back(s.toString().toStdString());
    auto src = [&](int i) { return i >= 0 && i < int(sources.size()) ? sources[i] : std::string(); };
    int id = 0;
    for (const auto& v : o.value("nodes").toArray())
    {
        const QJsonArray a = v.toArray();
        m.nodes.push_back({ ++id, a.at(0).toDouble(), a.at(1).toDouble(), a.at(2).toDouble(), a.at(3).toInt(), src(a.at(4).toInt()) });
    }
    id = 0;
    const QJsonArray quality = o.value("quality").toArray();
    for (const auto& v : o.value("elements").toArray())
    {
        const QJsonArray a = v.toArray();
        FeElement e;
        e.id = ++id;
        // Type inconnu : jamais converti en un autre type. L'élément garde ses nœuds et un type marqué invalide
        // par le nombre de nœuds : l'affichage et les contrôles le signalent (MeshDisplayModel, MeshQuality).
        const qsizetype t = kFeType.indexOf(a.at(0).toString());
        e.type = t >= 0 ? static_cast<FeType>(t) : FeType::Line2;
        e.source = src(a.at(1).toInt());
        if (t < 0) e.source = std::string(kFeUnknownTypePrefix) + " « " + a.at(0).toString().toStdString() + " » : " + e.source;
        e.boundary = a.at(2).toInt() != 0;
        for (int k = 3; k < a.size(); ++k) e.nodes.push_back(a.at(k).toInt());
        e.quality = quality.at(e.id - 1).toDouble(2.0);
        m.elements.push_back(e);
    }
    const QJsonArray gmshNodes = o.value("gmshNodeTags").toArray(), gmshElements = o.value("gmshElements").toArray();
    for (std::size_t i = 0; i < m.nodes.size() && i < std::size_t(gmshNodes.size()); ++i)
        m.nodes[i].gmshTag = std::size_t(gmshNodes.at(qsizetype(i)).toDouble());
    for (std::size_t i = 0; i < m.elements.size() && i < std::size_t(gmshElements.size()); ++i)
    {
        const QJsonArray g = gmshElements.at(qsizetype(i)).toArray();
        m.elements[i].gmshTag = std::size_t(g.at(0).toDouble());
        m.elements[i].gmshEntityDim = g.at(1).toInt(-1);
        m.elements[i].gmshEntityTag = g.at(2).toInt();
    }
    for (const auto& v : o.value("groups").toArray())
    {
        const QJsonObject g = v.toObject();
        FeGroup fg;
        fg.name = g.value("name").toString().toStdString();
        fg.dimension = g.value("dimension").toInt(1);
        for (const auto& e : g.value("elements").toArray()) fg.elements.push_back(e.toInt());
        for (const auto& n : g.value("nodes").toArray()) fg.nodes.push_back(n.toInt());
        if (const QJsonArray ph = g.value("gmshPhysical").toArray(); ph.size() == 2)
        {
            fg.gmshPhysicalDim = ph.at(0).toInt(-1);
            fg.gmshPhysicalTag = ph.at(1).toInt();
        }
        m.groups.push_back(fg);
    }
    return m;
}

QJsonObject MeshProject::toJson(bool includeMesh) const
{
    QJsonArray locals, zones;
    for (const auto& l : settings.locals) locals.append(localSettingToJson(l));
    for (const auto& z : settings.zones) zones.append(zoneToJson(z));
    QJsonObject o { { "schema", QLatin1String(kMeshProjectSchema) },
                    { "settings",
                      QJsonObject { { "global", globalSettingsToJson(settings.global) },
                                    { "locals", locals },
                                    { "zones", zones },
                                    { "profile", QString::fromStdString(settings.profile) } } } };
    if (hasMesh)
    {
        QJsonArray interfaces;
        for (const auto& i : record.interfaces)
            interfaces.append(QJsonObject { { "a", QString::fromStdString(i.a) }, { "b", QString::fromStdString(i.b) },
                                            { "description", QString::fromStdString(i.description) }, { "conforming", i.conforming } });
        QJsonObject params;
        for (const auto& [k, v] : record.parameters) params.insert(QString::fromStdString(k), QString::fromStdString(v));
        QJsonArray warn, err;
        for (const auto& w : record.warnings) warn.append(QString::fromStdString(w));
        for (const auto& e : record.errors) err.append(QString::fromStdString(e));
        o.insert("record", QJsonObject { { "mesher", QString::fromStdString(record.mesher) },
                                         { "mesherVersion", QString::fromStdString(record.mesherVersion) },
                                         { "generated", QString::fromStdString(record.generated) },
                                         { "durationMs", record.durationMs },
                                         { "settingsHash", QString::fromStdString(record.settingsHash) },
                                         { "geometryHash", QString::fromStdString(record.geometryHash) },
                                         { "profile", QString::fromStdString(record.profile) },
                                         { "warnings", warn },
                                         { "errors", err },
                                         { "interfaces", interfaces },
                                         { "parameters", params } });
        o.insert("quality", QJsonObject { { "available", quality.available },
                                          { "errors", quality.errors },
                                          { "warnings", quality.warnings },
                                          { "worstAspect", quality.worstAspect },
                                          { "minAngle", quality.minAngle },
                                          { "maxAngle", quality.maxAngle },
                                          { "minJacobian", quality.minJacobian },
                                          { "estimatedDofs", double(quality.estimatedDofs) } });
        if (includeMesh) o.insert("mesh", meshToJson(mesh));
    }
    return o;
}

MeshProject MeshProject::fromJson(const QJsonObject& o, QStringList* warnings)
{
    MeshProject p;
    if (o.isEmpty()) return p;
    if (o.value("schema").toString() != QLatin1String(kMeshProjectSchema) && warnings)
        warnings->append(QStringLiteral("maillage : schéma « %1 » inattendu, lecture tolérante").arg(o.value("schema").toString()));
    const QJsonObject s = o.value("settings").toObject();
    p.settings.global = globalSettingsFromJson(s.value("global").toObject());
    for (const auto& v : s.value("locals").toArray()) p.settings.locals.push_back(localSettingFromJson(v.toObject()));
    for (const auto& v : s.value("zones").toArray()) p.settings.zones.push_back(zoneFromJson(v.toObject()));
    p.settings.profile = s.value("profile").toString().toStdString();
    if (o.contains("record"))
    {
        const QJsonObject r = o.value("record").toObject();
        p.record.mesher = r.value("mesher").toString().toStdString();
        p.record.mesherVersion = r.value("mesherVersion").toString().toStdString();
        p.record.generated = r.value("generated").toString().toStdString();
        p.record.durationMs = r.value("durationMs").toDouble();
        p.record.settingsHash = r.value("settingsHash").toString().toStdString();
        p.record.geometryHash = r.value("geometryHash").toString().toStdString();
        p.record.profile = r.value("profile").toString().toStdString();
        p.record.warnings = stringsFrom(r.value("warnings"));
        p.record.errors = stringsFrom(r.value("errors"));
        for (const auto& v : r.value("interfaces").toArray())
        {
            const QJsonObject i = v.toObject();
            p.record.interfaces.push_back({ i.value("a").toString().toStdString(), i.value("b").toString().toStdString(),
                                            i.value("description").toString().toStdString(), i.value("conforming").toBool(true) });
        }
        const QJsonObject params = r.value("parameters").toObject();
        for (auto it = params.begin(); it != params.end(); ++it) p.record.parameters[it.key().toStdString()] = it.value().toString().toStdString();
        const QJsonObject q = o.value("quality").toObject();
        p.quality.available = q.value("available").toBool();
        p.quality.errors = q.value("errors").toInt();
        p.quality.warnings = q.value("warnings").toInt();
        p.quality.worstAspect = q.value("worstAspect").toDouble();
        p.quality.minAngle = q.value("minAngle").toDouble();
        p.quality.maxAngle = q.value("maxAngle").toDouble();
        p.quality.minJacobian = q.value("minJacobian").toDouble();
        p.quality.estimatedDofs = std::size_t(q.value("estimatedDofs").toDouble());
        p.mesh = meshFromJson(o.value("mesh").toObject());
        p.hasMesh = !p.mesh.empty();
        if (!p.hasMesh && warnings) warnings->append(QStringLiteral("maillage : rapport présent mais maillage absent du fichier"));
    }
    return p;
}

MeshProject MeshProject::load(const TSA::Model::Model& model)
{
    if (model.meshProjectJson().empty()) return {};
    return fromJson(QJsonDocument::fromJson(QByteArray::fromStdString(model.meshProjectJson())).object());
}

void MeshProject::store(TSA::Model::Model& model) const
{
    model.setMeshProjectJson(QJsonDocument(toJson()).toJson(QJsonDocument::Compact).toStdString());
}

bool MeshProject::settingsChanged() const
{
    return hasMesh && MeshGenerator::settingsHash(settings) != record.settingsHash;
}

// --- Profils ---------------------------------------------------------------------------------------

QJsonObject profileToJson(const MeshProfile& p)
{
    return { { "name", QString::fromStdString(p.name) },
             { "category", QString::fromStdString(p.category) },
             { "description", QString::fromStdString(p.description) },
             { "global", globalSettingsToJson(p.global) } };
}

MeshProfile profileFromJson(const QJsonObject& o)
{
    MeshProfile p;
    p.name = o.value("name").toString().toStdString();
    p.category = o.value("category").toString("general").toStdString();
    p.description = o.value("description").toString().toStdString();
    p.global = globalSettingsFromJson(o.value("global").toObject());
    return p;
}

std::vector<MeshProfile> builtInProfiles()
{
    auto make = [](const char* name, const char* cat, const char* desc, double size, Fineness f) {
        MeshProfile p;
        p.name = name;
        p.category = cat;
        p.description = desc;
        p.builtIn = true;
        p.global.globalSize = size;
        p.global.fineness = f;
        return p;
    };
    std::vector<MeshProfile> v;
    v.push_back(make("Maillage rapide", "general", "Grossier : vérifications rapides du modèle", 1.0, Fineness::Coarse));
    v.push_back(make("Maillage standard", "general", "Équilibre précision / temps de calcul", 0.5, Fineness::Standard));
    auto fine = make("Maillage fin", "general", "Raffinement autour des appuis et des charges ponctuelles", 0.5, Fineness::Fine);
    fine.global.nearSupports = { true, 0.10, 0.5 };
    fine.global.nearPointLoads = { true, 0.10, 0.5 };
    v.push_back(fine);
    auto veryFine = make("Maillage très fin", "general", "Études de convergence, zones critiques", 0.5, Fineness::VeryFine);
    veryFine.global.nearSupports = { true, 0.05, 0.3 };
    veryFine.global.nearPointLoads = { true, 0.05, 0.3 };
    veryFine.global.maxElements = 2000000;
    v.push_back(veryFine);
    v.push_back(make("Maillage personnalisé", "general", "Point de départ à adapter", 0.5, Fineness::Custom));
    auto beams = make("Poutres et portiques", "beam", "1D : raffinement aux connexions et changements de section", 0.5, Fineness::Standard);
    beams.global.mesh2D = beams.global.mesh3D = false;
    beams.global.nearConnections = { true, 0.15, 0.3 };
    beams.global.nearSectionChanges = { true, 0.10, 0.3 };
    v.push_back(beams);
    auto slabs = make("Dalles", "slab", "2D : quadrangles structurés si possible, raffinement autour des ouvertures", 0.4, Fineness::Standard);
    slabs.global.mesh3D = false;
    slabs.global.recombine = true;
    slabs.global.nearOpenings = { true, 0.15, 0.4 };
    v.push_back(slabs);
    auto walls = make("Voiles", "wall", "2D : quadrangles structurés (transfinis) si le voile est rectangulaire", 0.4, Fineness::Standard);
    walls.global.mesh3D = false;
    walls.global.preferStructured = true;
    v.push_back(walls);
    auto bridge = make("Ponts et tabliers", "bridge", "Tablier et appuis : raffinement aux appuis et charges ponctuelles", 0.5, Fineness::Fine);
    bridge.global.nearSupports = { true, 0.20, 1.0 };
    bridge.global.nearPointLoads = { true, 0.20, 1.0 };
    bridge.global.recombine = true;
    v.push_back(bridge);
    auto volumes = make("Volumes (semelles)", "volume", "3D : hexaèdres transfinis", 0.25, Fineness::Standard);
    volumes.global.mesh3D = true;
    volumes.global.volumeElement = VolumeElement::Hexahedra;
    auto tetra = make("Volumes tétraédriques", "volume", "3D : tétraèdres (Delaunay), raffinement libre", 0.25, Fineness::Standard);
    tetra.global.mesh3D = true;
    tetra.global.volumeElement = VolumeElement::Tetrahedra;
    v.push_back(volumes);
    v.push_back(tetra);
    return v;
}

MeshProfileLibrary::MeshProfileLibrary(QString file)
    : m_file(file.isEmpty() ? TSA::Core::AppPaths::userConfigDir() + QStringLiteral("/mesh-profiles.json") : file)
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly)) return;
    for (const auto& v : QJsonDocument::fromJson(f.readAll()).object().value("profiles").toArray()) m_user.push_back(profileFromJson(v.toObject()));
}

std::vector<MeshProfile> MeshProfileLibrary::all() const
{
    std::vector<MeshProfile> out;
    for (const auto& b : builtInProfiles())
    {
        bool replaced = false;
        for (const auto& u : m_user) replaced |= u.name == b.name;
        if (!replaced) out.push_back(b);
    }
    out.insert(out.end(), m_user.begin(), m_user.end());
    return out;
}

std::optional<MeshProfile> MeshProfileLibrary::find(const std::string& name) const
{
    for (const auto& p : all())
        if (p.name == name) return p;
    return std::nullopt;
}

bool MeshProfileLibrary::write(QString* error) const
{
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QJsonArray arr;
    for (const auto& p : m_user) arr.append(profileToJson(p));
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(QJsonObject { { "schema", "tsa-mesh-profiles/1" }, { "profiles", arr } }).toJson()) < 0 ||
        !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1").arg(m_file);
        return false;
    }
    return true;
}

bool MeshProfileLibrary::save(const MeshProfile& profile, QString* error)
{
    if (profile.name.empty())
    {
        if (error) *error = QStringLiteral("nom de profil obligatoire");
        return false;
    }
    MeshProfile p = profile;
    p.builtIn = false;
    for (auto& u : m_user)
        if (u.name == p.name)
        {
            u = p;
            return write(error);
        }
    m_user.push_back(p);
    return write(error);
}

bool MeshProfileLibrary::remove(const std::string& name, QString* error)
{
    const auto before = m_user.size();
    m_user.erase(std::remove_if(m_user.begin(), m_user.end(), [&](const MeshProfile& p) { return p.name == name; }), m_user.end());
    if (m_user.size() == before)
    {
        if (error) *error = QStringLiteral("profil utilisateur « %1 » introuvable (un profil de départ ne se supprime pas)").arg(QString::fromStdString(name));
        return false;
    }
    return write(error);
}

bool MeshProfileLibrary::exportProfile(const std::string& name, const QString& path, QString* error) const
{
    const auto p = find(name);
    if (!p)
    {
        if (error) *error = QStringLiteral("profil introuvable");
        return false;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(QJsonObject { { "schema", "tsa-mesh-profile/1" }, { "profile", profileToJson(*p) } }).toJson()) < 0 ||
        !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1").arg(path);
        return false;
    }
    return true;
}

bool MeshProfileLibrary::importProfile(const QString& path, std::string* name, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = QStringLiteral("lecture impossible de %1").arg(path);
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root.value("schema").toString() != QLatin1String("tsa-mesh-profile/1"))
    {
        if (error) *error = QStringLiteral("fichier de profil de maillage invalide (schéma tsa-mesh-profile/1 attendu)");
        return false;
    }
    const MeshProfile p = profileFromJson(root.value("profile").toObject());
    if (name) *name = p.name;
    return save(p, error);
}

} // namespace TSA::Mesh
