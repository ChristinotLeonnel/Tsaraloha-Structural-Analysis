// Courbes RDM d'une barre à partir de ses efforts d'extrémité (coupure en x, tronçon [0, x]) :
//   N(x) = -Fx_i - Px(x)                      (traction > 0)
//   M(x) = -Mz_i + x·Fy_i + MPy(x)            (M(0) = -Mz_i, M(L) = Mz_j)
//   V(x) = dM/dx = Fy_i + Py(x)
// Déformée « selon la RDM » (Navier-Bernoulli, intégration de la ligne élastique) :
//   EI v''(x) = M(x)   avec v(0) = v_i, v(L) = v_j   (rotules comprises : seuls les
//   déplacements d'extrémité sont imposés, la rotation de la section est une inconnue) ;
//   EA u'(x)  = N(x)   avec u(0) = u_i.
// Les intégrales sont exactes (Gauss 3 points par tronçon régulier, intégrandes polynomiales
// de degré ≤ 5). La version d'origine échantillonnait M et V avec un pas fixe (0,01 m) et ne
// calculait pas de déformée.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace mdd
{

using namespace detail;

namespace
{
/// Racine de g sur [a, b] (g(a), g(b) de signes opposés) par dichotomie.
double bisect(const std::function<double(double)>& g, double a, double b)
{
    double ga = g(a);
    for (int it = 0; it < 80 && b - a > 1e-12; ++it)
    {
        const double m = 0.5 * (a + b), gm = g(m);
        if ((gm > 0) == (ga > 0)) { a = m; ga = gm; }
        else b = m;
    }
    return 0.5 * (a + b);
}
} // namespace

void computeCurves(const Model& model, int memberIndex, const MemberEndForces& end,
                   const std::array<double, 4>& d, MemberResult& out)
{
    const Member& mb = model.members[static_cast<std::size_t>(memberIndex)];
    const Node& ni = model.nodes[static_cast<std::size_t>(mb.i)];
    const Node& nj = model.nodes[static_cast<std::size_t>(mb.j)];
    const double L = std::hypot(nj.x - ni.x, nj.y - ni.y);
    const LoadSet loads(model, memberIndex, L);
    const auto& br = loads.breakpoints();
    const double EI = mb.E * mb.I;
    const double EA = mb.E * mb.A * model.options.axialStiffnessFactor;
    const double ui = d[0], vi = d[1], vj = d[3];

    auto N = [&](double x) { return -end.fxI - loads.Px(x); };
    auto M = [&](double x) { return -end.mzI + x * end.fyI + loads.MPy(x); };
    auto V = [&](double x) { return end.fyI + loads.Py(x); };

    // Ligne élastique : v(x) = v_i + θ0·x + (1/EI)·∫0^x (x - s) M(s) ds
    const double intML = integrate([&](double s) { return (L - s) * M(s); }, 0.0, L, br);
    const double theta0 = EI > 0 ? (vj - vi - intML / EI) / L : (vj - vi) / L;
    auto vAt = [&](double x) {
        if (EI <= 0) return vi + theta0 * x;
        return vi + theta0 * x + integrate([&](double s) { return (x - s) * M(s); }, 0.0, x, br) / EI;
    };
    auto uAt = [&](double x) { return EA > 0 ? ui + integrate(N, 0.0, x, br) / EA : ui; };

    // Abscisses : grille régulière + points singuliers (de part et d'autre des forces
    // concentrées) + extrema de M (V = 0) et zéros de M en travée.
    const int n = std::max(3, model.options.curvePoints);
    std::vector<double> xs;
    for (int k = 0; k < n; ++k) xs.push_back(L * k / (n - 1));
    const double eps = 1e-9 * L;
    for (double b : br) { xs.push_back(b - eps); xs.push_back(b + eps); }
    std::sort(xs.begin(), xs.end());

    CurveSummary& sm = out.summary;
    sm = {};
    sm.length = L;
    // Racines de V (extrema de M) et de M : changement de signe strict entre deux points, ou
    // zéro exact sur un point de la grille encadré par des valeurs de signes opposés.
    double vScale = 0.0, mScale = 0.0;
    for (double x : xs) { vScale = std::max(vScale, std::abs(V(x))); mScale = std::max(mScale, std::abs(M(x))); }
    auto interior = [&](double x) { return x > 1e-6 * L && x < L * (1 - 1e-6); };
    auto roots = [&](const std::function<double(double)>& g, double scale, bool skipJumps) {
        std::vector<double> found;
        const double z = 1e-12 * std::max(scale, 1e-300);
        auto jumpBetween = [&](double a, double b) {
            return skipJumps && std::any_of(loads.pointAbscissas().begin(), loads.pointAbscissas().end(),
                                            [&](double p) { return p > a && p < b; });
        };
        for (std::size_t k = 0; k < xs.size(); ++k)
        {
            const double gk = g(xs[k]);
            if (std::abs(gk) <= z)
            {
                if (k == 0 || k + 1 >= xs.size() || !interior(xs[k])) continue;
                const double gp = g(xs[k - 1]), gn = g(xs[k + 1]);
                if (!jumpBetween(xs[k - 1], xs[k + 1]) && ((gp > z && gn < -z) || (gp < -z && gn > z))) found.push_back(xs[k]);
                continue;
            }
            if (k + 1 >= xs.size() || jumpBetween(xs[k], xs[k + 1])) continue;
            const double gn = g(xs[k + 1]);
            if (std::abs(gn) <= z) continue;   // zéro exact : traité au point suivant
            if ((gk > 0) != (gn > 0))
            {
                const double x = bisect(g, xs[k], xs[k + 1]);
                if (interior(x)) found.push_back(x);
            }
        }
        return found;
    };
    std::vector<double> extra = roots(V, vScale, true);
    for (double x : extra)
        if (!sm.hasSpanExtremum || std::abs(M(x)) > std::abs(sm.MSpanExtremum))
        {
            sm.hasSpanExtremum = true;
            sm.xSpanExtremum = x;
            sm.MSpanExtremum = M(x);
        }
    sm.momentZeros = roots(M, mScale, false);
    xs.insert(xs.end(), extra.begin(), extra.end());
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(), [&](double a, double b) { return std::abs(a - b) < 1e-12 * std::max(L, 1.0); }), xs.end());

    out.curve.clear();
    out.curve.reserve(xs.size());
    for (double x : xs) out.curve.push_back({ x, N(x), V(x), M(x), uAt(x), vAt(x) });

    // Valeurs caractéristiques
    sm.Mi = M(0.0);
    sm.Mj = M(L);
    const double tiny = 1e-14 * L;   // V(0+) et V(L-) : forces concentrées d'extrémité comprises
    sm.Vi = V(tiny);
    sm.Vj = V(L - tiny);
    sm.Mmax = sm.Mmin = sm.Mi;
    sm.Nmin = sm.Nmax = N(0.0);
    for (const auto& p : out.curve)
    {
        if (p.M > sm.Mmax) { sm.Mmax = p.M; sm.xMmax = p.x; }
        if (p.M < sm.Mmin) { sm.Mmin = p.M; sm.xMmin = p.x; }
        sm.Nmin = std::min(sm.Nmin, p.N);
        sm.Nmax = std::max(sm.Nmax, p.N);
    }

    // Flèche : écart à la corde, maximum affiné par recherche ternaire autour du meilleur point.
    auto f = [&](double x) { return vAt(x) - (vi + (vj - vi) * x / L); };
    std::size_t best = 0;
    for (std::size_t k = 0; k < out.curve.size(); ++k)
    {
        const double fk = out.curve[k].v - (vi + (vj - vi) * out.curve[k].x / L);
        const double fb = out.curve[best].v - (vi + (vj - vi) * out.curve[best].x / L);
        if (std::abs(fk) > std::abs(fb)) best = k;
    }
    double lo = best > 0 ? out.curve[best - 1].x : 0.0;
    double hi = best + 1 < out.curve.size() ? out.curve[best + 1].x : L;
    for (int it = 0; it < 60; ++it)
    {
        const double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
        if (std::abs(f(m1)) < std::abs(f(m2))) lo = m1; else hi = m2;
    }
    sm.xDeflectionMax = 0.5 * (lo + hi);
    sm.deflectionMax = f(sm.xDeflectionMax);

    sm.rotationI = theta0;
    sm.rotationJ = EI > 0 ? theta0 + integrate(M, 0.0, L, br) / EI : theta0;
}

} // namespace mdd
