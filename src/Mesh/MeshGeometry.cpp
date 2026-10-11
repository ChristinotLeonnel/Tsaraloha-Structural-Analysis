#include "MeshGeometry.h"

#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"

#include <QCryptographicHash>

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <tuple>

namespace TSA::Mesh
{

using namespace TSA::Model;

double P3::norm() const { return std::sqrt(x * x + y * y + z * z); }
P3 P3::normalized() const
{
    const double n = norm();
    return n > 1e-15 ? P3 { x / n, y / n, z / n } : P3 { 0, 0, 0 };
}
double distance(const P3& a, const P3& b) { return (a - b).norm(); }
double distanceToSegment(const P3& p, const P3& a, const P3& b)
{
    const P3 ab = b - a;
    const double l2 = ab.dot(ab);
    if (l2 < 1e-30) return distance(p, a);
    const double t = std::clamp((p - a).dot(ab) / l2, 0.0, 1.0);
    return distance(p, a + ab * t);
}

void PlaneFrame::toLocal(const P3& p, double* u, double* v) const
{
    const P3 d = p - origin;
    *u = d.dot(U);
    *v = d.dot(V);
}

PlaneFrame slabFrame(const std::vector<P3>& c)
{
    PlaneFrame f;
    if (c.size() < 3) return f;
    P3 n;
    for (std::size_t i = 0; i < c.size(); ++i)
    {
        const P3& a = c[i];
        const P3& b = c[(i + 1) % c.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    f.origin = c[0];
    f.N = n.normalized();
    f.U = (c[1] - c[0]).normalized();
    f.V = f.N.cross(f.U).normalized();
    return f;
}

bool pointInPolygon(double u, double v, const std::vector<std::pair<double, double>>& poly)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    {
        const auto& [xi, yi] = poly[i];
        const auto& [xj, yj] = poly[j];
        if (((yi > v) != (yj > v)) && (u < (xj - xi) * (v - yi) / (yj - yi) + xi)) in = !in;
    }
    return in;
}

namespace
{
std::vector<P3> slabCorners(const TSA::Model::Model& model, const Slab& s)
{
    std::vector<P3> c;
    for (int id : s.nodeIds())
        if (const auto* n = model.getNode(id)) c.push_back({ n->x(), n->y(), n->z() });
    return c;
}

double polygonArea2D(const std::vector<std::pair<double, double>>& p)
{
    double a = 0;
    for (std::size_t i = 0; i < p.size(); ++i)
    {
        const auto& [x1, y1] = p[i];
        const auto& [x2, y2] = p[(i + 1) % p.size()];
        a += x1 * y2 - x2 * y1;
    }
    return 0.5 * a;
}

bool segmentsCross(std::pair<double, double> a, std::pair<double, double> b, std::pair<double, double> c, std::pair<double, double> d)
{
    auto orient = [](std::pair<double, double> p, std::pair<double, double> q, std::pair<double, double> r) {
        return (q.first - p.first) * (r.second - p.second) - (q.second - p.second) * (r.first - p.first);
    };
    const double o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
    return ((o1 > 1e-12 && o2 < -1e-12) || (o1 < -1e-12 && o2 > 1e-12)) && ((o3 > 1e-12 && o4 < -1e-12) || (o3 < -1e-12 && o4 > 1e-12));
}

std::string refOf(const char* prefix, int id) { return std::string(prefix) + std::to_string(id); }
} // namespace

std::vector<std::vector<P3>> slabOpenings3D(const TSA::Model::Model& model, const Slab& slab)
{
    std::vector<std::vector<P3>> out;
    const auto corners = slabCorners(model, slab);
    if (corners.size() < 3) return out;
    const PlaneFrame f = slabFrame(corners);
    for (const auto& o : slab.openings())
    {
        std::vector<P3> poly;
        for (const auto& [u, v] : o.polygon) poly.push_back(f.toGlobal(u, v));
        if (poly.size() >= 3) out.push_back(poly);
    }
    return out;
}

static std::string validateOpening(const std::vector<std::pair<double, double>>& outer, const std::vector<TSA::Model::SlabOpening>& existing,
                            const std::vector<std::pair<double, double>>& polygon, int ignoreIndex, const std::string& what)
{
    if (polygon.size() < 3) return "une ouverture a au moins 3 sommets";
    if (std::abs(polygonArea2D(polygon)) < 1e-8) return "ouverture d'aire nulle";
    if (outer.size() < 3) return what + " sans contour valide";
    for (const auto& [u, v] : polygon)
        if (!pointInPolygon(u, v, outer)) return "l'ouverture dépasse le contour " + what;
    for (std::size_t i = 0; i < polygon.size(); ++i)
        for (std::size_t j = 0; j < outer.size(); ++j)
            if (segmentsCross(polygon[i], polygon[(i + 1) % polygon.size()], outer[j], outer[(j + 1) % outer.size()]))
                return "l'ouverture recoupe le contour " + what;
    for (std::size_t k = 0; k < existing.size(); ++k)
    {
        if (int(k) == ignoreIndex) continue;
        const auto& o = existing[k].polygon;
        for (const auto& [u, v] : polygon)
            if (pointInPolygon(u, v, o)) return "l'ouverture chevauche l'ouverture « " + existing[k].name + " »";
        for (const auto& [u, v] : o)
            if (pointInPolygon(u, v, polygon)) return "l'ouverture contient l'ouverture « " + existing[k].name + " »";
        for (std::size_t i = 0; i < polygon.size(); ++i)
            for (std::size_t j = 0; j < o.size(); ++j)
                if (segmentsCross(polygon[i], polygon[(i + 1) % polygon.size()], o[j], o[(j + 1) % o.size()]))
                    return "l'ouverture recoupe l'ouverture « " + existing[k].name + " »";
    }
    return {};
}

std::string validateSlabOpening(const TSA::Model::Model& model, const Slab& slab, const std::vector<std::pair<double, double>>& polygon, int ignoreIndex)
{
    const auto corners = slabCorners(model, slab);
    std::vector<std::pair<double, double>> outer;
    if (corners.size() >= 3)
    {
        const PlaneFrame f = slabFrame(corners);
        for (const auto& c : corners)
        {
            double u = 0, v = 0;
            f.toLocal(c, &u, &v);
            outer.emplace_back(u, v);
        }
    }
    return validateOpening(outer, slab.openings(), polygon, ignoreIndex, "de la dalle");
}

static bool isWallDoor(const std::vector<std::pair<double, double>>& p, double L, double H, double* u1, double* u2, double* top)
{
    if (p.size() != 4) return false;
    double a = 1e30, b = -1e30, lo = 1e30, hi = -1e30;
    for (const auto& [u, v] : p) a = std::min(a, u), b = std::max(b, u), lo = std::min(lo, v), hi = std::max(hi, v);
    for (const auto& [u, v] : p)
        if ((std::abs(u - a) > 1e-9 && std::abs(u - b) > 1e-9) || (std::abs(v - lo) > 1e-9 && std::abs(v - hi) > 1e-9)) return false;   // rectangle aligné
    if (std::abs(lo) > 1e-9 || a <= 1e-9 || b >= L - 1e-9 || hi >= H - 1e-9 || b - a <= 1e-9 || hi <= 1e-9) return false;
    if (u1) *u1 = a;
    if (u2) *u2 = b;
    if (top) *top = hi;
    return true;
}

std::string validateWallOpening(const TSA::Model::Model& model, const TSA::Model::Wall& wall, const std::vector<std::pair<double, double>>& polygon, int ignoreIndex)
{
    const double L = wall.length(model), H = wall.height();
    std::vector<std::pair<double, double>> outer;
    if (L > 1e-9 && H > 1e-9) outer = { { 0.0, 0.0 }, { L, 0.0 }, { L, H }, { 0.0, H } };
    // Porte : rectangle posé sur le pied du voile (encoche du contour) ; elle ne doit recouper ni les autres ouvertures
    // ni une autre porte
    double u1 = 0, u2 = 0, top = 0;
    if (isWallDoor(polygon, L, H, &u1, &u2, &top))
    {
        std::vector<std::pair<double, double>> lifted = polygon;
        for (auto& [u, v] : lifted)
            if (std::abs(v) <= 1e-9) v = 1e-6;   // contrôle de recouvrement sur la partie intérieure
        for (std::size_t k = 0; k < wall.openings().size(); ++k)
        {
            if (int(k) == ignoreIndex) continue;
            double a = 0, b = 0, t = 0;
            if (isWallDoor(wall.openings()[k].polygon, L, H, &a, &b, &t) && a < u2 + 1e-9 && b > u1 - 1e-9)
                return "la porte chevauche ou touche la porte « " + wall.openings()[k].name + " »";
        }
        std::vector<TSA::Model::SlabOpening> others;
        for (std::size_t k = 0; k < wall.openings().size(); ++k)
            if (int(k) != ignoreIndex && !isWallDoor(wall.openings()[k].polygon, L, H, nullptr, nullptr, nullptr)) others.push_back(wall.openings()[k]);
        return validateOpening(outer, others, lifted, -1, "du voile");
    }
    return validateOpening(outer, wall.openings(), polygon, ignoreIndex, "du voile");
}

bool wallSurface(const TSA::Model::Model& model, const TSA::Model::Wall& w, SurfaceGeom* out)
{
    const auto* na = model.getNode(w.startNodeId());
    const auto* nb = model.getNode(w.endNodeId());
    if (!na || !nb) return false;
    const P3 a { na->x(), na->y(), na->z() }, b { nb->x(), nb->y(), nb->z() };
    const P3 dir = P3 { b.x - a.x, b.y - a.y, 0 }.normalized();
    const P3 nrm { -dir.y, dir.x, 0 };
    const P3 off = nrm * w.offset();
    SurfaceGeom sg;
    sg.ref = refOf("W", w.id());
    sg.kind = SurfaceGeom::Kind::Wall;
    sg.id = w.id();
    sg.outer = { a + off, b + off, b + off + P3 { 0, 0, w.height() }, a + off + P3 { 0, 0, w.height() } };
    sg.cornerNodes = { std::abs(w.offset()) < 1e-12 ? w.startNodeId() : 0, std::abs(w.offset()) < 1e-12 ? w.endNodeId() : 0, 0, 0 };
    sg.frame.origin = sg.outer[0];
    sg.frame.U = (sg.outer[1] - sg.outer[0]).normalized();
    sg.frame.V = { 0, 0, 1 };
    sg.frame.N = sg.frame.U.cross(sg.frame.V).normalized();
    sg.area = distance(sg.outer[0], sg.outer[1]) * w.height();
    // Ouvertures (portes, fenêtres) : repère local du voile (u le long du voile, v vers le haut). Une porte posée au
    // pied devient une encoche du contour (elle ne peut pas être un trou : elle touche le bord).
    const double Lw = distance(sg.outer[0], sg.outer[1]);
    std::vector<std::tuple<double, double, double>> doors;
    for (const auto& o : w.openings())
    {
        double u1 = 0, u2 = 0, top = 0;
        if (isWallDoor(o.polygon, Lw, w.height(), &u1, &u2, &top)) doors.emplace_back(u1, u2, top);
    }
    if (!doors.empty())
    {
        std::sort(doors.begin(), doors.end());
        std::vector<P3> outer { sg.frame.toGlobal(0, 0) };
        std::vector<int> nodes { sg.cornerNodes[0] };
        for (const auto& [u1, u2, top] : doors)
        {
            for (const auto& [u, v] : { std::pair { u1, 0.0 }, { u1, top }, { u2, top }, { u2, 0.0 } }) outer.push_back(sg.frame.toGlobal(u, v)), nodes.push_back(0);
            sg.area -= (u2 - u1) * top;
        }
        outer.push_back(sg.outer[1]), outer.push_back(sg.outer[2]), outer.push_back(sg.outer[3]);
        nodes.push_back(sg.cornerNodes[1]), nodes.push_back(0), nodes.push_back(0);
        sg.outer = outer;
        sg.cornerNodes = nodes;
    }
    for (const auto& o : w.openings())
    {
        if (isWallDoor(o.polygon, Lw, w.height(), nullptr, nullptr, nullptr)) continue;
        std::vector<P3> hole;
        for (const auto& [u, v] : o.polygon) hole.push_back(sg.frame.toGlobal(u, v));
        if (hole.size() >= 3)
        {
            sg.holes.push_back(hole);
            sg.area -= std::abs(polygonArea2D(o.polygon));
        }
    }
    sg.formulation = w.formulation();
    sg.material = w.material().name;
    sg.thickness = w.thickness();
    if (sg.area <= 1e-12) return false;
    *out = sg;
    return true;
}

MeshGeometry MeshGeometry::extract(const TSA::Model::Model& model)
{
    MeshGeometry g;
    std::ostringstream h;
    h.precision(10);
    for (const auto& [id, n] : model.nodes()) g.tsaNodes[id] = { n.x(), n.y(), n.z() };

    // Barres
    std::map<int, std::vector<std::string>> sectionsAtNode;   // nœud → sections des barres qui s'y raccordent
    auto addMember = [&](int family, const char* prefix, int id, int a, int b, const std::string& section, const std::string& material) {
        auto ia = g.tsaNodes.find(a), ib = g.tsaNodes.find(b);
        if (ia == g.tsaNodes.end() || ib == g.tsaNodes.end()) return;
        MemberGeom m;
        m.ref = refOf(prefix, id);
        m.family = family;
        m.id = id;
        m.nodeI = a;
        m.nodeJ = b;
        m.a = ia->second;
        m.b = ib->second;
        m.length = distance(m.a, m.b);
        m.section = section;
        m.material = material;
        sectionsAtNode[a].push_back(section);
        sectionsAtNode[b].push_back(section);
        g.members.push_back(m);
    };
    for (const auto& [id, b] : model.beams()) addMember(0, "B", id, b.startNodeId(), b.endNodeId(), b.section().name, b.material().name);
    for (const auto& [id, c] : model.columns()) addMember(1, "C", id, c.startNodeId(), c.endNodeId(), c.section().name, c.material().name);
    for (const auto& [id, t] : model.trussMembers()) addMember(2, "T", id, t.startNodeId(), t.endNodeId(), t.section().name, t.material().name);
    for (const auto& [id, k] : model.cables()) addMember(3, "K", id, k.startNodeId(), k.endNodeId(), k.section().name, k.material().name);

    // Charges ponctuelles sur barres (points caractéristiques)
    const auto ls = model.loadManager().createSnapshot();
    static const char* prefixes[] = { "B", "C", "T", "K" };
    for (const auto& [lid, l] : ls.memberLoads)
    {
        if (l.type() != LoadType::MemberPoint) continue;
        const std::string ref = refOf(prefixes[static_cast<int>(l.targetType())], l.elementId());
        for (auto& m : g.members)
            if (m.ref == ref)
            {
                const double s = std::clamp(l.appliedRange(m.length).first, 0.0, m.length);
                m.pointLoads.push_back(s);
                g.points.push_back({ CharacteristicPoint::Kind::PointLoad, m.a + (m.b - m.a) * (m.length > 0 ? s / m.length : 0), 0, ref });
            }
    }
    for (auto& m : g.members) std::sort(m.pointLoads.begin(), m.pointLoads.end());

    // Appuis, connexions, changements de section
    for (const auto& [id, n] : model.nodes())
    {
        const auto& s = n.support();
        const bool supported = s.tx() != DOFState::Free || s.ty() != DOFState::Free || s.tz() != DOFState::Free || s.rx() != DOFState::Free ||
                               s.ry() != DOFState::Free || s.rz() != DOFState::Free;
        if (supported) g.points.push_back({ CharacteristicPoint::Kind::Support, g.tsaNodes[id], id, {} });
        const auto it = sectionsAtNode.find(id);
        if (it != sectionsAtNode.end() && it->second.size() >= 2)
        {
            g.points.push_back({ CharacteristicPoint::Kind::Connection, g.tsaNodes[id], id, {} });
            if (std::adjacent_find(it->second.begin(), it->second.end(), std::not_equal_to<>()) != it->second.end())
                g.points.push_back({ CharacteristicPoint::Kind::SectionChange, g.tsaNodes[id], id, {} });
        }
    }

    // Dalles (contour + ouvertures)
    for (const auto& [id, s] : model.slabs())
    {
        SurfaceGeom sg;
        sg.ref = refOf("SL", id);
        sg.kind = SurfaceGeom::Kind::Slab;
        sg.id = id;
        sg.outer = slabCorners(model, s);
        if (sg.outer.size() < 3) continue;
        sg.cornerNodes = s.nodeIds();
        sg.formulation = s.formulation();
        sg.material = s.material().name;
        sg.thickness = s.thickness();
        sg.frame = slabFrame(sg.outer);
        sg.holes = slabOpenings3D(model, s);
        std::vector<std::pair<double, double>> loc;
        for (const auto& p : sg.outer)
        {
            double u = 0, v = 0;
            sg.frame.toLocal(p, &u, &v);
            loc.emplace_back(u, v);
        }
        sg.area = std::abs(polygonArea2D(loc));
        for (const auto& o : s.openings()) sg.area -= std::abs(polygonArea2D(o.polygon));
        g.surfaces.push_back(sg);
    }
    // Voiles : contour médian (portes en encoche, fenêtres en trous), commun au maillage et à la vue 3D
    for (const auto& [id, w] : model.walls())
    {
        SurfaceGeom sg;
        if (wallSurface(model, w, &sg)) g.surfaces.push_back(sg);
    }
    // Semelles : volumes parallélépipédiques sous le nœud
    for (const auto& [id, f] : model.foundations())
    {
        auto in = g.tsaNodes.find(f.nodeId());
        if (in == g.tsaNodes.end()) continue;
        VolumeGeom v;
        v.ref = refOf("F", id);
        v.id = id;
        v.nodeId = f.nodeId();
        v.nodePos = in->second;
        v.min = { in->second.x - f.widthA() / 2, in->second.y - f.lengthB() / 2, in->second.z - f.heightH() };
        v.max = { in->second.x + f.widthA() / 2, in->second.y + f.lengthB() / 2, in->second.z };
        v.material = f.material().name;
        if (f.widthA() > 0 && f.lengthB() > 0 && f.heightH() > 0) g.volumes.push_back(v);
    }

    // Empreinte : tout ce qui influence le maillage (géométrie, connexions, appuis, charges ponctuelles)
    auto put = [&](const P3& p) { h << p.x << ',' << p.y << ',' << p.z << ';'; };
    for (const auto& m : g.members)
    {
        h << m.ref << ':' << m.nodeI << '-' << m.nodeJ << ':' << m.section << ':';
        put(m.a);
        put(m.b);
        for (double s : m.pointLoads) h << s << ',';
        h << '\n';
    }
    for (const auto& s : g.surfaces)
    {
        h << s.ref << ':';
        for (const auto& p : s.outer) put(p);
        for (const auto& hole : s.holes)
        {
            h << '|';
            for (const auto& p : hole) put(p);
        }
        h << '\n';
    }
    for (const auto& v : g.volumes)
    {
        h << v.ref << ':';
        put(v.min);
        put(v.max);
        h << '\n';
    }
    for (const auto& p : g.points)
    {
        h << int(p.kind) << ':' << p.nodeId << ':' << p.target << ':';
        put(p.p);
        h << '\n';
    }
    g.hash = QCryptographicHash::hash(QByteArray::fromStdString(h.str()), QCryptographicHash::Sha256).toHex().toStdString();
    return g;
}

const MemberGeom* MeshGeometry::member(const std::string& ref) const
{
    for (const auto& m : members)
        if (m.ref == ref) return &m;
    return nullptr;
}
const SurfaceGeom* MeshGeometry::surface(const std::string& ref) const
{
    for (const auto& s : surfaces)
        if (s.ref == ref) return &s;
    return nullptr;
}
const VolumeGeom* MeshGeometry::volume(const std::string& ref) const
{
    for (const auto& v : volumes)
        if (v.ref == ref) return &v;
    return nullptr;
}
std::vector<std::string> MeshGeometry::refs() const
{
    std::vector<std::string> r;
    for (const auto& m : members) r.push_back(m.ref);
    for (const auto& s : surfaces) r.push_back(s.ref);
    for (const auto& v : volumes) r.push_back(v.ref);
    return r;
}

} // namespace TSA::Mesh
