// Suite « mdd » : moteur 2D MetDeDeplacement (tests 150-158).
// Chaque résultat est comparé à une formule de RDM fermée (poutres de référence) ou à OpenSees.

#include "test_common.h"

#include <mdd/MetDeDeplacement.h>

#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/OpenSeesSolver.h"
#include "Model/Load/LoadManager.h"
#include "NDC/NDCGenerator.h"
#include "NDC/NDCPlanarCurves.h"

#include <chrono>

namespace
{
constexpr double E = 210e6;     // kPa
constexpr double I = 1e-4;      // m⁴
constexpr double A = 1e-2;      // m²
constexpr double EI = E * I;

bool rel(double a, double b, double tol = 1e-9)
{
    return std::abs(a - b) <= tol * std::max({ std::abs(a), std::abs(b), 1e-12 });
}

/// Poutre horizontale 0 → L, une barre.
mdd::Model beam(double L, bool fixI, bool rotI, bool fixJ, bool rotJ, bool xJ = false)
{
    mdd::Model m;
    m.nodes.push_back({ 0, 0, fixI, fixI, rotI });
    m.nodes.push_back({ L, 0, xJ, fixJ, rotJ });
    m.members.push_back({ 0, 1, E, A, I });
    return m;
}
} // namespace

bool runSuite_MetDeDeplacement(int& passed)
{
    const double q = 10.0, L = 6.0;

    // TEST 150 : poutre sur deux appuis, charge uniforme (qL²/8, 5qL⁴/384EI, qL/2, qL³/24EI)
    {
        auto m = beam(L, true, false, true, false);
        m.distributedLoads.push_back({ 0, 0, L, 0, -q, 0, -q });
        const auto r = mdd::solve(m);
        TEST_CHECK(r.success, "Test 150: calcul");
        const auto& s = r.members[0].summary;
        TEST_CHECK(s.hasSpanExtremum && rel(s.MSpanExtremum, q * L * L / 8) && rel(s.xSpanExtremum, L / 2, 1e-9),
                   "Test 150: M max = qL²/8 à mi-portée");
        TEST_CHECK(std::abs(s.Mi) < 1e-9 && std::abs(s.Mj) < 1e-9 && s.momentZeros.empty(), "Test 150: M nuls aux appuis");
        TEST_CHECK(rel(s.deflectionMax, -5 * q * std::pow(L, 4) / (384 * EI), 1e-9) && rel(s.xDeflectionMax, L / 2, 1e-6),
                   "Test 150: flèche 5qL⁴/384EI à mi-portée");
        TEST_CHECK(rel(r.reactions[0][1], q * L / 2) && rel(r.reactions[1][1], q * L / 2), "Test 150: réactions qL/2");
        TEST_CHECK(rel(s.Vi, q * L / 2) && rel(s.Vj, -q * L / 2), "Test 150: V = dM/dx = ±qL/2");
        TEST_CHECK(rel(r.displacements[0][2], -q * std::pow(L, 3) / (24 * EI), 1e-9) && rel(s.rotationI, r.displacements[0][2], 1e-9),
                   "Test 150: rotation d'appui qL³/24EI (nœud et section)");
        TEST_CHECK(r.equilibriumResidual < 1e-9, "Test 150: équilibre global");
        std::cout << "[PASS] Test 150: Poutre isostatique — formules RDM" << std::endl;
        ++passed;
    }

    // TEST 151 : poutre bi-encastrée (qL²/12, qL²/24, qL⁴/384EI, zéros de M)
    {
        auto m = beam(L, true, true, true, true);
        m.distributedLoads.push_back({ 0, 0, L, 0, -q, 0, -q });
        const auto r = mdd::solve(m);
        const auto& s = r.members[0].summary;
        TEST_CHECK(r.success && rel(s.Mi, -q * L * L / 12) && rel(s.Mj, -q * L * L / 12), "Test 151: M appuis = -qL²/12");
        TEST_CHECK(rel(s.MSpanExtremum, q * L * L / 24), "Test 151: M travée = qL²/24");
        TEST_CHECK(rel(s.deflectionMax, -q * std::pow(L, 4) / (384 * EI), 1e-9), "Test 151: flèche qL⁴/384EI");
        TEST_CHECK(s.momentZeros.size() == 2 && rel(s.momentZeros[0], L / 2 - L / (2 * std::sqrt(3.0)), 1e-9),
                   "Test 151: zéros de M en L/2 ± L/(2√3)");
        TEST_CHECK(rel(r.reactions[0][2], q * L * L / 12), "Test 151: moment d'encastrement (réaction)");
        std::cout << "[PASS] Test 151: Poutre bi-encastrée" << std::endl;
        ++passed;
    }

    // TEST 152 : encastrée-appuyée et rotule d'extrémité (qL²/8, 3qL/8, 9qL²/128 en 5L/8)
    {
        auto m = beam(L, true, true, true, false);
        m.distributedLoads.push_back({ 0, 0, L, 0, -q, 0, -q });
        const auto r = mdd::solve(m);
        const auto& s = r.members[0].summary;
        TEST_CHECK(r.success && rel(s.Mi, -q * L * L / 8) && rel(r.reactions[1][1], 3 * q * L / 8), "Test 152: qL²/8 et 3qL/8");
        TEST_CHECK(rel(s.MSpanExtremum, 9 * q * L * L / 128) && rel(s.xSpanExtremum, 5 * L / 8, 1e-9), "Test 152: 9qL²/128 en 5L/8");

        auto h = beam(L, true, true, true, true);   // encastré aux deux nœuds, rotule de barre en j
        h.members[0].releaseJ = true;
        h.distributedLoads = m.distributedLoads;
        const auto rh = mdd::solve(h);
        TEST_CHECK(rh.success && rel(rh.members[0].summary.Mi, -q * L * L / 8) && std::abs(rh.members[0].summary.Mj) < 1e-9,
                   "Test 152: rotule (condensation) = encastrée-appuyée, et non qL²/12 comme l'ancien code");
        TEST_CHECK(std::abs(rh.reactions[1][2]) < 1e-9, "Test 152: aucun moment transmis par la rotule");
        std::cout << "[PASS] Test 152: Rotules et appuis simples" << std::endl;
        ++passed;
    }

    // TEST 153 : console, force en bout de barre (PL³/3EI, -PL) ; force nodale équivalente
    {
        const double P = 5.0, l = 3.0;
        auto m = beam(l, true, true, false, false);
        m.pointLoads.push_back({ 0, l, 0, -P });
        const auto r = mdd::solve(m);
        TEST_CHECK(r.success && rel(r.displacements[1][1], -P * l * l * l / (3 * EI), 1e-9), "Test 153: flèche PL³/3EI");
        TEST_CHECK(rel(r.members[0].summary.Mi, -P * l) && rel(r.reactions[0][2], P * l), "Test 153: M d'encastrement PL");
        auto n = beam(l, true, true, false, false);
        n.nodalLoads.push_back({ 1, 0, -P, 0 });
        const auto rn = mdd::solve(n);
        TEST_CHECK(rel(rn.displacements[1][1], r.displacements[1][1], 1e-12), "Test 153: force en bout de barre = force nodale");
        auto t = beam(l, true, true, false, false);   // trapézoïdale partielle : contrôle par superposition
        t.distributedLoads.push_back({ 0, 1.0, 2.5, 0, -4, 0, -1 });
        const auto rt = mdd::solve(t);
        const double F = (4 + 1) / 2.0 * 1.5, xg = 1.0 + 1.5 * (4 + 2 * 1) / (3 * (4 + 1));
        TEST_CHECK(rel(rt.reactions[0][1], F) && rel(rt.reactions[0][2], F * xg), "Test 153: charge trapézoïdale partielle (résultante, moment)");
        std::cout << "[PASS] Test 153: Console et charges ponctuelles / trapézoïdales" << std::endl;
        ++passed;
    }

    // TEST 154 : barres articulées (treillis), rotation sans rigidité éliminée, efforts normaux
    {
        mdd::Model m;   // triangle 4 × 3 : appui double à gauche, appui simple à droite, charge au sommet
        m.nodes.push_back({ 0, 0, true, true, false });
        m.nodes.push_back({ 4, 0, false, true, false });
        m.nodes.push_back({ 2, 3, false, false, false });
        for (auto [i, j] : { std::pair { 0, 1 }, { 0, 2 }, { 1, 2 } }) m.members.push_back({ i, j, E, A, I, true, true });
        m.nodalLoads.push_back({ 2, 0, -26, 0 });
        const auto r = mdd::solve(m);
        TEST_CHECK(r.success, "Test 154: treillis calculé");
        const double Ldiag = std::hypot(2.0, 3.0);
        TEST_CHECK(rel(r.members[1].summary.Nmin, -13 * Ldiag / 3) && rel(r.members[0].summary.Nmax, 13 * 2 / 3.0),
                   "Test 154: diagonales comprimées 13·L/3, membrure tendue 26/3");
        int eliminated = 0;
        for (const auto& line : r.log) eliminated += line.find("rotation sans rigidit") != std::string::npos;
        TEST_CHECK(eliminated == 3, "Test 154: rotations sans rigidité éliminées et signalées");
        std::cout << "[PASS] Test 154: Treillis (barres articulées)" << std::endl;
        ++passed;
    }

    // TEST 155 : mécanisme détecté, données invalides refusées
    {
        auto m = beam(L, false, false, true, false);   // aucun blocage horizontal
        m.nodes[0].fixY = true;
        m.nodalLoads.push_back({ 1, 1, 0, 0 });
        const auto r = mdd::solve(m);
        TEST_CHECK(!r.success && r.message.find("instable") != std::string::npos, "Test 155: mécanisme signalé");
        auto z = beam(L, true, true, true, true);
        z.members[0].E = 0;
        TEST_CHECK(!mdd::solve(z).success, "Test 155: E nul refusé");
        TEST_CHECK(!mdd::solve(mdd::Model {}).success, "Test 155: modèle vide refusé");
        std::cout << "[PASS] Test 155: Contrôles d'entrée" << std::endl;
        ++passed;
    }

    // TEST 156 : barres inextensibles (hypothèse de la méthode des rotations)
    {
        mdd::Model m;   // poteau console comprimé
        m.nodes.push_back({ 0, 0, true, true, true });
        m.nodes.push_back({ 0, 4, false, false, false });
        m.members.push_back({ 0, 1, E, A, I });
        m.nodalLoads.push_back({ 1, 0, -100, 0 });
        const double exact = -100 * 4 / (E * A);
        const auto r1 = mdd::solve(m);
        m.options.axialStiffnessFactor = 1e4;
        const auto r2 = mdd::solve(m);
        TEST_CHECK(rel(r1.displacements[1][1], exact, 1e-9) && rel(r2.displacements[1][1], exact / 1e4, 1e-6),
                   "Test 156: raccourcissement réel, puis divisé par 10⁴");
        TEST_CHECK(rel(r1.members[0].summary.Nmin, -100) && rel(r2.members[0].summary.Nmin, -100), "Test 156: N inchangé");
        TEST_CHECK(rel(r1.members[0].curve.back().u, r1.displacements[1][1], 1e-9), "Test 156: u(L) = déplacement du nœud");
        std::cout << "[PASS] Test 156: Option barres inextensibles" << std::endl;
        ++passed;
    }

    // TEST 157 : performance (ossature 12 travées × 15 niveaux) et largeur de bande
    {
        mdd::Model m;
        const int nx = 13, ny = 16;
        auto id = [&](int ix, int iy) { return iy * nx + ix; };
        for (int iy = 0; iy < ny; ++iy)
            for (int ix = 0; ix < nx; ++ix)
                m.nodes.push_back({ 5.0 * ix, 3.0 * iy, iy == 0, iy == 0, iy == 0 });
        for (int iy = 0; iy < ny; ++iy)
            for (int ix = 0; ix < nx; ++ix)
            {
                if (iy > 0) m.members.push_back({ id(ix, iy - 1), id(ix, iy), E, A, I });
                if (ix > 0 && iy > 0)
                {
                    m.members.push_back({ id(ix - 1, iy), id(ix, iy), E, A, I });
                    m.distributedLoads.push_back({ static_cast<int>(m.members.size()) - 1, 0, 5, 0, -20, 0, -20 });
                }
            }
        for (int iy = 1; iy < ny; ++iy) m.nodalLoads.push_back({ id(0, iy), 10, 0, 0 });
        const auto t0 = std::chrono::steady_clock::now();
        const auto r = mdd::solve(m);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        TEST_CHECK(r.success && r.equilibriumResidual < 1e-6, "Test 157: grande ossature, équilibre");
        TEST_CHECK(r.halfBandwidth < r.equations / 4, "Test 157: renumérotation : bande étroite");
        std::cout << "  " << m.members.size() << " barres, " << r.equations << " équations, demi-bande " << r.halfBandwidth
                  << ", " << ms << " ms (courbes comprises)" << std::endl;
        TEST_CHECK(ms < 5000.0, "Test 157: temps de calcul raisonnable");
        std::cout << "[PASS] Test 157: Performance" << std::endl;
        ++passed;
    }

    // TEST 158 : TSA — Custom2D (MetDeDeplacement) = OpenSees sur le portique de l'axe B
    {
        using namespace TSA::Analysis;
        Model model;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0, 6 });
        def.setYPositions({ 0, 5 });
        const std::string gid = gm.addGrid(def)->id();
        int nodesB[4] = {};
        for (int k = 0; k < 2; ++k)
        {
            const double y = 5.0 * k;
            const int a = model.addNode(0, y, 0), b = model.addNode(0, y, 3), c = model.addNode(6, y, 3), d = model.addNode(6, y, 0);
            model.getNode(a)->setSupport(SupportDefinition::fixed());
            model.getNode(d)->setSupport(SupportDefinition::fixed());
            model.addColumn(a, b, Section::heb(200), Material::steelS235());
            const int bm = model.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
            model.addColumn(d, c, Section::heb(200), Material::steelS235());
            if (k == 1)
            {
                nodesB[0] = a; nodesB[1] = b; nodesB[2] = c; nodesB[3] = d;
                const int lc = model.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
                model.loadManager().addNodalLoad(NodalLoad(0, b, lc, 15.0, 0.0, -20.0));
                model.loadManager().addMemberLoad(MemberLoad::uniform(bm, lc, 12.0));
            }
        }
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.common.includeSelfWeight = false;
        c.scope.type = ScopeType::GridAxis;
        c.scope.gridId = gid;
        c.scope.axisFamily = GridAxisFamily::Y;
        c.scope.axisLabel = "B";

        c.engineId = "custom2d";
        c.dimension = AnalysisDimension::Plane2D;
        const auto p2 = mgr.prepare(model, &gm, c);
        const auto r2 = mgr.run(c, p2);
        TEST_CHECK(r2.success, "Test 158: Custom2D (MetDeDeplacement) disponible et calculé");

        c.engineId = "opensees";
        c.dimension = AnalysisDimension::Space3D;
        const auto p3 = mgr.prepare(model, &gm, c);
        const auto r3 = mgr.run(c, p3);
        TEST_CHECK(r3.success, "Test 158: OpenSees sur la même portée");
        for (int id : nodesB)
        {
            const auto a = r2.results.nodeDisplacement(id), b = r3.results.nodeDisplacement(id);
            TEST_CHECK(std::abs(a.ux - b.ux) <= 1e-7 * std::max(1e-6, std::abs(b.ux)) + 1e-12 &&
                       std::abs(a.uz - b.uz) <= 1e-7 * std::max(1e-6, std::abs(b.uz)) + 1e-12 &&
                       std::abs(a.ry - b.ry) <= 1e-7 * std::max(1e-6, std::abs(b.ry)) + 1e-12,
                       "Test 158: déplacements identiques à OpenSees");
        }
        for (const auto& [key, er] : r3.results.allElementResults())
        {
            const auto* e2 = r2.results.getElementResults(key);
            TEST_CHECK(e2 && std::abs(e2->startForces.My - er.startForces.My) <= 1e-6 * std::max(1.0, std::abs(er.startForces.My))
                          && std::abs(e2->endForces.N - er.endForces.N) <= 1e-6 * std::max(1.0, std::abs(er.endForces.N)),
                       "Test 158: efforts d'extrémité identiques à OpenSees");
        }
        const auto& curves = r2.results.planarCurves();
        TEST_CHECK(curves.size() == 3 && r2.results.executionMetadata().isEquilibriumVerified &&
                   !r2.results.executionMetadata().calculationMethod.empty(), "Test 158: courbes planes, équilibre, méthode tracés");
        std::cout << "[PASS] Test 158: Custom2D = OpenSees (portique plan)" << std::endl;
        ++passed;
    }

    // TEST 159 : note de calcul — chapitre des courbes RDM écrit avec les données du moteur 2D
    {
        using namespace TSA::Analysis;
        Model model;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0, 6 });
        def.setYPositions({ 0 });
        const std::string gid = gm.addGrid(def)->id();
        const int a = model.addNode(0, 0, 0), b = model.addNode(6, 0, 0);
        model.getNode(a)->setSupport(SupportDefinition::pinned());
        model.getNode(b)->setSupport(SupportDefinition::pinned());
        const int bm = model.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int lc = model.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        model.loadManager().addMemberLoad(MemberLoad::uniform(bm, lc, 10.0));

        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = "custom2d";
        c.dimension = AnalysisDimension::Plane2D;
        c.common.includeSelfWeight = false;
        c.scope.type = ScopeType::GridAxis;
        c.scope.gridId = gid;
        c.scope.axisLabel = "A";
        const auto run = mgr.run(c, mgr.prepare(model, &gm, c));
        TEST_CHECK(run.success, "Test 159: calcul Custom2D");
        const auto results = std::make_shared<ResultsModel>(run.results);
        const auto& curve = results->planarCurves().begin()->second;
        TEST_CHECK(rel(curve.MSpanExtremum, 45.0, 1e-9), "Test 159: M travée = qL²/8 dans les résultats TSA");

        const auto doc = TSA::NDC::NDCGenerator::generate(model, results, TSA::NDC::ReportConfiguration {});
        const TSA::NDC::NDCChapter* chapter = nullptr;
        for (const auto& ch : doc.chapters)
            if (ch.title.contains(QStringLiteral("Courbes RDM"))) chapter = &ch;
        TEST_CHECK(chapter != nullptr, "Test 159: chapitre des courbes présent");
        bool tableOk = false, figureOk = false;
        for (const auto& s : chapter->sections)
        {
            for (const auto& t : s.tables)
                for (const auto& row : t.rows)
                    tableOk |= row.size() == 11 && row[4].startsWith(QStringLiteral("45.00")) && row[9] != QStringLiteral("—");
            for (const auto& f : s.figures) figureOk |= f.imageBase64.startsWith(QStringLiteral("data:image/png;base64,")) && f.imageBase64.size() > 1000;
        }
        TEST_CHECK(tableOk, "Test 159: tableau des valeurs caractéristiques (M travée 45.00, L/f)");
        // Contrôle visuel facultatif : TSA_TEST_DUMP_DIR=<dossier> enregistre la figure et la note.
        if (const QString dump = qEnvironmentVariable("TSA_TEST_DUMP_DIR"); !dump.isEmpty())
        {
            TSA::NDC::renderPlanarMemberCurves(curve, QStringLiteral("Poutre B%1").arg(bm)).save(dump + "/ndc_courbes_mdd.png");
            QFile f(dump + "/ndc_mdd.html");
            if (f.open(QIODevice::WriteOnly)) f.write(doc.toHtml().toUtf8());
        }
        TEST_CHECK(figureOk, "Test 159: courbes N, V, M, déformée dessinées");
        const QString html = doc.toHtml();
        TEST_CHECK(html.contains(QStringLiteral("EI·v''(x) = M(x)")) && html.contains(QStringLiteral("Custom2D"))
                   && html.contains(QStringLiteral("3 degrés de liberté")), "Test 159: formules RDM et moteur réel dans la note");
        std::cout << "[PASS] Test 159: Note de calcul — courbes du moteur 2D" << std::endl;
        ++passed;
    }
    return true;
}
