#include "test_common.h"

#include "Analysis/OpenSeesSolver.h"
#include "Analysis/OpenSeesModelMap.h"
#include "Analysis/ElementTransformation.h"
#include "Analysis/ResultsExport.h"
#include "Analysis/ResultsContext.h"
#include "Analysis/LoadResolver.h"

#include <QElapsedTimer>
#include <QJsonArray>

// Validation numérique de l'extraction OpenSees : chaque grandeur exposée par TSA est comparée
// soit à la théorie des poutres, soit à une autre grandeur produite indépendamment par OpenSees
// (ex. K_global de printA contre l'assemblage des k_basic de chaque élément).
// Tolérances : valeurs des recorders à 16 chiffres significatifs, K_global de printA à 11 chiffres.

using namespace TSA::Analysis;

namespace
{
constexpr double kRelTolRecorder = 1e-9;  // grandeurs issues des recorders (16 chiffres)
constexpr double kRelTolPrintA = 1e-9;    // K_global (11 chiffres significatifs)

bool relClose(double a, double b, double rel, double absFloor = 1e-12)
{
    return std::abs(a - b) <= rel * std::max({ std::abs(a), std::abs(b), absFloor });
}

bool runSolve(Model& model, AnalysisParameters params, ResultsModel& out, std::string* err = nullptr)
{
    OpenSeesSolver solver;
    QString e;
    const bool ok = solver.solveSynchronous(model, params, &e);
    if (!ok)
    {
        std::cout << "  Solver error: " << e.toStdString() << "\n" << solver.results().journalLog() << "\n";
        if (err) *err = e.toStdString();
    }
    out = solver.results();
    return ok;
}

int addCase(Model& m)
{
    return m.loadManager().addLoadCase(LoadCase(1, "CAS1", LoadCaseCategory::Live, false, 1.0));
}

/// Assemble les K_global élémentaires (+ ressorts) dans la numérotation OpenSees.
DenseMatrix assemble(const AdvancedResults& adv, bool* complete)
{
    const int n = adv.dofMap.equationCount();
    DenseMatrix K(n, n);
    *complete = true;
    for (const auto& [key, em] : adv.elementMatrices)
    {
        if (!em.available) { *complete = false; continue; }
        int eqs[12];
        for (int a = 0; a < 12; ++a)
            eqs[a] = adv.dofMap.equationOf(a < 6 ? em.nodeI : em.nodeJ, a % 6);
        for (int a = 0; a < 12; ++a)
            for (int b = 0; b < 12; ++b)
                if (eqs[a] >= 0 && eqs[b] >= 0) K(eqs[a], eqs[b]) += em.kGlobal(a, b);
    }
    for (const auto& s : adv.springs)
        for (std::size_t i = 0; i < s.dofs.size(); ++i)
        {
            const int eq = adv.dofMap.equationOf(s.nodeId, s.dofs[i]);
            if (eq >= 0) K(eq, eq) += s.stiffness[i];
        }
    return K;
}

double maxAbs(const SparseMatrix& k)
{
    double m = 0.0;
    for (double v : k.values) m = std::max(m, std::abs(v));
    return m;
}

Vec12 elementDisplacements(const ResultsModel& r, int nodeI, int nodeJ)
{
    Vec12 u {};
    const NodeDisplacement di = r.nodeDisplacement(nodeI);
    const NodeDisplacement dj = r.nodeDisplacement(nodeJ);
    const double a[6] = { di.ux, di.uy, di.uz, di.rx, di.ry, di.rz };
    const double b[6] = { dj.ux, dj.uy, dj.uz, dj.rx, dj.ry, dj.rz };
    for (int k = 0; k < 6; ++k) { u[k] = a[k]; u[6 + k] = b[k]; }
    return u;
}
} // namespace

bool runSuite_Extraction(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 105 : identité TSA ↔ OpenSees — ids identiques dans des familles différentes
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 105: Mapping TSA <-> OpenSees (ids poutre/poteau identiques) ---" << std::endl;
        Model m;
        int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(5, 0, 3), d = m.addNode(5, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(d)->setSupport(SupportDefinition::fixed());
        int col1 = m.addColumn(a, b, Section::heb(200), Material::steelS235());
        int col2 = m.addColumn(d, c, Section::heb(200), Material::steelS235());
        int beam1 = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        int truss1 = m.addTrussMember(a, c, 0.05);
        TEST_CHECK(beam1 == 1 && col1 == 1 && truss1 == 1, "Test 105: ids TSA identiques entre familles (cas critique)");

        auto snap = CalculationSnapshot::capture(m);
        TEST_CHECK(snap.elementCount() == 4, "Test 105: aucun élément écrasé dans le snapshot");
        TEST_CHECK(snap.findElement(StructuralElementKind::Beam, 1) && snap.findElement(StructuralElementKind::Column, 1)
                   && snap.findElement(StructuralElementKind::Truss, 1), "Test 105: recherche par (famille, id)");
        AnalysisParameters p;
        auto map = OpenSeesModelMap::build(snap, p);
        TEST_CHECK(map.validate(snap).empty(), "Test 105: mapping cohérent");
        std::set<int> tags;
        for (const auto& e : map.elements()) tags.insert(e.tag);
        TEST_CHECK(tags.size() == 4, "Test 105: 4 tags OpenSees distincts");

        const std::string tcl = OpenSeesAnalysisBuilder::buildScript(snap, map, p);
        TEST_CHECK(tcl.find("element truss ") != std::string::npos && tcl.find("uniaxialMaterial Elastic") != std::string::npos,
                   "Test 105: treillis généré avec matériau (syntaxe OpenSees 3.8.0)");

        int lc = addCase(m);
        m.loadManager().addNodalLoad(NodalLoad(0, c, lc, 10.0, 0.0, -20.0));
        p.includeSelfWeight = false;

        // Charge sur le POTEAU 1 alors que la poutre 1 existe : appliquée au poteau seulement.
        MemberLoad onColumn = MemberLoad::uniform(col1, lc, 3.0, LoadDirection::GlobalX, "vent", MemberTargetType::Column);
        m.loadManager().addMemberLoad(onColumn);
        {
            auto s2 = CalculationSnapshot::capture(m);
            auto map2 = OpenSeesModelMap::build(s2, p);
            const int colTag = s2.findElement(StructuralElementKind::Column, col1)->tag;
            const int beamTag = s2.findElement(StructuralElementKind::Beam, beam1)->tag;
            const std::string tcl2 = OpenSeesAnalysisBuilder::buildScript(s2, map2, p);
            TEST_CHECK(tcl2.find("eleLoad -ele " + std::to_string(colTag) + " ") != std::string::npos
                       && tcl2.find("eleLoad -ele " + std::to_string(beamTag) + " ") == std::string::npos,
                       "Test 105: charge appliquée au poteau 1, pas à la poutre 1");
            // Compatibilité : charge « Beam » (anciens fichiers) sur l'id d'un treillis sans poutre homonyme.
            MemberLoad legacy = MemberLoad::uniform(99, lc, 1.0);
            TEST_CHECK(s2.findElementForLoad(legacy) == nullptr, "Test 105: charge orpheline ignorée");
        }
        ResultsModel r;
        TEST_CHECK(runSolve(m, p, r), "Test 105: calcul d'un modèle avec treillis (échouait : « Invalid matTag »)");
        TEST_CHECK(r.getElementResults(StructuralElementKind::Beam, 1) && r.getElementResults(StructuralElementKind::Column, 1)
                   && r.getElementResults(StructuralElementKind::Column, 2) && r.getElementResults(StructuralElementKind::Truss, 1),
                   "Test 105: résultats pour les 4 éléments");
        TEST_CHECK(r.allElementResults().size() == 4, "Test 105: aucun résultat confondu");
        std::cout << "[PASS] Test 105: Mapping TSA <-> OpenSees" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 106 : barre axiale — K = EA/L, U = PL/EA, N = P, R = -P
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 106: Barre axiale (K = EA/L) ---" << std::endl;
        Model m;
        int n1 = m.addNode(0, 0, 0), n2 = m.addNode(2.5, 0, 0);
        m.getNode(n1)->setSupport(SupportDefinition::fixed());
        using DS = TSA::Model::DOFState;
        m.getNode(n2)->setSupport(SupportDefinition::custom(DS::Free, DS::Fixed, DS::Fixed, DS::Fixed, DS::Fixed, DS::Fixed));
        int t = m.addTrussMember(n1, n2, 0.04);
        int lc = addCase(m);
        const double P = 37.5;
        m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, P, 0.0, 0.0));

        auto snap = CalculationSnapshot::capture(m);
        const auto* el = snap.findElement(StructuralElementKind::Truss, t);
        TEST_CHECK(el != nullptr, "Test 106: treillis dans le snapshot");
        const double EA_L = el->section.area() * el->material.mechanical.youngModulus * 1e-3 / 2.5; // kN/m

        AnalysisParameters p;
        p.includeSelfWeight = false;
        p.extractionLevel = ExtractionLevel::Advanced;
        ResultsModel r;
        TEST_CHECK(runSolve(m, p, r), "Test 106: calcul");
        const auto& adv = r.advanced();
        for (const auto& w : adv.warnings) std::cout << "  [advanced] " << w << std::endl;
        TEST_CHECK(adv.available && adv.dofMap.equationCount() == 1, "Test 106: un seul DDL libre (N2.UX)");
        TEST_CHECK(adv.dofMap.equationOf(n2, 0) == 0 && adv.dofMap.equationLabel(0) == "N2.UX", "Test 106: mapping DDL");
        TEST_CHECK(adv.hasGlobalStiffness && relClose(adv.kGlobal.at(0, 0), EA_L, kRelTolPrintA), "Test 106: K_global = EA/L");
        const auto& em = adv.elementMatrices.at(ElementKey{ StructuralElementKind::Truss, t });
        TEST_CHECK(em.available && relClose(em.kBasic(0, 0), EA_L, kRelTolRecorder), "Test 106: k_basic OpenSees = EA/L");
        TEST_CHECK(em.kBasicMeta.exact && em.kBasicMeta.source.find("OpenSees API") == 0, "Test 106: métadonnées k_basic");
        TEST_CHECK(relClose(r.nodeDisplacement(n2).ux, P / EA_L, kRelTolRecorder), "Test 106: U = PL/EA");
        const auto* er = r.getElementResults(StructuralElementKind::Truss, t);
        TEST_CHECK(er && relClose(er->startForces.N, P, kRelTolRecorder), "Test 106: N = +P (traction)");
        TEST_CHECK(relClose(r.nodeReaction(n1).rx, -P, kRelTolRecorder), "Test 106: réaction = -P");
        std::cout << "[PASS] Test 106: Barre axiale" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 107 : console — U = PL³/3EI, réactions, rigidités de flexion
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 107: Console (poutre simple) ---" << std::endl;
        Model m;
        const double L = 4.0, P = 12.0;
        int n1 = m.addNode(0, 0, 0), n2 = m.addNode(L, 0, 0);
        m.getNode(n1)->setSupport(SupportDefinition::fixed());
        int b = m.addBar(n1, n2, Section::ipe(240), Material::steelS235(), BarRole::Beam);
        int lc = addCase(m);
        m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 0.0, 0.0, -P));

        auto snap = CalculationSnapshot::capture(m);
        const auto* el = snap.findElement(StructuralElementKind::Beam, b);
        const double E = el->material.mechanical.youngModulus * 1e-3;
        const double Iy = el->section.iy(), Iz = el->section.iz(), A = el->section.area();

        AnalysisParameters p;
        p.includeSelfWeight = false;
        p.extractionLevel = ExtractionLevel::Advanced;
        ResultsModel r;
        TEST_CHECK(runSolve(m, p, r), "Test 107: calcul");

        TEST_CHECK(relClose(r.nodeDisplacement(n2).uz, -P * L * L * L / (3.0 * E * Iy), kRelTolRecorder), "Test 107: flèche PL³/3EIy");
        TEST_CHECK(relClose(r.nodeDisplacement(n2).ry, P * L * L / (2.0 * E * Iy), kRelTolRecorder), "Test 107: rotation PL²/2EIy");
        TEST_CHECK(relClose(r.nodeReaction(n1).rz, P, kRelTolRecorder), "Test 107: réaction verticale = P");
        TEST_CHECK(relClose(r.nodeReaction(n1).my, -P * L, kRelTolRecorder), "Test 107: moment d'encastrement = -PL (autour de Y)");

        const auto& adv = r.advanced();
        TEST_CHECK(adv.hasGlobalStiffness && adv.dofMap.equationCount() == 6, "Test 107: 6 DDL libres");
        auto K = [&](int d1, int d2) { return adv.kGlobal.at(adv.dofMap.equationOf(n2, d1), adv.dofMap.equationOf(n2, d2)); };
        TEST_CHECK(relClose(K(0, 0), E * A / L, kRelTolPrintA), "Test 107: K(UX,UX) = EA/L");
        TEST_CHECK(relClose(K(2, 2), 12.0 * E * Iy / (L * L * L), kRelTolPrintA), "Test 107: K(UZ,UZ) = 12EIy/L³");
        TEST_CHECK(relClose(K(4, 4), 4.0 * E * Iy / L, kRelTolPrintA), "Test 107: K(RY,RY) = 4EIy/L");
        TEST_CHECK(relClose(std::abs(K(2, 4)), 6.0 * E * Iy / (L * L), kRelTolPrintA), "Test 107: |K(UZ,RY)| = 6EIy/L²");
        TEST_CHECK(relClose(K(1, 1), 12.0 * E * Iz / (L * L * L), kRelTolPrintA), "Test 107: K(UY,UY) = 12EIz/L³");
        TEST_CHECK(adv.kGlobalMeta.exact && adv.kGlobalMeta.symmetric && adv.kGlobalMeta.source.find("printA") != std::string::npos,
                   "Test 107: métadonnées K_global");

        // Moment local à l'encastrement : M = P·L (localForce OpenSees, sans post-traitement).
        const auto& f = adv.elementForces.at(ElementKey{ StructuralElementKind::Beam, b });
        TEST_CHECK(f.local.size() == 12 && relClose(std::abs(f.local[4]), P * L, kRelTolRecorder), "Test 107: |My_i| local = PL");
        std::cout << "[PASS] Test 107: Console" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 108 : structure multi-éléments — K_global, U_global, F_global, ressorts
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 108: Portique 3D multi-éléments (K, U, F) ---" << std::endl;
        Model m;
        int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3.5), c = m.addNode(6, 0, 3.5), d = m.addNode(6, 0, 0);
        int e = m.addNode(6, 4, 3.5), f = m.addNode(6, 4, 0);
        using DS = TSA::Model::DOFState;
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(d)->setSupport(SupportDefinition::custom(DS::Fixed, DS::Fixed, DS::Fixed, DS::Fixed, DS::Free, DS::Fixed));
        m.getNode(f)->setSupport(SupportDefinition::elastic(2e4, 2e4, 5e5, 3e3, 3e3, 3e3));
        m.addColumn(a, b, Section::heb(220), Material::steelS235());
        int c2 = m.addColumn(d, c, Section::heb(220), Material::steelS235());
        m.addColumn(f, e, Section::heb(220), Material::steelS235());
        m.getColumn(c2)->setRotation(90.0);
        m.addBar(b, c, Section::ipe(330), Material::steelS235(), BarRole::Beam);
        int beam2 = m.addBar(c, e, Section::ipe(270), Material::steelS235(), BarRole::Beam);
        m.getBeam(beam2)->setRotation(25.0);
        m.addTrussMember(b, e, 0.03);
        int lc = addCase(m);
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 15.0, 4.0, -30.0, 0.0, 2.0, 0.0));
        m.loadManager().addNodalLoad(NodalLoad(0, e, lc, -5.0, 8.0, -45.0, 1.5, 0.0, -3.0));

        AnalysisParameters p;
        p.includeSelfWeight = false;
        p.extractionLevel = ExtractionLevel::Advanced;
        ResultsModel r;
        TEST_CHECK(runSolve(m, p, r), "Test 108: calcul");
        const auto& adv = r.advanced();
        TEST_CHECK(adv.available && adv.hasGlobalStiffness, "Test 108: matrices extraites");
        TEST_CHECK(adv.springs.size() == 1 && adv.springs[0].nodeId == f, "Test 108: ressort reconnu");
        TEST_CHECK(adv.dofMap.equationOf(d, 4) >= 0 && adv.dofMap.equationOf(d, 0) < 0, "Test 108: appui partiel (RY libre, UX contraint)");

        // (1) K_global OpenSees (printA) = Σ K_e reconstruits depuis k_basic OpenSees + ressorts.
        bool complete = false;
        const DenseMatrix Ka = assemble(adv, &complete);
        TEST_CHECK(complete, "Test 108: toutes les rigidités élémentaires disponibles");
        const double scale = maxAbs(adv.kGlobal);
        double maxDiff = 0.0;
        for (int i = 0; i < Ka.rows; ++i)
            for (int j = 0; j < Ka.cols; ++j)
                maxDiff = std::max(maxDiff, std::abs(Ka(i, j) - adv.kGlobal.at(i, j)));
        std::cout << "  max|K_printA - Σ K_e| / max|K| = " << maxDiff / scale << std::endl;
        TEST_CHECK(maxDiff <= kRelTolPrintA * scale, "Test 108: K_global printA == assemblage des K_e (exact)");

        // (2) K · U_global = F_global (charges nodales uniquement).
        const std::vector<double> U = adv.globalDisplacementVector(r.allDisplacements());
        const std::vector<double> KU = adv.kGlobal.multiply(U);
        std::vector<double> F(U.size(), 0.0);
        auto addF = [&](int node, const double comp[6]) {
            for (int dd = 0; dd < 6; ++dd)
            {
                const int eq = adv.dofMap.equationOf(node, dd);
                if (eq >= 0) F[eq] += comp[dd];
            }
        };
        const double fb[6] = { 15.0, 4.0, -30.0, 0.0, 2.0, 0.0 };
        const double fe[6] = { -5.0, 8.0, -45.0, 1.5, 0.0, -3.0 };
        addF(b, fb);
        addF(e, fe);
        double res = 0.0, fn = 0.0;
        for (std::size_t i = 0; i < F.size(); ++i) { res = std::max(res, std::abs(KU[i] - F[i])); fn = std::max(fn, std::abs(F[i])); }
        std::cout << "  max|K·U - F| / max|F| = " << res / fn << std::endl;
        TEST_CHECK(res <= 1e-8 * fn, "Test 108: K·U = F");

        // (3) Forces élémentaires : K_e · u_e == globalForce OpenSees ; T · global == localForce.
        double worstGlobal = 0.0, worstLocal = 0.0, worstBasic = 0.0;
        for (const auto& [key, em] : adv.elementMatrices)
        {
            const auto& fs = adv.elementForces.at(key);
            const Vec12 ue = elementDisplacements(r, em.nodeI, em.nodeJ);
            double fScale = 1e-12;
            for (double v : fs.global) fScale = std::max(fScale, std::abs(v));
            for (int i = 0; i < 12; ++i)
            {
                double s = 0.0;
                for (int j = 0; j < 12; ++j) s += em.kGlobal(i, j) * ue[j];
                worstGlobal = std::max(worstGlobal, std::abs(s - fs.global[i]) / fScale);
            }
            LocalAxes axes;
            axes.x = { em.rotation[0], em.rotation[1], em.rotation[2] };
            axes.y = { em.rotation[3], em.rotation[4], em.rotation[5] };
            axes.z = { em.rotation[6], em.rotation[7], em.rotation[8] };
            Vec12 g {};
            std::copy(fs.global.begin(), fs.global.end(), g.begin());
            const Vec12 l = ElementTransformation::globalToLocal(axes, g);
            for (int i = 0; i < 12; ++i)
                worstLocal = std::max(worstLocal, std::abs(l[i] - fs.local[i]) / fScale);
            if (em.opsClass == "elasticBeamColumn")
            {
                // q_basic = k_basic · ub(u_local)
                const auto ub = ElementTransformation::beamLocalToBasic(ElementTransformation::globalToLocal(axes, ue), em.length);
                for (int i = 0; i < 6; ++i)
                {
                    double s = 0.0;
                    for (int j = 0; j < 6; ++j) s += em.kBasic(i, j) * ub[j];
                    worstBasic = std::max(worstBasic, std::abs(s - fs.basic[i]) / fScale);
                }
            }
        }
        std::cout << "  écarts relatifs : global " << worstGlobal << ", local " << worstLocal << ", basic " << worstBasic << std::endl;
        TEST_CHECK(worstGlobal < 1e-8, "Test 108: K_e·u_e == globalForce OpenSees");
        TEST_CHECK(worstLocal < 1e-12, "Test 108: T·globalForce == localForce OpenSees");
        TEST_CHECK(worstBasic < 1e-8, "Test 108: k_basic·u_basic == basicForce OpenSees");

        // (4) Équilibre global, ressort compris : Σ F + Σ R ≈ 0.
        const auto eq = r.equilibrium();
        std::cout << "  résidu relatif d'équilibre = " << r.executionMetadata().relativeEquilibriumResidual << std::endl;
        TEST_CHECK(r.executionMetadata().relativeEquilibriumResidual < 1e-9, "Test 108: équilibre (réaction du ressort reportée)");
        TEST_CHECK(std::abs(r.nodeReaction(f).rz) > 1.0, "Test 108: réaction non nulle au nœud sur ressort");
        (void)eq;
        std::cout << "[PASS] Test 108: Portique 3D multi-éléments" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 109 : élément orienté — repères local/global
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 109: Transformation local <-> global (élément orienté) ---" << std::endl;
        const Vec3 pI { 1.0, -2.0, 0.5 }, pJ { 4.0, 2.0, 3.5 };
        for (double beta : { 0.0, 30.0, -75.0 })
        {
            const gp_Ax3 tsa = LoadResolver::computeElementLocalAxes(gp_Pnt(pI[0], pI[1], pI[2]), gp_Pnt(pJ[0], pJ[1], pJ[2]), beta);
            const Vec3 vecxz { tsa.Direction().X(), tsa.Direction().Y(), tsa.Direction().Z() };
            const LocalAxes ops = ElementTransformation::openSeesAxes(pI, pJ, vecxz);
            TEST_CHECK(ops.valid, "Test 109: axes valides");
            const gp_Dir ty = tsa.YDirection();
            TEST_CHECK(approxEqual(ops.y[0], ty.X(), 1e-12) && approxEqual(ops.y[1], ty.Y(), 1e-12) && approxEqual(ops.y[2], ty.Z(), 1e-12),
                       "Test 109: y OpenSees (vecxz × x) == y TSA (charges wy de même signe)");
            const double dotXY = ops.x[0] * ops.y[0] + ops.x[1] * ops.y[1] + ops.x[2] * ops.y[2];
            TEST_CHECK(std::abs(dotXY) < 1e-14, "Test 109: repère orthonormé");
            Vec12 g {};
            for (int i = 0; i < 12; ++i) g[i] = 0.1 * (i + 1) - 0.35 * (i % 3);
            const Vec12 back = ElementTransformation::localToGlobal(ops, ElementTransformation::globalToLocal(ops, g));
            for (int i = 0; i < 12; ++i)
                TEST_CHECK(approxEqual(back[i], g[i], 1e-14), "Test 109: global → local → global = identité");
            // Le vecteur axial global (pJ - pI) n'a qu'une composante locale x.
            const Vec12 axial { pJ[0] - pI[0], pJ[1] - pI[1], pJ[2] - pI[2], 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            const Vec12 la = ElementTransformation::globalToLocal(ops, axial);
            TEST_CHECK(approxEqual(la[0], ops.length, 1e-12) && approxEqual(la[1], 0.0, 1e-12) && approxEqual(la[2], 0.0, 1e-12),
                       "Test 109: axe de la barre = x local");
        }
        // Poteau vertical : référence globale Y (repère TSA) ; OpenSees doit l'accepter.
        const gp_Ax3 vert = LoadResolver::computeElementLocalAxes(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 3), 0.0);
        const LocalAxes v = ElementTransformation::openSeesAxes({ 0, 0, 0 }, { 0, 0, 3 },
            { vert.Direction().X(), vert.Direction().Y(), vert.Direction().Z() });
        TEST_CHECK(v.valid && approxEqual(v.x[2], 1.0, 1e-15), "Test 109: poteau vertical");

        // Élément réel orienté : localForce OpenSees == T · globalForce OpenSees.
        Model m;
        int n1 = m.addNode(pI[0], pI[1], pI[2]), n2 = m.addNode(pJ[0], pJ[1], pJ[2]);
        m.getNode(n1)->setSupport(SupportDefinition::fixed());
        int bb = m.addBar(n1, n2, Section::ipe(200), Material::steelS235(), BarRole::Beam);
        m.getBeam(bb)->setRotation(30.0);
        int lc = addCase(m);
        m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 3.0, -4.0, -6.0, 0.5, -0.7, 0.2));
        AnalysisParameters p;
        p.includeSelfWeight = false;
        p.extractionLevel = ExtractionLevel::Advanced;
        ResultsModel r;
        TEST_CHECK(runSolve(m, p, r), "Test 109: calcul de l'élément orienté");
        const auto& fs = r.advanced().elementForces.at(ElementKey{ StructuralElementKind::Beam, bb });
        const auto& em = r.advanced().elementMatrices.at(ElementKey{ StructuralElementKind::Beam, bb });
        LocalAxes axes;
        axes.x = { em.rotation[0], em.rotation[1], em.rotation[2] };
        axes.y = { em.rotation[3], em.rotation[4], em.rotation[5] };
        axes.z = { em.rotation[6], em.rotation[7], em.rotation[8] };
        Vec12 g {};
        std::copy(fs.global.begin(), fs.global.end(), g.begin());
        const Vec12 l = ElementTransformation::globalToLocal(axes, g);
        for (int i = 0; i < 12; ++i)
            TEST_CHECK(std::abs(l[i] - fs.local[i]) <= 1e-12 * 10.0, "Test 109: T · globalForce == localForce (élément orienté β = 30°)");
        std::cout << "[PASS] Test 109: Transformations local/global" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 110 : équilibre ΣF + ΣR ≈ 0 avec charges réparties, poids propre, unités N
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 110: Équilibre global (kN et N) ---" << std::endl;
        for (bool kN : { true, false })
        {
            Model m;
            int n1 = m.addNode(0, 0, 0), n2 = m.addNode(6, 0, 0), n3 = m.addNode(6, 0, -3);
            m.getNode(n1)->setSupport(SupportDefinition::fixed());
            m.getNode(n3)->setSupport(SupportDefinition::fixed());
            int bb = m.addBar(n1, n2, Section::ipe(300), Material::steelS235(), BarRole::Beam);
            m.addColumn(n3, n2, Section::heb(200), Material::steelS235());
            int lc = addCase(m);
            m.loadManager().addMemberLoad(MemberLoad::uniform(bb, lc, 12.0));
            m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 7.0, 0.0, -10.0));
            AnalysisParameters p;
            p.useKiloNewtons = kN;
            p.includeSelfWeight = true;
            ResultsModel r;
            TEST_CHECK(runSolve(m, p, r), "Test 110: calcul");
            const double resid = r.executionMetadata().relativeEquilibriumResidual;
            std::cout << "  unités " << r.units().force << " : résidu relatif = " << resid << std::endl;
            TEST_CHECK(resid < 1e-9, "Test 110: Σ F_ext + Σ R ≈ 0 (tolérance relative 1e-9)");
            TEST_CHECK(r.units().force == (kN ? "kN" : "N"), "Test 110: unités déclarées");
            TEST_CHECK(r.advanced().elementMatrices.empty() && !r.advanced().available, "Test 110: mode Light sans matrices");
        }
        std::cout << "[PASS] Test 110: Équilibre global" << std::endl;
        passed++;
    }

    // -------------------------------------------------------------------------
    // TEST 111 : exports, contexte IA, coût LIGHT vs ADVANCED
    // -------------------------------------------------------------------------
    {
        std::cout << "\n--- TEST 111: Exports, contexte IA et coût de l'extraction ---" << std::endl;
        Model m;
        // Taille par défaut réduite ; TSA_EXTRACTION_BENCH=nx,ny pour les mesures de performance.
        int nx = 6, ny = 4;
        const QByteArray bench = qgetenv("TSA_EXTRACTION_BENCH");
        if (!bench.isEmpty())
        {
            const auto parts = bench.split(',');
            if (parts.size() == 2) { nx = parts[0].toInt(); ny = parts[1].toInt(); }
        }
        std::map<std::pair<int, int>, int> base, top;
        for (int i = 0; i <= nx; ++i)
            for (int j = 0; j <= ny; ++j)
            {
                base[{ i, j }] = m.addNode(i * 5.0, j * 6.0, 0.0);
                top[{ i, j }] = m.addNode(i * 5.0, j * 6.0, 3.5);
                m.getNode(base[{ i, j }])->setSupport(SupportDefinition::fixed());
                m.addColumn(base[{ i, j }], top[{ i, j }], Section::heb(240), Material::steelS235());
            }
        for (int i = 0; i <= nx; ++i)
            for (int j = 0; j <= ny; ++j)
            {
                if (i < nx) m.addBar(top[{ i, j }], top[{ i + 1, j }], Section::ipe(300), Material::steelS235(), BarRole::Beam);
                if (j < ny) m.addBar(top[{ i, j }], top[{ i, j + 1 }], Section::ipe(300), Material::steelS235(), BarRole::Beam);
            }
        int lc = addCase(m);
        for (const auto& [ij, n] : top) m.loadManager().addNodalLoad(NodalLoad(0, n, lc, 2.0, 1.0, -25.0));

        AnalysisParameters p;
        p.includeSelfWeight = true;
        ResultsModel light, adv;
        QElapsedTimer t;
        t.start();
        TEST_CHECK(runSolve(m, p, light), "Test 111: calcul LIGHT");
        const qint64 tLight = t.restart();
        p.extractionLevel = ExtractionLevel::Advanced;
        TEST_CHECK(runSolve(m, p, adv), "Test 111: calcul ADVANCED");
        const qint64 tAdv = t.elapsed();
        const auto& A = adv.advanced();
        std::cout << "  " << m.nodes().size() << " nœuds, " << (m.beams().size() + m.columns().size()) << " éléments, "
                  << A.dofMap.equationCount() << " équations" << std::endl;
        std::cout << "  LIGHT " << tLight << " ms | ADVANCED " << tAdv << " ms (passage matrices " << A.matrixRunDurationMs
                  << " ms) | mémoire avancée " << A.memoryBytes() / 1024 << " Kio, K_global nnz " << A.kGlobal.nonZeros()
                  << " / " << static_cast<long long>(A.kGlobal.rows) * A.kGlobal.cols << std::endl;
        TEST_CHECK(A.hasGlobalStiffness || A.dofMap.equationCount() > 1500, "Test 111: K_global extraite sous le plafond");

        // Les résultats courants sont identiques dans les deux modes.
        double dmax = 0.0;
        for (const auto& [id, d] : light.allDisplacements())
            dmax = std::max(dmax, std::abs(d.uz - adv.nodeDisplacement(id).uz));
        TEST_CHECK(dmax == 0.0, "Test 111: ADVANCED ne modifie pas les résultats LIGHT");

        // Plafond : au-delà, K_global non extraite mais raison explicite.
        p.maxGlobalStiffnessDofs = 10;
        ResultsModel capped;
        TEST_CHECK(runSolve(m, p, capped), "Test 111: calcul ADVANCED plafonné");
        TEST_CHECK(!capped.advanced().hasGlobalStiffness && !capped.advanced().kGlobalUnavailableReason.empty()
                   && capped.advanced().dofMap.equationCount() == A.dofMap.equationCount(),
                   "Test 111: plafond respecté, mapping et matrices élémentaires conservés");

        std::string err;
        TEST_CHECK(ResultsExport::render(light, ResultsDataset::GlobalStiffness, ResultsExportFormat::Csv, &err).empty() && !err.empty(),
                   "Test 111: export K refusé en mode Light avec message");
        const std::string kcsv = ResultsExport::render(adv, ResultsDataset::GlobalStiffness, ResultsExportFormat::Csv);
        TEST_CHECK(kcsv.find("row,col,row_dof,col_dof,value") != std::string::npos && kcsv.find("printA") != std::string::npos,
                   "Test 111: K_global CSV avec métadonnées");
        const std::string dcsv = ResultsExport::render(adv, ResultsDataset::Displacements, ResultsExportFormat::Csv);
        TEST_CHECK(std::count(dcsv.begin(), dcsv.end(), '\n') == static_cast<long>(m.nodes().size() + 1), "Test 111: CSV déplacements");
        const std::string json = ResultsExport::render(adv, ResultsDataset::All, ResultsExportFormat::Json);
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
        TEST_CHECK(doc.isObject() && doc.object()["dofMap"].toObject()["equations"].toArray().size() == A.dofMap.equationCount(),
                   "Test 111: JSON complet valide");
        const std::string txt = ResultsExport::render(adv, ResultsDataset::ElementStiffness, ResultsExportFormat::Txt);
        TEST_CHECK(txt.find("LOCAL STIFFNESS") != std::string::npos && txt.find("GLOBAL FORCES") != std::string::npos,
                   "Test 111: rapport TXT élémentaire");

        const int node = top[{ 3, 2 }];
        const QJsonObject ctx = ResultsContext::nodeContext(m, adv, node);
        TEST_CHECK(ctx["connectedElements"].toArray().size() == 5, "Test 111: contexte IA — 1 poteau + 4 poutres connectés");
        TEST_CHECK(ctx["dofs"].toArray().size() == 6 && ctx["dofs"].toArray()[2].toObject().contains("K_diagonal"),
                   "Test 111: contexte IA — rigidité diagonale du DDL");
        TEST_CHECK(ctx["displacement"].toObject()["UZ"].toDouble() < 0.0, "Test 111: contexte IA — déplacement");
        std::cout << "[PASS] Test 111: Exports et contexte IA" << std::endl;
        passed++;
    }

    return true;
}
