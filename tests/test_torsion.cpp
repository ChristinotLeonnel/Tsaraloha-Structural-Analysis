// Suite « torsion » (tests 321-325) : moment de torsion appliqué à une barre, du calcul OpenSees jusqu'à la
// déformée 3D. Chaque grandeur est comparée à la RDM (θ = T·L / (G·J), flèches de console) ; la vue est
// contrôlée sur la géométrie produite (rotation des sections, repères, facteur d'échelle).
// Contrôle visuel facultatif : TSA_TEST_DUMP_DIR=<dossier> enregistre des captures du viewport réel.

#include "test_common.h"

#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Analysis/OpenSeesSolver.h"
#include "Geometry/BeamGeometry.h"
#include "Geometry/DeformedGeometry.h"
#include "Grid/GridManager.h"
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
} // namespace

bool runSuite_Torsion(int& passed)
{
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
    return true;
}
