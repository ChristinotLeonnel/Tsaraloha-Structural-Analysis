// Suite « displaymodes » : représentations du modèle (tests 250-256). Maillage du solveur (SolverMesh)
// réellement transmis par OpenSees et Custom2D, état affiché en mode Éléments finis, géométrie tracée,
// axes analytiques, politique des modes. La vue OpenGL n'est pas instanciée ici (vérification GUI).

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/Engines/Custom2D/Custom2DEngine.h"
#include "Analysis/OpenSeesAnalysisBuilder.h"
#include "Analysis/OpenSeesModelMap.h"
#include "Analysis/SolverMesh.h"
#include "Viewer/ModelDisplayMode.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>

using namespace TSA::Analysis;
using namespace TSA::Viewer;

namespace
{
/// Portique plan (axe y = yAxis) : 2 poteaux encastrés, 1 poutre.
std::array<int, 4> frame(Model& m, double y)
{
    const int a = m.addNode(0, y, 0), b = m.addNode(0, y, 3), c = m.addNode(6, y, 3), d = m.addNode(6, y, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, b, Section::heb(200), Material::steelS235());
    m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    return { a, b, c, d };
}

SolverMesh lineMesh()
{
    // Barre TSA B1 subdivisée en 2 éléments finis (nœud 100 propre au moteur) + un quadrangle.
    SolverMesh mesh;
    mesh.engineId = "test";
    mesh.nodes[1] = { 1, 0, 0, 0, 1, "" };
    mesh.nodes[2] = { 2, 4, 0, 0, 2, "" };
    mesh.nodes[100] = { 100, 2, 0, 0, 0, "subdivision" };
    mesh.nodes[3] = { 3, 4, 3, 0, 3, "" };
    mesh.nodes[4] = { 4, 0, 3, 0, 4, "" };
    const ElementKey b1 { StructuralElementKind::Beam, 1 };
    mesh.cells.push_back({ 1, SolverCellType::Line, "beam", { 1, 100 }, b1 });
    mesh.cells.push_back({ 2, SolverCellType::Line, "beam", { 100, 2 }, b1 });
    mesh.cells.push_back({ 3, SolverCellType::Quadrilateral, "shell", { 1, 2, 3, 4 }, std::nullopt });
    return mesh;
}
} // namespace

bool runSuite_DisplayModes(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 250 : SolverMesh — résumé, subdivisions, cohérence
    // -------------------------------------------------------------------------
    {
        SolverMesh mesh = lineMesh();
        const ElementKey b1 { StructuralElementKind::Beam, 1 };
        TEST_CHECK(mesh.cellsFor(b1) == 2 && mesh.internalNodeCount() == 1 && mesh.cellCount(SolverCellType::Line) == 2,
                   "Test 250: 2 éléments finis pour B1, 1 nœud propre au moteur");
        TEST_CHECK(mesh.validate().empty(), "Test 250: maillage cohérent");
        TEST_CHECK(mesh.summary().find("jusqu'à 2 éléments finis par élément TSA") != std::string::npos,
                   "Test 250: subdivisions annoncées (" << mesh.summary() << ")");
        mesh.cells.push_back({ 1, SolverCellType::Triangle, "tri", { 1, 999 }, std::nullopt });
        TEST_CHECK(mesh.validate().size() == 3, "Test 250: tag en double, nœuds manquants, nœud inconnu signalés");
        std::cout << "[PASS] Test 250: Maillage du solveur (structure neutre)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 251 : OpenSees — maillage écrit dans le script (sans exécuter le solveur)
    // -------------------------------------------------------------------------
    {
        Model m;
        const auto n = frame(m, 0.0);
        m.addTrussMember(n[0], n[2], 0.002);
        const int k = m.addNode(3, 0, 0);
        m.getNode(k)->setSupport(SupportDefinition::elastic(1e4, 1e4, 1e4));
        m.addColumn(k, m.addNode(3, 0, 3), Section::heb(200), Material::steelS235());
        const CalculationSnapshot snap = CalculationSnapshot::capture(m);
        AnalysisParameters params;
        const OpenSeesModelMap map = OpenSeesModelMap::build(snap, params);
        const SolverMesh mesh = map.solverMesh(snap);
        TEST_CHECK(mesh.engineId == "opensees" && mesh.validate().empty(), "Test 251: maillage OpenSees cohérent");
        TEST_CHECK(mesh.nodes.size() == snap.nodeCount() + map.springs().size() && map.springs().size() == 1,
                   "Test 251: nœuds du snapshot + nœud auxiliaire du ressort");
        TEST_CHECK(mesh.cells.size() == map.elements().size() + 1 && mesh.cellCount(SolverCellType::ZeroLength) == 1,
                   "Test 251: un élément par barre + zeroLength du ressort");
        bool oneEach = true;
        for (const auto& e : map.elements())
        {
            oneEach &= mesh.cellsFor(e.key) == 1;
            const auto it = std::find_if(mesh.cells.begin(), mesh.cells.end(), [&](const SolverMeshCell& c) { return c.tag == e.tag; });
            oneEach &= it != mesh.cells.end() && it->solverClass == e.opsClass && it->nodes == std::vector<int>{ e.nodeI, e.nodeJ };
        }
        TEST_CHECK(oneEach, "Test 251: tags, classes et nœuds identiques à OpenSeesModelMap (aucune subdivision)");
        TEST_CHECK(mesh.internalNodeCount() == 1 && mesh.node(map.springs()[0].auxNodeTag)->tsaNodeId == 0,
                   "Test 251: nœud auxiliaire marqué propre au moteur");
        std::cout << "[PASS] Test 251: Maillage OpenSees (correspondance du script)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 252 : OpenSees — maillage joint aux résultats d'un vrai calcul
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const auto n = frame(m, 0.0);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, n[1], lc, 10.0, 0.0, -20.0));
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        const auto rev = m.revision();
        const auto run = mgr.run(ctx, mgr.prepare(m, &gm, ctx));
        TEST_CHECK(run.success, "Test 252: calcul OpenSees (" << run.message << ")");
        const SolverMesh& mesh = run.results.solverMesh();
        TEST_CHECK(mesh.engineId == "opensees" && mesh.cells.size() == 3 && mesh.nodes.size() == 4 && mesh.validate().empty(),
                   "Test 252: 3 éléments finis, 4 nœuds (" << mesh.summary() << ")");
        bool sameCoords = true;
        for (const auto& [tag, nd] : mesh.nodes)
        {
            const auto* mn = m.getNode(nd.tsaNodeId);
            sameCoords &= mn && mn->x() == nd.x && mn->y() == nd.y && mn->z() == nd.z;
        }
        TEST_CHECK(sameCoords, "Test 252: nœuds du maillage = nœuds du modèle");
        auto results = std::make_shared<ResultsModel>(run.results);
        results->setValid(true);
        const auto before = results->nodeDisplacement(n[1]);
        const auto status = solverMeshStatus(results.get(), 0);
        const auto geometry = buildSolverMeshGeometry(results->solverMesh());
        TEST_CHECK(status.available && geometry.segments.size() == 3 && geometry.modelNodes.size() == 4 && geometry.solverNodes.empty(),
                   "Test 252: maillage affichable (3 segments, 4 nœuds)");
        TEST_CHECK(m.revision() == rev && results->nodeDisplacement(n[1]).ux == before.ux && results->solverMesh().cells.size() == 3,
                   "Test 252: affichage sans effet sur le modèle ni sur les résultats");
        ResultsModel cleared = *results;
        cleared.clear();
        TEST_CHECK(cleared.solverMesh().empty(), "Test 252: maillage effacé avec les résultats");
        std::cout << "[PASS] Test 252: Maillage OpenSees joint aux résultats" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 253 : Custom2D — maillage de l'ossature plane réellement résolue
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("Grille Test", GridType::Cartesian);
        def.setXPositions({ 0.0, 6.0 });
        def.setYPositions({ 0.0, 5.0 });
        def.setZLevels({ 0.0, 3.0 });
        const std::string gid = gm.addGrid(def)->id();
        frame(m, 0.0);
        const auto nb = frame(m, 5.0);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, nb[1], lc, 10.0, 0.0, -20.0));
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext c;
        c.engineId = Custom2DEngine::kId;
        c.dimension = AnalysisDimension::Plane2D;
        c.scope.type = ScopeType::GridAxis;
        c.scope.gridId = gid;
        c.scope.axisFamily = GridAxisFamily::Y;
        c.scope.axisLabel = "B";
        c.common.includeSelfWeight = false;
        const auto prep = mgr.prepare(m, &gm, c);
        const auto run = mgr.run(c, prep);
        TEST_CHECK(run.success, "Test 253: calcul Custom2D (" << run.message << ")");
        const SolverMesh& mesh = run.results.solverMesh();
        TEST_CHECK(mesh.engineId == "custom2d" && mesh.cells.size() == 3 && mesh.nodes.size() == 4 && mesh.validate().empty(),
                   "Test 253: portique de l'axe B seulement (" << mesh.summary() << ")");
        bool onAxisB = true, sourced = true;
        for (const auto& [tag, nd] : mesh.nodes) onAxisB &= std::abs(nd.y - 5.0) < 1e-12 && nd.tsaNodeId > 0;
        for (const auto& cell : mesh.cells) sourced &= cell.source.has_value() && mesh.cellsFor(*cell.source) == 1;
        TEST_CHECK(onAxisB && sourced, "Test 253: nœuds en coordonnées 3D réelles (y = 5), éléments rattachés aux barres TSA");
        std::cout << "[PASS] Test 253: Maillage Custom2D joint aux résultats" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 254 : état du maillage en mode Éléments finis (messages explicites)
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(!solverMeshStatus(nullptr, 0).available && solverMeshStatus(nullptr, 0).message.find("aucun calcul") != std::string::npos,
                   "Test 254: sans calcul : indisponible, expliqué");
        ResultsModel r;
        r.setSolverMesh(lineMesh());
        r.setValid(true);
        TEST_CHECK(solverMeshStatus(&r, 0).available, "Test 254: calcul à jour : disponible");
        TEST_CHECK(solverMeshStatus(&r, 2).message.find("Dalles et voiles") == std::string::npos,
                   "Test 254: quadrangles présents : pas d'avertissement surfaces");
        r.setValid(false);
        TEST_CHECK(!solverMeshStatus(&r, 0).available && solverMeshStatus(&r, 0).message.find("obsolètes") != std::string::npos,
                   "Test 254: résultats obsolètes : maillage non présenté comme actuel");
        ResultsModel noMesh;
        NodeDisplacement d;
        noMesh.setNodeDisplacement(1, d);
        noMesh.setValid(true);
        TEST_CHECK(!solverMeshStatus(&noMesh, 0).available && solverMeshStatus(&noMesh, 0).message.find("ne fournit pas") != std::string::npos,
                   "Test 254: moteur sans maillage : signalé, rien d'inventé");
        ResultsModel lines;
        SolverMesh lm = lineMesh();
        lm.cells.pop_back();
        lines.setSolverMesh(lm);
        lines.setValid(true);
        TEST_CHECK(solverMeshStatus(&lines, 2).message.find("Dalles et voiles") != std::string::npos,
                   "Test 254: dalles / voiles non maillés signalés");
        std::cout << "[PASS] Test 254: État du maillage du solveur" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 255 : géométrie tracée du maillage (subdivisions, surfaces, volumes, masquage)
    // -------------------------------------------------------------------------
    {
        const SolverMesh mesh = lineMesh();
        const auto g = buildSolverMeshGeometry(mesh);
        TEST_CHECK(g.cells == 3 && g.segments.size() == 2 + 4 && g.modelNodes.size() == 4 && g.solverNodes.size() == 1,
                   "Test 255: 2 segments de barre + 4 arêtes du quadrangle ; nœud de subdivision distingué");
        const auto hidden = buildSolverMeshGeometry(mesh, [](const ElementKey&) { return false; });
        TEST_CHECK(hidden.hiddenCells == 2 && hidden.cells == 1 && hidden.segments.size() == 4 && hidden.solverNodes.empty(),
                   "Test 255: éléments d'une barre masquée non tracés (le quadrangle sans origine reste)");
        SolverMesh hex;
        for (int i = 0; i < 8; ++i) hex.nodes[i + 1] = { i + 1, double(i & 1), double((i >> 1) & 1), double(i >> 2), 0, "" };
        hex.cells.push_back({ 1, SolverCellType::Hexahedron, "brick", { 1, 2, 4, 3, 5, 6, 8, 7 }, std::nullopt });
        hex.cells.push_back({ 2, SolverCellType::ZeroLength, "zeroLength", { 1, 1 }, std::nullopt });
        TEST_CHECK(buildSolverMeshGeometry(hex).segments.size() == 12, "Test 255: hexaèdre : 12 arêtes ; longueur nulle sans segment");
        std::cout << "[PASS] Test 255: Géométrie du maillage affiché" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 256 : politique des modes et axes analytiques
    // -------------------------------------------------------------------------
    {
        using M = ModelDisplayMode;
        TEST_CHECK(!DisplayPolicy::linearAsAxis(M::Physical) && DisplayPolicy::linearAsAxis(M::Analytical) &&
                       DisplayPolicy::linearAsAxis(M::FiniteElement) && !DisplayPolicy::linearAsAxis(M::Overlay),
                   "Test 256: axes à la place des sections en filaire et éléments finis seulement");
        TEST_CHECK(DisplayPolicy::solverMesh(M::FiniteElement) && !DisplayPolicy::solverMesh(M::Analytical) &&
                       DisplayPolicy::analyticalOverlay(M::Overlay) && !DisplayPolicy::analyticalOverlay(M::Physical),
                   "Test 256: maillage en éléments finis, axes superposés en superposition");
        TEST_CHECK(DisplayPolicy::physicalTransparency(M::Physical) == 0.0 && DisplayPolicy::physicalTransparency(M::Overlay) > 0.3,
                   "Test 256: modèle physique inchangé, solides translucides en superposition");
        const TopoDS_Shape axis = analyticalAxisShape(gp_Pnt(0, 0, 0), gp_Pnt(3, 4, 12));
        GProp_GProps props;
        if (!axis.IsNull()) BRepGProp::LinearProperties(axis, props);
        TEST_CHECK(!axis.IsNull() && axis.ShapeType() == TopAbs_EDGE && std::abs(props.Mass() - 13.0) < 1e-9,
                   "Test 256: axe = arête nœud à nœud (13 m)");
        TEST_CHECK(analyticalAxisShape(gp_Pnt(1, 1, 1), gp_Pnt(1, 1, 1)).IsNull(), "Test 256: nœuds confondus : aucune forme");
        const auto cb = analyticalColor(StructuralElementKind::Beam), cc = analyticalColor(StructuralElementKind::Column);
        TEST_CHECK(cb.Distance(cc) > 0.2, "Test 256: poutres et poteaux de couleurs distinctes");
        TEST_CHECK(analyticalKindOf(TSA::Model::BarRole::Column) == StructuralElementKind::Column &&
                       analyticalKindOf(TSA::Model::BarRole::Brace) == StructuralElementKind::Truss &&
                       analyticalKindOf(TSA::Model::BarRole::Beam) == StructuralElementKind::Beam,
                   "Test 256: barre de rôle Poteau affichée comme un poteau");
        std::cout << "[PASS] Test 256: Politique des représentations et axes analytiques" << std::endl;
        ++passed;
    }
    return true;
}
