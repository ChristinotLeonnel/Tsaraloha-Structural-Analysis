// Suite « blueprint » : programmation visuelle (ADR-024, phases 6 et 8) — tests 193 à 195, 197 à 199, 391
// (valeurs objet, réutilisation des résultats, variables, contrôles du graphe, fichier v2, copier-coller).
#include "test_common.h"

#include "AI/Tools/AIToolRegistry.h"
#include "Automation/CommandRegistry.h"
#include "Blueprint/BlueprintFile.h"
#include "Blueprint/BlueprintScript.h"
#include "Blueprint/BlueprintRuntime.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"
#include "UI/Blueprint/BlueprintEditor.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

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
    return true;
}
