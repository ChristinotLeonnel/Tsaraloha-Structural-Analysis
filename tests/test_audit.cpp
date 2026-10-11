// Suite « audit » : corrections issues de l'audit scientifique des calculs (docs/AUDIT_CALCULS.md).
// Chaque défaut corrigé a ici son test de non-régression, comparé à une référence indépendante :
//   365  propriétés des sections : intégration numérique indépendante de la géométrie (T, cornière, UPN, I) et
//        valeurs publiées des catalogues (EN 10365 / ArcelorMittal) pour les profils laminés ;
//   366  EN 1992-1-1 (flexion simple, aciers comprimés, bétons à haute résistance) et EN 1993-1-1 (Tableau 6.2,
//        Tableau 5.2, χ) comparés à des calculs manuels indépendants écrits dans le test ;
//   369  invariants mathématiques de la chaîne éléments finis (portique 3D incliné, OpenSees, extraction avancée) :
//        matrice locale = matrice analytique d'Euler-Bernoulli 12×12, R orthogonale, K_e global symétrique et sans
//        rigidité pour les 6 mouvements de corps rigide, K global = assemblage des K_e, symétrique et défini positif,
//        résidu ‖K·U − F‖/‖F‖, équilibre des réactions ;
//   370  rendu graphique : fournisseur, carte, version OpenGL et accélération matérielle effectivement obtenus
//        (diagnostic journalisé), mode de compatibilité pour les cartes anciennes.

#include "test_common.h"

#include "Standards/Design/ConcreteDesignEC2.h"
#include "Standards/Design/SteelDesignEC3.h"
#include "Analysis/Engine/AnalysisEngineRegistry.h"
#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "Model/Load/LoadManager.h"

#include <QJsonObject>
#include <QApplication>
#include <QThread>
#include "Viewer/OccView.h"

#include <functional>

namespace
{
/// Intégration numérique d'une section décrite par une fonction d'appartenance sur la boîte [0, b] × [0, h]
/// (grille n × n, centres de cellules) : aire, centre, inerties centrales, produit d'inertie. Indépendante des
/// formules fermées de Section (seule la géométrie est commune).
struct NumericSection
{
    double A = 0, yc = 0, zc = 0, Iy = 0, Iz = 0, Iyz = 0;
};
NumericSection integrate(double b, double h, const std::function<bool(double, double)>& inside, double cell = 1e-4)
{
    // Cellules de 0,1 mm : les parois (dimensions au dixième de millimètre) tombent sur des bords de cellules
    NumericSection r;
    const int ny = int(std::lround(b / cell)), nz = int(std::lround(h / cell));
    const double dy = b / ny, dz = h / nz, dA = dy * dz;
    double Sy = 0, Sz = 0, Syy = 0, Szz = 0, Syz = 0;
    for (int i = 0; i < ny; ++i)
        for (int j = 0; j < nz; ++j)
        {
            const double y = (i + 0.5) * dy, z = (j + 0.5) * dz;
            if (!inside(y, z)) continue;
            r.A += dA, Sy += y * dA, Sz += z * dA, Syy += y * y * dA, Szz += z * z * dA, Syz += y * z * dA;
        }
    r.yc = Sy / r.A, r.zc = Sz / r.A;
    // Inerties propres des cellules (dy³dz/12) ajoutées : exactes pour des parois alignées sur la grille
    r.Iy = Szz - r.A * r.zc * r.zc + r.A * dz * dz / 12.0;
    r.Iz = Syy - r.A * r.yc * r.yc + r.A * dy * dy / 12.0;
    r.Iyz = Syz - r.A * r.yc * r.zc;
    return r;
}

bool rel(double a, double b, double tol) { return std::abs(a - b) <= tol * std::max(std::abs(b), 1e-300); }
} // namespace

bool runSuite_Audit(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 365 : propriétés des sections — intégration indépendante et catalogues publiés
    // -------------------------------------------------------------------------
    {
        // a) Formes dissymétriques : formules fermées = intégration numérique de la géométrie (grille alignée sur les
        //    parois : écart d'intégration < 1e-4)
        struct Case
        {
            const char* name;
            Section s;
            std::function<bool(double, double)> inside;
        };
        const double t = 0.010;
        std::vector<Case> cases;
        {
            Section l = Section::angle(0.100, 0.100, t);
            cases.push_back({ "L 100x100x10", l, [=](double y, double z) { return y <= t || z <= t; } });
            Section l2 = Section::angle(0.150, 0.100, 0.012);
            cases.push_back({ "L 150x100x12", l2, [](double y, double z) { return y <= 0.012 || z <= 0.012; } });
            Section tt = Section::tSection(0.300, 0.200, 0.012, 0.020);
            cases.push_back({ "T 200x300", tt, [](double y, double z) { return z >= 0.280 || std::abs(y - 0.100) <= 0.006; } });
            Section u = Section::upn(200);
            cases.push_back({ "UPN 200", u, [](double y, double z) { return y <= 0.0085 || z <= 0.0115 || z >= 0.2 - 0.0115; } });
            Section w = Section::rectangular(0.2, 0.4);
            w.shape = TSA::Model::SectionShape::IShape;
            w.name = "PRS";   // profil reconstitué : pas de congé
            w.width = 0.200, w.height = 0.400, w.tw = 0.010, w.tf = 0.020;
            cases.push_back({ "PRS 400x200", w, [](double y, double z) { return z <= 0.020 || z >= 0.380 || std::abs(y - 0.100) <= 0.005; } });
        }
        for (const auto& c : cases)
        {
            const double b = c.s.width, h = c.s.height;
            const auto num = integrate(b, h, c.inside);
            const double vy = std::max(num.zc, h - num.zc), vz = std::max(num.yc, b - num.yc);
            std::cout << "  " << c.name << " : A " << c.s.area() * 1e4 << " / " << num.A * 1e4 << " cm², Iy " << c.s.iy() * 1e8 << " / " << num.Iy * 1e8
                      << " cm⁴, Iz " << c.s.iz() * 1e8 << " / " << num.Iz * 1e8 << " cm⁴, Wy " << c.s.wy() * 1e6 << " / " << num.Iy / vy * 1e6 << " cm³" << std::endl;
            TEST_CHECK(rel(c.s.area(), num.A, 1e-4) && rel(c.s.iy(), num.Iy, 1e-4) && rel(c.s.iz(), num.Iz, 1e-4), "Test 365: " << c.name << " — A, Iy, Iz");
            TEST_CHECK(rel(c.s.wy(), num.Iy / vy, 1e-4) && rel(c.s.wz(), num.Iz / vz, 1e-4), "Test 365: " << c.name << " — Wel aux fibres extrêmes");
            TEST_CHECK(std::abs(c.s.centroidFromBottom() - num.zc) < 1e-6 && std::abs(c.s.centroidFromLeft() - num.yc) < 1e-6, "Test 365: " << c.name << " — centre de gravité");
            TEST_CHECK(std::abs(c.s.iyz() - num.Iyz) <= 1e-4 * std::max(num.Iy, num.Iz), "Test 365: " << c.name << " — produit d'inertie " << c.s.iyz() << " / " << num.Iyz);
        }

        // b) Profils laminés : catalogues EN 10365 (ArcelorMittal, Sections and merchant bars) ; congés inclus
        struct Pub
        {
            Section s;
            double A, Iy, Iz, Wy, Wz;   // cm², cm⁴, cm³
        };
        const Pub pubs[] = {
            { Section::ipe(200), 28.48, 1943, 142.4, 194.3, 28.47 },
            { Section::ipe(300), 53.81, 8356, 603.8, 557.1, 80.50 },
            { Section::hea(200), 53.83, 3692, 1336, 388.6, 133.6 },
            { Section::heb(200), 78.08, 5696, 2003, 569.6, 200.3 },
        };
        for (const auto& p : pubs)
        {
            const double A = p.s.area() * 1e4, Iy = p.s.iy() * 1e8, Iz = p.s.iz() * 1e8, Wy = p.s.wy() * 1e6, Wz = p.s.wz() * 1e6;
            std::cout << "  " << p.s.name << " : A " << A << " (" << p.A << "), Iy " << Iy << " (" << p.Iy << "), Iz " << Iz << " (" << p.Iz << "), Wy " << Wy << " ("
                      << p.Wy << "), Wz " << Wz << " (" << p.Wz << ")" << std::endl;
            TEST_CHECK(rel(A, p.A, 0.005) && rel(Iy, p.Iy, 0.005) && rel(Iz, p.Iz, 0.01) && rel(Wy, p.Wy, 0.005) && rel(Wz, p.Wz, 0.01),
                       "Test 365: " << p.s.name << " conforme au catalogue (≤ 0,5 % sur A, Iy, Wy ; ≤ 1 % sur Iz, Wz)");
        }
        // Profils à ailes inclinées et cornières : modèle à parois d'épaisseur constante, sans congé — écarts connus,
        // documentés (docs/AUDIT_CALCULS.md), bornés ici pour détecter toute dérive.
        const Section u = Section::upn(200);
        TEST_CHECK(rel(u.area() * 1e4, 32.2, 0.01) && rel(u.iy() * 1e8, 1910, 0.02) && rel(u.iz() * 1e8, 148, 0.20) && u.iz() * 1e8 > 148,
                   "Test 365: UPN 200 — A, Iy à 2 % ; Iz à +15 % (ailes inclinées non modélisées), jamais l'ancien Iz ≈ 82 cm⁴");
        const Section l = Section::angle(0.100, 0.100, 0.010);
        TEST_CHECK(rel(l.area() * 1e4, 19.2, 0.015) && rel(l.iy() * 1e8, 177, 0.03) && rel(l.wy() * 1e6, 24.6, 0.03),
                   "Test 365: L 100×100×10 — A, I, Wel à 3 % du catalogue (I = " << l.iy() * 1e8 << " cm⁴ ; ancienne formule 84 cm⁴)");
        std::cout << "[PASS] Test 365: Propriétés des sections (intégration indépendante, catalogues)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 366 : EC2 et EC3 — calculs manuels indépendants
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Standards::Design;
        // a) Flexion simple, C25/30, B500, b = 0,30 m, d = 0,45 m, M_Ed = 150 kN·m (aciers tendus seuls)
        EC2BeamBendingInput in;
        in.b = 0.30, in.h = 0.50, in.d = 0.45, in.fck = 25e6, in.fyk = 500e6, in.Med = 150e3, in.gammaC = 1.5, in.gammaS = 1.15;
        const double fcd = 25e6 / 1.5, fyd = 500e6 / 1.15;
        const double mu = 150e3 / (0.30 * 0.45 * 0.45 * fcd);
        const double alpha = 1.25 * (1.0 - std::sqrt(1.0 - 2.0 * mu));
        const double As = 150e3 / (0.45 * (1.0 - 0.4 * alpha) * fyd);
        const auto r1 = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        TEST_CHECK(r1.valid && !r1.requiresCompressionSteel && rel(r1.mu_cu, mu, 1e-9) && rel(r1.As_required, As, 1e-9),
                   "Test 366: EC2 flexion simple, As = " << r1.As_required * 1e4 << " cm² (manuel " << As * 1e4 << ")");
        // b) M_Ed = 500 kN·m : μ > μ_lu, aciers comprimés (d2 = 5 cm) — l'ancienne version donnait As = M/(0,8·d·fyd), sans As2
        in.Med = 500e3;
        const double epsYd = fyd / 200e9, aLim = 3.5e-3 / (3.5e-3 + epsYd);
        const double muLu = 0.8 * aLim * (1.0 - 0.4 * aLim);
        const double Mlu = muLu * 0.30 * 0.45 * 0.45 * fcd, zlu = 0.45 * (1.0 - 0.4 * aLim), xlu = aLim * 0.45;
        const double sig2 = std::min(fyd, 200e9 * 3.5e-3 * (xlu - 0.05) / xlu);
        const double As2 = (500e3 - Mlu) / (0.40 * sig2), As1 = Mlu / (zlu * fyd) + As2 * sig2 / fyd;
        const auto r2 = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        TEST_CHECK(r2.valid && r2.requiresCompressionSteel && rel(r2.mu_lu, muLu, 1e-9) && rel(r2.As2_required, As2, 1e-9) && rel(r2.As_required, As1, 1e-9) && !r2.exceedsMaximum,
                   "Test 366: EC2 aciers comprimés As = " << r2.As_required * 1e4 << " / As2 = " << r2.As2_required * 1e4 << " cm² (manuel " << As1 * 1e4 << " / " << As2 * 1e4 << ")");
        TEST_CHECK(std::abs(r2.mu_lu - 0.3717) < 5e-4, "Test 366: μ_lu (B500, C25/30) = " << r2.mu_lu << " (0,372 usuel)");
        // c) C60/75 : λ = 0,775, η = 0,95, εcu3 = 2,88 ‰ (§3.1.7(3), Tableau 3.1)
        in.fck = 60e6, in.Med = 300e3;
        const auto r3 = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        const double fcd60 = 60e6 / 1.5, mu60 = 300e3 / (0.30 * 0.45 * 0.45 * fcd60);
        const double la = 1.0 - std::sqrt(1.0 - 2.0 * mu60 / 0.95);   // λ·α
        TEST_CHECK(r3.valid && std::abs(r3.lambda - 0.775) < 1e-12 && std::abs(r3.eta - 0.95) < 1e-12 && std::abs(r3.epsCu3 - 2.8835e-3) < 1e-7 &&
                       rel(r3.As_required, 300e3 / (0.45 * (1.0 - la / 2.0) * fyd), 1e-9),
                   "Test 366: EC2 béton C60/75 (λ, η, εcu3)");
        in.fck = 100e6;
        TEST_CHECK(!ConcreteDesignEC2::calculateRectangularBeamFlexure(in).valid, "Test 366: fck > 90 MPa refusé");
        // d) Section maximale dépassée : signalée
        in.fck = 25e6, in.Med = 900e3, in.b = 0.20;
        const auto r4 = ConcreteDesignEC2::calculateRectangularBeamFlexure(in);
        TEST_CHECK(r4.exceedsMaximum, "Test 366: As + As2 > 0,04·Ac signalé (" << (r4.As_provided + r4.As2_required) * 1e4 << " cm² > " << r4.As_max * 1e4 << ")");
        TEST_CHECK(!ConcreteDesignEC2::calculateFromModel(Section::ipe(300), Material::concreteC25_30(), 50e3).valid, "Test 366: section non rectangulaire refusée par EC2");

        // e) EC3 — IPE 200, S235, L = 4 m, axe faible : courbe b (h/b > 1,2, tf ≤ 40 mm), χ manuel
        const Section ipe = Section::ipe(200);
        const Material s235 = Material::steelS235();
        const double i = std::sqrt(ipe.iz() / ipe.area()), lb = 4.0 / i / (3.14159265358979323846 * std::sqrt(210e9 / 235e6));
        const double phi = 0.5 * (1.0 + 0.34 * (lb - 0.2) + lb * lb), chi = 1.0 / (phi + std::sqrt(phi * phi - lb * lb));
        const auto b1 = SteelDesignEC3::calculateFromBar(ipe, s235, 4.0, 1.0, 100e3, false);
        TEST_CHECK(b1.valid && rel(b1.chi, chi, 1e-9) && rel(b1.Nb_Rd, chi * ipe.area() * 235e6, 1e-9) && b1.sectionClass == 1 && b1.curveReason.contains("courbe b"),
                   "Test 366: EC3 IPE 200 χ = " << b1.chi << " (manuel " << chi << "), Nb,Rd = " << b1.Nb_Rd / 1e3 << " kN, " << b1.curveReason.toStdString());
        // f) Tableau 6.2 : HEB (h/b ≤ 1,2) → b / c (ancienne version : a / b, non sécuritaire) ; S460 → a ; tubes et pleins → c
        const Material s355 = Material::steelS355();
        TEST_CHECK(SteelDesignEC3::bucklingCurve(Section::heb(200), 355e6, true) == EC3BucklingCurve::b && SteelDesignEC3::bucklingCurve(Section::heb(200), 355e6, false) == EC3BucklingCurve::c,
                   "Test 366: HEB 200 → courbes b (y) et c (z)");
        TEST_CHECK(SteelDesignEC3::bucklingCurve(Section::ipe(300), 460e6, false) == EC3BucklingCurve::a0 && SteelDesignEC3::bucklingCurve(Section::heb(200), 460e6, false) == EC3BucklingCurve::a,
                   "Test 366: S460 → a0 (h/b > 1,2) et a (h/b ≤ 1,2)");
        TEST_CHECK(SteelDesignEC3::bucklingCurve(Section::rectangular(0.1, 0.1), 235e6, true) == EC3BucklingCurve::c && SteelDesignEC3::bucklingCurve(Section::pipe(0.2, 0.01), 235e6, true) == EC3BucklingCurve::c &&
                       SteelDesignEC3::bucklingCurve(Section::angle(0.1, 0.1, 0.01), 235e6, true) == EC3BucklingCurve::b && SteelDesignEC3::bucklingCurve(Section::upn(200), 235e6, true) == EC3BucklingCurve::c,
                   "Test 366: pleins, tubes (formés à froid), U → c ; cornières → b");
        // g) Tableau 5.2 : classe 4 détectée et vérification refusée (âme élancée, tube mince) ; HEB 200 S355 classe 1
        Section prs = Section::rectangular(0.3, 1.0);
        prs.shape = TSA::Model::SectionShape::IShape, prs.name = "PRS", prs.width = 0.30, prs.height = 1.00, prs.tw = 0.006, prs.tf = 0.020;
        const auto c4 = SteelDesignEC3::calculateFromBar(prs, s355, 3.0, 1.0, 500e3, true);
        TEST_CHECK(SteelDesignEC3::compressionClass(Section::heb(200), 355e6) == 1 && SteelDesignEC3::compressionClass(prs, 355e6) == 4 && !c4.valid && !c4.pass &&
                       SteelDesignEC3::compressionClass(Section::pipe(0.3239, 0.005), 355e6) == 4,
                   "Test 366: classes en compression (HEB 200 : 1 ; âme 960/6 et tube 324×5 en S355 : 4, vérification refusée)");
        std::cout << "[PASS] Test 366: EC2 et EC3 conformes aux calculs manuels (μ, As, As2, χ, courbes, classes)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 369 : invariants mathématiques de la chaîne éléments finis
    // -------------------------------------------------------------------------
    {
        using namespace TSA::Analysis;
        Model m;
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(0, 0, 3), n3 = m.addNode(4, 2, 3.5), n4 = m.addNode(4, 2, 0);
        m.getNode(n1)->setSupport(SupportDefinition::fixed());
        m.getNode(n4)->setSupport(SupportDefinition::fixed());
        const Material steel = Material::steelS355();
        m.addColumn(n1, n2, Section::rectangular(0.3, 0.5), steel, 30.0);   // section tournée de 30°
        m.addBar(n2, n3, Section::ipe(300), steel, BarRole::Beam);           // poutre inclinée dans l'espace
        m.addColumn(n4, n3, Section::heb(200), steel);
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "F", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 12.0, -7.0, -30.0, 4.0, -2.0, 3.0));
        m.loadManager().addNodalLoad(NodalLoad(0, n3, lc, -5.0, 9.0, -45.0, 0.0, 6.0, -1.5));
        AnalysisEngineRegistry reg;
        registerBuiltInEngines(reg);
        AnalysisManager mgr(reg);
        AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        ctx.loadCaseIds = { lc };
        ctx.engineSettings["opensees"] = QJsonObject { { "extractionLevel", 1 } };   // ExtractionLevel::Advanced
        GridManager gm;
        const auto prep = mgr.prepare(m, &gm, ctx);
        const auto run = prep.canRun() ? mgr.run(ctx, prep) : AnalysisRunResult {};
        TEST_CHECK(run.success, "Test 369: calcul avec extraction avancée (" << run.message << ")");
        const auto& adv = run.results.advanced();
        TEST_CHECK(adv.available && adv.hasGlobalStiffness && adv.elementMatrices.size() == 3, "Test 369: matrices extraites (" << adv.elementMatrices.size() << ")");

        const double E = steel.mechanical.youngModulus * 1e-3, G = E / (2.0 * (1.0 + steel.mechanical.poissonRatio));
        double worstLocal = 0, worstRigid = 0, worstSym = 0, worstOrth = 0;
        const int nEq = adv.dofMap.equationCount();
        std::vector<double> Kasm(std::size_t(nEq) * nEq, 0.0);
        for (const auto& [key, em] : adv.elementMatrices)
        {
            TEST_CHECK(em.available && em.kLocal.rows == 12 && em.kGlobal.rows == 12, "Test 369: matrices 12×12 de " << key.label());
            const Section sec = key.kind == StructuralElementKind::Beam ? m.getBeam(key.id)->section() : m.getColumn(key.id)->section();
            const double L = em.length, A = sec.area(), Iy = sec.iy(), Iz = sec.iz(), J = sec.it();
            // Matrice analytique d'Euler-Bernoulli, ordre OpenSees (ux uy uz rx ry rz)_i (…)_j
            double k[12][12] = {};
            auto set = [&](int i, int j, double v) { k[i][j] = k[j][i] = v; };
            set(0, 0, E * A / L), set(0, 6, -E * A / L), set(6, 6, E * A / L);
            set(3, 3, G * J / L), set(3, 9, -G * J / L), set(9, 9, G * J / L);
            const double z12 = 12 * E * Iz / (L * L * L), z6 = 6 * E * Iz / (L * L), z4 = 4 * E * Iz / L, z2 = 2 * E * Iz / L;   // plan x-y
            set(1, 1, z12), set(1, 5, z6), set(1, 7, -z12), set(1, 11, z6), set(5, 5, z4), set(5, 7, -z6), set(5, 11, z2), set(7, 7, z12), set(7, 11, -z6), set(11, 11, z4);
            const double y12 = 12 * E * Iy / (L * L * L), y6 = 6 * E * Iy / (L * L), y4 = 4 * E * Iy / L, y2 = 2 * E * Iy / L;   // plan x-z
            set(2, 2, y12), set(2, 4, -y6), set(2, 8, -y12), set(2, 10, -y6), set(4, 4, y4), set(4, 8, y6), set(4, 10, y2), set(8, 8, y12), set(8, 10, y6), set(10, 10, y4);
            double kmax = 0;
            for (int i = 0; i < 12; ++i)
                for (int j = 0; j < 12; ++j) kmax = std::max(kmax, std::abs(k[i][j]));
            for (int i = 0; i < 12; ++i)
                for (int j = 0; j < 12; ++j)
                {
                    worstLocal = std::max(worstLocal, std::abs(em.kLocal(i, j) - k[i][j]) / kmax);
                    worstSym = std::max(worstSym, std::abs(em.kGlobal(i, j) - em.kGlobal(j, i)) / kmax);
                }
            // R orthogonale (lignes = axes locaux x, y, z)
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                {
                    double d = 0;
                    for (int c = 0; c < 3; ++c) d += em.rotation[a * 3 + c] * em.rotation[b * 3 + c];
                    worstOrth = std::max(worstOrth, std::abs(d - (a == b ? 1.0 : 0.0)));
                }
            // Mouvements de corps rigide : 3 translations, 3 rotations u = θ × x (x : coordonnées des nœuds)
            const auto* ni = m.getNode(em.nodeI);
            const auto* nj = m.getNode(em.nodeJ);
            const double X[2][3] = { { ni->x(), ni->y(), ni->z() }, { nj->x(), nj->y(), nj->z() } };
            for (int mode = 0; mode < 6; ++mode)
            {
                double u[12] = {};
                for (int n = 0; n < 2; ++n)
                {
                    if (mode < 3) u[n * 6 + mode] = 1.0;
                    else
                    {
                        double th[3] = { 0, 0, 0 };
                        th[mode - 3] = 1.0;
                        u[n * 6 + 0] = th[1] * X[n][2] - th[2] * X[n][1];
                        u[n * 6 + 1] = th[2] * X[n][0] - th[0] * X[n][2];
                        u[n * 6 + 2] = th[0] * X[n][1] - th[1] * X[n][0];
                        u[n * 6 + 3 + (mode - 3)] = 1.0;
                    }
                }
                for (int i = 0; i < 12; ++i)
                {
                    double f = 0;
                    for (int j = 0; j < 12; ++j) f += em.kGlobal(i, j) * u[j];
                    worstRigid = std::max(worstRigid, std::abs(f) / (kmax * 10.0));
                }
            }
            // Assemblage sur les DDL libres
            for (int a = 0; a < 12; ++a)
            {
                const int ea = adv.dofMap.equationOf(a < 6 ? em.nodeI : em.nodeJ, a % 6);
                if (ea < 0) continue;
                for (int b = 0; b < 12; ++b)
                {
                    const int eb = adv.dofMap.equationOf(b < 6 ? em.nodeI : em.nodeJ, b % 6);
                    if (eb >= 0) Kasm[std::size_t(ea) * nEq + eb] += em.kGlobal(a, b);
                }
            }
        }
        std::cout << "  écarts : K_local/analytique " << worstLocal << ", symétrie " << worstSym << ", RRᵀ − I " << worstOrth << ", corps rigide " << worstRigid << std::endl;
        TEST_CHECK(worstLocal < 1e-6, "Test 369: matrice locale = Euler-Bernoulli analytique (écart relatif " << worstLocal << ")");
        TEST_CHECK(worstSym < 1e-9 && worstOrth < 1e-9, "Test 369: K_e global symétrique, R orthogonale");
        TEST_CHECK(worstRigid < 1e-7, "Test 369: 6 mouvements de corps rigide sans effort (" << worstRigid << ")");

        // K global OpenSees = assemblage TSA ; symétrique ; défini positif (Cholesky) ; résidu K·U − F
        double kAbs = 0, dAsm = 0, dSym = 0;
        for (int i = 0; i < nEq; ++i)
            for (int j = 0; j < nEq; ++j) kAbs = std::max(kAbs, std::abs(adv.kGlobal.at(i, j)));
        for (int i = 0; i < nEq; ++i)
            for (int j = 0; j < nEq; ++j)
            {
                dAsm = std::max(dAsm, std::abs(adv.kGlobal.at(i, j) - Kasm[std::size_t(i) * nEq + j]) / kAbs);
                dSym = std::max(dSym, std::abs(adv.kGlobal.at(i, j) - adv.kGlobal.at(j, i)) / kAbs);
            }
        std::vector<double> L(std::size_t(nEq) * nEq, 0.0);
        bool spd = true;
        for (int j = 0; j < nEq && spd; ++j)
        {
            double s = adv.kGlobal.at(j, j);
            for (int k2 = 0; k2 < j; ++k2) s -= L[std::size_t(j) * nEq + k2] * L[std::size_t(j) * nEq + k2];
            if (s <= 0) spd = false;
            else L[std::size_t(j) * nEq + j] = std::sqrt(s);
            for (int i = j + 1; i < nEq && spd; ++i)
            {
                double v = adv.kGlobal.at(i, j);
                for (int k2 = 0; k2 < j; ++k2) v -= L[std::size_t(i) * nEq + k2] * L[std::size_t(j) * nEq + k2];
                L[std::size_t(i) * nEq + j] = v / L[std::size_t(j) * nEq + j];
            }
        }
        std::vector<double> F(nEq, 0.0);
        for (const auto& [id, nl] : m.loadManager().nodalLoads())
        {
            const double f[6] = { nl.fx(), nl.fy(), nl.fz(), nl.mx(), nl.my(), nl.mz() };
            for (int d = 0; d < 6; ++d)
                if (const int e = adv.dofMap.equationOf(nl.nodeId(), d); e >= 0) F[e] += f[d];
        }
        const auto U = adv.globalDisplacementVector(run.results.allDisplacements());
        double rn = 0, fn = 0;
        for (int i = 0; i < nEq; ++i)
        {
            double r = -F[i];
            for (int j = 0; j < nEq; ++j) r += adv.kGlobal.at(i, j) * U[j];
            rn += r * r, fn += F[i] * F[i];
        }
        const double resid = std::sqrt(rn / fn);
        std::cout << "  K global : " << nEq << " équations, écart à l'assemblage " << dAsm << ", symétrie " << dSym << ", résidu ‖KU − F‖/‖F‖ = " << resid << std::endl;
        TEST_CHECK(nEq == 12 && dAsm < 1e-8 && dSym < 1e-12 && spd, "Test 369: K global = Σ K_e assemblées, symétrique, défini positif");
        TEST_CHECK(resid < 1e-6, "Test 369: résidu ‖K·U − F‖/‖F‖ = " << resid);
        const auto eq = run.results.equilibrium();
        TEST_CHECK(std::abs(eq.reactionFx + 7.0) < 1e-6 && std::abs(eq.reactionFy + 2.0) < 1e-6 && std::abs(eq.reactionFz - 75.0) < 1e-6,
                   "Test 369: Σ réactions = −Σ charges (" << eq.reactionFx << ", " << eq.reactionFy << ", " << eq.reactionFz << ")");
        std::cout << "[PASS] Test 369: Invariants de la chaîne éléments finis (matrices, assemblage, résidu, équilibre)" << std::endl;
        ++passed;
    }
    // -------------------------------------------------------------------------
    // TEST 370 : rendu graphique réellement obtenu
    // -------------------------------------------------------------------------
    {
        OccView view;
        view.resize(320, 240);
        view.show();
        for (int k = 0; k < 20; ++k) { QApplication::processEvents(); QThread::msleep(10); }
        const auto info = view.renderingInfo();
        std::cout << "  rendu : " << info.summary().toStdString() << std::endl;
        TEST_CHECK(!info.device.isEmpty() && !info.version.isEmpty() && !info.summary().isEmpty(), "Test 370: carte et version OpenGL identifiées");
        TEST_CHECK(!info.compatibilityMode || qEnvironmentVariableIntValue("TSA_RENDER_COMPAT") == 1, "Test 370: mode de compatibilité seulement sur demande");
        view.hide();
        std::cout << "[PASS] Test 370: Rendu graphique identifié (" << (info.hardware ? "matériel" : "logiciel") << ")" << std::endl;
        ++passed;
    }
    return true;
}
