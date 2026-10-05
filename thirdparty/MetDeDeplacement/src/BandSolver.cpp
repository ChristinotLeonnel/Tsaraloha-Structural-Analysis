// Résolution du système K·d = F : stockage bande symétrique + Cholesky (O(n·b²) au lieu de
// l'inversion complète O(n³) de la version d'origine), après renumérotation Cuthill–McKee
// inverse des nœuds.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace mdd::detail
{

BandMatrix::BandMatrix(int n, int halfBandwidth)
    : m_n(n), m_bw(halfBandwidth), m_a(static_cast<std::size_t>(n) * (halfBandwidth + 1), 0.0)
{
}

void BandMatrix::add(int i, int j, double v)
{
    if (i > j) std::swap(i, j);
    at(i, j - i) += v;
}

bool BandMatrix::factorize(int* failedEquation)
{
    double maxDiag = 0.0;
    for (int i = 0; i < m_n; ++i) maxDiag = std::max(maxDiag, std::abs(at(i, 0)));
    const double tol = std::max(maxDiag, 1.0) * 1e-12;

    // Cholesky bande : A = Uᵀ U, U stocké à la place de A (partie supérieure).
    for (int i = 0; i < m_n; ++i)
    {
        const int kmin = std::max(0, i - m_bw);
        double d = at(i, 0);
        for (int k = kmin; k < i; ++k)
        {
            const double u = at(k, i - k);
            d -= u * u;
        }
        if (d <= tol)
        {
            if (failedEquation) *failedEquation = i;
            return false;
        }
        const double piv = std::sqrt(d);
        at(i, 0) = piv;
        const int jmax = std::min(m_n - 1, i + m_bw);
        for (int j = i + 1; j <= jmax; ++j)
        {
            double s = at(i, j - i);
            const int k0 = std::max(kmin, j - m_bw);
            for (int k = k0; k < i; ++k) s -= at(k, i - k) * at(k, j - k);
            at(i, j - i) = s / piv;
        }
    }
    return true;
}

void BandMatrix::solve(std::vector<double>& b) const
{
    for (int i = 0; i < m_n; ++i)   // Uᵀ y = b
    {
        double s = b[i];
        for (int k = std::max(0, i - m_bw); k < i; ++k) s -= at(k, i - k) * b[k];
        b[i] = s / at(i, 0);
    }
    for (int i = m_n - 1; i >= 0; --i)   // U x = y
    {
        double s = b[i];
        for (int j = i + 1; j <= std::min(m_n - 1, i + m_bw); ++j) s -= at(i, j - i) * b[j];
        b[i] = s / at(i, 0);
    }
}

std::vector<int> reverseCuthillMcKee(int nodeCount, const std::vector<Member>& members)
{
    std::vector<std::vector<int>> adj(static_cast<std::size_t>(nodeCount));
    for (const auto& m : members)
    {
        if (m.i < 0 || m.j < 0 || m.i >= nodeCount || m.j >= nodeCount || m.i == m.j) continue;
        adj[m.i].push_back(m.j);
        adj[m.j].push_back(m.i);
    }
    for (auto& a : adj)
    {
        std::sort(a.begin(), a.end());
        a.erase(std::unique(a.begin(), a.end()), a.end());
    }
    std::vector<int> order;
    std::vector<char> seen(static_cast<std::size_t>(nodeCount), 0);
    auto degree = [&](int n) { return adj[n].size(); };
    while (static_cast<int>(order.size()) < nodeCount)
    {
        int start = -1;   // départ : nœud non visité de plus petit degré (composante suivante)
        for (int n = 0; n < nodeCount; ++n)
            if (!seen[n] && (start < 0 || degree(n) < degree(start))) start = n;
        std::queue<int> q;
        q.push(start);
        seen[start] = 1;
        while (!q.empty())
        {
            const int n = q.front();
            q.pop();
            order.push_back(n);
            std::vector<int> next;
            for (int m : adj[n])
                if (!seen[m]) { seen[m] = 1; next.push_back(m); }
            std::sort(next.begin(), next.end(), [&](int a, int b) { return degree(a) < degree(b); });
            for (int m : next) q.push(m);
        }
    }
    std::reverse(order.begin(), order.end());
    return order;
}

} // namespace mdd::detail
