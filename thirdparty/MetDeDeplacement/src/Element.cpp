// Barre de portique plan : rigidité locale, efforts d'encastrement parfait, rotules.
//
// Rigidité « méthode des rotations » : K = 4EI/L, report 2EI/L (= K/2), termes de
// translation 6EI/L² et 12EI/L³ ; effort normal EA/L.
// Efforts d'encastrement parfait obtenus par la méthode des forces sur la barre bi-encastrée
// (EI v'' = M, v = v' = 0 aux deux extrémités), pour toute combinaison de charges :
//   Fy_i = 12 (I1 - L·I0/2) / L³,  Mz_i = Fy_i·L/2 + I0/L,  Fx_i = -(1/L) ∫ Px
// avec I0 = ∫ MPy, I1 = ∫ (L - x) MPy (MPy : moment des charges transversales).
// Exemple : charge uniforme q → Fy = qL/2, M = qL²/12. Une rotule est traitée par
// condensation statique du DDL de rotation concerné (rigidité ET efforts), ce qui donne
// 3EI/L et qL²/8 côté encastré — le cas que la version d'origine laissait à qL²/12.

#include "Internal.h"

#include <algorithm>
#include <cmath>

namespace mdd::detail
{

LoadSet::LoadSet(const Model& model, int member, double length)
    : m_L(length)
{
    auto clamp = [&](double v) { return std::min(std::max(v, 0.0), m_L); };
    for (const auto& d : model.distributedLoads)
    {
        if (d.member != member) continue;
        const double a = clamp(std::min(d.a, d.b)), b = clamp(std::max(d.a, d.b));
        if (b - a <= 1e-12) continue;
        m_dist.push_back({ a, b, d.px1, d.py1, (d.px2 - d.px1) / (b - a), (d.py2 - d.py1) / (b - a) });
        m_breaks.push_back(a);
        m_breaks.push_back(b);
    }
    for (const auto& p : model.pointLoads)
    {
        if (p.member != member) continue;
        const double a = clamp(p.a);
        m_pts.push_back({ a, p.px, p.py });
        m_breaks.push_back(a);
        m_points.push_back(a);
    }
    auto tidy = [&](std::vector<double>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](double x) { return x <= 1e-12 || x >= m_L - 1e-12; }), v.end());
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end(), [](double a, double b) { return std::abs(a - b) < 1e-12; }), v.end());
    };
    tidy(m_breaks);
    tidy(m_points);
}

double LoadSet::Px(double x) const
{
    double s = 0.0;
    for (const auto& d : m_dist)
    {
        if (x <= d.a) continue;
        const double t = std::min(x, d.b) - d.a;
        s += d.px1 * t + d.kx * t * t / 2.0;
    }
    for (const auto& p : m_pts)
        if (acts(p.a, x)) s += p.px;
    return s;
}

double LoadSet::Py(double x) const
{
    double s = 0.0;
    for (const auto& d : m_dist)
    {
        if (x <= d.a) continue;
        const double t = std::min(x, d.b) - d.a;
        s += d.py1 * t + d.ky * t * t / 2.0;
    }
    for (const auto& p : m_pts)
        if (acts(p.a, x)) s += p.py;
    return s;
}

double LoadSet::MPy(double x) const
{
    double s = 0.0;
    for (const auto& d : m_dist)
    {
        if (x <= d.a) continue;
        if (x <= d.b)
        {
            const double t = x - d.a;   // ∫0^t (t - τ)(py1 + kτ) dτ
            s += d.py1 * t * t / 2.0 + d.ky * t * t * t / 6.0;
        }
        else
        {
            const double lb = d.b - d.a;
            const double F = d.py1 * lb + d.ky * lb * lb / 2.0;                         // résultante
            const double S = d.a * F + d.py1 * lb * lb / 2.0 + d.ky * lb * lb * lb / 3.0;   // ∫ s py ds
            s += x * F - S;
        }
    }
    for (const auto& p : m_pts)
        if (acts(p.a, x)) s += p.py * (x - p.a);
    return s;
}

namespace
{
void condense(Mat6& k, Vec6& f, int r)
{
    const double krr = k[r][r];
    if (std::abs(krr) < 1e-300) return;
    Mat6 kn = k;
    Vec6 fn = f;
    for (int a = 0; a < 6; ++a)
    {
        fn[a] = f[a] - k[a][r] * f[r] / krr;
        for (int b = 0; b < 6; ++b) kn[a][b] = k[a][b] - k[a][r] * k[r][b] / krr;
    }
    for (int a = 0; a < 6; ++a) { kn[a][r] = kn[r][a] = 0.0; }
    fn[r] = 0.0;
    k = kn;
    f = fn;
}
} // namespace

LocalElement buildElement(const Model& model, int member)
{
    LocalElement e;
    const Member& m = model.members[static_cast<std::size_t>(member)];
    const Node& ni = model.nodes[static_cast<std::size_t>(m.i)];
    const Node& nj = model.nodes[static_cast<std::size_t>(m.j)];
    const double dx = nj.x - ni.x, dy = nj.y - ni.y;
    e.L = std::hypot(dx, dy);
    if (e.L <= 0.0) return e;
    e.c = dx / e.L;
    e.s = dy / e.L;

    const double L = e.L, L2 = L * L, L3 = L2 * L;
    const double EA = m.E * m.A * model.options.axialStiffnessFactor / L;
    const double EI = m.E * m.I;
    auto& k = e.k;
    k[0][0] = k[3][3] = EA;
    k[0][3] = k[3][0] = -EA;
    k[1][1] = k[4][4] = 12 * EI / L3;
    k[1][4] = k[4][1] = -12 * EI / L3;
    k[1][2] = k[2][1] = k[1][5] = k[5][1] = 6 * EI / L2;
    k[2][4] = k[4][2] = k[4][5] = k[5][4] = -6 * EI / L2;
    k[2][2] = k[5][5] = 4 * EI / L;   // K de la méthode des rotations
    k[2][5] = k[5][2] = 2 * EI / L;   // report : K/2

    // Efforts d'encastrement parfait (barre bi-encastrée)
    const LoadSet loads(model, member, L);
    const auto& br = loads.breakpoints();
    const double I0 = integrate([&](double x) { return loads.MPy(x); }, 0.0, L, br);
    const double I1 = integrate([&](double x) { return (L - x) * loads.MPy(x); }, 0.0, L, br);
    const double Ip = integrate([&](double x) { return loads.Px(x); }, 0.0, L, br);
    auto& f = e.fixedEnd;
    f[1] = 12.0 * (I1 - L * I0 / 2.0) / L3;
    f[2] = f[1] * L / 2.0 + I0 / L;
    f[0] = -Ip / L;
    f[3] = -f[0] - loads.Px(L);
    f[4] = -f[1] - loads.Py(L);
    f[5] = -f[2] + L * f[1] + loads.MPy(L);

    if (m.releaseI) condense(e.k, e.fixedEnd, 2);
    if (m.releaseJ) condense(e.k, e.fixedEnd, 5);
    return e;
}

} // namespace mdd::detail
