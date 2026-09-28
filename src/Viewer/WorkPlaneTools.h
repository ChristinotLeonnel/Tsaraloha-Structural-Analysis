#pragma once

// Outils purs (sans état) pour le plan de travail :
//  - construction du plan suivant X / Y / Z
//  - collage du plan sur une ligne de grille
//  - orientation de la vue (projection) + flèche indiquant la face regardée
// Header-only : aucun ajout à CMakeLists.txt nécessaire.

#include <AIS_Shape.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Compound.hxx>
#include <V3d_View.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace TSA::Viewer::WorkPlaneTools
{

enum class Axis
{
    X,
    Y,
    Z
};

/// "Plan de travail suivant X / Y / Z" : plan PERPENDICULAIRE à l'axe, à la cote 'offset'.
///  - Z : plan XY (vue de dessus)   normale +Z, X horizontal, Y vertical
///  - Y : plan XZ (vue de face)     normale -Y, X horizontal, Z vertical
///  - X : plan YZ (vue de droite)   normale +X, Y horizontal, Z vertical
inline gp_Ax3 fromAxis(Axis axis, double offset)
{
    switch (axis)
    {
    case Axis::X: return gp_Ax3(gp_Pnt(offset, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
    case Axis::Y: return gp_Ax3(gp_Pnt(0, offset, 0), gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
    case Axis::Z:
    default:      return gp_Ax3(gp_Pnt(0, 0, offset), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    }
}

/// Collage sur une ligne de grille [a,b] (à appeler quand le snap renvoie une ligne de grille).
/// Une droite appartient à une infinité de plans : seuls les plans "de grille" (normale = axe
/// global perpendiculaire à la droite) sont candidats ; on prend celui qui fait le plus face à
/// la caméra. 'cycle' (touche Tab) bascule sur le candidat suivant.
/// 'eyeDir' = direction cible -> oeil (même convention que V3d_View::Proj).
/// L'axe X du plan est aligné sur la ligne. Retourne nullopt si la ligne est oblique.
inline std::optional<gp_Ax3> fromGridLine(const gp_Pnt& a, const gp_Pnt& b, const gp_Dir& eyeDir, int cycle = 0)
{
    if (a.Distance(b) < 1e-9)
        return std::nullopt;

    const gp_Dir d(gp_Vec(a, b));

    std::vector<gp_Dir> cands;
    const gp_Dir axes[3] = {gp_Dir(1, 0, 0), gp_Dir(0, 1, 0), gp_Dir(0, 0, 1)};
    for (const gp_Dir& ax : axes)
        if (std::abs(ax.Dot(d)) < 1e-6)
            cands.push_back(ax);
    if (cands.empty())
        return std::nullopt;

    std::stable_sort(cands.begin(), cands.end(),
                     [&](const gp_Dir& p, const gp_Dir& q) { return std::abs(p.Dot(eyeDir)) > std::abs(q.Dot(eyeDir)); });

    gp_Dir n = cands[static_cast<size_t>(cycle < 0 ? 0 : cycle) % cands.size()];
    if (n.Dot(eyeDir) < 0.0)
        n.Reverse(); // normale orientée vers la caméra
    return gp_Ax3(a, n, d);
}

/// Direction cible -> oeil pour regarder le plan (flip : face opposée).
inline gp_Dir projectionDirection(const gp_Ax3& plane, bool flip = false)
{
    gp_Dir n = plane.Direction();
    if (flip)
        n.Reverse();
    return n;
}

/// Oriente la vue perpendiculairement au plan (équivalent de viewNormalToWorkPlane, mais
/// avec choix de la face). Le haut de l'écran = axe Y du plan.
inline void applyProjection(const Handle(V3d_View)& view, const gp_Ax3& plane, bool flip = false, bool fit = true)
{
    if (view.IsNull())
        return;
    const gp_Dir n = projectionDirection(plane, flip);
    const gp_Dir up = plane.YDirection();
    view->SetUp(up.X(), up.Y(), up.Z());
    view->SetProj(n.X(), n.Y(), n.Z());
    if (fit)
        view->FitAll(0.1, false);
    view->Update();
}

/// Flèche 3D partant de l'origine du plan, orientée vers la face depuis laquelle on regarde la
/// projection. Longueur en unités monde. À protéger de l'isolation (IsolationManager::protect).
inline Handle(AIS_Shape) makeProjectionArrow(const gp_Ax3& plane, double length, bool flip = false)
{
    const gp_Dir n = projectionDirection(plane, flip);
    const double head = length * 0.28;
    const double shaftLen = std::max(length - head, 1e-6);

    const gp_Ax2 axShaft(plane.Location(), n);
    const gp_Ax2 axHead(plane.Location().Translated(gp_Vec(n) * shaftLen), n);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    bb.Add(comp, BRepPrimAPI_MakeCylinder(axShaft, length * 0.03, shaftLen).Shape());
    bb.Add(comp, BRepPrimAPI_MakeCone(axHead, length * 0.08, 0.0, head).Shape());

    Handle(AIS_Shape) arrow = new AIS_Shape(comp);
    arrow->SetColor(Quantity_Color(1.0, 0.55, 0.0, Quantity_TOC_RGB));
    arrow->SetZLayer(Graphic3d_ZLayerId_Top);
    return arrow;
}

} // namespace TSA::Viewer::WorkPlaneTools
