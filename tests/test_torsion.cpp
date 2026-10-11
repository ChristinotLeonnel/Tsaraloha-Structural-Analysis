// Suite « torsion » (tests 321-331) : moment de torsion appliqué à une barre, du calcul OpenSees jusqu'à la
// déformée 3D. Chaque grandeur est comparée à la RDM (θ = T·L / (G·J), flèches de console) ; la vue est
// contrôlée sur la géométrie produite (rotation des sections, repères, facteur d'échelle).
// Contrôle visuel facultatif : TSA_TEST_DUMP_DIR=<dossier> enregistre des captures du viewport réel.

#include "test_common.h"

#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/OpenSeesSolver.h"
#include "Geometry/BeamGeometry.h"
#include "Geometry/DeformedGeometry.h"
#include "Geometry/MemberLoadGlyph.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "IO/Tsa3d/Tsa3d.h"
#include "Analysis/Engine/AnalysisContext.h"
#include "Model/Load/LoadManager.h"
#include "Standards/ModelValidator.h"
#include "Viewer/OccView.h"
#include "Viewer/ResultsVisualManager.h"

#include <Poly_Triangulation.hxx>
#include <QThread>

using namespace TSA::Analysis;

namespace
{
bool relClose(double a, double b, double rel)
{
    return std::abs(a - b) <= rel * std::max({ std::abs(a), std::abs(b), 1e-300 });
}

bool solve(Model& model, AnalysisParameters params, ResultsModel& out)
{
    OpenSeesSolver solver;
    QString e;
    const bool ok = solver.solveSynchronous(model, params, &e);
    if (!ok) std::cout << "  Solver error: " << e.toStdString() << "\n" << solver.results().journalLog() << "\n";
    out = solver.results();
    return ok;
}

/// Module de cisaillement utilisé par le calcul (kN/m², comme OpenSeesAnalysisBuilder).
double shearModulus(const Material& m)
{
    return m.mechanical.youngModulus * 1e-3 / (2.0 * (1.0 + m.mechanical.poissonRatio));
}

DeformedAxis axisOf(const Model& m, const ResultsModel& r, StructuralElementKind kind, int id)
{
    const int i = kind == StructuralElementKind::Beam ? m.getBeam(id)->startNodeId() : m.getColumn(id)->startNodeId();
    const int j = kind == StructuralElementKind::Beam ? m.getBeam(id)->endNodeId() : m.getColumn(id)->endNodeId();
    const double beta = kind == StructuralElementKind::Beam ? m.getBeam(id)->rotation() : m.getColumn(id)->rotation();
    const auto* n1 = m.getNode(i);
    const auto* n2 = m.getNode(j);
    return DeformedGeometry::computeMemberAxis(gp_Pnt(n1->x(), n1->y(), n1->z()), gp_Pnt(n2->x(), n2->y(), n2->z()), beta,
                                               r.getNodeDisplacement(i), r.getNodeDisplacement(j), r.getElementResults(kind, id), true);
}

/// Angle signé (rad) du vecteur a vers b autour de l'axe unitaire t.
double signedAngle(const gp_Vec& a, const gp_Vec& b, const gp_Vec& t)
{
    return std::atan2(a.Crossed(b).Dot(t), a.Dot(b));
}

/// Étendue selon Y des sommets de la barre déformée situés à l'abscisse x (une section).
double sectionExtentY(const TopoDS_Shape& solid, double x)
{
    if (solid.IsNull()) return -1.0;
    TopLoc_Location loc;
    const auto tri = BRep_Tool::Triangulation(TopoDS::Face(solid), loc);
    if (tri.IsNull()) return -1.0;
    double lo = 1e300, hi = -1e300;
    for (int i = 1; i <= tri->NbNodes(); ++i)
    {
        const gp_Pnt p = tri->Node(i);
        if (std::abs(p.X() - x) > 1e-6) continue;
        lo = std::min(lo, p.Y());
        hi = std::max(hi, p.Y());
    }
    return hi - lo;
}

int wireCount(const TopoDS_Shape& s)
{
    int n = 0;
    for (TopExp_Explorer ex(s, TopAbs_WIRE); ex.More(); ex.Next()) ++n;
    return n;
}

/// Console le long de X, encastrée en x = 0 ; renvoie l'identifiant de la poutre.
int cantilever(Model& m, double L, const Section& sec, int* tip)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(L, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    *tip = b;
    return m.addBar(a, b, sec, Material::steelS235(), BarRole::Beam);
}

/// Captures du viewport OCCT réel (contrôle visuel), seulement si TSA_TEST_DUMP_DIR est défini.
void dumpViews(Model& m, const std::shared_ptr<ResultsModel>& results, const QString& name, bool twist,
               ScalePreset preset = ScalePreset::Auto, double custom = 1.0)
{
    const QString dir = qEnvironmentVariable("TSA_TEST_DUMP_DIR");
    if (dir.isEmpty()) return;
    OccView view;
    view.resize(960, 640);
    view.show();
    for (int k = 0; k < 20; ++k) { QApplication::processEvents(); QThread::msleep(10); }
    view.setModel(&m);
    view.setResultsModel(results);
    view.resultsVisual()->setSectionRotationVisible(twist);
    view.resultsVisual()->setDeformationScalePreset(preset, custom);
    for (int k = 0; k < 10; ++k) QApplication::processEvents();
    view.viewIsometric();
    view.fitModel();
    for (int k = 0; k < 10; ++k) QApplication::processEvents();
    view.captureViewImage(960, 640).save(dir + "/" + name + "_iso.png");
    view.viewRight();
    view.fitModel();
    for (int k = 0; k < 10; ++k) QApplication::processEvents();
    view.captureViewImage(960, 640).save(dir + "/" + name + "_bout.png");
    std::cout << "  captures : " << (dir + "/" + name).toStdString() << "_iso.png, _bout.png ; échelle ×"
              << view.resultsVisual()->deformationScale() << ", repères de torsion : " << view.resultsVisual()->sectionMarkerCount()
              << "\n  légende : " << view.resultsVisual()->legendSummaryText().toStdString() << "\n";
    view.setResultsModel(nullptr);
    view.setModel(nullptr);
}

/// Moment réparti (couple par unité de longueur) sur une barre.
MemberLoad couple(int bar, int lc, double q1, double q2, LoadDirection dir, double x1 = 0.0, double x2 = 0.0)
{
    MemberLoad l(0, bar, lc, LoadType::MemberMoment, q1, q2, dir, MemberLoad::coordSystemFor(dir), x1, x2);
    l.setName("couple réparti");
    return l;
}

int addCase(Model& m, const char* name = "CAS")
{
    return m.loadManager().addLoadCase(LoadCase(0, name, LoadCaseCategory::Live, false, 1.0));
}

bool hasIssue(const TSA::Standards::ModelValidationReport& report, TSA::Standards::ValidationSeverity sev, const char* text)
{
    for (const auto& i : report.issues())
        if (i.severity == sev && i.message.find(text) != std::string::npos) return true;
    return false;
}

/// Profil laminé publié : British Steel, fiches « European IPE sections » et « European HE sections » du 21.07.26
/// (dimensions, rayon de congé r, constante de torsion J en cm⁴, constante de gauchissement H en dm⁶).
struct RolledReference
{
    const char* name;
    double h, b, tw, tf, r;  // mm
    double J;                // cm⁴
    double H;                // dm⁶ (3 chiffres significatifs imprimés)
};
const RolledReference kBritishSteel[] = {
    { "HE 100 A", 96.0, 100.0, 5.0, 8.0, 12.0, 5.28, 0.0026 },
    { "HE 100 B", 100.0, 100.0, 6.0, 10.0, 12.0, 9.33, 0.0034 },
    { "HE 120 A", 114.0, 120.0, 5.0, 8.0, 12.0, 6.04, 0.0065 },
    { "HE 120 B", 120.0, 120.0, 6.5, 11.0, 12.0, 13.9, 0.0094 },
    { "HE 140 A", 133.0, 140.0, 5.5, 8.5, 12.0, 8.10, 0.0151 },
    { "HE 140 B", 140.0, 140.0, 7.0, 12.0, 12.0, 20.2, 0.0225 },
    { "HE 160 A", 152.0, 160.0, 6.0, 9.0, 15.0, 12.1, 0.0315 },
    { "HE 160 B", 160.0, 160.0, 8.0, 13.0, 15.0, 31.3, 0.0480 },
    { "HE 180 A", 171.0, 180.0, 6.0, 9.5, 16.0, 15.5, 0.0603 },
    { "HE 180 B", 180.0, 180.0, 8.5, 14.0, 16.0, 43.2, 0.0939 },
    { "HE 200 A", 190.0, 200.0, 6.5, 10.0, 20.0, 23.1, 0.108 },
    { "HE 200 B", 200.0, 200.0, 9.0, 15.0, 20.0, 62.8, 0.171 },
    { "HE 220 A", 210.0, 220.0, 7.0, 11.0, 20.0, 30.8, 0.194 },
    { "HE 220 B", 220.0, 220.0, 9.5, 16.0, 20.0, 80.4, 0.296 },
    { "HE 240 A", 230.0, 240.0, 7.5, 12.0, 20.0, 40.7, 0.329 },
    { "HE 240 B", 240.0, 240.0, 10.0, 17.0, 20.0, 102, 0.488 },
    { "HE 260 A", 250.0, 260.0, 7.5, 12.5, 20.0, 47.5, 0.517 },
    { "HE 260 B", 260.0, 260.0, 10.0, 17.5, 20.0, 117, 0.754 },
    { "HE 280 A", 270.0, 280.0, 8.0, 13.0, 27.0, 70.5, 0.787 },
    { "HE 280 B", 280.0, 280.0, 10.5, 18.0, 27.0, 156, 1.13 },
    { "HE 300 A", 290.0, 300.0, 8.5, 14.0, 27.0, 87.8, 1.20 },
    { "HE 300 B", 300.0, 300.0, 11.0, 19.0, 27.0, 189, 1.69 },
    { "HE 320 A", 310.0, 300.0, 9.0, 15.5, 27.0, 112, 1.51 },
    { "HE 320 B", 320.0, 300.0, 11.5, 20.5, 27.0, 230, 2.07 },
    { "HE 340 A", 330.0, 300.0, 9.5, 16.5, 27.0, 131, 1.83 },
    { "HE 340 B", 340.0, 300.0, 12.0, 21.5, 27.0, 263, 2.46 },
    { "HE 360 A", 350.0, 300.0, 10.0, 17.5, 27.0, 153, 2.18 },
    { "HE 360 B", 360.0, 300.0, 12.5, 22.5, 27.0, 298, 2.89 },
    { "HE 400 A", 390.0, 300.0, 11.0, 19.0, 27.0, 193, 2.95 },
    { "HE 400 B", 400.0, 300.0, 13.5, 24.0, 27.0, 361, 3.82 },
    { "HE 450 A", 440.0, 300.0, 11.5, 21.0, 27.0, 250, 4.15 },
    { "HE 450 B", 450.0, 300.0, 14.0, 26.0, 27.0, 448, 5.27 },
    { "HE 500 A", 490.0, 300.0, 12.0, 23.0, 27.0, 318, 5.65 },
    { "HE 500 B", 500.0, 300.0, 14.5, 28.0, 27.0, 548, 7.03 },
    { "HE 550 A", 540.0, 300.0, 12.5, 24.0, 27.0, 360, 7.20 },
    { "HE 550 B", 550.0, 300.0, 15.0, 29.0, 27.0, 610, 8.87 },
    { "HE 600 A", 590.0, 300.0, 13.0, 25.0, 27.0, 407, 9.00 },
    { "HE 600 B", 600.0, 300.0, 15.5, 30.0, 27.0, 677, 11.0 },
    { "HE 650 A", 640.0, 300.0, 13.5, 26.0, 27.0, 458, 11.0 },
    { "HE 650 B", 650.0, 300.0, 16.0, 31.0, 27.0, 749, 13.4 },
    { "HE 700 A", 690.0, 300.0, 14.5, 27.0, 27.0, 522, 13.4 },
    { "HE 700 B", 700.0, 300.0, 17.0, 32.0, 27.0, 839, 16.1 },
    { "HE 800 A", 790.0, 300.0, 15.0, 28.0, 27.0, 588, 18.3 },
    { "HE 800 B", 800.0, 300.0, 17.5, 33.0, 27.0, 932, 21.9 },
    { "HE 900 A", 890.0, 300.0, 16.0, 30.0, 30.0, 749, 25.0 },
    { "HE 900 B", 900.0, 300.0, 18.5, 35.0, 30.0, 1150, 29.6 },
    { "HE 1000 A", 990.0, 300.0, 16.5, 31.0, 30.0, 835, 32.2 },
    { "HE 1000 B", 1000.0, 300.0, 19.0, 36.0, 30.0, 1267, 37.8 },
    { "IPE 100", 100.0, 55.0, 4.1, 5.7, 7.0, 1.16, 0.0004 },
    { "IPE 120", 120.0, 64.0, 4.4, 6.3, 7.0, 1.69, 0.0009 },
    { "IPE 140", 140.0, 73.0, 4.7, 6.9, 7.0, 2.40, 0.0020 },
    { "IPE 160", 160.0, 82.0, 5.0, 7.4, 9.0, 3.54, 0.0040 },
    { "IPE 180", 180.0, 91.0, 5.3, 8.0, 9.0, 4.73, 0.0075 },
    { "IPE 200", 200.0, 100.0, 5.6, 8.5, 12.0, 6.92, 0.0131 },
    { "IPE 220", 220.0, 110.0, 5.9, 9.2, 12.0, 9.03, 0.0228 },
    { "IPE 240", 240.0, 120.0, 6.2, 9.8, 15.0, 13.0, 0.0376 },
    { "IPE 270", 270.0, 135.0, 6.6, 10.2, 15.0, 15.9, 0.0708 },
    { "IPE 300", 300.0, 150.0, 7.1, 10.7, 15.0, 19.9, 0.126 },
    { "IPE 330", 330.0, 160.0, 7.5, 11.5, 20.0, 30.4, 0.200 },
    { "IPE 360", 360.0, 170.0, 8.0, 12.7, 16.0, 35.3, 0.314 },
    { "IPE 400", 400.0, 180.0, 8.6, 13.5, 20.0, 49.7, 0.492 },
    { "IPE 450", 450.0, 190.0, 9.4, 14.6, 20.0, 65.0, 0.794 },
    { "IPE 500", 500.0, 200.0, 10.2, 16.0, 20.0, 87.1, 1.25 },
    { "IPE 550", 550.0, 210.0, 11.1, 17.2, 27.0, 132, 1.90 },
    { "IPE 600", 600.0, 220.0, 12.0, 19.0, 20.0, 154, 2.86 },
};

/// Tests 326-331 : couple inversé, couple réparti, import TSA3D, barre inclinée, appuis, vue.
bool runTorsionMechanics(int& passed)
{
    const Section sec = Section::rectangular(0.2, 0.4);
    const Material mat = Material::steelS235();
    const double G = shearModulus(mat), J = sec.it(), GJ = G * J, E = mat.mechanical.youngModulus * 1e-3;
    AnalysisParameters p;
    p.includeSelfWeight = false;

    // TEST 326 (B) : inversion du couple — rotation et réactions changent de signe, même amplitude
    {
        std::cout << "\n--- TEST 326: Inversion du couple ---" << std::endl;
        const double L = 3.0, T = 10.0, m = 4.0;
        double rx[2] = {}, mx[2] = {}, rxq[2] = {}, mxq[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            const double sgn = k == 0 ? 1.0 : -1.0;
            Model mdl;
            int tip = 0;
            const int bar = cantilever(mdl, L, sec, &tip);
            const int lcT = addCase(mdl, "T"), lcM = addCase(mdl, "m");
            mdl.loadManager().addNodalLoad(NodalLoad(0, tip, lcT, 0, 0, 0, sgn * T, 0, 0));
            mdl.loadManager().addMemberLoad(couple(bar, lcM, sgn * m, sgn * m, LoadDirection::LocalX));
            ResultsModel r;
            p.targetLoadCaseId = lcT;
            TEST_CHECK(solve(mdl, p, r), "Test 326: calcul couple concentré");
            rx[k] = r.nodeDisplacement(tip).rx;
            mx[k] = r.getNodeReaction(mdl.getBeam(bar)->startNodeId()) ? r.getNodeReaction(mdl.getBeam(bar)->startNodeId())->mx : 0.0;
            p.targetLoadCaseId = lcM;
            TEST_CHECK(solve(mdl, p, r), "Test 326: calcul couple réparti");
            rxq[k] = r.nodeDisplacement(tip).rx;
            mxq[k] = r.getNodeReaction(mdl.getBeam(bar)->startNodeId()) ? r.getNodeReaction(mdl.getBeam(bar)->startNodeId())->mx : 0.0;
        }
        p.targetLoadCaseId = 0;
        std::cout << "  θ(+T) = " << rx[0] << ", θ(−T) = " << rx[1] << " ; réaction " << mx[0] << " / " << mx[1] << std::endl;
        TEST_CHECK(relClose(rx[0], T * L / GJ, 1e-9) && relClose(rx[1], -rx[0], 1e-12) && relClose(mx[1], -mx[0], 1e-12)
                       && relClose(mx[0], -T, 1e-9),
                   "Test 326: couple concentré — θ et réaction opposés, |θ| = TL/GJ, réaction = −T");
        TEST_CHECK(relClose(rxq[1], -rxq[0], 1e-12) && relClose(mxq[1], -mxq[0], 1e-12) && relClose(mxq[0], -m * L, 1e-9),
                   "Test 326: couple réparti — θ et réaction opposés, réaction = −m·L");
        std::cout << "[PASS] Test 326: Inversion du couple" << std::endl;
        ++passed;
    }

    // TEST 327 (C) : couple réparti — T(x) = m(L − x), θ(L) = m L² / 2GJ, distributions variables, flexion
    {
        std::cout << "\n--- TEST 327: Couple réparti ---" << std::endl;
        const double L = 3.0, m = 2.0;
        // 1. Uniforme sur console
        {
            Model mdl;
            int tip = 0;
            const int bar = cantilever(mdl, L, sec, &tip);
            const int lc = addCase(mdl);
            mdl.loadManager().addMemberLoad(couple(bar, lc, m, m, LoadDirection::LocalX));
            TEST_CHECK(!hasIssue(TSA::Standards::ModelValidator::validateForAnalysis(mdl, p), TSA::Standards::ValidationSeverity::Warning, "moment réparti"),
                       "Test 327: moment réparti transmis (plus d'avertissement « non pris en compte »)");
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 327: calcul console, couple uniforme");
            const double ref = m * L * L / (2.0 * GJ);
            std::cout << "  θ(L) = " << r.nodeDisplacement(tip).rx << " ; référence m·L²/(2GJ) = " << ref << std::endl;
            TEST_CHECK(relClose(r.nodeDisplacement(tip).rx, ref, 1e-9), "Test 327: θ(L) = m·L²/(2GJ)");
            const auto& eq = r.equilibrium();
            TEST_CHECK(relClose(eq.appliedMx, m * L, 1e-12) && relClose(eq.reactionMx, -m * L, 1e-9) && std::abs(eq.errorMx()) < 1e-9 * m * L
                           && r.executionMetadata().isEquilibriumVerified,
                       "Test 327: équilibre global — couple appliqué m·L = −réaction");
            const auto* er = r.getElementResults(StructuralElementKind::Beam, bar);
            TEST_CHECK(er && relClose(er->startForces.Mx, m * L, 1e-9) && std::abs(er->endForces.Mx) < 1e-9 * m * L,
                       "Test 327: Mx(0) = m·L, Mx(L) = 0");
            double worstT = 0.0, worstTheta = 0.0;
            for (const auto& s : er->intermediateStations)
            {
                const double x = s.position;
                worstT = std::max(worstT, std::abs(s.Mx - m * (L - x)));
                worstTheta = std::max(worstTheta, std::abs(s.rx - m * (L * x - 0.5 * x * x) / GJ));
            }
            std::cout << "  écart max T(x) = " << worstT << " kNm ; écart max θ(x) = " << worstTheta / ref << " × θ(L)" << std::endl;
            TEST_CHECK(worstT < 1e-9 * m * L, "Test 327: T_int(x) = m·(L − x) aux stations");
            TEST_CHECK(worstTheta < 1e-9 * ref, "Test 327: θ(x) = m·(L·x − x²/2)/GJ aux stations (parabolique)");
        }
        // 2. Linéaire m1 → m2 sur toute la barre, puis uniforme partielle [a, b]
        {
            const double m1 = 1.0, m2 = 3.0, a = 1.0, b = 2.5;
            Model mdl;
            int tip = 0;
            const int bar = cantilever(mdl, L, sec, &tip);
            const int lc1 = addCase(mdl, "lin"), lc2 = addCase(mdl, "part");
            mdl.loadManager().addMemberLoad(couple(bar, lc1, m1, m2, LoadDirection::LocalX));
            mdl.loadManager().addMemberLoad(couple(bar, lc2, m, m, LoadDirection::LocalX, a, b));
            ResultsModel r;
            p.targetLoadCaseId = lc1;
            TEST_CHECK(solve(mdl, p, r), "Test 327: calcul couple linéaire");
            TEST_CHECK(relClose(r.nodeDisplacement(tip).rx, L * L * (m1 / 6.0 + m2 / 3.0) / GJ, 1e-9)
                           && relClose(r.getNodeReaction(mdl.getBeam(bar)->startNodeId())->mx, -(m1 + m2) * L / 2.0, 1e-9),
                       "Test 327: couple linéaire — θ(L) = L²(m1/6 + m2/3)/GJ, réaction = −(m1 + m2)L/2");
            p.targetLoadCaseId = lc2;
            TEST_CHECK(solve(mdl, p, r), "Test 327: calcul couple partiel");
            TEST_CHECK(relClose(r.nodeDisplacement(tip).rx, m * (b * b - a * a) / (2.0 * GJ), 1e-9)
                           && relClose(r.getNodeReaction(mdl.getBeam(bar)->startNodeId())->mx, -m * (b - a), 1e-9),
                       "Test 327: couple partiel [a, b] — θ(L) = m(b² − a²)/2GJ, réaction = −m(b − a)");
            p.targetLoadCaseId = 0;
        }
        // 3. Bi-encastrée, couple uniforme : réactions −m·L/2, θ(L/2) = m·L²/8GJ (reconstruction en travée)
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(L, 0, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::fixed());
            mdl.getNode(b)->setSupport(SupportDefinition::fixed());
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addMemberLoad(couple(bar, addCase(mdl), m, m, LoadDirection::LocalX));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 327: calcul bi-encastrée");
            const auto* er = r.getElementResults(StructuralElementKind::Beam, bar);
            double mid = 0.0;
            for (const auto& s : er->intermediateStations)
                if (std::abs(s.position - L / 2) < 1e-9) mid = s.rx;
            TEST_CHECK(relClose(r.getNodeReaction(a)->mx, -m * L / 2, 1e-9) && relClose(r.getNodeReaction(b)->mx, -m * L / 2, 1e-9)
                           && relClose(mid, m * L * L / (8.0 * GJ), 1e-9),
                       "Test 327: bi-encastrée — réactions −m·L/2, θ(L/2) = m·L²/(8GJ)");
        }
        // 4. Moment de flexion réparti (axe local z) : v(L) = m·L³/3EI, θ(L) = m·L²/2EI, Mz(x) = m(L − x)
        {
            Model mdl;
            int tip = 0;
            const int bar = cantilever(mdl, L, sec, &tip);
            mdl.loadManager().addMemberLoad(couple(bar, addCase(mdl), m, m, LoadDirection::LocalZ));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 327: calcul flexion répartie");
            const auto d = r.nodeDisplacement(tip);
            const double EIz = E * sec.iz();
            const double rot = std::sqrt(d.rx * d.rx + d.ry * d.ry + d.rz * d.rz), u = std::sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz);
            TEST_CHECK(relClose(rot, m * L * L / (2.0 * EIz), 1e-9) && relClose(u, m * L * L * L / (3.0 * EIz), 1e-9) && std::abs(d.rx) < 1e-15,
                       "Test 327: couple de flexion réparti — θ = m·L²/2EI, v = ∫M(L − x)/EI = m·L³/3EI, aucune torsion");
            const auto* er = r.getElementResults(StructuralElementKind::Beam, bar);
            double worst = 0.0;
            for (const auto& s : er->intermediateStations) worst = std::max(worst, std::abs(std::abs(s.Mz) - m * (L - s.position)));
            TEST_CHECK(worst < 1e-9 * m * L && r.executionMetadata().isEquilibriumVerified, "Test 327: |Mz(x)| = m(L − x), équilibre vérifié");
        }
        std::cout << "[PASS] Test 327: Couple réparti (uniforme, linéaire, partiel, bi-encastré, flexion)" << std::endl;
        ++passed;
    }

    // TEST 328 (D) : moment de torsion réparti importé d'un fichier TSA3D (unités en millimètres)
    {
        std::cout << "\n--- TEST 328: Couple réparti importé (TSA3D) ---" << std::endl;
        const QString path = QStringLiteral(TSA_SOURCE_DIR) + "/tests/data/torsion_repartie.tsa3d";
        QJsonObject doc;
        TSA::IO::Tsa3d::Report report;
        TEST_CHECK(TSA::IO::Tsa3d::readFile(path, &doc, &report), "Test 328: lecture du fichier TSA3D");
        Model mdl;
        const auto res = TSA::IO::Tsa3d::importDocument(doc, mdl);
        TEST_CHECK(res.ok, "Test 328: import validé");
        TEST_CHECK(mdl.loadManager().memberLoads().size() == 1, "Test 328: une charge sur barre importée");
        const MemberLoad& l = mdl.loadManager().memberLoads().begin()->second;
        TEST_CHECK(l.type() == LoadType::MemberMoment && l.direction() == LoadDirection::LocalX && relClose(l.q1(), 2.0, 1e-12)
                       && relClose(l.q2(), 2.0, 1e-12),
                   "Test 328: moment réparti local x, 2 kN·m/m (kN·mm/mm converti sans facteur 1000) — q1 = " + std::to_string(l.q1()));
        const int bar = mdl.beams().begin()->first;
        const int tip = mdl.getBeam(bar)->endNodeId();
        TEST_CHECK(std::abs(mdl.getNode(tip)->x() - 3.0) < 1e-12, "Test 328: longueur 3000 mm = 3 m");
        ResultsModel r;
        TEST_CHECK(solve(mdl, p, r), "Test 328: calcul du modèle importé");
        const Section& s = mdl.getBeam(bar)->section();
        const double ref = 2.0 * 9.0 / (2.0 * shearModulus(mdl.getBeam(bar)->material()) * s.it());
        std::cout << "  θ(L) = " << r.nodeDisplacement(tip).rx << " ; référence = " << ref << std::endl;
        TEST_CHECK(relClose(r.nodeDisplacement(tip).rx, ref, 1e-9), "Test 328: la charge importée atteint le solveur — θ(L) = m·L²/(2GJ)");
        std::cout << "[PASS] Test 328: Couple réparti importé (TSA3D)" << std::endl;
        ++passed;
    }

    // TEST 329 (E) : barre inclinée dans l'espace (β = 30°) — couple autour de l'axe local x
    {
        std::cout << "\n--- TEST 329: Barre inclinée ---" << std::endl;
        const gp_Pnt p1(0, 0, 0), p2(3.0, 2.0, 1.5);
        const double L = p1.Distance(p2), T = 7.0, m = 1.5, beta = 30.0;
        const gp_Vec ex = gp_Vec(p1, p2).Normalized();
        Model mdl;
        const int a = mdl.addNode(p1.X(), p1.Y(), p1.Z()), b = mdl.addNode(p2.X(), p2.Y(), p2.Z());
        mdl.getNode(a)->setSupport(SupportDefinition::fixed());
        const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam, beta);
        const int lcT = addCase(mdl, "T"), lcM = addCase(mdl, "m");
        mdl.loadManager().addNodalLoad(NodalLoad(0, b, lcT, 0, 0, 0, T * ex.X(), T * ex.Y(), T * ex.Z())); // vecteur T·x (global)
        mdl.loadManager().addMemberLoad(couple(bar, lcM, m, m, LoadDirection::LocalX));
        auto r = std::make_shared<ResultsModel>();
        p.targetLoadCaseId = lcT;
        TEST_CHECK(solve(mdl, p, *r), "Test 329: calcul couple concentré");
        auto d = r->nodeDisplacement(b);
        const gp_Vec R(d.rx, d.ry, d.rz);
        TEST_CHECK(relClose(R.Dot(ex), T * L / GJ, 1e-9) && R.Crossed(ex).Magnitude() < 1e-9 * R.Magnitude()
                       && std::abs(d.ux) + std::abs(d.uy) + std::abs(d.uz) < 1e-12,
                   "Test 329: rotation portée par l'axe de la barre, θ = T·L/GJ, aucune translation");
        const DeformedAxis axis = axisOf(mdl, *r, StructuralElementKind::Beam, bar);
        const double s = 40.0;
        const auto frames = DeformedGeometry::sectionFrames(axis, beta, s, true);
        const auto base = BeamGeometry::sectionFrame(p1, p2, beta);
        TEST_CHECK(relClose(axis.twist.back(), T * L / GJ, 1e-9) && relClose(signedAngle(base.x, frames.back().x, ex), s * T * L / GJ, 1e-9)
                       && std::abs(signedAngle(base.x, frames.front().x, ex)) < 1e-12,
                   "Test 329: vue — section d'extrémité tournée de s·θ autour de l'axe incliné");
        p.targetLoadCaseId = lcM;
        TEST_CHECK(solve(mdl, p, *r), "Test 329: calcul couple réparti");
        d = r->nodeDisplacement(b);
        const gp_Vec Rm(d.rx, d.ry, d.rz);
        TEST_CHECK(relClose(Rm.Dot(ex), m * L * L / (2.0 * GJ), 1e-9) && Rm.Crossed(ex).Magnitude() < 1e-9 * Rm.Magnitude()
                       && r->executionMetadata().isEquilibriumVerified,
                   "Test 329: couple réparti local x — θ = m·L²/2GJ autour de l'axe, équilibre vérifié");
        p.targetLoadCaseId = 0;
        std::cout << "[PASS] Test 329: Barre inclinée" << std::endl;
        ++passed;
    }

    // TEST 330 (F) : conditions aux limites — fourches, ressort, bi-encastrement, mécanismes détectés
    {
        std::cout << "\n--- TEST 330: Conditions aux limites ---" << std::endl;
        // 1. Poutre selon Y sur deux appuis articulés (échouait : matrice singulière ; flexion bloquée par RX)
        {
            const double L = 6.0, q = 10.0;
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(0, L, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::pinned());
            mdl.getNode(b)->setSupport(SupportDefinition::pinned());
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addMemberLoad(MemberLoad::uniform(bar, addCase(mdl), q));
            const auto snap = CalculationSnapshot::capture(mdl);
            bool forkY = snap.automaticRestraints().size() == 2;
            for (const auto& ar : snap.automaticRestraints()) forkY &= ar.dof == 4 && ar.reason.find("fourche") != std::string::npos;
            TEST_CHECK(forkY, "Test 330: appuis articulés d'une poutre selon Y — fourche RY tracée (et non RX)");
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 330: calcul de la poutre selon Y");
            double w = 0.0;
            for (const auto& s : r.getElementResults(StructuralElementKind::Beam, bar)->intermediateStations)
                if (std::abs(s.position - L / 2) < 1e-9) w = s.uz;
            // Flèche en travée : double intégration numérique de la courbure (400 pas) → écart ~1e-5.
            TEST_CHECK(relClose(w, -5.0 * q * std::pow(L, 4) / (384.0 * E * sec.iy()), 2e-5) && r.executionMetadata().warnings.empty(),
                       "Test 330: flèche 5qL⁴/384EI (appuis réellement articulés en flexion), fourches sans effort");
        }
        // 2. Poutre selon X articulée aux deux bouts, couple au milieu : repris par les fourches → signalé
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), c = mdl.addNode(2, 0, 0), b = mdl.addNode(4, 0, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::pinned());
            mdl.getNode(b)->setSupport(SupportDefinition::pinned());
            mdl.addBar(a, c, sec, mat, BarRole::Beam);
            mdl.addBar(c, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addNodalLoad(NodalLoad(0, c, addCase(mdl), 0, 0, 0, 6.0, 0, 0));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 330: calcul poutre articulée en torsion");
            const auto& w = r.executionMetadata().warnings;
            TEST_CHECK(w.size() == 2 && w.front().find("reprend") != std::string::npos && relClose(r.getNodeReaction(a)->mx, -3.0, 1e-9)
                           && relClose(r.getNodeReaction(b)->mx, -3.0, 1e-9),
                   "Test 330: couple repris par les fourches automatiques (−3 kNm chacune) et signalé");
        }
        // 3. Bi-encastrée, couple T en x = a : réactions −T·b/L et −T·a/L, θ(a) = T·a·b/(GJ·L)
        {
            const double L = 3.0, aPos = 1.0, T = 9.0;
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), c = mdl.addNode(aPos, 0, 0), b = mdl.addNode(L, 0, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::fixed());
            mdl.getNode(b)->setSupport(SupportDefinition::fixed());
            mdl.addBar(a, c, sec, mat, BarRole::Beam);
            mdl.addBar(c, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addNodalLoad(NodalLoad(0, c, addCase(mdl), 0, 0, 0, T, 0, 0));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 330: calcul bi-encastrée");
            const double bPos = L - aPos;
            TEST_CHECK(relClose(r.getNodeReaction(a)->mx, -T * bPos / L, 1e-9) && relClose(r.getNodeReaction(b)->mx, -T * aPos / L, 1e-9)
                           && relClose(r.nodeDisplacement(c).rx, T * aPos * bPos / (GJ * L), 1e-9),
                       "Test 330: bi-encastrée — réactions −T·b/L, −T·a/L, θ(a) = T·a·b/(GJ·L)");
        }
        // 3b. Barre unique bi-encastrée (aucun DDL libre : échouait dans OpenSees) — flexion qL²/12, qL⁴/384EI
        {
            const double L = 6.0, q = 10.0;
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(L, 0, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::fixed());
            mdl.getNode(b)->setSupport(SupportDefinition::fixed());
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addMemberLoad(MemberLoad::uniform(bar, addCase(mdl), q));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 330: calcul barre bi-encastrée sans DDL libre");
            const auto* er = r.getElementResults(StructuralElementKind::Beam, bar);
            double w = 0.0;
            for (const auto& s : er->intermediateStations)
                if (std::abs(s.position - L / 2) < 1e-9) w = s.uz;
            TEST_CHECK(relClose(std::abs(er->startForces.My), q * L * L / 12.0, 1e-9) && relClose(w, -q * std::pow(L, 4) / (384.0 * E * sec.iy()), 1e-4) /* intégration numérique de la courbure */
                           && relClose(r.getNodeReaction(a)->rz, q * L / 2.0, 1e-9),
                       "Test 330: bi-encastrée seule — M = qL²/12, w = qL⁴/384EI, R = qL/2");
        }
        // 4. Ressort de torsion à l'appui : θ = T/k + T·L/GJ
        {
            const double L = 3.0, T = 5.0, k = 2000.0;
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(L, 0, 0);
            using DS = TSA::Model::DOFState;
            auto s = SupportDefinition::custom(DS::Fixed, DS::Fixed, DS::Fixed, DS::Spring, DS::Fixed, DS::Fixed);
            s.setKrx(k);
            mdl.getNode(a)->setSupport(s);
            mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addNodalLoad(NodalLoad(0, b, addCase(mdl), 0, 0, 0, T, 0, 0));
            ResultsModel r;
            TEST_CHECK(solve(mdl, p, r), "Test 330: calcul avec ressort de torsion");
            TEST_CHECK(relClose(r.nodeDisplacement(b).rx, T / k + T * L / GJ, 1e-9), "Test 330: ressort de torsion — θ = T/k + T·L/GJ");
        }
        // 5. Rotation autour de l'axe libre aux deux appuis : mécanisme détecté avant le calcul
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(4, 0, 0);
            using DS = TSA::Model::DOFState;
            const auto free = SupportDefinition::custom(DS::Fixed, DS::Fixed, DS::Fixed, DS::Free, DS::Fixed, DS::Fixed);
            mdl.getNode(a)->setSupport(free);
            mdl.getNode(b)->setSupport(free);
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addNodalLoad(NodalLoad(0, b, addCase(mdl), 0, 0, 0, 5.0, 0, 0));
            TEST_CHECK(hasIssue(TSA::Standards::ModelValidator::validateForAnalysis(mdl, p), TSA::Standards::ValidationSeverity::Error,
                                ("Mécanisme de torsion : poutre " + std::to_string(bar)).c_str()),
                       "Test 330: mécanisme de torsion détecté avant le calcul (barre désignée)");
            OpenSeesSolver solver;
            QString e;
            TEST_CHECK(!solver.solveSynchronous(mdl, p, &e) && e.contains(QStringLiteral("Mécanisme de torsion")),
                       "Test 330: calcul refusé avec le message de mécanisme");
        }
        // 6. Mécanisme non détecté avant le calcul (barres non alignées) : résultats absurdes refusés, message explicite
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(3, 0, 0), c = mdl.addNode(3, 3, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::pinned()); // un seul appui articulé : rotation libre de l'ensemble
            mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.addBar(b, c, sec, mat, BarRole::Beam);
            mdl.loadManager().addNodalLoad(NodalLoad(0, c, addCase(mdl), 0, 0, -5.0));
            OpenSeesSolver solver;
            QString e;
            const bool ok = solver.solveSynchronous(mdl, p, &e);
            std::cout << "  message : " << e.toStdString() << std::endl;
            TEST_CHECK(!ok && e.contains(QStringLiteral("mécanisme")) && !solver.results().isValid(),
                       "Test 330: mécanisme quasi singulier (déplacements de 10¹¹ m auparavant présentés comme résultats) refusé explicitement");
        }
        std::cout << "[PASS] Test 330: Conditions aux limites" << std::endl;
        ++passed;
    }

    // TEST 331 (G) : visualisation d'une torsion répartie — rotations et coordonnées des sections
    {
        std::cout << "\n--- TEST 331: Vue d'une torsion répartie ---" << std::endl;
        const double L = 3.0, m = 2.0;
        Model mdl;
        int tip = 0;
        const int bar = cantilever(mdl, L, sec, &tip);
        mdl.loadManager().addMemberLoad(couple(bar, addCase(mdl), m, m, LoadDirection::LocalX));
        auto r = std::make_shared<ResultsModel>();
        TEST_CHECK(solve(mdl, p, *r), "Test 331: calcul");
        const DeformedAxis axis = axisOf(mdl, *r, StructuralElementKind::Beam, bar);
        auto theta = [&](double x) { return m * (L * x - 0.5 * x * x) / GJ; };
        double worst = 0.0;
        for (std::size_t i = 0; i < axis.size(); ++i) worst = std::max(worst, std::abs(axis.twist[i] - theta(axis.x[i])));
        TEST_CHECK(axis.source == DeformedAxisSource::SolverStations && worst < 1e-9 * theta(L),
                   "Test 331: θx de la vue = m(L·x − x²/2)/GJ en chaque point (non linéaire)");
        ResultsVisualManager vm(nullptr);
        vm.setModel(&mdl);
        vm.setResultsModel(r);
        const double s = vm.deformationScale();
        const auto frames = DeformedGeometry::sectionFrames(axis, 0.0, s, true);
        const auto base = BeamGeometry::sectionFrame(gp_Pnt(0, 0, 0), gp_Pnt(L, 0, 0), 0.0);
        bool angles = true;
        for (std::size_t i = 0; i < frames.size(); ++i)
            angles &= std::abs(signedAngle(base.x, frames[i].x, gp_Vec(1, 0, 0)) - s * theta(axis.x[i])) < 1e-9;
        TEST_CHECK(angles && relClose(vm.maxTwist(), theta(L), 1e-9), "Test 331: chaque section tournée de s·θx(x)");
        const TopoDS_Shape solid = DeformedGeometry::createMemberSolid(axis, sec, 0.0, s, true);
        const double xm = 1.5, phi = s * theta(xm);
        TEST_CHECK(std::abs(sectionExtentY(solid, xm) - (0.2 * std::abs(std::cos(phi)) + 0.4 * std::abs(std::sin(phi)))) < 1e-9,
                   "Test 331: coordonnées des sommets de la section à mi-longueur");
        TEST_CHECK(r->nodeDisplacement(tip).rx == axis.twist.back(), "Test 331: valeur affichée = résultat du calcul (non amplifiée)");
        const auto& sum = r->summary();
        TEST_CHECK(relClose(sum.maxTorsionMoment, m * L, 1e-9) && relClose(sum.maxTwist, theta(L), 1e-9) && sum.maxTorsionMomentElementId == bar
                       && sum.maxDisplacement < 1e-12 && sum.maxBendingMoment < 1e-9,
                   "Test 331: synthèse — Mx max = m·L, θx max = θ(L) (déplacement et flexion nuls en torsion pure)");
        // Symbole de la charge : vecteurs moment parallèles à la barre (torsion), sens de +x pour m > 0
        const auto glyph = TSA::Geometry::buildMemberLoadGlyph(mdl.loadManager().memberLoads().begin()->second, gp_Pnt(0, 0, 0),
                                                               gp_Pnt(L, 0, 0), 0.0);
        bool parallel = glyph.kind == TSA::Geometry::MemberLoadGlyphKind::DistributedMoment && glyph.arrows.empty() && glyph.momentVectors.size() >= 2;
        for (const auto& v : glyph.momentVectors) parallel &= gp_Vec(v.direction).Dot(gp_Vec(1, 0, 0)) > 1.0 - 1e-12;
        TEST_CHECK(parallel, "Test 331: symbole du couple réparti — vecteurs moment parallèles à la barre, aucune flèche de force");
        // Projet d'essai de l'interface (TSA_TEST_DUMP_DIR) : console en torsion (couple en bout), barre
        // inclinée sous couple réparti, cas FLEXION séparé ; réglages d'analyse : OpenSees 3D, cas TORSION.
        if (const QString dir = qEnvironmentVariable("TSA_TEST_DUMP_DIR"); !dir.isEmpty())
        {
            Model ui;
            const int n1 = ui.addNode(0, 0, 0), n2 = ui.addNode(3, 0, 0), n3 = ui.addNode(0, 3, 0), n4 = ui.addNode(2.5, 4.5, 1.5);
            ui.getNode(n1)->setSupport(SupportDefinition::fixed());
            ui.getNode(n3)->setSupport(SupportDefinition::fixed());
            ui.addBar(n1, n2, sec, mat, BarRole::Beam, 0.0, "Console torsion");
            const int b2 = ui.addBar(n3, n4, Section::circular(0.25), mat, BarRole::Beam, 0.0, "Barre inclinée");
            const int lcT = ui.loadManager().addLoadCase(LoadCase(0, "TORSION", LoadCaseCategory::Live, false, 1.0));
            const int lcF = ui.loadManager().addLoadCase(LoadCase(0, "FLEXION", LoadCaseCategory::Live, false, 1.0));
            ui.loadManager().addNodalLoad(NodalLoad(0, n2, lcT, 0, 0, 0, 10.0, 0, 0, LoadCoordSystem::Global, "T = 10 kNm"));
            ui.loadManager().addMemberLoad(couple(b2, lcT, 2.0, 2.0, LoadDirection::LocalX));
            ui.loadManager().addNodalLoad(NodalLoad(0, n2, lcF, 0, 0, -20.0, 0, 0, 0, LoadCoordSystem::Global, "F = 20 kN"));
            TSA::Analysis::AnalysisContext ctx;
            ctx.engineId = "opensees";
            ctx.dimension = TSA::Analysis::AnalysisDimension::Space3D;
            ctx.loadCaseIds = { lcT };
            ctx.common.includeSelfWeight = false;
            ui.setAnalysisSettingsJson(QJsonDocument(ctx.toJson()).toJson(QJsonDocument::Compact).toStdString());
            TSA::IO::TSAFileWriter writer;
            std::string err;
            const bool saved = writer.saveToFile((dir + "/torsion_ui.tsa").toStdString(), ui, nullptr, "Essai torsion", "TSA", &err);
            std::cout << "  projet d'essai de l'interface : " << (dir + "/torsion_ui.tsa").toStdString() << (saved ? "" : " — ÉCHEC " + err) << std::endl;
            // Essai de la boîte « Charge sur barre » : console (B1) et treillis (T1), aucune charge, cas « T ».
            Model dlg;
            const int d1 = dlg.addNode(0, 0, 0), d2 = dlg.addNode(3, 0, 0), d3 = dlg.addNode(3, 2, 0);
            dlg.getNode(d1)->setSupport(SupportDefinition::fixed());
            dlg.getNode(d3)->setSupport(SupportDefinition::fixed());
            dlg.addBar(d1, d2, sec, mat, BarRole::Beam, 0.0, "Console");
            dlg.addTrussMember(d2, d3, 0.05, "Tirant");
            const int lcD = dlg.loadManager().addLoadCase(LoadCase(0, "T", LoadCaseCategory::Live, false, 1.0));
            TSA::Analysis::AnalysisContext dctx = ctx;
            dctx.loadCaseIds = { lcD };
            dlg.setAnalysisSettingsJson(QJsonDocument(dctx.toJson()).toJson(QJsonDocument::Compact).toStdString());
            const bool savedD = writer.saveToFile((dir + "/torsion_dialog.tsa").toStdString(), dlg, nullptr, "Essai dialogue", "TSA", &err);
            std::cout << "  projet d'essai de la boîte de dialogue : " << (dir + "/torsion_dialog.tsa").toStdString() << (savedD ? "" : " — ÉCHEC " + err) << std::endl;
        }
        std::cout << "[PASS] Test 331: Vue d'une torsion répartie" << std::endl;
        ++passed;
    }

    // TEST 332 : constantes de torsion et de gauchissement des profils laminés, comparées à des fiches publiées
    {
        std::cout << "\n--- TEST 332: J et Iw des profils laminés (fiches British Steel) ---" << std::endl;
        double worstJ = 0.0, worstH = 0.0;
        std::string worstJName, worstHName;
        for (const auto& ref : kBritishSteel)
        {
            const double m = 1e-3; // mm → m
            const double J = Section::rolledIShapeTorsionConstant(ref.h * m, ref.b * m, ref.tw * m, ref.tf * m, ref.r * m) * 1e8; // cm⁴
            Section s;
            s.shape = SectionShape::IShape;
            s.height = ref.h * m; s.width = ref.b * m; s.tw = ref.tw * m; s.tf = ref.tf * m;
            const double H = s.iw() * 1e6; // dm⁶
            const double eJ = std::abs(J / ref.J - 1.0);
            // H imprimé à 3 chiffres significatifs et 4 décimales au plus (0,0004 dm⁶ pour l'IPE 100) : demi-unité du dernier chiffre tolérée.
            const double digit = std::max(1e-4, std::pow(10.0, std::floor(std::log10(ref.H)) - 2.0)); // 4 décimales au plus
            const double eH = std::max(0.0, std::abs(H - ref.H) - 0.5 * digit) / ref.H;
            if (eJ > worstJ) { worstJ = eJ; worstJName = ref.name; }
            if (eH > worstH) { worstH = eH; worstHName = ref.name; }
        }
        std::cout << "  " << std::size(kBritishSteel) << " profils : écart max J " << worstJ * 100 << " % (" << worstJName << "), Iw "
                  << worstH * 100 << " % (" << worstHName << ")" << std::endl;
        TEST_CHECK(worstJ < 5e-3, "Test 332: J (El Darwish & Johnston, congés) = fiches British Steel à 0,5 % près (" + worstJName + ")");
        TEST_CHECK(worstH < 1.5e-2, "Test 332: Iw = tf·b³(h − tf)²/24 = fiches British Steel à 1,5 % près (" + worstHName + ")");

        // Profils de TSA : rayon EN 10365. Identique à British Steel pour les IPE 100-300 et HE 100-160 : J identique.
        int compared = 0;
        for (const auto& ref : kBritishSteel)
        {
            const std::string n = ref.name;
            Section s;
            if (n.rfind("IPE ", 0) == 0) s = Section::ipe(std::stoi(n.substr(4)));
            else if (n.size() > 3 && n.rfind("HE ", 0) == 0) s = n.back() == 'A' ? Section::hea(std::stoi(n.substr(3))) : Section::heb(std::stoi(n.substr(3)));
            if (s.rootRadius() <= 0.0 || std::abs(s.rootRadius() * 1e3 - ref.r) > 1e-9) continue;
            ++compared;
            TEST_CHECK(relClose(s.it() * 1e8, ref.J, 5e-3), "Test 332: " + s.name + " — J de TSA = fiche (" + std::to_string(s.it() * 1e8) + " cm⁴)");
        }
        std::cout << "  " << compared << " profils de TSA de même rayon de congé que la fiche : J identique à 0,5 % près" << std::endl;
        TEST_CHECK(compared >= 15, "Test 332: au moins 15 profils de TSA comparés directement");

        // Rayon différent (British Steel lamine le HE 200 B avec r = 20 mm, EN 10365 : 18 mm) : écart attendu et documenté.
        const Section heb200 = Section::heb(200);
        TEST_CHECK(std::abs(heb200.rootRadius() - 0.018) < 1e-12 && heb200.it() * 1e8 < 62.8 && heb200.it() * 1e8 > 58.0,
                   "Test 332: HEB 200 (r = 18 mm EN 10365) — J entre parois minces et fiche à r = 20 mm");

        // Section paramétrique ou modifiée : jamais de congé supposé
        Section modified = Section::ipe(300);
        modified.tf = 0.012;
        Section renamed = Section::ipe(300);
        renamed.name = "Poutre soudée 300";
        TEST_CHECK(modified.rootRadius() == 0.0 && renamed.rootRadius() == 0.0 && Section::ipe(999).rootRadius() == 0.0,
                   "Test 332: dimensions modifiées, désignation libre ou numéro inconnu — formule des parois minces");
        TEST_CHECK(relClose(renamed.it(), (2.0 * 0.150 * std::pow(0.0107, 3) + (0.300 - 0.0107) * std::pow(0.0071, 3)) / 3.0, 1e-12),
                   "Test 332: profil reconstitué sans congé — J parois minces");
        TEST_CHECK(std::isnan(Section::upn(200).iw()) && Section::boxHollow(0.2, 0.2, 0.01).iw() == 0.0,
                   "Test 332: Iw non calculé (UPN) signalé par NaN, négligeable pour les sections fermées");

        // Persistance : désignation et dimensions enregistrées → même J après réouverture
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(4, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.addBar(a, b, Section::hea(260), Material::steelS235(), BarRole::Beam);
        const std::string path = (QDir::tempPath() + "/tsa_test_332.tsa").toStdString();
        TSA::IO::TSAFileWriter writer;
        std::string err;
        TEST_CHECK(writer.saveToFile(path, m, nullptr, "J", "TSA", &err), "Test 332: enregistrement");
        Model back;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, nullptr, "", nullptr, nullptr, nullptr, &err), "Test 332: relecture");
        const Section& reread = back.beams().begin()->second.section();
        TEST_CHECK(reread.rootRadius() == Section::hea(260).rootRadius() && reread.it() == Section::hea(260).it(),
                   "Test 332: J identique après réouverture (rayon déduit de la désignation, aucun champ ajouté au format)");
        std::filesystem::remove(path);
        std::cout << "[PASS] Test 332: J et Iw des profils laminés" << std::endl;
        ++passed;
    }

    // TEST 333 : modes de blocage des appuis articulés — choix explicite, historique conservé, risques signalés
    {
        std::cout << "\n--- TEST 333: Appuis articulés — fourche, historique, aucun blocage ---" << std::endl;
        using TSA::Analysis::SupportRestraintMode;
        const double L = 6.0, q = 10.0;
        auto yBeam = [&](Model& mdl) {
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(0, L, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::pinned());
            mdl.getNode(b)->setSupport(SupportDefinition::pinned());
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            mdl.loadManager().addMemberLoad(MemberLoad::uniform(bar, addCase(mdl), q));
            return bar;
        };
        // 1. Poutre selon Y sur deux articulations
        {
            Model mdl;
            const int bar = yBeam(mdl);
            AnalysisParameters pf = p;
            pf.supportRestraints = SupportRestraintMode::Fork;
            ResultsModel r;
            TEST_CHECK(solve(mdl, pf, r), "Test 333: mode fourche — poutre selon Y calculée");
            double w = 0.0;
            for (const auto& st : r.getElementResults(StructuralElementKind::Beam, bar)->intermediateStations)
                if (std::abs(st.position - L / 2) < 1e-9) w = st.uz;
            TEST_CHECK(relClose(w, -5.0 * q * std::pow(L, 4) / (384.0 * E * sec.iy()), 2e-5) && r.executionMetadata().isEquilibriumVerified,
                       "Test 333: mode fourche — 5qL⁴/384EI, équilibre vérifié");

            AnalysisParameters pl = p;
            pl.supportRestraints = SupportRestraintMode::Legacy;
            const auto report = TSA::Standards::ModelValidator::validateForAnalysis(mdl, pl);
            bool risk = false;
            for (const auto& i : report.issues())
                risk |= i.severity == TSA::Standards::ValidationSeverity::Warning && i.message.find("RX globale") != std::string::npos
                        && i.message.find("Risque") != std::string::npos && i.message.find("Correction") != std::string::npos;
            TEST_CHECK(risk, "Test 333: mode historique — avertissement (DDL RX, risque, correction) avant le calcul");
            const auto legacy = CalculationSnapshot::capture(mdl, nullptr, SupportRestraintMode::Legacy);
            bool rxOnly = !legacy.automaticRestraints().empty();
            for (const auto& ar : legacy.automaticRestraints()) rxOnly &= ar.dof == 3;
            TEST_CHECK(rxOnly, "Test 333: mode historique — RX globale bloquée (comportement des versions antérieures, tracé)");
            OpenSeesSolver solver;
            QString e;
            const bool ok = solver.solveSynchronous(mdl, pl, &e);
            std::cout << "  mode historique, poutre selon Y : " << (ok ? "calculé" : e.toStdString()) << std::endl;
            TEST_CHECK(!ok && (e.contains(QStringLiteral("Mécanisme de torsion")) || e.contains(QStringLiteral("mécanisme"))),
                       "Test 333: mode historique — torsion de la poutre selon Y libre : mécanisme signalé, aucun résultat");

            AnalysisParameters pn = p;
            pn.supportRestraints = SupportRestraintMode::None;
            TEST_CHECK(CalculationSnapshot::capture(mdl, nullptr, SupportRestraintMode::None).automaticRestraints().empty(),
                       "Test 333: aucun blocage — aucun DDL ajouté");
            OpenSeesSolver s2;
            TEST_CHECK(!s2.solveSynchronous(mdl, pn, &e) && e.contains(QStringLiteral("Mécanisme de torsion")),
                       "Test 333: aucun blocage — mécanisme de torsion détecté avant le calcul");
        }
        // 2. Portique plan XZ, pieds articulés, charges dans le plan : fourche et historique identiques, blocages sans effort
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(0, 0, 4), c = mdl.addNode(6, 0, 4), d = mdl.addNode(6, 0, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::pinned());
            mdl.getNode(d)->setSupport(SupportDefinition::pinned());
            mdl.addColumn(a, b, Section::heb(200), mat);
            const int beam = mdl.addBar(b, c, Section::ipe(300), mat, BarRole::Beam);
            mdl.addColumn(d, c, Section::heb(200), mat);
            const int lc = addCase(mdl);
            mdl.loadManager().addMemberLoad(MemberLoad::uniform(beam, lc, 12.0));
            mdl.loadManager().addNodalLoad(NodalLoad(0, b, lc, 8.0, 0, 0));
            ResultsModel rf, rl;
            AnalysisParameters pf = p, pl = p;
            pf.supportRestraints = SupportRestraintMode::Fork;
            pl.supportRestraints = SupportRestraintMode::Legacy;
            TEST_CHECK(solve(mdl, pf, rf) && solve(mdl, pl, rl), "Test 333: portique plan — calculs fourche et historique");
            const auto df = rf.nodeDisplacement(c), dl = rl.nodeDisplacement(c);
            TEST_CHECK(std::abs(df.ux - dl.ux) < 1e-12 && std::abs(df.uz - dl.uz) < 1e-12 && std::abs(df.ry - dl.ry) < 1e-12,
                       "Test 333: portique plan — résultats identiques (structure plane : blocages hors plan sans effet)");
            const auto& eq = rf.equilibrium();
            TEST_CHECK(rf.executionMetadata().isEquilibriumVerified && std::abs(eq.errorFx()) < 1e-9 && std::abs(eq.errorFz()) < 1e-9
                           && rf.executionMetadata().warnings.empty() && rl.executionMetadata().warnings.empty(),
                       "Test 333: portique plan — équilibre vérifié, aucun effort repris par un blocage automatique");
            TEST_CHECK(relClose(rf.getNodeReaction(a)->rx + rf.getNodeReaction(d)->rx, -8.0, 1e-9)
                           && relClose(rf.getNodeReaction(a)->rz + rf.getNodeReaction(d)->rz, 12.0 * 6.0, 1e-9),
                       "Test 333: portique plan — réactions ΣRx = −H, ΣRz = q·L");
        }
        // 3. Réglage enregistré : nouveau projet → fourche ; projet antérieur (clé absente) → historique
        {
            TSA::Analysis::AnalysisContext fresh;
            TEST_CHECK(fresh.common.supportRestraints == SupportRestraintMode::Fork, "Test 333: nouveau projet — appui à fourche");
            QJsonObject json = fresh.toJson();
            QJsonObject common = json.value("common").toObject();
            TEST_CHECK(common.value("pinnedSupports").toString() == "fork", "Test 333: réglage enregistré (clé pinnedSupports)");
            common.remove("pinnedSupports");
            json["common"] = common;
            TEST_CHECK(TSA::Analysis::AnalysisContext::fromJson(json).common.supportRestraints == SupportRestraintMode::Legacy,
                       "Test 333: projet antérieur (clé absente) — mode historique conservé, non modifié en silence");
            common["pinnedSupports"] = "none";
            json["common"] = common;
            TEST_CHECK(TSA::Analysis::AnalysisContext::fromJson(json).common.supportRestraints == SupportRestraintMode::None,
                       "Test 333: relecture du mode « aucun blocage »");
        }
        std::cout << "[PASS] Test 333: Appuis articulés — modes de blocage" << std::endl;
        ++passed;
    }

    // TEST 334 : charges stockables mais non transmises — signalées ou refusées avant tout résultat
    {
        std::cout << "\n--- TEST 334: Charges non transmises au solveur ---" << std::endl;
        auto issueText = [&](const Model& mdl, TSA::Standards::ValidationSeverity sev, const char* text) {
            return hasIssue(TSA::Standards::ModelValidator::validateForAnalysis(mdl, p), sev, text);
        };
        using Sev = TSA::Standards::ValidationSeverity;
        // 1. Charge surfacique d'un fichier TSA3D : conservée, jamais appliquée
        {
            const QByteArray json = R"({ "format": "TSA3D", "version": "1.0",
              "materials": [ { "id": "S235", "type": "steel", "E": 210e9, "nu": 0.3, "density": 7850, "fk": 235e6 } ],
              "sections": [ { "id": "R", "shape": "rectangular", "dimensions": { "b": 0.2, "h": 0.4 } } ],
              "nodes": [ { "id": "N1", "position": [0, 0, 0], "support": { "tx": "fixed", "ty": "fixed", "tz": "fixed", "rx": "fixed", "ry": "fixed", "rz": "fixed" } },
                         { "id": "N2", "position": [3, 0, 0] } ],
              "members": [ { "id": "B1", "type": "beam", "nodes": ["N1", "N2"], "section": "R", "material": "S235" } ],
              "loads": { "cases": [ { "id": "G", "name": "G", "category": "dead" } ],
                         "nodal": [ { "id": "F", "node": "N2", "case": "G", "force": [0, 0, -1] } ],
                         "surface": [ { "id": "P1", "case": "G", "pressure": -2.5 } ] } })";
            Model mdl;
            const auto res = TSA::IO::Tsa3d::importDocument(QJsonDocument::fromJson(json).object(), mdl);
            TEST_CHECK(res.ok, "Test 334: import TSA3D avec charge surfacique");
            TEST_CHECK(issueText(mdl, Sev::Warning, "surfacique"), "Test 334: charge surfacique conservée signalée NON appliquée avant le calcul");
        }
        // 2. Poids propre explicite, moment réparti sur treillis, moment réparti sans axe
        {
            Model mdl;
            const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(3, 0, 0), c = mdl.addNode(3, 2, 0);
            mdl.getNode(a)->setSupport(SupportDefinition::fixed());
            mdl.getNode(c)->setSupport(SupportDefinition::fixed());
            const int bar = mdl.addBar(a, b, sec, mat, BarRole::Beam);
            const int truss = mdl.addTrussMember(b, c, 0.05);
            const int lc = addCase(mdl);
            MemberLoad sw(0, bar, lc, LoadType::SelfWeight, 1.0, 1.0, LoadDirection::Gravity);
            mdl.loadManager().addMemberLoad(sw);
            TEST_CHECK(issueText(mdl, Sev::Warning, "poids propre"), "Test 334: poids propre explicite signalé (comportement de chaque moteur)");
            MemberLoad onTruss = couple(truss, lc, 1.0, 1.0, LoadDirection::LocalX);
            onTruss.setTargetType(MemberTargetType::Truss);
            mdl.loadManager().addMemberLoad(onTruss);
            TEST_CHECK(issueText(mdl, Sev::Warning, "barre articulée"), "Test 334: moment réparti sur treillis signalé non transmis");
            mdl.loadManager().addMemberLoad(couple(bar, lc, 1.0, 1.0, LoadDirection::Gravity));
            TEST_CHECK(issueText(mdl, Sev::Error, "pas d'axe défini"), "Test 334: moment réparti sans axe refusé (erreur)");
        }
        std::cout << "[PASS] Test 334: Charges non transmises signalées" << std::endl;
        ++passed;
    }
    return true;
}

/// Contrôle d'un projet enregistré par l'application après l'essai de la boîte de dialogue (TSA_UI_CHECK_FILE) :
/// charges relues, transmission au solveur, θ comparé à m·L²/(2GJ). Hors décompte des tests.
bool checkDialogProject(const QString& path)
{
    Model m;
    TSA::IO::TSAFileReader reader;
    std::string err;
    if (!reader.loadFromFile(path.toStdString(), m, nullptr, "", nullptr, nullptr, nullptr, &err))
    {
        std::cout << "UI-CHECK ÉCHEC lecture : " << err << std::endl;
        return false;
    }
    std::cout << "UI-CHECK charges sur barre relues : " << m.loadManager().memberLoads().size() << std::endl;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        std::cout << "UI-CHECK   ML" << id << " type=" << static_cast<int>(l.type()) << " (MemberMoment=" << static_cast<int>(LoadType::MemberMoment)
                  << ") cible=" << static_cast<int>(l.targetType()) << " #" << l.elementId() << " direction=" << static_cast<int>(l.direction())
                  << " (LocalX=" << static_cast<int>(LoadDirection::LocalX) << ") q1=" << l.q1() << " q2=" << l.q2() << std::endl;
    const auto report = TSA::Standards::ModelValidator::validateForAnalysis(m, AnalysisParameters {});
    for (const auto& i : report.issues())
        if (i.severity != TSA::Standards::ValidationSeverity::Info) std::cout << "UI-CHECK   validation : " << i.message << std::endl;
    AnalysisParameters p;
    p.includeSelfWeight = false;
    ResultsModel r;
    if (!solve(m, p, r)) { std::cout << "UI-CHECK ÉCHEC calcul" << std::endl; return false; }
    const auto& beam = m.beams().begin()->second;
    const double L = beam.length(m), GJ = shearModulus(beam.material()) * beam.section().it();
    double mBeam = 0.0;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        if (l.type() == LoadType::MemberMoment && l.targetType() == MemberTargetType::Beam && l.direction() == LoadDirection::LocalX) mBeam += l.q1();
    const double theta = r.nodeDisplacement(beam.endNodeId()).rx, ref = mBeam * L * L / (2.0 * GJ);
    std::cout << "UI-CHECK θ(L) = " << theta << " rad ; référence m·L²/(2GJ) = " << ref << " ; écart relatif "
              << (ref != 0.0 ? std::abs(theta - ref) / std::abs(ref) : std::abs(theta)) << std::endl;
    std::cout << "UI-CHECK équilibre vérifié : " << r.executionMetadata().isEquilibriumVerified << " ; ΣMx appliqué = "
              << r.equilibrium().appliedMx << " ; réaction = " << r.equilibrium().reactionMx << std::endl;
    return std::abs(theta - ref) <= 1e-9 * std::max(1e-12, std::abs(ref));
}
} // namespace

bool runSuite_Torsion(int& passed)
{
    if (const QString f = qEnvironmentVariable("TSA_UI_CHECK_FILE"); !f.isEmpty())
    {
        std::cout << (checkDialogProject(f) ? "UI-CHECK RÉSULTAT : OK" : "UI-CHECK RÉSULTAT : ÉCHEC") << std::endl;
        return true;
    }

    // TEST 321 : console en torsion pure — θ = T·L / (G·J), du solveur à la vue 3D
    {
        std::cout << "\n--- TEST 321: Console en torsion (référence T·L/GJ) ---" << std::endl;
        const double L = 3.0, T = 10.0; // m, kNm
        const Section sec = Section::rectangular(0.2, 0.4);
        Model m;
        int tip = 0;
        const int bar = cantilever(m, L, sec, &tip);
        const int lc = m.loadManager().addLoadCase(LoadCase(1, "TORSION", LoadCaseCategory::Live, false, 1.0));
        m.loadManager().addNodalLoad(NodalLoad(0, tip, lc, 0, 0, 0, T, 0, 0)); // moment autour de X = axe de la barre

        // Constante de Saint-Venant, et non moment polaire : rectangle a/b = 2 → J = 0.229·a·b³ (Timoshenko).
        const double J = sec.it(), Ip = sec.iy() + sec.iz();
        TEST_CHECK(relClose(J, 0.229 * 0.4 * std::pow(0.2, 3), 5e-3), "Test 321: J de Saint-Venant du rectangle (0.229·a·b³)");
        TEST_CHECK(J < 0.6 * Ip, "Test 321: J distinct du moment polaire Iy + Iz (" + std::to_string(J) + " / " + std::to_string(Ip) + ")");

        AnalysisParameters p;
        p.includeSelfWeight = false;
        auto r = std::make_shared<ResultsModel>();
        TEST_CHECK(solve(m, p, *r), "Test 321: calcul OpenSees");
        TEST_CHECK(r->isValid(), "Test 321: résultats valides");
        const double G = shearModulus(Material::steelS235());
        const double thetaRef = T * L / (G * J);
        const NodeDisplacement d = r->nodeDisplacement(tip);
        std::cout << "  θ calculé = " << d.rx << " rad ; référence T·L/(G·J) = " << thetaRef << " rad (écart relatif "
                  << std::abs(d.rx - thetaRef) / thetaRef << ")" << std::endl;
        TEST_CHECK(std::abs(d.rx) > 1e-6, "Test 321: rotation de torsion non nulle");
        TEST_CHECK(relClose(d.rx, thetaRef, 1e-9), "Test 321: θ = T·L / (G·J)");
        TEST_CHECK(std::abs(d.ux) + std::abs(d.uy) + std::abs(d.uz) < 1e-12 && std::abs(d.ry) + std::abs(d.rz) < 1e-12,
                   "Test 321: torsion pure — l'axe ne se déplace pas (la déformée de l'axe seule est invisible)");
        const auto* er = r->getElementResults(StructuralElementKind::Beam, bar);
        TEST_CHECK(er && relClose(std::abs(er->startForces.Mx), T, 1e-9) && relClose(std::abs(er->endForces.Mx), T, 1e-9),
                   "Test 321: moment de torsion Mx = T le long de la barre");

        // Axe déformé : θx aux points de la barre, linéaire (Saint-Venant), extrémités = rotations nodales
        const DeformedAxis axis = axisOf(m, *r, StructuralElementKind::Beam, bar);
        TEST_CHECK(axis.valid && axis.hasTwist() && axis.twist.front() == 0.0 && relClose(axis.twist.back(), d.rx, 1e-15),
                   "Test 321: θx de l'axe = rotations nodales du solveur");
        bool linear = true;
        for (std::size_t i = 0; i < axis.size(); ++i) linear &= std::abs(axis.twist[i] - thetaRef * axis.x[i] / L) < 1e-8 * thetaRef;
        TEST_CHECK(linear && relClose(axis.maxAbsTwist(), thetaRef, 1e-9), "Test 321: θx(x) = T·x / (G·J) le long de la barre");

        // Vue : les sections tournent de s·θx autour de l'axe ; sans torsion (ancien rendu), rien ne tourne
        const double s = 25.0;
        const auto frames = DeformedGeometry::sectionFrames(axis, 0.0, s, true);
        const auto base = BeamGeometry::sectionFrame(gp_Pnt(0, 0, 0), gp_Pnt(L, 0, 0), 0.0);
        TEST_CHECK(frames.size() == axis.size() && std::abs(signedAngle(base.x, frames.front().x, gp_Vec(1, 0, 0))) < 1e-12
                       && relClose(signedAngle(base.x, frames.back().x, gp_Vec(1, 0, 0)), s * thetaRef, 1e-9),
                   "Test 321: section d'extrémité tournée de s·θ (" + std::to_string(s * thetaRef) + " rad)");
        const auto still = DeformedGeometry::sectionFrames(axis, 0.0, s, false);
        TEST_CHECK(std::abs(signedAngle(base.x, still.back().x, gp_Vec(1, 0, 0))) < 1e-12,
                   "Test 321: sans rotation des sections, la torsion pure reste invisible (cause du défaut)");
        const TopoDS_Shape solid = DeformedGeometry::createMemberSolid(axis, sec, 0.0, s, true);
        const double phi = s * thetaRef;
        TEST_CHECK(std::abs(sectionExtentY(solid, 0.0) - 0.2) < 1e-9
                       && std::abs(sectionExtentY(solid, L) - (0.2 * std::abs(std::cos(phi)) + 0.4 * std::abs(std::sin(phi)))) < 1e-9,
                   "Test 321: barre affichée — section encastrée intacte, section libre tournée");
        TEST_CHECK(std::abs(sectionExtentY(DeformedGeometry::createMemberSolid(axis, sec, 0.0, s, false), L) - 0.2) < 1e-9,
                   "Test 321: option désactivée — sections non tournées");
        const TopoDS_Shape markers = DeformedGeometry::createSectionMarkers(axis, sec, 0.0, s, true, 5);
        TEST_CHECK(wireCount(markers) == 6, "Test 321: repères de torsion (5 contours de section + génératrice)");

        // Échelle automatique : tient compte de la rotation des sections (auparavant ×1 : déplacements nuls)
        ResultsVisualManager vm(nullptr);
        vm.setModel(&m);
        vm.setResultsModel(r);
        const double scale = vm.deformationScale();
        TEST_CHECK(scale > 1.0 && std::isfinite(scale), "Test 321: échelle automatique > 1 en torsion pure (×" + std::to_string(scale) + ")");
        TEST_CHECK(scale * thetaRef <= 0.5236 + 1e-9 && scale * thetaRef * DeformedGeometry::sectionRadius(sec) <= 0.05 * 5.0 + 1e-9,
                   "Test 321: rotation affichée ≤ 30° et fibre extrême ≤ 5 % de l'envergure");
        TEST_CHECK(relClose(vm.maxTwist(), thetaRef, 1e-9) && vm.maxTwistElement() == "Beam " + std::to_string(bar),
                   "Test 321: torsion maximale identifiée");
        const QString legend = vm.legendSummaryText();
        TEST_CHECK(legend.contains(QStringLiteral("TORSION")) && legend.contains(QStringLiteral("sections tournées")),
                   "Test 321: légende — θx max et facteur appliqué aux sections");
        vm.setDeformationScalePreset(ScalePreset::X1);
        TEST_CHECK(vm.deformationScale() == 1.0, "Test 321: déformée non amplifiée (×1)");
        vm.setDeformationScalePreset(ScalePreset::Custom, 40.0);
        TEST_CHECK(vm.deformationScale() == 40.0, "Test 321: échelle manuelle");
        vm.setSectionRotationVisible(false);
        TEST_CHECK(vm.legendSummaryText().contains(QStringLiteral("rotation des sections masquée")), "Test 321: option signalée dans la légende");
        vm.setSectionRotationVisible(true);
        vm.setDeformationScalePreset(ScalePreset::Auto);
        TEST_CHECK(vm.deformationScale() == scale, "Test 321: retour à l'échelle automatique");
        TEST_CHECK(r->nodeDisplacement(tip).rx == d.rx && r->nodeDisplacement(tip).uz == d.uz,
                   "Test 321: résultats numériques inchangés par l'affichage");

        dumpViews(m, r, QStringLiteral("torsion_rect"), true);
        dumpViews(m, r, QStringLiteral("torsion_rect_sans_rotation"), false);
        std::cout << "[PASS] Test 321: Console en torsion — θ = T·L/(G·J), sections tournées" << std::endl;
        ++passed;
    }

    // TEST 322 : constantes de torsion (Saint-Venant ≠ polaire) et transmission au solveur
    {
        std::cout << "\n--- TEST 322: Constantes de torsion J ---" << std::endl;
        const Section circ = Section::circular(0.3);
        TEST_CHECK(relClose(circ.it(), M_PI * std::pow(0.3, 4) / 32.0, 1e-12) && relClose(circ.it(), circ.iy() + circ.iz(), 1e-12),
                   "Test 322: section pleine ronde — J = πd⁴/32 = moment polaire");
        const Section pipe = Section::pipe(0.2, 0.01);
        TEST_CHECK(relClose(pipe.it(), M_PI * (std::pow(0.2, 4) - std::pow(0.18, 4)) / 32.0, 1e-12), "Test 322: tube rond — J = π(D⁴ − d⁴)/32");
        // Caisson 200×200×10 : Bredt 4·Am²·t/p + p·t³/3 (ligne moyenne 190 mm), et non Iy + Iz
        const Section box = Section::boxHollow(0.2, 0.2, 0.01);
        const double am = 0.19 * 0.19, perim = 4.0 * 0.19, t = 0.01;
        const double jBox = 4.0 * am * am * t / perim + perim * t * t * t / 3.0;
        TEST_CHECK(relClose(box.it(), jBox, 1e-12), "Test 322: caisson — J de Bredt (" + std::to_string(box.it()) + " m⁴)");
        TEST_CHECK(box.it() < 0.8 * (box.iy() + box.iz()), "Test 322: caisson — J < moment polaire (anciennement confondus)");
        // Profil laminé IPE 300 (rayon de congé EN 10365 = 15 mm, identique à British Steel) : formule avec congés.
        const Section ipe = Section::ipe(300);
        std::cout << "  IPE 300 : J = " << ipe.it() * 1e8 << " cm⁴ (fiche British Steel : 19.9 cm⁴)" << std::endl;
        TEST_CHECK(std::abs(ipe.rootRadius() - 0.015) < 1e-12 && relClose(ipe.it(), 19.9e-8, 5e-3),
                   "Test 322: IPE 300 — J avec congés = 19.9 cm⁴ (British Steel)");

        // Le J du caisson est bien celui du calcul : console en torsion
        Model m;
        int tip = 0;
        cantilever(m, 2.0, box, &tip);
        const int lc = m.loadManager().addLoadCase(LoadCase(1, "T", LoadCaseCategory::Live, false, 1.0));
        m.loadManager().addNodalLoad(NodalLoad(0, tip, lc, 0, 0, 0, 5.0, 0, 0));
        AnalysisParameters p;
        p.includeSelfWeight = false;
        ResultsModel r;
        TEST_CHECK(solve(m, p, r), "Test 322: calcul du caisson");
        const double ref = 5.0 * 2.0 / (shearModulus(Material::steelS235()) * jBox);
        TEST_CHECK(relClose(r.nodeDisplacement(tip).rx, ref, 1e-9), "Test 322: θ du caisson = T·L / (G·J_Bredt)");

        // Charge nodale déclarée « locale » : un nœud n'a pas de repère local → signalée, axes globaux
        m.loadManager().addNodalLoad(NodalLoad(0, tip, lc, 0, 0, 0, 1.0, 0, 0, LoadCoordSystem::Local, "T locale"));
        bool warned = false;
        const auto report = TSA::Standards::ModelValidator::validateForAnalysis(m, p);
        for (const auto& issue : report.issues())
            warned |= issue.severity == TSA::Standards::ValidationSeverity::Warning && issue.message.find("T locale") != std::string::npos
                      && issue.message.find("axes globaux") != std::string::npos;
        TEST_CHECK(warned, "Test 322: charge nodale en repère local signalée (appliquée selon les axes globaux)");
        std::cout << "[PASS] Test 322: Constantes de torsion (Saint-Venant, Bredt)" << std::endl;
        ++passed;
    }

    // TEST 323 : non-régression — force transversale, moment de flexion, portique 3D flexion + torsion
    {
        std::cout << "\n--- TEST 323: Flexion, torsion combinée (portique en L) ---" << std::endl;
        const Section sec = Section::rectangular(0.2, 0.4);
        const Material mat = Material::steelS235();
        const double E = mat.mechanical.youngModulus * 1e-3, G = shearModulus(mat);
        AnalysisParameters p;
        p.includeSelfWeight = false;

        // 1. Force transversale en bout : w = P·L³ / 3EI, aucune torsion, aucun repère de torsion
        {
            const double L = 3.0, P = 20.0;
            Model m;
            int tip = 0;
            const int bar = cantilever(m, L, sec, &tip);
            const int lc = m.loadManager().addLoadCase(LoadCase(1, "P", LoadCaseCategory::Live, false, 1.0));
            m.loadManager().addNodalLoad(NodalLoad(0, tip, lc, 0, 0, -P));
            auto r = std::make_shared<ResultsModel>();
            TEST_CHECK(solve(m, p, *r), "Test 323: calcul console (force)");
            TEST_CHECK(relClose(r->nodeDisplacement(tip).uz, -P * L * L * L / (3.0 * E * sec.iy()), 1e-9), "Test 323: w = P·L³/3EI");
            const DeformedAxis axis = axisOf(m, *r, StructuralElementKind::Beam, bar);
            TEST_CHECK(axis.hasTwist() && axis.maxAbsTwist() < 1e-15, "Test 323: force transversale — aucune torsion");
            ResultsVisualManager vm(nullptr);
            vm.setModel(&m);
            vm.setResultsModel(r);
            TEST_CHECK(vm.maxTwist() < 1e-15 && !vm.legendSummaryText().contains(QStringLiteral("TORSION")),
                       "Test 323: aucune torsion annoncée en flexion simple");
            TEST_CHECK(relClose(vm.deformationScale(), 0.05 * 5.0 / std::abs(r->nodeDisplacement(tip).uz), 1e-9),
                       "Test 323: échelle automatique inchangée en flexion (5 % de l'envergure)");
        }
        // 2. Moment de flexion en bout (axe global Y) : θy = M·L/EI, w = −M·L²/2EI, aucune torsion
        {
            const double L = 3.0, M = 15.0;
            Model m;
            int tip = 0;
            const int bar = cantilever(m, L, sec, &tip);
            const int lc = m.loadManager().addLoadCase(LoadCase(1, "M", LoadCaseCategory::Live, false, 1.0));
            m.loadManager().addNodalLoad(NodalLoad(0, tip, lc, 0, 0, 0, 0, M, 0));
            ResultsModel r;
            TEST_CHECK(solve(m, p, r), "Test 323: calcul console (moment)");
            const auto d = r.nodeDisplacement(tip);
            TEST_CHECK(relClose(d.ry, M * L / (E * sec.iy()), 1e-9) && relClose(d.uz, -M * L * L / (2.0 * E * sec.iy()), 1e-9),
                       "Test 323: θ = M·L/EI et w = −M·L²/2EI");
            TEST_CHECK(axisOf(m, r, StructuralElementKind::Beam, bar).maxAbsTwist() < 1e-15, "Test 323: moment de flexion — aucune torsion");
        }
        // 3. Portique en L : barre 1 selon X (encastrée), barre 2 selon Y, P vers le bas au bout de la barre 2.
        //    Barre 1 : torsion T = P·a → θ1 = −P·a·L1/(G·J) ; flèche P·L1³/3EI. Barre 2 : torsion de corps
        //    rigide uniforme = rotation de flexion de la barre 1 (θy au nœud commun).
        {
            const double L1 = 4.0, a = 2.0, P = 10.0;
            Model m;
            const int n0 = m.addNode(0, 0, 0), n1 = m.addNode(L1, 0, 0), n2 = m.addNode(L1, a, 0);
            m.getNode(n0)->setSupport(SupportDefinition::fixed());
            const int b1 = m.addBar(n0, n1, sec, mat, BarRole::Beam);
            const int b2 = m.addBar(n1, n2, sec, mat, BarRole::Beam);
            const int lc = m.loadManager().addLoadCase(LoadCase(1, "P", LoadCaseCategory::Live, false, 1.0));
            m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 0, 0, -P));
            auto r = std::make_shared<ResultsModel>();
            TEST_CHECK(solve(m, p, *r), "Test 323: calcul du portique en L");
            const auto d1 = r->nodeDisplacement(n1);
            const double theta1 = -P * a * L1 / (G * sec.it());
            std::cout << "  barre 1 : θx = " << d1.rx << " rad (référence −P·a·L1/(G·J) = " << theta1 << ")" << std::endl;
            TEST_CHECK(relClose(d1.rx, theta1, 1e-9), "Test 323: torsion de la barre 1 = −P·a·L1/(G·J)");
            TEST_CHECK(relClose(d1.uz, -P * L1 * L1 * L1 / (3.0 * E * sec.iy()), 1e-9), "Test 323: flèche de la barre 1 = P·L1³/3EI");
            const DeformedAxis ax1 = axisOf(m, *r, StructuralElementKind::Beam, b1);
            const DeformedAxis ax2 = axisOf(m, *r, StructuralElementKind::Beam, b2);
            TEST_CHECK(relClose(ax1.twist.back(), theta1, 1e-9) && std::abs(ax1.twist.front()) < 1e-15, "Test 323: θx de la barre 1 dans la vue");
            // Barre 2 (axe local x = +Y) : torsion = rotation globale autour de Y au nœud commun, constante
            TEST_CHECK(relClose(ax2.twist.front(), d1.ry, 1e-12) && relClose(ax2.twist.back(), d1.ry, 1e-6),
                       "Test 323: barre 2 — torsion de corps rigide = θy de la barre 1 (repères cohérents)");
            ResultsVisualManager vm(nullptr);
            vm.setModel(&m);
            vm.setResultsModel(r);
            TEST_CHECK(vm.maxTwist() >= std::abs(theta1) && vm.deformationScale() * vm.maxTwist() <= 0.5236 + 1e-9,
                       "Test 323: échelle automatique — flexion et rotation des sections");
            dumpViews(m, r, QStringLiteral("torsion_portique"), true);
        }
        std::cout << "[PASS] Test 323: Non-régression flexion / torsion combinée" << std::endl;
        ++passed;
    }

    // TEST 324 : modèle plan (Custom2D) — la torsion longitudinale n'existe pas : signalée, jamais inventée
    {
        std::cout << "\n--- TEST 324: Modèle 2D et torsion hors plan ---" << std::endl;
        Model model;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0, 4 });
        def.setYPositions({ 0 });
        const std::string gid = gm.addGrid(def)->id();
        const int a = model.addNode(0, 0, 0), b = model.addNode(4, 0, 0);
        model.getNode(a)->setSupport(SupportDefinition::fixed());
        const int bar = model.addBar(a, b, Section::rectangular(0.2, 0.4), Material::steelS235(), BarRole::Beam);
        const int lc = model.loadManager().addLoadCase(LoadCase(0, "T", LoadCaseCategory::Live));
        model.loadManager().addNodalLoad(NodalLoad(0, b, lc, 0, 0, 0, 10.0, 0, 0)); // torsion autour de l'axe X de la barre

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
        const auto prepared = mgr.prepare(model, &gm, c);
        bool signalled = false;
        for (const auto& w : prepared.validation.texts(ValidationSeverity::Warning))
            signalled |= w.find("hors du plan") != std::string::npos;
        TEST_CHECK(signalled, "Test 324: moment de torsion hors du plan signalé par le moteur 2D");
        const auto run = mgr.run(c, prepared);
        TEST_CHECK(run.success, "Test 324: calcul plan exécuté");
        const auto r = std::make_shared<ResultsModel>(run.results);
        const auto d = r->nodeDisplacement(b);
        TEST_CHECK(d.rx == 0.0 && std::abs(d.ux) + std::abs(d.uy) + std::abs(d.uz) < 1e-15, "Test 324: aucune rotation de torsion calculée en 2D");
        TEST_CHECK(axisOf(model, *r, StructuralElementKind::Beam, bar).maxAbsTwist() == 0.0, "Test 324: aucune torsion dans la vue");
        ResultsVisualManager vm(nullptr);
        vm.setModel(&model);
        vm.setResultsModel(r);
        TEST_CHECK(vm.deformationScale() == 1.0 && vm.maxTwist() == 0.0 && !vm.legendSummaryText().contains(QStringLiteral("TORSION")),
                   "Test 324: résultats nuls — aucune déformée inventée (échelle ×1)");
        std::cout << "[PASS] Test 324: Modèle 2D — torsion hors plan signalée" << std::endl;
        ++passed;
    }

    // TEST 325 : cas de charge multiples, réinitialisation de la déformée, changement de facteur
    {
        std::cout << "\n--- TEST 325: Cas de charge multiples et réinitialisation ---" << std::endl;
        const double L = 3.0, T = 8.0, P = 12.0;
        const Section sec = Section::circular(0.25);
        Model m;
        int tip = 0;
        const int bar = cantilever(m, L, sec, &tip);
        const int lcT = m.loadManager().addLoadCase(LoadCase(1, "TORSION", LoadCaseCategory::Live, false, 1.0));
        const int lcP = m.loadManager().addLoadCase(LoadCase(2, "FLEXION", LoadCaseCategory::Live, false, 1.0));
        m.loadManager().addNodalLoad(NodalLoad(0, tip, lcT, 0, 0, 0, T, 0, 0));
        m.loadManager().addNodalLoad(NodalLoad(0, tip, lcP, 0, 0, -P));
        AnalysisParameters p;
        p.includeSelfWeight = false;
        p.targetLoadCaseId = lcT;
        auto rT = std::make_shared<ResultsModel>();
        TEST_CHECK(solve(m, p, *rT), "Test 325: calcul du cas TORSION");
        p.targetLoadCaseId = lcP;
        auto rP = std::make_shared<ResultsModel>();
        TEST_CHECK(solve(m, p, *rP), "Test 325: calcul du cas FLEXION");
        const double thetaRef = T * L / (shearModulus(Material::steelS235()) * sec.it());
        TEST_CHECK(relClose(rT->nodeDisplacement(tip).rx, thetaRef, 1e-9) && std::abs(rP->nodeDisplacement(tip).rx) < 1e-15,
                   "Test 325: torsion dans le seul cas TORSION");

        ResultsVisualManager vm(nullptr);
        vm.setModel(&m);
        vm.setResultsModel(rT);
        const double scaleT = vm.deformationScale();
        TEST_CHECK(relClose(vm.maxTwist(), thetaRef, 1e-9) && scaleT > 1.0, "Test 325: cas TORSION affiché (sections tournées)");
        // Section ronde : contour invariant par rotation → seuls les repères rendent la torsion visible
        const DeformedAxis axis = axisOf(m, *rT, StructuralElementKind::Beam, bar);
        const TopoDS_Shape markers = DeformedGeometry::createSectionMarkers(axis, sec, 0.0, scaleT, true, 4);
        TEST_CHECK(wireCount(markers) == 5, "Test 325: section ronde — contours et génératrice tracés");
        const auto frames = DeformedGeometry::sectionFrames(axis, 0.0, scaleT, true);
        const auto base = BeamGeometry::sectionFrame(gp_Pnt(0, 0, 0), gp_Pnt(L, 0, 0), 0.0);
        TEST_CHECK(relClose(signedAngle(base.y, frames.back().y, gp_Vec(1, 0, 0)), scaleT * thetaRef, 1e-9),
                   "Test 325: génératrice tournée de s·θ en bout");

        vm.setResultsModel(rP); // changement de cas : la déformée est reconstruite
        TEST_CHECK(vm.maxTwist() == 0.0 && !vm.legendSummaryText().contains(QStringLiteral("TORSION")),
                   "Test 325: cas FLEXION — torsion réinitialisée");
        TEST_CHECK(relClose(vm.deformationScale(), 0.05 * 5.0 / std::abs(rP->nodeDisplacement(tip).uz), 1e-9),
                   "Test 325: échelle automatique recalculée pour le cas affiché");
        vm.setDeformationScalePreset(ScalePreset::X100);
        vm.setResultsModel(rT);
        TEST_CHECK(vm.deformationScale() == 100.0 && relClose(vm.maxTwist(), thetaRef, 1e-9),
                   "Test 325: facteur choisi conservé d'un cas à l'autre, torsion rétablie");
        vm.setResultsModel(nullptr);
        TEST_CHECK(!vm.hasResults() && vm.legendSummaryText().isEmpty(), "Test 325: déformée effacée sans résultats");
        dumpViews(m, rT, QStringLiteral("torsion_ronde"), true);
        std::cout << "[PASS] Test 325: Cas multiples, réinitialisation, facteur" << std::endl;
        ++passed;
    }
    return runTorsionMechanics(passed);
}
