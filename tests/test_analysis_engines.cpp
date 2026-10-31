// Suite « engines » : architecture d'analyse multi-moteurs (tests 130-138, 196, 205-206).
// Registre, capacités, contexte, extraction 2D par axe de grille, mapping, validation pilotée par
// les capacités, non-régression OpenSees, fenêtre Analysis commune, remappage des résultats.

#include "test_common.h"

#include "Analysis/AnalysisController.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Automation/CommandRegistry.h"
#include "Analysis/Engines/Custom2D/Custom2DAdapter.h"
#include "Analysis/Engines/Custom2D/Custom2DEngine.h"
#include "Analysis/Engines/OpenSees/OpenSeesEngine.h"
#ifdef TSA_HAS_FEM3D
#include "Analysis/Engines/Fem3D/Fem3DEngine.h"
#endif
#include "Analysis/CalculationSnapshot.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"
#include "Analysis/OpenSeesManager.h"
#include "Analysis/OpenSeesSolver.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"
#include "UI/Analysis/AnalysisDialog.h"
#include "UI/Analysis/AnalysisEngineOptions.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
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
#ifdef TSA_HAS_FEM3D
        constexpr std::size_t builtInEngines = 3;   // + moteur EF 3D Fem3D (dépôt voisin 3DDesplacement)
#else
        constexpr std::size_t builtInEngines = 2;
#endif
        TEST_CHECK(reg.size() == builtInEngines && reg.ids().front() == "opensees", "Test 130: ordre d'enregistrement conservé (OpenSees par défaut)");
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
        TEST_CHECK(os.supportsFrame && os.supportsTruss && os.supportsCable && os.supportsSprings && os.supportsShell,
                   "Test 131: OpenSees éléments filaires et coques maillées (ShellMITC4 / ShellDKGT, suite surfaces)");
        TEST_CHECK(os.supportsStatic && os.supportsModal,
                   "Test 131: OpenSees statique linéaire et modale");
        TEST_CHECK(os.planarElementPolicy == UnsupportedElementPolicy::ExcludeWithWarning, "Test 131: OpenSees exclut avec avertissement les dalles/voiles non maillés");

        const auto c2 = Custom2DEngine().capabilities();
        TEST_CHECK(c2.supports2D && !c2.supports3D && c2.supportsFrame && !c2.supportsShell && c2.supportsStatic,
                   "Test 131: Custom2D ossature plane statique");
        TEST_CHECK(!c2.supportsModal && !c2.providesGlobalStiffness,
                   "Test 131: Custom2D ne déclare rien d'autre");
        TEST_CHECK(c2.planarElementPolicy == UnsupportedElementPolicy::Reject, "Test 131: Custom2D refuse les coques");
        TEST_CHECK(c2.supportsAnalysisType(AnalysisType::LinearStatic) && !c2.supportsAnalysisType(AnalysisType::Modal),
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
        c.engineSettings["opensees"] = QJsonObject{ { "systemSolver", 2 } };

        const AnalysisContext back = AnalysisContext::fromJson(c.toJson());
        TEST_CHECK(back.engineId == "custom2d" && back.dimension == AnalysisDimension::Plane2D, "Test 132: moteur et dimension");
        TEST_CHECK(back.scope == c.scope && back.scope.hasLevelRestriction(), "Test 132: portée axe B ∩ niveau 2");
        TEST_CHECK(back.loadCaseIds == std::vector<int>({ dead, live }), "Test 132: cas Dead + Live");
        TEST_CHECK(back.settingsFor("opensees").value("systemSolver").toInt() == 2, "Test 132: réglages par moteur conservés");
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

        AnalysisContext modal = custom2dContext(gid, "B");
        modal.type = AnalysisType::Modal;
        TEST_CHECK(hasError(mgr.prepare(m, &gm, modal).validation, "type d'analyse"), "Test 135: type non supporté");

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
        TEST_CHECK(osTypes == std::vector<AnalysisType>({ AnalysisType::LinearStatic, AnalysisType::Modal }),
                   "Test 137: types filtrés par capacités (statique linéaire, modale)");
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

    // -------------------------------------------------------------------------
    // TEST 205 : annulation reçue avant que le moteur ait créé son solveur (course d'annulation).
    // Avant correction, OpenSeesSolver::solveSnapshot remettait le drapeau d'arrêt à zéro et
    // OpenSeesEngine::cancel() ne trouvait pas encore le solveur : le calcul allait à son terme.
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        addGrid(gm);
        Frames f = buildFrames(m, false);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['A'][1], lc, 10.0, 0.0, -20.0));

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "opensees";
        c.common.includeSelfWeight = false;
        const auto prep = mgr.prepare(m, &gm, c);
        TEST_CHECK(prep.canRun(), "Test 205: modèle calculable");

        AnalysisRunCallbacks cancelled;
        cancelled.cancelRequested = [] { return true; };
        const auto stopped = mgr.run(c, prep, cancelled);
        TEST_CHECK(!stopped.success && stopped.message.find("interrompu") != std::string::npos
                       && !stopped.results.hasResults(),
                   "Test 205: annulation antérieure au solveur respectée (" << stopped.message << ")");

        const auto normal = mgr.run(c, prep, AnalysisRunCallbacks{});
        TEST_CHECK(normal.success, "Test 205: calcul suivant non affecté");

        // Solveur réutilisé : stop() avant solveSnapshot interrompt, puis le drapeau est réarmé.
        OpenSeesSolver solver;
        const CalculationSnapshot snap = CalculationSnapshot::capture(m);
        AnalysisParameters params;
        params.includeSelfWeight = false;
        QString err;
        solver.stop();
        TEST_CHECK(!solver.solveSnapshot(snap, params, &err) && err.contains(QStringLiteral("interrompu")),
                   "Test 205: stop() avant solveSnapshot respecté");
        TEST_CHECK(!solver.isRunning(), "Test 205: solveur au repos après interruption");
        TEST_CHECK(solver.solveSnapshot(snap, params, &err), "Test 205: drapeau réarmé pour le calcul suivant (" << err.toStdString() << ")");
        std::cout << "[PASS] Test 205: Annulation d'un calcul avant création du solveur" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 206 : modèle modifié pendant un calcul en tâche de fond. Avant correction, les résultats
    // (calculés sur l'état d'avant) étaient publiés « à jour » : le garde prenait la révision du
    // modèle à la publication et non au lancement (panneau d'analyse non modal).
    // -------------------------------------------------------------------------
    {
        TSA::Project::ProjectSession session;
        Model& m = session.model();
        const std::string gid = addGrid(session.grids());
        Frames f = buildFrames(m, false);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['B'][1], lc, 10.0, 0.0, -20.0));
        AnalysisController& ac = session.analysis();
        ac.setContext(custom2dContext(gid, "B"));

        bool done = false, ok = false;
        int stale = 0;
        QEventLoop loop;
        QObject::connect(&ac, &AnalysisController::resultsBecameStale, &loop, [&] { ++stale; });
        QObject::connect(&ac, &AnalysisController::finished, &loop, [&](bool success, const QString&) {
            done = true;
            ok = success;
            loop.quit();
        });
        QString error;
        TEST_CHECK(ac.start(&error), "Test 206: calcul lancé (" << error.toStdString() << ")");
        // Modification pendant le calcul : la fin du thread n'est traitée qu'à loop.exec().
        m.pushUndoState("Déplacer", "");
        m.getNode(f.nodes['B'][1])->setCoordinates(0.0, 5.0, 3.5);
        m.notifyNodeModified(f.nodes['B'][1]);
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        TEST_CHECK(done && ok, "Test 206: calcul terminé");
        TEST_CHECK(ac.results() && !ac.results()->isValid() && !ac.resultsUpToDate(),
                   "Test 206: résultats d'un état antérieur publiés comme obsolètes");
        TEST_CHECK(stale == 1, "Test 206: signal d'obsolescence émis une fois");

        // Sans modification pendant le calcul : résultats à jour (comportement inchangé).
        done = false;
        TEST_CHECK(ac.start(&error), "Test 206: second calcul lancé");
        loop.exec();
        TEST_CHECK(done && ok && ac.results() && ac.results()->isValid() && ac.resultsUpToDate(),
                   "Test 206: calcul sans modification concurrente à jour");
        std::cout << "[PASS] Test 206: Modèle modifié pendant un calcul en tâche de fond" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 212 : charge uniforme sur une partie de la barre transmise au calcul sur son intervalle
    // (avant correction : OpenSees appliquait beamUniform sur toute la barre, Custom2D aussi).
    // -------------------------------------------------------------------------
    {
        Model beam;
        GridManager gm;
        const int s = beam.addNode(0, 0, 0), e = beam.addNode(6, 0, 0);
        beam.getNode(s)->setSupport(SupportDefinition::fixed());
        beam.getNode(e)->setSupport(SupportDefinition::pinned());   // rotations libres (OpenSees refuse un système sans DDL)
        const int bm = beam.addBar(s, e, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        TSA::Model::MemberLoad partial = TSA::Model::MemberLoad::uniform(0, bm, 1, 10.0);
        partial.setX1(1.0);
        partial.setX2(3.0);
        beam.loadManager().addMemberLoad(partial);

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        const auto run = mgr.run(ctx, mgr.prepare(beam, &gm, ctx));
        TEST_CHECK(run.success, "Test 212: calcul OpenSees (" << run.message << ") | "
                   << run.results.journalLog().substr(run.results.journalLog().size() > 3000 ? run.results.journalLog().size() - 3000 : 0));
        TEST_CHECK(std::abs(run.results.equilibrium().reactionFz - 20.0) < 1e-6,
                   "Test 212: 10 kN/m sur [1, 3] m → réaction 20 kN (" << run.results.equilibrium().reactionFz << ")");
        // Encastrée en i, articulée en j, charge proche de i : réaction plus forte en i.
        const auto ri = run.results.nodeReaction(s), rj = run.results.nodeReaction(e);
        TEST_CHECK(ri.rz > rj.rz && rj.rz > 0.0, "Test 212: répartition des réactions cohérente avec l'intervalle");
        std::cout << "[PASS] Test 212: Charge uniforme partielle transmise au calcul" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TESTS 532-536 : version linéaire seule (analyse non linéaire retirée, ADR-025). Un ancien projet qui la demandait
    // est signalé et refusé : il n'est jamais calculé en linéaire à la place.
    // -------------------------------------------------------------------------
    auto textHas = [](const std::string& text, const std::string& needle) { return text.find(needle) != std::string::npos; };
    auto validationHas = [&](const ValidationResult& v, const std::string& needle) {
        for (const auto& e : v.texts(ValidationSeverity::Error))
            if (textHas(e, needle)) return true;
        return false;
    };
    // Portique simple calculable par OpenSees : poteau encastré, poutre, charge en tête.
    auto smallFrame = [](Model& m) {
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(4, 0, 3);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(c)->setSupport(SupportDefinition::pinned());
        m.addColumn(a, b, Section::heb(200), Material::steelS235());
        m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 5.0, 0.0, -10.0));
        return b;
    };
    {
        Model m;
        GridManager gm;
        smallFrame(m);
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);

        QJsonObject old { { "schemaVersion", 1 }, { "engineId", "opensees" }, { "analysisType", "nonlinear_static" } };
        bool ok = false;
        const AnalysisContext c = AnalysisContext::fromJson(old, &ok);
        TEST_CHECK(ok && c.unsupportedAnalysisType == "nonlinear_static", "Test 532: ancien type lu et marqué, réglages lisibles");
        TEST_CHECK(c.toJson().value("analysisType").toString() == "nonlinear_static", "Test 532: demande d'origine réécrite telle quelle");
        const auto features = mgr.unsupportedFeatures(c);
        TEST_CHECK(features.size() == 1 && textHas(features.front(), "non linéaire"), "Test 532: fonctionnalité retirée nommée");
        const auto prep = mgr.prepare(m, &gm, c);
        TEST_CHECK(!prep.canRun() && validationHas(prep.validation, "n'est plus prise en charge"), "Test 532: préparation refusée");

        // run() refuse aussi, même avec un modèle préparé par un contexte linéaire valide.
        AnalysisContext linear = c;
        linear.unsupportedAnalysisType.clear();
        linear.common.includeSelfWeight = false;
        const auto linearPrep = mgr.prepare(m, &gm, linear);
        TEST_CHECK(linearPrep.canRun() && mgr.unsupportedFeatures(linear).empty(), "Test 532: même contexte en linéaire accepté");
        AnalysisContext marked = linear;
        marked.unsupportedAnalysisType = "nonlinear_static";
        const auto refused = mgr.run(marked, linearPrep);
        TEST_CHECK(!refused.success && !refused.results.hasResults() && textHas(refused.message, "n'est plus prise en charge"),
                   "Test 532: calcul refusé sans résultat (" << refused.message << ")");

        old["analysisType"] = "pushover";
        const AnalysisContext unknown = AnalysisContext::fromJson(old, &ok);
        TEST_CHECK(ok && !mgr.unsupportedFeatures(unknown).empty() && textHas(mgr.unsupportedFeatures(unknown).front(), "pushover"),
                   "Test 532: type inconnu refusé, jamais converti en statique linéaire");
        old.remove("analysisType");
        TEST_CHECK(AnalysisContext::fromJson(old, &ok).unsupportedAnalysisType.empty() && ok, "Test 532: sans type → statique linéaire (anciens fichiers)");
        std::cout << "[PASS] Test 532: Ancien type d'analyse non linéaire refusé" << std::endl;
        ++passed;
    }
    {
        Model m;
        GridManager gm;
        smallFrame(m);
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "opensees";
        c.common.includeSelfWeight = false;
        const std::vector<std::pair<QJsonObject, std::string>> refusedSettings {
            { QJsonObject { { "geomTransf", 1 } }, "P-Delta" },
            { QJsonObject { { "geomTransf", 2 } }, "corotationnelle" },
            { QJsonObject { { "trussFormulation", 1 } }, "treillis corotationnels" },
            { QJsonObject { { "trussFormulation", 3 } }, "treillis corotationnels" },
        };
        for (const auto& [settings, needle] : refusedSettings)
        {
            c.engineSettings["opensees"] = settings;
            const auto prep = mgr.prepare(m, &gm, c);
            TEST_CHECK(!prep.canRun() && validationHas(prep.validation, needle), "Test 533: ancien réglage « " << needle << " » refusé");
        }
        // Réglages des anciennes versions sans effet sur un calcul linéaire : ignorés, le calcul reste possible.
        c.engineSettings["opensees"] = QJsonObject { { "geomTransf", 0 }, { "trussFormulation", 2 }, { "algorithm", 3 }, { "integrator", 0 },
                                                     { "numSteps", 10 }, { "tolerance", 1e-8 }, { "saveAllSteps", true } };
        TEST_CHECK(mgr.unsupportedFeatures(c).empty() && mgr.prepare(m, &gm, c).canRun(), "Test 533: clés inertes ignorées");

        // Le panneau OpenSees ne relit ni ne réécrit ces réglages.
        TSA::UI::AnalysisEngineOptionsRegistry options;
        TSA::UI::registerBuiltInEngineOptions(options);
        std::unique_ptr<TSA::UI::AnalysisEngineOptionsWidget> panel(options.create("opensees", nullptr));
        panel->loadSettings(QJsonObject { { "geomTransf", 1 }, { "numSteps", 10 }, { "systemSolver", 2 } });
        const QJsonObject saved = panel->saveSettings();
        TEST_CHECK(saved.keys() == QStringList({ "constraintHandler", "extractionLevel", "maxGlobalStiffnessDofs", "systemSolver", "useKiloNewtons" }) &&
                       saved.value("systemSolver").toInt() == 2,
                   "Test 533: panneau OpenSees limité aux réglages linéaires");
        std::cout << "[PASS] Test 533: Anciens réglages OpenSees non linéaires refusés" << std::endl;
        ++passed;
    }
    {
        Model m;
        const int head = smallFrame(m);
        const int top = m.addNode(4, 3, 3), ground = m.addNode(4, 3, 0);
        m.getNode(ground)->setSupport(SupportDefinition::pinned());
        m.addTrussMember(head, top, 0.05);
        m.addCable(top, ground, 0.020);
        AnalysisParameters p;
        p.type = AnalysisType::LinearStatic;
        p.includeSelfWeight = false;
        const std::string tcl = OpenSeesAnalysisBuilder::buildScript(CalculationSnapshot::capture(m), p);
        TEST_CHECK(textHas(tcl, "algorithm Linear\n") && textHas(tcl, "integrator LoadControl 1.0\n") && textHas(tcl, "analysis Static\n") &&
                       textHas(tcl, "set ok [analyze 1]\n") && textHas(tcl, "geomTransf Linear "),
                   "Test 534: statique linéaire, un seul pas, transformation linéaire");
        std::size_t analyzeCount = 0;
        for (std::size_t pos = tcl.find("[analyze "); pos != std::string::npos; pos = tcl.find("[analyze ", pos + 1)) ++analyzeCount;
        TEST_CHECK(analyzeCount == 1, "Test 534: une seule résolution (" << analyzeCount << ")");
        for (const char* forbidden : { "Newton", "BFGS", "Broyden", "PDelta", "Corotational", "DisplacementControl", "ArcLength", "MinUnbalDispNorm",
                                       "TSA_OPS_LOAD_FACTOR", "getTime" })
            TEST_CHECK(!textHas(tcl, forbidden), "Test 534: aucune commande non linéaire (" << forbidden << ")");
        TEST_CHECK(textHas(tcl, "element truss ") && textHas(tcl, "element corotTruss "),
                   "Test 534: treillis en truss ; câble en corotTruss historique, résolu en un pas linéaire");
        std::cout << "[PASS] Test 534: Script OpenSees statique linéaire" << std::endl;
        ++passed;
    }
    {
        // Ancien projet .tsa : contexte non linéaire et réglage P-Delta enregistrés (chunk SETT).
        const std::string dir = "./build/test_tsa_data/";
        std::filesystem::create_directories(dir);
        const QString file = QString::fromStdString(dir + "test_linear_only_legacy.tsa");
        Model legacy;
        smallFrame(legacy);
        QJsonObject settings { { "schemaVersion", 1 }, { "engineId", "opensees" }, { "analysisType", "nonlinear_static" },
                               { "engineSettings", QJsonObject { { "opensees", QJsonObject { { "geomTransf", 1 }, { "numSteps", 10 } } } } } };
        legacy.setAnalysisSettingsJson(QJsonDocument(settings).toJson(QJsonDocument::Compact).toStdString());
        std::string err;
        TEST_CHECK(TSAProjectIO::saveToFile(file, legacy, nullptr, &err), "Test 535: ancien projet écrit (" << err << ")");

        Model r;
        GridManager gm;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, r, nullptr, &err), "Test 535: ancien projet ouvert (" << err << ")");
        TEST_CHECK(r.nodes().size() == legacy.nodes().size() && r.loadManager().nodalLoads().size() == 1, "Test 535: données du projet conservées");
        AnalysisController ac(&r, &gm);
        TEST_CHECK(ac.restoreContextFromModel(), "Test 535: réglages lisibles (pas remplacés par les valeurs par défaut)");
        const std::string message = ac.unsupportedFeaturesMessage();
        TEST_CHECK(textHas(message, "non linéaire") && textHas(message, "P-Delta") && textHas(message, "analyse structurale linéaire"),
                   "Test 535: message clair (" << message << ")");
        const auto run = ac.runBlocking(ac.context(), ac.prepare());
        TEST_CHECK(!run.success && !ac.results(), "Test 535: aucun calcul, aucun résultat");
        ac.storeContextInModel();
        TEST_CHECK(textHas(r.analysisSettingsJson(), "nonlinear_static"), "Test 535: réenregistrer conserve la demande d'origine");

        // Fenêtre Analysis : bandeau, puis choix explicite d'une analyse linéaire.
        TSA::UI::AnalysisEngineOptionsRegistry options;
        TSA::UI::registerBuiltInEngineOptions(options);
        TSA::UI::AnalysisDialog dlg(ac.manager(), options, &r, &gm, ElementSet {});
        dlg.setContext(ac.context());
        TEST_CHECK(dlg.showsUnsupportedWarning(), "Test 535: bandeau d'avertissement affiché");
        const AnalysisContext chosen = dlg.context();
        TEST_CHECK(chosen.unsupportedAnalysisType.empty() && chosen.type == AnalysisType::LinearStatic && ac.manager().unsupportedFeatures(chosen).empty(),
                   "Test 535: analyse linéaire choisie dans la fenêtre");
        ac.setContext(chosen);
        TEST_CHECK(ac.unsupportedFeaturesMessage().empty() && ac.prepare().canRun(), "Test 535: calcul de nouveau possible après le choix");
        dlg.setContext(chosen);
        TEST_CHECK(!dlg.showsUnsupportedWarning(), "Test 535: bandeau retiré");
        std::cout << "[PASS] Test 535: Ancien projet non linéaire ouvert, signalé, jamais calculé en linéaire" << std::endl;
        ++passed;
    }
    {
        // Interface : aucun libellé qui laisse croire à une analyse non linéaire.
        Model m;
        GridManager gm;
        smallFrame(m);
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        TSA::UI::AnalysisEngineOptionsRegistry options;
        TSA::UI::registerBuiltInEngineOptions(options);
        TSA::UI::AnalysisDialog dlg(mgr, options, &m, &gm, ElementSet {});
        QStringList texts;
        for (const auto* w : dlg.findChildren<QWidget*>())
        {
            texts << w->toolTip();
            if (const auto* l = qobject_cast<const QLabel*>(w)) texts << l->text();
            if (const auto* b = qobject_cast<const QAbstractButton*>(w)) texts << b->text();
            if (const auto* g = qobject_cast<const QGroupBox*>(w)) texts << g->title();
            if (const auto* cb = qobject_cast<const QComboBox*>(w))
                for (int i = 0; i < cb->count(); ++i) texts << cb->itemText(i);
        }
        const QString all = texts.join('\n').toLower();
        for (const char* forbidden : { "non linéaire", "non-linéaire", "nonlinear", "newton", "p-delta", "second ordre", "2nd ordre", "corot",
                                       "arc-length", "displacement control", "contrôle du déplacement", "load control", "nombre de pas",
                                       "incrément", "itérations", "tolérance de convergence", "grands déplacements", "bfgs", "broyden" })
            TEST_CHECK(!all.contains(QString::fromUtf8(forbidden)), "Test 536: fenêtre Analysis sans « " << forbidden << " »");
        TEST_CHECK(all.contains(QStringLiteral("statique linéaire")), "Test 536: libellés lus (statique linéaire présent)");
        std::cout << "[PASS] Test 536: Fenêtre Analysis sans option non linéaire" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TESTS 538-540 : BUG-148, barre à rotule sous charge non uniforme. Toutes les charges d'une barre à rotule passent par
    // des forces d'encastrement condensées. Avant correction (OpenSees) : moment non nul à la rotule et moment d'encastrement
    // faux, sans alerte (13,33 au lieu de 26,67 kN·m), équilibre global pourtant vérifié.
    // -------------------------------------------------------------------------
    {
        const double L = 6.0, w = 12.0;
        auto fixedEnd = [&](const BeamElementLoad& load) {
            std::array<double, 12> f {};
            load.fixedEndForces(L, f.data(), f.data() + 6);
            return f;
        };
        auto uniform = [&](double wy, double wz, BeamElementLoad::Releases releases) {
            BeamElementLoad q;
            q.wy = wy;
            q.wz = wz;
            q.releases = releases;
            return fixedEnd(q);
        };
        // [Fx Fy Fz Mx My Mz] en i (0-5) puis en j (6-11), forces exercées sur la barre.
        const double m8 = w * L * L / 8.0, v5 = 5.0 * w * L / 8.0, v3 = 3.0 * w * L / 8.0;
        auto f = uniform(w, 0.0, { false, false, false, true });
        TEST_CHECK(approxEqual(f[5], -m8, 1e-9) && std::abs(f[11]) < 1e-12 && approxEqual(f[1], -v5, 1e-9) && approxEqual(f[7], -v3, 1e-9),
                   "Test 538: rotule Mz en j : encastrée-appuyée (wL²/8, 5wL/8, 3wL/8)");
        f = uniform(w, 0.0, { false, true, false, false });
        TEST_CHECK(std::abs(f[5]) < 1e-12 && approxEqual(f[11], m8, 1e-9) && approxEqual(f[1], -v3, 1e-9) && approxEqual(f[7], -v5, 1e-9),
                   "Test 538: rotule Mz en i");
        f = uniform(0.0, w, { false, false, true, false });
        TEST_CHECK(approxEqual(f[4], m8, 1e-9) && std::abs(f[10]) < 1e-12 && approxEqual(f[2], -v5, 1e-9) && approxEqual(f[8], -v3, 1e-9),
                   "Test 538: rotule My en j (autre convention de signe du plan x-z)");
        f = uniform(0.0, w, { true, false, false, false });
        TEST_CHECK(std::abs(f[4]) < 1e-12 && approxEqual(f[10], -m8, 1e-9) && approxEqual(f[2], -v3, 1e-9) && approxEqual(f[8], -v5, 1e-9),
                   "Test 538: rotule My en i");
        f = uniform(w, 0.0, { false, true, false, true });
        TEST_CHECK(std::abs(f[5]) < 1e-12 && std::abs(f[11]) < 1e-12 && approxEqual(f[1], -w * L / 2.0, 1e-9) && approxEqual(f[7], -w * L / 2.0, 1e-9),
                   "Test 538: deux rotules : réactions de la poutre sur deux appuis");

        // Toute combinaison de rotules, toute famille de charges : moment nul à chaque rotule, charge statiquement inchangée.
        BeamElementLoad kinds[4];
        kinds[0].wx = 2.0, kinds[0].wy = -3.0, kinds[0].wz = 5.0;
        kinds[1] = kinds[0], kinds[1].kind = BeamElementLoad::Kind::Point, kinds[1].relativePosition = 0.3;
        kinds[2] = kinds[0], kinds[2].kind = BeamElementLoad::Kind::Linear, kinds[2].relativePosition = 0.2, kinds[2].relativeEnd = 0.9;
        kinds[2].wxB = -1.0, kinds[2].wyB = 4.0, kinds[2].wzB = 7.0;
        kinds[3] = kinds[2], kinds[3].kind = BeamElementLoad::Kind::Couple;
        for (auto load : kinds)
        {
            double gI[6], gJ[6];
            load.fixedFixedEndForces(L, gI, gJ);
            f = fixedEnd(load);
            TEST_CHECK(std::equal(f.begin(), f.begin() + 6, gI) && std::equal(f.begin() + 6, f.end(), gJ), "Test 538: sans rotule, forces de la barre bi-encastrée, à l'identique");
            for (int bits = 1; bits < 16; ++bits)
            {
                load.releases = { (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0, (bits & 8) != 0 };
                TEST_CHECK(load.nodalEquivalent(), "Test 538: charge d'une barre à rotule transmise aux nœuds");
                f = fixedEnd(load);
                TEST_CHECK((!load.releases.myI || std::abs(f[4]) < 1e-12) && (!load.releases.mzI || std::abs(f[5]) < 1e-12) &&
                               (!load.releases.myJ || std::abs(f[10]) < 1e-12) && (!load.releases.mzJ || std::abs(f[11]) < 1e-12),
                           "Test 538: moment nul à chaque rotule (rotules " << bits << ")");
                for (int k = 0; k < 4; ++k)
                    TEST_CHECK(std::abs(f[k] + f[k + 6] - gI[k] - gJ[k]) < 1e-9, "Test 538: résultante inchangée (composante " << k << ", rotules " << bits << ")");
                TEST_CHECK(std::abs(f[5] + f[11] + L * f[7] - (gI[5] + gJ[5] + L * gJ[1])) < 1e-9 &&
                               std::abs(f[4] + f[10] - L * f[8] - (gI[4] + gJ[4] - L * gJ[2])) < 1e-9,
                           "Test 538: moment résultant inchangé (rotules " << bits << ")");
            }
        }
        std::cout << "[PASS] Test 538: Forces d'encastrement condensées sur les rotules" << std::endl;
        ++passed;
    }
    {
        // Barre de 6 m entre deux nœuds encastrés : les rotules de la barre font seules le schéma statique, solutions analytiques.
        const double L = 6.0;
        struct Hinged
        {
            AnalysisRunResult run;
            int i = 0, j = 0;
            std::string script;
        };
        auto solve = [&](const EndRelease& atI, const EndRelease& atJ, auto&& addLoads) {
            Model m;
            GridManager gm;
            Hinged h;
            h.i = m.addNode(0, 0, 0);
            h.j = m.addNode(L, 0, 0);
            m.getNode(h.i)->setSupport(SupportDefinition::fixed());
            m.getNode(h.j)->setSupport(SupportDefinition::fixed());
            const int bar = m.addBar(h.i, h.j, Section::ipe(300), Material::steelS235(), BarRole::Beam);
            m.getBeam(bar)->setStartRelease(atI);
            m.getBeam(bar)->setEndRelease(atJ);
            const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
            addLoads(m.loadManager(), bar, lc);
            AnalysisEngineRegistry reg;
            registerBuiltInEngines(reg);
            AnalysisManager mgr(reg);
            AnalysisContext c;
            c.engineId = "opensees";
            c.common.includeSelfWeight = false;
            h.run = mgr.run(c, mgr.prepare(m, &gm, c));
            AnalysisParameters p;
            p.type = AnalysisType::LinearStatic;
            p.includeSelfWeight = false;
            h.script = OpenSeesAnalysisBuilder::buildScript(CalculationSnapshot::capture(m), p);
            return h;
        };
        auto barOf = [](const Hinged& h) -> const ElementResults& { return h.run.results.allElementResults().begin()->second; };
        auto atMid = [&](const ElementResults& r) {
            for (const auto& s : r.intermediateStations)
                if (std::abs(s.position - 0.5 * L) < 1e-9) return s;
            return StationForces {};
        };

        EndRelease none, hingeY, hingeZ;
        hingeY.my = true;
        hingeZ.mz = true;
        const double P = 30.0, a = 2.0, b = L - a;
        const double mFixed = P * a * b * (L + a) / (2.0 * L * L);            // encastrement en j, rotule en i
        const double rHinge = P * b * b * (3.0 * L - b) / (2.0 * L * L * L);  // réaction côté rotule

        auto pointZ = [&](auto& lm, int bar, int lc) { lm.addMemberLoad(MemberLoad::pointOnMember(bar, lc, -P, a, LoadDirection::GlobalZ, "P")); };
        const Hinged y = solve(hingeY, none, pointZ);
        TEST_CHECK(y.run.success, "Test 539: calcul, rotule My (" << y.run.message << ")");
        TEST_CHECK(std::abs(barOf(y).startForces.My) < 1e-9, "Test 539: moment nul à la rotule (" << barOf(y).startForces.My << ")");
        TEST_CHECK(approxEqual(std::abs(barOf(y).endForces.My), mFixed, 1e-9), "Test 539: moment d'encastrement Pab(L+a)/2L² (" << barOf(y).endForces.My << ", attendu " << mFixed << ")");
        TEST_CHECK(approxEqual(y.run.results.nodeReaction(y.i).rz, rHinge, 1e-9) && approxEqual(y.run.results.nodeReaction(y.j).rz, P - rHinge, 1e-9),
                   "Test 539: réactions (" << y.run.results.nodeReaction(y.i).rz << ", " << y.run.results.nodeReaction(y.j).rz << ")");
        TEST_CHECK(y.script.find("eleLoad") == std::string::npos && y.script.find("barre à rotule") != std::string::npos,
                   "Test 539: aucun eleLoad sur une barre à rotule, forces nodales condensées");

        const Hinged z = solve(hingeZ, none, [&](auto& lm, int bar, int lc) {
            lm.addMemberLoad(MemberLoad::pointOnMember(bar, lc, -P, a, LoadDirection::GlobalY, "P"));
        });
        TEST_CHECK(z.run.success, "Test 539: calcul, rotule Mz (" << z.run.message << ")");
        TEST_CHECK(std::abs(barOf(z).startForces.Mz) < 1e-9 && approxEqual(std::abs(barOf(z).endForces.Mz), mFixed, 1e-9),
                   "Test 539: plan x-y, moment nul à la rotule, encastrement (" << barOf(z).startForces.Mz << ", " << barOf(z).endForces.Mz << ")");
        TEST_CHECK(approxEqual(z.run.results.nodeReaction(z.i).ry, rHinge, 1e-9), "Test 539: plan x-y, réaction (" << z.run.results.nodeReaction(z.i).ry << ")");

        // Deux rotules, charge trapézoïdale à intensité constante sur toute la barre : M = qL²/8 à mi-portée, moments d'extrémité nuls.
        const double q = 12.0;
        const Hinged pin = solve(hingeY, hingeY, [&](auto& lm, int bar, int lc) {
            lm.addMemberLoad(MemberLoad::trapezoidal(bar, lc, -q, -q, 0.0, L, LoadDirection::GlobalZ, "q"));
        });
        TEST_CHECK(pin.run.success, "Test 539: calcul, deux rotules (" << pin.run.message << ")");
        TEST_CHECK(std::abs(barOf(pin).startForces.My) < 1e-9 && std::abs(barOf(pin).endForces.My) < 1e-9, "Test 539: deux rotules : moments d'extrémité nuls");
        TEST_CHECK(approxEqual(std::abs(atMid(barOf(pin)).My), q * L * L / 8.0, 1e-9), "Test 539: deux rotules : qL²/8 à mi-portée (" << atMid(barOf(pin)).My << ")");

        // Uniforme partielle sur [1, 3] m, rotule en j : M_i = ∫ q·x·(L−x)·(2L−x)/2L² dx, fixe à l'encastrement i.
        const double qp = 10.0, c1 = 1.0, c2 = 3.0;
        auto primitive = [&](double x) { return L * L * x * x - L * x * x * x + x * x * x * x / 4.0; };
        const double mPartial = qp * (primitive(c2) - primitive(c1)) / (2.0 * L * L);
        const Hinged partial = solve(none, hingeY, [&](auto& lm, int bar, int lc) {
            lm.addMemberLoad(MemberLoad::trapezoidal(bar, lc, -qp, -qp, c1, c2, LoadDirection::GlobalZ, "q partielle"));
        });
        TEST_CHECK(partial.run.success, "Test 539: calcul, charge partielle (" << partial.run.message << ")");
        TEST_CHECK(std::abs(barOf(partial).endForces.My) < 1e-9 && approxEqual(std::abs(barOf(partial).startForces.My), mPartial, 1e-9),
                   "Test 539: charge partielle, rotule en j (" << barOf(partial).startForces.My << ", attendu " << mPartial << ")");

        // Témoin sans rotule : chemin eleLoad d'origine, bi-encastrée.
        const Hinged plain = solve(none, none, pointZ);
        TEST_CHECK(plain.run.success && plain.script.find("-beamPoint") != std::string::npos && plain.script.find("barre à rotule") == std::string::npos,
                   "Test 539: barre sans rotule : eleLoad -beamPoint conservé");
        TEST_CHECK(approxEqual(std::abs(barOf(plain).startForces.My), P * a * b * b / (L * L), 1e-9) && approxEqual(std::abs(barOf(plain).endForces.My), P * a * a * b / (L * L), 1e-9),
                   "Test 539: barre sans rotule : Pab²/L², Pa²b/L²");
        std::cout << "[PASS] Test 539: OpenSees : barre à rotule sous charge ponctuelle, trapézoïdale ou partielle, deux plans" << std::endl;
        ++passed;
    }
#ifdef TSA_HAS_FEM3D
    // -------------------------------------------------------------------------
    // TESTS 476-480 : moteur EF 3D Fem3D (moteur secondaire, logique de secours explicite)
    // -------------------------------------------------------------------------
    {
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        TEST_CHECK(reg.hasEngine("fem3d") && reg.ids().front() == "opensees" && reg.ids().back() == "fem3d",
                   "Test 476: Fem3D enregistré après OpenSees, qui reste le moteur par défaut");
        const AnalysisEngine* fem = reg.engine("fem3d");
        const AnalysisCapabilities c = fem->capabilities();
        TEST_CHECK(c.supports3D && !c.supports2D && c.supportsFrame && c.supportsSprings && c.supportsStatic,
                   "Test 476: périmètre déclaré (3D, poutres, ressorts, statique)");
        TEST_CHECK(!c.supportsTruss && !c.supportsCable && !c.supportsShell && !c.supportsModal && !c.supportsMesh,
                   "Test 476: rien de plus que le périmètre qualifié");
        TEST_CHECK(fem->info().version == "0.3.0" && fem->availability().available, "Test 476: version réelle de la bibliothèque");
        TEST_CHECK(fem->defaultSettings().value("mode").toString() == "fem3d" && fem->defaultSettings().value("fallback").toString() == "none",
                   "Test 476: par défaut, Fem3D seul et aucun secours");
        std::cout << "[PASS] Test 476: Moteur Fem3D enregistré" << std::endl;
        ++passed;
    }
    {
        // Portiques A, B, C reliés par des poutres transversales : rotules, ressorts, charges nodales, réparties
        // uniformes et trapézoïdales sur toute la longueur, poids propre. Fem3D comparé à OpenSees (mode « compare »).
        Model m;
        GridManager gm;
        Frames f = buildFrames(m, true);
        m.getNode(f.nodes['C'][3])->setSupport(SupportDefinition::elastic(5.0e4, 4.0e4, 2.0e5, 3.0e3, 2.5e3, 2.0e3));
        EndRelease hinge;
        hinge.my = true;
        hinge.mz = true;
        m.getBeam(f.transverse[0])->setEndRelease(hinge);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G+Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['A'][1], lc, 12.0, -4.0, -30.0, 1.0, 0.0, 2.0));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['C'][2], lc, 0.0, 8.0, -20.0));
        for (char axis : { 'A', 'B', 'C' })
            m.loadManager().addMemberLoad(MemberLoad::uniform(f.members[axis][1], lc, -6.0, LoadDirection::GlobalZ, "q"));
        m.loadManager().addMemberLoad(MemberLoad::trapezoidal(f.members['B'][1], lc, -4.0, -14.0, 0.0, 1.0, LoadDirection::GlobalZ,
                                                              LoadCoordSystem::Global, true, "trapèze"));
        m.loadManager().addMemberLoad(MemberLoad::uniform(f.members['A'][0], lc, 3.0, LoadDirection::GlobalY, LoadCoordSystem::Global, "vent",
                                                          MemberTargetType::Column));

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "fem3d";
        c.loadCaseIds = { lc };
        c.common.includeSelfWeight = true;
        c.engineSettings["fem3d"] = QJsonObject { { "mode", "compare" }, { "fallback", "none" } };
        const auto prep = mgr.prepare(m, &gm, c);
        TEST_CHECK(prep.canRun(), "Test 477: modèle dans le périmètre de Fem3D");
        const auto run = mgr.run(c, prep);
        TEST_CHECK(run.success, "Test 477: calcul Fem3D (" << run.message << ")");
        TEST_CHECK(run.results.executionMetadata().engineId == "fem3d" && run.results.executionMetadata().solverEngine == "Fem3D",
                   "Test 477: résultats étiquetés Fem3D");
        TEST_CHECK(run.message.find(" 0 hors tolérance") != std::string::npos, "Test 477: Fem3D = OpenSees (" << run.message << ")");
        const auto& tables = run.results.engineTables();
        TEST_CHECK(tables.size() == 1 && tables[0].rows.size() == 4 && tables[0].rows.back()[2] == "0",
                   "Test 477: table de comparaison (déplacements, réactions, efforts) sans écart");
        TEST_CHECK(run.results.allReactions().count(f.nodes['C'][3]) == 1, "Test 477: réaction du ressort reportée sur son nœud");

        // Stations : même reconstruction pour les deux moteurs (charge trapézoïdale comprise)
        AnalysisContext os = c;
        os.engineId = "opensees";
        const auto osRun = mgr.run(os, mgr.prepare(m, &gm, os));
        TEST_CHECK(osRun.success, "Test 477: calcul OpenSees de référence");
        double worst = 0.0, scale = 0.0;
        for (const auto& [key, e] : osRun.results.allElementResults())
        {
            const ElementResults* a = run.results.getElementResults(key);
            TEST_CHECK(a && a->intermediateStations.size() == e.intermediateStations.size(), "Test 477: mêmes stations");
            for (std::size_t i = 0; i < e.intermediateStations.size(); ++i)
            {
                const auto& x = a->intermediateStations[i];
                const auto& y = e.intermediateStations[i];
                for (const auto& [u, v] : { std::pair{ x.My, y.My }, std::pair{ x.Mz, y.Mz }, std::pair{ x.Vz, y.Vz }, std::pair{ x.N, y.N } })
                {
                    worst = std::max(worst, std::abs(u - v));
                    scale = std::max(scale, std::abs(v));
                }
            }
        }
        TEST_CHECK(scale > 1.0 && worst <= 1e-6 * scale, "Test 477: efforts aux stations identiques (écart " << worst << " / " << scale << ")");
        std::cout << "[PASS] Test 477: Fem3D comparé à OpenSees sur un portique spatial TSA" << std::endl;
        ++passed;

        // TEST 499 : comparaison Fem3D / OpenSees quand OpenSees calcule en N (option « kN, m, kPa » décochée) — Fem3D
        // calcule toujours en kN : la comparaison ramène les deux côtés à la même unité (BUG-117, R8).
        AnalysisContext cN = c;
        cN.engineSettings["opensees"] = QJsonObject { { "useKiloNewtons", false } };
        const auto runN = mgr.run(cN, mgr.prepare(m, &gm, cN));
        TEST_CHECK(runN.success, "Test 499: calcul Fem3D, comparaison OpenSees en N (" << runN.message << ")");
        TEST_CHECK(runN.message.find(" 0 hors tolérance") != std::string::npos, "Test 499: Fem3D (kN) = OpenSees (N) (" << runN.message << ")");
        TEST_CHECK(runN.results.units().force == "kN", "Test 499: résultats affichés = Fem3D, en kN");
        AnalysisContext osN = os;
        osN.engineSettings["opensees"] = QJsonObject { { "useKiloNewtons", false } };
        const auto osRunN = mgr.run(osN, mgr.prepare(m, &gm, osN));
        TEST_CHECK(osRunN.success && osRunN.results.units().force == "N", "Test 499: OpenSees seul, en N");
        TSA::Analysis::ResultsModel fem = run.results;
        const auto cmpKN = compareResults(fem, osRun.results), cmpN = compareResults(fem, osRunN.results);
        TEST_CHECK(cmpKN.compared > 0 && cmpKN.failed == 0 && cmpN.compared == cmpKN.compared && cmpN.failed == 0,
                   "Test 499: compareResults indépendant de l'unité d'OpenSees (" << cmpN.failed << " écarts en N)");
        std::cout << "[PASS] Test 499: Comparaison Fem3D / OpenSees en kN et en N" << std::endl;
        ++passed;

        // TEST 504 : métadonnées des vrais moteurs (BUG-111, étape 0) — version lue sur le moteur, aucune annexe nationale
        // attribuée au calcul, configuration réelle du script OpenSees, messages de validation de la préparation repris.
        std::size_t prepMessages = 0;
        for (const auto& msg : prep.validation.messages()) prepMessages += msg.severity != ValidationSeverity::Error ? 1 : 0;
        const auto& fm = run.results.executionMetadata();
        const auto& om = osRun.results.executionMetadata();
        const std::string installed = TSA::Analysis::OpenSeesManager::instance().versionInfo().versionString.toStdString();
        TEST_CHECK(fm.solverEngine == "Fem3D" && !fm.solverVersion.empty() && fm.nationalAnnex.empty() && fm.normativeFramework.empty(),
                   "Test 504: Fem3D : version du moteur, aucune annexe ni référentiel attribués (" << fm.solverVersion << ")");
        TEST_CHECK(om.solverEngine == "OpenSees" && om.solverVersion == installed && om.nationalAnnex.empty()
                       && om.modelBuilder == "BasicBuilder -ndm 3 -ndf 6" && om.numberer == "RCM",
                   "Test 504: OpenSees : version lue (" << om.solverVersion << "), configuration du script, aucune annexe");
        TEST_CHECK(fm.preparationMessages.size() == prepMessages,
                   "Test 504: messages de la validation avant calcul repris dans les résultats (" << prepMessages << ")");
        std::cout << "[PASS] Test 504: Métadonnées des moteurs Fem3D et OpenSees sans valeur inventée" << std::endl;
        ++passed;
    }
    {
        // Hors périmètre de Fem3D (barre de treillis) : refus motivé, ou secours OpenSees choisi et étiqueté.
        // Depuis Fem3D 0.3.0, les charges ponctuelles et partielles et les moments répartis sur barre sont calculés
        // (test 530) : ils ne déclenchent plus ce chemin. Les treillis et les coques sont déjà refusés pour Fem3D par
        // les contrôles communs d'AnalysisManager (familles d'éléments), avant la décision propre au moteur : celle-ci est
        // donc vérifiée directement sur le moteur, avec le modèle préparé pour OpenSees (qui calcule les treillis).
        Model m;
        GridManager gm;
        Frames f = buildFrames(m, false);
        m.addTrussMember(f.nodes['A'][0], f.nodes['A'][2], 0.002);   // diagonale articulée dans l'axe A
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "P", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, f.nodes['A'][1], lc, 10.0, 0.0, -25.0));
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "opensees";
        c.loadCaseIds = { lc };
        c.common.includeSelfWeight = false;
        const auto prepared = mgr.prepare(m, &gm, c);
        TEST_CHECK(prepared.canRun(), "Test 478: modèle avec treillis préparé");
        AnalysisEngine* fem = reg.engine("fem3d");
        TEST_CHECK(fem != nullptr, "Test 478: moteur Fem3D enregistré");
        c.engineId = "fem3d";

        const ValidationResult refused = fem->validate(c, prepared.model);
        const auto errors = refused.texts(ValidationSeverity::Error);
        TEST_CHECK(!refused.isValid() && std::any_of(errors.begin(), errors.end(), [](const std::string& e) {
                       return e.find("Hors du périmètre de Fem3D") != std::string::npos && e.find("treillis") != std::string::npos;
                   }),
                   "Test 478: refus explicite, raison donnée");
        TEST_CHECK(!fem->run(c, prepared.model, {}).success, "Test 478: aucun calcul sans secours choisi");

        c.engineSettings["fem3d"] = QJsonObject { { "mode", "fem3d" }, { "fallback", "opensees" } };
        const ValidationResult withFallback = fem->validate(c, prepared.model);
        const auto warnings = withFallback.texts(ValidationSeverity::Warning);
        TEST_CHECK(withFallback.isValid() && std::any_of(warnings.begin(), warnings.end(), [](const std::string& w) {
                       return w.find("Calcul confié à OpenSees") != std::string::npos;
                   }),
                   "Test 478: secours annoncé avant le calcul");
        const auto run = fem->run(c, prepared.model, {});
        TEST_CHECK(run.success && run.producedBy && run.producedBy->id == "opensees" && run.results.executionMetadata().solverEngine == "OpenSees",
                   "Test 478: résultats étiquetés OpenSees, jamais Fem3D (" << (run.producedBy ? run.producedBy->id : std::string("?")) << ")");
        const auto& w = run.results.executionMetadata().warnings;
        TEST_CHECK(std::any_of(w.begin(), w.end(), [](const std::string& x) { return x.find("secours de Fem3D") != std::string::npos; }),
                   "Test 478: raison du secours dans la traçabilité");
        std::cout << "[PASS] Test 478: Hors périmètre : refus ou secours OpenSees explicite" << std::endl;
        ++passed;
    }
    {
        // TEST 530 : charges sur barre calculées par Fem3D (0.3.0) — forces ponctuelles (repères global et local, sur poutre
        // et sur poteau), charges uniformes et trapézoïdales sur une partie de la barre, moments répartis (torsion et
        // flexion) ; comparaison automatique à OpenSees (mode « compare ») et efforts aux stations identiques.
        Model m;
        GridManager gm;
        Frames f = buildFrames(m, true);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        auto& lm = m.loadManager();
        lm.addMemberLoad(MemberLoad::pointOnMember(f.members['A'][1], lc, -25.0, 2.0, LoadDirection::GlobalZ, "P"));
        lm.addMemberLoad(MemberLoad::pointOnMember(f.members['B'][1], lc, 8.0, 0.35, LoadDirection::LocalY, LoadCoordSystem::Local, true, "P local"));
        lm.addMemberLoad(MemberLoad(0, f.members['C'][0], lc, LoadType::MemberPoint, 6.0, 0.0, LoadDirection::GlobalX, LoadCoordSystem::Global, 1.2, 0.0,
                                    false, "choc", MemberTargetType::Column));
        lm.addMemberLoad(MemberLoad::trapezoidal(f.members['A'][1], lc, -9.0, -9.0, 1.0, 4.5, LoadDirection::GlobalZ, "uniforme partielle"));
        lm.addMemberLoad(MemberLoad::trapezoidal(f.members['C'][1], lc, -3.0, -12.0, 0.2, 0.7, LoadDirection::GlobalZ, LoadCoordSystem::Global, true,
                                                 "trapèze partiel"));
        lm.addMemberLoad(MemberLoad(0, f.transverse[0], lc, LoadType::MemberMoment, 2.0, 2.0, LoadDirection::LocalX,
                                    MemberLoad::coordSystemFor(LoadDirection::LocalX), 0.0, 0.0));   // torsion répartie
        lm.addMemberLoad(MemberLoad(0, f.members['B'][1], lc, LoadType::MemberMoment, 1.5, 4.0, LoadDirection::LocalZ,
                                    MemberLoad::coordSystemFor(LoadDirection::LocalZ), 1.0, 5.0));   // couple de flexion partiel

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "fem3d";
        c.loadCaseIds = { lc };
        c.common.includeSelfWeight = false;
        c.engineSettings["fem3d"] = QJsonObject { { "mode", "compare" }, { "fallback", "none" } };
        const auto prep = mgr.prepare(m, &gm, c);
        const auto errors = prep.validation.texts(ValidationSeverity::Error);
        TEST_CHECK(prep.canRun(), "Test 530: charges sur barre dans le périmètre de Fem3D (" << (errors.empty() ? std::string() : errors.front()) << ")");
        const auto run = mgr.run(c, prep);
        TEST_CHECK(run.success && run.results.executionMetadata().solverEngine == "Fem3D", "Test 530: calcul Fem3D (" << run.message << ")");
        TEST_CHECK(run.message.find(" 0 hors tolérance") != std::string::npos, "Test 530: Fem3D = OpenSees (" << run.message << ")");

        AnalysisContext os = c;
        os.engineId = "opensees";
        const auto osRun = mgr.run(os, mgr.prepare(m, &gm, os));
        TEST_CHECK(osRun.success, "Test 530: calcul OpenSees de référence");
        double worst = 0.0, scale = 0.0;
        for (const auto& [key, e] : osRun.results.allElementResults())
        {
            const ElementResults* a = run.results.getElementResults(key);
            TEST_CHECK(a && a->intermediateStations.size() == e.intermediateStations.size(), "Test 530: mêmes stations");
            for (std::size_t i = 0; i < e.intermediateStations.size(); ++i)
            {
                const auto& x = a->intermediateStations[i];
                const auto& y = e.intermediateStations[i];
                for (const auto& [u, v] : { std::pair{ x.My, y.My }, std::pair{ x.Mz, y.Mz }, std::pair{ x.Vy, y.Vy }, std::pair{ x.Vz, y.Vz },
                                            std::pair{ x.N, y.N }, std::pair{ x.Mx, y.Mx } })
                {
                    worst = std::max(worst, std::abs(u - v));
                    scale = std::max(scale, std::abs(v));
                }
            }
        }
        TEST_CHECK(scale > 1.0 && worst <= 1e-6 * scale, "Test 530: efforts aux stations identiques (écart " << worst << " / " << scale << ")");
        std::cout << "[PASS] Test 530: Fem3D calcule les charges ponctuelles, partielles et les moments répartis (= OpenSees)" << std::endl;
        ++passed;
    }
    {
        // TEST 540 : BUG-148 — portique spatial à rotules (deux plans, une ou deux extrémités) sous toutes les familles de
        // charges, moments répartis et poids propre compris : OpenSees = Fem3D (condensation indépendante), déplacements,
        // réactions et efforts aux stations ; moment nul à chaque rotule.
        Model m;
        GridManager gm;
        Frames f = buildFrames(m, true);
        EndRelease hingeY, hingeZ, hingeYZ;
        hingeY.my = true;
        hingeZ.mz = true;
        hingeYZ.my = hingeYZ.mz = true;
        m.getBeam(f.members['A'][1])->setStartRelease(hingeY);
        m.getBeam(f.members['B'][1])->setEndRelease(hingeZ);
        m.getBeam(f.members['C'][1])->setStartRelease(hingeY);
        m.getBeam(f.members['C'][1])->setEndRelease(hingeY);
        m.getBeam(f.transverse[0])->setEndRelease(hingeYZ);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        auto& lm = m.loadManager();
        lm.addMemberLoad(MemberLoad::pointOnMember(f.members['A'][1], lc, -25.0, 2.0, LoadDirection::GlobalZ, "P"));
        lm.addMemberLoad(MemberLoad::pointOnMember(f.members['B'][1], lc, 8.0, 0.35, LoadDirection::LocalY, LoadCoordSystem::Local, true, "P local"));
        lm.addMemberLoad(MemberLoad::trapezoidal(f.members['A'][1], lc, -9.0, -9.0, 1.0, 4.5, LoadDirection::GlobalZ, "uniforme partielle"));
        lm.addMemberLoad(MemberLoad::trapezoidal(f.members['C'][1], lc, -3.0, -12.0, 0.2, 0.7, LoadDirection::GlobalZ, LoadCoordSystem::Global, true,
                                                 "trapèze partiel"));
        lm.addMemberLoad(MemberLoad::uniform(0, f.transverse[0], lc, 6.0));
        lm.addMemberLoad(MemberLoad(0, f.transverse[0], lc, LoadType::MemberMoment, 2.0, 2.0, LoadDirection::LocalX,
                                    MemberLoad::coordSystemFor(LoadDirection::LocalX), 0.0, 0.0));   // torsion répartie
        lm.addMemberLoad(MemberLoad(0, f.members['B'][1], lc, LoadType::MemberMoment, 1.5, 4.0, LoadDirection::LocalZ,
                                    MemberLoad::coordSystemFor(LoadDirection::LocalZ), 1.0, 5.0));   // couple de flexion partiel, plan de la rotule
        lm.addMemberLoad(MemberLoad(0, f.members['A'][1], lc, LoadType::MemberMoment, 2.5, 1.0, LoadDirection::LocalY,
                                    MemberLoad::coordSystemFor(LoadDirection::LocalY), 0.5, 3.5));   // couple de flexion partiel, plan de la rotule

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "fem3d";
        c.loadCaseIds = { lc };
        c.common.includeSelfWeight = true;
        c.engineSettings["fem3d"] = QJsonObject { { "mode", "compare" }, { "fallback", "none" } };
        const auto prep = mgr.prepare(m, &gm, c);
        const auto errors = prep.validation.texts(ValidationSeverity::Error);
        TEST_CHECK(prep.canRun(), "Test 540: préparation (" << (errors.empty() ? std::string() : errors.front()) << ")");
        const auto run = mgr.run(c, prep);
        TEST_CHECK(run.success && run.results.executionMetadata().solverEngine == "Fem3D", "Test 540: calcul Fem3D (" << run.message << ")");
        TEST_CHECK(run.message.find(" 0 hors tolérance") != std::string::npos, "Test 540: Fem3D = OpenSees (" << run.message << ")");

        AnalysisContext os = c;
        os.engineId = "opensees";
        const auto osRun = mgr.run(os, mgr.prepare(m, &gm, os));
        TEST_CHECK(osRun.success, "Test 540: calcul OpenSees (" << osRun.message << ")");
        double worst = 0.0, scale = 0.0;
        for (const auto& [key, e] : osRun.results.allElementResults())
        {
            const ElementResults* a = run.results.getElementResults(key);
            TEST_CHECK(a && a->intermediateStations.size() == e.intermediateStations.size(), "Test 540: mêmes stations");
            for (std::size_t i = 0; i < e.intermediateStations.size(); ++i)
            {
                const auto& x = a->intermediateStations[i];
                const auto& y = e.intermediateStations[i];
                for (const auto& [u, v] : { std::pair { x.My, y.My }, std::pair { x.Mz, y.Mz }, std::pair { x.Vy, y.Vy }, std::pair { x.Vz, y.Vz },
                                            std::pair { x.N, y.N }, std::pair { x.Mx, y.Mx } })
                {
                    worst = std::max(worst, std::abs(u - v));
                    scale = std::max(scale, std::abs(v));
                }
            }
        }
        TEST_CHECK(scale > 1.0 && worst <= 1e-6 * scale, "Test 540: efforts aux stations identiques (écart " << worst << " / " << scale << ")");

        // Moment nul à chaque rotule, côté OpenSees (ce qu'aucun contrôle d'équilibre ne voit).
        int hinges = 0;
        for (const auto& [key, e] : osRun.results.allElementResults())
        {
            if (e.kind != StructuralElementKind::Beam) continue;
            const auto* beam = m.getBeam(e.elementId);
            if (!beam) continue;
            const auto si = beam->startRelease(), sj = beam->endRelease();
            TEST_CHECK((!si.my || std::abs(e.startForces.My) < 1e-9) && (!si.mz || std::abs(e.startForces.Mz) < 1e-9) &&
                           (!sj.my || std::abs(e.endForces.My) < 1e-9) && (!sj.mz || std::abs(e.endForces.Mz) < 1e-9),
                       "Test 540: moment nul à la rotule de la barre " << e.elementId);
            hinges += si.my + si.mz + sj.my + sj.mz;
        }
        TEST_CHECK(hinges == 6, "Test 540: 6 rotules contrôlées (" << hinges << ")");
        std::cout << "[PASS] Test 540: Portique à rotules, toutes charges : OpenSees = Fem3D, moment nul aux rotules" << std::endl;
        ++passed;
    }
    {
        // Mécanisme (poteau à pied articulé dans toutes les rotations) : refus, et jamais de secours. Le modèle complet
        // est arrêté plus tôt par le contrôle de stabilité de TSA (ModelValidator) ; une SÉLECTION qui forme un mécanisme
        // n'y passe pas et arrive au moteur.
        Model m;
        GridManager gm;
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), d = m.addNode(4, 0, 0), e = m.addNode(4, 0, 3);
        m.getNode(a)->setSupport(SupportDefinition::custom(DOFState::Fixed, DOFState::Fixed, DOFState::Fixed, DOFState::Free, DOFState::Free, DOFState::Free));
        m.getNode(d)->setSupport(SupportDefinition::fixed());
        const int column = m.addColumn(a, b, Section::heb(200), Material::steelS235());
        m.addColumn(d, e, Section::heb(200), Material::steelS235());
        m.addBar(b, e, Section::ipe(300), Material::steelS235(), BarRole::Beam);   // le modèle complet est stable
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "H", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 5.0, 0.0, 0.0));
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "fem3d";
        c.loadCaseIds = { lc };
        c.common.includeSelfWeight = false;
        c.common.supportRestraints = SupportRestraintMode::None;
        c.scope.type = ScopeType::SelectedElements;
        c.scope.columns = { column };
        c.engineSettings["fem3d"] = QJsonObject { { "mode", "fem3d" }, { "fallback", "opensees" } };
        const auto prep = mgr.prepare(m, &gm, c);
        const auto run = prep.canRun() ? mgr.run(c, prep) : AnalysisRunResult {};
        std::string prepErrors;
        for (const auto& e : prep.validation.texts(ValidationSeverity::Error)) prepErrors += " | " + e;
        TEST_CHECK(prep.canRun() && !run.success && !run.results.hasResults(),
                   "Test 479: mécanisme refusé, aucun résultat (préparation" << prepErrors << " ; calcul : " << run.success << " " << run.message << ")");
        TEST_CHECK(run.message.find("mécanisme") != std::string::npos && run.message.find("Aucun moteur de secours") != std::string::npos,
                   "Test 479: diagnostic du mécanisme, pas de secours (" << run.message << ")");
        std::cout << "[PASS] Test 479: Mécanisme : diagnostic, jamais de secours" << std::endl;
        ++passed;
    }
    {
        TSA::UI::AnalysisEngineOptionsRegistry options;
        TSA::UI::registerBuiltInEngineOptions(options);
        TEST_CHECK(options.hasOptions("fem3d"), "Test 480: panneau d'options Fem3D");
        std::unique_ptr<TSA::UI::AnalysisEngineOptionsWidget> panel(options.create("fem3d", nullptr));
        panel->loadSettings(QJsonObject { { "mode", "compare" }, { "fallback", "opensees" } });
        const QJsonObject back = panel->saveSettings();
        TEST_CHECK(back.value("mode").toString() == "compare" && back.value("fallback").toString() == "opensees", "Test 480: réglages relus");
        panel->loadSettings({});
        TEST_CHECK(panel->saveSettings() == Fem3DEngine::settingsToJson({}), "Test 480: valeurs par défaut du moteur");
        std::cout << "[PASS] Test 480: Options Fem3D" << std::endl;
        ++passed;
    }
#endif

    return true;
}
