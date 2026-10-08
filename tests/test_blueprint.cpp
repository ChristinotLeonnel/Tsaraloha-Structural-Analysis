// Suite « blueprint » : programmation visuelle (ADR-024, phases 6 et 8) — tests 193 à 195, 197, 198.
#include "test_common.h"

#include "AI/Tools/AIToolRegistry.h"
#include "Automation/CommandRegistry.h"
#include "Blueprint/BlueprintFile.h"
#include "Blueprint/BlueprintScript.h"
#include "Blueprint/BlueprintRuntime.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

namespace
{
using namespace TSA::Blueprint;

/// Portique paramétrique : Pour i = 0..n-1 → nœud (i·portée, 0, 0) ; puis résumé du modèle.
Graph portalBlueprint(const NodeLibrary& lib, int& summaryNode, int& loopNode)
{
    Graph g;
    g.name = "Nœuds en ligne";
    const int start = g.addNode("event.start");
    const int count = g.addNode("param.integer");
    g.setValue(count, "name", std::string("nombre"));
    g.setValue(count, "value", 4LL);
    const int span = g.addNode("param.real");
    g.setValue(span, "name", std::string("portée"));
    g.setValue(span, "value", 5.0);
    const int last = g.addNode("math.subtract");
    g.setValue(last, "b", 1.0);
    const int lastInt = g.addNode("math.to_integer");
    loopNode = g.addNode("flow.for");
    const int x = g.addNode("math.multiply");
    const int point = g.addNode("math.make_point");
    const int create = g.addNode("cmd.model.create_node");
    summaryNode = g.addNode("cmd.query.model_summary");

    auto link = [&](int a, const char* out, int b, const char* in) {
        std::string why;
        if (!lib.connect(g, { a, out, b, in }, &why)) throw std::runtime_error(why);
    };
    link(start, "then", loopNode, "exec");
    link(count, "value", last, "a");            // entier → réel admis
    link(last, "result", lastInt, "value");
    link(lastInt, "result", loopNode, "last");
    link(loopNode, "index", x, "a");
    link(span, "value", x, "b");
    link(x, "result", point, "x");
    link(loopNode, "body", create, "exec");
    link(point, "point", create, "position");
    link(loopNode, "completed", summaryNode, "exec");
    return g;
}
} // namespace

bool runSuite_Blueprint(int& passed)
{
    const NodeLibrary& lib = NodeLibrary::standard();

    // TEST 193 : bibliothèque — commandes du registre exposées comme nœuds, typage des liens
    {
        TEST_CHECK(lib.find("event.start") && lib.find("flow.for") && lib.find("math.add") && lib.find("science.validation_bench"),
                   "Test 193: nœuds standard");
        for (const auto* spec : TSA::Automation::CommandRegistry::builtIn().commands())
            TEST_CHECK(lib.find("cmd." + spec->id), "Test 193: chaque commande du registre est un nœud (" << spec->id << ")");
        const auto* create = lib.find("cmd.model.create_node");
        TEST_CHECK(create->input("exec") && create->input("position") && create->output("then") && create->output("id"),
                   "Test 193: broches générées depuis la commande");

        Graph g;
        const int a = g.addNode("math.add"), p = g.addNode("math.make_point"), s = g.addNode("event.start"),
                  print = g.addNode("debug.print");
        std::string why;
        TEST_CHECK(!lib.connect(g, { p, "point", a, "a" }, &why), "Test 193: point → réel refusé");
        TEST_CHECK(!lib.connect(g, { s, "then", a, "a" }, &why), "Test 193: exécution → donnée refusé");
        TEST_CHECK(lib.connect(g, { a, "result", p, "x" }, &why), "Test 193: réel → réel accepté");
        TEST_CHECK(lib.connect(g, { p, "point", print, "value" }, &why), "Test 193: toute donnée → entrée générique");
        TEST_CHECK(lib.connect(g, { a, "result", p, "y" }) && g.links().size() == 3, "Test 193: une sortie alimente plusieurs entrées");
        TEST_CHECK(lib.connect(g, { a, "result", p, "x" }) && g.links().size() == 3, "Test 193: une entrée n'a qu'une source");

        Graph cyc;
        const int m1 = cyc.addNode("math.add"), m2 = cyc.addNode("math.add");
        lib.connect(cyc, { m1, "result", m2, "a" });
        lib.connect(cyc, { m2, "result", m1, "a" });
        TEST_CHECK(!lib.validate(cyc).empty(), "Test 193: cycle de données détecté");
        std::cout << "[PASS] Test 193: bibliothèque de nœuds (commandes du registre, typage, validation)" << std::endl;
        ++passed;
    }

    // TEST 194 : exécution sur un projet — boucle, maths, commandes, paramètres, profil, Annuler
    {
        TSA::Project::ProjectSession session;
        int summary = 0, loop = 0;
        Graph g = portalBlueprint(lib, summary, loop);
        TEST_CHECK(lib.validate(g).empty(), "Test 194: Blueprint valide");

        Runner runner(lib, g, &session);
        const ExecutionReport r = runner.run();
        TEST_CHECK(r.ok, "Test 194: exécution (" << r.message << ")");
        TEST_CHECK(session.model().nodes().size() == 4, "Test 194: 4 nœuds créés par la boucle");
        TEST_CHECK(session.model().getNode(4) && std::abs(session.model().getNode(4)->x() - 15.0) < 1e-12,
                   "Test 194: position calculée (index × portée)");
        TEST_CHECK(r.profile.at(loop).executions == 1 && r.profile.count(summary) && r.profile.at(summary).executions == 1,
                   "Test 194: profil (exécutions par nœud)");
        TEST_CHECK(!r.log.empty() && r.log.back().find("4 nœud") != std::string::npos, "Test 194: journal des commandes");

        // Reconstruction paramétrique : mêmes nœuds de graphe, autre valeur de paramètre
        TSA::Project::ProjectSession other;
        Runner rerun(lib, g, &other);
        const auto r2 = rerun.run({ { "nombre", 2LL }, { "portée", 7.5 } });
        TEST_CHECK(r2.ok && other.model().nodes().size() == 2 && std::abs(other.model().getNode(2)->x() - 7.5) < 1e-12,
                   "Test 194: paramètres imposés (nombre = 2, portée = 7,5)");

        // Une entrée Annuler par commande exécutée
        TEST_CHECK(session.undo() && session.model().nodes().size() == 3, "Test 194: Annuler le dernier nœud créé par le Blueprint");

        // Erreur : commande refusée → exécution arrêtée, nœud fautif signalé, modèle intact
        Graph bad;
        const int s = bad.addNode("event.start"), beam = bad.addNode("cmd.model.create_beam");
        lib.connect(bad, { s, "then", beam, "exec" });
        bad.setValue(beam, "start", 1LL);
        bad.setValue(beam, "end", 999LL);
        const std::size_t before = session.model().beams().size();
        const auto r3 = Runner(lib, bad, &session).run();
        TEST_CHECK(!r3.ok && r3.failedNode == beam && session.model().beams().size() == before,
                   "Test 194: échec d'une commande signalé sur son nœud, modèle inchangé");

        // Garde-fou : boucle infinie bornée
        Graph inf;
        const int s2 = inf.addNode("event.start"), w = inf.addNode("flow.while");
        inf.setValue(w, "condition", true);
        lib.connect(inf, { s2, "then", w, "exec" });
        ExecutionLimits lim;
        lim.maxIterations = 50;
        const auto r4 = Runner(lib, inf, &session, lim).run();
        TEST_CHECK(!r4.ok && r4.failedNode == w, "Test 194: boucle infinie interrompue");
        std::cout << "[PASS] Test 194: exécution (boucle, commandes, paramètres, profil, erreurs)" << std::endl;
        ++passed;
    }

    // TEST 195 : fichier .tsbp — aller-retour, compatibilité de version
    {
        int summary = 0, loop = 0;
        const Graph g = portalBlueprint(lib, summary, loop);
        QTemporaryDir dir;
        const QString path = dir.filePath("portique.tsbp");
        QString error;
        TEST_CHECK(saveFile(g, path, &error), "Test 195: enregistrement");
        Graph back;
        TEST_CHECK(loadFile(path, back, &error), "Test 195: relecture");
        TEST_CHECK(back.nodes().size() == g.nodes().size() && back.links().size() == g.links().size() && back.name == g.name,
                   "Test 195: nœuds, liens et nom conservés");
        TEST_CHECK(std::get<long long>(back.node(2)->values.at("value")) == 4 && std::get<std::string>(back.node(2)->values.at("name")) == "nombre",
                   "Test 195: valeurs typées conservées");
        TSA::Project::ProjectSession session;
        TEST_CHECK(Runner(lib, back, &session).run().ok && session.model().nodes().size() == 4, "Test 195: le Blueprint relu s'exécute");

        Graph tmp;
        TEST_CHECK(!fromJson(R"({"format":"tsbp","version":99,"nodes":[],"links":[]})", tmp, &error), "Test 195: version future refusée");
        TEST_CHECK(!fromJson(R"({"format":"autre"})", tmp, &error), "Test 195: autre format refusé");
        TEST_CHECK(fromJson(R"({"format":"tsbp","version":1,"nodes":[{"id":1,"type":"plugin.inconnu"}],"links":[]})", tmp, &error)
                       && !lib.validate(tmp).empty(),
                   "Test 195: nœud inconnu chargé puis signalé à la validation");
        std::cout << "[PASS] Test 195: fichier .tsbp" << std::endl;
        ++passed;
    }

    // TEST 197 : débogueur — points d'arrêt, pas à pas, valeurs produites, interruption, arrêt demandé
    {
        int summary = 0, loop = 0;
        const Graph g = portalBlueprint(lib, summary, loop);
        TEST_CHECK(lib.usesProject(g), "Test 197: graphe à commandes = exécution dans le thread de l'interface");

        // Point d'arrêt sur le résumé : une seule pause, après les 4 créations de nœuds.
        {
            TSA::Project::ProjectSession session;
            Runner runner(lib, g, &session);
            int pauses = 0, pausedNode = 0;
            std::size_t nodesAtPause = 0, outputsAtPause = 0;
            BreakpointDebugger dbg([&](const DebugState& s) {
                ++pauses;
                pausedNode = s.node;
                nodesAtPause = session.model().nodes().size();
                outputsAtPause = s.outputs ? s.outputs->size() : 0;
                return DebugAction::Continue;
            }, { summary });
            runner.setDebugger(&dbg);
            const auto r = runner.run();
            TEST_CHECK(pausedNode == summary, "Test 197: pause sur le nœud du point d'arrêt");
            TEST_CHECK(r.ok && pauses == 1 && nodesAtPause == 4 && outputsAtPause > 0,
                       "Test 197: arrêt au point d'arrêt, projet et valeurs visibles pendant la pause");
        }
        // Pas à pas depuis le début : une pause par nœud d'action (Pour, 4 × créer, résumé).
        {
            TSA::Project::ProjectSession session;
            Runner runner(lib, g, &session);
            std::vector<int> visited;
            BreakpointDebugger dbg([&](const DebugState& s) { visited.push_back(s.node); return DebugAction::Step; }, {}, true);
            runner.setDebugger(&dbg);
            TEST_CHECK(runner.run().ok, "Test 197: exécution pas à pas");
            TEST_CHECK(visited.size() == 7 && visited[1] == loop && visited.back() == summary,
                       "Test 197: Début, Pour, 4 créations, résumé visités dans l'ordre");
        }
        // Interruption pendant une pause : aucune modification après l'arrêt.
        {
            TSA::Project::ProjectSession session;
            Runner runner(lib, g, &session);
            int created = 0;
            BreakpointDebugger dbg([&](const DebugState& s) {
                const auto* n = g.node(s.node);
                if (n && n->type == "cmd.model.create_node" && ++created == 3) return DebugAction::Abort;
                return DebugAction::Step;
            }, {}, true);
            runner.setDebugger(&dbg);
            const auto r = runner.run();
            TEST_CHECK(!r.ok && r.message.find("interrompue") != std::string::npos && session.model().nodes().size() == 2,
                       "Test 197: interruption au 3e nœud (2 nœuds créés)");
        }
        // Arrêt demandé (autre thread) : l'exécution s'arrête avant le nœud suivant.
        {
            TSA::Project::ProjectSession session;
            Runner runner(lib, g, &session);
            runner.requestStop();
            const auto r = runner.run();   // run() réarme l'arrêt : exécution complète
            TEST_CHECK(r.ok, "Test 197: un arrêt demandé avant run() n'affecte pas l'exécution suivante");
            TSA::Project::ProjectSession other;
            Runner stopped(lib, g, &other);
            BreakpointDebugger dbg2([&](const DebugState&) {
                stopped.requestStop();
                return DebugAction::Continue;
            }, { loop });
            stopped.setDebugger(&dbg2);
            const auto r2 = stopped.run();
            TEST_CHECK(!r2.ok && r2.message.find("arrêtée") != std::string::npos && other.model().nodes().empty(),
                       "Test 197: arrêt demandé pendant l'exécution");
        }
        // Graphe sans accès au projet : exécutable en tâche de fond.
        Graph pure;
        const int s = pure.addNode("event.start");
        const int print = pure.addNode("debug.print");
        pure.setValue(print, "value", std::string("bonjour"));
        std::string why;
        TEST_CHECK(lib.connect(pure, { s, "then", print, "exec" }, &why) && !lib.usesProject(pure), "Test 197: graphe pur détecté");
        std::cout << "[PASS] Test 197: débogueur Blueprint" << std::endl;
        ++passed;
    }

    // TEST 198 : pont texte ↔ Blueprint (scripts, IA) et proposition de Blueprint par l'IA
    {
        const auto& commands = TSA::Automation::CommandRegistry::builtIn();
        const std::string script =
            "# portique\n"
            "a = model.create_node position=0,0,0\n"
            "b = model.create_node position=0,0,3\n"
            "c = model.create_node position=6,0,3\n"
            "model.create_column start=a.id end=b.id\n"
            "poutre = model.create_beam start=b.id end=c.id section=\"IPE 300\"\n"
            "cas = loads.create_case name=Exploitation category=live\n"
            "loads.add_uniform beam=poutre.id case=cas.id q=12.5\n";
        Graph g;
        std::string error;
        TEST_CHECK(fromCommandScript(script, lib, commands, g, &error), "Test 198: script → Blueprint (" << error << ")");
        TEST_CHECK(g.nodes().size() == 8 && lib.validate(g).empty(), "Test 198: Début + 7 commandes, graphe valide");
        int dataLinks = 0;
        for (const auto& l : g.links()) dataLinks += l.fromPin == "id" ? 1 : 0;
        TEST_CHECK(dataLinks == 6, "Test 198: références « a.id » → liens de données");

        TSA::Project::ProjectSession session;
        const auto r = Runner(lib, g, &session).run();
        const auto& m = session.model();
        TEST_CHECK(r.ok && m.nodes().size() == 3 && m.beams().size() == 2 && m.loadManager().memberLoads().size() == 1,
                   "Test 198: Blueprint du script exécuté (3 nœuds, poteau + poutre, charge)");

        // Aller-retour : le script régénéré redonne le même graphe exécutable.
        std::vector<std::string> warnings;
        const std::string back = toCommandScript(g, lib, &warnings);
        Graph g2;
        TEST_CHECK(warnings.empty() && fromCommandScript(back, lib, commands, g2, &error) && g2.nodes().size() == g.nodes().size()
                       && g2.links().size() == g.links().size(),
                   "Test 198: Blueprint → script → Blueprint (" << error << ")");
        TEST_CHECK(back.find("section=\"IPE 300\"") != std::string::npos && back.find("=n") != std::string::npos,
                   "Test 198: valeurs et références réécrites");
        TEST_CHECK(describe(g, lib).find("Créer une poutre") != std::string::npos, "Test 198: description pour l'IA");

        Graph bad;
        TEST_CHECK(!fromCommandScript("model.create_beam start=x.id end=2", lib, commands, bad, &error), "Test 198: référence inconnue refusée");
        TEST_CHECK(!fromCommandScript("model.inconnue a=1", lib, commands, bad, &error) && error.find("ligne 1") != std::string::npos,
                   "Test 198: commande inconnue refusée avec le numéro de ligne");
        warnings.clear();
        int s0 = 0, l0 = 0;
        toCommandScript(portalBlueprint(lib, s0, l0), lib, &warnings);
        TEST_CHECK(!warnings.empty(), "Test 198: boucle non exprimable en script signalée");

        // IA : proposition vérifiée, rien n'est exécuté avant l'acceptation.
        TSA::Project::ProjectSession ai;
        TSA::AI::AIToolRegistry tools([&] { TSA::AI::EngineeringSources src; src.model = &ai.model(); return src; }, nullptr);
        const auto list = tools.execute("list_commands", "{}");
        TEST_CHECK(list.ok && list.data["commands"].toArray().size() >= 10, "Test 198: list_commands");
        QJsonObject args { { "title", "Portique" }, { "script", QString::fromStdString(script) }, { "rationale", "test" } };
        const auto prop = tools.execute("propose_blueprint", QString::fromUtf8(QJsonDocument(args).toJson()));
        TEST_CHECK(prop.ok && prop.proposal && prop.proposal->tool == "propose_blueprint" && ai.model().nodes().empty(),
                   "Test 198: propose_blueprint crée une proposition sans exécuter");
        args["script"] = "model.create_beam start=1";
        const auto badProp = tools.execute("propose_blueprint", QString::fromUtf8(QJsonDocument(args).toJson()));
        TEST_CHECK(!badProp.ok && badProp.error.contains("Script invalide"), "Test 198: script invalide renvoyé à l'IA");
        std::cout << "[PASS] Test 198: script de commandes ↔ Blueprint, proposition de l'IA" << std::endl;
        ++passed;
    }
    return true;
}
