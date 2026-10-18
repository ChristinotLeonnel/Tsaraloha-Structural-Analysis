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
//   358  persistance : formulations, ouvertures des voiles, charges surfaciques ; validation des ouvertures de voile ;
//   359  analyse modale (essai F) : fréquence propre d'une bande de coque en console comparée à la théorie des poutres,
//        masse totale = ρ·e·A, masses converties depuis un cas de charge (m = q·A/g), participation, formes propres par
//        mode (nœuds du maillage), paramètres modaux enregistrés avec le contexte ;
//   360  appuis linéiques de bord : plaque carrée simplement appuyée sur ses 4 bords (Navier), appuis élastiques (raideur
//        linéique répartie, tassement), pied de voile interrompu par une porte, bords inexistants, surface non transmise et
//        conditions redondantes signalés, persistance ;
//   361  charges sur une zone localisée (rectangle du repère local, découpe exacte des éléments et des ouvertures) et à
//        variation linéaire (pression hydrostatique sur un voile : résultante γ·h²/2·L, point d'application à h/3) ;
//   362  unités (essai G) : même modèle (coques, poids propre, charge surfacique, appuis linéiques élastiques, masses) calculé
//        en kN·m et en N·m : déplacements et fréquences identiques, réactions et masses dans le rapport 1000, équilibre ;
//   363  robustesse et performance : plancher de 24 dalles sur poutres et poteaux (≈ 1500 coques), temps de maillage et de
//        calcul, équilibre global ; invalidation : ouverture → remaillage, charge ou appui linéique → maillage conservé ;
//   364  API interne des surfaces (sans interface) : description, capacités du moteur, modifications validées, export JSON
//        versionné ; IFC : élément surfacique analytique avec ouvertures (IfcFaceBound) et type selon la formulation.

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "Api/SurfaceApi.h"
#include "BIM/IFC/IfcExporter.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Mesh/MeshGenerator.h"
#include "Mesh/MeshGeometry.h"
#include "Mesh/MeshProject.h"
#include "Mesh/MeshResultField.h"
#include "Mesh/Solvers/OpenSeesMeshAdapter.h"
#include "Model/Load/LoadManager.h"
#include "NDC/NDCExporter.h"
#include "NDC/ReportManager.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <functional>

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

SurfRun runSurfaces(Model& m, const MeshSettingsSet& s, bool selfWeight = false, int caseId = 0,
                    const std::function<void(TSA::Analysis::AnalysisContext&)>& tune = {})
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
    if (tune) tune(ctx);
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
        const auto& msum = r.run.results.surfaceMeshSummary();
        TEST_CHECK(msum.quads == r.compat.surfaces.elements.size() && msum.minAngle > 45.0 && msum.maxAspect < 2.5 && std::abs(msum.maxEdge - 0.5) < 0.05,
                   "Test 356: résumé du maillage transmis aux résultats (" << msum.quads << " quadrangles, angle min " << msum.minAngle << "°)");
        const auto sum = r.run.results.summary();
        TEST_CHECK(m.getNode(sum.maxDisplacementNodeId) != nullptr, "Test 356: synthèse des déplacements sur les nœuds TSA (nœud " << sum.maxDisplacementNodeId << "), surfaces à part");
        // Note de calcul détaillée : hypothèses des coques, charges surfaciques, résultantes, contrôle de la charge verticale
        {
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&m);
            mgr.setResultsModel(std::make_shared<TSA::Analysis::ResultsModel>(r.run.results));
            mgr.applyTemplate(TSA::NDC::ReportTemplateType::BureauEtudes);
            const auto doc = mgr.generateReport();
            const QString html = TSA::NDC::NDCExporter::renderHtml(doc);
            TEST_CHECK(html.contains("Surfaces calculées en coques") && html.contains("Charges surfaciques") && html.contains("Résultantes des coques") &&
                           html.contains("CONCORDANT") && html.contains("Maillage calculé") && html.contains("ShellMITC4") && html.contains("Origine des valeurs"),
                       "Test 356: note de calcul — hypothèses des coques, charges surfaciques, résultantes, charge verticale concordante");
        }

        // Projet d'exemple (TSA_DEMO_DIR) : dalle sur poutres, charge surfacique et maillage enregistrés dans le .tsa
        if (const QString dir = qEnvironmentVariable("TSA_DEMO_DIR"); !dir.isEmpty())
        {
            MeshProject mp;
            mp.settings = settings2D(0.5, true);
            mp.hasMesh = true;
            mp.mesh = r.mesh.mesh;
            mp.record = r.mesh.record;
            m.setMeshProjectJson(QJsonDocument(mp.toJson()).toJson(QJsonDocument::Compact).toStdString());
            TSA::IO::TSAFileWriter writer;
            GridManager grids;
            std::string err;
            const std::string path = (dir + "/TSA_Exemple_Dalle_Coques.tsa").toStdString();
            TEST_CHECK(writer.saveToFile(path, m, &grids, "Dalle sur poutres — coques", "TSA — exemples", &err), "Test 356: projet d'exemple écrit (" << err << ")");
            std::cout << "  projet d'exemple : " << path << std::endl;
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
    // -------------------------------------------------------------------------
    // TEST 359 : analyse modale (essai F) — bande de coque en console
    // -------------------------------------------------------------------------
    {
        const double L = 4.0, b = 1.0, h = 0.20, rho = 2.5;   // t/m³ (calcul en kN, m, s)
        Model m;
        const int s = cantilever(m, L, b, h, 0.125);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G2", LoadCaseCategory::Dead));
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = 10.0;   // kN/m²
        m.loadManager().addSurfaceLoad(sl);
        const double E = m.getSlab(s)->material().E * 1e-3;   // kPa
        const double EI = E * b * h * h * h / 12.0, mu = rho * b * h;
        const double f1 = 1.875104 * 1.875104 / (2.0 * 3.14159265358979 * L * L) * std::sqrt(EI / mu);
        auto modal = [&](std::vector<int> massCases) {
            return runSurfaces(m, settings2D(0.25, true), false, 0, [&](TSA::Analysis::AnalysisContext& c) {
                c.type = TSA::Analysis::AnalysisType::Modal;
                c.common.modeCount = 4;
                c.common.modalMassCaseIds = massCases;
            });
        };
        const auto r = modal({});
        TEST_CHECK(!r.compat.blocking() && r.prepared.canRun() && r.run.success,
                   "Test 359: calcul modal (" << blockingText(r.compat) << errorsOf(r.prepared) << r.run.message << ")");
        const auto& md = r.run.results.modal();
        TEST_CHECK(md.modes.size() == 4 && r.run.results.stepCount() == 4 && r.run.results.analysisType() == TSA::Analysis::AnalysisType::Modal,
                   "Test 359: 4 modes lus (" << md.modes.size() << ")");
        const double err = std::abs(md.modes[0].frequency - f1) / f1;
        TEST_CHECK(err < 0.02, "Test 359: f1 = " << md.modes[0].frequency << " Hz, théorie " << f1 << " Hz (écart " << err * 100 << " %)");
        TEST_CHECK(std::abs(md.modes[0].period * md.modes[0].frequency - 1.0) < 1e-9 &&
                       std::abs(md.modes[0].omega * md.modes[0].omega - md.modes[0].eigenvalue) < 1e-6 * md.modes[0].eigenvalue,
                   "Test 359: T = 1/f, ω² = λ");
        TEST_CHECK(md.modes[1].frequency >= md.modes[0].frequency && md.modes[3].frequency >= md.modes[2].frequency, "Test 359: modes triés");
        const double mass = rho * L * b * h;
        TEST_CHECK(std::abs(md.totalMass[2] - mass) < 1e-6 * mass && std::abs(md.totalMass[0] - mass) < 1e-6 * mass,
                   "Test 359: masse totale " << md.totalMass[2] << " t = ρ·e·A = " << mass << " t");
        TEST_CHECK(md.totalFreeMass[2] < md.totalMass[2] && md.totalFreeMass[2] > 0.9 * mass, "Test 359: masse libre (hors nœuds encastrés)");
        // Participation verticale du 1er mode : ≈ 61 % pour une console continue (rapportée à la masse libre)
        TEST_CHECK(md.modes[0].massRatio[2] > 55.0 && md.modes[0].massRatio[2] < 70.0 && md.modes[0].cumulativeRatio[2] <= 100.0 + 1e-6,
                   "Test 359: participation MZ du mode 1 = " << md.modes[0].massRatio[2] << " %");
        // Forme propre : maillage des surfaces, normalisée (translation max = 1), maximum en bout de console
        const auto& st = r.run.results.allStepResults()[0];
        double best = 0, xBest = 0;
        for (const auto& [mesh, d] : st.surfaceDisplacements)
            if (std::abs(d.uz) > best) best = std::abs(d.uz), xBest = r.mesh.mesh.node(mesh).x;
        TEST_CHECK(!st.surfaceDisplacements.empty() && std::abs(best - 1.0) < 1e-9 && std::abs(xBest - L) < 1e-6,
                   "Test 359: forme propre 1 (max " << best << " en x = " << xBest << ")");
        TSA::Analysis::ResultsModel copy = r.run.results;
        copy.setActiveStep(1);
        TEST_CHECK(copy.surfaceNodeDisplacements().size() == st.surfaceDisplacements.size(), "Test 359: parcours des modes (surfaces)");

        // Masses converties depuis le cas G2 : m = q·A/g ; masse uniforme → f réduite de √(m0 / (m0 + mq))
        const auto r2 = modal({ lc });
        TEST_CHECK(r2.run.success, "Test 359: calcul modal avec masses converties (" << r2.run.message << ")");
        const auto& md2 = r2.run.results.modal();
        const double mq = 10.0 * L * b / 9.81;
        TEST_CHECK(!md2.empty() && std::abs(md2.totalMass[2] - (mass + mq)) < 1e-6 * (mass + mq), "Test 359: masse totale avec G2 = " << (md2.empty() ? 0.0 : md2.totalMass[2]) << " t (attendu " << mass + mq << ")");
        const double f1b = md.modes[0].frequency * std::sqrt(mass / (mass + mq));
        TEST_CHECK(!md2.empty() && std::abs(md2.modes[0].frequency - f1b) / f1b < 0.005,
                   "Test 359: f1 avec masses converties = " << (md2.empty() ? 0.0 : md2.modes[0].frequency) << " Hz (attendu " << f1b << ")");
        TEST_CHECK(md2.massSource.find("m = F/g") != std::string::npos, "Test 359: origine des masses tracée");

        // Note de calcul : chapitre modal (fréquences calculées), aucun résultat statique présenté
        {
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&m);
            mgr.setResultsModel(std::make_shared<TSA::Analysis::ResultsModel>(r.run.results));
            mgr.applyTemplate(TSA::NDC::ReportTemplateType::BureauEtudes);
            const QString html = TSA::NDC::NDCExporter::renderHtml(mgr.generateReport());
            TEST_CHECK(html.contains("Analyse modale") && html.contains(QString::number(md.modes[0].frequency, 'f', 3)) &&
                           html.contains("Aucun résultat statique") && html.contains("(K − ω² · M) · φ = 0"),
                       "Test 359: note de calcul — chapitre modal, pas de résultat statique inventé");
        }

        // Contexte : paramètres modaux enregistrés
        TSA::Analysis::AnalysisContext c;
        c.type = TSA::Analysis::AnalysisType::Modal;
        c.common.modeCount = 9;
        c.common.modalMassCaseIds = { 3, 5 };
        const auto back = TSA::Analysis::AnalysisContext::fromJson(c.toJson());
        TEST_CHECK(back.type == TSA::Analysis::AnalysisType::Modal && back.common.modeCount == 9 && back.common.modalMassCaseIds == std::vector<int>({ 3, 5 }),
                   "Test 359: contexte modal relu");
        std::cout << "[PASS] Test 359: Analyse modale des coques (f1 " << md.modes[0].frequency << " Hz / " << f1 << " Hz)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 360 : appuis linéiques sur les bords des surfaces
    // -------------------------------------------------------------------------
    {
        auto sumReactions = [](const AnalysisRunResult& r, int dof) {
            double s = 0;
            for (const auto& [id, rc] : r.results.surfaceNodeReactions()) s += dof == 0 ? rc.rx : dof == 1 ? rc.ry : rc.rz;
            for (const auto& [id, rc] : r.results.allReactions()) s += dof == 0 ? rc.rx : dof == 1 ? rc.ry : rc.rz;
            return s;
        };
        // a) Plaque carrée a × a simplement appuyée sur ses 4 bords (Tz bloqué ; Tx, Ty bloqués pour la membrane),
        //    charge uniforme : w = 0,00406 · q · a⁴ / D (Timoshenko, Navier), Σ Rz = q · a²
        const double a = 6.0, h = 0.20, q = 10.0;
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(a, 0, 0), n3 = m.addNode(a, a, 0), n4 = m.addNode(0, a, 0);
        const int s = m.addSlab({ n1, n2, n3, n4 }, h, "Plaque");
        TSA::Model::SurfaceEdgeSupport ss;
        ss.edge = -1;
        ss.fixed = { true, true, true, false, false, false };
        ss.name = "appui simple";
        m.getSlab(s)->setEdgeSupports({ ss });
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = q;
        m.loadManager().addSurfaceLoad(sl);
        const auto r = runSurfaces(m, settings2D(0.25, true), false, lc);
        TEST_CHECK(!r.compat.blocking() && r.prepared.canRun() && r.run.success,
                   "Test 360: plaque sur appuis linéiques (" << blockingText(r.compat) << errorsOf(r.prepared) << r.run.message << ")");
        const auto& mat = m.getSlab(s)->material();
        const double D = mat.E * 1e-3 * h * h * h / (12.0 * (1.0 - mat.nu * mat.nu));
        const double wRef = 0.00406 * q * a * a * a * a / D, w = maxSurfaceUz(r.run);
        TEST_CHECK(std::abs(w - wRef) / wRef < 0.05, "Test 360: flèche " << w * 1000 << " mm, Navier " << wRef * 1000 << " mm");
        TEST_CHECK(std::abs(sumReactions(r.run, 2) - q * a * a) < 1e-3 * q * a * a, "Test 360: Σ Rz = " << sumReactions(r.run, 2) << " kN (q·a² = " << q * a * a << ")");
        double perim = 0;
        for (const auto& ns : r.compat.surfaces.supports) perim += ns.length;
        TEST_CHECK(std::abs(perim - 4 * a) < 1e-6 && !r.run.results.surfaceNodeReactions().empty(), "Test 360: longueurs d'influence = périmètre (" << perim << " m)");

        // b) Appui élastique sur tout le contour : Σ kᵢ = k · périmètre ; tassement ≈ q·A / (k·périmètre) (plaque raide)
        TSA::Model::SurfaceEdgeSupport el;
        el.fixed = { true, true, false, false, false, true };
        el.stiffness = { 0, 0, 500.0, 0, 0, 0 };   // kN/m/m
        m.getSlab(s)->setEdgeSupports({ el });
        const auto re = runSurfaces(m, settings2D(0.5, true), false, lc);
        TEST_CHECK(re.run.success, "Test 360: appui élastique (" << blockingText(re.compat) << errorsOf(re.prepared) << re.run.message << ")");
        double ksum = 0;
        for (const auto& ns : re.compat.surfaces.supports) ksum += ns.stiffness[2];
        TEST_CHECK(std::abs(ksum - 500.0 * 4 * a) < 1e-6 * ksum, "Test 360: Σ kᵢ = " << ksum << " kN/m (k·périmètre = " << 500.0 * 4 * a << ")");
        TEST_CHECK(std::abs(sumReactions(re.run, 2) - q * a * a) < 1e-3 * q * a * a, "Test 360: Σ Rz (ressorts) = " << sumReactions(re.run, 2) << " kN");
        double wEdge = 1e9;
        for (const auto& ns : re.compat.surfaces.supports)
            if (const auto* d = re.run.results.surfaceNodeDisplacements().count(ns.meshNode) ? &re.run.results.surfaceNodeDisplacements().at(ns.meshNode) : nullptr)
                wEdge = std::min(wEdge, std::abs(d->uz));
        TEST_CHECK(wEdge > 0.5 * q * a * a / ksum && maxSurfaceUz(re.run) > wRef, "Test 360: tassement des bords " << wEdge * 1000 << " mm (q·A/Σk = " << q * a * a / ksum * 1000 << " mm)");

        // c) Contrôles : bord inexistant (bloquant), DDL bloqué et élastique, double blocage (redondant), appui sans effet
        TSA::Model::SurfaceEdgeSupport bad = ss;
        bad.edge = 7;
        TSA::Model::SurfaceEdgeSupport both = ss;
        both.edge = 0;
        both.stiffness[2] = 100.0;
        TSA::Model::SurfaceEdgeSupport none;
        none.edge = 1;
        m.getSlab(s)->setEdgeSupports({ ss, bad, both, none });
        const auto rc = runSurfaces(m, settings2D(0.5, true), false, lc);
        std::string warnings;
        for (const auto& i : rc.compat.items)
            if (i.level == SolverCheckItem::Level::Warning) warnings += i.message + " | ";
        TEST_CHECK(rc.compat.blocking() && blockingText(rc.compat).find("inexistant") != std::string::npos, "Test 360: bord inexistant refusé");
        TEST_CHECK(warnings.find("raideur est sans effet") != std::string::npos && warnings.find("redondante") != std::string::npos &&
                       warnings.find("sans effet") != std::string::npos,
                   "Test 360: conditions redondantes ou sans effet signalées (" << warnings << ")");
        m.getSlab(s)->setEdgeSupports({ ss });
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::NonStructural);
        m.loadManager().clear();
        const auto rn = runSurfaces(m, settings2D(0.5, true));
        TEST_CHECK(rn.compat.blocking() && blockingText(rn.compat).find("non transmise") != std::string::npos, "Test 360: appui sur une surface non transmise refusé");
        m.getSlab(s)->setFormulation(TSA::Model::SurfaceFormulation::Shell);

        // d) Voile de 5 m encastré en pied, porte de 1 m au pied : appui sur 4 m ; vent 1 kN/m² (normale), Σ Rx + q·A_nette = 0
        Model v;
        const int pa = v.addNode(0, 0, 0), pb = v.addNode(5, 0, 0);
        const int wid = v.addWall(pa, pb, 3.0, 0.2, "Voile");
        SlabOpening door;
        door.polygon = { { 1.0, 0.0 }, { 2.0, 0.0 }, { 2.0, 2.1 }, { 1.0, 2.1 } };
        v.getWall(wid)->setOpenings({ door });
        TSA::Model::SurfaceEdgeSupport base;
        base.edge = 0;
        base.fixed = { true, true, true, true, true, true };
        v.getWall(wid)->setEdgeSupports({ base });
        const int vc = v.loadManager().addLoadCase(LoadCase(0, "W", LoadCaseCategory::Wind));
        TSA::Model::SurfaceLoad wl;
        wl.targetKind = TSA::Model::SurfaceTargetKind::Wall;
        wl.surfaceId = wid;
        wl.loadCaseId = vc;
        wl.direction = TSA::Model::SurfaceLoadDirection::GlobalY;
        wl.value = 1.0;
        v.loadManager().addSurfaceLoad(wl);
        const auto rw = runSurfaces(v, settings2D(0.25, true), false, vc);
        double baseLength = 0;
        for (const auto& ns : rw.compat.surfaces.supports) baseLength += ns.length;
        TEST_CHECK(rw.run.success && std::abs(baseLength - 4.0) < 1e-6, "Test 360: pied du voile appuyé sur " << baseLength << " m (5 m − porte 1 m) " << rw.run.message);
        const double net = 5.0 * 3.0 - 1.0 * 2.1;
        TEST_CHECK(std::abs(sumReactions(rw.run, 1) + net) < 1e-3 * net, "Test 360: Σ Ry = " << sumReactions(rw.run, 1) << " kN (charge " << net << " kN)");

        // e) Persistance des appuis linéiques
        QTemporaryDir tmp;
        const std::string path = (tmp.path() + "/appuis.tsa").toStdString();
        std::string err;
        TSA::IO::TSAFileWriter writer;
        GridManager grids;
        TEST_CHECK(writer.saveToFile(path, v, &grids, "Appuis", "TSA", &err), "Test 360: enregistrement (" << err << ")");
        Model back;
        GridManager bg;
        std::string name;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, &bg, "", &name, nullptr, nullptr, &err) && back.getWall(wid) &&
                       back.getWall(wid)->edgeSupports() == v.getWall(wid)->edgeSupports(),
                   "Test 360: appuis linéiques relus (" << err << ")");
        {
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&v);
            mgr.setResultsModel(std::make_shared<TSA::Analysis::ResultsModel>(rw.run.results));
            mgr.applyTemplate(TSA::NDC::ReportTemplateType::BureauEtudes);
            const QString html = TSA::NDC::NDCExporter::renderHtml(mgr.generateReport());
            TEST_CHECK(html.contains("Appuis linéiques des bords des surfaces") && html.contains("pied") && html.contains("Tx, Ty, Tz, Rx, Ry, Rz"),
                       "Test 360: appuis linéiques dans la note de calcul");
        }
        std::cout << "[PASS] Test 360: Appuis linéiques des surfaces (Navier " << w * 1000 << " / " << wRef * 1000 << " mm)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 361 : charges sur zone et à variation linéaire
    // -------------------------------------------------------------------------
    {
        auto sumRy = [](const AnalysisRunResult& r, int dof) {
            double s = 0;
            for (const auto& [id, rc] : r.results.surfaceNodeReactions()) s += dof == 1 ? rc.ry : rc.rz;
            for (const auto& [id, rc] : r.results.allReactions()) s += dof == 1 ? rc.ry : rc.rz;
            return s;
        };
        // a) Dalle 6 × 6 m appuyée sur son contour, ouverture 1 × 1 m, charge 5 kN/m² sur une zone qui chevauche l'ouverture
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(6, 0, 0), n3 = m.addNode(6, 6, 0), n4 = m.addNode(0, 6, 0);
        const int s = m.addSlab({ n1, n2, n3, n4 }, 0.2, "Dalle");
        SlabOpening o;
        o.polygon = { { 2, 2 }, { 3, 2 }, { 3, 3 }, { 2, 3 } };
        m.getSlab(s)->setOpenings({ o });
        TSA::Model::SurfaceEdgeSupport ss;
        ss.fixed = { true, true, true, false, false, false };
        m.getSlab(s)->setEdgeSupports({ ss });
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        TSA::Model::SurfaceLoad zl;
        zl.surfaceId = s;
        zl.loadCaseId = lc;
        zl.value = 5.0;
        zl.zone = true;
        zl.zoneU0 = 1.3, zl.zoneV0 = 0.7, zl.zoneU1 = 4.1, zl.zoneV1 = 2.6;
        const int zid = m.loadManager().addSurfaceLoad(zl);
        const auto r = runSurfaces(m, settings2D(0.5, true), false, lc);
        const double zoneArea = 2.8 * 1.9 - 1.0 * 0.6;
        double fz = 0, mx = 0, my = 0;
        for (const auto& f : r.compat.surfaces.loads)
        {
            const auto& n = r.mesh.mesh.node(f.meshNode);
            fz += f.fz, mx += n.y * f.fz, my += n.x * f.fz;
        }
        // centre de la zone nette : rectangle (2,7 ; 1,65) moins la partie de l'ouverture (2,5 ; 2,3)
        const double cx = (2.8 * 1.9 * 2.7 - 0.6 * 2.5) / zoneArea, cy = (2.8 * 1.9 * 1.65 - 0.6 * 2.3) / zoneArea;
        TEST_CHECK(r.run.success && std::abs(fz + 5.0 * zoneArea) < 1e-6 * 5.0 * zoneArea && std::abs(my / fz - cx) < 1e-6 && std::abs(mx / fz - cy) < 1e-6,
                   "Test 361: charge de zone Σ Fz = " << fz << " kN (attendu " << -5.0 * zoneArea << "), centre (" << my / fz << " ; " << mx / fz << ") (" << blockingText(r.compat)
                                                      << r.run.message << ")");
        TEST_CHECK(std::abs(sumRy(r.run, 2) - 5.0 * zoneArea) < 1e-3 * 5.0 * zoneArea, "Test 361: Σ Rz = " << sumRy(r.run, 2) << " kN");
        // zone vide refusée
        auto bad = *m.loadManager().getSurfaceLoad(zid);
        bad.zoneU1 = bad.zoneU0;
        m.loadManager().removeSurfaceLoad(zid);
        const int bid = m.loadManager().addSurfaceLoad(bad);
        const auto rb = runSurfaces(m, settings2D(0.5, true), false, lc);
        TEST_CHECK(rb.compat.blocking() && blockingText(rb.compat).find("zone") != std::string::npos, "Test 361: zone vide refusée (" << blockingText(rb.compat) << ")");
        m.loadManager().removeSurfaceLoad(bid);

        // b) Voile 5 × 3 m encastré en pied, eau sur h = 2 m (γ = 10 kN/m³) : p = γ·(h − v) sur v ≤ h
        Model v;
        const int pa = v.addNode(0, 0, 0), pb = v.addNode(5, 0, 0);
        const int wid = v.addWall(pa, pb, 3.0, 0.2, "Voile");
        TSA::Model::SurfaceEdgeSupport base;
        base.edge = 0;
        base.fixed = { true, true, true, true, true, true };
        v.getWall(wid)->setEdgeSupports({ base });
        const int vc = v.loadManager().addLoadCase(LoadCase(0, "Eau", LoadCaseCategory::Live));
        TSA::Model::SurfaceLoad hl;
        hl.targetKind = TSA::Model::SurfaceTargetKind::Wall;
        hl.surfaceId = wid;
        hl.loadCaseId = vc;
        hl.direction = TSA::Model::SurfaceLoadDirection::GlobalY;
        hl.value = 20.0;
        hl.gradV = -10.0;
        hl.zone = true;
        hl.zoneU0 = 0, hl.zoneV0 = 0, hl.zoneU1 = 5, hl.zoneV1 = 2;
        v.loadManager().addSurfaceLoad(hl);
        const auto rh = runSurfaces(v, settings2D(0.4, true), false, vc);
        double fy = 0, mz = 0;
        for (const auto& f : rh.compat.surfaces.loads)
        {
            fy += f.fy;
            mz += rh.mesh.mesh.node(f.meshNode).z * f.fy;
        }
        const double Ph = 10.0 * 2.0 * 2.0 / 2.0 * 5.0;   // 100 kN
        TEST_CHECK(rh.run.success && std::abs(fy - Ph) < 1e-6 * Ph && std::abs(mz / fy - 2.0 / 3.0) < 1e-6,
                   "Test 361: hydrostatique Σ Fy = " << fy << " kN (γh²L/2 = " << Ph << "), point d'application z = " << mz / fy << " m (h/3) " << blockingText(rh.compat) << rh.run.message);
        TEST_CHECK(std::abs(sumRy(rh.run, 1) + Ph) < 1e-3 * Ph, "Test 361: Σ Ry = " << sumRy(rh.run, 1) << " kN");
        // Projet d'exemple (TSA_DEMO_DIR) : voile de réservoir encastré en pied (appui linéique), poussée hydrostatique, maillage enregistré
        if (const QString dir = qEnvironmentVariable("TSA_DEMO_DIR"); !dir.isEmpty())
        {
            MeshProject mp;
            mp.settings = settings2D(0.4, true);
            mp.hasMesh = true;
            mp.mesh = rh.mesh.mesh;
            mp.record = rh.mesh.record;
            v.setMeshProjectJson(QJsonDocument(mp.toJson()).toJson(QJsonDocument::Compact).toStdString());
            TSA::IO::TSAFileWriter writer;
            GridManager grids;
            std::string err;
            const std::string path = (dir + "/TSA_Exemple_Voile_Reservoir.tsa").toStdString();
            TEST_CHECK(writer.saveToFile(path, v, &grids, "Voile de réservoir — poussée hydrostatique", "TSA — exemples", &err), "Test 361: projet d'exemple écrit (" << err << ")");
            std::cout << "  projet d'exemple : " << path << std::endl;
        }

        // c) Persistance de la zone et des gradients
        QTemporaryDir tmp;
        const std::string path = (tmp.path() + "/zone.tsa").toStdString();
        std::string err;
        TSA::IO::TSAFileWriter writer;
        GridManager grids;
        TEST_CHECK(writer.saveToFile(path, v, &grids, "Zone", "TSA", &err), "Test 361: enregistrement (" << err << ")");
        Model back;
        GridManager bg;
        std::string name;
        TSA::IO::TSAFileReader reader;
        bool same = reader.loadFromFile(path, back, &bg, "", &name, nullptr, nullptr, &err) && back.loadManager().surfaceLoads().size() == 1;
        if (same) same = back.loadManager().surfaceLoads().begin()->second == v.loadManager().surfaceLoads().begin()->second;
        TEST_CHECK(same, "Test 361: zone et variation linéaire relues à l'identique (" << err << ")");
        std::cout << "[PASS] Test 361: Charges sur zone et à variation linéaire" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 362 : unités (essai G) — calcul en kN et en N
    // -------------------------------------------------------------------------
    {
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0), n3 = m.addNode(5, 4, 0), n4 = m.addNode(0, 4, 0);
        const int s = m.addSlab({ n1, n2, n3, n4 }, 0.2, "Dalle");
        TSA::Model::SurfaceEdgeSupport rigid, spring;
        rigid.edge = 3;   // bord n4 → n1 encastré
        rigid.fixed = { true, true, true, true, true, true };
        spring.edge = 1;  // bord n2 → n3 sur appui élastique vertical
        spring.fixed = { true, true, false, false, false, false };
        spring.stiffness = { 0, 0, 2000.0, 0, 0, 0 };
        m.getSlab(s)->setEdgeSupports({ rigid, spring });
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = s;
        sl.loadCaseId = lc;
        sl.value = 3.0;
        m.loadManager().addSurfaceLoad(sl);
        auto run = [&](bool kN, bool modal) {
            return runSurfaces(m, settings2D(0.5, true), true, modal ? 0 : lc, [&](TSA::Analysis::AnalysisContext& c) {
                QJsonObject o = c.settingsFor("opensees");
                o["useKiloNewtons"] = kN;
                c.engineSettings["opensees"] = o;
                if (modal) c.type = TSA::Analysis::AnalysisType::Modal, c.common.modeCount = 3;
            });
        };
        const auto a = run(true, false), b = run(false, false);
        TEST_CHECK(a.run.success && b.run.success, "Test 362: calculs en kN et en N (" << a.run.message << " / " << b.run.message << ")");
        double maxDiff = 0, wMax = 0;
        for (const auto& [id, d] : a.run.results.surfaceNodeDisplacements())
        {
            const auto& e = b.run.results.surfaceNodeDisplacements().at(id);
            maxDiff = std::max(maxDiff, std::abs(d.uz - e.uz));
            wMax = std::max(wMax, std::abs(d.uz));
        }
        TEST_CHECK(wMax > 1e-5 && maxDiff < 1e-9 + 1e-7 * wMax, "Test 362: déplacements identiques (écart " << maxDiff << " m sur " << wMax << " m)");
        auto sumRz = [](const AnalysisRunResult& r) {
            double z = 0;
            for (const auto& [id, rc] : r.results.surfaceNodeReactions()) z += rc.rz;
            for (const auto& [id, rc] : r.results.allReactions()) z += rc.rz;
            return z;
        };
        const double expected = 3.0 * 20.0 + 2500 * 9.81 * 0.2 * 20.0 / 1000.0;   // q·A + ρ·g·e·A (kN)
        TEST_CHECK(std::abs(sumRz(a.run) - expected) < 1e-3 * expected && std::abs(sumRz(b.run) - 1000.0 * expected) < 1e-3 * 1000.0 * expected,
                   "Test 362: Σ Rz = " << sumRz(a.run) << " kN et " << sumRz(b.run) << " N (attendu " << expected << " kN)");
        const auto ma = run(true, true), mb = run(false, true);
        TEST_CHECK(ma.run.success && mb.run.success && !ma.run.results.modal().empty() && !mb.run.results.modal().empty(),
                   "Test 362: calculs modaux (" << ma.run.message << " / " << mb.run.message << ")");
        const auto& fa = ma.run.results.modal(), & fb = mb.run.results.modal();
        TEST_CHECK(std::abs(fa.modes[0].frequency - fb.modes[0].frequency) < 1e-6 * fa.modes[0].frequency &&
                       std::abs(fb.totalMass[2] - 1000.0 * fa.totalMass[2]) < 1e-6 * fb.totalMass[2] && std::abs(fa.totalMass[2] - 2.5 * 0.2 * 20.0) < 1e-9,
                   "Test 362: f1 = " << fa.modes[0].frequency << " / " << fb.modes[0].frequency << " Hz, masses " << fa.totalMass[2] << " t / " << fb.totalMass[2] << " kg");
        std::cout << "[PASS] Test 362: Unités kN / N : déplacements, réactions, fréquences et masses cohérents" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 363 : nombreuses surfaces — temps, équilibre, invalidation du maillage
    // -------------------------------------------------------------------------
    {
        Model m;
        const int nx = 6, ny = 4;
        const double a = 4.0, H = 3.0;
        const Material c = Material::concreteC30_37();
        std::map<std::pair<int, int>, int> top;
        for (int i = 0; i <= nx; ++i)
            for (int j = 0; j <= ny; ++j)
            {
                const int b = m.addNode(i * a, j * a, 0);
                m.getNode(b)->setSupport(SupportDefinition::fixed());
                top[{ i, j }] = m.addNode(i * a, j * a, H);
                m.addColumn(b, top[{ i, j }], Section::rectangular(0.4, 0.4), c, 0.0, "Poteau");
            }
        double beamLength = 0;
        for (int i = 0; i <= nx; ++i)
            for (int j = 0; j <= ny; ++j)
            {
                if (i < nx) m.addBar(top[{ i, j }], top[{ i + 1, j }], Section::rectangular(0.3, 0.5), c, BarRole::Beam, 0.0, "Poutre"), beamLength += a;
                if (j < ny) m.addBar(top[{ i, j }], top[{ i, j + 1 }], Section::rectangular(0.3, 0.5), c, BarRole::Beam, 0.0, "Poutre"), beamLength += a;
            }
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "G+Q", LoadCaseCategory::Dead, true, 1.0));
        std::vector<int> slabs;
        for (int i = 0; i < nx; ++i)
            for (int j = 0; j < ny; ++j)
            {
                const int s = m.addSlab({ top[{ i, j }], top[{ i + 1, j }], top[{ i + 1, j + 1 }], top[{ i, j + 1 }] }, 0.2, "Dalle");
                m.getSlab(s)->setMaterial(c);
                slabs.push_back(s);
                TSA::Model::SurfaceLoad sl;
                sl.surfaceId = s;
                sl.loadCaseId = lc;
                sl.value = 3.0;
                m.loadManager().addSurfaceLoad(sl);
            }
        QElapsedTimer t;
        t.start();
        const auto r = runSurfaces(m, settings2D(0.5, true), true, lc);
        const double seconds = t.elapsed() / 1000.0;
        TEST_CHECK(!r.compat.blocking() && r.run.success, "Test 363: plancher de " << slabs.size() << " dalles (" << blockingText(r.compat) << errorsOf(r.prepared) << r.run.message << ")");
        const double g = c.mechanical.density * 9.81 / 1000.0;
        const double area = nx * ny * a * a;
        const double expected = g * (0.2 * area + Section::rectangular(0.4, 0.4).area() * H * (nx + 1) * (ny + 1) + Section::rectangular(0.3, 0.5).area() * beamLength) + 3.0 * area;
        const double rz = r.run.results.equilibrium().reactionFz;
        std::cout << "  " << slabs.size() << " dalles, " << r.compat.surfaces.elements.size() << " coques, " << r.compat.surfaces.nodes.size() << " nœuds de surface : maillage + calcul "
                  << seconds << " s ; Σ Rz " << rz << " kN (attendu " << expected << ")" << std::endl;
        TEST_CHECK(r.compat.surfaces.elements.size() >= 1000 && std::abs(rz - expected) < 1e-6 * expected, "Test 363: équilibre global (Σ Rz = " << rz << ", attendu " << expected << ")");
        TEST_CHECK(seconds < 300.0, "Test 363: durée raisonnable (" << seconds << " s)");

        // Invalidation : charges et appuis linéiques ne modifient pas l'empreinte du maillage (pas de remaillage inutile) ;
        // une ouverture la modifie (remaillage nécessaire).
        const std::string h0 = MeshGeometry::extract(m).hash;
        TSA::Model::SurfaceEdgeSupport es;
        es.fixed = { false, false, true, false, false, false };
        m.getSlab(slabs.front())->setEdgeSupports({ es });
        TSA::Model::SurfaceLoad extra;
        extra.surfaceId = slabs.back();
        extra.loadCaseId = lc;
        extra.value = 1.0;
        extra.zone = true;
        extra.zoneU1 = extra.zoneV1 = 1.0;
        m.loadManager().addSurfaceLoad(extra);
        const std::string h1 = MeshGeometry::extract(m).hash;
        SlabOpening o;
        o.polygon = { { 1, 1 }, { 2, 1 }, { 2, 2 }, { 1, 2 } };
        m.getSlab(slabs.front())->setOpenings({ o });
        const std::string h2 = MeshGeometry::extract(m).hash;
        TEST_CHECK(h0 == h1 && h1 != h2, "Test 363: empreinte du maillage conservée (charges, appuis) et modifiée (ouverture)");
        std::cout << "[PASS] Test 363: Plancher de " << slabs.size() << " dalles (" << seconds << " s), invalidation du maillage" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 364 : API interne des surfaces, export IFC des surfaces analytiques
    // -------------------------------------------------------------------------
    {
        using TSA::Api::SurfaceApi;
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(6, 0, 0), n3 = m.addNode(6, 4, 0), n4 = m.addNode(0, 4, 0);
        const int s = m.addSlab({ n1, n2, n3, n4 }, 0.2, "Dalle");
        const int w = m.addWall(n1, n2, 3.0, 0.2, "Voile");
        std::string err;
        TEST_CHECK(SurfaceApi::addOpening(m, TSA::Model::SurfaceTargetKind::Slab, s, { { 2, 1 }, { 3, 1 }, { 3, 2 }, { 2, 2 } }, "trémie", &err),
                   "Test 364: ouverture ajoutée (" << err << ")");
        TEST_CHECK(!SurfaceApi::addOpening(m, TSA::Model::SurfaceTargetKind::Slab, s, { { 5, 3 }, { 7, 3 }, { 7, 5 }, { 5, 5 } }, "hors dalle", &err) && !err.empty(),
                   "Test 364: ouverture hors de la dalle refusée");
        TSA::Model::SurfaceEdgeSupport sp;
        sp.edge = 2;
        sp.fixed[2] = true;
        TEST_CHECK(SurfaceApi::addEdgeSupport(m, TSA::Model::SurfaceTargetKind::Slab, s, sp, &err), "Test 364: appui linéique ajouté");
        sp.edge = 9;
        TEST_CHECK(!SurfaceApi::addEdgeSupport(m, TSA::Model::SurfaceTargetKind::Slab, s, sp, &err), "Test 364: bord inexistant refusé");
        TEST_CHECK(!SurfaceApi::addEdgeSupport(m, TSA::Model::SurfaceTargetKind::Wall, w, TSA::Model::SurfaceEdgeSupport {}, &err), "Test 364: appui sans effet refusé");
        TSA::Model::SurfaceLoad l;
        l.surfaceId = s;
        l.loadCaseId = 999;
        TEST_CHECK(SurfaceApi::addLoad(m, l, &err) == 0, "Test 364: cas de charge inexistant refusé");
        l.loadCaseId = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        l.value = 2.0;
        TEST_CHECK(SurfaceApi::addLoad(m, l, &err) > 0, "Test 364: charge ajoutée (" << err << ")");
        TEST_CHECK(SurfaceApi::setFormulation(m, TSA::Model::SurfaceTargetKind::Wall, w, TSA::Model::SurfaceFormulation::Plate, &err), "Test 364: formulation");

        const auto list = SurfaceApi::list(m);
        TEST_CHECK(list.size() == 2 && list[0].ref == "SL" + std::to_string(s) && std::abs(list[0].netArea - 23.0) < 1e-9 && std::abs(list[0].grossArea - 24.0) < 1e-9 &&
                       list[0].openings == 1 && list[0].edgeSupports == 1 && list[0].loads == 1 && list[1].formulation == TSA::Model::SurfaceFormulation::Plate,
                   "Test 364: description des surfaces");
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        const auto os = SurfaceApi::capabilities(*reg.engine("opensees"));
        const auto c2 = SurfaceApi::capabilities(*reg.engine("custom2d"));
        TEST_CHECK(os.shell && os.modal && os.edgeSupports && os.zoneLoads && !os.plate && !os.membrane && !os.thermal && !os.supports(TSA::Model::SurfaceFormulation::Plate) &&
                       !c2.shell && !c2.edgeSupports,
                   "Test 364: capacités interrogées avant usage (OpenSees : coques, modal ; Custom2D : aucune surface)");
        const QJsonObject j = SurfaceApi::toJson(m);
        const QJsonArray arr = j.value("surfaces").toArray();
        TEST_CHECK(j.value("format").toString() == "tsa.surfaces/1" && arr.size() == 2 && arr[0].toObject().value("openings").toArray().size() == 1 &&
                       arr[0].toObject().value("outer").toArray().size() == 4 && arr[1].toObject().value("formulation").toString() == "plate",
                   "Test 364: export JSON versionné");

        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-12T00:00:00";
        std::string ifc;
        TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, ifc);
        TEST_CHECK(ifc.find("IFCSTRUCTURALSURFACEMEMBER") != std::string::npos && ifc.find("IFCFACEBOUND(") != std::string::npos &&
                       ifc.find(".BENDING_ELEMENT.") != std::string::npos && ifc.find(".SHELL.") != std::string::npos,
                   "Test 364: IFC — surfaces analytiques avec ouverture (IfcFaceBound) et type selon la formulation");
        TEST_CHECK(!list[0].globalId.empty() && ifc.find(list[0].globalId) != std::string::npos, "Test 364: GlobalId stable de la dalle présent dans l'IFC (" << list[0].globalId << ")");
        std::cout << "[PASS] Test 364: API interne des surfaces et export IFC" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 429 : voile à base inclinée calculé en coques — maillage dans le parallélogramme, poids propre =
    // ρ·g·t·(longueur en plan × hauteur), Σ réactions à l'équilibre (avant correction : repère non orthogonal, aire 3D)
    // -------------------------------------------------------------------------
    {
        Model v;
        const double Lh = 4.0, dz = 1.0, H = 3.0, t = 0.2;
        std::vector<int> base;
        for (int k = 0; k <= 8; ++k) base.push_back(v.addNode(Lh * k / 8, 0, dz * k / 8));
        for (int id : base) v.getNode(id)->setSupport(SupportDefinition::fixed());
        const int wl = v.addWall(base.front(), base.back(), H, t, "Voile incliné");
        const int lc = v.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
        const auto r = runSurfaces(v, settings2D(0.4, true), true, lc);
        TEST_CHECK(!r.compat.blocking() && r.run.success, "Test 429: voile incliné calculé (" << blockingText(r.compat) << errorsOf(r.prepared) << ")");
        bool inside = !r.compat.surfaces.elements.empty();
        for (const auto& e : r.compat.surfaces.elements)
        {
            double cx = 0, cy = 0, cz = 0;
            for (int n : e.meshNodes)
            {
                cx += r.mesh.mesh.node(n).x / e.meshNodes.size();
                cy += r.mesh.mesh.node(n).y / e.meshNodes.size();
                cz += r.mesh.mesh.node(n).z / e.meshNodes.size();
            }
            const double zBase = dz * cx / Lh;
            inside = inside && cx > -1e-9 && cx < Lh + 1e-9 && std::abs(cy) < 1e-9 && cz > zBase - 1e-9 && cz < zBase + H + 1e-9;
        }
        TEST_CHECK(inside, "Test 429: tous les éléments de coque dans le parallélogramme du voile");
        const double rho = v.getWall(wl)->material().mechanical.density;
        const double expected = rho * 9.81 * t / 1000.0 * Lh * H;
        const auto eq = r.run.results.equilibrium();
        const double sw = r.compat.surfaces.selfWeightBySurface.count("W" + std::to_string(wl)) ? r.compat.surfaces.selfWeightBySurface.at("W" + std::to_string(wl)) : -1.0;
        std::cout << "  voile incliné : poids propre " << sw << " kN, Σ Rz " << eq.reactionFz << " kN, attendu " << expected
                  << " kN (aire en plan × hauteur = " << Lh * H << " m², aire 3D " << std::hypot(Lh, dz) * H << " m²)" << std::endl;
        TEST_CHECK(std::abs(sw - expected) < 1e-6 * expected && std::abs(eq.reactionFz - expected) < 1e-6 * expected && eq.isBalanced(1e-6),
                   "Test 429: poids propre et Σ réactions = ρ·g·t·(4 × 3 m²)");
        std::cout << "[PASS] Test 429: Voile à base inclinée calculé (coques)" << std::endl;
        ++passed;
    }

    return true;
}
