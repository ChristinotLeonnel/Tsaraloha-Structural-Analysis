// Suite « automation » : registre central des commandes exécutables (ADR-024, phase 5) — test 192 ;
// serveur d'automatisation et pont MCP (Claude Code co-ingénieur) — test 200.
// Une même commande sert l'interface, le Blueprint, l'IA et la console : création par ligne de
// commande, typage des arguments, une entrée Annuler par commande, échec sans effet sur le modèle.
#include "test_common.h"

#include "Automation/AutomationServer.h"
#include "Automation/CommandRegistry.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QThread>

bool runSuite_Automation(int& passed)
{
    using namespace TSA::Automation;

    // TEST 192 : commandes intégrées par ligne de commande, validation, Annuler
    {
        const CommandRegistry& reg = CommandRegistry::builtIn();
        TEST_CHECK(reg.find("model.create_node") && reg.find("model.create_beam") && reg.find("loads.add_uniform"),
                   "Test 192: commandes intégrées enregistrées");
        TEST_CHECK(reg.find("model.create_node")->modifiesModel && !reg.find("query.model_summary")->modifiesModel,
                   "Test 192: commandes modifiantes / requêtes distinguées");

        TSA::Project::ProjectSession session;
        auto run = [&](const std::string& line) { return executeCommandLine(reg, session, line); };

        const auto n1 = run("model.create_node position=0,0,0");
        const auto n2 = run("model.create_node position=6,0,0 name=\"Appui B\"");
        TEST_CHECK(n1.ok && n2.ok, "Test 192: nœuds créés");
        const long long id1 = std::get<long long>(n1.outputs.at("id")), id2 = std::get<long long>(n2.outputs.at("id"));
        TEST_CHECK(session.model().getNode(int(id2)) && session.model().getNode(int(id2))->x() == 6.0,
                   "Test 192: position du nœud");

        const auto beam = run("model.create_beam start=" + std::to_string(id1) + " end=" + std::to_string(id2) + " section=\"HEA 200\"");
        TEST_CHECK(beam.ok && session.model().beams().size() == 1, "Test 192: poutre créée (section par nom)");
        const long long beamId = std::get<long long>(beam.outputs.at("id"));
        TEST_CHECK(run("model.set_support node=" + std::to_string(id1) + " type=pinned").ok, "Test 192: appui défini");

        const auto lc = run("loads.create_case name=Exploitation category=live");
        TEST_CHECK(lc.ok, "Test 192: cas de charge créé");
        const long long caseId = std::get<long long>(lc.outputs.at("id"));
        const auto q = run("loads.add_uniform beam=" + std::to_string(beamId) + " case=" + std::to_string(caseId) + " q=10");
        TEST_CHECK(q.ok && session.model().loadManager().memberLoads().size() == 1, "Test 192: charge répartie (entier accepté pour un réel)");

        // Arguments invalides : refus explicite, modèle inchangé
        const std::size_t nodes = session.model().nodes().size();
        TEST_CHECK(!run("model.create_node position=1,2").ok, "Test 192: point incomplet refusé");
        TEST_CHECK(!run("model.create_node").ok, "Test 192: paramètre requis manquant refusé");
        TEST_CHECK(!run("model.create_beam start=1 end=999").ok, "Test 192: nœud inexistant refusé");
        TEST_CHECK(!run("model.create_beam start=" + std::to_string(id1) + " end=" + std::to_string(id2) + " section=XYZ").ok,
                   "Test 192: section inconnue refusée");
        TEST_CHECK(!run("model.inconnue a=1").ok && !run("model.create_node position=0,0,0 couleur=rouge").ok,
                   "Test 192: commande / paramètre inconnus refusés");
        TEST_CHECK(session.model().nodes().size() == nodes && session.model().beams().size() == 1,
                   "Test 192: un échec ne modifie pas le modèle");

        // Requête (lecture seule, aucune entrée Annuler)
        const auto sum = run("query.model_summary");
        TEST_CHECK(sum.ok && std::get<long long>(sum.outputs.at("nodes")) == 2 && std::get<long long>(sum.outputs.at("beams")) == 1
                       && std::get<long long>(sum.outputs.at("columns")) == 0,
                   "Test 192: requête de résumé");
        const auto col = run("model.create_column start=" + std::to_string(id2) + " end=" + std::to_string(id1));
        const auto sum2 = run("query.model_summary");
        TEST_CHECK(col.ok && std::get<long long>(sum2.outputs.at("columns")) == 1 && std::get<long long>(sum2.outputs.at("beams")) == 1,
                   "Test 192: poteau compté comme poteau (rôle de la barre)");
        TEST_CHECK(session.undo(), "Test 192: Annuler le poteau");

        // Une entrée Annuler par commande : annuler la charge puis la poutre
        TEST_CHECK(session.undo() && session.model().loadManager().memberLoads().empty(), "Test 192: Annuler la charge");
        TEST_CHECK(session.undo() && session.undo() && session.undo() && session.model().beams().empty(),
                   "Test 192: Annuler cas, appui puis poutre");
        TEST_CHECK(session.redo() && session.model().beams().size() == 1, "Test 192: Rétablir la poutre");
        std::cout << "[PASS] Test 192: registre de commandes (ligne de commande, typage, Annuler)" << std::endl;
        ++passed;
    }

    // TEST 200 : serveur d'automatisation (projet ouvert) et pont MCP tsaraloha-mcp (Claude Code)
    {
        TSA::Project::ProjectSession session;
        AutomationServer server(&session);
        auto call = [&](const QString& method, const QJsonObject& params = {}) {
            return server.handle(QJsonObject { { "method", method }, { "params", params } });
        };

        // Appels directs (thread de l'interface)
        const auto list = call("list_commands").value("result").toObject().value("commands").toArray();
        TEST_CHECK(list.size() >= 10, "Test 200: list_commands");
        const auto n1 = call("execute", { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", QJsonArray { 0, 0, 0 } } } } });
        const auto n2 = call("execute", { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", "6,0,0" } } } });
        TEST_CHECK(n1.value("result").toObject().value("ok").toBool() && n2.value("result").toObject().value("ok").toBool()
                       && session.model().nodes().size() == 2,
                   "Test 200: execute (point JSON ou texte)");
        const int id1 = n1.value("result").toObject().value("outputs").toObject().value("id").toInt();
        const int id2 = n2.value("result").toObject().value("outputs").toObject().value("id").toInt();
        const auto beam = call("execute_line", { { "line", QStringLiteral("model.create_beam start=%1 end=%2 section=\"IPE 300\"").arg(id1).arg(id2) } });
        TEST_CHECK(beam.value("result").toObject().value("ok").toBool() && session.model().beams().size() == 1, "Test 200: execute_line");
        TEST_CHECK(call("execute", { { "command", "model.inconnue" } }).contains("error"), "Test 200: commande inconnue refusée");
        TEST_CHECK(call("execute", { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", true } } } }).contains("error"),
                   "Test 200: argument mal typé refusé");
        const auto script = call("run_script", { { "script", "a = model.create_node position=0,0,3\nquery.model_summary" } });
        TEST_CHECK(script.value("result").toObject().value("ok").toBool() && session.model().nodes().size() == 3, "Test 200: run_script");
        const auto info = call("read_tool", { { "tool", "get_project_info" } });
        TEST_CHECK(info.contains("result"), "Test 200: outil de lecture de l'IA");
        TEST_CHECK(call("read_tool", { { "tool", "propose_section_change" } }).contains("error"), "Test 200: proposition refusée (lecture seule)");
        TEST_CHECK(call("model_summary").value("result").isObject(), "Test 200: contexte d'ingénierie");
        TEST_CHECK(call("undo").value("result").toObject().value("ok").toBool() && session.model().nodes().size() == 2,
                   "Test 200: undo (le script est annulable)");

        // Pont MCP réel : JSON-RPC sur stdin / stdout, relayé par le canal local.
        const QString channel = QStringLiteral("testsuite%1").arg(QCoreApplication::applicationPid());
        QString error;
        TEST_CHECK(server.start(QStringLiteral("tsaraloha-") + channel, &error), "Test 200: canal local (" << error.toStdString() << ")");
        QProcess bridge;
        bridge.start(QStringLiteral(TSARALOHA_MCP_EXE), { QStringLiteral("--app"), channel });
        TEST_CHECK(bridge.waitForStarted(5000), "Test 200: tsaraloha-mcp démarré");
        int rpcId = 0;
        auto rpc = [&](const QString& method, const QJsonObject& params) {
            const QJsonObject msg { { "jsonrpc", "2.0" }, { "id", ++rpcId }, { "method", method }, { "params", params } };
            bridge.write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
            QElapsedTimer t;
            t.start();
            // Le serveur vit dans ce processus : la boucle d'événements doit tourner pendant l'attente.
            while (!bridge.canReadLine() && t.elapsed() < 20000)
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                bridge.waitForReadyRead(10);
            }
            return QJsonDocument::fromJson(bridge.readLine()).object();
        };
        const auto init = rpc("initialize", { { "protocolVersion", "2025-06-18" }, { "capabilities", QJsonObject {} },
                                              { "clientInfo", QJsonObject { { "name", "test" }, { "version", "1" } } } });
        TEST_CHECK(init.value("result").toObject().value("serverInfo").toObject().value("name").toString() == "tsaraloha",
                   "Test 200: MCP initialize");
        bridge.write("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n");
        const auto tools = rpc("tools/list", {}).value("result").toObject().value("tools").toArray();
        TEST_CHECK(tools.size() == 8, "Test 200: MCP tools/list (8 outils)");
        const auto status = rpc("tools/call", { { "name", "tsaraloha_status" }, { "arguments", QJsonObject {} } }).value("result").toObject();
        TEST_CHECK(!status.value("isError").toBool() && status.value("content").toArray()[0].toObject().value("text").toString().contains("TSA"),
                   "Test 200: MCP tsaraloha_status");
        const auto created = rpc("tools/call", { { "name", "execute_command" },
                                                 { "arguments", QJsonObject { { "command", "model.create_node" },
                                                                              { "arguments", QJsonObject { { "position", QJsonArray { 9, 0, 0 } } } } } } })
                                 .value("result").toObject();
        TEST_CHECK(!created.value("isError").toBool() && session.model().nodes().size() == 3, "Test 200: MCP execute_command modifie le projet");
        const auto refused = rpc("tools/call", { { "name", "execute_command" }, { "arguments", QJsonObject { { "command", "model.create_beam" },
                                                 { "arguments", QJsonObject { { "start", 1 }, { "end", 999 } } } } } }).value("result").toObject();
        TEST_CHECK(refused.value("isError").toBool(), "Test 200: MCP échec métier signalé comme erreur d'outil");
        bridge.closeWriteChannel();
        TEST_CHECK(bridge.waitForFinished(5000), "Test 200: tsaraloha-mcp se termine à la fin de l'entrée");
        server.stop();
        std::cout << "[PASS] Test 200: serveur d'automatisation et pont MCP" << std::endl;
        ++passed;
    }
    return true;
}
