#include "IfcGeometryMapper.h"

#include "../../Model/Model.h"

#include <cmath>

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;
using TSA::Model::ElementKind;

namespace
{
Vec3 sub(const Vec3& a, const Vec3& b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
Vec3 add(const Vec3& a, const Vec3& b) { return { a[0] + b[0], a[1] + b[1], a[2] + b[2] }; }
Vec3 mul(const Vec3& a, double k) { return { a[0] * k, a[1] * k, a[2] * k }; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b) { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; }
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a)
{
    const double n = norm(a);
    return n > 0.0 ? mul(a, 1.0 / n) : Vec3 { 0, 0, 1 };
}

bool nodePos(const TSA::Model::Model& m, int id, Vec3& p)
{
    const auto* n = m.getNode(id);
    if (!n) return false;
    p = { n->x(), n->y(), n->z() };
    return true;
}
} // namespace

int IfcGeometryMapper::point(const Vec3& p)
{
    return m_ctx.w.add("IFCCARTESIANPOINT", W::reals({ p[0], p[1], p[2] }));
}

int IfcGeometryMapper::point2(double x, double y)
{
    return m_ctx.w.add("IFCCARTESIANPOINT", W::reals({ x, y }));
}

int IfcGeometryMapper::direction(const Vec3& d)
{
    const std::string args = W::reals({ d[0], d[1], d[2] });
    if (auto it = m_dirs.find(args); it != m_dirs.end()) return it->second;
    return m_dirs[args] = m_ctx.w.add("IFCDIRECTION", args);
}

int IfcGeometryMapper::placement3(const Vec3& origin, const Vec3& axis, const Vec3& refDir)
{
    return m_ctx.w.add("IFCAXIS2PLACEMENT3D", W::ref(point(origin)) + "," + W::ref(direction(axis)) + "," + W::ref(direction(refDir)));
}

int IfcGeometryMapper::localPlacement(int relativeTo, int axisPlacement)
{
    return m_ctx.w.add("IFCLOCALPLACEMENT", (relativeTo ? W::ref(relativeTo) : "$") + "," + W::ref(axisPlacement));
}

void IfcGeometryMapper::barFrame(const Vec3& a, const Vec3& b, double rotationDeg, Vec3& x, Vec3& y, Vec3& z)
{
    // Même construction que TSA::Geometry::BeamGeometry::createBeamShape
    z = unit(sub(b, a));
    Vec3 x0, y0;
    if (std::abs(z[2]) > 0.999)
    {
        y0 = unit(cross(z, { 1, 0, 0 }));
        x0 = unit(cross(y0, z));
    }
    else
    {
        x0 = unit(cross(z, { 0, 0, 1 }));
        y0 = unit(cross(z, x0));
    }
    const double r = rotationDeg * 3.14159265358979323846 / 180.0;
    x = add(mul(x0, std::cos(r)), mul(y0, std::sin(r)));
    y = add(mul(x0, -std::sin(r)), mul(y0, std::cos(r)));
}

int IfcGeometryMapper::extrude(int profile, int placement, const Vec3& dir, double depth)
{
    return m_ctx.w.add("IFCEXTRUDEDAREASOLID", W::ref(profile) + "," + W::ref(placement) + "," + W::ref(direction(dir)) + "," + W::real(depth));
}

int IfcGeometryMapper::barSolid(const TSA::Model::Section& s, const Vec3& a, const Vec3& b, double rotationDeg, int ecc)
{
    const double len = norm(sub(b, a));
    if (len < 1e-6) return 0;
    Vec3 x, y, z;
    barFrame(a, b, rotationDeg, x, y, z);
    Vec3 o = a;
    using E = TSA::Model::BarEccentricity;
    switch (static_cast<E>(ecc))
    {
    case E::TopFlange: o = add(o, mul(y, -s.height / 2.0)); break;
    case E::BottomFlange: o = add(o, mul(y, s.height / 2.0)); break;
    case E::LeftFlange: o = add(o, mul(x, s.width / 2.0)); break;
    case E::RightFlange: o = add(o, mul(x, -s.width / 2.0)); break;
    default: break;
    }
    // Profil dans le plan (x, y) du repère ; extrusion selon +z local
    return extrude(m_mapper.profile(s), placement3(o, z, x), { 0, 0, 1 }, len);
}

int IfcGeometryMapper::slabSolid(const std::vector<int>& nodeIds, double thickness)
{
    std::vector<Vec3> pts;
    for (int id : nodeIds)
    {
        Vec3 p;
        if (nodePos(m_ctx.model, id, p)) pts.push_back(p);
    }
    if (pts.size() < 3 || thickness <= 0.0) return 0;
    // Normale de Newell, repère local au premier sommet ; extrusion vers le bas (comme SlabGeometry)
    Vec3 n { 0, 0, 0 };
    for (std::size_t i = 0; i < pts.size(); ++i)
    {
        const Vec3& p = pts[i];
        const Vec3& q = pts[(i + 1) % pts.size()];
        n = add(n, { (p[1] - q[1]) * (p[2] + q[2]), (p[2] - q[2]) * (p[0] + q[0]), (p[0] - q[0]) * (p[1] + q[1]) });
    }
    if (norm(n) < 1e-12) return 0;
    n = unit(n);
    if (n[2] < 0) n = mul(n, -1.0);
    Vec3 x = unit(sub(pts[1], pts[0]));
    x = unit(sub(x, mul(n, dot(x, n))));
    const Vec3 y = cross(n, x);

    std::vector<int> ids;
    for (const auto& p : pts)
    {
        const Vec3 d = sub(p, pts[0]);
        ids.push_back(point2(dot(d, x), dot(d, y)));
    }
    ids.push_back(ids.front());   // polyligne fermée
    const int poly = m_ctx.w.add("IFCPOLYLINE", W::refs(ids));
    const int prof = m_ctx.w.add("IFCARBITRARYCLOSEDPROFILEDEF", ".AREA.,$," + W::ref(poly));
    return extrude(prof, placement3(pts[0], n, x), { 0, 0, -1 }, thickness);
}

int IfcGeometryMapper::wallSolid(const Vec3& a, const Vec3& b, double height, double thickness, double offset)
{
    const Vec3 d { b[0] - a[0], b[1] - a[1], 0.0 };
    const double len = norm(d);
    if (len < 1e-4 || height <= 0.0 || thickness <= 0.0) return 0;
    const Vec3 x = unit(d);
    // Profil rectangle (longueur × épaisseur) centré à (L/2, offset) ; normale du viewport (-dy, dx)
    const int pos = m_ctx.w.add("IFCAXIS2PLACEMENT2D", W::ref(point2(len / 2.0, offset)) + ",$");
    const int prof = m_ctx.w.add("IFCRECTANGLEPROFILEDEF", ".AREA.,$," + W::ref(pos) + "," + W::real(len) + "," + W::real(thickness));
    return extrude(prof, placement3(a, { 0, 0, 1 }, x), { 0, 0, 1 }, height);
}

int IfcGeometryMapper::boxSolid(const Vec3& top, double a, double b, double h)
{
    if (a <= 0 || b <= 0 || h <= 0) return 0;
    const int prof = m_ctx.w.add("IFCRECTANGLEPROFILEDEF", ".AREA.,$,$," + W::real(a) + "," + W::real(b));
    return extrude(prof, placement3(top, { 0, 0, 1 }, { 1, 0, 0 }), { 0, 0, -1 }, h);
}

int IfcGeometryMapper::cylinderSolid(const Vec3& top, double d, double h)
{
    if (d <= 0 || h <= 0) return 0;
    const int prof = m_ctx.w.add("IFCCIRCLEPROFILEDEF", ".AREA.,$,$," + W::real(d / 2.0));
    return extrude(prof, placement3(top, { 0, 0, 1 }, { 1, 0, 0 }), { 0, 0, -1 }, h);
}

int IfcGeometryMapper::productShape(const PhysicalElement& e)
{
    const auto& m = m_ctx.model;
    std::vector<int> solids;
    std::vector<Vec3> axis;   // polyligne d'axe des éléments linéaires
    auto linear = [&](const TSA::Model::Section& s, int na, int nb, double rot, int ecc) {
        Vec3 a, b;
        if (!nodePos(m, na, a) || !nodePos(m, nb, b)) return;
        if (int id = barSolid(s, a, b, rot, ecc)) solids.push_back(id);
        if (axis.empty()) axis.push_back(a);
        axis.push_back(b);
    };

    for (const auto& r : e.analytical)
    {
        switch (r.kind)
        {
        case ElementKind::Beam:
            if (const auto* b = m.getBeam(r.id)) linear(b->section(), b->startNodeId(), b->endNodeId(), b->rotation(), static_cast<int>(b->eccentricity()));
            break;
        case ElementKind::Column:
            if (const auto* c = m.getColumn(r.id)) linear(c->section(), c->startNodeId(), c->endNodeId(), c->rotation(), 0);
            break;
        case ElementKind::TrussMember:
            if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end())
                linear(it->second.section(), it->second.startNodeId(), it->second.endNodeId(), 0.0, 0);
            break;
        case ElementKind::Cable:
            if (auto it = m.cables().find(r.id); it != m.cables().end())
                linear(TSA::Model::Section::circular(it->second.diameter(), it->second.section().name), it->second.startNodeId(),
                       it->second.endNodeId(), 0.0, 0);
            break;
        case ElementKind::Slab:
            if (auto it = m.slabs().find(r.id); it != m.slabs().end())
                if (int id = slabSolid(it->second.nodeIds(), it->second.thickness())) solids.push_back(id);
            break;
        case ElementKind::Wall:
            if (auto it = m.walls().find(r.id); it != m.walls().end())
            {
                Vec3 a, b;
                if (nodePos(m, it->second.startNodeId(), a) && nodePos(m, it->second.endNodeId(), b))
                    if (int id = wallSolid(a, b, it->second.height(), it->second.thickness(), it->second.offset())) solids.push_back(id);
            }
            break;
        case ElementKind::Foundation:
            if (auto it = m.foundations().find(r.id); it != m.foundations().end())
            {
                const auto& f = it->second;
                Vec3 p;
                if (!nodePos(m, f.nodeId(), p)) break;
                const int id = f.foundationType() == TSA::Model::FoundationType::Pile
                    ? cylinderSolid(p, f.widthA(), f.heightH())
                    : boxSolid(p, f.widthA(), f.lengthB(), f.heightH());
                if (id) solids.push_back(id);
            }
            break;
        default:
            break;
        }
    }
    if (solids.empty()) return 0;

    // 'Body' en premier : représentation principale lue par les visionneuses
    std::vector<int> reps { m_ctx.w.add("IFCSHAPEREPRESENTATION", W::ref(m_ctx.bodyContext) + ",'Body','SweptSolid'," + W::refs(solids)) };
    if (axis.size() >= 2)
    {
        std::vector<int> pts;
        for (const auto& p : axis) pts.push_back(point(p));
        const int poly = m_ctx.w.add("IFCPOLYLINE", W::refs(pts));
        reps.push_back(m_ctx.w.add("IFCSHAPEREPRESENTATION", W::ref(m_ctx.axisContext) + ",'Axis','Curve3D'," + W::refs({ poly })));
    }
    return m_ctx.w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs(reps));
}

} // namespace TSA::BIM::Ifc
