#include "AutomationServer.h"

#include "CommandRegistry.h"
#include "../AI/Tools/AIToolRegistry.h"
#include "../Blueprint/BlueprintScript.h"
#include "../Project/ProjectSession.h"

#include <ProductIdentity.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

#include <algorithm>

namespace TSA::Automation
{

namespace
{
QJsonValue toJson(const Value& v)
{
    if (const auto* b = std::get_if<bool>(&v)) return *b;
    if (const auto* i = std::get_if<long long>(&v)) return static_cast<qint64>(*i);
    if (const auto* d = std::get_if<double>(&v)) return *d;
    if (const auto* s = std::get_if<std::string>(&v)) return QString::fromStdString(*s);
    if (const auto* p = std::get_if<Point3>(&v)) return QJsonArray { (*p)[0], (*p)[1], (*p)[2] };
    if (const auto* ids = std::get_if<std::vector<int>>(&v))
    {
        QJsonArray a;
        for (int id : *ids) a.append(id);
        return a;
    }
    return QJsonValue();
}

/// Valeur JSON → valeur typée du registre ; les chaînes passent par la syntaxe de la ligne de commande.
bool fromJson(const QJsonValue& j, ValueType type, Value& out)
{
    if (j.isString()) return parseArgument(j.toString().toStdString(), type, out);
    switch (type)
    {
    case ValueType::Bool:
        if (!j.isBool()) return false;
        out = j.toBool();
        return true;
    case ValueType::Integer:
        if (!j.isDouble() || j.toDouble() != static_cast<double>(j.toInteger())) return false;
        out = static_cast<long long>(j.toInteger());
        return true;
    case ValueType::Real:
        if (!j.isDouble()) return false;
        out = j.toDouble();
        return true;
    case ValueType::Text:
        return false;
    case ValueType::Point3:
    {
        const QJsonArray a = j.toArray();
        if (!j.isArray() || a.size() != 3) return false;
        Point3 p {};
        for (int k = 0; k < 3; ++k)
        {
            if (!a[k].isDouble()) return false;
            p[static_cast<std::size_t>(k)] = a[k].toDouble();
        }
        out = p;
        return true;
    }
    case ValueType::IdList:
    {
        if (!j.isArray()) return false;
        std::vector<int> ids;
        for (const auto& x : j.toArray())
        {
            if (!x.isDouble()) return false;
            ids.push_back(x.toInt());
        }
        out = ids;
        return true;
    }
    }
    return false;
}

QJsonObject outputsToJson(const Arguments& outputs)
{
    QJsonObject o;
    for (const auto& [name, v] : outputs) o[QString::fromStdString(name)] = toJson(v);
    return o;
}

QJsonObject resultJson(const CommandResult& r)
{
    return QJsonObject { { "ok", r.ok }, { "message", QString::fromStdString(r.message) }, { "outputs", outputsToJson(r.outputs) } };
}

QJsonArray parametersJson(const std::vector<ParameterSpec>& specs, bool inputs)
{
    QJsonArray a;
    for (const auto& p : specs)
    {
        QJsonObject o { { "name", QString::fromStdString(p.name) }, { "label", QString::fromStdString(p.label) },
                        { "type", QString::fromUtf8(typeName(p.type)) } };
        if (const char* unit = quantityUnit(p.quantity); unit && *unit) o["unit"] = QString::fromUtf8(unit);
        if (inputs)
        {
            o["required"] = p.required;
            if (!p.required && !std::holds_alternative<std::monostate>(p.defaultValue)) o["default"] = toJson(p.defaultValue);
        }
        if (!p.description.empty()) o["description"] = QString::fromStdString(p.description);
        a.append(o);
    }
    return a;
}

QJsonObject error(const QString& message)
{
    return QJsonObject { { "error", QJsonObject { { "message", message } } } };
}

QJsonObject result(const QJsonValue& value)
{
    return QJsonObject { { "result", value } };
}
} // namespace

AutomationServer::AutomationServer(TSA::Project::ProjectSession* session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
}

AutomationServer::~AutomationServer()
{
    stop();
}

QString AutomationServer::defaultName()
{
    return QStringLiteral("tsaraloha-%1").arg(QString::fromLatin1(TSA::Product::kName).toLower());
}

bool AutomationServer::start(const QString& name, QString* errorOut)
{
    stop();
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);   // compte Windows de l'utilisateur seulement
    if (!m_server->listen(name))
    {
        if (errorOut) *errorOut = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &AutomationServer::onNewConnection);
    return true;
}

void AutomationServer::stop()
{
    if (!m_server) return;
    m_server->close();
    delete m_server;
    m_server = nullptr;
}

bool AutomationServer::isListening() const
{
    return m_server && m_server->isListening();
}

QString AutomationServer::serverName() const
{
    return m_server ? m_server->serverName() : QString();
}

void AutomationServer::onNewConnection()
{
    while (QLocalSocket* socket = m_server->nextPendingConnection())
    {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        emit logMessage(tr("Client d'automatisation connecté (MCP)."), QStringLiteral("SYS"));
    }
}

void AutomationServer::onReadyRead(QLocalSocket* socket)
{
    while (socket->canReadLine())
    {
        const QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        QJsonObject response;
        if (!doc.isObject())
            response = error(QStringLiteral("requête JSON invalide : %1").arg(pe.errorString()));
        else
        {
            response = handle(doc.object());
            response["id"] = doc.object().value("id");
        }
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
        socket->flush();
    }
}

TSA::AI::EngineeringSources AutomationServer::sources() const
{
    if (m_sources) return m_sources();
    TSA::AI::EngineeringSources s;
    s.model = m_session ? &m_session->model() : nullptr;
    return s;
}

QJsonObject AutomationServer::handle(const QJsonObject& request)
{
    const QString method = request.value("method").toString();
    const QJsonObject params = request.value("params").toObject();
    if (!m_session) return error(QStringLiteral("aucun projet ouvert"));
    const CommandRegistry& registry = CommandRegistry::builtIn();

    if (method == "hello")
    {
        QJsonObject o { { "product", QString::fromLatin1(TSA::Product::kName) },
                        { "version", QString::fromLatin1(TSA::Product::kVersion) } };
        if (m_projectInfo)
        {
            const QJsonObject info = m_projectInfo();
            for (auto it = info.begin(); it != info.end(); ++it) o[it.key()] = it.value();
        }
        return result(o);
    }

    if (method == "list_commands")
    {
        QJsonArray list;
        for (const CommandSpec* spec : registry.commands())
            list.append(QJsonObject { { "id", QString::fromStdString(spec->id) }, { "title", QString::fromStdString(spec->title) },
                                      { "category", QString::fromStdString(spec->category) },
                                      { "description", QString::fromStdString(spec->description) },
                                      { "modifies_model", spec->modifiesModel },
                                      { "parameters", parametersJson(spec->parameters, true) },
                                      { "outputs", parametersJson(spec->outputs, false) } });
        return result(QJsonObject { { "commands", list } });
    }

    if (method == "execute")
    {
        const std::string id = params.value("command").toString().toStdString();
        const CommandSpec* spec = registry.find(id);
        if (!spec) return error(QStringLiteral("commande inconnue : %1 (voir list_commands)").arg(QString::fromStdString(id)));
        Arguments args;
        const QJsonObject jargs = params.value("arguments").toObject();
        for (auto it = jargs.begin(); it != jargs.end(); ++it)
        {
            const std::string name = it.key().toStdString();
            const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(),
                                        [&](const ParameterSpec& s) { return s.name == name; });
            if (p == spec->parameters.end())
                return error(QStringLiteral("paramètre inconnu « %1 » pour %2").arg(it.key(), QString::fromStdString(id)));
            Value v;
            if (!fromJson(it.value(), p->type, v))
                return error(QStringLiteral("paramètre « %1 » : %2 attendu").arg(it.key(), QString::fromUtf8(typeName(p->type))));
            args[name] = v;
        }
        const CommandResult r = registry.execute(*m_session, id, std::move(args));
        emit logMessage(QStringLiteral("MCP › %1 : %2").arg(QString::fromStdString(id), QString::fromStdString(r.message)),
                        r.ok ? QStringLiteral("INFO") : QStringLiteral("ERROR"));
        if (r.ok) emit projectModified();
        return result(resultJson(r));
    }

    if (method == "execute_line")
    {
        const QString line = params.value("line").toString();
        const CommandResult r = executeCommandLine(registry, *m_session, line.toStdString());
        emit logMessage(QStringLiteral("MCP › %1 : %2").arg(line, QString::fromStdString(r.message)),
                        r.ok ? QStringLiteral("INFO") : QStringLiteral("ERROR"));
        if (r.ok) emit projectModified();
        return result(resultJson(r));
    }

    if (method == "run_script")
    {
        TSA::Blueprint::Graph graph;
        std::string why;
        const auto& library = TSA::Blueprint::NodeLibrary::standard();
        if (!TSA::Blueprint::fromCommandScript(params.value("script").toString().toStdString(), library, registry, graph, &why))
            return error(QStringLiteral("script invalide : %1").arg(QString::fromStdString(why)));
        const auto report = TSA::Blueprint::Runner(library, graph, m_session).run();
        QJsonArray log;
        for (const auto& l : report.log) log.append(QString::fromStdString(l));
        emit logMessage(QStringLiteral("MCP › script : %1").arg(QString::fromStdString(report.message)),
                        report.ok ? QStringLiteral("INFO") : QStringLiteral("ERROR"));
        emit projectModified();
        return result(QJsonObject { { "ok", report.ok }, { "message", QString::fromStdString(report.message) },
                                    { "steps", report.steps }, { "log", log } });
    }

    if (method == "model_summary")
    {
        TSA::AI::ContextOptions opt;
        opt.includeCheckReport = params.value("include_checks").toBool(false);
        return result(TSA::AI::EngineeringContextBuilder::summary(sources(), opt));
    }

    if (method == "read_tool")
    {
        // Outils de LECTURE de l'assistant IA seulement (les propositions passent par les commandes).
        const QString name = params.value("tool").toString();
        if (!TSA::AI::AIToolRegistry::whitelist().contains(name) || TSA::AI::AIToolRegistry::kindOf(name) != TSA::AI::ToolKind::Read)
            return error(QStringLiteral("outil de lecture inconnu : %1").arg(name));
        TSA::AI::AIToolRegistry tools([this] { return sources(); }, nullptr);
        const auto out = tools.execute(name, QString::fromUtf8(QJsonDocument(params.value("arguments").toObject()).toJson(QJsonDocument::Compact)));
        if (!out.ok) return error(out.error);
        return result(out.data);
    }

    if (method == "undo" || method == "redo")
    {
        std::string action;
        const bool ok = method == "undo" ? m_session->undo(&action) : m_session->redo(&action);
        if (ok) emit projectModified();
        return result(QJsonObject { { "ok", ok }, { "action", QString::fromStdString(action) } });
    }

    return error(QStringLiteral("méthode inconnue : %1").arg(method));
}

} // namespace TSA::Automation
