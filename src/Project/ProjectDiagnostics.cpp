#include "ProjectDiagnostics.h"

#include "../Analysis/Engine/AnalysisContext.h"
#include "../Analysis/ResultsModel.h"
#include "../Standards/ModelValidator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace TSA::Project
{
namespace
{
QString q(const std::string& s) { return QString::fromStdString(s); }
std::string s(const QJsonValue& v) { return v.toString().toStdString(); }

QJsonArray toArray(const std::vector<std::string>& list)
{
    QJsonArray a;
    for (const auto& item : list) a.append(q(item));
    return a;
}
std::vector<std::string> fromArray(const QJsonValue& v)
{
    std::vector<std::string> out;
    for (const QJsonValue& item : v.toArray()) out.push_back(s(item));
    return out;
}

/// Les kMaxLogLines dernières lignes (le début d'un long journal est le moins utile pour comprendre un échec).
std::string lastLines(const std::string& text, std::size_t maxLines)
{
    std::size_t lines = 0, pos = text.size();
    while (pos > 0)
    {
        const std::size_t nl = text.rfind('\n', pos - 1);
        if (nl == std::string::npos) return text;
        if (nl != text.size() - 1 && ++lines == maxLines) return text.substr(nl + 1);
        pos = nl;
    }
    return text;
}

const char* severityKey(TSA::Standards::ValidationSeverity sev)
{
    switch (sev)
    {
    case TSA::Standards::ValidationSeverity::Error: return "error";
    case TSA::Standards::ValidationSeverity::Warning: return "warning";
    case TSA::Standards::ValidationSeverity::Info: return "info";
    }
    return "info";
}
} // namespace

void ProjectDiagnostics::recordCalculation(const TSA::Analysis::AnalysisContext& context, bool success, const std::string& message,
                                           const TSA::Analysis::ResultsModel* results, const std::string& engineLog,
                                           double durationMs, const std::string& modelFingerprint, const std::string& timestamp)
{
    CalculationRecord r;
    r.timestamp = timestamp;
    r.engineId = context.engineId;
    r.analysisType = TSA::Analysis::analysisTypeKey(context.type);
    if (context.combinationId > 0)
        r.loadSelection = "combinaison " + std::to_string(context.combinationId);
    else
    {
        for (int id : context.loadCaseIds) r.loadSelection += (r.loadSelection.empty() ? "cas " : ", ") + std::to_string(id);
        if (r.loadSelection.empty()) r.loadSelection = "cas actif";
    }
    r.success = success;
    r.message = message;
    r.durationMs = durationMs;
    r.modelFingerprint = modelFingerprint;

    m_lastMessages.clear();
    if (results && success)
    {
        const auto& meta = results->executionMetadata();
        r.engine = meta.solverEngine + (meta.solverVersion.empty() ? std::string() : " " + meta.solverVersion);
        r.scope = meta.analysisScope;
        if (!results->caseOrComboName().empty()) r.loadSelection = results->caseOrComboName();
        for (const auto& m : meta.preparationMessages) m_lastMessages.push_back("préparation : " + m);
        for (const auto& w : meta.warnings) m_lastMessages.push_back("avertissement : " + w);
    }
    else
    {
        r.scope = context.scope.type == TSA::Analysis::ScopeType::EntireModel
                      ? std::string()
                      : std::string(TSA::Analysis::scopeTypeKey(context.scope.type)) +
                            (context.scope.axisLabel.empty() ? std::string() : " " + context.scope.axisLabel);
        if (!message.empty()) m_lastMessages.push_back("erreur : " + message);
    }
    r.warnings = m_lastMessages.size();
    m_lastEngineLog = lastLines(engineLog, kMaxLogLines);

    m_history.push_back(std::move(r));
    while (m_history.size() > kMaxRecords) m_history.pop_front();
}

void ProjectDiagnostics::setValidation(const TSA::Standards::ModelValidationReport& report)
{
    m_validation.clear();
    for (const auto& issue : report.issues())
        m_validation.push_back({ severityKey(issue.severity), issue.category, issue.message, issue.entityId });
}

void ProjectDiagnostics::clear()
{
    m_history.clear();
    m_lastMessages.clear();
    m_lastEngineLog.clear();
    m_validation.clear();
}

std::string ProjectDiagnostics::toJson() const
{
    QJsonArray history;
    for (const auto& r : m_history)
        history.append(QJsonObject { { "timestamp", q(r.timestamp) },
                                     { "engineId", q(r.engineId) },
                                     { "engine", q(r.engine) },
                                     { "analysisType", q(r.analysisType) },
                                     { "loads", q(r.loadSelection) },
                                     { "scope", q(r.scope) },
                                     { "success", r.success },
                                     { "message", q(r.message) },
                                     { "durationMs", r.durationMs },
                                     { "warnings", static_cast<double>(r.warnings) },
                                     { "modelFingerprint", q(r.modelFingerprint) } });
    QJsonArray validation;
    for (const auto& v : m_validation)
        validation.append(QJsonObject { { "severity", q(v.severity) }, { "category", q(v.category) }, { "message", q(v.message) },
                                        { "entity", v.entityId } });
    const QJsonObject root { { "schema", 1 },
                             { "calculations", history },
                             { "lastMessages", toArray(m_lastMessages) },
                             { "lastEngineLog", q(m_lastEngineLog) },
                             { "validation", validation } };
    return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

bool ProjectDiagnostics::fromJson(const std::string& json, std::string* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &parseError);
    if (!doc.isObject())
    {
        if (error) *error = "diagnostics illisibles : " + parseError.errorString().toStdString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value("schema").toInt(0) != 1)
    {
        if (error) *error = "diagnostics : version " + std::to_string(root.value("schema").toInt(0)) + " inconnue";
        return false;
    }
    ProjectDiagnostics d;
    for (const QJsonValue& v : root.value("calculations").toArray())
    {
        const QJsonObject o = v.toObject();
        CalculationRecord r;
        r.timestamp = s(o.value("timestamp"));
        r.engineId = s(o.value("engineId"));
        r.engine = s(o.value("engine"));
        r.analysisType = s(o.value("analysisType"));
        r.loadSelection = s(o.value("loads"));
        r.scope = s(o.value("scope"));
        r.success = o.value("success").toBool();
        r.message = s(o.value("message"));
        r.durationMs = o.value("durationMs").toDouble();
        r.warnings = static_cast<std::size_t>(o.value("warnings").toDouble());
        r.modelFingerprint = s(o.value("modelFingerprint"));
        d.m_history.push_back(std::move(r));
    }
    while (d.m_history.size() > kMaxRecords) d.m_history.pop_front();
    d.m_lastMessages = fromArray(root.value("lastMessages"));
    d.m_lastEngineLog = lastLines(s(root.value("lastEngineLog")), kMaxLogLines);
    for (const QJsonValue& v : root.value("validation").toArray())
    {
        const QJsonObject o = v.toObject();
        d.m_validation.push_back({ s(o.value("severity")), s(o.value("category")), s(o.value("message")), o.value("entity").toInt() });
    }
    *this = std::move(d);
    return true;
}

} // namespace TSA::Project
