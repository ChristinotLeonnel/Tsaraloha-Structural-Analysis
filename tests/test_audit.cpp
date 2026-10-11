// Suite « audit » : corrections issues de l'audit scientifique des calculs (docs/AUDIT_CALCULS.md).
// Chaque défaut corrigé a ici son test de non-régression, comparé à une référence indépendante :
//   365  propriétés des sections : intégration numérique indépendante de la géométrie (T, cornière, UPN, I) et
//        valeurs publiées des catalogues (EN 10365 / ArcelorMittal) pour les profils laminés.

#include "test_common.h"

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
    return true;
}
