#include "WorkPlaneGrid.h"

#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Prs3d_Presentation.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace TSA::Viewer
{

double WorkPlaneGrid::displayedStep(double baseStep, double worldPerPixel, double half, double minPixels)
{
    if (!(baseStep > 1e-9) || !std::isfinite(baseStep))
        baseStep = 1.0;
    const double minWorld = std::isfinite(worldPerPixel) && worldPerPixel > 0.0 ? worldPerPixel * minPixels : 0.0;
    const double maxLinesWorld = half > 0.0 ? 2.0 * half / kMaxLinesPerDirection : 0.0;
    const double needed = std::max(minWorld, maxLinesWorld);
    // Suite 1, 2, 5, 10, 20, 50… : chaque pas affiché est un multiple entier du pas d'accrochage du plan
    for (double decade = 1.0; decade < 1e9; decade *= 10.0)
    {
        for (double m : { 1.0, 2.0, 5.0 })
        {
            const double step = baseStep * m * decade;
            if (step >= needed)
                return step;
        }
    }
    return baseStep * 1e9;
}

int WorkPlaneGrid::lineCount(double half, double step, double center)
{
    if (!(step > 0.0) || !(half >= 0.0))
        return 0;
    const long long first = static_cast<long long>(std::ceil((center - half) / step - 1e-9));
    const long long last = static_cast<long long>(std::floor((center + half) / step + 1e-9));
    return last >= first ? static_cast<int>(last - first + 1) : 0;
}

bool WorkPlaneGrid::setLayout(const Layout& layout)
{
    if (layout == m_layout)
        return false;
    m_layout = layout;
    return true;
}

bool WorkPlaneGrid::setColors(const Quantity_Color& minor, const Quantity_Color& major)
{
    if (minor.IsEqual(m_minor) && major.IsEqual(m_major))
        return false;
    m_minor = minor;
    m_major = major;
    return true;
}

void WorkPlaneGrid::Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs, const int mode)
{
    if (mode != 0)
        return;
    const Layout& L = m_layout;
    const int nu = lineCount(L.halfU, L.stepU, L.centerU), nv = lineCount(L.halfV, L.stepV, L.centerV);
    if (nu == 0 || nv == 0)
        return;

    // Lignes à U constant (parallèles à V) puis à V constant ; majeures : indice multiple de majorEvery (axes compris)
    std::vector<std::pair<gp_Pnt, gp_Pnt>> minor, major;
    minor.reserve(static_cast<size_t>(nu + nv));
    // Lignes aux multiples du pas depuis l'origine du plan (positions de l'accrochage), dans le rectangle centré
    auto addLines = [&](double center, double half, double step, double otherCenter, double otherHalf, bool alongV) {
        const long long first = static_cast<long long>(std::ceil((center - half) / step - 1e-9));
        const long long last = static_cast<long long>(std::floor((center + half) / step + 1e-9));
        const double o0 = otherCenter - otherHalf, o1 = otherCenter + otherHalf;
        for (long long i = first; i <= last; ++i)
        {
            const double c = static_cast<double>(i) * step;
            const gp_Pnt a = alongV ? gp_Pnt(c, o0, 0.0) : gp_Pnt(o0, c, 0.0);
            const gp_Pnt b = alongV ? gp_Pnt(c, o1, 0.0) : gp_Pnt(o1, c, 0.0);
            (L.majorEvery > 0 && i % L.majorEvery == 0 ? major : minor).emplace_back(a, b);
        }
    };
    addLines(L.centerU, L.halfU, L.stepU, L.centerV, L.halfV, true);
    addLines(L.centerV, L.halfV, L.stepV, L.centerU, L.halfU, false);

    auto draw = [&](const std::vector<std::pair<gp_Pnt, gp_Pnt>>& lines, const Quantity_Color& color, double width) {
        if (lines.empty())
            return;
        Handle(Graphic3d_ArrayOfSegments) arr = new Graphic3d_ArrayOfSegments(static_cast<int>(lines.size()) * 2);
        for (const auto& [a, b] : lines)
        {
            arr->AddVertex(a);
            arr->AddVertex(b);
        }
        Handle(Graphic3d_Group) g = prs->NewGroup();
        g->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(color, Aspect_TOL_SOLID, width));
        g->AddPrimitiveArray(arr);
    };
    draw(minor, m_minor, 1.0);
    draw(major, m_major, 1.6);
}

} // namespace TSA::Viewer
