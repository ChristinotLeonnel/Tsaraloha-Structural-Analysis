// Suite « cleanup » : nettoyage topologique du modèle (tests 160-165).

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "Model/Load/LoadManager.h"
#include "Model/ModelCleanup.h"
#include "UI/Tools/ModelCleanupDialog.h"
#include "UndoRedo/EditTransaction.h"

using namespace TSA::Model;

namespace
{
int bar(Model& m, int a, int b) { return m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam); }
} // namespace

bool runSuite_ModelCleanup(int& passed)
{
    // TEST 160 : nœuds confondus ; le bilan ne modifie pas le modèle
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        const int c = m.addNode(5.0004, 0, 0), d = m.addNode(10, 0, 0);   // 0,4 mm : confondu
        bar(m, a, b);
        bar(m, c, d);
        const auto preview = ModelCleanup::analyze(m);
        TEST_CHECK(preview.mergedNodes == 1 && m.nodes().size() == 4, "Test 160: bilan sans modification du modèle");
        const auto r = ModelCleanup::clean(m);
        TEST_CHECK(r.mergedNodes == 1 && m.nodes().size() == 3, "Test 160: nœud confondu fusionné");
        std::set<int> used;
        for (const auto& [id, bm] : m.beams()) { used.insert(bm.startNodeId()); used.insert(bm.endNodeId()); }
        TEST_CHECK(used.size() == 3, "Test 160: les deux poutres partagent le nœud");
        TEST_CHECK(!ModelCleanup::analyze(m).changed(), "Test 160: second passage sans effet (idempotent)");
        std::cout << "[PASS] Test 160: Fusion des nœuds confondus" << std::endl;
        ++passed;
    }

    // TEST 161 : nœuds parasites supprimés ; nœuds appuyés / chargés / de dalle conservés
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(4, 0, 0);
        bar(m, a, b);
        m.addNode(2, 5, 0);                                   // parasite
        const int sup = m.addNode(9, 9, 0);                    // appui sans élément
        m.getNode(sup)->setSupport(SupportDefinition::fixed());
        const int loaded = m.addNode(8, 8, 0);                 // charge sans élément
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q"));
        m.loadManager().addNodalLoad(NodalLoad(0, loaded, lc, 0, 0, -5));
        const int s1 = m.addNode(20, 0, 0), s2 = m.addNode(24, 0, 0), s3 = m.addNode(24, 4, 0);
        m.addSlab({ s1, s2, s3 });
        const auto r = ModelCleanup::clean(m);
        TEST_CHECK(r.orphanNodesRemoved == 1 && m.nodes().size() == 7, "Test 161: seul le nœud parasite est supprimé");
        TEST_CHECK(m.getNode(sup) && m.getNode(loaded) && r.warnings.size() == 2, "Test 161: appui / charge isolés conservés et signalés");
        std::cout << "[PASS] Test 161: Nœuds parasites" << std::endl;
        ++passed;
    }

    // TEST 162 : nœud posé sur une barre (jonction en T) raccordé ; division refusée si charge ponctuelle
    {
        Model m;
        const int a = m.addNode(0, 0, 3), b = m.addNode(6, 0, 3);
        const int beam = bar(m, a, b);
        const int foot = m.addNode(2, 0, 0), top = m.addNode(2, 0, 3);
        m.addColumn(foot, top, Section::heb(200), Material::steelS235());
        const auto r = ModelCleanup::clean(m);
        TEST_CHECK(r.nodesConnectedOnBars == 1 && m.beams().size() == 2, "Test 162: poutre divisée sur la tête de poteau");
        TEST_CHECK(m.getBeam(beam)->endNodeId() == top, "Test 162: tronçon relié au nœud existant");

        Model p;
        const int pa = p.addNode(0, 0, 3), pb = p.addNode(6, 0, 3);
        const int pbm = bar(p, pa, pb);
        const int pf = p.addNode(2, 0, 0), pt = p.addNode(2, 0, 3);
        p.addColumn(pf, pt, Section::heb(200), Material::steelS235());
        const int lc = p.loadManager().addLoadCase(LoadCase(0, "Q"));
        p.loadManager().addMemberLoad(MemberLoad::pointOnMember(pbm, lc, 10.0, 4.0));
        const auto rp = ModelCleanup::clean(p);
        TEST_CHECK(rp.nodesConnectedOnBars == 0 && rp.refused.size() == 1 && p.beams().size() == 1,
                   "Test 162: division refusée et signalée (charge ponctuelle)");
        std::cout << "[PASS] Test 162: Nœuds posés sur une barre" << std::endl;
        ++passed;
    }

    // TEST 163 : barres en double (charges reportées) ; mêmes nœuds, familles différentes signalées
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        const int b1 = bar(m, a, b), b2 = bar(m, b, a);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G"));
        const int load = m.loadManager().addMemberLoad(MemberLoad::uniform(b2, lc, 3.0));
        const int c = m.addNode(0, 0, 4);
        m.addColumn(a, c, Section::heb(200), Material::steelS235());
        m.addTrussMember(a, c, 0.05);
        const auto r = ModelCleanup::clean(m);
        TEST_CHECK(r.duplicateBarsRemoved == 1 && m.beams().size() == 1 && m.getBeam(b1), "Test 163: doublon supprimé, plus petit id conservé");
        TEST_CHECK(m.loadManager().getMemberLoad(load) && m.loadManager().getMemberLoad(load)->elementId() == b1,
                   "Test 163: charge du doublon reportée");
        TEST_CHECK(r.warnings.size() == 1 && m.trussMembers().size() == 1, "Test 163: poteau + treillis sur les mêmes nœuds : signalé, non supprimé");
        std::cout << "[PASS] Test 163: Barres en double" << std::endl;
        ++passed;
    }

    // TEST 164 : croisements (option), annulation en une étape, fenêtre
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(4, 0, 4), c = m.addNode(4, 0, 0), d = m.addNode(0, 0, 4);
        m.addTrussMember(a, b, 0.05);
        m.addTrussMember(c, d, 0.05);
        TEST_CHECK(!ModelCleanup::analyze(m).changed(), "Test 164: croisement ignoré par défaut (contreventement en X)");
        CleanupOptions o;
        o.splitCrossingBars = true;
        m.clearUndoRedo();
        {
            TSA::UndoRedo::EditTransaction tx(m, "Nettoyage");
            const auto r = ModelCleanup::clean(m, o);
            TEST_CHECK(r.crossingNodesCreated == 1 && m.trussMembers().size() == 4 && m.nodes().size() == 5, "Test 164: nœud au croisement");
            tx.commit();
        }
        TEST_CHECK(m.undo() && m.trussMembers().size() == 2 && m.nodes().size() == 4 && !m.canUndo(), "Test 164: une seule entrée Annuler");

        TSA::UI::ModelCleanupDialog dlg(m);
        TEST_CHECK(!dlg.refreshPreview().changed(), "Test 164: fenêtre — rien à faire par défaut");
        dlg.setOptions(o);
        TEST_CHECK(dlg.refreshPreview().crossingNodesCreated == 1 && m.nodes().size() == 4, "Test 164: fenêtre — bilan sans modification");
        std::cout << "[PASS] Test 164: Croisements, Annuler, fenêtre" << std::endl;
        ++passed;
    }

    // TEST 165 : erreur de calcul évitée — traverse non reliée aux poteaux (nœuds confondus)
    {
        using namespace TSA::Analysis;
        Model m;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0, 6 });
        def.setYPositions({ 0 });
        const std::string gid = gm.addGrid(def)->id();
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(6, 0, 3), d = m.addNode(6, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(d)->setSupport(SupportDefinition::fixed());
        m.addColumn(a, b, Section::heb(200), Material::steelS235());
        m.addColumn(d, c, Section::heb(200), Material::steelS235());
        const int b2 = m.addNode(0.0003, 0, 3), c2 = m.addNode(5.9998, 0, 3);   // traverse dessinée à part
        const int beam = bar(m, b2, c2);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G"));
        m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, 10.0));

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext ctx;
        ctx.engineId = "custom2d";
        ctx.dimension = AnalysisDimension::Plane2D;
        ctx.common.includeSelfWeight = false;
        ctx.scope.type = ScopeType::GridAxis;
        ctx.scope.gridId = gid;
        ctx.scope.axisLabel = "A";
        const auto before = mgr.run(ctx, mgr.prepare(m, &gm, ctx));
        TEST_CHECK(!before.success && before.message.find("instable") != std::string::npos,
                   "Test 165: avant nettoyage, traverse non reliée → structure instable");
        const auto r = ModelCleanup::clean(m);
        TEST_CHECK(r.mergedNodes == 2, "Test 165: deux nœuds confondus fusionnés");
        const auto after = mgr.run(ctx, mgr.prepare(m, &gm, ctx));
        TEST_CHECK(after.success && after.results.hasResults(), "Test 165: après nettoyage, calcul réussi");
        std::cout << "[PASS] Test 165: Erreur de calcul évitée par le nettoyage" << std::endl;
        ++passed;
    }
    return true;
}
