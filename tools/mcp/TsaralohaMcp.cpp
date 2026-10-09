// tsaraloha-mcp : serveur MCP (Model Context Protocol, transport stdio) de l'écosystème Tsaraloha.
//
// Claude Code (ou tout client MCP) lance ce programme ; il relaie les appels d'outils vers TSA ou TSALab
// ouvert sur le poste, par le canal local de leur AutomationServer (« tsaraloha-tsa », « tsaraloha-tsalab »,
// réservé au compte Windows de l'utilisateur). Le projet est modifié EN DIRECT dans l'application, par les
// commandes du registre central : chaque modification est visible et annulable (Ctrl+Z).
//
// Usage : tsaraloha-mcp [--app TSA|TSALab]   (sans option : TSALab s'il est ouvert, sinon TSA)
// Protocole : JSON-RPC 2.0, un message par ligne sur stdin / stdout ; rien d'autre n'est écrit sur stdout.

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStringList>

#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
constexpr const char* kServerVersion = "1.0.0";
constexpr int kCallTimeoutMs = 15 * 60 * 1000;   // un calcul peut être long

QStringList g_candidates;
QLocalSocket* g_socket = nullptr;
QString g_connectedTo;
int g_nextId = 1;

QJsonObject schema(const QJsonObject& properties, const QStringList& required = {})
{
    QJsonObject s { { "type", "object" }, { "properties", properties } };
    if (!required.isEmpty()) s["required"] = QJsonArray::fromStringList(required);
    return s;
}

QJsonObject prop(const QString& type, const QString& description)
{
    return QJsonObject { { "type", type }, { "description", description } };
}

QJsonArray toolList()
{
    const QJsonArray readTools { "get_project_info", "list_members", "list_nodes", "get_object", "list_load_cases",
                                 "list_load_combinations", "list_loads", "get_results_summary", "check_model" };
    auto tool = [](const QString& name, const QString& description, const QJsonObject& input) {
        return QJsonObject { { "name", name }, { "description", description }, { "inputSchema", input } };
    };
    return QJsonArray {
        tool("tsaraloha_status", "Application connectée (TSA ou TSALab), version et projet ouvert. À appeler en premier.", schema({})),
        tool("list_commands",
             "Commandes du registre central de TSA : identifiant, paramètres typés avec unités (m, kN, kN/m, kN·m, degrés), "
             "valeurs par défaut, sorties. Une même commande sert l'interface, la console, les Blueprints et ce serveur.",
             schema({})),
        tool("execute_command",
             "Exécute UNE commande du registre sur le projet ouvert (modification visible en direct, une entrée Annuler). "
             "Exemple : command = \"model.create_node\", arguments = {\"position\": [0, 0, 3]}. Point = [x, y, z] en m ; "
             "liste d'identifiants = [1, 2] ; les sorties (ex. « id ») servent aux commandes suivantes.",
             schema({ { "command", prop("string", "Identifiant de la commande (voir list_commands)") },
                      { "arguments", prop("object", "Paramètres nommés de la commande") } },
                    { "command" })),
        tool("run_command_script",
             "Exécute un script de commandes (une par ligne, syntaxe de la console) comme un Blueprint : « a = commande … » nomme "
             "une commande, « a.id » réutilise sa sortie. Ex. :\na = model.create_node position=0,0,0\n"
             "b = model.create_node position=6,0,0\nmodel.create_beam start=a.id end=b.id section=\"IPE 300\"",
             schema({ { "script", prop("string", "Script, une commande par ligne") } }, { "script" })),
        tool("get_model_summary",
             "Contexte d'ingénierie structuré du projet : effectifs, matériaux, sections, appuis, charges, résultats (unités "
             "explicites). Listes plafonnées : détail par inspect_model.",
             schema({ { "include_checks", prop("boolean", "Ajouter les contrôles automatiques du modèle") } })),
        tool("inspect_model",
             "Lecture détaillée du modèle (lecture seule) : get_project_info, list_members {type, offset, limit}, list_nodes "
             "{offset, limit}, get_object {type, id}, list_load_cases, list_load_combinations, list_loads {load_case_id, limit}, "
             "get_results_summary, check_model.",
             schema({ { "tool", QJsonObject { { "type", "string" }, { "enum", readTools } } },
                      { "arguments", prop("object", "Arguments de l'outil de lecture") } },
                    { "tool" })),
        tool("undo", "Annule la dernière action du projet (comme Ctrl+Z dans l'application).", schema({})),
        tool("redo", "Rétablit la dernière action annulée.", schema({})),
    };
}

bool ensureConnected(QString* error)
{
    if (g_socket && g_socket->state() == QLocalSocket::ConnectedState) return true;
    delete g_socket;
    g_socket = new QLocalSocket();
    for (const QString& name : g_candidates)
    {
        g_socket->connectToServer(name);
        if (g_socket->waitForConnected(1500))
        {
            g_connectedTo = name;
            return true;
        }
        g_socket->abort();
    }
    *error = QStringLiteral("Aucune application Tsaraloha ouverte (%1). Ouvrez TSA ou TSALab avec un projet, puis réessayez.")
                 .arg(g_candidates.join(QStringLiteral(", ")));
    return false;
}

/// Appel de l'AutomationServer : résultat, ou faux avec *error.
bool callApp(const QString& method, const QJsonObject& params, QJsonValue* resultOut, QString* error)
{
    if (!ensureConnected(error)) return false;
    const int id = g_nextId++;
    const QJsonObject request { { "id", id }, { "method", method }, { "params", params } };
    g_socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    if (!g_socket->waitForBytesWritten(5000))
    {
        *error = QStringLiteral("Envoi impossible vers %1.").arg(g_connectedTo);
        return false;
    }
    while (!g_socket->canReadLine())
        if (!g_socket->waitForReadyRead(kCallTimeoutMs))
        {
            *error = QStringLiteral("Pas de réponse de %1 (application fermée ou occupée).").arg(g_connectedTo);
            g_socket->abort();
            return false;
        }
    const QJsonObject response = QJsonDocument::fromJson(g_socket->readLine()).object();
    if (response.contains("error"))
    {
        *error = response.value("error").toObject().value("message").toString();
        return false;
    }
    *resultOut = response.value("result");
    return true;
}

QJsonObject textResult(const QJsonValue& value, bool isError)
{
    const QString text = value.isString() ? value.toString()
                                          : QString::fromUtf8(QJsonDocument(value.isObject() ? QJsonDocument(value.toObject())
                                                                                              : QJsonDocument(value.toArray()))
                                                                  .toJson(QJsonDocument::Indented));
    return QJsonObject { { "content", QJsonArray { QJsonObject { { "type", "text" }, { "text", text } } } }, { "isError", isError } };
}

QJsonObject callTool(const QString& name, const QJsonObject& args)
{
    QString method;
    QJsonObject params;
    if (name == "tsaraloha_status") method = "hello";
    else if (name == "list_commands") method = "list_commands";
    else if (name == "execute_command")
    {
        method = "execute";
        params = QJsonObject { { "command", args.value("command") }, { "arguments", args.value("arguments").toObject() } };
    }
    else if (name == "run_command_script")
    {
        method = "run_script";
        params = QJsonObject { { "script", args.value("script") } };
    }
    else if (name == "get_model_summary")
    {
        method = "model_summary";
        params = QJsonObject { { "include_checks", args.value("include_checks").toBool(false) } };
    }
    else if (name == "inspect_model")
    {
        method = "read_tool";
        params = QJsonObject { { "tool", args.value("tool") }, { "arguments", args.value("arguments").toObject() } };
    }
    else if (name == "undo" || name == "redo") method = name;
    else return textResult(QStringLiteral("Outil inconnu : %1").arg(name), true);

    QJsonValue value;
    QString error;
    if (!callApp(method, params, &value, &error)) return textResult(error, true);
    if (name == "tsaraloha_status" && value.isObject())
    {
        QJsonObject o = value.toObject();
        o["channel"] = g_connectedTo;
        value = o;
    }
    // Échec métier (commande refusée, script en erreur) : signalé comme erreur d'outil.
    const bool failed = value.isObject() && value.toObject().contains("ok") && !value.toObject().value("ok").toBool();
    return textResult(value, failed);
}

void send(const QJsonObject& message)
{
    const QByteArray bytes = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stdout);
    std::fflush(stdout);
}

void reply(const QJsonValue& id, const QJsonObject& result)
{
    send(QJsonObject { { "jsonrpc", "2.0" }, { "id", id }, { "result", result } });
}

void replyError(const QJsonValue& id, int code, const QString& message)
{
    send(QJsonObject { { "jsonrpc", "2.0" }, { "id", id }, { "error", QJsonObject { { "code", code }, { "message", message } } } });
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const QStringList args = app.arguments();
    const int appIndex = args.indexOf(QStringLiteral("--app"));
    if (appIndex > 0 && appIndex + 1 < args.size())
        g_candidates << QStringLiteral("tsaraloha-%1").arg(args[appIndex + 1].toLower());
    else
        g_candidates << QStringLiteral("tsaraloha-tsalab") << QStringLiteral("tsaraloha-tsa");

    std::string line;
    while (std::getline(std::cin, line))
    {
        const QByteArray bytes = QByteArray::fromStdString(line).trimmed();
        if (bytes.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (!doc.isObject())
        {
            replyError(QJsonValue(), -32700, QStringLiteral("JSON invalide"));
            continue;
        }
        const QJsonObject msg = doc.object();
        const QString method = msg.value("method").toString();
        if (!msg.contains("id")) continue;   // notification (ex. notifications/initialized) : aucune réponse
        const QJsonValue id = msg.value("id");
        const QJsonObject params = msg.value("params").toObject();

        if (method == "initialize")
        {
            const QString version = params.value("protocolVersion").toString(QStringLiteral("2025-06-18"));
            reply(id, QJsonObject {
                          { "protocolVersion", version },
                          { "capabilities", QJsonObject { { "tools", QJsonObject { { "listChanged", false } } } } },
                          { "serverInfo", QJsonObject { { "name", "tsaraloha" }, { "version", kServerVersion } } },
                          { "instructions",
                            "Co-ingénieur de TSA / TSALab (analyse de structures, génie civil). Le projet ouvert dans "
                            "l'application est modifié en direct. Commencer par tsaraloha_status puis get_model_summary ; "
                            "consulter list_commands avant execute_command. Unités : m, kN, kN/m, kN·m, degrés. Chaque "
                            "commande modifiante crée une entrée Annuler : expliquer à l'ingénieur ce qui a été fait. Les "
                            "résultats de calcul doivent être vérifiés par un ingénieur qualifié." } });
        }
        else if (method == "ping")
            reply(id, QJsonObject {});
        else if (method == "tools/list")
            reply(id, QJsonObject { { "tools", toolList() } });
        else if (method == "tools/call")
            reply(id, callTool(params.value("name").toString(), params.value("arguments").toObject()));
        else
            replyError(id, -32601, QStringLiteral("Méthode non prise en charge : %1").arg(method));
    }
    delete g_socket;
    return 0;
}
