#include "CopyTransform.h"

#include "../Coordinate/CoordinateTransformationService.h"

#include <gp_Mat.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <tuple>

namespace TSA::Model
{

namespace
{
using TSA::Coordinate::CoordinateTransformationService;
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

double barRotationImage(const gp_Trsf& trsf, const gp_Pnt& start, const gp_Pnt& end, double rotationDeg, const gp_Pnt& copyStart, const gp_Pnt& copyEnd)
{
    const gp_Ax3 source = CoordinateTransformationService::computeElementLocalFrame(start, end, rotationDeg);
    const gp_Ax3 base = CoordinateTransformationService::computeElementLocalFrame(copyStart, copyEnd, 0.0);
    gp_Vec y = linearImage(gp_Vec(source.YDirection()), trsf);
    if (reflects(trsf)) y.Reverse();   // la symétrie garde z : y' s'oppose à l'image de y pour que le repère reste direct
    const double angle = std::atan2(cleaned(y.Dot(gp_Vec(base.Direction()))), cleaned(y.Dot(gp_Vec(base.YDirection())))) * 180.0 / std::numbers::pi;
    return std::abs(angle - rotationDeg) < 1e-9 ? rotationDeg : angle;
}

std::vector<MemberLoad> memberLoadInFrame(const MemberLoad& load, const gp_Ax3& from, const gp_Ax3& to)
{
    if (load.coordSystem() != LoadCoordSystem::Local) return { load };
    const gp_Vec direction = load.direction() == LoadDirection::LocalX ? gp_Vec(from.XDirection())
                             : load.direction() == LoadDirection::LocalY ? gp_Vec(from.YDirection())
                                                                         : gp_Vec(from.Direction());
    const std::array<std::pair<gp_Vec, LoadDirection>, 3> axes { { { gp_Vec(to.XDirection()), LoadDirection::LocalX },
                                                                   { gp_Vec(to.YDirection()), LoadDirection::LocalY },
                                                                   { gp_Vec(to.Direction()), LoadDirection::LocalZ } } };
    std::vector<MemberLoad> parts;
    for (const auto& [axis, name] : axes)
    {
        const double c = cleaned(direction.Dot(axis));
        if (c == 0.0) continue;
        MemberLoad part = load;
        part.setDirection(name);
        part.setQ1(load.q1() * c);
        part.setQ2(load.q2() * c);
        parts.push_back(part);
    }
    return parts;
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

SurfaceLoad surfaceLoadAt(const SurfaceLoad& load, const SurfacePointMap& map)
{
    SurfaceLoad image = load;
    std::tie(image.u, image.v) = map(load.u, load.v);
    // p(u, v) = value + gradU·u + gradV·v, exprimée en (u', v') : la matrice du passage est orthogonale, son inverse est sa transposée.
    image.gradU = load.gradU * map.uu + load.gradV * map.uv;
    image.gradV = load.gradU * map.vu + load.gradV * map.vv;
    image.value = load.value - image.gradU * map.a - image.gradV * map.b;
    if (load.zone)
    {
        const auto [u0, v0] = map(load.zoneU0, load.zoneV0);
        const auto [u1, v1] = map(load.zoneU1, load.zoneV1);
        image.zoneU0 = std::min(u0, u1), image.zoneU1 = std::max(u0, u1);
        image.zoneV0 = std::min(v0, v1), image.zoneV1 = std::max(v0, v1);
    }
    return image;
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
