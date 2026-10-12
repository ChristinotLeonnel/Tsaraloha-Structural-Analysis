// Suite « mesh » : maillage par Gmsh (tests 289-298, docs/MESH.md). Barre 1D (taille, divisions locales,
// progression, nœud de charge), dalle 2D avec ouverture (aire nette, aucun élément dans l'ouverture,
// structuré), volume 3D (tétraèdres, hexaèdres, liaison au nœud), raffinement local, groupes physiques et
// interfaces, import / export, conversion vers OpenSees comparée à des solutions analytiques, charges et
// propriétés, diagnostics de qualité et éléments non pris en charge, enregistrement et relecture,
// charges surfaciques (résultante et moment).

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "IO/TSAFile.h"
#include "Mesh/MeshProject.h"
#include "Mesh/MeshQuality.h"
#include "Mesh/Solvers/OpenSeesMeshAdapter.h"
#include "Mesh/SurfaceLoads.h"
#include "Analysis/AnalysisController.h"
#include "Model/Load/LoadManager.h"
#include "Project/ProjectSession.h"
#include "UI/Analysis/AnalysisManagerPanel.h"
#include "UI/Mesh/MeshAnalysisBridge.h"
#include "UI/Mesh/MeshController.h"
#include "UI/Mesh/MeshSettingsPanel.h"

#include <QComboBox>
#include <QEventLoop>
#include <QTimer>

#include <QTemporaryDir>

#include <filesystem>
#include <set>

using namespace TSA::Mesh;

namespace
{
MeshSettingsSet settings1D(double h)
{
    MeshSettingsSet s;
    s.global.globalSize = h;
    s.global.mesh2D = s.global.mesh3D = false;
    return s;
}

std::size_t elementsOf(const FeMesh& m, const std::string& src)
{
    std::size_t n = 0;
    for (const auto& e : m.elements)
        if (e.source == src && !e.boundary) ++n;
    return n;
}

double elementArea(const FeMesh& m, const FeElement& e)
{
    const int nc = feTypeCornerCount(e.type);
    P3 n;
    const auto& a = m.node(e.nodes[0]);
    for (int k = 1; k + 1 < nc; ++k)
    {
        const auto& b = m.node(e.nodes[k]);
        const auto& c = m.node(e.nodes[k + 1]);
        n = n + P3 { b.x - a.x, b.y - a.y, b.z - a.z }.cross(P3 { c.x - a.x, c.y - a.y, c.z - a.z });
    }
    return 0.5 * n.norm();
}

P3 centroid(const FeMesh& m, const FeElement& e)
{
    P3 c;
    const int nc = feTypeCornerCount(e.type);
    for (int k = 0; k < nc; ++k)
    {
        const auto& n = m.node(e.nodes[k]);
        c = c + P3 { n.x, n.y, n.z };
    }
    return c * (1.0 / nc);
}

/// Dalle 6 × 4 avec ouverture 1 × 1 centrée en (3, 2), poutre de bord B sur le côté y = 0.
int slabModel(Model& m, int* beam)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
    const int s = m.addSlab({ a, b, c, d }, 0.2, "Dalle");
    SlabOpening o;
    o.name = "trémie";
    o.shape = "rectangle";
    o.polygon = { { 2.5, 1.5 }, { 3.5, 1.5 }, { 3.5, 2.5 }, { 2.5, 2.5 } };   // repère local : origine a, u = x, v = y
    m.getSlab(s)->setOpenings({ o });
    if (beam) *beam = m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
    m.getNode(a)->setSupport(SupportDefinition::pinned());
    return s;
}

TSA::Analysis::AnalysisRunResult runWith(Model& m, const std::map<TSA::Analysis::ElementKey, std::vector<double>>& sub)
{
    GridManager gm;
    TSA::Analysis::AnalysisEngineRegistry reg;
    TSA::Analysis::registerBuiltInEngines(reg);
    TSA::Analysis::AnalysisManager mgr(reg);
    TSA::Analysis::AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = false;
    ctx.memberSubdivisions = sub;
    return mgr.run(ctx, mgr.prepare(m, &gm, ctx));
}

/// Flèche (translation locale) maximale le long d'une barre, à partir des stations.
double maxDeflection(const TSA::Analysis::ElementResults& er, double* atX = nullptr)
{
    double best = 0;
    std::vector<const TSA::Analysis::StationForces*> st { &er.startForces, &er.endForces };
    for (const auto& s : er.intermediateStations) st.push_back(&s);
    for (const auto* s : st)
    {
        const double d = std::max(std::abs(s->uy), std::abs(s->uz));
        if (d > best)
        {
            best = d;
            if (atX) *atX = s->position;
        }
    }
    return best;
}
} // namespace

bool runSuite_Mesh(int& passed)
{
    const MesherCapabilities caps = meshEngineCapabilities();
    TEST_CHECK(caps.available, "Gmsh disponible (" << caps.unavailableReason << ")");
    std::cout << "  moteur : " << caps.name << " " << caps.version << std::endl;

    // -------------------------------------------------------------------------
    // TEST 289 : maillage 1D — taille globale, divisions locales, progression, nœud de charge
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(12, 0, 0);
        const int b1 = m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const int b2 = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        const std::string r1 = "B" + std::to_string(b1), r2 = "B" + std::to_string(b2);
        auto g = MeshGeometry::extract(m);
        auto coarse = MeshGenerator::generate(g, settings1D(1.0));
        auto fine = MeshGenerator::generate(g, settings1D(0.5));
        TEST_CHECK(coarse.ok && fine.ok, "Test 289: générations 1D (" << (coarse.record.errors.empty() ? "" : coarse.record.errors.front()) << ")");
        const auto n1 = elementsOf(coarse.mesh, r1), n2 = elementsOf(fine.mesh, r1);
        TEST_CHECK(n1 >= 5 && n1 <= 7 && n2 >= 11 && n2 <= 13 && n2 > n1, "Test 289: taille 1,0 m → " << n1 << " éléments, 0,5 m → " << n2 << " sur 6 m");
        TEST_CHECK(coarse.record.mesher == "Gmsh" && coarse.record.mesherVersion == caps.version, "Test 289: moteur et version enregistrés");

        auto s = settings1D(1.0);
        LocalMeshSetting loc;
        loc.targets = { r2 };
        loc.divisions = 4;
        loc.distribution = Distribution::Progressive;
        loc.progression = 1.5;
        s.locals.push_back(loc);
        auto local = MeshGenerator::generate(g, s);
        TEST_CHECK(local.ok && elementsOf(local.mesh, r2) == 4 && elementsOf(local.mesh, r1) == n1,
                   "Test 289: 4 divisions sur " << r2 << " seulement (" << elementsOf(local.mesh, r1) << " inchangé sur " << r1 << ")");
        const auto pos = local.mesh.memberInteriorPositions(r2);
        TEST_CHECK(pos.size() == 3 && pos[0] < 1.0 - pos[2], "Test 289: progression géométrique depuis le nœud de départ");

        m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addMemberLoad(TSA::Model::MemberLoad::pointOnMember(0, b1, 1, -10.0, 2.2));
        g = MeshGeometry::extract(m);
        auto withLoad = MeshGenerator::generate(g, settings1D(1.0));
        bool nodeAtLoad = false;
        for (const auto& n : withLoad.mesh.nodes) nodeAtLoad |= std::abs(n.x - 2.2) < 1e-9 && std::abs(n.y) < 1e-12;
        bool group = false;
        for (const auto& gr : withLoad.mesh.groups) group |= gr.name == "charges ponctuelles" && gr.nodes.size() == 1;
        TEST_CHECK(withLoad.ok && nodeAtLoad && group, "Test 289: nœud conservé sous la charge ponctuelle (x = 2,2 m), groupe « charges ponctuelles »");
        std::cout << "[PASS] Test 289: Maillage 1D par Gmsh" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 290 : dalle 2D avec ouverture ; dalle structurée
    // -------------------------------------------------------------------------
    {
        Model m;
        slabModel(m, nullptr);
        const auto g = MeshGeometry::extract(m);
        MeshSettingsSet s;
        s.global.globalSize = 0.4;
        s.global.mesh3D = false;
        auto r = MeshGenerator::generate(g, s);
        TEST_CHECK(r.ok, "Test 290: dalle avec ouverture maillée (" << (r.record.errors.empty() ? "" : r.record.errors.front()) << ")");
        double area = 0;
        bool inHole = false;
        for (const auto& e : r.mesh.elements)
        {
            if (feTypeDimension(e.type) != 2 || e.boundary) continue;
            area += elementArea(r.mesh, e);
            const P3 c = centroid(r.mesh, e);
            inHole |= c.x > 2.5 && c.x < 3.5 && c.y > 1.5 && c.y < 2.5;
        }
        TEST_CHECK(std::abs(area - 23.0) < 1e-6 && !inHole, "Test 290: aire maillée = aire nette 23 m² (" << area << "), aucun élément dans l'ouverture");
        bool holeEdgeNodes = false;
        for (const auto& n : r.mesh.nodes) holeEdgeNodes |= std::abs(n.x - 2.5) < 1e-9 && n.y > 1.6 && n.y < 2.4;
        TEST_CHECK(holeEdgeNodes, "Test 290: nœuds sur le contour de l'ouverture");
        s.global.recombine = true;
        auto q = MeshGenerator::generate(g, s);
        TEST_CHECK(q.ok && q.mesh.countOf(FeType::Quad4) > 0, "Test 290: recombinaison en quadrangles (" << q.mesh.countOf(FeType::Quad4) << ")");

        Model plain;
        const int a = plain.addNode(0, 0, 0), b = plain.addNode(6, 0, 0), c = plain.addNode(6, 4, 0), d = plain.addNode(0, 4, 0);
        plain.addSlab({ a, b, c, d }, 0.2);
        MeshSettingsSet st;
        st.global.globalSize = 0.5;
        st.global.mesh3D = false;
        auto structured = MeshGenerator::generate(MeshGeometry::extract(plain), st);
        TEST_CHECK(structured.ok && structured.mesh.countOf(FeType::Quad4) == 96 && structured.mesh.countOf(FeType::Tri3) == 0,
                   "Test 290: dalle rectangulaire structurée 12 × 8 = 96 quadrangles (" << structured.mesh.countOf(FeType::Quad4) << ")");
        std::cout << "[PASS] Test 290: Maillage 2D (dalle avec ouverture)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 291 : volume 3D (semelle) — tétraèdres, hexaèdres, liaison au nœud du poteau
    // -------------------------------------------------------------------------
    {
        Model m;
        const int base = m.addNode(0, 0, 0), top = m.addNode(0, 0, 3);
        m.addColumn(base, top, Section::rectangular(0.3, 0.3), Material::concreteC25_30());
        const int f = m.addFoundation(base, 2.0, 2.0, 0.6, "S1");
        const auto g = MeshGeometry::extract(m);
        MeshSettingsSet s;
        s.global.globalSize = 0.25;
        s.global.mesh3D = true;
        s.global.volumeElement = VolumeElement::Hexahedra;
        auto hex = MeshGenerator::generate(g, s);
        const std::string ref = "F" + std::to_string(f);
        TEST_CHECK(hex.ok && hex.mesh.countOf(FeType::Hex8) == 8 * 8 * 3,
                   "Test 291: semelle 2 × 2 × 0,6 en hexaèdres : " << hex.mesh.countOf(FeType::Hex8) << " (attendu 192)");
        bool linked = false;
        for (const auto& n : hex.mesh.nodes) linked |= n.tsaNode == base;
        TEST_CHECK(linked, "Test 291: nœud du poteau sommet du maillage de la semelle");
        s.global.volumeElement = VolumeElement::Tetrahedra;
        auto tet = MeshGenerator::generate(g, s);
        double qmin = 1;
        for (const auto& e : tet.mesh.elements)
            if (e.type == FeType::Tet4) qmin = std::min(qmin, e.quality);
        TEST_CHECK(tet.ok && tet.mesh.countOf(FeType::Tet4) > 100 && qmin > 0, "Test 291: tétraèdres valides (" << tet.mesh.countOf(FeType::Tet4) << ", minSICN " << qmin << ")");
        double vol = 0;
        for (const auto& e : tet.mesh.elements)
            if (e.type == FeType::Tet4)
            {
                const auto &A = tet.mesh.node(e.nodes[0]), &B = tet.mesh.node(e.nodes[1]), &C = tet.mesh.node(e.nodes[2]), &D = tet.mesh.node(e.nodes[3]);
                const P3 a { A.x, A.y, A.z };
                vol += std::abs((P3 { B.x, B.y, B.z } - a).dot((P3 { C.x, C.y, C.z } - a).cross(P3 { D.x, D.y, D.z } - a))) / 6;
            }
        TEST_CHECK(std::abs(vol - 2.4) < 1e-6, "Test 291: volume maillé = 2,4 m³ (" << vol << ")");
        s.global.order = ElementOrder::Second;
        auto tet10 = MeshGenerator::generate(g, s);
        TEST_CHECK(tet10.ok && tet10.mesh.countOf(FeType::Tet10) == tet.mesh.countOf(FeType::Tet4), "Test 291: second ordre (tet10)");
        std::cout << "[PASS] Test 291: Maillage 3D (semelle)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 292 : raffinement local (0,10 m autour d'un appui, 0,50 m ailleurs)
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
        m.addSlab({ a, b, c, d }, 0.2);
        m.getNode(a)->setSupport(SupportDefinition::pinned());
        MeshSettingsSet s;
        s.global.globalSize = 0.5;
        s.global.mesh3D = false;
        RefinementZone z;
        z.kind = RefinementZone::Kind::Support;
        z.radius = 0.3;
        z.size = 0.1;
        s.zones.push_back(z);
        auto r = MeshGenerator::generate(MeshGeometry::extract(m), s);
        double near = 0, far = 0;
        int nn = 0, nf = 0;
        for (const auto& e : r.mesh.elements)
        {
            if (feTypeDimension(e.type) != 2) continue;
            const P3 c = centroid(r.mesh, e);
            const double h = std::sqrt(2 * elementArea(r.mesh, e));
            if (c.norm() < 0.3) near += h, ++nn;
            else if (c.norm() > 3.0) far += h, ++nf;
        }
        near /= std::max(1, nn), far /= std::max(1, nf);
        TEST_CHECK(r.ok && nn > 5 && near < 0.2 && far > 0.35, "Test 292: tailles près de l'appui " << near << " m, loin " << far << " m");
        bool unstructuredNoted = r.record.parameters["surfaces structurées"] == "0 / 1";
        TEST_CHECK(unstructuredNoted, "Test 292: zone de raffinement → maillage non structuré (taille variable), consigné");
        std::cout << "[PASS] Test 292: Raffinement local" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 293 : groupes physiques, interfaces, export / import
    // -------------------------------------------------------------------------
    {
        Model m;
        int beam = 0;
        const int slab = slabModel(m, &beam);
        const auto g = MeshGeometry::extract(m);
        MeshSettingsSet s;
        s.global.globalSize = 0.5;
        s.global.mesh3D = false;
        s.global.boundaryElements = true;
        QTemporaryDir tmp;
        GenerationOptions o;
        o.writeMshTo = tmp.filePath("modele.msh").toStdString();
        auto r = MeshGenerator::generate(g, s, o);
        const std::string rb = "B" + std::to_string(beam), rs = "SL" + std::to_string(slab);
        std::set<std::string> names;
        for (const auto& gr : r.mesh.groups) names.insert(gr.name);
        std::string all;
        for (const auto& n : names) all += n + " ; ";
        TEST_CHECK(r.ok && names.count(rb) && names.count(rs) && names.count(rs + ":contour") && names.count("appuis") && names.count("matériau:" + Material::steelS235().name) &&
                       names.count("interface:" + rb + "|" + rs),
                   "Test 293: groupes objets, contour, appuis, matériaux, interface poutre / dalle (" << all << ")");
        // Nœuds de la poutre = nœuds de la dalle sur le bord commun (interface conforme)
        std::set<int> slabNodes;
        for (const auto& e : r.mesh.elements)
            if (e.source == rs && !e.boundary)
                for (int n : e.nodes) slabNodes.insert(n);
        bool shared = elementsOf(r.mesh, rb) > 0;
        for (const auto& e : r.mesh.elements)
            if (e.source == rb)
                for (int n : e.nodes) shared &= slabNodes.count(n) > 0;
        TEST_CHECK(shared, "Test 293: nœuds de la poutre partagés avec la dalle");
        TEST_CHECK(std::filesystem::exists(o.writeMshTo), "Test 293: fichier .msh écrit par Gmsh");
        FeMesh imported;
        std::vector<std::string> warn;
        std::string err;
        TEST_CHECK(MeshGenerator::importMesh(o.writeMshTo, &imported, &warn, &err) && elementsOf(imported, rb) == elementsOf(r.mesh, rb) &&
                       elementsOf(imported, rs) == elementsOf(r.mesh, rs),
                   "Test 293: import .msh : objets retrouvés par les groupes physiques (" << err << ")");
        const std::string vtk = tmp.filePath("export.vtk").toStdString(), msh2 = tmp.filePath("export.msh").toStdString();
        TEST_CHECK(MeshGenerator::exportMesh(r.mesh, vtk, &err) && std::filesystem::exists(vtk) && MeshGenerator::exportMesh(r.mesh, msh2, &err),
                   "Test 293: export VTK et MSH du maillage TSA (" << err << ")");
        FeMesh round;
        TEST_CHECK(MeshGenerator::importMesh(msh2, &round, &warn, &err) && round.elements.size() == r.mesh.elements.size() &&
                       round.nodes.size() == r.mesh.nodes.size() && elementsOf(round, rs) == elementsOf(r.mesh, rs),
                   "Test 293: aller-retour export / import identique (" << round.elements.size() << " / " << r.mesh.elements.size() << ")");
        std::cout << "[PASS] Test 293: Groupes physiques, interfaces, import / export" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 294 : conversion vers OpenSees — poutre sur deux appuis, référence analytique
    // -------------------------------------------------------------------------
    {
        Model m;
        const double L = 6.0, q = 12.0;
        const int a = m.addNode(0, 0, 0), b = m.addNode(L, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::pinned());
        m.getNode(b)->setSupport(SupportDefinition::pinned());
        const Section sec = Section::rectangular(0.3, 0.5);
        const Material mat = Material::concreteC25_30();
        const int beam = m.addBar(a, b, sec, mat, BarRole::Beam);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addMemberLoad(TSA::Model::MemberLoad::uniform(0, beam, lc, -q));
        const auto g = MeshGeometry::extract(m);
        auto r = MeshGenerator::generate(g, settings1D(0.5));
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        const auto compat = OpenSeesMeshAdapter::check(r.mesh, g, "OpenSees", reg.engine("opensees")->capabilities());
        TEST_CHECK(!compat.blocking() && compat.memberSubdivisions.size() == 1 && compat.memberSubdivisions.begin()->second.size() == 11,
                   "Test 294: 12 sous-éléments elasticBeamColumn transmissibles (" << compat.summary() << ")");
        const auto run = runWith(m, compat.memberSubdivisions);
        TEST_CHECK(run.success, "Test 294: calcul OpenSees avec barre subdivisée (" << run.message << ")");
        const auto* er = run.results.getElementResults(TSA::Analysis::StructuralElementKind::Beam, beam);
        TEST_CHECK(er && std::abs(er->length - L) < 1e-9 && er->intermediateStations.size() > 30, "Test 294: résultats recomposés sur la barre TSA");
        double x = 0;
        const double EI = mat.mechanical.youngModulus / 1000.0 * sec.iy();
        const double ref = 5 * q * std::pow(L, 4) / (384 * EI);
        const double fem = maxDeflection(*er, &x);
        TEST_CHECK(std::abs(fem - ref) / ref < 1e-3 && std::abs(x - L / 2) < 1e-6,
                   "Test 294: flèche à mi-portée " << fem * 1000 << " mm, analytique 5qL⁴/384EI = " << ref * 1000 << " mm");
        const auto* ra = run.results.getNodeReaction(a);
        TEST_CHECK(ra && std::abs(std::abs(ra->rz) - q * L / 2) < 1e-6, "Test 294: réaction qL/2 = " << q * L / 2 << " kN");
        double mmax = 0;
        for (const auto& s : er->intermediateStations) mmax = std::max({ mmax, std::abs(s.My), std::abs(s.Mz) });
        TEST_CHECK(std::abs(mmax - q * L * L / 8) / (q * L * L / 8) < 1e-6, "Test 294: moment maximal qL²/8 = " << q * L * L / 8 << " kN·m (" << mmax << ")");
        int internal = 0;
        for (const auto& [n, d] : run.results.allDisplacements()) internal += m.getNode(n) ? 0 : 1;
        TEST_CHECK(internal == 0, "Test 294: aucun nœud intermédiaire exposé dans les résultats");
        std::cout << "[PASS] Test 294: Conversion vers OpenSees (référence analytique)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 295 : conservation des charges et des propriétés (charge ponctuelle, relâchement)
    // -------------------------------------------------------------------------
    {
        Model m;
        const double L = 5.0, P = 40.0;
        const int a = m.addNode(0, 0, 0), b = m.addNode(L, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::pinned());
        m.getNode(b)->setSupport(SupportDefinition::pinned());
        const Section sec = Section::ipe(300);
        const Material mat = Material::steelS235();
        const int beam = m.addBar(a, b, sec, mat, BarRole::Beam);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addMemberLoad(TSA::Model::MemberLoad::pointOnMember(0, beam, lc, -P, 1.7));
        const auto g = MeshGeometry::extract(m);
        auto r = MeshGenerator::generate(g, settings1D(0.6));
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        const auto compat = OpenSeesMeshAdapter::check(r.mesh, g, "OpenSees", reg.engine("opensees")->capabilities());
        const auto meshed = runWith(m, compat.memberSubdivisions);
        const auto single = runWith(m, {});
        TEST_CHECK(meshed.success && single.success, "Test 295: calculs avec et sans maillage");
        const double rA = P * (L - 1.7) / L, rB = P * 1.7 / L;
        const auto *ma = meshed.results.getNodeReaction(a), *mb = meshed.results.getNodeReaction(b);
        TEST_CHECK(ma && mb && std::abs(std::abs(ma->rz) - rA) < 1e-6 && std::abs(std::abs(mb->rz) - rB) < 1e-6,
                   "Test 295: réactions P(L−a)/L et Pa/L conservées (" << ma->rz << ", " << mb->rz << ")");
        const auto *e1 = meshed.results.getElementResults(TSA::Analysis::StructuralElementKind::Beam, beam),
                   *e0 = single.results.getElementResults(TSA::Analysis::StructuralElementKind::Beam, beam);
        double m1 = 0, m0 = 0;
        for (const auto& s : e1->intermediateStations) m1 = std::max({ m1, std::abs(s.My), std::abs(s.Mz) });
        for (const auto& s : e0->intermediateStations) m0 = std::max({ m0, std::abs(s.My), std::abs(s.Mz) });
        const double mref = P * 1.7 * (L - 1.7) / L;
        TEST_CHECK(std::abs(m1 - mref) / mref < 1e-6, "Test 295: moment sous la charge Pab/L = " << mref << " kN·m (maillé " << m1 << ", non maillé " << m0 << ")");
        const double EI = mat.mechanical.youngModulus / 1000.0 * sec.iy();
        const double aa = 1.7, bb = L - 1.7;
        const double dRef = P * aa * aa * bb * bb / (3 * EI * L);   // flèche sous la charge
        double dAt = 0;
        for (const auto& s : e1->intermediateStations)
            if (std::abs(s.position - 1.7) < 1e-9) dAt = std::max(std::abs(s.uy), std::abs(s.uz));
        TEST_CHECK(std::abs(dAt - dRef) / dRef < 1e-3, "Test 295: flèche sous la charge Pa²b²/3EIL = " << dRef * 1000 << " mm (" << dAt * 1000 << ")");
        std::cout << "[PASS] Test 295: Charges et propriétés conservées" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 296 : diagnostics de qualité et éléments non pris en charge
    // -------------------------------------------------------------------------
    {
        FeMesh bad;
        auto node = [&](double x, double y) {
            FeNode n;
            n.id = int(bad.nodes.size()) + 1;
            n.x = x, n.y = y;
            bad.nodes.push_back(n);
            return n.id;
        };
        const int n1 = node(0, 0), n2 = node(1, 0), n3 = node(1, 1), n4 = node(0, 1), n5 = node(2, 0), n6 = node(2, 1), n7 = node(3, 0), n8 = node(9, 9);
        (void)n8;
        auto el = [&](FeType t, std::vector<int> ns) {
            FeElement e;
            e.id = int(bad.elements.size()) + 1;
            e.type = t;
            e.nodes = ns;
            e.source = "SL1";
            bad.elements.push_back(e);
        };
        el(FeType::Quad4, { n1, n2, n3, n4 });
        el(FeType::Quad4, { n2, n3, n6, n5 });   // orientation inversée par rapport à sa voisine
        el(FeType::Tri3, { n5, n7, n5 });        // nœud répété
        const auto rep = analyzeMesh(bad, QualityCriteria {});
        std::set<std::string> kinds;
        for (const auto& i : rep.issues) kinds.insert(i.kind);
        TEST_CHECK(kinds.count("élément inversé") && kinds.count("connexion incorrecte") && kinds.count("nœud isolé") && rep.errors >= 2,
                   "Test 296: élément inversé, nœud répété, nœud isolé détectés (" << rep.errors << " erreurs)");

        Model m;
        int beam = 0;
        slabModel(m, &beam);
        const auto g = MeshGeometry::extract(m);
        MeshSettingsSet s;
        s.global.globalSize = 0.5;
        s.global.mesh3D = false;
        s.global.order = ElementOrder::Second;
        auto r = MeshGenerator::generate(g, s);
        const auto good = analyzeMesh(r.mesh, s.global.quality, r.record.interfaces);
        TEST_CHECK(r.ok && good.errors == 0, "Test 296: maillage Gmsh sans erreur de qualité (" << good.errors << ")");
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        const auto compat = OpenSeesMeshAdapter::check(r.mesh, g, "OpenSees", reg.engine("opensees")->capabilities());
        bool order = false, shells = false;
        for (const auto& i : compat.items)
        {
            order |= i.category == "Ordre des éléments" && i.level == SolverCheckItem::Level::Blocking;
            shells |= i.message.find("Quad8") != std::string::npos || i.message.find("surface non transmis") != std::string::npos;
        }
        TEST_CHECK(compat.blocking() && order && shells && compat.memberSubdivisions.empty() && !compat.strategies.empty(),
                   "Test 296: second ordre refusé pour OpenSees, coques signalées, stratégie proposée, aucune conversion silencieuse");
        std::cout << "[PASS] Test 296: Diagnostics de qualité et éléments non pris en charge" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 297 : enregistrement et relecture d'un projet ; maillage obsolète
    // -------------------------------------------------------------------------
    {
        Model m;
        int beam = 0;
        slabModel(m, &beam);
        MeshProject p;
        p.settings.global.globalSize = 0.45;
        p.settings.global.mesh3D = false;
        p.settings.profile = "Maillage standard";
        LocalMeshSetting loc;
        loc.targets = { "B" + std::to_string(beam) };
        loc.divisions = 7;
        p.settings.locals.push_back(loc);
        RefinementZone z;
        z.kind = RefinementZone::Kind::Point;
        z.x = 1, z.y = 1;
        z.size = 0.15;
        p.settings.zones.push_back(z);
        const auto g = MeshGeometry::extract(m);
        auto r = MeshGenerator::generate(g, p.settings);
        TEST_CHECK(r.ok, "Test 297: génération");
        p.hasMesh = true;
        p.mesh = r.mesh;
        p.record = r.record;
        p.store(m);
        const std::string path = "test_mesh_projet.tsa";
        TSAFileWriter w;
        std::string err;
        TEST_CHECK(w.saveToFile(path, m, nullptr, "Maillage", "Test", &err), "Test 297: enregistrement .tsa (" << err << ")");
        Model loaded;
        TSAFileReader rd;
        TEST_CHECK(rd.loadFromFile(path, loaded, nullptr, "", nullptr, nullptr, nullptr, &err), "Test 297: relecture (" << err << ")");
        std::filesystem::remove(path);
        const MeshProject back = MeshProject::load(loaded);
        TEST_CHECK(back.hasMesh && back.mesh.elements.size() == r.mesh.elements.size() && back.mesh.nodes.size() == r.mesh.nodes.size() &&
                       MeshGenerator::settingsHash(back.settings) == MeshGenerator::settingsHash(p.settings) && back.record.mesherVersion == r.record.mesherVersion &&
                       back.settings.profile == "Maillage standard" && back.settings.locals.size() == 1 && back.settings.zones.size() == 1,
                   "Test 297: paramètres, profil, maillage et rapport restitués");
        const auto* slab = loaded.slabs().begin()->second.openings().empty() ? nullptr : &loaded.slabs().begin()->second;
        TEST_CHECK(slab && slab->openings().front().polygon.size() == 4, "Test 297: ouverture de dalle restituée");
        TEST_CHECK(!back.isStale(MeshGeometry::extract(loaded).hash), "Test 297: maillage à jour après relecture");
        loaded.getNode(loaded.nodes().rbegin()->first)->setCoordinates(0, 4.5, 0);
        TEST_CHECK(back.isStale(MeshGeometry::extract(loaded).hash), "Test 297: géométrie modifiée → maillage obsolète détecté");
        auto again = MeshGenerator::generate(MeshGeometry::extract(loaded), back.settings);
        TEST_CHECK(again.ok, "Test 297: régénération avec les paramètres restitués");
        std::cout << "[PASS] Test 297: Enregistrement, relecture, maillage obsolète" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 298 : charges surfaciques — charges nodales équivalentes, résultante et moment
    // -------------------------------------------------------------------------
    {
        Model m;
        slabModel(m, nullptr);
        const int wa = m.addNode(0, 6, 0), wb = m.addNode(4, 6, 0);
        const int wall = m.addWall(wa, wb, 3.0, 0.2);
        const auto g = MeshGeometry::extract(m);
        MeshSettingsSet s;
        s.global.globalSize = 0.5;
        s.global.mesh3D = false;
        for (ElementOrder order : { ElementOrder::First, ElementOrder::Second })
        {
            s.global.order = order;
            for (bool quads : { false, true })
            {
                s.global.recombine = quads;
                auto r = MeshGenerator::generate(g, s);
                SurfacePressure p { "SL" + std::to_string(m.slabs().begin()->first), 5.0, SurfacePressure::Direction::Gravity, false };
                const auto d = distributeSurfaceLoad(r.mesh, g, p);
                TEST_CHECK(d.ok && std::abs(d.resultant.z + 5.0 * 23.0) < 1e-6 && d.momentError < 1e-6,
                           "Test 298: dalle avec ouverture, ordre " << int(order) << (quads ? ", quadrangles" : ", triangles") << " : résultante " << d.resultant.z
                                                                    << " kN (attendu −115), écart de moment " << d.momentError);
            }
        }
        s.global.order = ElementOrder::First;
        s.global.recombine = false;
        auto r = MeshGenerator::generate(g, s);
        SurfacePressure wind { "W" + std::to_string(wall), 1.2, SurfacePressure::Direction::LocalNormal, false };
        const auto dw = distributeSurfaceLoad(r.mesh, g, wind);
        TEST_CHECK(dw.ok && std::abs(dw.resultant.norm() - 1.2 * 12.0) < 1e-6 && std::abs(dw.resultant.z) < 1e-9,
                   "Test 298: pression normale sur voile : résultante horizontale 14,4 kN");
        SurfacePressure missing { "SL999", 1.0 };
        TEST_CHECK(!distributeSurfaceLoad(r.mesh, g, missing).ok, "Test 298: surface inconnue : charge non convertie, signalée");
        std::cout << "[PASS] Test 298: Charges surfaciques" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 299 : contrôleur et panneau — génération en arrière-plan (interface réactive), aperçu, obsolescence
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(30, 0, 0), c = m.addNode(30, 20, 0), d = m.addNode(0, 20, 0);
        m.addSlab({ a, b, c, d }, 0.25);
        TSA::UI::MeshController ctl;
        ctl.setModel(&m);
        ctl.settings().global.globalSize = 0.12;
        ctl.settings().global.mesh3D = false;
        ctl.settings().global.preferStructured = false;   // maillage non structuré : génération plus longue
        ctl.commitSettings();
        int ticks = 0;
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
        timer.start(5);
        bool done = false, okGen = false;
        QEventLoop loop;
        QObject::connect(&ctl, &TSA::UI::MeshController::generationFinished, [&](bool, bool ok, const QString&) {
            done = true;
            okGen = ok;
            loop.quit();
        });
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        TEST_CHECK(ctl.generate(false) && ctl.busy(), "Test 299: génération lancée dans un thread de travail");
        loop.exec();
        timer.stop();
        TEST_CHECK(done && okGen && ctl.hasMesh() && ctl.project().mesh.countByDimension(2) > 30000,
                   "Test 299: maillage de " << ctl.project().mesh.countByDimension(2) << " éléments 2D généré");
        TEST_CHECK(ticks > 3, "Test 299: interface réactive pendant la génération (" << ticks << " événements traités)");
        TEST_CHECK(!m.meshProjectJson().empty() && !ctl.stale() && ctl.quality().elements2D == ctl.project().mesh.countByDimension(2),
                   "Test 299: maillage enregistré dans le projet, à jour, qualité contrôlée");
        const auto elements = ctl.project().mesh.elements.size();
        ctl.settings().global.globalSize = 2.0;
        done = false;
        TEST_CHECK(ctl.generate(true), "Test 299: aperçu lancé");
        loop.exec();
        TEST_CHECK(done && ctl.preview() && ctl.preview()->elements.size() < elements && ctl.project().mesh.elements.size() == elements,
                   "Test 299: aperçu distinct, maillage du projet inchangé");
        ctl.discardPreview();
        m.getNode(c)->setCoordinates(31, 20, 0);
        TEST_CHECK(ctl.stale() && TSA::UI::meshStatusText(ctl).contains(QStringLiteral("OBSOLÈTE")), "Test 299: géométrie modifiée → maillage signalé obsolète");
        TSA::UI::MeshSettingsPanel panel(&ctl);
        int algos = 0, volumes = 0;
        for (auto* combo : panel.findChildren<QComboBox*>())
        {
            for (int i = 0; i < combo->count(); ++i)
            {
                algos += combo->itemText(i) == QStringLiteral("Frontal-Delaunay pour quadrangles");
                volumes += combo->itemText(i).startsWith(QStringLiteral("Hexaèdres (transfini"));
            }
        }
        TEST_CHECK(algos == 1 && volumes == 1, "Test 299: panneau : algorithmes Gmsh proposés, hexaèdres réservés aux volumes transfinis");
        std::cout << "[PASS] Test 299: Contrôleur et panneau de maillage" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 390 : maillage transmis au calcul par le gestionnaire d'analyse partagé (TSA et TSALab) —
    // plaque carrée, appui simple dur, flèche comparée à la référence de Mindlin (cas FEA-07)
    // -------------------------------------------------------------------------
    {
        TSA::Project::ProjectSession s;
        Model& m = s.model();
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 6, 0), d = m.addNode(0, 6, 0);
        const int sl = m.addSlab({ a, b, c, d }, 0.2, "Dalle");
        TSA::Model::Material mat = TSA::Model::Material::concreteC25_30();
        mat.E = 31.0e9, mat.nu = 0.2, mat.density = 2500.0;
        mat.syncMechanical();
        m.getSlab(sl)->setMaterial(mat);
        std::vector<TSA::Model::SurfaceEdgeSupport> sups(4);
        for (int e = 0; e < 4; ++e)
        {
            sups[e].edge = e;
            for (int k = 0; k < 3; ++k) sups[e].fixed[k] = true;
            sups[e].fixed[e % 2 == 0 ? 4 : 3] = true;   // rotation tangentielle au bord bloquée (appui dur)
        }
        m.getSlab(sl)->setEdgeSupports(sups);
        const int lc = m.loadManager().addLoadCase(TSA::Model::LoadCase(0, "q", TSA::Model::LoadCaseCategory::Live));
        TSA::Model::SurfaceLoad q;
        q.surfaceId = sl, q.loadCaseId = lc, q.value = 10.0;
        m.loadManager().addSurfaceLoad(q);

        TSA::UI::MeshController ctl;
        ctl.setModel(&m);
        auto& g = ctl.settings().global;
        g.globalSize = 0.25, g.mesh3D = false, g.recombine = true;
        g.nearSupports.enabled = g.nearPointLoads.enabled = g.nearOpenings.enabled = g.nearConnections.enabled = g.nearSectionChanges.enabled = false;
        g.globalSize *= 0.25 / g.effectiveSize();
        ctl.commitSettings();
        QEventLoop loop;
        bool genOk = false;
        QObject::connect(&ctl, &TSA::UI::MeshController::generationFinished, [&](bool, bool ok, const QString&) { genOk = ok; loop.quit(); });
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        TEST_CHECK(ctl.generate(false), "Test 390: génération lancée");
        loop.exec();
        TEST_CHECK(genOk && ctl.hasMesh() && !ctl.stale(), "Test 390: plaque maillée (" << ctl.project().mesh.countByDimension(2) << " éléments 2D)");

        auto ctx = s.analysis().context();
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        ctx.common.useGeneratedMesh = true;
        ctx.loadCaseIds = { lc };
        s.analysis().setContext(ctx);
        const auto* engine = s.analysis().registry().engine("opensees");
        TEST_CHECK(engine && engine->availability().available, "Test 390: OpenSees disponible");

        // Sans transmission : la dalle n'est pas calculée (exclue avec avertissement)
        TSA::Analysis::AnalysisContext bare = ctx;
        TEST_CHECK(TSA::UI::applyMeshToAnalysis(nullptr, bare, *engine, nullptr) && bare.surfaces.elements.empty(), "Test 390: sans contrôleur — aucun maillage transmis");
        // Avec transmission : coques du maillage dans le contexte, réglages du projet inchangés
        TSA::Analysis::AnalysisContext meshed = ctx;
        std::vector<QString> logs;
        TEST_CHECK(TSA::UI::applyMeshToAnalysis(&ctl, meshed, *engine, nullptr, [&](const QString& t, const QString&) { logs.push_back(t); }) &&
                       meshed.surfaces.elements.size() == ctl.project().mesh.countByDimension(2) && !meshed.meshLabel.empty() && !logs.empty() &&
                       s.analysis().context().surfaces.elements.empty(),
                   "Test 390: maillage transmis au contexte du calcul (" << meshed.surfaces.elements.size() << " coques), réglages du projet inchangés");
        const auto prepared = s.analysis().prepare(meshed);
        for (const auto& w : prepared.validation.texts(TSA::Analysis::ValidationSeverity::Warning)) std::cout << "  avertissement : " << w << std::endl;
        TEST_CHECK(prepared.canRun() && !prepared.validation.hasWarnings(), "Test 390: modèle maillé valide, sans avertissement (le panneau ne demandera rien)");

        TSA::UI::AnalysisManagerPanel panel(&s, nullptr);
        int preparerCalls = 0;
        panel.setContextPreparer([&](TSA::Analysis::AnalysisContext& cx, const TSA::Analysis::AnalysisEngine& en) {
            ++preparerCalls;
            return TSA::UI::applyMeshToAnalysis(&ctl, cx, en, &panel);
        });
        bool runOk = false;
        QObject::connect(&s.analysis(), &TSA::Analysis::AnalysisController::runCompleted, &loop,
                         [&](const TSA::Analysis::AnalysisContext&, bool ok, const QString&) { runOk = ok; loop.quit(); });
        QTimer::singleShot(300000, &loop, &QEventLoop::quit);
        TEST_CHECK(panel.run(), "Test 390: calcul lancé par le gestionnaire d'analyse");
        loop.exec();
        const auto r = s.analysis().results();
        double uz = 0;
        if (r)
            for (const auto& [id, dsp] : r->surfaceNodeDisplacements())
                if (std::abs(dsp.uz) > std::abs(uz)) uz = dsp.uz;
        const double D = 31.0e6 * 0.008 / (12.0 * 0.96), G = 31.0e6 / 2.4;
        const double ref = -(0.004062352660675069 * 10.0 * 1296.0 / D + 0.07367135326539033 * 10.0 * 36.0 / (5.0 / 6.0 * G * 0.2));
        TEST_CHECK(runOk && preparerCalls == 1 && std::abs(uz - ref) <= 1e-3 * std::abs(ref) && s.analysis().context().surfaces.elements.empty(),
                   "Test 390: flèche " << uz << " m, référence de Mindlin " << ref << " m (écart " << std::abs(uz / ref - 1) * 100 << " %)");
        std::cout << "[PASS] Test 390: maillage transmis au calcul par le gestionnaire d'analyse partagé (référence de Mindlin)" << std::endl;
        ++passed;
    }
    return true;
}
