// Suite « audit » : corrections issues de l'audit scientifique des calculs (docs/AUDIT_CALCULS.md).
// Chaque défaut corrigé a ici son test de non-régression, comparé à une référence indépendante :
//   365  propriétés des sections : intégration numérique indépendante de la géométrie (T, cornière, UPN, I) et
//        valeurs publiées des catalogues (EN 10365 / ArcelorMittal) pour les profils laminés ;
//   366  EN 1992-1-1 (flexion simple, aciers comprimés, bétons à haute résistance) et EN 1993-1-1 (Tableau 6.2,
//        Tableau 5.2, χ) comparés à des calculs manuels indépendants écrits dans le test.

#include "test_common.h"

#include "Standards/Design/ConcreteDesignEC2.h"
#include "Standards/Design/SteelDesignEC3.h"

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
    return true;
}
