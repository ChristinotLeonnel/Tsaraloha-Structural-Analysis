// Suite « display » : affichage des résultats, des annotations et des charges dans la vue 3D réelle (OCCT).
//   371  résultats résiduels : portique dont les ids de poutre et de poteau coïncident ; diagrammes, déformée et
//        réactions affichés puis retirés (nouveau jeu de résultats, absence de résultats, modèle vidé = nouveau
//        projet) : aucun objet orphelin dans la scène, aucun cumul après plusieurs calculs.

#include "test_common.h"

#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "Model/Load/LoadManager.h"
#include "Viewer/OccView.h"
#include "Viewer/ResultsVisualManager.h"

#include <AIS_ListOfInteractive.hxx>
#include <QApplication>
#include <QThread>

using namespace TSA::Analysis;

namespace
{
int displayedObjects(OccView& v)
{
    AIS_ListOfInteractive list;
    v.context()->DisplayedObjects(list);
    return list.Extent();
}

void pump(int n = 10)
{
    for (int k = 0; k < n; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

/// Portique à deux poteaux (ids 1, 2) et une poutre (id 1) : la poutre 1 et le poteau 1 ont le même id.
std::shared_ptr<ResultsModel> portalResults(Model& m, double h = 3.0, double load = 20.0)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, h), c = m.addNode(6, 0, h), d = m.addNode(6, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, b, Section::heb(200), Material::steelS235());
    const int beam = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
    m.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, load));
    m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 10.0, 0.0, 0.0));
    AnalysisEngineRegistry reg;
    registerBuiltInEngines(reg);
    AnalysisManager mgr(reg);
    AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = false;
    ctx.loadCaseIds = { lc };
    GridManager gm;
    const auto prep = mgr.prepare(m, &gm, ctx);
    if (!prep.canRun()) return nullptr;
    auto run = mgr.run(ctx, prep);
    return run.success ? std::make_shared<ResultsModel>(run.results) : nullptr;
}
} // namespace

bool runSuite_Display(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 371 : aucun résultat résiduel dans la scène
    // -------------------------------------------------------------------------
    {
        Model m;
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        pump();
        const int empty = displayedObjects(view);
        auto results = portalResults(m);
        TEST_CHECK(results && results->isValid(), "Test 371: calcul du portique");
        view.rebuildAllShapes();
        pump();
        const int withModel = displayedObjects(view);
        auto* rv = view.resultsVisual();
        view.setResultsModel(results);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        rv->setDeformedVisible(true);
        rv->setReactionsVisible(true);
        pump();
        const auto d1 = rv->diagnostics();
        const int shown = displayedObjects(view);
        std::cout << "  scène : vide " << empty << ", modèle " << withModel << ", résultats " << shown << " (diagrammes " << d1.diagrams << ", déformées " << d1.deformed
                  << ", réactions " << d1.reactions << ", étiquettes " << d1.diagramLabels + d1.reactionLabels << ")" << std::endl;
        TEST_CHECK(d1.diagrams == 3 && d1.deformed >= 3, "Test 371: un diagramme et une déformée par barre (poutre 1 et poteau 1 distincts) : " << d1.diagrams << " diagrammes");
        TEST_CHECK(shown == withModel + d1.total(), "Test 371: chaque objet de résultat affiché est connu du gestionnaire (aucun orphelin) : " << shown << " = " << withModel << " + " << d1.total());

        // Plusieurs calculs et changements de grandeur : aucun cumul d'objets
        for (int k = 0; k < 5; ++k)
        {
            view.setResultsModel(results);
            rv->setDiagramType(k % 2 ? TSA::Geometry::DiagramType::ShearForceVz : TSA::Geometry::DiagramType::BendingMy);
            pump(2);
        }
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        TEST_CHECK(displayedObjects(view) == shown, "Test 371: aucun cumul après 5 calculs (" << displayedObjects(view) << " / " << shown << ")");

        // Absence de résultats (nouveau calcul en échec, projet ouvert) : la scène revient au modèle seul
        view.setResultsModel(nullptr);
        pump();
        TEST_CHECK(displayedObjects(view) == withModel && rv->diagnostics().total() == 0, "Test 371: résultats retirés (" << displayedObjects(view) << " / " << withModel << ")");

        // Nouveau projet : modèle vidé après un calcul affiché → scène vide (aucune courbe de l'ancien projet)
        view.setResultsModel(results);
        pump();
        view.setResultsModel(nullptr);
        m.clear();
        view.rebuildAllShapes();
        pump();
        TEST_CHECK(displayedObjects(view) == empty, "Test 371: nouveau projet — scène propre (" << displayedObjects(view) << " objets, vide : " << empty << ")");

        // Projet B, différent, calculé puis affiché ; aucune trace de A
        Model mb;
        view.setModel(&mb);
        auto rb = portalResults(mb, 4.5, 35.0);
        view.rebuildAllShapes();
        view.setResultsModel(rb);
        rv->setDiagramType(TSA::Geometry::DiagramType::BendingMy);
        pump();
        TEST_CHECK(rv->diagnostics().diagrams == 3 && displayedObjects(view) - rv->diagnostics().total() > 0, "Test 371: projet B affiché avec ses seuls résultats");
        view.setResultsModel(nullptr);
        view.setModel(nullptr);
        pump();
        view.hide();
        std::cout << "[PASS] Test 371: Aucun résultat résiduel (ids de familles distinctes, calculs successifs, nouveau projet)" << std::endl;
        ++passed;
    }
    return true;
}
