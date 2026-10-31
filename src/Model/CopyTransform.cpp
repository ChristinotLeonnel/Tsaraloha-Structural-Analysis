#include "CopyTransform.h"

#include <gp_Mat.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace TSA::Model
{

namespace
{
constexpr double kTol = 1e-12;

gp_Vec linearImage(const gp_Vec& v, const gp_Trsf& trsf)
{
    gp_XYZ c = v.XYZ();
    c.Multiply(trsf.VectorialPart());
    return gp_Vec(c);
}

/// Bruit d'arrondi retiré : une composante nulle reste nulle, une composante unitaire reste exactement ±1.
double cleaned(double c)
{
    if (std::abs(c) < kTol) return 0.0;
    if (std::abs(std::abs(c) - 1.0) < kTol) return c > 0.0 ? 1.0 : -1.0;
    return c;
}

gp_Vec cleanedVector(const gp_Vec& v, double magnitude)
{
    auto clean = [magnitude](double c) { return std::abs(c) < kTol * magnitude ? 0.0 : c; };
    return gp_Vec(clean(v.X()), clean(v.Y()), clean(v.Z()));
}

gp_XYZ toXYZ(const TSA::Mesh::P3& p)
{
    return gp_XYZ(p.x, p.y, p.z);
}
} // namespace

bool isTranslation(const gp_Trsf& trsf)
{
    const gp_Vec x = linearImage(gp_Vec(1.0, 0.0, 0.0), trsf), y = linearImage(gp_Vec(0.0, 1.0, 0.0), trsf);
    return std::abs(x.X() - 1.0) < kTol && std::abs(y.Y() - 1.0) < kTol && !reflects(trsf);
}

bool reflects(const gp_Trsf& trsf)
{
    return trsf.VectorialPart().Determinant() < 0.0;
}

bool keepsVertical(const gp_Trsf& trsf)
{
    return std::abs(linearImage(gp_Vec(0.0, 0.0, 1.0), trsf).Z() - 1.0) < kTol;
}

NodalLoad nodalLoadImage(const NodalLoad& load, const gp_Trsf& trsf)
{
    NodalLoad image = load;
    if (load.coordSystem() == LoadCoordSystem::Local) return image;   // repère déclaré sans effet sur les moteurs : valeurs gardées
    const gp_Vec f = cleanedVector(linearImage(gp_Vec(load.fx(), load.fy(), load.fz()), trsf), load.forceMagnitude());
    // Un moment est un vecteur axial : une symétrie en inverse le sens en plus de le retourner.
    const gp_Vec m = cleanedVector(linearImage(gp_Vec(load.mx(), load.my(), load.mz()), trsf) * (reflects(trsf) ? -1.0 : 1.0), load.momentMagnitude());
    image.setFx(f.X()), image.setFy(f.Y()), image.setFz(f.Z());
    image.setMx(m.X()), image.setMy(m.Y()), image.setMz(m.Z());
    return image;
}

std::vector<MemberLoad> memberLoadImage(const MemberLoad& load, const gp_Trsf& trsf, const gp_Ax3& source, const gp_Ax3& copy)
{
    const bool local = load.coordSystem() == LoadCoordSystem::Local;
    std::optional<gp_Vec> direction;
    switch (load.direction())
    {
    case LoadDirection::GlobalX: if (!local) direction = gp_Vec(1.0, 0.0, 0.0); break;
    case LoadDirection::GlobalY: if (!local) direction = gp_Vec(0.0, 1.0, 0.0); break;
    case LoadDirection::GlobalZ: if (!local) direction = gp_Vec(0.0, 0.0, 1.0); break;
    case LoadDirection::LocalX: if (local) direction = gp_Vec(source.XDirection()); break;
    case LoadDirection::LocalY: if (local) direction = gp_Vec(source.YDirection()); break;
    case LoadDirection::LocalZ: if (local) direction = gp_Vec(source.Direction()); break;
    default: break;
    }
    if (!direction) return { load };   // gravité (ou axe sans rapport avec le repère, lu comme la gravité par les moteurs)

    gp_Vec image = linearImage(*direction, trsf);
    if (load.type() == LoadType::MemberMoment && reflects(trsf)) image.Reverse();   // vecteur axial

    const std::array<gp_Vec, 3> axes = local ? std::array<gp_Vec, 3> { gp_Vec(copy.XDirection()), gp_Vec(copy.YDirection()), gp_Vec(copy.Direction()) }
                                             : std::array<gp_Vec, 3> { gp_Vec(1.0, 0.0, 0.0), gp_Vec(0.0, 1.0, 0.0), gp_Vec(0.0, 0.0, 1.0) };
    const std::array<LoadDirection, 3> directions = local ? std::array { LoadDirection::LocalX, LoadDirection::LocalY, LoadDirection::LocalZ }
                                                          : std::array { LoadDirection::GlobalX, LoadDirection::GlobalY, LoadDirection::GlobalZ };
    std::vector<MemberLoad> images;
    for (std::size_t k = 0; k < 3; ++k)
    {
        const double c = cleaned(image.Dot(axes[k]));
        if (c == 0.0) continue;
        MemberLoad part = load;
        part.setDirection(directions[k]);
        part.setQ1(load.q1() * c);
        part.setQ2(load.q2() * c);
        images.push_back(part);
    }
    return images;
}

SurfacePointMap surfacePointMap(const TSA::Mesh::PlaneFrame& source, const TSA::Mesh::PlaneFrame& copy, const gp_Trsf& trsf)
{
    gp_XYZ origin = toXYZ(source.origin);
    trsf.Transforms(origin);
    const gp_Vec d(origin - toXYZ(copy.origin));
    const gp_Vec u = linearImage(gp_Vec(toXYZ(source.U)), trsf), v = linearImage(gp_Vec(toXYZ(source.V)), trsf);
    const gp_Vec cu(toXYZ(copy.U)), cv(toXYZ(copy.V));
    SurfacePointMap map;
    map.a = d.Dot(cu);
    map.b = d.Dot(cv);
    map.uu = cleaned(u.Dot(cu));
    map.uv = cleaned(v.Dot(cu));
    map.vu = cleaned(u.Dot(cv));
    map.vv = cleaned(v.Dot(cv));
    return map;
}

std::vector<SurfaceLoad> surfaceLoadImage(const SurfaceLoad& load, const gp_Trsf& trsf, const SurfacePointMap& map, const gp_Dir& sourceNormal,
                                          bool normalReversed)
{
    SurfaceLoad base = load;
    std::tie(base.u, base.v) = map(load.u, load.v);
    // p(u, v) = value + gradU·u + gradV·v, exprimée en (u', v') : la matrice du passage est orthogonale, son inverse est sa transposée.
    base.gradU = load.gradU * map.uu + load.gradV * map.uv;
    base.gradV = load.gradU * map.vu + load.gradV * map.vv;
    base.value = load.value - base.gradU * map.a - base.gradV * map.b;
    if (load.zone)
    {
        const auto [u0, v0] = map(load.zoneU0, load.zoneV0);
        const auto [u1, v1] = map(load.zoneU1, load.zoneV1);
        base.zoneU0 = std::min(u0, u1), base.zoneU1 = std::max(u0, u1);
        base.zoneV0 = std::min(v0, v1), base.zoneV1 = std::max(v0, v1);
    }
    auto scaled = [](SurfaceLoad l, double c) {
        l.value *= c, l.gradU *= c, l.gradV *= c;
        return l;
    };

    std::optional<gp_Vec> direction;
    switch (load.direction)
    {
    case SurfaceLoadDirection::GlobalX: direction = gp_Vec(1.0, 0.0, 0.0); break;
    case SurfaceLoadDirection::GlobalY: direction = gp_Vec(0.0, 1.0, 0.0); break;
    case SurfaceLoadDirection::GlobalZ: direction = gp_Vec(0.0, 0.0, 1.0); break;
    case SurfaceLoadDirection::LocalNormal: return { normalReversed ? scaled(base, -1.0) : base };
    default: return { base };   // la gravité reste la gravité
    }

    const gp_Vec image = linearImage(*direction, trsf);
    const std::array<std::pair<gp_Vec, SurfaceLoadDirection>, 3> axes { { { gp_Vec(1.0, 0.0, 0.0), SurfaceLoadDirection::GlobalX },
                                                                          { gp_Vec(0.0, 1.0, 0.0), SurfaceLoadDirection::GlobalY },
                                                                          { gp_Vec(0.0, 0.0, 1.0), SurfaceLoadDirection::GlobalZ } } };
    std::vector<SurfaceLoad> images;
    int components = 0;
    for (const auto& [axis, name] : axes) components += cleaned(image.Dot(axis)) != 0.0 ? 1 : 0;
    if (components > 1 && base.projected)
    {
        // Intensité rapportée à la surface projetée : ramenée à la surface réelle, seule définition valable composante par composante.
        base = scaled(base, std::abs(gp_Vec(sourceNormal).Dot(*direction)));
        base.projected = false;
    }
    for (const auto& [axis, name] : axes)
    {
        const double c = cleaned(image.Dot(axis));
        if (c == 0.0) continue;
        SurfaceLoad part = scaled(base, c);
        part.direction = name;
        images.push_back(part);
    }
    return images;
}

std::vector<int> mirroredContour(const std::vector<int>& nodeIds)
{
    std::vector<int> contour(nodeIds.rbegin(), nodeIds.rend());
    if (contour.size() >= 2) std::rotate(contour.begin(), contour.end() - 2, contour.end());
    return contour;
}

int mirroredEdge(int edge, int nodeCount)
{
    return edge <= 0 ? edge : nodeCount - edge;
}

} // namespace TSA::Model
