// Opérateur IA (src/AI/Operator) : politique d'actions, exécution par le registre central, vérification de l'état du
// modèle, outil « execute_command », calcul réel. Tests 481-486 : sans modèle de langage (aucune réponse simulée n'est
// nécessaire, l'opérateur est appelé comme le ferait l'orchestrateur).
// Test 487 (suite « ailive ») : scénario de bout en bout avec un VRAI modèle de langage, exécuté seulement si
// TSA_AI_LIVE_URL est défini (serveur compatible OpenAI : llama-server local, Gemini, OpenRouter…) ; sinon « SKIP ».

#include "test_common.h"

#include "AI/Core/AIOrchestrator.h"
#include "AI/Operator/AIOperator.h"
#include "AI/Python/PythonAIBridge.h"
#include "AI/Tools/AIToolRegistry.h"
#include "Analysis/AnalysisController.h"
#include "Automation/AutomationServer.h"
#include "Model/Model.h"
#include "Project/ProjectSession.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

using namespace TSA::AI;

namespace
{
QJsonObject obj(std::initializer_list<QPair<QString, QJsonValue>> items)
{
    QJsonObject o;
    for (const auto& [k, v] : items) o[k] = v;
    return o;
}
} // namespace

bool runSuite_AIOperator(int& passed)
{
    // TEST 481 : politique — lecture permise, modification refusée hors mode opérateur, commande inconnue refusée
    {
        TSA::Project::ProjectSession session;
        AIOperator op([&] { return &session; });
        TEST_CHECK(!op.actEnabled(), "Test 481: mode opérateur désactivé par défaut");
        const auto read = op.execute("query.model_summary", {});
        TEST_CHECK(read.executed && read.verified && !read.refused, "Test 481: lecture exécutée hors mode opérateur");
        const size_t nodes = session.model().nodes().size();
        const auto denied = op.execute("model.create_node", obj({ { "position", QJsonArray { 0, 0, 0 } } }));
        TEST_CHECK(denied.refused && !denied.executed && session.model().nodes().size() == nodes,
                   "Test 481: création refusée hors mode opérateur, modèle inchangé");
        TEST_CHECK(op.execute("model.delete_everything", {}).refused, "Test 481: commande hors registre refusée");
        AIOperator orphan([] { return nullptr; });
        orphan.setActEnabled(true);
        TEST_CHECK(orphan.execute("query.model_summary", {}).refused, "Test 481: aucun projet — refus explicite");
        TEST_CHECK(op.journal().size() == 3 && AIOperator::isModifying("analysis.run") && !AIOperator::isModifying("query.node"),
                   "Test 481: journal complet ; calcul classé comme action");
        std::cout << "[PASS] Test 481: Opérateur IA — politique d'actions" << std::endl;
        ++passed;
    }

    // TEST 482 : exécution vérifiée — nœuds, poteau, appui ; arguments en texte ; erreurs sans effet ; Annuler
    {
        TSA::Project::ProjectSession session;
        AIOperator op([&] { return &session; });
        op.setActEnabled(true);
        int observed = 0;
        op.setObserver([&](const OperatorAction&) { ++observed; });

        const auto n1 = op.execute("model.create_node", obj({ { "position", QJsonArray { 0, 0, 0 } } }));
        const auto n2 = op.execute("model.create_node", obj({ { "position", "0,0,3" } })); // syntaxe de la ligne de commande
        TEST_CHECK(n1.executed && n1.verified && n2.executed && n2.verified, "Test 482: nœuds créés et vérifiés (tableau et texte)");
        TEST_CHECK(n2.verification.value("position_matches").toBool(), "Test 482: position relue dans le modèle");
        const int a = n1.outputs.value("id").toInt(), b = n2.outputs.value("id").toInt();
        const auto col = op.execute("model.create_column", obj({ { "start", a }, { "end", b }, { "section", "HEB 200" } }));
        TEST_CHECK(col.executed && col.verified && col.verification.value("start").toInt() == a, "Test 482: poteau créé, extrémités relues");
        const auto sup = op.execute("model.set_support", obj({ { "node", a }, { "type", "fixed" } }));
        TEST_CHECK(sup.verified && session.model().getNode(a)->supportType() == TSA::Model::SupportType::Fixed, "Test 482: encastrement vérifié");

        const size_t nodes = session.model().nodes().size();
        const auto badParam = op.execute("model.create_node", obj({ { "position", QJsonArray { 1, 0, 0 } }, { "couleur", "rouge" } }));
        TEST_CHECK(badParam.refused && badParam.message.contains("position"), "Test 482: paramètre inconnu refusé, paramètres valides indiqués");
        const auto badType = op.execute("model.create_node", obj({ { "position", "abc" } }));
        TEST_CHECK(badType.refused && session.model().nodes().size() == nodes, "Test 482: valeur invalide refusée, modèle inchangé");
        const auto missing = op.execute("model.create_beam", obj({ { "start", a }, { "end", 999 } }));
        TEST_CHECK(!missing.executed && !missing.verified && missing.toJson().value("executed") == false,
                   "Test 482: échec de la commande rendu comme échec (nœud inexistant)");

        TEST_CHECK(session.undo() && session.model().getNode(a)->supportType() != TSA::Model::SupportType::Fixed,
                   "Test 482: dernière action de l'IA annulable (Ctrl+Z)");
        TEST_CHECK(observed == static_cast<int>(op.journal().size()) && observed == 7, "Test 482: chaque action notifiée à l'interface");
        std::cout << "[PASS] Test 482: Opérateur IA — exécution par le registre et vérification du modèle" << std::endl;
        ++passed;
    }

    // TEST 483 : limite d'actions par demande, remise à zéro à chaque demande
    {
        TSA::Project::ProjectSession session;
        AIOperator op([&] { return &session; });
        op.setActEnabled(true);
        op.setMaxActionsPerRequest(2);
        op.beginRequest();
        TEST_CHECK(op.execute("model.create_node", obj({ { "position", "0,0,0" } })).executed &&
                       op.execute("model.create_node", obj({ { "position", "1,0,0" } })).executed,
                   "Test 483: actions dans la limite exécutées");
        const auto third = op.execute("model.create_node", obj({ { "position", "2,0,0" } }));
        TEST_CHECK(third.refused && third.message.contains("Limite"), "Test 483: action au-delà de la limite refusée");
        TEST_CHECK(op.execute("query.model_summary", {}).executed, "Test 483: lecture toujours permise au-delà de la limite");
        op.beginRequest();
        TEST_CHECK(op.execute("model.create_node", obj({ { "position", "2,0,0" } })).executed, "Test 483: nouvelle demande — compteur remis à zéro");
        std::cout << "[PASS] Test 483: Opérateur IA — limite d'actions par demande" << std::endl;
        ++passed;
    }

    // TEST 484 : outil « execute_command » du registre d'outils de l'IA (définition, arguments JSON texte, résultat)
    {
        TSA::Project::ProjectSession session;
        AIToolRegistry tools([&] { EngineeringSources s; s.model = &session.model(); return s; }, nullptr);
        auto hasTool = [&](const QString& name) {
            for (const auto& t : tools.toolDefinitions())
                if (t.toObject()["function"].toObject()["name"].toString() == name) return true;
            return false;
        };
        TEST_CHECK(!hasTool("execute_command"), "Test 484: aucun outil d'action sans opérateur branché");
        TEST_CHECK(!tools.execute("execute_command", R"({"command":"query.model_summary","arguments":{}})").ok,
                   "Test 484: outil d'action refusé sans opérateur");
        AIOperator op([&] { return &session; });
        op.setActEnabled(true);
        tools.setOperator(&op);
        TEST_CHECK(hasTool("execute_command") && AIToolRegistry::kindOf("execute_command") == ToolKind::Action, "Test 484: outil d'action déclaré");
        TEST_CHECK(tools.toolDefinitions().size() == AIToolRegistry::whitelist().size() + 1,
                   "Test 484: définitions = liste blanche + outil d'action, rien d'autre");
        const auto out = tools.execute("execute_command", R"({"command":"model.create_node","arguments":"{\"position\":[2,0,1]}"})");
        TEST_CHECK(out.ok && out.data.value("executed").toBool() && out.data.value("verified").toBool(),
                   "Test 484: arguments transmis en texte JSON acceptés, résultat vérifié");
        op.setActEnabled(false);
        const auto refused = tools.execute("execute_command", R"({"command":"model.create_node","arguments":{"position":[3,0,1]}})");
        TEST_CHECK(refused.ok && refused.data.value("refused").toBool() && !refused.data.value("executed").toBool(),
                   "Test 484: refus rendu à l'IA (jamais masqué en succès)");
        std::cout << "[PASS] Test 484: Opérateur IA — outil execute_command" << std::endl;
        ++passed;
    }

    // TEST 485 : vérification non conforme signalée (le résultat n'est jamais présenté comme un succès)
    {
        OperatorAction a;
        a.command = "model.create_node";
        a.executed = true;
        a.verified = false;
        TEST_CHECK(a.toJson().contains("warning") && !a.toJson().value("verified").toBool(), "Test 485: exécution non vérifiée signalée à l'IA");
        TEST_CHECK(AIOperator::commandCatalog().contains("model.create_node(position:") && AIOperator::commandIds().contains("analysis.run"),
                   "Test 485: catalogue compact des commandes (signatures typées)");
        std::cout << "[PASS] Test 485: Opérateur IA — signalement d'une vérification non conforme" << std::endl;
        ++passed;
    }

    // TEST 486 : scénario complet par l'opérateur — console, appui, cas, charge, calcul réel (Custom2D), résultats
    {
        TSA::Project::ProjectSession session;
        session.grids().clearAllGrids();
        AIOperator op([&] { return &session; });
        op.setActEnabled(true);
        const auto a = op.execute("model.create_node", obj({ { "position", QJsonArray { 0, 0, 0 } } }));
        const auto b = op.execute("model.create_node", obj({ { "position", QJsonArray { 4, 0, 0 } } }));
        const int na = a.outputs.value("id").toInt(), nb = b.outputs.value("id").toInt();
        const auto beam = op.execute("model.create_beam", obj({ { "start", na }, { "end", nb }, { "section", "IPE 300" } }));
        const auto sup = op.execute("model.set_support", obj({ { "node", na }, { "type", "fixed" } }));
        const auto lc = op.execute("loads.create_case", obj({ { "name", "P" }, { "category", "live" } }));
        const auto load = op.execute("loads.add_nodal", obj({ { "node", nb }, { "case", lc.outputs.value("id").toInt() }, { "fz", -10 } }));
        TEST_CHECK(beam.verified && sup.verified && lc.verified && load.verified, "Test 486: modèle construit et vérifié par l'opérateur");
        const auto run = op.execute("analysis.run", obj({ { "engine", "custom2d" }, { "axis", "plan" }, { "self_weight", "non" }, { "case", "P" } }));
        TEST_CHECK(run.executed && run.verified && run.verification.value("results_up_to_date").toBool(),
                   "Test 486: calcul exécuté, résultats publiés et à jour (" << run.message.toStdString() << ")");
        const auto* bar = session.model().getBeam(beam.outputs.value("id").toInt());
        const double EI = bar->material().mechanical.E() * 1e-3 * bar->section().iy();
        const double expected = 10.0 * 64.0 / (3.0 * EI);
        TEST_CHECK(std::abs(run.outputs.value("max_displacement").toDouble() - expected) <= 1e-3 * expected,
                   "Test 486: flèche calculée par le moteur = P·L³/3EI");
        const auto res = op.execute("results.summary", {});
        TEST_CHECK(res.executed && res.outputs.value("up_to_date").toBool(), "Test 486: résultats relus par une commande de lecture");
        std::cout << "[PASS] Test 486: Opérateur IA — construction, calcul et lecture des résultats réels" << std::endl;
        ++passed;
    }
    // TEST 488 : canal de la couche IA Python — méthodes operator_* du serveur d'automatisation (même opérateur)
    {
        TSA::Project::ProjectSession session;
        AIOperator op([&] { return &session; });
        TSA::Automation::AutomationServer server(&session);
        auto call = [&](const QString& method, const QJsonObject& params = {}) {
            return server.handle(QJsonObject { { "id", 1 }, { "method", method }, { "params", params } });
        };
        TEST_CHECK(call("operator_status").contains("error"), "Test 488: sans opérateur branché — erreur explicite");
        server.setOperator(&op);
        TEST_CHECK(!call("operator_status")["result"].toObject().value("act_enabled").toBool(), "Test 488: statut lu (mode opérateur désactivé)");
        const QJsonObject create { { "command", "model.create_node" }, { "arguments", QJsonObject { { "position", QJsonArray { 1, 2, 3 } } } } };
        TEST_CHECK(call("operator_execute", create)["result"].toObject().value("refused").toBool() && session.model().nodes().empty(),
                   "Test 488: le client ne peut pas modifier tant que l'utilisateur n'a pas activé le mode opérateur");
        op.setActEnabled(true); // geste de l'utilisateur dans le panneau
        TEST_CHECK(call("operator_begin")["result"].toObject().value("ok").toBool(), "Test 488: nouvelle demande");
        const QJsonObject r = call("operator_execute", create)["result"].toObject();
        TEST_CHECK(r.value("executed").toBool() && r.value("verified").toBool() && session.model().nodes().size() == 1,
                   "Test 488: action exécutée et vérifiée par le canal");
        std::cout << "[PASS] Test 488: Opérateur IA — canal du client Python (operator_status, operator_begin, operator_execute)" << std::endl;
        ++passed;
    }

    // TEST 489 : pont vers la couche IA Python (vrai processus « python -m tsa_ai serve », aucun modèle lancé)
    if (PythonAIBridge::findPython().isEmpty() || PythonAIBridge::packageRoot().isEmpty())
        std::cout << "[SKIP] Test 489: Python ou python/tsa_ai introuvable — pont non testé" << std::endl;
    else
    {
        PythonAIBridge bridge;
        QList<QJsonObject> events;
        QEventLoop loop;
        QObject::connect(&bridge, &PythonAIBridge::event, &loop, [&](const QJsonObject& e) { events << e; loop.quit(); });
        auto waitFor = [&](const QString& type, int ms) {
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(ms);
            while (timer.isActive())
            {
                for (const auto& e : events)
                    if (e.value("type").toString() == type) return e;
                loop.exec();
            }
            return QJsonObject {};
        };
        QString why;
        TEST_CHECK(bridge.start(QStringLiteral("tsaraloha-essai-sans-tsa"), QString(), &why), "Test 489: service Python démarré (" << why.toStdString() << ")");
        TEST_CHECK(!waitFor("ready", 20000).isEmpty(), "Test 489: service prêt");
        bridge.requestHardware("cpu");
        const QJsonObject hw = waitFor("hardware", 60000);
        TEST_CHECK(hw.value("choice").toObject().value("device").toString() == "none", "Test 489: choix manuel CPU respecté (détection seule)");
        bridge.configure("local", "", "");
        TEST_CHECK(waitFor("configured", 10000).value("remote").toBool() == false, "Test 489: fournisseur local configuré, aucune clé requise");
        bridge.chat("lis le modèle");
        const QJsonObject err = waitFor("error", 20000);
        TEST_CHECK(err.value("kind").toString() == "tsa", "Test 489: TSA injoignable — erreur explicite, aucun faux succès");
        bridge.stop();
        TEST_CHECK(!bridge.isRunning(), "Test 489: service arrêté");
        std::cout << "[PASS] Test 489: Pont vers la couche IA Python (démarrage, matériel, configuration, erreur, arrêt)" << std::endl;
        ++passed;
    }
    return true;
}

bool runSuite_AIOperatorLive(int& passed)
{
    // TEST 487 : VRAI modèle de langage (aucune simulation) piloté par l'orchestrateur de TSA, en mode opérateur.
    const QString url = qEnvironmentVariable("TSA_AI_LIVE_URL");
    if (url.isEmpty())
    {
        std::cout << "[SKIP] Test 487: TSA_AI_LIVE_URL non défini — aucun fournisseur réel configuré, test non exécuté" << std::endl;
        return true;
    }
    const QString model = qEnvironmentVariable("TSA_AI_LIVE_MODEL", "default");
    const int timeoutS = qEnvironmentVariableIntValue("TSA_AI_LIVE_TIMEOUT_S") > 0 ? qEnvironmentVariableIntValue("TSA_AI_LIVE_TIMEOUT_S") : 900;

    TSA::Project::ProjectSession session;
    session.grids().clearAllGrids();
    AIOrchestrator orch;
    orch.setSourcesProvider([&] { EngineeringSources s; s.model = &session.model(); s.projectName = "essai-operateur"; return s; });
    AIOperator op([&] { return &session; });
    op.setActEnabled(true);
    orch.setOperator(&op);
    // Réglages EN MÉMOIRE seulement (jamais enregistrés) : point d'accès de l'essai, accord donné pour cet essai.
    auto& st = orch.settings();
    st.mode = AIMode::Cloud;
    st.cloud.preset = qEnvironmentVariable("TSA_AI_LIVE_PRESET", "openai-compatible");
    st.cloud.baseUrl = url;
    st.cloud.model = model;
    st.autoAllowCloud = true;
    orch.reloadCloudProvider();
    TEST_CHECK(orch.cloudConfigured(), "Test 487: fournisseur configuré (" << url.toStdString() << ")");

    const QString request = QStringLiteral(
        "Construis dans le projet ouvert une console : un nœud en (0,0,0) et un nœud en (4,0,0), une poutre IPE 300 entre eux, "
        "un encastrement (fixed) sur le nœud en (0,0,0). Crée un cas de charge nommé P de catégorie live, puis une charge nodale "
        "fz = -10 kN sur le nœud en (4,0,0) dans ce cas. Lance ensuite le calcul avec engine=custom2d, axis=plan, "
        "self_weight=non, case=P. Termine par un bilan : identifiants créés et flèche maximale calculée par TSA.");
    QString finalText, error;
    QStringList actions;
    QEventLoop loop;
    QObject::connect(&orch, &AIOrchestrator::assistantFinished, &loop, [&](const QString& t, const QString&) { finalText = t; loop.quit(); });
    QObject::connect(&orch, &AIOrchestrator::assistantFailed, &loop, [&](const QString& e) { error = e; loop.quit(); });
    QObject::connect(&orch, &AIOrchestrator::operatorActionPerformed, &loop, [&](const QJsonObject& a) {
        actions << QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
    });
    QTimer::singleShot(timeoutS * 1000, &loop, [&] { error = QStringLiteral("délai de %1 s dépassé").arg(timeoutS); loop.quit(); });
    QElapsedTimer clock;
    clock.start();
    orch.submit(AITask::Chat, request);
    loop.exec();
    const qint64 ms = clock.elapsed();

    std::cout << "  Modèle : " << model.toStdString() << " — durée " << ms / 1000.0 << " s — " << actions.size() << " action(s)" << std::endl;
    for (const auto& a : actions) std::cout << "  action : " << a.left(260).toStdString() << std::endl;
    std::cout << "  Réponse finale : " << finalText.left(1200).toStdString() << std::endl;
    TEST_CHECK(error.isEmpty(), "Test 487: réponse du fournisseur obtenue (" << error.toStdString() << ")");

    // Vérification INDÉPENDANTE de ce que dit l'IA : état réel du modèle et des résultats.
    const auto& m = session.model();
    int beamId = -1;
    for (const auto& [id, beam] : m.beams())
    {
        const auto* s = m.getNode(beam.startNodeId());
        const auto* e = m.getNode(beam.endNodeId());
        if (s && e && std::abs(std::abs(e->x() - s->x()) - 4.0) < 1e-9) beamId = id;
    }
    TEST_CHECK(m.nodes().size() == 2 && m.beams().size() == 1 && beamId > 0, "Test 487: deux nœuds et une poutre de 4 m dans le modèle");
    bool fixed = false;
    for (const auto& [id, n] : m.nodes()) fixed |= n.supportType() == TSA::Model::SupportType::Fixed && std::abs(n.x()) < 1e-9;
    TEST_CHECK(fixed, "Test 487: encastrement en (0,0,0)");
    TEST_CHECK(m.loadManager().nodalLoads().size() == 1, "Test 487: une charge nodale");
    TEST_CHECK(session.analysis().results() && session.analysis().resultsUpToDate(), "Test 487: calcul réalisé, résultats à jour");
    for (const auto& a : op.journal())
        TEST_CHECK(!(a.executed && !a.verified), "Test 487: aucune action exécutée sans vérification conforme (" << a.command.toStdString() << ")");
    std::cout << "[PASS] Test 487: Opérateur IA — scénario réel de bout en bout (" << model.toStdString() << ")" << std::endl;
    ++passed;
    return true;
}
