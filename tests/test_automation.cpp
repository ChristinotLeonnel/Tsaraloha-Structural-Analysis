// Suite « automation » : registre central des commandes exécutables (ADR-024, phase 5) — test 192.
// Une même commande sert l'interface, le Blueprint, l'IA et la console : création par ligne de
// commande, typage des arguments, une entrée Annuler par commande, échec sans effet sur le modèle.
#include "test_common.h"

#include "Automation/CommandRegistry.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"

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
    return true;
}
