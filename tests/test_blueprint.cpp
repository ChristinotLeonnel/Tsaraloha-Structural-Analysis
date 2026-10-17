// Suite « blueprint » : programmation visuelle (ADR-024, phases 6 et 8) — tests 193 à 195, 197 à 199, 391
// (valeurs objet, réutilisation des résultats, variables, contrôles du graphe, fichier v2, copier-coller), 392
// (sous-graphes réutilisables), 393 (sous-graphes dans l'éditeur).
#include "test_common.h"

#include "AI/Tools/AIToolRegistry.h"
#include "Automation/CommandRegistry.h"
#include "Blueprint/BlueprintFile.h"
#include "Blueprint/BlueprintScript.h"
#include "Blueprint/BlueprintRuntime.h"
#include "Blueprint/BlueprintSubgraph.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"
#include "UI/Blueprint/BlueprintEditor.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <memory>

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

// --- Test 391 : objets de test et bibliothèque locale (la bibliothèque standard partagée n'est pas modifiée) ----

class TestVector final : public TSA::Automation::ObjectValue
{
public:
    explicit TestVector(std::vector<double> v) : values(std::move(v)) {}
    std::string typeName() const override { return "TestVector"; }
    std::string summary() const override { return "vecteur de " + std::to_string(values.size()) + " composantes"; }
    std::string details() const override
    {
        std::string s = "composantes :";
        for (double x : values) s += " " + std::to_string(x);
        return s;
    }
    bool equals(const ObjectValue& o) const override
    {
        const auto* t = dynamic_cast<const TestVector*>(&o);
        return t && t->values == values;
    }
    std::vector<double> values;
};

class TestOther final : public TSA::Automation::ObjectValue
{
public:
    std::string typeName() const override { return "TestOther"; }
    std::string summary() const override { return "autre objet"; }
    bool equals(const ObjectValue& o) const override { return dynamic_cast<const TestOther*>(&o) != nullptr; }
};

PinSpec testPin(const char* name, ValueType type, Value def = {}, const char* objectType = "")
{
    PinSpec p;
    p.name = name;
    p.label = name;
    p.type = type;
    p.defaultValue = std::move(def);
    p.objectType = objectType;
    return p;
}

/// Nœuds standard + test.make (pur, réutilisable : compte ses évaluations), test.sum, test.other, test.consume_other.
struct TestLibrary
{
    NodeLibrary lib;
    std::shared_ptr<int> makeCount = std::make_shared<int>(0);
    std::shared_ptr<int> sumCount = std::make_shared<int>(0);

    TestLibrary()
    {
        registerStandardNodes(lib, TSA::Automation::CommandRegistry::builtIn());
        NodeDefinition make;
        make.id = "test.make";
        make.title = "Fabriquer un vecteur";
        make.category = "Test";
        make.pure = true;
        make.cacheable = true;
        make.inputs = { testPin("n", ValueType::Integer, 3LL), testPin("value", ValueType::Real, 1.0) };
        make.outputs = { testPin("vector", ValueType::Object, {}, "TestVector") };
        lib.add(make, [count = makeCount](ExecutionContext& c) {
            ++*count;
            const long long n = c.integer("n");
            const double v = c.real("value");
            if (c.failed()) return false;
            c.setOutput("vector", ObjectRef(std::make_shared<TestVector>(std::vector<double>(static_cast<std::size_t>(n), v))));
            return true;
        });
        NodeDefinition sum;
        sum.id = "test.sum";
        sum.title = "Somme";
        sum.category = "Test";
        sum.pure = true;
        sum.cacheable = true;
        sum.inputs = { testPin("vector", ValueType::Object, {}, "TestVector") };
        sum.outputs = { testPin("sum", ValueType::Real) };
        lib.add(sum, [count = sumCount](ExecutionContext& c) {
            ++*count;
            const auto v = c.objectAs<TestVector>("vector");
            if (!v) return false;
            double s = 0.0;
            for (double x : v->values) s += x;
            c.setOutput("sum", s);
            return true;
        });
        NodeDefinition other;
        other.id = "test.other";
        other.title = "Autre objet";
        other.category = "Test";
        other.pure = true;
        other.outputs = { testPin("other", ValueType::Object, {}, "TestOther") };
        lib.add(other, [](ExecutionContext& c) {
            c.setOutput("other", ObjectRef(std::make_shared<TestOther>()));
            return true;
        });
        NodeDefinition consume;
        consume.id = "test.consume_other";
        consume.title = "Consommer un autre objet";
        consume.category = "Test";
        consume.pure = true;
        consume.inputs = { testPin("other", ValueType::Object, {}, "TestOther") };
        consume.outputs = { testPin("ok", ValueType::Bool) };
        lib.add(consume, [](ExecutionContext& c) {
            if (!c.object("other")) return false;
            c.setOutput("ok", true);
            return true;
        });
    }
};
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

    // TEST 199 : fermeture de l'éditeur avec un nœud sélectionné (BUG-039). La scène, enfant de l'éditeur,
    // est détruite après lui ; en se vidant elle émet selectionChanged : aucun slot de l'éditeur ne doit
    // être appelé (sinon assertion Qt « Called object is not of the correct type » à la fermeture de TSALab).
    {
        int summary = 0, loop = 0;
        TSA::Project::ProjectSession session;
        auto* editor = new TSA::UI::BlueprintEditor();
        editor->setSession(&session);
        editor->setGraph(portalBlueprint(lib, summary, loop));
        auto* view = editor->findChild<QGraphicsView*>();
        TEST_CHECK(view && view->scene(), "Test 199: scène de l'éditeur");
        int selected = 0;
        for (QGraphicsItem* item : view->scene()->items())
            if (item->flags() & QGraphicsItem::ItemIsSelectable)
            {
                item->setSelected(true);
                ++selected;
            }
        TEST_CHECK(selected > 0 && !view->scene()->selectedItems().isEmpty(), "Test 199: nœuds sélectionnés");
        delete editor;   // avant correction : assertion Qt (arrêt du programme)
        std::cout << "[PASS] Test 199: fermeture de l'éditeur Blueprint avec une sélection" << std::endl;
        ++passed;
    }

    // TEST 391 : valeurs objet typées, réutilisation des résultats (dans une exécution et d'une exécution à l'autre),
    // variables, cycle d'exécution, dépendances mal ordonnées, fichier v2 (commentaires, versions), copier-coller
    {
        TestLibrary t;
        const NodeLibrary& L = t.lib;
        std::string why;
        // 1. Typage des objets
        {
            Graph g;
            const int make = g.addNode("test.make"), sum = g.addNode("test.sum"), cons = g.addNode("test.consume_other");
            const int print = g.addNode("debug.print"), add = g.addNode("math.add");
            TEST_CHECK(L.connect(g, { make, "vector", sum, "vector" }, &why), "Test 391: objet relié à une entrée du même type");
            TEST_CHECK(!L.canConnect(g, { make, "vector", cons, "other" }, &why) && why.find("TestVector") != std::string::npos
                           && why.find("TestOther") != std::string::npos,
                       "Test 391: objets de types différents refusés (" << why << ")");
            TEST_CHECK(!L.canConnect(g, { make, "vector", add, "a" }, &why), "Test 391: objet vers réel refusé");
            TEST_CHECK(L.canConnect(g, { make, "vector", print, "value" }, &why), "Test 391: objet vers broche générique admis");
            const ObjectRef a = std::make_shared<TestVector>(std::vector<double> { 1.0, 2.0 });
            TEST_CHECK(TSA::Automation::formatValue(a) == "vecteur de 2 composantes", "Test 391: texte d'un objet = son résumé");
            TEST_CHECK(TSA::Automation::sameValue(a, ObjectRef(std::make_shared<TestVector>(std::vector<double> { 1.0, 2.0 })))
                           && !TSA::Automation::sameValue(a, ObjectRef(std::make_shared<TestVector>(std::vector<double> { 1.0, 3.0 }))),
                       "Test 391: égalité des objets par contenu");
            Value parsed;
            TEST_CHECK(!TSA::Automation::parseArgument("x", ValueType::Object, parsed), "Test 391: un objet n'a pas de forme texte");
            // Objet du mauvais type arrivé par une broche générique (variable) : refusé à l'exécution avec les deux types
            Graph w;
            const int st = w.addNode("event.start"), set = w.addNode("var.set"), get = w.addNode("var.get"), oth = w.addNode("test.other");
            const int s2 = w.addNode("test.sum"), pr = w.addNode("debug.print");
            w.setValue(set, "name", std::string("v"));
            w.setValue(get, "name", std::string("v"));
            TEST_CHECK(L.connect(w, { st, "then", set, "exec" }) && L.connect(w, { oth, "other", set, "value" }) && L.connect(w, { set, "then", pr, "exec" })
                           && L.connect(w, { get, "value", s2, "vector" }) && L.connect(w, { s2, "sum", pr, "value" }),
                       "Test 391: graphe à variable générique");
            const auto rep = Runner(L, w, nullptr).run();
            TEST_CHECK(!rep.ok && rep.message.find("TestVector attendu") != std::string::npos && rep.message.find("TestOther") != std::string::npos,
                       "Test 391: type d'objet contrôlé à l'exécution (" << rep.message << ")");
        }
        // 2-3. Réutilisation : un nœud lu par trois consommateurs n'est évalué qu'une fois ; d'une exécution à l'autre,
        // rien n'est réévalué ; une valeur modifiée ne recalcule que ce qui en dépend
        {
            Graph g;
            const int start = g.addNode("event.start"), make = g.addNode("test.make");
            g.setValue(make, "n", 4LL);
            g.setValue(make, "value", 2.5);
            int sums[3], prints[3];
            int prev = start;
            for (int k = 0; k < 3; ++k)
            {
                sums[k] = g.addNode("test.sum");
                prints[k] = g.addNode("debug.print");
                TEST_CHECK(L.connect(g, { make, "vector", sums[k], "vector" }) && L.connect(g, { sums[k], "sum", prints[k], "value" })
                               && L.connect(g, { prev, "then", prints[k], "exec" }),
                           "Test 391: graphe de réutilisation");
                prev = prints[k];
            }
            *t.makeCount = 0;
            auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && *t.makeCount == 1 && rep.log.size() == 3 && rep.log[0] == "10",
                       "Test 391: une seule évaluation pour trois lecteurs (" << *t.makeCount << " évaluation(s), " << rep.message << ")");
            TEST_CHECK(rep.values.count(make) && std::holds_alternative<ObjectRef>(rep.values.at(make).at("vector")),
                       "Test 391: valeurs intermédiaires rendues dans le rapport");
            EvaluationCache cache;
            Runner first(L, g, nullptr);
            first.setCache(&cache);
            TEST_CHECK(first.run().ok && cache.size() >= 4, "Test 391: cache rempli (" << cache.size() << " entrée(s))");
            const int makes = *t.makeCount, summed = *t.sumCount;
            Runner second(L, g, nullptr);
            second.setCache(&cache);
            rep = second.run();
            TEST_CHECK(rep.ok && *t.makeCount == makes && *t.sumCount == summed && rep.profile[make].reused >= 1 && rep.reused >= 4,
                       "Test 391: seconde exécution sans réévaluation (" << rep.reused << " réutilisation(s))");
            g.setValue(make, "value", 3.0);
            Runner third(L, g, nullptr);
            third.setCache(&cache);
            rep = third.run();
            TEST_CHECK(rep.ok && *t.makeCount == makes + 1 && *t.sumCount == summed + 3 && rep.log[0] == "12",
                       "Test 391: valeur modifiée → seuls les nœuds dépendants recalculés (" << rep.log[0] << ")");
            // 8. Copier-coller : nœuds choisis et lien interne, sans le lien sortant de la sélection
            const QByteArray frag = fragmentToJson(g, { make, sums[0] });
            Graph h = g;
            std::vector<int> created;
            TEST_CHECK(pasteFragment(h, frag, 40.0, 40.0, &created) && created.size() == 2 && h.nodes().size() == g.nodes().size() + 2,
                       "Test 391: fragment collé (2 nœuds)");
            const auto* copy = h.node(created[0]);
            TEST_CHECK(copy && copy->type == "test.make" && std::get<long long>(copy->values.at("n")) == 4 && copy->x == g.node(make)->x + 40.0,
                       "Test 391: valeurs et position décalée conservées");
            TEST_CHECK(h.links().size() == g.links().size() + 1 && h.linkTo(created[1], "vector").has_value()
                           && h.linkTo(created[1], "vector")->fromNode == created[0],
                       "Test 391: lien interne recréé entre les copies, liens externes ignorés");
            TEST_CHECK(!pasteFragment(h, QByteArray("{}"), 0, 0), "Test 391: presse-papiers sans nœud refusé");
        }
        // 4. Variables : somme 1..10 accumulée dans une boucle
        {
            Graph g;
            const int start = g.addNode("event.start"), init = g.addNode("var.set"), loop = g.addNode("flow.for");
            const int acc = g.addNode("var.set"), get = g.addNode("var.get"), add = g.addNode("math.add"), out = g.addNode("var.get"), print = g.addNode("debug.print");
            for (int n : { init, acc, get, out }) g.setValue(n, "name", std::string("s"));
            g.setValue(init, "value", 0.0);
            g.setValue(loop, "first", 1LL);
            g.setValue(loop, "last", 10LL);
            TEST_CHECK(L.connect(g, { start, "then", init, "exec" }) && L.connect(g, { init, "then", loop, "exec" }) && L.connect(g, { loop, "body", acc, "exec" })
                           && L.connect(g, { get, "value", add, "a" }) && L.connect(g, { loop, "index", add, "b" }) && L.connect(g, { add, "result", acc, "value" })
                           && L.connect(g, { loop, "completed", print, "exec" }) && L.connect(g, { out, "value", print, "value" }),
                       "Test 391: graphe d'accumulation");
            TEST_CHECK(L.warnings(g).empty(), "Test 391: boucle correctement ordonnée, aucun avertissement");
            const auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && !rep.log.empty() && rep.log.back() == "55" && rep.variables.count("s")
                           && std::get<double>(rep.variables.at("s")) == 55.0,
                       "Test 391: variable accumulée Σ 1..10 = 55 (" << rep.message << ")");
            Graph u;
            const int s0 = u.addNode("event.start"), p0 = u.addNode("debug.print"), g0 = u.addNode("var.get");
            u.setValue(g0, "name", std::string("absente"));
            TEST_CHECK(L.connect(u, { s0, "then", p0, "exec" }) && L.connect(u, { g0, "value", p0, "value" }), "Test 391: lecture d'une variable");
            const auto bad = Runner(L, u, nullptr).run();
            TEST_CHECK(!bad.ok && bad.message.find("non affectée") != std::string::npos, "Test 391: variable non affectée signalée");
        }
        // 5-6. Cycle d'exécution (bloquant) et dépendance mal ordonnée (avertissement)
        {
            Graph c;
            const int s = c.addNode("event.start"), a = c.addNode("debug.print"), b = c.addNode("debug.print");
            TEST_CHECK(L.connect(c, { s, "then", a, "exec" }) && L.connect(c, { a, "then", b, "exec" }) && L.connect(c, { b, "then", a, "exec" }),
                       "Test 391: liens d'exécution en boucle");
            bool cycle = false;
            for (const auto& i : L.validate(c)) cycle = cycle || i.message.find("cycle d'exécution") != std::string::npos;
            TEST_CHECK(cycle, "Test 391: cycle d'exécution refusé à la validation");
            Graph d;
            const int st = d.addNode("event.start"), print = d.addNode("debug.print"), set = d.addNode("var.set");
            d.setValue(set, "name", std::string("x"));
            d.setValue(set, "value", 1.0);
            TEST_CHECK(L.connect(d, { st, "then", print, "exec" }) && L.connect(d, { print, "then", set, "exec" }) && L.connect(d, { set, "value", print, "value" }),
                       "Test 391: lecture avant production");
            const auto warnings = L.warnings(d);
            TEST_CHECK(warnings.size() == 1 && warnings[0].node == print && L.validate(d).empty(), "Test 391: dépendance mal ordonnée signalée (non bloquante)");
            TEST_CHECK(!Runner(L, d, nullptr).run().ok, "Test 391: à l'exécution, sortie non disponible");
        }
        // 7. Fichier v2 : commentaires et versions ; version 1 lisible ; version plus récente que la bibliothèque signalée
        {
            Graph f;
            f.name = "Fichier v2";
            const int s = f.addNode("event.start", -100, 0), m = f.addNode("test.make", 50, 20);
            f.setValue(m, "value", 4.25);
            TEST_CHECK(L.connect(f, { s, "then", f.addNode("debug.print", 300, 0), "exec" }), "Test 391: graphe à enregistrer");
            Comment note;
            note.x = -150, note.y = -60, note.width = 520, note.height = 220;
            note.text = "Étape 1 — Modèle\nNœuds et éléments";
            note.color = "#00796B";
            const int cid = f.addComment(note);
            TEST_CHECK(f.nodesInComment(cid).size() == 3, "Test 391: nœuds contenus dans le commentaire");
            QTemporaryDir dir;
            const QString path = dir.filePath("v2.tsbp");
            TEST_CHECK(saveFile(f, path), "Test 391: enregistrement");
            Graph back;
            QString error;
            TEST_CHECK(loadFile(path, back, &error), "Test 391: relecture (" << error.toStdString() << ")");
            TEST_CHECK(back.comments().size() == 1 && back.comment(cid)->text == note.text && back.comment(cid)->color == "#00796B"
                           && back.comment(cid)->width == 520.0,
                       "Test 391: commentaire relu");
            TEST_CHECK(back.node(s)->version == 1 && back.node(m)->version == 1 && std::get<double>(back.node(m)->values.at("value")) == 4.25,
                       "Test 391: versions des nœuds enregistrées, valeurs relues");
            Graph again;
            TEST_CHECK(fromJson(toJson(back), again) && again.sameContent(back), "Test 391: relecture idempotente");
            back.node(m)->version = 9;
            bool newer = false;
            for (const auto& i : L.validate(back)) newer = newer || i.message.find("version plus récente") != std::string::npos;
            TEST_CHECK(newer, "Test 391: nœud d'une version plus récente signalé");
            Graph v1;
            TEST_CHECK(fromJson("{\"format\":\"tsbp\",\"version\":1,\"name\":\"ancien\",\"nodes\":[{\"id\":1,\"type\":\"event.start\",\"x\":0,\"y\":0}],"
                                "\"links\":[]}",
                                v1)
                           && v1.nodes().size() == 1 && v1.node(1)->version == 1 && v1.comments().empty(),
                       "Test 391: fichier de version 1 lu (nœuds en version 1, sans commentaire)");
        }
        // 9. Contenu complet d'un objet dans le journal
        {
            Graph g;
            const int s = g.addNode("event.start"), make = g.addNode("test.make"), show = g.addNode("debug.print_details");
            g.setValue(make, "n", 2LL);
            g.setValue(show, "title", std::string("Vecteur"));
            TEST_CHECK(L.connect(g, { s, "then", show, "exec" }) && L.connect(g, { make, "vector", show, "value" }), "Test 391: affichage du contenu");
            const auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0].find("Vecteur\ncomposantes : 1.0") == 0, "Test 391: contenu complet de l'objet écrit");
        }
        std::cout << "[PASS] Test 391: objets typés, réutilisation des résultats, variables, cycles et dépendances, fichier v2, copier-coller"
                  << std::endl;
        ++passed;
    }
    // TEST 392 : sous-graphes réutilisables — définition, instances pures et d'action, entrées et sorties typées, valeurs
    // par défaut, entrées obligatoires, erreurs du corps, récursion, fin non atteinte, définition absente, version plus
    // récente, fichier v3 et .tsbps, copier-coller entre graphes, encapsulation d'une sélection, mise à jour des instances,
    // réutilisation des résultats
    {
        TestLibrary t;
        const NodeLibrary& L = t.lib;
        std::string why;
        auto realPin = [](const char* name, Value def = {}, bool required = false) {
            PinSpec p = testPin(name, ValueType::Real, std::move(def));
            p.required = required;
            return p;
        };
        // Sous-graphe pur « double » : y = 2·x (x par défaut 1)
        auto makeDouble = [&]() {
            SubgraphDefinition d;
            d.id = "double";
            d.name = "Double";
            d.inputs = { realPin("x", 1.0) };
            d.outputs = { realPin("y") };
            auto body = std::make_shared<Graph>(*makeSubgraphBody("double"));
            int in = 0, out = 0;
            for (const auto& [id, n] : body->nodes()) (n.type == "sub.double.in" ? in : out) = id;
            const int mul = body->addNode("math.multiply");
            body->setValue(mul, "b", 2.0);
            body->addLink({ in, "x", mul, "a" });
            body->addLink({ mul, "result", out, "y" });
            d.body = body;
            return d;
        };
        // 1. Instance pure, composition de deux instances, valeur par défaut
        {
            Graph g;
            g.subgraphs.push_back(makeDouble());
            const int s = g.addNode("event.start"), a = g.addNode("sub.double"), b = g.addNode("sub.double"), c = g.addNode("sub.double");
            const int p1 = g.addNode("debug.print"), p2 = g.addNode("debug.print");
            g.setValue(a, "x", 3.0);
            TEST_CHECK(L.connect(g, { a, "y", b, "x" }, &why) && L.connect(g, { b, "y", p1, "value" }, &why) && L.connect(g, { c, "y", p2, "value" }, &why)
                           && L.connect(g, { s, "then", p1, "exec" }, &why) && L.connect(g, { p1, "then", p2, "exec" }, &why),
                       "Test 392: instances reliées (" << why << ")");
            TEST_CHECK(L.validate(g).empty(), "Test 392: graphe à sous-graphes valide");
            const auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 2 && rep.log[0] == "12" && rep.log[1] == "2",
                       "Test 392: 2·(2·3) = 12, valeur par défaut 2·1 = 2 (" << rep.message << ")");
            std::unique_ptr<NodeLibrary> holder;
            const NodeDefinition* d = L.forGraph(g, holder).find("sub.double");
            TEST_CHECK(d && d->pure && d->cacheable && d->version == 1 && d->input("x") && d->output("y") && !d->input("exec"),
                       "Test 392: instance pure et réutilisable, broches typées");
        }
        // 2. Entrée et sortie objet typées : somme d'un TestVector ; type d'objet contrôlé à la connexion
        SubgraphDefinition vsum;
        {
            vsum.id = "somme";
            vsum.name = "Somme d'un vecteur";
            vsum.inputs = { testPin("v", ValueType::Object, {}, "TestVector") };
            vsum.inputs[0].required = true;
            vsum.outputs = { realPin("s") };
            auto body = std::make_shared<Graph>(*makeSubgraphBody("somme"));
            int in = 0, out = 0;
            for (const auto& [id, n] : body->nodes()) (n.type == "sub.somme.in" ? in : out) = id;
            const int sum = body->addNode("test.sum");
            body->addLink({ in, "v", sum, "vector" });
            body->addLink({ sum, "sum", out, "s" });
            vsum.body = body;
            Graph g;
            g.subgraphs.push_back(vsum);
            const int s = g.addNode("event.start"), make = g.addNode("test.make"), inst = g.addNode("sub.somme"), p = g.addNode("debug.print");
            const int other = g.addNode("test.other");
            g.setValue(make, "n", 4LL);
            g.setValue(make, "value", 2.5);
            TEST_CHECK(!L.canConnect(g, { other, "other", inst, "v" }, &why) && why.find("TestVector") != std::string::npos, "Test 392: objet d'un autre type refusé");
            TEST_CHECK(L.connect(g, { make, "vector", inst, "v" }) && L.connect(g, { inst, "s", p, "value" }) && L.connect(g, { s, "then", p, "exec" }),
                       "Test 392: objet relié à l'instance");
            auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0] == "10", "Test 392: somme du vecteur par le sous-graphe (" << rep.message << ")");
            // Réutilisation : instance pure réutilisable d'une exécution à l'autre, corps non réévalué
            EvaluationCache cache;
            Runner r1(L, g, nullptr);
            r1.setCache(&cache);
            TEST_CHECK(r1.run().ok, "Test 392: exécution avec cache");
            const int sums = *t.sumCount;
            Runner r2(L, g, nullptr);
            r2.setCache(&cache);
            rep = r2.run();
            TEST_CHECK(rep.ok && *t.sumCount == sums && rep.profile[inst].reused >= 1, "Test 392: instance réutilisée sans réexécuter le corps");
            // Définition modifiée (même type « sub.somme », mêmes entrées) : résultat recalculé, jamais celui de l'ancienne
            SubgraphDefinition doubled = vsum;
            auto db = std::make_shared<Graph>(*vsum.body);
            int dsum = 0, dout = 0;
            for (const auto& [id, n] : db->nodes())
            {
                if (n.type == "test.sum") dsum = id;
                if (n.type == "sub.somme.out") dout = id;
            }
            db->removeLink({ dsum, "sum", dout, "s" });
            const int twice = db->addNode("math.multiply");
            db->setValue(twice, "b", 2.0);
            db->addLink({ dsum, "sum", twice, "a" });
            db->addLink({ twice, "result", dout, "s" });
            doubled.body = db;
            TEST_CHECK(defineSubgraph(L, g, doubled, nullptr, &why) && g.subgraph("somme")->version == 2, "Test 392: définition modifiée (" << why << ")");
            Runner r3(L, g, nullptr);
            r3.setCache(&cache);
            rep = r3.run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0] == "20" && rep.profile[inst].reused == 0,
                       "Test 392: définition modifiée — résultat recalculé malgré le cache (" << (rep.log.empty() ? rep.message : rep.log[0]) << ")");
            // Entrée obligatoire non reliée
            g.removeLink({ make, "vector", inst, "v" });
            bool required = false;
            for (const auto& i : L.validate(g)) required = required || (i.node == inst && i.message.find("entrée requise") != std::string::npos);
            TEST_CHECK(required, "Test 392: entrée obligatoire non renseignée signalée");
        }
        // 3. Sous-graphe d'action : journal du corps repris, broches d'exécution
        SubgraphDefinition act;
        {
            act.id = "annonce";
            act.name = "Annonce";
            act.inputs = { testPin("texte", ValueType::Text, std::string("bonjour")) };
            auto body = std::make_shared<Graph>();
            const int in = body->addNode("sub.annonce.in"), pr = body->addNode("debug.print"), out = body->addNode("sub.annonce.out");
            body->addLink({ in, "then", pr, "exec" });
            body->addLink({ pr, "then", out, "exec" });
            body->addLink({ in, "texte", pr, "value" });
            act.body = body;
            Graph g;
            g.subgraphs.push_back(act);
            const int s = g.addNode("event.start"), inst = g.addNode("sub.annonce"), p = g.addNode("debug.print");
            g.setValue(inst, "texte", std::string("dans le corps"));
            g.setValue(p, "value", std::string("après"));
            TEST_CHECK(L.connect(g, { s, "then", inst, "exec" }, &why) && L.connect(g, { inst, "then", p, "exec" }, &why), "Test 392: instance d'action reliée (" << why << ")");
            const auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 2 && rep.log[0] == "[Annonce] dans le corps" && rep.log[1] == "après",
                       "Test 392: corps d'action exécuté, journal préfixé, suite exécutée (" << rep.message << ")");
            // Corps exécuté seul (essai) : valeurs par défaut des entrées
            const auto alone = Runner(L, [&] { Graph b = *act.body; b.subgraphs = { act }; return b; }(), nullptr).run();
            TEST_CHECK(alone.ok && alone.log.size() == 1 && alone.log[0] == "bonjour", "Test 392: corps exécuté seul avec les valeurs par défaut (" << alone.message << ")");
        }
        // 4. Erreur du corps propagée avec son message
        {
            SubgraphDefinition bad = makeDouble();
            bad.id = "divise";
            auto body = std::make_shared<Graph>();
            const int in = body->addNode("sub.divise.in"), div = body->addNode("math.divide"), out = body->addNode("sub.divise.out");
            body->setValue(div, "a", 1.0);
            body->setValue(div, "b", 0.0);
            body->addLink({ in, "then", out, "exec" });
            body->addLink({ div, "result", out, "y" });
            bad.body = body;
            Graph g;
            g.subgraphs.push_back(bad);
            const int s = g.addNode("event.start"), inst = g.addNode("sub.divise"), p = g.addNode("debug.print");
            TEST_CHECK(L.connect(g, { inst, "y", p, "value" }) && L.connect(g, { s, "then", p, "exec" }), "Test 392: graphe à erreur");
            const auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(!rep.ok && rep.message.find("sous-graphe « Double »") != std::string::npos && rep.message.find("division par zéro") != std::string::npos,
                       "Test 392: erreur du corps propagée (" << rep.message << ")");
        }
        // 5. Contrôles : récursion, fin non atteinte, « Début » dans un corps, définition absente, version plus récente
        {
            auto issuesOf = [&](const Graph& g) {
                std::string all;
                for (const auto& i : L.validate(g)) all += i.message + "\n";
                return all;
            };
            SubgraphDefinition rec = makeDouble();
            rec.id = "boucle";
            auto body = std::make_shared<Graph>();
            const int in = body->addNode("sub.boucle.in"), self = body->addNode("sub.boucle"), out = body->addNode("sub.boucle.out");
            body->addLink({ in, "then", out, "exec" });
            body->addLink({ in, "x", self, "x" });
            body->addLink({ self, "y", out, "y" });
            rec.body = body;
            Graph g;
            g.subgraphs.push_back(rec);
            TEST_CHECK(issuesOf(g).find("appel récursif de sous-graphes : boucle → boucle") != std::string::npos, "Test 392: récursion refusée (" << issuesOf(g) << ")");
            SubgraphDefinition cut = makeDouble();
            auto cutBody = std::make_shared<Graph>(*cut.body);
            for (const auto& l : std::vector<Link>(cutBody->links()))
                if (l.fromPin == "then") cutBody->removeLink(l);
            cut.body = cutBody;
            Graph h;
            h.subgraphs.push_back(cut);
            TEST_CHECK(issuesOf(h).find("fin du corps n'est pas atteinte") != std::string::npos, "Test 392: fin non atteinte signalée");
            SubgraphDefinition start = makeDouble();
            auto startBody = std::make_shared<Graph>(*start.body);
            startBody->addNode("event.start");
            start.body = startBody;
            Graph k;
            k.subgraphs.push_back(start);
            TEST_CHECK(issuesOf(k).find("« Début » interdit") != std::string::npos, "Test 392: « Début » dans un corps refusé");
            Graph missing;
            missing.addNode("sub.absent");
            TEST_CHECK(issuesOf(missing).find("sous-graphe « absent » introuvable") != std::string::npos, "Test 392: définition absente signalée");
            Graph newer;
            newer.subgraphs.push_back(makeDouble());
            const int n = newer.addNode("sub.double");
            newer.node(n)->version = 4;
            TEST_CHECK(issuesOf(newer).find("version plus récente") != std::string::npos, "Test 392: instance d'une version plus récente signalée");
        }
        // 6. Fichiers : v2 sans sous-graphe, v3 avec, relecture à l'identique ; .tsbps avec dépendance
        {
            Graph plain;
            plain.addNode("event.start");
            TEST_CHECK(QJsonDocument::fromJson(toJson(plain)).object()["version"].toInt() == 2, "Test 392: graphe sans sous-graphe enregistré en version 2");
            Graph g;
            g.name = "avec sous-graphes";
            g.subgraphs.push_back(makeDouble());
            // « quadruple » appelle « double » deux fois
            SubgraphDefinition quad;
            quad.id = "quadruple";
            quad.name = "Quadruple";
            quad.inputs = { realPin("x", 1.0) };
            quad.outputs = { realPin("y") };
            auto qb = std::make_shared<Graph>();
            const int in = qb->addNode("sub.quadruple.in"), d1 = qb->addNode("sub.double"), d2 = qb->addNode("sub.double"), out = qb->addNode("sub.quadruple.out");
            qb->addLink({ in, "then", out, "exec" });
            qb->addLink({ in, "x", d1, "x" });
            qb->addLink({ d1, "y", d2, "x" });
            qb->addLink({ d2, "y", out, "y" });
            quad.body = qb;
            g.subgraphs.push_back(quad);
            const int s = g.addNode("event.start"), q = g.addNode("sub.quadruple"), p = g.addNode("debug.print");
            g.setValue(q, "x", 2.5);
            TEST_CHECK(L.connect(g, { q, "y", p, "value" }) && L.connect(g, { s, "then", p, "exec" }), "Test 392: sous-graphes imbriqués");
            auto rep = Runner(L, g, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0] == "10", "Test 392: sous-graphe appelant un sous-graphe (" << rep.message << ")");
            const QJsonObject root = QJsonDocument::fromJson(toJson(g)).object();
            TEST_CHECK(root["version"].toInt() == 3 && root["subgraphs"].toArray().size() == 2, "Test 392: fichier v3 avec les définitions");
            // L'enregistrement fixe la version de chaque nœud (0 = courante en mémoire) : relecture idempotente, comme au test 391
            Graph back, again;
            QString error;
            TEST_CHECK(fromJson(toJson(g), back, &error) && fromJson(toJson(back), again, &error) && again.sameContent(back)
                           && back.subgraphs.size() == 2 && back.node(q)->version == 1 && back.subgraph("quadruple")->inputs.size() == 1,
                       "Test 392: relecture à l'identique (" << error.toStdString() << ")");
            rep = Runner(L, back, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0] == "10", "Test 392: graphe relu exécuté");
            QTemporaryDir dir;
            const QString file = dir.filePath("quadruple.tsbps");
            TEST_CHECK(saveSubgraphFile(g, "quadruple", file, &error), "Test 392: sous-graphe exporté (" << error.toStdString() << ")");
            std::vector<SubgraphDefinition> defs;
            TEST_CHECK(loadSubgraphFile(file, defs, &error) && defs.size() == 2 && defs[0].id == "double" && defs[1].id == "quadruple",
                       "Test 392: fichier .tsbps relu, dépendance d'abord (" << error.toStdString() << ")");
            Graph reuse;
            std::vector<std::string> updates;
            for (auto& d : defs) TEST_CHECK(defineSubgraph(L, reuse, d, &updates, &why), "Test 392: définition importée (" << why << ")");
            const int s2 = reuse.addNode("event.start"), q2 = reuse.addNode("sub.quadruple"), p2 = reuse.addNode("debug.print");
            TEST_CHECK(L.connect(reuse, { q2, "y", p2, "value" }) && L.connect(reuse, { s2, "then", p2, "exec" }), "Test 392: réutilisation dans un autre graphe");
            rep = Runner(L, reuse, nullptr).run();
            TEST_CHECK(rep.ok && rep.log.size() == 1 && rep.log[0] == "4", "Test 392: sous-graphe réutilisé dans un autre graphe (valeur par défaut)");
            TEST_CHECK(fromJson("{\"format\":\"tsbp\",\"version\":4,\"nodes\":[],\"links\":[]}", back, &error) == false && error.contains("version"),
                       "Test 392: fichier d'une version future refusé");
            // Copier-coller : le fragment emporte les définitions utilisées
            const QByteArray frag = fragmentToJson(g, { q });
            Graph target;
            std::vector<int> created;
            std::vector<std::string> notes;
            TEST_CHECK(pasteFragment(target, frag, 0, 0, &created, nullptr, &error, &notes) && created.size() == 1 && target.subgraphs.size() == 2
                           && notes.size() == 2,
                       "Test 392: copier-coller avec les deux définitions (" << notes.size() << " note(s))");
        }
        // 7. Encapsulation d'une sélection (pure, puis d'action) : même résultat qu'avant
        {
            Graph g;
            const int s = g.addNode("event.start"), make = g.addNode("test.make"), sum = g.addNode("test.sum"), mul = g.addNode("math.multiply");
            const int p = g.addNode("debug.print");
            g.setValue(make, "n", 4LL);
            g.setValue(make, "value", 2.5);
            g.setValue(mul, "b", 3.0);
            TEST_CHECK(L.connect(g, { make, "vector", sum, "vector" }) && L.connect(g, { sum, "sum", mul, "a" }) && L.connect(g, { mul, "result", p, "value" })
                           && L.connect(g, { s, "then", p, "exec" }),
                       "Test 392: graphe à encapsuler");
            const auto before = Runner(L, g, nullptr).run();
            auto r = encapsulate(L, g, { sum, mul }, "somme_x3", "Somme × 3");
            TEST_CHECK(r.ok && g.subgraphs.size() == 1 && g.subgraph("somme_x3")->inputs.size() == 1 && g.subgraph("somme_x3")->outputs.size() == 1
                           && !g.node(sum) && g.node(r.instance),
                       "Test 392: sélection pure encapsulée (" << r.message << ")");
            TEST_CHECK(L.validate(g).empty(), "Test 392: graphe encapsulé valide");
            const auto after = Runner(L, g, nullptr).run();
            TEST_CHECK(before.ok && after.ok && before.log == after.log && after.log[0] == "30", "Test 392: même résultat après encapsulation (" << after.message << ")");
            // Sélection d'action (impression) : point d'entrée et suite d'exécution conservés
            const int p2 = g.addNode("debug.print");
            g.setValue(p2, "value", std::string("fin"));
            TEST_CHECK(L.connect(g, { p, "then", p2, "exec" }), "Test 392: suite d'exécution");
            r = encapsulate(L, g, { p }, "affiche", "Affiche");
            TEST_CHECK(r.ok, "Test 392: sélection d'action encapsulée (" << r.message << ")");
            std::unique_ptr<NodeLibrary> holder;
            TEST_CHECK(!L.forGraph(g, holder).find("sub.affiche")->pure, "Test 392: instance d'action");
            const auto again = Runner(L, g, nullptr).run();
            TEST_CHECK(again.ok && again.log.size() == 2 && again.log[0] == "[Affiche] 30" && again.log[1] == "fin", "Test 392: flux conservé (" << again.message << ")");
            TEST_CHECK(!encapsulate(L, g, { s }, "debut", "x").ok, "Test 392: « Début » non encapsulable");
            Graph two;
            const int st = two.addNode("event.start"), br = two.addNode("flow.branch"), a1 = two.addNode("debug.print"), a2 = two.addNode("debug.print");
            TEST_CHECK(L.connect(two, { st, "then", br, "exec" }) && L.connect(two, { br, "true", a1, "exec" }) && L.connect(two, { br, "false", a2, "exec" }),
                       "Test 392: graphe à deux entrées");
            TEST_CHECK(!encapsulate(L, two, { a1, a2 }, "deux", "x").ok, "Test 392: deux points d'entrée refusés");
            // Mise à jour des instances : sortie supprimée → lien retiré, version incrémentée
            SubgraphDefinition changed = *g.subgraph("somme_x3");
            changed.outputs.clear();
            auto body = std::make_shared<Graph>(*changed.body);
            for (const auto& l : std::vector<Link>(body->links()))
                if (body->node(l.toNode)->type == "sub.somme_x3.out" && l.toPin != "exec") body->removeLink(l);
            changed.body = body;
            std::vector<std::string> updates;
            TEST_CHECK(defineSubgraph(L, g, changed, &updates, &why) && g.subgraph("somme_x3")->version == 2 && !updates.empty(),
                       "Test 392: définition modifiée, version 2, instance mise à jour (" << why << ", " << updates.size() << " mise(s) à jour)");
            TEST_CHECK(subgraphInstances(g, "somme_x3").size() == 1 && g.linkTo(subgraphInstances(g, "somme_x3")[0], "vector").has_value(),
                       "Test 392: lien d'entrée encore valide conservé");
            TEST_CHECK(defineSubgraph(L, g, *g.subgraph("somme_x3"), nullptr, &why) && g.subgraph("somme_x3")->version == 2, "Test 392: définition inchangée : version conservée");
            std::string removal;
            TEST_CHECK(!removeSubgraph(g, "somme_x3", &removal) && removal.find("encore utilisé") != std::string::npos, "Test 392: définition utilisée non supprimable");
        }
        std::cout << "[PASS] Test 392: sous-graphes — instances pures et d'action, objets typés, défauts, erreurs, récursion, v3 et .tsbps, "
                     "copier-coller, encapsulation, mise à jour, réutilisation"
                  << std::endl;
        ++passed;
    }
    // TEST 393 : sous-graphes dans l'éditeur — encapsulation de la sélection (identifiant tiré du nom), bibliothèque
    // effective et palette, corps ouvert (essai seul), retour : nouvelle version et instances mises à jour, annuler,
    // corps invalide refusé puis abandonné, interface (entrée ajoutée, sortie supprimée), encapsulation imbriquée reportée
    // au retour, graphe composé depuis un corps ouvert, export / import .tsbps, contrôles d'un corps (validateBody),
    // suppression d'une définition inutilisée
    {
        TSA::Project::ProjectSession session;
        auto editor = std::make_unique<TSA::UI::BlueprintEditor>();
        editor->setSession(&session);
        const NodeLibrary& S = NodeLibrary::standard();
        std::string why;
        QString error;
        auto logOf = [&]() { return editor->lastReport().log; };
        auto nodeOfType = [&](const Graph& g, const std::string& type) {
            for (const auto& [id, n] : g.nodes())
                if (n.type == type) return id;
            return 0;
        };
        // Début → Afficher((a + b) × 3), a = 2, b = 5
        Graph g;
        const int s = g.addNode("event.start"), add = g.addNode("math.add"), mul = g.addNode("math.multiply"), p = g.addNode("debug.print");
        g.setValue(add, "a", 2.0);
        g.setValue(add, "b", 5.0);
        g.setValue(mul, "b", 3.0);
        TEST_CHECK(S.connect(g, { add, "result", mul, "a" }) && S.connect(g, { mul, "result", p, "value" }) && S.connect(g, { s, "then", p, "exec" }),
                   "Test 393: graphe initial");
        editor->setGraph(g);
        TEST_CHECK(editor->run() && logOf() == std::vector<std::string> { "21" }, "Test 393: 3·(2 + 5) = 21 avant encapsulation");

        // 1. Encapsulation de la sélection
        editor->selectNodes({ add, mul });
        TEST_CHECK(editor->encapsulateSelection(QStringLiteral("Somme × 3"), &error), "Test 393: sélection encapsulée (" << error.toStdString() << ")");
        TEST_CHECK(editor->graph().subgraphs.size() == 1 && editor->graph().subgraphs[0].id == "somme_3" && !editor->graph().node(add),
                   "Test 393: identifiant tiré du nom (« somme_3 »)");
        const int inst = nodeOfType(editor->graph(), "sub.somme_3");
        TEST_CHECK(inst && editor->graph().linkTo(p, "value") && editor->graph().linkTo(p, "value")->fromNode == inst, "Test 393: instance reliée à l'affichage");
        TEST_CHECK(editor->run() && logOf() == std::vector<std::string> { "21" }, "Test 393: même résultat après encapsulation");
        const NodeDefinition* instDef = editor->library().find("sub.somme_3");
        TEST_CHECK(instDef && instDef->pure && instDef->output("result") && !editor->library().find("sub.somme_3")->input("exec"),
                   "Test 393: instance pure dans la bibliothèque effective de l'éditeur");
        QTreeWidget* palette = nullptr;
        for (auto* tree : editor->findChildren<QTreeWidget*>())
            if (tree->isHeaderHidden()) palette = tree;
        auto paletteHas = [&](const std::string& type) {
            for (int c = 0; palette && c < palette->topLevelItemCount(); ++c)
                for (int i = 0; i < palette->topLevelItem(c)->childCount(); ++i)
                    if (palette->topLevelItem(c)->child(i)->data(0, Qt::UserRole + 10).toString().toStdString() == type) return true;
            return false;
        };
        TEST_CHECK(paletteHas("sub.somme_3") && !paletteHas("sub.somme_3.in") && !paletteHas("sub.somme_3.out"),
                   "Test 393: palette — instance proposée, entrée et sortie du corps non proposées hors du corps");

        // 2. Corps ouvert : essai seul, modification (× 4), retour → version 2, instance mise à jour, annuler
        TEST_CHECK(editor->openSubgraph("somme_3", &error) && editor->subgraphDepth() == 1 && editor->currentSubgraph() == "somme_3",
                   "Test 393: corps ouvert (" << error.toStdString() << ")");
        TEST_CHECK(paletteHas("sub.somme_3.out") && !paletteHas("sub.somme_3"), "Test 393: palette du corps (sortie proposée, pas d'appel récursif)");
        TEST_CHECK(!editor->canUndo(), "Test 393: historique propre au corps");
        const int bodyMul = nodeOfType(editor->graph(), "math.multiply");
        const int bodyOut = nodeOfType(editor->graph(), "sub.somme_3.out");
        editor->graph().setValue(bodyMul, "b", 4.0);
        TEST_CHECK(editor->run() && editor->lastReport().values.count(bodyOut) && editor->lastReport().values.at(bodyOut).count("result")
                       && std::get<double>(editor->lastReport().values.at(bodyOut).at("result")) == 28.0,
                   "Test 393: corps exécuté seul, sortie 4·7 = 28 (" << editor->lastReport().message << ")");
        TEST_CHECK(editor->closeSubgraph(true, &error) && editor->subgraphDepth() == 0 && editor->graph().subgraph("somme_3")->version == 2,
                   "Test 393: retour, version 2 (" << error.toStdString() << ")");
        TEST_CHECK(editor->run() && logOf() == std::vector<std::string> { "28" }, "Test 393: instance mise à jour (28)");
        TEST_CHECK(editor->canUndo(), "Test 393: une entrée Annuler pour la modification du corps");
        editor->undo();
        TEST_CHECK(editor->graph().subgraph("somme_3")->version == 1 && editor->run() && logOf() == std::vector<std::string> { "21" },
                   "Test 393: annuler restaure la version 1");
        editor->redo();
        TEST_CHECK(editor->graph().subgraph("somme_3")->version == 2, "Test 393: rétablir");

        // 3. Corps invalide (sortie non reliée) : retour refusé, édition poursuivie ; abandon → définition inchangée
        TEST_CHECK(editor->openSubgraph("somme_3", &error), "Test 393: corps rouvert");
        for (const auto& l : std::vector<Link>(editor->graph().links()))
            if (l.toNode == nodeOfType(editor->graph(), "sub.somme_3.out") && l.toPin == "result") editor->graph().removeLink(l);
        TEST_CHECK(!editor->closeSubgraph(true, &error) && error.contains(QStringLiteral("requise")) && editor->subgraphDepth() == 1,
                   "Test 393: corps invalide refusé, édition poursuivie (" << error.toStdString() << ")");
        const auto bodyIssues = validateBody(S, editor->graph(), "somme_3", editor->graph().subgraphs);
        bool onOut = false;
        for (const auto& i : bodyIssues) onOut = onOut || (i.node == nodeOfType(editor->graph(), "sub.somme_3.out") && i.message.find("requise") != std::string::npos);
        TEST_CHECK(onOut, "Test 393: problème du corps rattaché au nœud de sortie");
        TEST_CHECK(editor->closeSubgraph(false, &error) && editor->subgraphDepth() == 0 && editor->graph().subgraph("somme_3")->version == 2
                       && editor->run() && logOf() == std::vector<std::string> { "28" },
                   "Test 393: modifications abandonnées");

        // 4. Interface : entrée « k » (défaut 3) reliée au facteur ; puis sortie supprimée (lien de l'instance retiré)
        TEST_CHECK(editor->openSubgraph("somme_3", &error), "Test 393: corps ouvert pour l'interface");
        SubgraphDefinition iface = *editor->graph().subgraph("somme_3");
        PinSpec k = testPin("k", ValueType::Real, 3.0);
        k.label = "Facteur";
        iface.inputs.push_back(k);
        TEST_CHECK(editor->setSubgraphInterface(iface, &error), "Test 393: entrée ajoutée (" << error.toStdString() << ")");
        const int bodyIn = nodeOfType(editor->graph(), "sub.somme_3.in");
        TEST_CHECK(editor->library().find("sub.somme_3.in")->output("k") && editor->library().connect(editor->graph(), { bodyIn, "k", bodyMul, "b" }, &why),
                   "Test 393: entrée « k » reliée dans le corps (" << why << ")");
        TEST_CHECK(editor->closeSubgraph(true, &error) && editor->graph().subgraph("somme_3")->version == 3 && editor->library().find("sub.somme_3")->input("k"),
                   "Test 393: version 3, instance avec l'entrée « k » (" << error.toStdString() << ")");
        TEST_CHECK(editor->run() && logOf() == std::vector<std::string> { "21" }, "Test 393: facteur par défaut (3)");
        editor->graph().setValue(inst, "k", 10.0);
        TEST_CHECK(editor->run() && logOf() == std::vector<std::string> { "70" }, "Test 393: facteur saisi sur l'instance (10)");
        TEST_CHECK(editor->openSubgraph("somme_3", &error), "Test 393: corps ouvert (sortie)");
        iface = *editor->graph().subgraph("somme_3");
        iface.outputs.clear();
        TEST_CHECK(editor->setSubgraphInterface(iface, &error) && !editor->graph().linkTo(nodeOfType(editor->graph(), "sub.somme_3.out"), "result"),
                   "Test 393: sortie supprimée, lien du corps retiré");
        TEST_CHECK(editor->closeSubgraph(true, &error) && !editor->graph().linkTo(p, "value") && editor->graph().subgraph("somme_3")->version == 4,
                   "Test 393: lien de l'instance vers la sortie supprimée retiré (" << error.toStdString() << ")");
        editor->undo();
        TEST_CHECK(editor->graph().subgraph("somme_3")->version == 3 && editor->graph().linkTo(p, "value"), "Test 393: annuler restaure la sortie et son lien");

        // 5. Encapsulation imbriquée dans un corps : reportée au retour (deux définitions, même résultat)
        TEST_CHECK(editor->openSubgraph("somme_3", &error), "Test 393: corps ouvert (imbrication)");
        editor->selectNodes({ nodeOfType(editor->graph(), "math.add") });
        TEST_CHECK(editor->encapsulateSelection(QStringLiteral("Addition"), &error) && editor->graph().subgraphs.size() == 2,
                   "Test 393: sous-graphe créé dans un corps (" << error.toStdString() << ")");
        TEST_CHECK(editor->openSubgraph("addition", &error) && editor->subgraphDepth() == 2, "Test 393: deuxième niveau ouvert");
        TEST_CHECK(!editor->openSubgraph("somme_3", &error), "Test 393: corps déjà ouvert refusé (appel récursif)");
        // Graphe composé depuis le deuxième niveau : les deux corps reportés, enregistrable et relu
        Graph composed;
        TEST_CHECK(editor->composedGraph(composed, &error) && composed.subgraphs.size() == 2 && composed.subgraph("addition")
                       && composed.subgraph("somme_3")->version == 4,
                   "Test 393: graphe composé depuis un corps ouvert (" << error.toStdString() << ")");
        QTemporaryDir dir;
        Graph reread;
        TEST_CHECK(saveFile(composed, dir.filePath("compose.tsbp"), &error) && loadFile(dir.filePath("compose.tsbp"), reread, &error)
                       && Runner(S, reread, nullptr).run().log == std::vector<std::string> { "70" },
                   "Test 393: graphe composé enregistré, relu et exécuté (70)");
        TEST_CHECK(editor->closeSubgraph(true, &error) && editor->closeSubgraph(true, &error) && editor->subgraphDepth() == 0,
                   "Test 393: retour des deux niveaux (" << error.toStdString() << ")");
        TEST_CHECK(editor->graph().subgraphs.size() == 2 && editor->graph().subgraph("somme_3")->version == 4 && editor->run()
                       && logOf() == std::vector<std::string> { "70" },
                   "Test 393: définitions reportées, même résultat (70)");

        // 6. Export depuis le graphe, import dans un nouveau graphe ; réimport identique ; suppression
        TEST_CHECK(editor->exportSubgraph("somme_3", dir.filePath("somme.tsbps"), &error), "Test 393: export .tsbps (" << error.toStdString() << ")");
        editor->newBlueprint();
        TEST_CHECK(editor->graph().subgraphs.empty() && editor->subgraphDepth() == 0 && !editor->library().find("sub.somme_3"),
                   "Test 393: nouveau graphe sans définition");
        TEST_CHECK(editor->importSubgraphs(dir.filePath("somme.tsbps"), &error) && editor->graph().subgraphs.size() == 2
                       && editor->graph().subgraph("somme_3")->version == 4 && editor->library().find("sub.somme_3") && paletteHas("sub.somme_3"),
                   "Test 393: import (dépendance comprise) (" << error.toStdString() << ")");
        TEST_CHECK(editor->importSubgraphs(dir.filePath("somme.tsbps"), &error) && editor->graph().subgraph("somme_3")->version == 4,
                   "Test 393: réimport identique sans nouvelle version");
        // Conflit : définition locale modifiée (entrée « extra », version 5), import du fichier (version 4) → remplacée par celle
        // du fichier en version 6 ; version antérieure et entrée supprimée signalées ; Annuler rétablit la définition locale
        QStringList warnings;
        QObject::connect(editor.get(), &TSA::UI::BlueprintEditor::logMessage, [&warnings](const QString& text, const QString& type) {
            if (type == QLatin1String("WARN")) warnings << text;
        });
        TEST_CHECK(editor->openSubgraph("somme_3", &error), "Test 393: corps ouvert (conflit)");
        SubgraphDefinition local = *editor->graph().subgraph("somme_3");
        local.inputs.push_back(testPin("extra", ValueType::Real, 1.0));
        TEST_CHECK(editor->setSubgraphInterface(local, &error) && editor->closeSubgraph(true, &error) && editor->graph().subgraph("somme_3")->version == 5,
                   "Test 393: définition locale modifiée, version 5 (" << error.toStdString() << ")");
        TEST_CHECK(editor->importSubgraphs(dir.filePath("somme.tsbps"), &error) && editor->graph().subgraph("somme_3")->version == 6
                       && editor->graph().subgraph("somme_3")->inputs.size() == 1 && editor->graph().subgraph("somme_3")->inputs[0].name == "k",
                   "Test 393: conflit — définition du fichier importée, version 6 (" << error.toStdString() << ")");
        const QString warned = warnings.join(QLatin1Char('\n'));
        TEST_CHECK(warned.contains(QStringLiteral("antérieure")) && warned.contains(QStringLiteral("« extra » supprimée")),
                   "Test 393: version antérieure et entrée supprimée signalées (" << warned.toStdString() << ")");
        editor->undo();
        TEST_CHECK(editor->graph().subgraph("somme_3")->version == 5 && editor->graph().subgraph("somme_3")->inputs.back().name == "extra",
                   "Test 393: Annuler rétablit la définition locale");
        TEST_CHECK(!editor->removeSubgraphDefinition("addition", &error) && error.contains(QStringLiteral("encore utilisé")),
                   "Test 393: définition utilisée par un autre corps non supprimable");
        TEST_CHECK(editor->removeSubgraphDefinition("somme_3", &error) && editor->removeSubgraphDefinition("addition", &error)
                       && editor->graph().subgraphs.empty() && !editor->library().find("sub.addition"),
                   "Test 393: définitions inutilisées supprimées (" << error.toStdString() << ")");

        // 7. validateBody : « Début » interdit, fin non atteinte (problèmes du graphe), sans doublon des problèmes de nœuds
        SubgraphDefinition d;
        d.id = "essai";
        d.outputs = { testPin("y", ValueType::Real) };
        d.body = makeSubgraphBody("essai");
        Graph body = *d.body;
        body.addNode("event.start");
        for (const auto& l : std::vector<Link>(body.links())) body.removeLink(l);
        int start = 0, unreached = 0, required = 0;
        for (const auto& i : validateBody(S, body, "essai", { d }))
        {
            start += i.message.find("« Début » interdit") != std::string::npos;
            unreached += i.message.find("fin du corps n'est pas atteinte") != std::string::npos;
            required += i.message.find("requise") != std::string::npos;
        }
        TEST_CHECK(start == 1 && unreached == 1 && required == 1, "Test 393: contrôles du corps sans doublon (" << start << ", " << unreached << ", " << required << ")");
        editor.reset();
        std::cout << "[PASS] Test 393: sous-graphes dans l'éditeur — encapsulation, corps ouvert, versions, annuler, interface, imbrication, "
                     "graphe composé, .tsbps, contrôles du corps"
                  << std::endl;
        ++passed;
    }
    return true;
}
