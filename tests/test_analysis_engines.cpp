// Suite « engines » : architecture d'analyse multi-moteurs (tests 130-138, 196).
// Registre, capacités, contexte, extraction 2D par axe de grille, mapping, validation pilotée par
// les capacités, non-régression OpenSees, fenêtre Analysis commune, remappage des résultats.

#include "test_common.h"

#include "Analysis/AnalysisController.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Automation/CommandRegistry.h"
#include "Analysis/Engines/Custom2D/Custom2DAdapter.h"
#include "Analysis/Engines/Custom2D/Custom2DEngine.h"
#include "Analysis/Engines/OpenSees/OpenSeesEngine.h"
#include "Analysis/OpenSeesSolver.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"
#include "UI/Analysis/AnalysisDialog.h"
#include "UI/Analysis/AnalysisEngineOptions.h"

#include <QEventLoop>
#include <QTimer>

using namespace TSA::Analysis;

namespace
{
/// Trois portiques plans sur les axes A, B, C (y = 0, 5, 10), travée x = 0 → 6 m, hauteur 3 m.
/// withTransverse : poutres longitudinales x = 0 entre A-B et B-C à z = 3 (relient les portiques).
struct Frames
{
    std::map<char, std::array<int, 4>> nodes;   // pied gauche, tête gauche, tête droite, pied droit
    std::map<char, std::array<int, 3>> members; // poteau gauche, poutre, poteau droit
    std::vector<int> transverse;
};

Frames buildFrames(Model& m, bool withTransverse)
{
    Frames f;
    const char axes[] = { 'A', 'B', 'C' };
    for (int i = 0; i < 3; ++i)
    {
        const double y = 5.0 * i;
        const int a = m.addNode(0, y, 0), b = m.addNode(0, y, 3), c = m.addNode(6, y, 3), d = m.addNode(6, y, 0);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(d)->setSupport(SupportDefinition::fixed());
        const int c1 = m.addColumn(a, b, Section::heb(200), Material::steelS235());
        const int bm = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int c2 = m.addColumn(d, c, Section::heb(200), Material::steelS235());
        f.nodes[axes[i]] = { a, b, c, d };
        f.members[axes[i]] = { c1, bm, c2 };
    }
    if (withTransverse)
    {
        f.transverse.push_back(m.addBar(f.nodes['A'][1], f.nodes['B'][1], Section::ipe(300), Material::steelS235(), BarRole::Beam));
        f.transverse.push_back(m.addBar(f.nodes['B'][1], f.nodes['C'][1], Section::ipe(300), Material::steelS235(), BarRole::Beam));
    }
    return f;
}

/// Grille cartésienne : axes 1, 2 (x = 0, 6) et A, B, C (y = 0, 5, 10).
std::string addGrid(GridManager& gm, double rotationDeg = 0.0)
{
    gm.clearAllGrids();   // GridManager crée une « Main Grid » par défaut
    GridDefinition def("Grille Test", GridType::Cartesian);
    def.setOrigin(0, 0, 0);
    def.setRotationDeg(rotationDeg);
    def.setXPositions({ 0.0, 6.0 });
    def.setYPositions({ 0.0, 5.0, 10.0 });
    def.setZLevels({ 0.0, 3.0 });
    return gm.addGrid(def)->id();
}

AnalysisScope gridScope(const std::string& gridId, GridAxisFamily family, const std::string& label)
{
    AnalysisScope s;
    s.type = ScopeType::GridAxis;
    s.gridId = gridId;
    s.axisFamily = family;
    s.axisLabel = label;
    return s;
}

/// Solveur de test (tests uniquement) : renvoie des valeurs DÉTERMINISTES dérivées des indices
/// reçus, pour vérifier le remappage indice 2D → objet TSA. Ce n'est pas un calcul.
class IndexEchoSolver final : public Custom2D::ISolver
{
public:
    Custom2D::Input lastInput;
    std::string name() const override { return "IndexEcho (test)"; }
    std::string version() const override { return "test"; }
    Custom2D::Output solve(const Custom2D::Input& in) override
    {
        lastInput = in;
        Custom2D::Output out;
        out.success = true;
        for (const auto& n : in.nodes)
        {
            out.displacements.push_back({ n.index, 0.001 * n.index, -0.002 * n.index, 1e-4 * n.index });
            if (n.fixX || n.fixY || n.fixRz) out.reactions.push_back({ n.index, 1.0 * n.index, 2.0 * n.index, 3.0 * n.index });
        }
        for (const auto& e : in.elements)
            out.elementForces.push_back({ e.index, -10.0 * e.index, 4.0 * e.index, 5.0 * e.index,
                                          10.0 * e.index, -4.0 * e.index, 6.0 * e.index, {} });
        Custom2D::CustomTable t;
        t.title = "Indicateur test";
        t.columns = { "Élément", "Valeur" };
        for (const auto& e : in.elements) t.rows.push_back({ static_cast<double>(e.index), 0.5 * e.index });
        t.elementColumn = 0;
        out.customTables.push_back(t);
        return out;
    }
};

AnalysisContext custom2dContext(const std::string& gridId, const std::string& axis)
{
    AnalysisContext c;
    c.engineId = Custom2DEngine::kId;
    c.dimension = AnalysisDimension::Plane2D;
    c.type = AnalysisType::LinearStatic;
    c.scope = gridScope(gridId, GridAxisFamily::Y, axis);
    c.common.includeSelfWeight = false;
    return c;
}
} // namespace

bool runSuite_Engines(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 130 : registre des moteurs
    // -------------------------------------------------------------------------
    {
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        TEST_CHECK(reg.hasEngine("opensees") && reg.hasEngine("custom2d"), "Test 130: OpenSees et Custom2D enregistrés");
        TEST_CHECK(reg.size() == 2 && reg.ids().front() == "opensees", "Test 130: ordre d'enregistrement conservé");
        TEST_CHECK(!reg.registerEngine(std::make_unique<Custom2DEngine>()), "Test 130: identifiant en double refusé");
        TEST_CHECK(reg.engine("inconnu") == nullptr && !reg.hasEngine(""), "Test 130: moteur inconnu");
        const auto infos = reg.engines();
        TEST_CHECK(infos[1].name == "Custom2D" && infos[1].version == "2.0.0",
                   "Test 130: version réelle du solveur branché (MetDeDeplacement)");
        TEST_CHECK(Custom2DEngine().info().version.empty(), "Test 130: aucune version inventée pour un solveur non connecté");
        std::cout << "[PASS] Test 130: Registre des moteurs" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 131 : capacités déclarées
    // -------------------------------------------------------------------------
    {
        const auto os = OpenSeesEngine().capabilities();
        TEST_CHECK(os.supports3D && !os.supports2D, "Test 131: OpenSees 3D uniquement (ndm 3)");
        TEST_CHECK(os.supportsFrame && os.supportsTruss && os.supportsCable && os.supportsSprings && !os.supportsShell,
                   "Test 131: OpenSees éléments filaires, pas de coques");
        TEST_CHECK(os.supportsStatic && os.supportsNonlinear,
                   "Test 131: OpenSees statique linéaire et non linéaire (TSA : calcul statique uniquement)");
        TEST_CHECK(os.planarElementPolicy == UnsupportedElementPolicy::ExcludeWithWarning, "Test 131: OpenSees exclut dalles/voiles avec avertissement");

        const auto c2 = Custom2DEngine().capabilities();
        TEST_CHECK(c2.supports2D && !c2.supports3D && c2.supportsFrame && !c2.supportsShell && c2.supportsStatic,
                   "Test 131: Custom2D ossature plane statique");
        TEST_CHECK(!c2.supportsNonlinear && !c2.providesGlobalStiffness,
                   "Test 131: Custom2D ne déclare rien d'autre");
        TEST_CHECK(c2.planarElementPolicy == UnsupportedElementPolicy::Reject, "Test 131: Custom2D refuse les coques");
        TEST_CHECK(c2.supportsAnalysisType(AnalysisType::LinearStatic) && !c2.supportsAnalysisType(AnalysisType::NonLinearStatic),
                   "Test 131: supportsAnalysisType");
        TEST_CHECK(!Custom2DEngine().availability().available && Custom2DEngine(std::make_unique<IndexEchoSolver>()).availability().available,
                   "Test 131: Custom2D disponible seulement avec un solveur connecté");
        std::cout << "[PASS] Test 131: Capacités des moteurs" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 132 : contexte Custom2D / axe B / niveau 2 / Dead + Live
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        buildFrames(m, false);
        TEST_CHECK(m.levelManager()->addLevelWithId("L2", "Niveau 2", 3.0) != nullptr, "Test 132: niveau");
        auto& lm = m.loadManager();
        const int dead = lm.addLoadCase(LoadCase(0, "Dead", LoadCaseCategory::Dead));
        const int live = lm.addLoadCase(LoadCase(0, "Live", LoadCaseCategory::Live));

        AnalysisContext c = custom2dContext(gid, "B");
        c.scope.levelId = "L2";
        c.loadCaseIds = { dead, live };
        c.engineSettings["opensees"] = QJsonObject{ { "numSteps", 42 } };

        const AnalysisContext back = AnalysisContext::fromJson(c.toJson());
        TEST_CHECK(back.engineId == "custom2d" && back.dimension == AnalysisDimension::Plane2D, "Test 132: moteur et dimension");
        TEST_CHECK(back.scope == c.scope && back.scope.hasLevelRestriction(), "Test 132: portée axe B ∩ niveau 2");
        TEST_CHECK(back.loadCaseIds == std::vector<int>({ dead, live }), "Test 132: cas Dead + Live");
        TEST_CHECK(back.settingsFor("opensees").value("numSteps").toInt() == 42, "Test 132: réglages par moteur conservés");
        bool ok = false;
        AnalysisContext::fromJson(QJsonObject{ { "schemaVersion", 99 } }, &ok);
        TEST_CHECK(!ok, "Test 132: version de schéma future signalée");

        const auto r = AnalysisScopeResolver::resolve(m, &gm, back.scope);
        TEST_CHECK(r.ok && r.label.find("Axe B") != std::string::npos && r.label.find("niveau") != std::string::npos,
                   "Test 132: portée résolue et nommée");
        TEST_CHECK(r.elements.beams.size() == 1 && r.elements.columns.empty(),
                   "Test 132: axe B ∩ niveau 2 = la seule poutre de l'axe B (poteaux exclus)");
        std::cout << "[PASS] Test 132: Contexte d'analyse" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 133 : extraction 2D de l'axe B dans un modèle à plusieurs plans
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        Frames f = buildFrames(m, true);
        const auto r = AnalysisScopeResolver::resolve(m, &gm, gridScope(gid, GridAxisFamily::Y, "B"));
        TEST_CHECK(r.ok && r.plane, "Test 133: plan de l'axe B");
        const AnalysisModel am = AnalysisModelExtractor::extract(m, r, AnalysisDimension::Plane2D);
        TEST_CHECK(am.snapshot.elementCount() == 3 && am.snapshot.nodeCount() == 4, "Test 133: 2 poteaux + 1 poutre, 4 nœuds");
        TEST_CHECK(am.snapshot.hasElement(StructuralElementKind::Beam, f.members['B'][1]) &&
                   !am.snapshot.hasElement(StructuralElementKind::Beam, f.members['A'][1]),
                   "Test 133: seuls les éléments de l'axe B");
        TEST_CHECK(am.crossingElements.size() == 2, "Test 133: 2 poutres transversales reliées à B signalées");
        const auto uv = am.planarCoordinates.at(f.nodes['B'][2]);
        TEST_CHECK(approxEqual(uv.first, 6.0, 1e-9) && approxEqual(uv.second, 3.0, 1e-9), "Test 133: coordonnées dans le plan (u, v)");
        TEST_CHECK(am.outOfPlaneNodes.empty(), "Test 133: aucun nœud hors plan");

        // Axe « 2 » (x = 6) : plan vertical normal à X.
        const auto r2 = AnalysisScopeResolver::resolve(m, &gm, gridScope(gid, GridAxisFamily::X, "2"));
        TEST_CHECK(r2.ok && r2.elements.columns.size() == 3 && r2.elements.beams.empty(), "Test 133: axe 2 = 3 poteaux droits");

        // Grille tournée de 90° : l'axe B (y local = 5) devient le plan x = -5.
        Model mr;
        GridManager gmr;
        const std::string gidr = addGrid(gmr, 90.0);
        const int a = mr.addNode(-5, 0, 0), b = mr.addNode(-5, 0, 3), c = mr.addNode(-5, 6, 3);
        mr.addColumn(a, b, Section::heb(200), Material::steelS235());
        mr.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        mr.addColumn(mr.addNode(0, 0, 0), mr.addNode(0, 0, 3), Section::heb(200), Material::steelS235());   // axe A
        const auto rr = AnalysisScopeResolver::resolve(mr, &gmr, gridScope(gidr, GridAxisFamily::Y, "B"));
        TEST_CHECK(rr.ok && rr.elements.columns.size() == 1 && rr.elements.beams.size() == 1,
                   "Test 133: axe B d'une grille tournée (origine + rotation de GridDefinition)");
        TEST_CHECK(approxEqual(rr.plane->u.Y(), 1.0, 1e-12), "Test 133: u suit la ligne d'axe tournée");

        const auto bad = AnalysisScopeResolver::resolve(m, &gm, gridScope(gid, GridAxisFamily::Y, "Z"));
        TEST_CHECK(!bad.ok && !bad.error.empty(), "Test 133: axe inexistant refusé explicitement");

        const auto options = AnalysisScopeResolver::availableScopes(m, &gm);
        int axes = 0;
        for (const auto& o : options) axes += o.scope.type == ScopeType::GridAxis ? 1 : 0;
        TEST_CHECK(axes == 5 && options.front().scope.type == ScopeType::EntireModel, "Test 133: portées proposées (A, B, C, 1, 2)");
        std::cout << "[PASS] Test 133: Extraction 2D par axe de grille" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 134 : mapping TSA ↔ modèle d'analyse
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        Frames f = buildFrames(m, false);
        const auto r = AnalysisScopeResolver::resolve(m, &gm, gridScope(gid, GridAxisFamily::Y, "B"));
        const AnalysisModel am = AnalysisModelExtractor::extract(m, r, AnalysisDimension::Plane2D);
        const auto& map = am.mapping;
        TEST_CHECK(map.nodeCount() == 4 && map.elementCount() == 3, "Test 134: tailles");
        for (int id : f.nodes['B'])
        {
            const int idx = map.analysisNode(id);
            TEST_CHECK(idx >= 1 && idx <= 4 && map.tsaNode(idx) == id, "Test 134: nœud TSA ↔ indice d'analyse");
        }
        const ElementKey beamB { StructuralElementKind::Beam, f.members['B'][1] };
        const int ei = map.analysisElement(beamB);
        TEST_CHECK(ei > 0 && map.tsaElement(ei) && *map.tsaElement(ei) == beamB, "Test 134: élément TSA ↔ indice d'analyse");
        TEST_CHECK(map.analysisNode(f.nodes['A'][0]) == 0 && !map.tsaElement(999) && map.tsaNode(0) == 0,
                   "Test 134: objets hors portée non mappés");
        // Poutre 1 et poteau 1 (mêmes ids TSA, familles différentes) : indices distincts.
        const ElementKey col1 { StructuralElementKind::Column, f.members['B'][0] };
        TEST_CHECK(map.analysisElement(col1) != ei, "Test 134: familles distinctes");
        std::cout << "[PASS] Test 134: Mapping TSA <-> analyse" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 135 : validation pilotée par les capacités
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        Frames f = buildFrames(m, false);
        m.addWall(f.nodes['C'][0], f.nodes['C'][3], 3.0, 0.2);           // voile dans l'axe C
        m.addTrussMember(f.nodes['A'][0], f.nodes['A'][2], 0.002);        // diagonale dans l'axe A

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisEngineRegistry bare;
        bare.registerEngine(std::make_unique<Custom2DEngine>());
        AnalysisManager bareMgr(bare);
        auto hasError = [](const ValidationResult& v, const std::string& needle) {
            for (const auto& e : v.texts(ValidationSeverity::Error))
                if (e.find(needle) != std::string::npos) return true;
            return false;
        };

        auto ok = mgr.prepare(m, &gm, custom2dContext(gid, "B"));
        TEST_CHECK(ok.extracted && ok.validation.isValid(), "Test 135: axe B compatible avec Custom2D");
        TEST_CHECK(bareMgr.prepare(m, &gm, custom2dContext(gid, "B")).validation.hasWarnings(), "Test 135: solveur non connecté signalé");

        auto shell = mgr.prepare(m, &gm, custom2dContext(gid, "C"));
        TEST_CHECK(!shell.validation.isValid() && hasError(shell.validation, "ne supporte pas les éléments de type Shell"),
                   "Test 135: voile refusé par Custom2D, message explicite");

        auto truss = mgr.prepare(m, &gm, custom2dContext(gid, "A"));
        TEST_CHECK(truss.validation.isValid(), "Test 135: treillis accepté (MetDeDeplacement : barres articulées)");
        TEST_CHECK(hasError(bareMgr.prepare(m, &gm, custom2dContext(gid, "A")).validation, "Treillis"),
                   "Test 135: treillis refusé par un moteur qui ne le déclare pas");

        AnalysisContext whole = custom2dContext(gid, "B");
        whole.scope = AnalysisScope{};
        TEST_CHECK(hasError(mgr.prepare(m, &gm, whole).validation, "portée plane"), "Test 135: 2D sans plan refusé");

        AnalysisContext in3d = custom2dContext(gid, "B");
        in3d.dimension = AnalysisDimension::Space3D;
        TEST_CHECK(hasError(mgr.prepare(m, &gm, in3d).validation, "ne calcule pas en 3D"), "Test 135: dimension non supportée");

        AnalysisContext nonlinear = custom2dContext(gid, "B");
        nonlinear.type = AnalysisType::NonLinearStatic;
        TEST_CHECK(hasError(mgr.prepare(m, &gm, nonlinear).validation, "type d'analyse"), "Test 135: type non supporté");

        AnalysisContext unknown = custom2dContext(gid, "B");
        unknown.engineId = "moteur-x";
        TEST_CHECK(!mgr.prepare(m, &gm, unknown).extracted, "Test 135: moteur inconnu");

        // OpenSees : le même voile n'est qu'un avertissement (comportement historique).
        AnalysisContext os = custom2dContext(gid, "C");
        os.engineId = "opensees";
        os.dimension = AnalysisDimension::Space3D;
        const auto osPrep = mgr.prepare(m, &gm, os);
        TEST_CHECK(osPrep.validation.isValid() && osPrep.validation.hasWarnings(), "Test 135: OpenSees exclut le voile avec avertissement");

        // Le calcul est refusé pour un modèle invalide, sans résultat.
        const auto refused = mgr.run(custom2dContext(gid, "C"), shell);
        TEST_CHECK(!refused.success && !refused.results.hasResults(), "Test 135: aucun calcul sur modèle invalide");
        const auto notConnected = bareMgr.run(custom2dContext(gid, "B"), bareMgr.prepare(m, &gm, custom2dContext(gid, "B")));
        TEST_CHECK(!notConnected.success && notConnected.message.find("non connecté") != std::string::npos &&
                   !notConnected.results.hasResults(), "Test 135: Custom2D non connecté : refus explicite, aucun résultat fictif");
        std::cout << "[PASS] Test 135: Validation pilotée par les capacités" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 136 : OpenSees derrière AnalysisEngine (non-régression + portée)
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        Frames f = buildFrames(m, false);   // portiques indépendants
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        for (char axis : { 'A', 'B', 'C' })
            m.loadManager().addNodalLoad(NodalLoad(0, f.nodes[axis][1], lc, 10.0, 0.0, -20.0));

        AnalysisParameters legacyParams;
        legacyParams.includeSelfWeight = false;
        OpenSeesSolver legacy;
        QString err;
        TEST_CHECK(legacy.solveSynchronous(m, legacyParams, &err), "Test 136: chemin historique OpenSees");

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "opensees";
        c.common.includeSelfWeight = false;
        c.engineSettings["opensees"] = OpenSeesEngine::settingsFromParameters(legacyParams);

        const auto prepAll = mgr.prepare(m, &gm, c);
        const auto runAll = mgr.run(c, prepAll);
        TEST_CHECK(runAll.success, "Test 136: modèle complet via AnalysisManager");
        for (const auto& [id, d] : legacy.results().allDisplacements())
        {
            const auto e = runAll.results.nodeDisplacement(id);
            TEST_CHECK(e.ux == d.ux && e.uz == d.uz && e.ry == d.ry, "Test 136: résultats identiques au chemin historique");
        }
        TEST_CHECK(runAll.results.executionMetadata().engineId == "opensees" && runAll.results.availability().elementForces,
                   "Test 136: traçabilité moteur et catégories disponibles");

        c.scope = gridScope(gid, GridAxisFamily::Y, "B");
        const auto prepB = mgr.prepare(m, &gm, c);
        const auto runB = mgr.run(c, prepB);
        TEST_CHECK(runB.success, "Test 136: portée axe B en 3D");
        TEST_CHECK(runB.results.allDisplacements().size() == 4, "Test 136: seuls les nœuds de l'axe B");
        for (int id : f.nodes['B'])
        {
            const auto a = legacy.results().nodeDisplacement(id), b = runB.results.nodeDisplacement(id);
            TEST_CHECK(std::abs(a.ux - b.ux) <= 1e-9 * std::max(1e-12, std::abs(a.ux)) + 1e-15 &&
                       std::abs(a.uz - b.uz) <= 1e-9 * std::max(1e-12, std::abs(a.uz)) + 1e-15,
                       "Test 136: portique B isolé = portique B du modèle complet");
        }
        TEST_CHECK(runB.results.executionMetadata().analysisScope.find("Axe B") != std::string::npos, "Test 136: portée tracée");
        std::cout << "[PASS] Test 136: OpenSees via AnalysisEngine" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 137 : une seule fenêtre Analysis pour tous les moteurs
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        buildFrames(m, false);
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        TSA::UI::AnalysisEngineOptionsRegistry options;
        TSA::UI::registerBuiltInEngineOptions(options);

        TSA::UI::AnalysisDialog dlg(mgr, options, &m, &gm, ElementSet{});
        TEST_CHECK(dlg.currentEngineId() == "opensees" && dlg.hasEngineOptionsPanel(), "Test 137: OpenSees + panneau d'options");
        TEST_CHECK(dlg.offeredDimensions() == std::vector<AnalysisDimension>({ AnalysisDimension::Space3D }),
                   "Test 137: dimensions d'OpenSees");
        const auto osTypes = dlg.offeredAnalysisTypes();
        TEST_CHECK(osTypes == std::vector<AnalysisType>({ AnalysisType::LinearStatic, AnalysisType::NonLinearStatic }),
                   "Test 137: types filtrés par capacités (statique uniquement)");
        const QJsonObject osSettings = dlg.context().settingsFor("opensees");
        TEST_CHECK(!osSettings.isEmpty(), "Test 137: réglages OpenSees produits par son panneau");

        TEST_CHECK(dlg.setEngine("custom2d"), "Test 137: même fenêtre, moteur Custom2D");
        TEST_CHECK(dlg.hasEngineOptionsPanel(), "Test 137: panneau d'options propre à Custom2D");
        TEST_CHECK(dlg.offeredDimensions() == std::vector<AnalysisDimension>({ AnalysisDimension::Plane2D }) &&
                   dlg.offeredAnalysisTypes() == std::vector<AnalysisType>({ AnalysisType::LinearStatic }),
                   "Test 137: Custom2D 2D statique");
        TEST_CHECK(dlg.isRunEnabled(), "Test 137: calcul possible (solveur MetDeDeplacement branché)");
        TEST_CHECK(dlg.scopeLabels().contains("Axe B — Grille Test") && dlg.selectScope("Axe B — Grille Test"),
                   "Test 137: portées par axe de grille");
        const auto v = dlg.validateNow();
        TEST_CHECK(v.isValid(), "Test 137: validation depuis la fenêtre");
        const AnalysisContext ctx = dlg.context();
        TEST_CHECK(ctx.engineId == "custom2d" && ctx.scope.axisLabel == "B" && ctx.dimension == AnalysisDimension::Plane2D,
                   "Test 137: contexte construit par la fenêtre");
        TEST_CHECK(ctx.settingsFor("opensees") == osSettings, "Test 137: réglages OpenSees conservés en changeant de moteur");

        TEST_CHECK(dlg.setEngine("opensees") && dlg.hasEngineOptionsPanel(), "Test 137: retour à OpenSees");
        std::cout << "[PASS] Test 137: Fenêtre Analysis commune" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 138 : conversion 2D et remappage des résultats vers les objets TSA
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const std::string gid = addGrid(gm);
        Frames f = buildFrames(m, false);
        m.getNode(f.nodes['B'][3])->setSupport(SupportDefinition::pinned());
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['B'][1], lc, 10.0, 7.0, -20.0));

        auto engine = std::make_unique<Custom2DEngine>(std::make_unique<IndexEchoSolver>());
        AnalysisEngineRegistry reg;
        reg.registerEngine(std::move(engine));
        AnalysisManager mgr(reg);
        const AnalysisContext c = custom2dContext(gid, "B");
        const auto prep = mgr.prepare(m, &gm, c);
        TEST_CHECK(prep.canRun(), "Test 138: portée valide");

        // Conversion : appuis projetés à partir des blocages DÉFINIS (pas des blocages anti-singularité 3D).
        ValidationResult diag;
        const auto in = Custom2D::buildInput(c, prep.model, &diag);
        const auto& map = prep.model.mapping;
        const auto& fixedNode = in.nodes[map.analysisNode(f.nodes['B'][0]) - 1];
        const auto& pinnedNode = in.nodes[map.analysisNode(f.nodes['B'][3]) - 1];
        TEST_CHECK(fixedNode.fixX && fixedNode.fixY && fixedNode.fixRz, "Test 138: encastrement → 3 DDL bloqués");
        TEST_CHECK(pinnedNode.fixX && pinnedNode.fixY && !pinnedNode.fixRz, "Test 138: articulation → rotation libre");
        TEST_CHECK(in.nodalLoads.size() == 1 && approxEqual(in.nodalLoads[0].fx, 10.0) && approxEqual(in.nodalLoads[0].fy, -20.0),
                   "Test 138: charge projetée dans le plan");
        TEST_CHECK(diag.hasWarnings(), "Test 138: composante hors plan (Fy = 7 kN) signalée");
        const auto* beamSnap = prep.model.snapshot.findElement(StructuralElementKind::Beam, f.members['B'][1]);
        const auto& beamIn = *std::find_if(in.elements.begin(), in.elements.end(), [&](const auto& e) { return e.index == beamSnap->tag; });
        TEST_CHECK(approxEqual(beamIn.I, Section::ipe(300).iy(), 1e-12) && approxEqual(beamIn.E, beamSnap->material.mechanical.youngModulus * 1e-3, 1e-6),
                   "Test 138: inertie de flexion dans le plan = axe fort, E en kPa");
        TEST_CHECK(approxEqual(in.request.gravityY, -1.0, 1e-12) && approxEqual(in.request.gravityX, 0.0, 1e-12),
                   "Test 138: pesanteur dans le plan");

        const auto run = mgr.run(c, prep);
        TEST_CHECK(run.success, "Test 138: calcul via le solveur connecté");
        const auto& res = run.results;
        // Nœud tête droite de l'axe B : (ux, uy, θz) du plan → (UX, UZ, RY = -θz) en global.
        const int topRight = f.nodes['B'][2];
        const int idx = map.analysisNode(topRight);
        const auto d = res.nodeDisplacement(topRight);
        TEST_CHECK(approxEqual(d.ux, 0.001 * idx, 1e-15) && approxEqual(d.uz, -0.002 * idx, 1e-15) &&
                   approxEqual(d.uy, 0.0, 1e-15) && approxEqual(d.ry, -1e-4 * idx, 1e-15),
                   "Test 138: déplacement remappé sur le nœud TSA en 3D global");
        TEST_CHECK(res.hasNodeReaction(f.nodes['B'][0]) && !res.hasNodeReaction(topRight), "Test 138: réactions sur les appuis");

        const ElementKey beamKey { StructuralElementKind::Beam, f.members['B'][1] };
        const auto* er = res.getElementResults(beamKey);
        const int ei = map.analysisElement(beamKey);
        TEST_CHECK(er != nullptr, "Test 138: efforts remappés sur la poutre TSA");
        TEST_CHECK(approxEqual(er->startForces.N, 10.0 * ei, 1e-9) && approxEqual(er->endForces.N, 10.0 * ei, 1e-9),
                   "Test 138: N (convention RDM, traction > 0)");
        TEST_CHECK(approxEqual(std::abs(er->startForces.My), 5.0 * ei, 1e-9) && approxEqual(er->startForces.Mz, 0.0, 1e-12) &&
                   approxEqual(std::abs(er->startForces.Vz), 4.0 * ei, 1e-9) && approxEqual(er->startForces.Vy, 0.0, 1e-12),
                   "Test 138: flexion dans le plan → axe fort (My, Vz) comme OpenSees");
        TEST_CHECK(er->intermediateStations.empty(), "Test 138: aucune station inventée");

        const auto& av = res.availability();
        TEST_CHECK(av.displacements && av.reactions && av.elementForces && !av.globalStiffness,
                   "Test 138: catégories de résultats = capacités ∧ données");
        TEST_CHECK(res.executionMetadata().engineId == "custom2d" && res.executionMetadata().solverEngine == "Custom2D" &&
                   res.executionMetadata().analysisDimension == "2d", "Test 138: traçabilité du moteur");
        TEST_CHECK(res.engineTables().size() == 1 && res.engineTables()[0].rows.size() == 3, "Test 138: table propre au moteur");
        bool labelled = false;
        for (const auto& row : res.engineTables()[0].rows) labelled |= row[0] == beamKey.label();
        TEST_CHECK(labelled, "Test 138: table propre remappée sur les identifiants TSA");
        std::cout << "[PASS] Test 138: Remappage des résultats Custom2D" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 196 : contrôleur d'analyse partagé (session) — calcul en tâche de fond, système K·U = F,
    // réglages enregistrés avec le projet, invalidation des résultats
    // -------------------------------------------------------------------------
    {
        TSA::Project::ProjectSession session;
        Model& m = session.model();
        const std::string gid = addGrid(session.grids());
        Frames f = buildFrames(m, false);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['B'][1], lc, 10.0, 0.0, -20.0));

        AnalysisController& ac = session.analysis();
        TEST_CHECK(ac.registry().hasEngine("opensees") && ac.registry().hasEngine("custom2d"),
                   "Test 196: moteurs intégrés enregistrés par le contrôleur");
        TEST_CHECK(ac.manager().registry().engine("custom2d")->capabilities().providesGlobalStiffness,
                   "Test 196: Custom2D déclare la matrice de rigidité (solveur MetDeDeplacement)");

        AnalysisContext c = custom2dContext(gid, "B");
        c.engineSettings["custom2d"] = QJsonObject { { "exportSystem", true } };
        ac.setContext(c);
        TEST_CHECK(ac.storeContextInModel() && !m.analysisSettingsJson().empty(), "Test 196: réglages enregistrés dans le modèle");
        ac.setContext(AnalysisContext());
        TEST_CHECK(ac.restoreContextFromModel() && ac.context().engineId == "custom2d" && ac.context().scope == c.scope,
                   "Test 196: réglages relus depuis le modèle");

        // Calcul asynchrone : la main revient tout de suite, fin signalée par finished().
        bool done = false, ok = false;
        int progressEvents = 0, resultsEvents = 0;
        QEventLoop loop;
        QObject::connect(&ac, &AnalysisController::progressChanged, &loop, [&](int, const QString&) { ++progressEvents; });
        QObject::connect(&ac, &AnalysisController::resultsChanged, &loop, [&] { ++resultsEvents; });
        QObject::connect(&ac, &AnalysisController::finished, &loop, [&](bool success, const QString&) {
            done = true;
            ok = success;
            loop.quit();
        });
        QString error;
        TEST_CHECK(ac.start(&error), "Test 196: calcul lancé en tâche de fond");
        TEST_CHECK(ac.isRunning() && !ac.start(), "Test 196: un seul calcul à la fois");
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();
        TEST_CHECK(done && ok && !ac.isRunning(), "Test 196: calcul terminé avec succès");
        TEST_CHECK(progressEvents > 0 && resultsEvents == 1, "Test 196: progression puis publication des résultats");

        const auto res = ac.results();
        TEST_CHECK(res && res->isValid() && ac.resultsUpToDate() && res->allDisplacements().size() == 4,
                   "Test 196: résultats publiés (portique B)");
        const auto& adv = res->advanced();
        TEST_CHECK(res->availability().globalStiffness && res->availability().dofMapping && adv.hasGlobalStiffness,
                   "Test 196: K globale et DDL disponibles");
        // Portique bi-encastré : 2 nœuds libres × 3 DDL.
        TEST_CHECK(adv.kGlobal.rows == 6 && adv.dofMap.equationCount() == 6 && adv.loadVector.size() == 6 &&
                   adv.displacementVector.size() == 6, "Test 196: 6 équations (2 nœuds libres × 3 DDL)");
        bool symmetric = true;
        for (std::size_t q = 0; q < adv.kGlobal.values.size(); ++q)
            symmetric &= approxEqual(adv.kGlobal.at(adv.kGlobal.colIndex[q], adv.kGlobal.rowIndex[q]), adv.kGlobal.values[q], 1e-9);
        TEST_CHECK(symmetric, "Test 196: K symétrique");
        const std::vector<double> ku = adv.kGlobal.multiply(adv.displacementVector);
        double residual = 0.0, scale = 0.0;
        for (std::size_t i = 0; i < ku.size(); ++i)
        {
            residual = std::max(residual, std::abs(ku[i] - adv.loadVector[i]));
            scale = std::max(scale, std::abs(adv.loadVector[i]));
        }
        TEST_CHECK(scale > 0.0 && residual <= 1e-8 * scale, "Test 196: K·U = F vérifié");
        bool mapped = true;
        for (const auto& e : adv.dofMap.equations) mapped &= e.nodeId == f.nodes['B'][1] || e.nodeId == f.nodes['B'][2];
        TEST_CHECK(mapped, "Test 196: équations rattachées aux nœuds TSA libres");

        // Modification du modèle : résultats invalidés, signal différé émis une fois.
        int stale = 0;
        QObject::connect(&ac, &AnalysisController::resultsBecameStale, &loop, [&] { ++stale; });
        m.pushUndoState("Déplacer", "");
        m.getNode(f.nodes['B'][1])->setCoordinates(0.0, 5.0, 3.5);
        m.notifyNodeModified(f.nodes['B'][1]);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        TEST_CHECK(!res->isValid() && !ac.resultsUpToDate() && stale == 1, "Test 196: résultats invalidés après modification");

        // Calcul synchrone (scripts, Blueprint) et oubli des résultats.
        const auto sync = ac.runBlocking(ac.context(), ac.prepare());
        TEST_CHECK(sync.success && ac.results() && ac.results()->isValid(), "Test 196: calcul synchrone publié");
        ac.clearResults();
        TEST_CHECK(!ac.results() && !ac.resultsUpToDate(), "Test 196: résultats oubliés (nouveau projet)");

        // Commandes du registre central (console, Blueprint, IA) : calcul et lecture des résultats.
        using TSA::Automation::executeCommandLine;
        const auto& reg = TSA::Automation::CommandRegistry::builtIn();
        TEST_CHECK(!executeCommandLine(reg, session, "results.summary").ok, "Test 196: aucun résultat → requête refusée");
        TEST_CHECK(!executeCommandLine(reg, session, "analysis.run axis=Z").ok, "Test 196: axe inconnu refusé");
        TEST_CHECK(!executeCommandLine(reg, session, "analysis.run engine=inconnu").ok, "Test 196: moteur inconnu refusé");
        const auto runCmd = executeCommandLine(reg, session, "analysis.run engine=custom2d axis=A export_system=true");
        TEST_CHECK(runCmd.ok && std::get<long long>(runCmd.outputs.at("equations")) == 6 &&
                   std::get<double>(runCmd.outputs.at("max_displacement")) == 0.0,
                   "Test 196: analysis.run (axe A, non chargé : déplacements nuls)");
        TEST_CHECK(session.analysis().context().scope.axisLabel == "A", "Test 196: portée enregistrée dans les réglages");
        const auto runB = executeCommandLine(reg, session, "analysis.run axis=B");
        const auto sum = executeCommandLine(reg, session, "results.summary");
        TEST_CHECK(runB.ok && sum.ok && std::get<double>(sum.outputs.at("max_displacement")) > 0.0 &&
                   std::get<bool>(sum.outputs.at("up_to_date")), "Test 196: results.summary après calcul de l'axe B");
        const auto nd = executeCommandLine(reg, session, "results.node_displacement id=" + std::to_string(f.nodes['B'][1]));
        TEST_CHECK(nd.ok && std::get<double>(nd.outputs.at("magnitude")) > 0.0, "Test 196: results.node_displacement");
        TEST_CHECK(!executeCommandLine(reg, session, "results.node_displacement id=" + std::to_string(f.nodes['A'][1])).ok,
                   "Test 196: nœud hors portée refusé");

        // Modèle plan sans grille (console) : portée « plan du modèle », calculable en 2D tel quel.
        TSA::Project::ProjectSession plain;
        Model& pm = plain.model();
        plain.grids().clearAllGrids();
        const int a = pm.addNode(0, 0, 0), b = pm.addNode(4, 0, 0);
        pm.getNode(a)->setSupport(SupportDefinition::fixed());
        const int cb = pm.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int plc = pm.loadManager().addLoadCase(LoadCase(0, "P", LoadCaseCategory::Live));
        pm.loadManager().addNodalLoad(NodalLoad(0, b, plc, 0.0, 0.0, -10.0));
        bool offered = false;
        for (const auto& o : AnalysisScopeResolver::availableScopes(pm, &plain.grids()))
            offered |= o.scope.type == ScopeType::ModelPlane && o.label == "Plan du modèle (vertical XZ)";
        TEST_CHECK(offered, "Test 196: portée « plan du modèle » proposée (poutre colinéaire → plan vertical)");
        AnalysisContext noWeight = plain.analysis().context();
        noWeight.common.includeSelfWeight = false;   // charge d'extrémité seule (solution analytique)
        plain.analysis().setContext(noWeight);
        const auto cons = executeCommandLine(reg, plain, "analysis.run engine=custom2d axis=plan");
        TEST_CHECK(cons.ok, "Test 196: console calculée sans grille (" << cons.message << ")");
        const auto* bar = pm.getBeam(cb);
        const double EI = bar->material().mechanical.E() * 1e-3 * bar->section().iy();   // E en Pa → kN/m², EI en kN·m²
        const double expected = 10.0 * 64.0 / (3.0 * EI);
        TEST_CHECK(std::abs(std::get<double>(cons.outputs.at("max_displacement")) - expected) <= 1e-3 * expected,
                   "Test 196: flèche de la console = P·L³ / 3EI");
        TEST_CHECK(std::abs(std::get<double>(cons.outputs.at("max_moment")) - 40.0) <= 1e-6,
                   "Test 196: moment d'encastrement = P·L");
        std::cout << "[PASS] Test 196: Contrôleur d'analyse partagé" << std::endl;
        ++passed;
    }
    return true;
}
