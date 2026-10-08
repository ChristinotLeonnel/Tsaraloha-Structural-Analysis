#include "AnalysisContext.h"

#include <QJsonArray>

namespace TSA::Analysis
{

namespace
{
template <typename E, std::size_t N>
E enumFromKey(const QString& key, const std::pair<E, const char*> (&table)[N], E fallback)
{
    for (const auto& [value, name] : table)
        if (key == QLatin1String(name)) return value;
    return fallback;
}

template <typename E, std::size_t N>
const char* keyFromEnum(E value, const std::pair<E, const char*> (&table)[N])
{
    for (const auto& [v, name] : table)
        if (v == value) return name;
    return table[0].second;
}

const std::pair<AnalysisType, const char*> kTypes[] = {
    { AnalysisType::LinearStatic, "linear_static" },
    { AnalysisType::NonLinearStatic, "nonlinear_static" },
};
const std::pair<ScopeType, const char*> kScopes[] = {
    { ScopeType::EntireModel, "entire_model" },
    { ScopeType::SelectedElements, "selection" },
    { ScopeType::GridAxis, "grid_axis" },
    { ScopeType::Level, "level" },
    { ScopeType::WorkPlane, "work_plane" },
    { ScopeType::ModelPlane, "model_plane" },
};
const std::pair<AnalysisDimension, const char*> kDims[] = {
    { AnalysisDimension::Space3D, "3d" },
    { AnalysisDimension::Plane2D, "2d" },
};

QJsonArray toArray(const std::set<int>& ids)
{
    QJsonArray a;
    for (int id : ids) a.append(id);
    return a;
}

std::set<int> toSet(const QJsonValue& v)
{
    std::set<int> s;
    for (const auto& x : v.toArray()) s.insert(x.toInt());
    return s;
}
} // namespace

const char* analysisTypeKey(AnalysisType t) { return keyFromEnum(t, kTypes); }
const char* scopeTypeKey(ScopeType t) { return keyFromEnum(t, kScopes); }
const char* dimensionKey(AnalysisDimension d) { return keyFromEnum(d, kDims); }

bool AnalysisScope::operator==(const AnalysisScope& o) const
{
    return type == o.type && gridId == o.gridId && axisFamily == o.axisFamily && axisLabel == o.axisLabel &&
           levelId == o.levelId && workPlaneId == o.workPlaneId && nodes == o.nodes && beams == o.beams &&
           columns == o.columns && trussMembers == o.trussMembers && cables == o.cables && slabs == o.slabs &&
           walls == o.walls && foundations == o.foundations;
}

QJsonObject AnalysisContext::toJson() const
{
    QJsonObject scopeJson{
        { "type", scopeTypeKey(scope.type) },
    };
    if (scope.type == ScopeType::GridAxis)
    {
        scopeJson["gridId"] = QString::fromStdString(scope.gridId);
        scopeJson["axisFamily"] = scope.axisFamily == GridAxisFamily::X ? "x" : "y";
        scopeJson["axisLabel"] = QString::fromStdString(scope.axisLabel);
    }
    if (!scope.levelId.empty()) scopeJson["levelId"] = QString::fromStdString(scope.levelId);
    if (scope.type == ScopeType::WorkPlane) scopeJson["workPlaneId"] = scope.workPlaneId;
    if (scope.type == ScopeType::SelectedElements)
    {
        scopeJson["nodes"] = toArray(scope.nodes);
        scopeJson["beams"] = toArray(scope.beams);
        scopeJson["columns"] = toArray(scope.columns);
        scopeJson["trussMembers"] = toArray(scope.trussMembers);
        scopeJson["cables"] = toArray(scope.cables);
        scopeJson["slabs"] = toArray(scope.slabs);
        scopeJson["walls"] = toArray(scope.walls);
        scopeJson["foundations"] = toArray(scope.foundations);
    }

    QJsonArray cases;
    for (int id : loadCaseIds) cases.append(id);

    QJsonObject engines;
    for (const auto& [id, settings] : engineSettings) engines[QString::fromStdString(id)] = settings;

    return QJsonObject{
        { "schemaVersion", kSchemaVersion },
        { "engineId", QString::fromStdString(engineId) },
        { "dimension", dimensionKey(dimension) },
        { "analysisType", analysisTypeKey(type) },
        { "scope", scopeJson },
        { "loadCases", cases },
        { "combinationId", combinationId },
        { "common", QJsonObject{ { "includeSelfWeight", common.includeSelfWeight } } },
        { "engineSettings", engines },
    };
}

AnalysisContext AnalysisContext::fromJson(const QJsonObject& json, bool* ok)
{
    AnalysisContext c;
    const int version = json.value("schemaVersion").toInt(0);
    if (ok) *ok = version >= 1 && version <= kSchemaVersion;
    if (version < 1) return c;

    c.engineId = json.value("engineId").toString().toStdString();
    c.dimension = enumFromKey(json.value("dimension").toString(), kDims, AnalysisDimension::Space3D);
    c.type = enumFromKey(json.value("analysisType").toString(), kTypes, AnalysisType::LinearStatic);

    const QJsonObject s = json.value("scope").toObject();
    c.scope.type = enumFromKey(s.value("type").toString(), kScopes, ScopeType::EntireModel);
    c.scope.gridId = s.value("gridId").toString().toStdString();
    c.scope.axisFamily = s.value("axisFamily").toString() == "x" ? GridAxisFamily::X : GridAxisFamily::Y;
    c.scope.axisLabel = s.value("axisLabel").toString().toStdString();
    c.scope.levelId = s.value("levelId").toString().toStdString();
    c.scope.workPlaneId = s.value("workPlaneId").toInt(0);
    c.scope.nodes = toSet(s.value("nodes"));
    c.scope.beams = toSet(s.value("beams"));
    c.scope.columns = toSet(s.value("columns"));
    c.scope.trussMembers = toSet(s.value("trussMembers"));
    c.scope.cables = toSet(s.value("cables"));
    c.scope.slabs = toSet(s.value("slabs"));
    c.scope.walls = toSet(s.value("walls"));
    c.scope.foundations = toSet(s.value("foundations"));

    for (const auto& v : json.value("loadCases").toArray()) c.loadCaseIds.push_back(v.toInt());
    c.combinationId = json.value("combinationId").toInt(0);

    const QJsonObject common = json.value("common").toObject();
    c.common.includeSelfWeight = common.value("includeSelfWeight").toBool(true);

    const QJsonObject engines = json.value("engineSettings").toObject();
    for (auto it = engines.begin(); it != engines.end(); ++it)
        c.engineSettings[it.key().toStdString()] = it.value().toObject();
    return c;
}

} // namespace TSA::Analysis
