// Suite « surfaces » : éléments surfaciques (dalles, voiles) transmis au calcul en coques OpenSees (docs/SURFACES.md).
// Chaîne réelle : modèle TSA → MeshGeometry → Gmsh → OpenSeesMeshAdapter (coques, charges nodales cohérentes, poids propre)
// → AnalysisManager (CalculationSnapshot::withSurfaces) → OpenSees (ShellMITC4 / ShellDKGT) → ResultsModel.
//   354  plaque console (bande encastrée) : flèche et moment d'encastrement comparés à la théorie des poutres (ν = 0),
//        quadrangles et triangles, convergence du maillage, équilibre ;
//   355  ouvertures : aire maillée nette, poids propre et charge uniforme sur l'aire nette, charge ponctuelle (refusée dans
//        l'ouverture), ouverture de voile maillée ;
//   356  dalle sur poutres et poteaux : nœuds partagés, transfert des charges aux poutres, Σ réactions = charges + poids propre ;
//        voile de contreventement en console (flexion + cisaillement) ;
//   357  formulations non prises en charge (plaque, membrane) refusées, surface non structurale, charge sur une surface
//        non transmise refusée, charge sans maillage refusée ;
//   358  persistance : formulations, ouvertures des voiles, charges surfaciques ; validation des ouvertures de voile.

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Mesh/MeshGenerator.h"
#include "Mesh/MeshGeometry.h"
#include "Mesh/MeshResultField.h"
#include "Mesh/Solvers/OpenSeesMeshAdapter.h"
#include "Model/Load/LoadManager.h"
#include "NDC/NDCExporter.h"
#include "NDC/ReportManager.h"

#include <QTemporaryDir>

using namespace TSA::Mesh;
using TSA::Analysis::AnalysisRunResult;

namespace
{
struct SurfRun
{
    GenerationResult mesh;
    SolverCompatibility compat;
    TSA::Analysis::PreparedAnalysis prepared;
    AnalysisRunResult run;
};

MeshSettingsSet settings2D(double h, bool quads)
{
    MeshSettingsSet s;
    s.global.globalSize = h;
    s.global.mesh3D = false;
    s.global.recombine = quads;
    s.global.nearSupports.enabled = s.global.nearPointLoads.enabled = s.global.nearOpenings.enabled = s.global.nearConnections.enabled =
        s.global.nearSectionChanges.enabled = false;
    s.global.globalSize *= h / s.global.effectiveSize();   // taille effective = h (facteur de finesse compensé)
    return s;
}

SurfRun runSurfaces(Model& m, const MeshSettingsSet& s, bool selfWeight = false, int caseId = 0)
{
    SurfRun r;
    const auto g = MeshGeometry::extract(m);
    r.mesh = MeshGenerator::generate(g, s);
    TSA::Analysis::AnalysisEngineRegistry reg;
    TSA::Analysis::registerBuiltInEngines(reg);
    r.compat = OpenSeesMeshAdapter::check(r.mesh.mesh, g, "OpenSees", reg.engine("opensees")->capabilities(), 3000000, &m);
    GridManager gm;
    TSA::Analysis::AnalysisManager mgr(reg);
    TSA::Analysis::AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.common.includeSelfWeight = selfWeight;
    ctx.common.supportRestraints = TSA::Analysis::SupportRestraintMode::None;
    if (caseId) ctx.loadCaseIds = { caseId };
    ctx.memberSubdivisions = r.compat.memberSubdivisions;
    ctx.surfaces = r.compat.surfaces;
    ctx.surfaces.meshStamp = "maillage-essai";
    r.prepared = mgr.prepare(m, &gm, ctx);
    if (!r.compat.blocking() && r.prepared.canRun()) r.run = mgr.run(ctx, r.prepared);
    return r;
}

std::string blockingText(const SolverCompatibility& c)
{
    std::string s;
    for (const auto& i : c.items)
        if (i.level == SolverCheckItem::Level::Blocking) s += i.message + " | ";
    return s;
}

std::string errorsOf(const TSA::Analysis::PreparedAnalysis& p)
{
    std::string s;
    for (const auto& e : p.validation.texts(TSA::Analysis::ValidationSeverity::Error)) s += e + " | ";
    return s;
}

/// Bande en console : dalle L × b encastrée sur le côté x = 0 (nœuds TSA tous les `step` m sur ce côté, appuis encastrés).
int cantilever(Model& m, double L, double b, double h, double step)
{
    std::vector<int> ids;
    const int n = int(std::lround(b / step));
    // contour : (0,0) → (L,0) → (L,b) → (0,b) puis le côté encastré de y = b à y = 0 (nœuds intermédiaires)
    ids.push_back(m.addNode(0, 0, 0));
    ids.push_back(m.addNode(L, 0, 0));
    ids.push_back(m.addNode(L, b, 0));
    for (int k = n; k >= 1; --k) ids.push_back(m.addNode(0, b * k / n, 0));
    for (int id : ids)
        if (std::abs(m.getNode(id)->x()) < 1e-12) m.getNode(id)->setSupport(SupportDefinition::fixed());
    const int s = m.addSlab(ids, h, "Bande");
    Material mat = Material::concreteC25_30();
    mat.nu = 0.0;
    mat.syncMechanical();
    m.getSlab(s)->setMaterial(mat);
    return s;
}

double maxSurfaceUz(const AnalysisRunResult& r)
{
    double w = 0;
    for (const auto& [id, d] : r.results.surfaceNodeDisplacements()) w = std::max(w, std::abs(d.uz));
    return w;
}
} // namespace

bool runSuite_Surfaces(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 354 : plaque console — théorie des poutres (ν = 0), convergence, équilibre
    // -------------------------------------------------------------------------
    {
        const double L = 4.0, b = 1.0, h = 0.20, q = 10.0;
        double errQuadFine = 1, errQuadCoarse = 1, errTri = 1;
        double wRef = 0;
        for (int pass = 0; pass < 3; ++pass)
        {
            Model m;
            const double step = pass == 0 ? 0.5 : 0.25;
            const int s = cantilever(m, L, b, h, step / 2);   // nœuds d'appui au demi-pas : aucun nœud de maillage libre sur l'encastrement
            const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
            TSA::Model::SurfaceLoad sl;
            sl.surfaceId = s;
            sl.loadCaseId = lc;
            sl.value = q;
            m.loadManager().addSurfaceLoad(sl);
            const auto r = runSurfaces(m, settings2D(step, pass != 2), false, lc);
            TEST_CHECK(!r.compat.blocking() && r.prepared.canRun() && r.run.success,
                       "Test 354: calcul de la bande en console (" << blockingText(r.compat) << errorsOf(r.prepared) << r.run.message << ")");
            const double E = m.getSlab(s)->material().mechanical.youngModulus / 1000.0, I = b * h * h * h / 12.0;
            wRef = q * b * std::pow(L, 4) / (8 * E * I);
            const double w = maxSurfaceUz(r.run);
            const double err = std::abs(w - wRef) / wRef;
            const auto eq = r.run.results.equilibrium();
            std::cout << "  bande " << (pass == 2 ? "triangles" : "quadrangles") << " h = " << step << " m : " << r.compat.surfaces.elements.size()
                      << " éléments, flèche " << w * 1000 << " mm (poutre " << wRef * 1000 << " mm, écart " << err * 100 << " %), Σ Rz " << eq.reactionFz
                      << " kN pour " << q * L * b << " kN" << std::endl;
            TEST_CHECK(std::abs(eq.reactionFz - q * L * b) < 1e-6 * q * L * b && eq.isBalanced(1e-6), "Test 354: Σ réactions = q·L·b");
            (pass == 0 ? errQuadCoarse : pass == 1 ? errQuadFine : errTri) = err;
            {
                int edge = 0, edgeTsa = 0;
                for (const auto& n : r.compat.surfaces.nodes)
                    if (std::abs(n.x) < 1e-9) ++edge, edgeTsa += n.tsaNode > 0;
                TEST_CHECK(edge > 0 && edge == edgeTsa, "Test 354: tous les nœuds du bord encastré sont des nœuds d'appui TSA (" << edgeTsa << " / " << edge << ")");
            }
            if (pass == 1)
            {
                double mClamp = 0;
                // Moment d'encastrement : somme des réactions Mx … autour de y (My global) aux nœuds encastrés
                for (const auto& [id, rr] : r.run.results.allReactions()) mClamp += rr.my;
                const double mRef = q * b * L * L / 2;
                TEST_CHECK(std::abs(std::abs(mClamp) - mRef) / mRef < 1e-6, "Test 354: moment d'encastrement qbL²/2 = " << mRef << " kN·m (" << mClamp << ")");
                double mxx = 0;
                for (const auto& [id, sr] : r.run.results.shellResults()) mxx = std::max(mxx, std::abs(sr.mean[3]));
                std::cout << "  moment Mxx maximal (moyenne des points de Gauss d'un élément) : " << mxx << " kN·m/m (qL²/2 = " << q * L * L / 2 << ")" << std::endl;
                TEST_CHECK(!r.run.results.shellResults().empty() && mxx > 0.8 * q * L * L / 2 && mxx < q * L * L / 2 * 1.02,
                           "Test 354: résultantes de coque lues (Mxx près de l'encastrement)");
                // Report sur le maillage : déplacements exacts aux nœuds des coques, résultantes par élément ; refusé sur un autre maillage
                const auto set = ResultsModelMeshAdapter(m, r.run.results, "maillage-essai").extract(r.mesh.mesh);
                const auto* u = set.find("Déplacement");
                const auto* mp = set.find("Moment principal (coque)");
                std::size_t covered = 0;
                for (const auto& n : r.compat.surfaces.nodes) covered += u && u->has(n.meshNode);
                TEST_CHECK(u && covered == r.compat.surfaces.nodes.size() && mp && mp->values.size() == r.compat.surfaces.elements.size() &&
                               std::max(std::abs(mp->range().first), std::abs(mp->range().second)) > 0.8 * q * L * L / 2,
                           "Test 354: champs sur le maillage (déplacements de " << covered << " nœuds de coque, moment principal sur " << (mp ? mp->values.size() : 0) << " éléments)");
                const auto other = ResultsModelMeshAdapter(m, r.run.results, "autre-maillage").extract(r.mesh.mesh);
                TEST_CHECK(!other.find("Moment principal (coque)") && !other.notes.empty(), "Test 354: résultats des coques non reportés sur un maillage différent");
            }
        }
        TEST_CHECK(errQuadFine < 0.03 && errTri < 0.05, "Test 354: flèche à moins de 3 % (quadrangles) et 5 % (triangles) de qL⁴/8EI");
        TEST_CHECK(errQuadFine <= errQuadCoarse + 1e-9, "Test 354: convergence (écart " << errQuadCoarse * 100 << " % → " << errQuadFine * 100 << " %)");
        std::cout << "[PASS] Test 354: Plaque console — théorie des poutres, convergence, équilibre" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 355 : ouvertures — aire nette, poids propre, charges uniforme et ponctuelle, voile ouvert
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(6, 4, 0), d = m.addNode(0, 4, 0);
        for (int id : { a, b, c, d }) m.getNode(id)->setSupport(SupportDefinition::fixed());
        const int s = m.addSlab({ a, b, c, d }, 0.2, "Dalle");
        SlabOpening o;
        o.name = "trémie";
        o.polygon = { { 2.5, 1.5 }, { 3.5, 1.5 }, { 3.5, 2.5 }, { 2.5, 2.5 } };
        m.getSlab(s)->setOpenings({ o });
        TEST_CHECK(std::abs(m.getSlab(s)->area(m) - 23.0) < 1e-9, "Test 355: aire nette de la dalle 23 m²");
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = 2.0;
        m.loadManager().addSurfaceLoad(sl);
        TSA::Model::SurfaceLoad pl;
        pl.surfaceId = s;
        pl.loadCaseId = lc;
        pl.distribution = TSA::Model::SurfaceLoadDistribution::Point;
        pl.value = 15.0;
        pl.u = 1.0;
        pl.v = 1.0;
        m.loadManager().addSurfaceLoad(pl);
        const auto r = runSurfaces(m, settings2D(0.4, false), true, lc);
        TEST_CHECK(!r.compat.blocking() && r.run.success, "Test 355: calcul de la dalle trouée (" << blockingText(r.compat) << errorsOf(r.prepared) << ")");
        bool inHole = false;
        for (const auto& e : r.compat.surfaces.elements)
        {
            double cx = 0, cy = 0;
            for (int n : e.meshNodes) cx += r.mesh.mesh.node(n).x / e.meshNodes.size(), cy += r.mesh.mesh.node(n).y / e.meshNodes.size();
            inHole |= cx > 2.5 && cx < 3.5 && cy > 1.5 && cy < 2.5;
        }
        TEST_CHECK(!inHole, "Test 355: aucun élément de coque dans l'ouverture");
        const double rho = m.getSlab(s)->material().mechanical.density;
        const double sw = rho * 9.81 * 0.2 / 1000.0 * 23.0;
        const double expected = sw + 2.0 * 23.0 + 15.0;
        const auto eq = r.run.results.equilibrium();
        std::cout << "  dalle trouée : poids propre " << sw << " kN (aire nette 23 m²), Σ Rz " << eq.reactionFz << " kN, attendu " << expected << " kN" << std::endl;
        TEST_CHECK(std::abs(eq.reactionFz - expected) < 1e-6 * expected && std::abs(r.compat.surfaces.selfWeightBySurface.at("SL" + std::to_string(s)) - sw) < 1e-6 * sw,
                   "Test 355: Σ réactions = poids propre (aire nette) + q·A nette + P");

        // Charge ponctuelle dans l'ouverture : refusée
        TSA::Model::SurfaceLoad bad = pl;
        bad.u = 3.0;
        bad.v = 2.0;
        const int badId = m.loadManager().addSurfaceLoad(bad);
        const auto r2 = runSurfaces(m, settings2D(0.4, false), true, lc);
        TEST_CHECK(r2.compat.blocking() && blockingText(r2.compat).find("ouverture") != std::string::npos,
                   "Test 355: charge ponctuelle dans l'ouverture refusée (" << blockingText(r2.compat) << ")");
        m.loadManager().removeSurfaceLoad(badId);

        // Voile avec porte (posée au pied : encoche du contour) et fenêtre (trou) : ouvertures maillées, aire nette
        Model w;
        const int p = w.addNode(0, 0, 0), q = w.addNode(5, 0, 0);
        const int wid = w.addWall(p, q, 3.0, 0.2, "Voile");
        SlabOpening door, window, outside;
        door.name = "porte";
        door.polygon = { { 1.0, 0.0 }, { 2.0, 0.0 }, { 2.0, 2.1 }, { 1.0, 2.1 } };
        window.name = "fenêtre";
        window.polygon = { { 3.0, 1.0 }, { 4.2, 1.0 }, { 4.2, 2.0 }, { 3.0, 2.0 } };
        outside.polygon = { { 1.0, -0.2 }, { 2.0, -0.2 }, { 2.0, 2.1 }, { 1.0, 2.1 } };
        TEST_CHECK(!validateWallOpening(w, *w.getWall(wid), outside.polygon).empty(), "Test 355: ouverture dépassant le voile refusée");
        TEST_CHECK(validateWallOpening(w, *w.getWall(wid), door.polygon).empty(), "Test 355: porte posée au pied du voile acceptée");
        w.getWall(wid)->setOpenings({ door });
        TEST_CHECK(validateWallOpening(w, *w.getWall(wid), window.polygon).empty(), "Test 355: fenêtre acceptée à côté de la porte");
        TEST_CHECK(!validateWallOpening(w, *w.getWall(wid), { { 1.5, 0.0 }, { 2.5, 0.0 }, { 2.5, 2.0 }, { 1.5, 2.0 } }).empty(), "Test 355: porte chevauchant la porte refusée");
        w.getWall(wid)->setOpenings({ door, window });
        const auto gw = MeshGeometry::extract(w);
        const auto mw = MeshGenerator::generate(gw, settings2D(0.25, false));
        double area = 0;
        bool inside = false;
        for (const auto& e : mw.mesh.elements)
        {
            if (feTypeDimension(e.type) != 2 || e.boundary) continue;
            const auto &n0 = mw.mesh.node(e.nodes[0]), &n1 = mw.mesh.node(e.nodes[1]), &n2 = mw.mesh.node(e.nodes[2]);
            const P3 u { n1.x - n0.x, n1.y - n0.y, n1.z - n0.z }, v { n2.x - n0.x, n2.y - n0.y, n2.z - n0.z };
            area += 0.5 * u.cross(v).norm();
            const double cx = (n0.x + n1.x + n2.x) / 3, cz = (n0.z + n1.z + n2.z) / 3;
            inside |= (cx > 1.0 && cx < 2.0 && cz < 2.1) || (cx > 3.0 && cx < 4.2 && cz > 1.0 && cz < 2.0);
        }
        const double net = 15.0 - 2.1 - 1.2;
        TEST_CHECK(mw.ok && std::abs(area - net) < 1e-6 && !inside && std::abs(w.getWall(wid)->area(w) - net) < 1e-9 && std::abs(gw.surfaces[0].area - net) < 1e-9,
                   "Test 355: voile 5 × 3 avec porte 1 × 2,1 et fenêtre 1,2 × 1 : aire maillée " << area << " m² = " << net << " m², aucun élément dans les ouvertures");
        std::cout << "[PASS] Test 355: Ouvertures (dalle, voile), poids propre, charges surfaciques" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 356 : dalle sur poutres et poteaux ; voile de contreventement
    // -------------------------------------------------------------------------
    {
        Model m;
        const double H = 3.0;
        const int b1 = m.addNode(0, 0, 0), b2 = m.addNode(6, 0, 0), b3 = m.addNode(6, 4, 0), b4 = m.addNode(0, 4, 0);
        const int t1 = m.addNode(0, 0, H), t2 = m.addNode(6, 0, H), t3 = m.addNode(6, 4, H), t4 = m.addNode(0, 4, H);
        for (int id : { b1, b2, b3, b4 }) m.getNode(id)->setSupport(SupportDefinition::fixed());
        const Material c = Material::concreteC30_37();
        for (auto [lo, hi] : { std::pair { b1, t1 }, { b2, t2 }, { b3, t3 }, { b4, t4 } }) m.addColumn(lo, hi, Section::rectangular(0.4, 0.4), c, 0.0, "Poteau");
        std::vector<int> beams;
        for (auto [i, j] : { std::pair { t1, t2 }, { t2, t3 }, { t3, t4 }, { t4, t1 } })
            beams.push_back(m.addBar(i, j, Section::rectangular(0.3, 0.6), c, BarRole::Beam, 0.0, "Poutre"));
        const int s = m.addSlab({ t1, t2, t3, t4 }, 0.2, "Dalle");
        m.getSlab(s)->setMaterial(c);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = 5.0;
        m.loadManager().addSurfaceLoad(sl);
        const auto r = runSurfaces(m, settings2D(0.5, true), true, lc);
        TEST_CHECK(!r.compat.blocking() && r.run.success, "Test 356: dalle sur poutres calculée (" << blockingText(r.compat) << errorsOf(r.prepared) << ")");
        // Nœuds partagés : chaque nœud de coque situé sur une poutre est un nœud du calcul de la poutre (aucun doublon)
        const auto& map = r.prepared.model.surfaceNodes;
        std::size_t onBeams = 0, shared = 0;
        for (const auto& n : r.compat.surfaces.nodes)
        {
            const bool edge = std::abs(n.x) < 1e-9 || std::abs(n.x - 6) < 1e-9 || std::abs(n.y) < 1e-9 || std::abs(n.y - 4) < 1e-9;
            if (!edge) continue;
            ++onBeams;
            shared += map.surfaceOnlyNodes.count(map.meshToNode.at(n.meshNode)) == 0;
        }
        TEST_CHECK(onBeams > 8 && shared == onBeams, "Test 356: " << shared << " / " << onBeams << " nœuds de bord de dalle partagés avec les poutres");
        double wSlab = 0, wBeams = 0;
        for (const auto& [mat, sec, L] : { std::tuple { c, Section::rectangular(0.4, 0.4), H } }) wBeams += 4 * mat.mechanical.density * 9.81 / 1000.0 * sec.area() * L;
        wBeams += 2 * (6 + 4) * c.mechanical.density * 9.81 / 1000.0 * Section::rectangular(0.3, 0.6).area();
        wSlab = c.mechanical.density * 9.81 / 1000.0 * 0.2 * 24.0;
        const double expected = wBeams + wSlab + 5.0 * 24.0;
        const auto eq = r.run.results.equilibrium();
        std::cout << "  dalle sur poutres : Σ Rz " << eq.reactionFz << " kN, attendu " << expected << " kN (poteaux et poutres " << wBeams << ", dalle " << wSlab
                  << ", charge " << 120.0 << ")" << std::endl;
        TEST_CHECK(std::abs(eq.reactionFz - expected) < 1e-6 * expected, "Test 356: Σ réactions = charges + poids propre des barres et de la dalle");
        double mBeam = 0;
        for (int id : beams)
            if (const auto* er = r.run.results.getElementResults(TSA::Analysis::StructuralElementKind::Beam, id))
                for (const auto& st : er->intermediateStations) mBeam = std::max(mBeam, std::abs(st.My) + std::abs(st.Mz));
        int wn = 0;
        const double wMax = r.run.results.maxSurfaceDisplacement(&wn);
        std::cout << "  moment maximal dans les poutres de rive " << mBeam << " kN·m, déplacement maximal de la dalle " << wMax * 1000 << " mm" << std::endl;
        TEST_CHECK(mBeam > 1.0 && wMax > 0 && wn > 0, "Test 356: la dalle charge les poutres (flexion des poutres de rive) et se déforme");
        // Note de calcul détaillée : hypothèses des coques, charges surfaciques, résultantes, contrôle de la charge verticale
        {
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&m);
            mgr.setResultsModel(std::make_shared<TSA::Analysis::ResultsModel>(r.run.results));
            mgr.applyTemplate(TSA::NDC::ReportTemplateType::BureauEtudes);
            const auto doc = mgr.generateReport();
            const QString html = TSA::NDC::NDCExporter::renderHtml(doc);
            TEST_CHECK(html.contains("Surfaces calculées en coques") && html.contains("Charges surfaciques") && html.contains("Résultantes des coques") &&
                           html.contains("CONCORDANT"),
                       "Test 356: note de calcul — hypothèses des coques, charges surfaciques, résultantes, charge verticale concordante");
        }

        // Voile de contreventement en console : déplacement en tête sous un effort horizontal (flexion + cisaillement)
        Model v;
        const double Lw = 4.0, Hw = 6.0, t = 0.25, F = 200.0;
        std::vector<int> base;
        for (int k = 0; k <= 8; ++k) base.push_back(v.addNode(Lw * k / 8, 0, 0));
        for (int id : base) v.getNode(id)->setSupport(SupportDefinition::fixed());
        const int wl = v.addWall(base.front(), base.back(), Hw, t, "Voile");
        Material cv = Material::concreteC30_37();
        cv.nu = 0.2;
        cv.syncMechanical();
        v.getWall(wl)->setMaterial(cv);
        const int lw = v.loadManager().addLoadCase(LoadCase(0, "W", LoadCaseCategory::Wind));
        TSA::Model::SurfaceLoad top;
        top.targetKind = TSA::Model::SurfaceTargetKind::Wall;
        top.surfaceId = wl;
        top.loadCaseId = lw;
        top.distribution = TSA::Model::SurfaceLoadDistribution::Point;
        top.direction = TSA::Model::SurfaceLoadDirection::GlobalX;
        top.value = F;
        top.u = Lw / 2;
        top.v = Hw - 1e-6;
        v.loadManager().addSurfaceLoad(top);
        const auto rv = runSurfaces(v, settings2D(0.5, true), false, lw);
        TEST_CHECK(!rv.compat.blocking() && rv.run.success, "Test 356: voile calculé (" << blockingText(rv.compat) << errorsOf(rv.prepared) << ")");
        double ux = 0;
        for (const auto& [id, d] : rv.run.results.surfaceNodeDisplacements()) ux = std::max(ux, std::abs(d.ux));
        const double E = cv.mechanical.youngModulus / 1000.0, G = E / (2 * (1 + 0.2)), I = t * Lw * Lw * Lw / 12.0, A = t * Lw;
        const double uRef = F * Hw * Hw * Hw / (3 * E * I) + 1.2 * F * Hw / (G * A);
        std::cout << "  voile en console : déplacement en tête " << ux * 1000 << " mm (Timoshenko " << uRef * 1000 << " mm, écart "
                  << std::abs(ux - uRef) / uRef * 100 << " %)" << std::endl;
        TEST_CHECK(std::abs(ux - uRef) / uRef < 0.10, "Test 356: voile en console à moins de 10 % de la théorie de Timoshenko (encastrement par nœuds, effort concentré)");
        std::cout << "[PASS] Test 356: Dalle sur poutres et poteaux, voile de contreventement" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 357 : formulations non prises en charge, surface non structurale, charges non transmissibles
    // -------------------------------------------------------------------------
    {
        Model m;
        const int s = cantilever(m, 3.0, 1.0, 0.2, 0.5);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::Plate);
        auto r = runSurfaces(m, settings2D(0.5, true));
        TEST_CHECK(r.compat.blocking() && blockingText(r.compat).find("plaque seule") != std::string::npos && r.compat.surfaces.empty(),
                   "Test 357: formulation « plaque » refusée, sans substitution");
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::Membrane);
        r = runSurfaces(m, settings2D(0.5, true));
        TEST_CHECK(r.compat.blocking() && blockingText(r.compat).find("membrane seule") != std::string::npos, "Test 357: formulation « membrane » refusée");
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::NonStructural);
        r = runSurfaces(m, settings2D(0.5, true));
        TEST_CHECK(!r.compat.blocking() && r.compat.surfaces.empty(), "Test 357: surface non structurale non transmise (avertissement)");
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = 3.0;
        m.loadManager().addSurfaceLoad(sl);
        r = runSurfaces(m, settings2D(0.5, true));
        TEST_CHECK(r.compat.blocking() && blockingText(r.compat).find("non transmise") != std::string::npos, "Test 357: charge sur une surface non transmise refusée");
        // Dalle isolée (aucun appui, aucune barre, aucune autre surface) : refusée, aucune liaison rigide inventée
        {
            Model iso;
            const int a = iso.addNode(0, 0, 3), b = iso.addNode(4, 0, 3), c = iso.addNode(4, 3, 3), d = iso.addNode(0, 3, 3);
            iso.addSlab({ a, b, c, d }, 0.2, "Isolée");
            const auto ri = runSurfaces(iso, settings2D(0.5, true));
            TEST_CHECK(ri.compat.blocking() && blockingText(ri.compat).find("mécanisme") != std::string::npos, "Test 357: surface non connectée refusée (" << blockingText(ri.compat) << ")");
        }
        // Charge surfacique sans maillage : calcul refusé (jamais ignorée en silence)
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::Shell);
        GridManager gm;
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        TSA::Analysis::AnalysisManager mgr(reg);
        TSA::Analysis::AnalysisContext ctx;
        ctx.engineId = "opensees";
        const auto p = mgr.prepare(m, &gm, ctx);
        TEST_CHECK(!p.canRun() && errorsOf(p).find("charge(s) surfacique(s)") != std::string::npos, "Test 357: charge surfacique sans maillage : calcul refusé");
        // Suppression de la dalle : ses charges disparaissent
        m.removeSlab(s);
        TEST_CHECK(m.loadManager().surfaceLoads().empty(), "Test 357: charges supprimées avec leur surface");
        std::cout << "[PASS] Test 357: Formulations et charges non transmissibles refusées" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 358 : persistance (formulations, ouvertures des voiles, charges surfaciques)
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(5, 4, 0), d = m.addNode(0, 4, 0);
        const int s = m.addSlab({ a, b, c, d }, 0.22, "Dalle");
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::NonStructural);
        const int w = m.addWall(a, b, 3.0, 0.2, "Voile");
        SlabOpening win;
        win.name = "fenêtre";
        win.shape = "rectangle";
        win.polygon = { { 1, 1 }, { 2.2, 1 }, { 2.2, 2.2 }, { 1, 2.2 } };
        m.getWall(w)->setOpenings({ win });
        m.getWall(w)->setFormulation(TSA::Model::SurfaceFormulation::Membrane);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "W", LoadCaseCategory::Wind));
        TSA::Model::SurfaceLoad sl;
        sl.targetKind = TSA::Model::SurfaceTargetKind::Wall;
        sl.surfaceId = w;
        sl.loadCaseId = lc;
        sl.name = "Vent";
        sl.direction = TSA::Model::SurfaceLoadDirection::LocalNormal;
        sl.value = 0.8;
        const int lid = m.loadManager().addSurfaceLoad(sl);
        TSA::Model::SurfaceLoad pt;
        pt.surfaceId = s;
        pt.loadCaseId = lc;
        pt.distribution = TSA::Model::SurfaceLoadDistribution::Point;
        pt.value = 12;
        pt.u = 2;
        pt.v = 1.5;
        pt.projected = true;
        const int pid = m.loadManager().addSurfaceLoad(pt);
        QTemporaryDir tmp;
        const std::string path = (tmp.path() + "/surfaces.tsa").toStdString();
        std::string err;
        TSA::IO::TSAFileWriter writer;
        GridManager grids;
        TEST_CHECK(writer.saveToFile(path, m, &grids, "Surfaces", "TSA", &err), "Test 358: enregistrement (" << err << ")");
        Model back;
        GridManager bg;
        std::string name;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, &bg, "", &name, nullptr, nullptr, &err), "Test 358: relecture (" << err << ")");
        const auto* bs = back.getSlab(s);
        const auto* bw = back.getWall(w);
        const auto* bl = back.loadManager().getSurfaceLoad(lid);
        const auto* bp = back.loadManager().getSurfaceLoad(pid);
        TEST_CHECK(bs && bw && bs->formulation() == TSA::Model::SurfaceFormulation::NonStructural && bw->formulation() == TSA::Model::SurfaceFormulation::Membrane,
                   "Test 358: formulations relues");
        TEST_CHECK(bw && bw->openings().size() == 1 && bw->openings()[0].name == "fenêtre" && bw->openings()[0].polygon == win.polygon, "Test 358: ouverture du voile relue");
        TEST_CHECK(bl && bp && *bl == *m.loadManager().getSurfaceLoad(lid) && *bp == *m.loadManager().getSurfaceLoad(pid), "Test 358: charges surfaciques relues à l'identique");
        // Ancien projet (sans chunk SURF) : surfaces en coque, aucune charge surfacique (lecture du modèle par défaut)
        Model fresh;
        TEST_CHECK(Slab().formulation() == TSA::Model::SurfaceFormulation::Shell && fresh.loadManager().surfaceLoads().empty(), "Test 358: valeurs par défaut (coque)");
        std::cout << "[PASS] Test 358: Persistance des surfaces et de leurs charges" << std::endl;
        ++passed;
    }
    return true;
}
