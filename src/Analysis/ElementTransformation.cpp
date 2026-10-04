#include "ElementTransformation.h"

#include <cmath>

namespace TSA::Analysis
{

namespace
{
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}

double norm(const Vec3& a)
{
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

/// Ligne r (0..2) de la matrice de rotation R.
const Vec3& axisRow(const LocalAxes& axes, int r)
{
    return r == 0 ? axes.x : (r == 1 ? axes.y : axes.z);
}
} // namespace

LocalAxes ElementTransformation::openSeesAxes(const Vec3& nodeI, const Vec3& nodeJ, const Vec3& vecxz)
{
    LocalAxes axes;
    const Vec3 dx { nodeJ[0] - nodeI[0], nodeJ[1] - nodeI[1], nodeJ[2] - nodeI[2] };
    const double L = norm(dx);
    if (L <= 0.0)
        return axes;

    const Vec3 x { dx[0] / L, dx[1] / L, dx[2] / L };
    Vec3 y = cross(vecxz, x);
    const double ny = norm(y);
    if (ny <= 0.0) // vecxz parallèle à x : OpenSees refuse aussi (getLocalAxes retourne -3)
        return axes;
    y = { y[0] / ny, y[1] / ny, y[2] / ny };
    const Vec3 z = cross(x, y);

    axes.x = x;
    axes.y = y;
    axes.z = z;
    axes.length = L;
    axes.valid = true;
    return axes;
}

Vec12 ElementTransformation::globalToLocal(const LocalAxes& axes, const Vec12& g)
{
    Vec12 l {};
    for (int block = 0; block < 4; ++block)
        for (int r = 0; r < 3; ++r)
        {
            const Vec3& a = axisRow(axes, r);
            l[block * 3 + r] = a[0] * g[block * 3] + a[1] * g[block * 3 + 1] + a[2] * g[block * 3 + 2];
        }
    return l;
}

Vec12 ElementTransformation::localToGlobal(const LocalAxes& axes, const Vec12& l)
{
    Vec12 g {};
    for (int block = 0; block < 4; ++block)
        for (int c = 0; c < 3; ++c)
            g[block * 3 + c] = axes.x[c] * l[block * 3] + axes.y[c] * l[block * 3 + 1] + axes.z[c] * l[block * 3 + 2];
    return g;
}

std::array<double, 6> ElementTransformation::beamLocalToBasic(const Vec12& ul, double L)
{
    std::array<double, 6> ub {};
    const double oneOverL = 1.0 / L;
    ub[0] = ul[6] - ul[0];
    double tmp = oneOverL * (ul[1] - ul[7]);
    ub[1] = ul[5] + tmp;
    ub[2] = ul[11] + tmp;
    tmp = oneOverL * (ul[8] - ul[2]);
    ub[3] = ul[4] + tmp;
    ub[4] = ul[10] + tmp;
    ub[5] = ul[9] - ul[3];
    return ub;
}

DenseMatrix ElementTransformation::beamBasicToLocalStiffness(const DenseMatrix& kb, double L)
{
    // T_bl (6×12), lignes = relations de beamLocalToBasic.
    DenseMatrix T(6, 12);
    const double oneOverL = 1.0 / L;
    T(0, 0) = -1.0;       T(0, 6) = 1.0;
    T(1, 1) = oneOverL;   T(1, 7) = -oneOverL;  T(1, 5) = 1.0;
    T(2, 1) = oneOverL;   T(2, 7) = -oneOverL;  T(2, 11) = 1.0;
    T(3, 2) = -oneOverL;  T(3, 8) = oneOverL;   T(3, 4) = 1.0;
    T(4, 2) = -oneOverL;  T(4, 8) = oneOverL;   T(4, 10) = 1.0;
    T(5, 3) = -1.0;       T(5, 9) = 1.0;

    DenseMatrix kbT(6, 12);
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 12; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 6; ++k) s += kb(i, k) * T(k, j);
            kbT(i, j) = s;
        }

    DenseMatrix kl(12, 12);
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 12; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 6; ++k) s += T(k, i) * kbT(k, j);
            kl(i, j) = s;
        }
    return kl;
}

DenseMatrix ElementTransformation::trussBasicToLocalStiffness(double kb)
{
    DenseMatrix kl(12, 12);
    kl(0, 0) = kb;   kl(0, 6) = -kb;
    kl(6, 0) = -kb;  kl(6, 6) = kb;
    return kl;
}

DenseMatrix ElementTransformation::transformationMatrix(const LocalAxes& axes)
{
    DenseMatrix T(12, 12);
    for (int block = 0; block < 4; ++block)
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                T(block * 3 + r, block * 3 + c) = axisRow(axes, r)[c];
    return T;
}

DenseMatrix ElementTransformation::localToGlobalStiffness(const LocalAxes& axes, const DenseMatrix& kl)
{
    const DenseMatrix T = transformationMatrix(axes);
    DenseMatrix klT(12, 12);
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 12; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 12; ++k) s += kl(i, k) * T(k, j);
            klT(i, j) = s;
        }
    DenseMatrix kg(12, 12);
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 12; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 12; ++k) s += T(k, i) * klT(k, j);
            kg(i, j) = s;
        }
    return kg;
}

// -----------------------------------------------------------------------------
// AnalysisTypes : implémentations non triviales
// -----------------------------------------------------------------------------

double SparseMatrix::at(int i, int j) const
{
    std::size_t lo = 0, hi = values.size();
    while (lo < hi)
    {
        const std::size_t mid = (lo + hi) / 2;
        const bool less = rowIndex[mid] < i || (rowIndex[mid] == i && colIndex[mid] < j);
        if (less) lo = mid + 1;
        else hi = mid;
    }
    if (lo < values.size() && rowIndex[lo] == i && colIndex[lo] == j)
        return values[lo];
    return 0.0;
}

std::vector<double> SparseMatrix::multiply(const std::vector<double>& x) const
{
    std::vector<double> y(static_cast<std::size_t>(rows), 0.0);
    if (static_cast<int>(x.size()) != cols)
        return y;
    for (std::size_t k = 0; k < values.size(); ++k)
        y[rowIndex[k]] += values[k] * x[colIndex[k]];
    return y;
}

SparseMatrix SparseMatrix::fromDense(const DenseMatrix& m)
{
    SparseMatrix s;
    s.rows = m.rows;
    s.cols = m.cols;
    for (int i = 0; i < m.rows; ++i)
        for (int j = 0; j < m.cols; ++j)
        {
            const double v = m(i, j);
            if (v != 0.0)
            {
                s.rowIndex.push_back(i);
                s.colIndex.push_back(j);
                s.values.push_back(v);
            }
        }
    return s;
}

int DofMap::equationOf(int nodeId, int dof) const
{
    auto it = nodeEquations.find(nodeId);
    if (it == nodeEquations.end() || dof < 0 || dof >= static_cast<int>(it->second.size()))
        return -1;
    return it->second[dof];
}

std::string DofMap::equationLabel(int equation) const
{
    if (equation < 0 || equation >= equationCount())
        return "?";
    const auto& e = equations[equation];
    const std::string dofName = (e.dof >= 0 && e.dof < static_cast<int>(dofLabels.size()))
        ? dofLabels[e.dof] : std::to_string(e.dof + 1);
    return "N" + std::to_string(e.nodeId) + "." + dofName;
}

} // namespace TSA::Analysis
