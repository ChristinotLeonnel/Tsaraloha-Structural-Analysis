#pragma once

// Outils internes : fonctions de charge d'une barre (intégrales exactes), rigidité locale,
// efforts d'encastrement parfait et solveur bande.

#include "mdd/MetDeDeplacement.h"

#include <array>
#include <vector>

namespace mdd::detail
{

using Mat6 = std::array<std::array<double, 6>, 6>;
using Vec6 = std::array<double, 6>;

/// Charges d'une barre (repère local) et leurs intégrales exactes le long de x.
///  Px(x)  = ∫0^x px ds          Py(x) = ∫0^x py ds
///  MPy(x) = ∫0^x (x - s) py ds  (moment en x des charges transversales de [0, x])
/// Les forces concentrées en a ne comptent que pour x > a.
class LoadSet
{
public:
    LoadSet(const Model& model, int member, double length);

    double Px(double x) const;
    double Py(double x) const;
    double MPy(double x) const;

    /// Abscisses singulières (début / fin de charge répartie, forces concentrées), triées, dans ]0, L[.
    const std::vector<double>& breakpoints() const { return m_breaks; }
    /// Abscisses des forces concentrées (discontinuité de V).
    const std::vector<double>& pointAbscissas() const { return m_points; }
    double length() const { return m_L; }

private:
    /// Force concentrée en a comptée pour x > a (et en x = L si elle est à l'extrémité j).
    bool acts(double a, double x) const { return x > a || (a >= m_L && x >= m_L); }
    struct Dist { double a, b, px1, py1, kx, ky; };
    struct Pt { double a, px, py; };
    std::vector<Dist> m_dist;
    std::vector<Pt> m_pts;
    std::vector<double> m_breaks, m_points;
    double m_L = 0.0;
};

/// ∫ f sur [x0, x1] découpé aux abscisses singulières, Gauss-Legendre 3 points par tronçon
/// (exact pour les polynômes de degré ≤ 5 : les intégrandes de la méthode le sont par tronçon).
template <typename F>
double integrate(F&& f, double x0, double x1, const std::vector<double>& breaks)
{
    static constexpr double g = 0.77459666924148337704;   // sqrt(3/5)
    static constexpr double w0 = 8.0 / 9.0, w1 = 5.0 / 9.0;
    double sum = 0.0, a = x0;
    auto piece = [&](double lo, double hi) {
        if (hi - lo <= 0.0) return;
        const double c = 0.5 * (lo + hi), h = 0.5 * (hi - lo);
        sum += h * (w0 * f(c) + w1 * (f(c - g * h) + f(c + g * h)));
    };
    for (double b : breaks)
    {
        if (b <= a) continue;
        if (b >= x1) break;
        piece(a, b);
        a = b;
    }
    piece(a, x1);
    return sum;
}

struct LocalElement
{
    double L = 0.0, c = 1.0, s = 0.0;
    Mat6 k {};          ///< rigidité locale (rotules condensées)
    Vec6 fixedEnd {};   ///< efforts d'encastrement parfait sur la barre (rotules condensées)
};

LocalElement buildElement(const Model& model, int member);

/// Système bande symétrique défini positif (stockage supérieur), Cholesky en place.
class BandMatrix
{
public:
    BandMatrix(int n, int halfBandwidth);
    void add(int i, int j, double v);   ///< i, j quelconques ; seule la partie supérieure est stockée
    double diag(int i) const { return m_a[static_cast<std::size_t>(i) * (m_bw + 1)]; }
    /// false si pivot non positif (mécanisme) ; *failedEquation = équation fautive.
    bool factorize(int* failedEquation);
    void solve(std::vector<double>& b) const;
    int size() const { return m_n; }

private:
    double& at(int i, int k) { return m_a[static_cast<std::size_t>(i) * (m_bw + 1) + k]; }
    double at(int i, int k) const { return m_a[static_cast<std::size_t>(i) * (m_bw + 1) + k]; }
    int m_n = 0, m_bw = 0;
    std::vector<double> m_a;
};

/// Renumérotation Cuthill–McKee inverse des nœuds (réduit la largeur de bande).
std::vector<int> reverseCuthillMcKee(int nodeCount, const std::vector<Member>& members);

} // namespace mdd::detail
