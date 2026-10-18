#include "GmshMeshEngine.h"

#include "../MeshSizing.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

#ifdef TSA_HAVE_GMSH
#include <gmsh.h_cwrap>
#ifdef _WIN32
#include <windows.h>
#endif
#endif

namespace TSA::Mesh::GmshMeshEngine
{

bool fromGmshType(int t, FeType* type)
{
    switch (t)
    {
    case 1: *type = FeType::Line2; return true;
    case 8: *type = FeType::Line3; return true;
    case 2: *type = FeType::Tri3; return true;
    case 9: *type = FeType::Tri6; return true;
    case 3: *type = FeType::Quad4; return true;
    case 16: *type = FeType::Quad8; return true;
    case 4: *type = FeType::Tet4; return true;
    case 11: *type = FeType::Tet10; return true;
    case 5: *type = FeType::Hex8; return true;
    case 17: *type = FeType::Hex20; return true;
    default: return false;
    }
}

int toGmshType(FeType t)
{
    switch (t)
    {
    case FeType::Line2: return 1;
    case FeType::Line3: return 8;
    case FeType::Tri3: return 2;
    case FeType::Tri6: return 9;
    case FeType::Quad4: return 3;
    case FeType::Quad8: return 16;
    case FeType::Tet4: return 4;
    case FeType::Tet10: return 11;
    case FeType::Hex8: return 5;
    case FeType::Hex20: return 17;
    }
    return 0;
}

#ifndef TSA_HAVE_GMSH

MesherCapabilities capabilities()
{
    MesherCapabilities c;
    c.available = false;
    c.unavailableReason = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                      "TSA a été compilé sans le SDK Gmsh (option TSA_WITH_GMSH ou SDK absent) : aucun maillage ne peut être généré")
                              .toStdString();
    return c;
}
GenerationResult generate(const MeshGeometry&, const MeshSettingsSet&, const GenerationOptions&)
{
    GenerationResult r;
    r.record.mesher = "Gmsh";
    r.record.errors.push_back(capabilities().unavailableReason);
    return r;
}
bool exportMesh(const FeMesh&, const std::string&, std::string* error)
{
    if (error) *error = capabilities().unavailableReason;
    return false;
}
bool importMesh(const std::string&, FeMesh*, std::vector<std::string>*, std::string* error)
{
    if (error) *error = capabilities().unavailableReason;
    return false;
}

#else

namespace
{
/// DLL chargée en différé : présence vérifiée avant le premier appel (sinon exception système au premier appel).
bool dllLoaded(std::string* why)
{
    static std::once_flag once;
    static bool ok = false;
    static std::string reason;
    std::call_once(once, [] {
#ifdef _WIN32
        const QString name = QStringLiteral(TSA_GMSH_DLL_NAME);
        HMODULE h = nullptr;
        if (QCoreApplication::instance())
        {
            const QString local = QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).filePath(name));
            h = LoadLibraryExW(reinterpret_cast<LPCWSTR>(local.utf16()), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        if (!h) h = LoadLibraryW(reinterpret_cast<LPCWSTR>(name.utf16()));
        ok = h != nullptr;
        if (!ok)
            reason = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "bibliothèque Gmsh « %1 » introuvable à côté de l'application : maillage indisponible")
                         .arg(name)
                         .toStdString();
#else
        ok = true;
#endif
    });
    if (why) *why = reason;
    return ok;
}

std::mutex& apiMutex()
{
    static std::mutex m;
    return m;
}

/// Session Gmsh : état global initialisé et fermé à chaque opération, sous verrou.
class Session
{
public:
    Session()
        : m_lock(apiMutex())
    {
        gmsh::initialize(0, nullptr, false, false);
        gmsh::option::setNumber("General.Terminal", 0);
        gmsh::logger::start();
    }
    ~Session()
    {
        try
        {
            gmsh::logger::stop();
        }
        catch (...)
        {
        }
        try
        {
            gmsh::finalize();
        }
        catch (...)
        {
        }
    }
    std::vector<std::string> log() const
    {
        std::vector<std::string> l;
        try
        {
            gmsh::logger::get(l);
        }
        catch (...)
        {
        }
        return l;
    }

private:
    std::lock_guard<std::mutex> m_lock;
};

std::string versionString()
{
    std::string v;
    try
    {
        Session s;
        gmsh::option::getString("General.Version", v);
    }
    catch (...)
    {
    }
    return v;
}

std::string fmt(double v)
{
    std::ostringstream o;
    o.precision(12);
    o << v;
    return o.str();
}

using DimTags = std::vector<std::pair<int, int>>;

/// Construction et paramétrage du modèle Gmsh depuis la géométrie TSA.
class Builder
{
public:
    Builder(const MeshGeometry& g, const MeshSettingsSet& s, const GenerationOptions& o, GenerationRecord& rec)
        : geo(g)
        , set(s)
        , opts(o)
        , ev(s, g)
        , record(rec)
        , tol(std::max(1e-9, s.global.geometricTolerance))
    {
        for (const auto& [id, p] : g.tsaNodes) tsaByKey[key(p)] = id;
    }

    const MeshGeometry& geo;
    const MeshSettingsSet& set;
    const GenerationOptions& opts;
    SizeEvaluator ev;
    GenerationRecord& record;
    double tol;

    using Key = std::tuple<long long, long long, long long>;
    std::map<Key, int> tsaByKey;              // position → nœud TSA
    std::map<int, int> tsaPoint;              // nœud TSA → point Gmsh
    std::map<Key, int> coordPoint;            // points sans nœud TSA
    std::map<int, P3> pointPos;
    std::map<std::pair<int, int>, int> lineByPoints;   // (min, max) → courbe
    std::map<int, std::pair<int, int>> lineEnds;       // courbe → (début, fin)
    std::map<int, std::vector<std::string>> curveUsers;

    struct MemberSeg
    {
        int line;
        bool forward;
        double s0, s1;
    };
    std::map<std::string, std::vector<MemberSeg>> memberSegs;
    std::set<int> memberLines;
    std::map<int, int> fixedCount;            // courbe → divisions imposées par une barre
    std::map<std::string, int> surfaceTag;
    std::map<int, std::string> surfaceRef;
    std::map<int, std::vector<int>> surfaceLines;    // surface → courbes de contour (extérieur puis ouvertures)
    std::map<int, std::vector<int>> holeLines;
    std::set<int> embeddedInSurface;          // surfaces contenant des entités plongées
    std::map<std::string, std::vector<int>> volumeTags;
    std::map<std::string, std::vector<int>> volumeFaces;   // faces extérieures
    std::map<std::string, std::vector<int>> volumeLines;
    std::map<std::string, int> volumeCounts[3];
    std::vector<std::pair<int, std::string>> loadPoints;
    std::map<std::string, std::vector<int>> entityCurves, entitySurfaces, entityVolumes;   // ref → entités

    Key key(const P3& p) const { return { std::llround(p.x / tol), std::llround(p.y / tol), std::llround(p.z / tol) }; }
    bool wanted(const std::string& ref) const { return opts.only.empty() || std::find(opts.only.begin(), opts.only.end(), ref) != opts.only.end(); }

    int point(const P3& p, int tsaNode = 0)
    {
        if (tsaNode == 0)
        {
            auto t = tsaByKey.find(key(p));
            if (t != tsaByKey.end()) tsaNode = t->second;
        }
        if (tsaNode > 0)
        {
            auto it = tsaPoint.find(tsaNode);
            if (it != tsaPoint.end()) return it->second;
            const P3& q = geo.tsaNodes.at(tsaNode);
            const int tag = gmsh::model::geo::addPoint(q.x, q.y, q.z);
            tsaPoint[tsaNode] = tag;
            pointPos[tag] = q;
            return tag;
        }
        const Key k = key(p);
        auto it = coordPoint.find(k);
        if (it != coordPoint.end()) return it->second;
        const int tag = gmsh::model::geo::addPoint(p.x, p.y, p.z);
        coordPoint[k] = tag;
        pointPos[tag] = p;
        return tag;
    }
    /// Courbe a → b (signe négatif si la courbe existante est orientée b → a).
    int line(int a, int b, const std::string& user)
    {
        const auto k = std::make_pair(std::min(a, b), std::max(a, b));
        int tag;
        auto it = lineByPoints.find(k);
        if (it == lineByPoints.end())
        {
            tag = gmsh::model::geo::addLine(a, b);
            lineByPoints[k] = tag;
            lineEnds[tag] = { a, b };
        }
        else
            tag = it->second;
        auto& u = curveUsers[tag];
        if (std::find(u.begin(), u.end(), user) == u.end()) u.push_back(user);
        return lineEnds[tag].first == a ? tag : -tag;
    }
    P3 linePoint(int tag, bool start) const
    {
        const auto& e = lineEnds.at(std::abs(tag));
        return pointPos.at(start ? e.first : e.second);
    }

    // --- Géométrie ---------------------------------------------------------------------------------
    void buildMembers()
    {
        if (!set.global.mesh1D) return;
        for (const auto& m : geo.members)
        {
            if (!wanted(m.ref) || m.length <= tol) continue;
            const auto* loc = localSettingFor(set, m.ref);
            std::vector<double> cuts;
            if (!(loc && loc->divisions))
            {
                if (set.global.keepCharacteristicPoints) cuts = m.pointLoads;
                if (loc)
                    for (const auto& p : loc->portions) cuts.push_back(p.from * m.length), cuts.push_back(p.to * m.length);
            }
            std::sort(cuts.begin(), cuts.end());
            std::vector<double> pos { 0.0 };
            for (double c : cuts)
                if (c > pos.back() + 1e-6 && c < m.length - 1e-6) pos.push_back(c);
            pos.push_back(m.length);
            int prev = point(m.a, m.nodeI);
            for (std::size_t i = 1; i < pos.size(); ++i)
            {
                const bool last = i + 1 == pos.size();
                const P3 p = m.a + (m.b - m.a) * (pos[i] / m.length);
                const int pt = last ? point(m.b, m.nodeJ) : point(p);
                if (!last && std::find(m.pointLoads.begin(), m.pointLoads.end(), pos[i]) != m.pointLoads.end()) loadPoints.push_back({ pt, m.ref });
                const int signedLine = line(prev, pt, m.ref);
                memberSegs[m.ref].push_back({ std::abs(signedLine), signedLine > 0, pos[i - 1], pos[i] });
                memberLines.insert(std::abs(signedLine));
                entityCurves[m.ref].push_back(std::abs(signedLine));
                prev = pt;
            }
            if (loc && loc->divisions && memberSegs[m.ref].size() == 1) fixedCount[memberSegs[m.ref][0].line] = *loc->divisions;
        }
    }

    int loop(const std::vector<P3>& poly, const std::vector<int>& nodes, const std::string& ref, std::vector<int>* lines)
    {
        std::vector<int> pts, signedLines;
        for (std::size_t i = 0; i < poly.size(); ++i) pts.push_back(point(poly[i], i < nodes.size() ? nodes[i] : 0));
        for (std::size_t i = 0; i < pts.size(); ++i)
        {
            const int l = line(pts[i], pts[(i + 1) % pts.size()], ref);
            signedLines.push_back(l);
            lines->push_back(std::abs(l));
        }
        return gmsh::model::geo::addCurveLoop(signedLines);
    }

    void buildSurfaces()
    {
        if (!set.global.mesh2D) return;
        for (const auto& s : geo.surfaces)
        {
            if (!wanted(s.ref)) continue;
            std::vector<int> lines, holes;
            std::vector<int> loops { loop(s.outer, s.cornerNodes, s.ref, &lines) };
            for (const auto& h : s.holes) loops.push_back(loop(h, {}, s.ref, &holes));
            const int tag = gmsh::model::geo::addPlaneSurface(loops);
            surfaceTag[s.ref] = tag;
            surfaceRef[tag] = s.ref;
            surfaceLines[tag] = lines;
            surfaceLines[tag].insert(surfaceLines[tag].end(), holes.begin(), holes.end());
            holeLines[tag] = holes;
            entitySurfaces[s.ref].push_back(tag);
        }
    }

    /// Semelle : 4 volumes autour de l'arête verticale passant par le nœud d'appui (centre de la face supérieure),
    /// qui devient un sommet du maillage (liaison avec le poteau) sans empêcher le maillage hexaédrique.
    void buildVolumes()
    {
        if (!set.global.mesh3D) return;
        for (const auto& v : geo.volumes)
        {
            if (!wanted(v.ref)) continue;
            const bool split = v.nodePos.x > v.min.x + tol && v.nodePos.x < v.max.x - tol && v.nodePos.y > v.min.y + tol && v.nodePos.y < v.max.y - tol;
            const std::vector<double> xs = split ? std::vector<double> { v.min.x, v.nodePos.x, v.max.x } : std::vector<double> { v.min.x, v.max.x };
            const std::vector<double> ys = split ? std::vector<double> { v.min.y, v.nodePos.y, v.max.y } : std::vector<double> { v.min.y, v.max.y };
            const std::vector<double> zs { v.min.z, v.max.z };
            const int nx = int(xs.size()), ny = int(ys.size());
            auto P = [&](int i, int j, int k) {
                const P3 p { xs[i], ys[j], zs[k] };
                const bool center = split && i == 1 && j == 1 && k == 1;
                return point(p, center ? v.nodeId : 0);
            };
            std::map<std::vector<int>, std::pair<int, int>> faceUse;   // coins triés → (surface, utilisations)
            auto face = [&](std::vector<int> c) {
                std::vector<int> k = c;
                std::sort(k.begin(), k.end());
                auto it = faceUse.find(k);
                if (it != faceUse.end())
                {
                    ++it->second.second;
                    return it->second.first;
                }
                std::vector<int> sl;
                for (int i = 0; i < 4; ++i)
                {
                    const int l = line(c[i], c[(i + 1) % 4], v.ref);
                    sl.push_back(l);
                    volumeLines[v.ref].push_back(std::abs(l));
                }
                const int s = gmsh::model::geo::addPlaneSurface({ gmsh::model::geo::addCurveLoop(sl) });
                faceUse[k] = { s, 1 };
                return s;
            };
            for (int i = 0; i + 1 < nx; ++i)
                for (int j = 0; j + 1 < ny; ++j)
                {
                    const int a = P(i, j, 0), b = P(i + 1, j, 0), c = P(i + 1, j + 1, 0), d = P(i, j + 1, 0);
                    const int e = P(i, j, 1), f = P(i + 1, j, 1), g = P(i + 1, j + 1, 1), h = P(i, j + 1, 1);
                    const std::vector<int> faces { face({ a, d, c, b }), face({ e, f, g, h }), face({ a, b, f, e }), face({ b, c, g, f }),
                                                   face({ c, d, h, g }), face({ d, a, e, h }) };
                    const int shell = gmsh::model::geo::addSurfaceLoop(faces);
                    const int vol = gmsh::model::geo::addVolume({ shell });
                    volumeTags[v.ref].push_back(vol);
                    entityVolumes[v.ref].push_back(vol);
                }
            for (const auto& [k, use] : faceUse)
                if (use.second == 1) volumeFaces[v.ref].push_back(use.first);
            if (!split)
                record.warnings.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "%1 : nœud d'appui hors de la semelle : semelle maillée sans liaison au nœud")
                                              .arg(QString::fromStdString(v.ref))
                                              .toStdString());
        }
    }

    // --- Plongements et interfaces ------------------------------------------------------------------
    static bool inPolygon2D(const SurfaceGeom& s, const P3& p, bool* onBoundary)
    {
        *onBoundary = false;
        for (std::size_t i = 0; i < s.outer.size(); ++i)
            if (distanceToSegment(p, s.outer[i], s.outer[(i + 1) % s.outer.size()]) < 1e-6) *onBoundary = true;
        double u = 0, v = 0;
        s.frame.toLocal(p, &u, &v);
        std::vector<std::pair<double, double>> outer;
        for (const auto& c : s.outer)
        {
            double cu = 0, cv = 0;
            s.frame.toLocal(c, &cu, &cv);
            outer.emplace_back(cu, cv);
        }
        if (!pointInPolygon(u, v, outer)) return false;
        for (const auto& h : s.holes)
        {
            std::vector<std::pair<double, double>> hl;
            for (const auto& c : h)
            {
                double cu = 0, cv = 0;
                s.frame.toLocal(c, &cu, &cv);
                hl.emplace_back(cu, cv);
            }
            if (pointInPolygon(u, v, hl)) return false;
        }
        return true;
    }

    void embedAndCheck()
    {
        for (const auto& s : geo.surfaces)
        {
            auto st = surfaceTag.find(s.ref);
            if (st == surfaceTag.end()) continue;
            const int surf = st->second;
            auto coplanar = [&](const P3& p) { return std::abs((p - s.frame.origin).dot(s.frame.N)) < 1e-6; };
            auto isVertex = [&](const P3& p) {
                for (const auto& c : s.outer)
                    if (distance(c, p) < 1e-6) return true;
                return false;
            };
            // Nœuds TSA intérieurs (pied de poteau, nœud de barre) : sommets du maillage de la surface.
            std::vector<int> pts;
            if (set.global.keepCharacteristicPoints)
                for (const auto& [node, tag] : tsaPoint)
                {
                    const P3& p = pointPos[tag];
                    bool onB = false;
                    if (coplanar(p) && inPolygon2D(s, p, &onB) && !onB) pts.push_back(tag);
                }
            // Courbes (barres, pieds de voiles) posées dans le plan de la surface, à l'intérieur.
            std::vector<int> lines;
            const auto& own = surfaceLines[surf];
            for (const auto& [tag, ends] : lineEnds)
            {
                if (std::find(own.begin(), own.end(), tag) != own.end()) continue;
                const P3 a = pointPos[ends.first], b = pointPos[ends.second];
                if (!coplanar(a) || !coplanar(b)) continue;
                bool onA = false, onB = false, onM = false;
                const bool inA = inPolygon2D(s, a, &onA) || onA, inB = inPolygon2D(s, b, &onB) || onB;
                const bool inM = inPolygon2D(s, (a + b) * 0.5, &onM);
                if (!inM || onM) continue;   // hors surface ou confondue avec un bord (arête déjà partagée)
                const std::string user = curveUsers[tag].empty() ? std::string("?") : curveUsers[tag].front();
                if (inA && inB && (!onA || isVertex(a)) && (!onB || isVertex(b)))
                {
                    lines.push_back(tag);
                    record.interfaces.push_back(
                        { user, s.ref, QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "courbe plongée dans la surface : nœuds partagés").toStdString(), true });
                }
                else
                    record.interfaces.push_back({ user, s.ref,
                                                  QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                                              "courbe recoupant le contour de la surface hors d'un sommet : intersection non maillée")
                                                      .toStdString(),
                                                  false });
            }
            try
            {
                if (!pts.empty()) gmsh::model::mesh::embed(0, pts, 2, surf), embeddedInSurface.insert(surf);
                if (!lines.empty()) gmsh::model::mesh::embed(1, lines, 2, surf), embeddedInSurface.insert(surf);
            }
            catch (const std::exception& e)
            {
                record.warnings.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "%1 : plongement refusé par Gmsh (%2)")
                                              .arg(QString::fromStdString(s.ref), QString::fromStdString(e.what()))
                                              .toStdString());
            }
        }
        // Courbes partagées : interfaces conformes
        for (const auto& [tag, users] : curveUsers)
            for (std::size_t i = 0; i < users.size(); ++i)
                for (std::size_t j = i + 1; j < users.size(); ++j)
                    record.interfaces.push_back(
                        { users[i], users[j], QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "courbe commune : nœuds partagés").toStdString(), true });
        // Barres partiellement confondues avec un bord de surface (extrémités différentes)
        for (const auto& m : geo.members)
            for (const auto& s : geo.surfaces)
                for (std::size_t i = 0; i < s.outer.size(); ++i)
                {
                    const P3 &a = s.outer[i], &b = s.outer[(i + 1) % s.outer.size()];
                    const bool same = (distance(a, m.a) < 1e-6 && distance(b, m.b) < 1e-6) || (distance(a, m.b) < 1e-6 && distance(b, m.a) < 1e-6);
                    if (same) continue;
                    const bool mOnEdge = distanceToSegment(m.a, a, b) < 1e-6 && distanceToSegment(m.b, a, b) < 1e-6;
                    const bool edgeOnM = distanceToSegment(a, m.a, m.b) < 1e-6 && distanceToSegment(b, m.a, m.b) < 1e-6;
                    if (mOnEdge || edgeOnM)
                        record.interfaces.push_back({ m.ref, s.ref,
                                                      QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                                                  "barre partiellement confondue avec un bord : nœuds intermédiaires non partagés")
                                                          .toStdString(),
                                                      false });
                }
        // Volumes jointifs ou superposés : maillés séparément
        for (std::size_t i = 0; i < geo.volumes.size(); ++i)
            for (std::size_t j = i + 1; j < geo.volumes.size(); ++j)
            {
                const auto &a = geo.volumes[i], &b = geo.volumes[j];
                const double ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x), oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y),
                             oz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
                if (ox < -tol || oy < -tol || oz < -tol) continue;
                record.interfaces.push_back(
                    { a.ref, b.ref,
                      (ox > tol && oy > tol && oz > tol ? QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "volumes superposés : maillages indépendants")
                                                        : QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "volumes jointifs : faces non fusionnées"))
                          .toStdString(),
                      false });
            }
        for (const auto& v : geo.volumes)
            for (const auto& m : geo.members)
                if ((m.nodeI == v.nodeId || m.nodeJ == v.nodeId) && entityVolumes.count(v.ref) && memberSegs.count(m.ref))
                    record.interfaces.push_back({ m.ref, v.ref,
                                                  QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                                              "liaison barre – volume par un seul nœud : rotations de la barre non transmises au volume")
                                                      .toStdString(),
                                                  false });
    }

    // --- Contraintes de maillage --------------------------------------------------------------------
    void transfinite()
    {
        // Divisions imposées sur les barres
        for (const auto& m : geo.members)
        {
            auto it = memberSegs.find(m.ref);
            const auto* loc = localSettingFor(set, m.ref);
            if (it == memberSegs.end() || !loc || !loc->divisions || it->second.size() != 1) continue;
            const auto& seg = it->second[0];
            const int n = *loc->divisions;
            switch (loc->distribution)
            {
            case Distribution::Uniform: gmsh::model::mesh::setTransfiniteCurve(seg.line, n + 1, "Progression", 1.0); break;
            case Distribution::Progressive:
                gmsh::model::mesh::setTransfiniteCurve(seg.line, n + 1, "Progression", seg.forward ? loc->progression : 1.0 / loc->progression);
                break;
            case Distribution::Symmetric:
                gmsh::model::mesh::setTransfiniteCurve(seg.line, n + 1, "Bump", 1.0 / std::max(1.0001, loc->progression));
                break;
            }
        }
        // Surfaces structurées (4 côtés, sans ouverture ni entité plongée, sans zone de raffinement)
        std::map<int, int> required;   // courbe → divisions
        std::set<int> structured;
        for (const auto& s : geo.surfaces)
        {
            auto st = surfaceTag.find(s.ref);
            if (st == surfaceTag.end()) continue;
            const auto* loc = localSettingFor(set, s.ref);
            const bool want = loc && loc->structured ? *loc->structured : set.global.preferStructured;
            if (!want) continue;
            std::string why;
            std::vector<P3> samples = s.outer;
            P3 c;
            for (const auto& p : s.outer) c = c + p;
            samples.push_back(c * (1.0 / double(s.outer.size())));
            if (s.outer.size() != 4)
                why = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "contour à %1 côtés").arg(QString::number(s.outer.size())).toStdString();
            else if (!s.holes.empty()) why = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "ouverture(s)").toStdString();
            else if (embeddedInSurface.count(st->second)) why = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "nœuds ou courbes plongés").toStdString();
            else if (!(loc && loc->structured && *loc->structured) && ev.refined(s.ref, samples))
                why = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "zone de raffinement (taille variable)").toStdString();
            if (!why.empty())
            {
                if (loc && loc->structured && *loc->structured)
                    record.warnings.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "%1 : maillage structuré impossible (%2) : maillage non structuré")
                                                  .arg(QString::fromStdString(s.ref), QString::fromStdString(why))
                                                  .toStdString());
                continue;
            }
            structured.insert(st->second);
        }
        for (int pass = 0; pass < 20; ++pass)
        {
            bool changed = false;
            for (auto it = structured.begin(); it != structured.end();)
            {
                const int surf = *it;
                const auto& lines = surfaceLines[surf];
                const std::string& ref = surfaceRef[surf];
                const auto* loc = localSettingFor(set, ref);
                bool ok = true;
                for (int dir = 0; dir < 2 && ok; ++dir)
                {
                    const int l1 = lines[dir], l2 = lines[dir + 2];
                    auto natural = [&](int l) {
                        if (fixedCount.count(l)) return fixedCount[l];
                        if (required.count(l)) return required[l];
                        return ev.divisions(linePoint(l, true), linePoint(l, false), curveUsers[l]);
                    };
                    int n = std::max(natural(l1), natural(l2));
                    const auto& forced = dir == 0 ? (loc ? loc->divisionsU : std::nullopt) : (loc ? loc->divisionsV : std::nullopt);
                    if (forced) n = std::max(n, *forced);
                    for (int l : { l1, l2 })
                        if (fixedCount.count(l) && fixedCount[l] != n) ok = false;
                    if (!ok) break;
                    for (int l : { l1, l2 })
                        if (required[l] != n)
                        {
                            required[l] = std::max(required[l], n);
                            changed = true;
                        }
                }
                if (!ok)
                {
                    record.warnings.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                                          "%1 : divisions imposées par une barre de bord incompatibles avec un maillage structuré : "
                                                                          "maillage non structuré")
                                                  .arg(QString::fromStdString(ref))
                                                  .toStdString());
                    it = structured.erase(it);
                    changed = true;
                }
                else
                    ++it;
            }
            if (!changed) break;
        }
        for (const auto& [l, n] : required)
            if (!fixedCount.count(l)) gmsh::model::mesh::setTransfiniteCurve(l, n + 1);
        for (int surf : structured)
        {
            gmsh::model::mesh::setTransfiniteSurface(surf);
            const auto* loc = localSettingFor(set, surfaceRef[surf]);
            if (!(loc && loc->shape == SurfaceShape::Triangles)) gmsh::model::mesh::setRecombine(2, surf);
        }
        // Surfaces non structurées : algorithme local, recombinaison
        for (const auto& [ref, surf] : surfaceTag)
        {
            if (structured.count(surf)) continue;
            const auto* loc = localSettingFor(set, ref);
            const Algorithm2D algo = loc && loc->algorithm ? *loc->algorithm : set.global.algorithm2D;
            if (loc && loc->algorithm) gmsh::model::mesh::setAlgorithm(2, surf, static_cast<int>(algo));
            const bool quads = set.global.recombine || (loc && loc->shape == SurfaceShape::Quadrilaterals) || algo == Algorithm2D::FrontalDelaunayQuads ||
                               algo == Algorithm2D::QuasiStructuredQuad;
            if (quads && !(loc && loc->shape == SurfaceShape::Triangles)) gmsh::model::mesh::setRecombine(2, surf);
        }
        record.parameters["surfaces structurées"] = std::to_string(structured.size()) + " / " + std::to_string(surfaceTag.size());
        // Volumes hexaédriques : contraintes transfinies sur les 4 sous-volumes de chaque semelle
        if (set.global.volumeElement == VolumeElement::Hexahedra)
            for (const auto& [ref, vols] : volumeTags)
            {
                std::map<int, int> counts;   // direction → divisions (mêmes longueurs par direction dans une semelle)
                for (int l : volumeLines[ref])
                {
                    const P3 a = linePoint(l, true), b = linePoint(l, false), d = b - a;
                    const int dir = std::abs(d.x) > std::abs(d.y) ? (std::abs(d.x) > std::abs(d.z) ? 0 : 2) : (std::abs(d.y) > std::abs(d.z) ? 1 : 2);
                    counts[dir] = std::max(counts[dir], ev.divisions(a, b, { ref }));
                }
                for (int l : volumeLines[ref])
                {
                    const P3 a = linePoint(l, true), b = linePoint(l, false), d = b - a;
                    const int dir = std::abs(d.x) > std::abs(d.y) ? (std::abs(d.x) > std::abs(d.z) ? 0 : 2) : (std::abs(d.y) > std::abs(d.z) ? 1 : 2);
                    gmsh::model::mesh::setTransfiniteCurve(l, counts[dir] + 1);
                }
                DimTags faces;
                gmsh::model::getBoundary([&] {
                    DimTags d;
                    for (int v : vols) d.push_back({ 3, v });
                    return d;
                }(),
                                         faces, false, false, false);
                std::set<int> done;
                for (const auto& [dim, f] : faces)
                    if (done.insert(std::abs(f)).second)
                    {
                        gmsh::model::mesh::setTransfiniteSurface(std::abs(f));
                        gmsh::model::mesh::setRecombine(2, std::abs(f));
                    }
                for (int v : vols) gmsh::model::mesh::setTransfiniteVolume(v);
            }
    }

    // --- Champs de taille ---------------------------------------------------------------------------
    int fieldConstant(double value, const std::vector<int>& curves, const std::vector<int>& surfaces, const std::vector<int>& volumes)
    {
        const int f = gmsh::model::mesh::field::add("Constant");
        gmsh::model::mesh::field::setNumber(f, "VIn", value);
        gmsh::model::mesh::field::setNumber(f, "VOut", 1e22);
        gmsh::model::mesh::field::setNumber(f, "IncludeBoundary", 1);
        auto list = [](const std::vector<int>& v) {
            std::vector<double> d;
            for (int x : v) d.push_back(double(x));
            return d;
        };
        if (!curves.empty()) gmsh::model::mesh::field::setNumbers(f, "CurvesList", list(curves));
        if (!surfaces.empty()) gmsh::model::mesh::field::setNumbers(f, "SurfacesList", list(surfaces));
        if (!volumes.empty()) gmsh::model::mesh::field::setNumbers(f, "VolumesList", list(volumes));
        return f;
    }
    int restrictTo(int field, const std::vector<std::string>& targets)
    {
        std::vector<double> c, s, v;
        for (const auto& t : targets)
        {
            for (int x : entityCurves[t]) c.push_back(x);
            for (int x : entitySurfaces[t]) s.push_back(x);
            for (int x : entityVolumes[t]) v.push_back(x);
        }
        const int r = gmsh::model::mesh::field::add("Restrict");
        gmsh::model::mesh::field::setNumber(r, "InField", field);
        gmsh::model::mesh::field::setNumber(r, "IncludeBoundary", 1);
        if (!c.empty()) gmsh::model::mesh::field::setNumbers(r, "CurvesList", c);
        if (!s.empty()) gmsh::model::mesh::field::setNumbers(r, "SurfacesList", s);
        if (!v.empty()) gmsh::model::mesh::field::setNumbers(r, "VolumesList", v);
        return r;
    }
    int threshold(int in, double hz, double dmin)
    {
        const double hmax = std::max(set.global.maxSize, hz);
        const double rate = std::max(0.05, set.global.transitionRate);
        const int t = gmsh::model::mesh::field::add("Threshold");
        gmsh::model::mesh::field::setNumber(t, "InField", in);
        gmsh::model::mesh::field::setNumber(t, "SizeMin", hz);
        gmsh::model::mesh::field::setNumber(t, "SizeMax", hmax);
        gmsh::model::mesh::field::setNumber(t, "DistMin", dmin);
        gmsh::model::mesh::field::setNumber(t, "DistMax", dmin + (hmax - hz) / rate);
        gmsh::model::mesh::field::setNumber(t, "StopAtDistMax", 1);
        return t;
    }
    int distanceTo(const P3& p)
    {
        const int f = gmsh::model::mesh::field::add("MathEval");
        gmsh::model::mesh::field::setString(f, "F", "sqrt((x-(" + fmt(p.x) + "))^2+(y-(" + fmt(p.y) + "))^2+(z-(" + fmt(p.z) + "))^2)");
        return f;
    }

    void fields()
    {
        std::vector<double> all;
        // Taille propre de chaque objet (locale ou globale), regroupée par valeur
        std::map<double, std::array<std::vector<int>, 3>> bySize;
        auto own = [&](const std::string& ref) {
            auto& e = bySize[effectiveSizeFor(set, ref)];
            for (int x : entityCurves[ref]) e[0].push_back(x);
            for (int x : entitySurfaces[ref]) e[1].push_back(x);
            for (int x : entityVolumes[ref]) e[2].push_back(x);
        };
        for (const auto& [ref, v] : entityCurves) own(ref);
        for (const auto& [ref, v] : entitySurfaces) own(ref);
        for (const auto& [ref, v] : entityVolumes) own(ref);
        for (const auto& [h, e] : bySize) all.push_back(fieldConstant(h, e[0], e[1], e[2]));
        // Paramètres 1D : longueur maximale, portions, raffinement aux extrémités
        for (const auto& m : geo.members)
        {
            auto it = memberSegs.find(m.ref);
            const auto* loc = localSettingFor(set, m.ref);
            if (it == memberSegs.end() || !loc || loc->divisions) continue;
            std::vector<int> lines;
            for (const auto& s : it->second) lines.push_back(s.line);
            if (loc->maxLength) all.push_back(fieldConstant(*loc->maxLength, lines, {}, {}));
            for (const auto& p : loc->portions)
            {
                std::vector<int> part;
                for (const auto& s : it->second)
                    if (s.s0 >= p.from * m.length - 1e-6 && s.s1 <= p.to * m.length + 1e-6) part.push_back(s.line);
                if (!part.empty()) all.push_back(fieldConstant(p.size, part, {}, {}));
            }
            if (loc->endSize)
                for (const P3& end : { m.a, m.b }) all.push_back(restrictTo(threshold(distanceTo(end), ev.clamp(*loc->endSize), 0.0), { m.ref }));
        }
        // Zones de raffinement (utilisateur et automatiques)
        for (const auto& z : ev.zones())
        {
            const std::string ref = z.targets.empty() ? std::string() : z.targets.front();
            const double hz = z.size ? ev.clamp(*z.size) : ev.clamp(effectiveSizeFor(set, ref) * z.factor.value_or(1.0));
            int in = -1;
            if (z.segment)
            {
                // Contour d'ouverture : distance à la courbe correspondante
                const Key ka = key(z.a), kb = key(z.b);
                auto pa = coordPoint.find(ka), pb = coordPoint.find(kb);
                if (pa == coordPoint.end() || pb == coordPoint.end()) continue;
                auto l = lineByPoints.find({ std::min(pa->second, pb->second), std::max(pa->second, pb->second) });
                if (l == lineByPoints.end()) continue;
                in = gmsh::model::mesh::field::add("Distance");
                gmsh::model::mesh::field::setNumbers(in, "CurvesList", { double(l->second) });
                gmsh::model::mesh::field::setNumber(in, "Sampling", 50);
            }
            else
                in = distanceTo(z.a);
            int f = threshold(in, hz, z.radius);
            if (!z.targets.empty()) f = restrictTo(f, z.targets);
            all.push_back(f);
        }
        const int mn = gmsh::model::mesh::field::add("Min");
        gmsh::model::mesh::field::setNumbers(mn, "FieldsList", all);
        gmsh::model::mesh::field::setAsBackgroundMesh(mn);
        record.parameters["champs de taille Gmsh"] = std::to_string(all.size());
    }

    void options()
    {
        const auto& g = set.global;
        gmsh::option::setNumber("General.NumThreads", 1);   // résultat reproductible
        gmsh::option::setNumber("Geometry.Tolerance", tol);
        gmsh::option::setNumber("Mesh.MeshSizeMin", g.minSize);
        gmsh::option::setNumber("Mesh.MeshSizeMax", g.maxSize);
        gmsh::option::setNumber("Mesh.MeshSizeFromPoints", 0);
        gmsh::option::setNumber("Mesh.MeshSizeFromCurvature", 0);
        gmsh::option::setNumber("Mesh.MeshSizeExtendFromBoundary", 0);
        gmsh::option::setNumber("Mesh.Algorithm", static_cast<int>(g.algorithm2D));
        gmsh::option::setNumber("Mesh.Algorithm3D", static_cast<int>(g.algorithm3D));
        gmsh::option::setNumber("Mesh.RecombinationAlgorithm", static_cast<int>(g.recombination));
        gmsh::option::setNumber("Mesh.Smoothing", g.smoothing);
        gmsh::option::setNumber("Mesh.Optimize", g.optimize ? 1 : 0);
        gmsh::option::setNumber("Mesh.ElementOrder", g.order == ElementOrder::Second ? 2 : 1);
        gmsh::option::setNumber("Mesh.SecondOrderIncomplete", 1);   // éléments de Serendip : quad8, hex20
        gmsh::option::setNumber("Mesh.SaveAll", 0);
        gmsh::option::setNumber("Mesh.MshFileVersion", 4.1);
    }

    // --- Extraction ---------------------------------------------------------------------------------
    void extract(FeMesh& mesh)
    {
        std::vector<std::size_t> tags;
        std::vector<double> coord, param;
        gmsh::model::mesh::getNodes(tags, coord, param, -1, -1, true, false);
        std::unordered_map<std::size_t, int> idOf;
        mesh.nodes.reserve(tags.size());
        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (idOf.count(tags[i])) continue;
            FeNode n;
            n.id = int(mesh.nodes.size()) + 1;
            n.x = coord[3 * i], n.y = coord[3 * i + 1], n.z = coord[3 * i + 2];
            n.gmshTag = tags[i];
            idOf[tags[i]] = n.id;
            mesh.nodes.push_back(n);
        }
        for (const auto& [node, pt] : tsaPoint)
        {
            std::vector<std::size_t> t;
            std::vector<double> c, pc;
            gmsh::model::mesh::getNodes(t, c, pc, 0, pt, true, false);
            if (!t.empty() && idOf.count(t[0])) mesh.nodes[std::size_t(idOf[t[0]] - 1)].tsaNode = node;
        }
        std::unordered_map<std::size_t, int> elementOf;   // étiquette Gmsh → id TSA
        std::map<std::string, std::vector<int>> groupElements;
        std::set<int> unknownTypes;
        auto take = [&](int dim, int tag, const std::string& source, bool boundary) {
            std::vector<int> types;
            std::vector<std::vector<std::size_t>> et, en;
            gmsh::model::mesh::getElements(types, et, en, dim, tag);
            for (std::size_t k = 0; k < types.size(); ++k)
            {
                FeType ft;
                if (!fromGmshType(types[k], &ft))
                {
                    unknownTypes.insert(types[k]);
                    continue;
                }
                const std::size_t nn = en[k].size() / std::max<std::size_t>(1, et[k].size());
                for (std::size_t e = 0; e < et[k].size(); ++e)
                {
                    if (elementOf.count(et[k][e])) continue;
                    FeElement fe;
                    fe.id = int(mesh.elements.size()) + 1;
                    fe.type = ft;
                    fe.source = source;
                    fe.boundary = boundary;
                    fe.gmshTag = et[k][e];
                    fe.gmshEntityDim = dim;
                    fe.gmshEntityTag = tag;
                    for (std::size_t j = 0; j < nn; ++j) fe.nodes.push_back(idOf[en[k][e * nn + j]]);
                    elementOf[et[k][e]] = fe.id;
                    groupElements[boundary ? source + ":contour" : source].push_back(fe.id);
                    mesh.elements.push_back(fe);
                }
            }
        };
        for (const auto& [ref, segs] : memberSegs)
            for (const auto& s : segs) take(1, s.line, ref, false);
        for (const auto& [ref, surf] : surfaceTag) take(2, surf, ref, false);
        for (const auto& [ref, vols] : volumeTags)
            for (int v : vols) take(3, v, ref, false);
        if (set.global.boundaryElements)
        {
            for (const auto& [surf, lines] : surfaceLines)
                for (int l : lines)
                    if (!memberLines.count(l)) take(1, l, surfaceRef[surf], true);
            for (const auto& [ref, faces] : volumeFaces)
                for (int f : faces) take(2, f, ref, true);
        }
        if (!unknownTypes.empty())
        {
            std::string t;
            for (int u : unknownTypes) t += " " + std::to_string(u);
            record.warnings.push_back(
                QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "types d'éléments Gmsh non repris dans le maillage TSA :%1").arg(QString::fromStdString(t)).toStdString());
        }
        // Source des nœuds : objet du premier élément qui les utilise
        for (const auto& e : mesh.elements)
            for (int n : e.nodes)
                if (n >= 1 && mesh.nodes[std::size_t(n - 1)].source.empty()) mesh.nodes[std::size_t(n - 1)].source = e.source;
        // Qualité Gmsh (minSICN) des éléments 2D et 3D
        std::vector<std::size_t> qTags;
        for (const auto& [gt, id] : elementOf)
            if (feTypeDimension(mesh.elements[std::size_t(id - 1)].type) >= 2) qTags.push_back(gt);
        if (!qTags.empty())
        {
            std::vector<double> q;
            gmsh::model::mesh::getElementQualities(qTags, q, "minSICN");
            for (std::size_t i = 0; i < qTags.size() && i < q.size(); ++i) mesh.elements[std::size_t(elementOf[qTags[i]] - 1)].quality = q[i];
        }
        // Groupes : objets, contours, appuis, charges ponctuelles, matériaux, interfaces
        if (set.global.physicalGroups)
        {
            for (const auto& [name, els] : groupElements)
            {
                FeGroup g;
                g.name = name;
                g.dimension = feTypeDimension(mesh.elements[std::size_t(els.front() - 1)].type);
                g.elements = els;
                mesh.groups.push_back(g);
            }
            FeGroup supports { "appuis", 0, {}, {} };
            for (const auto& p : geo.points)
                if (p.kind == CharacteristicPoint::Kind::Support)
                    for (const auto& n : mesh.nodes)
                        if (n.tsaNode == p.nodeId) supports.nodes.push_back(n.id);
            if (!supports.nodes.empty()) mesh.groups.push_back(supports);
            FeGroup loads { "charges ponctuelles", 0, {}, {} };
            for (const auto& [pt, ref] : loadPoints)
            {
                std::vector<std::size_t> t;
                std::vector<double> c, pc;
                gmsh::model::mesh::getNodes(t, c, pc, 0, pt, true, false);
                if (!t.empty()) loads.nodes.push_back(idOf[t[0]]);
            }
            if (!loads.nodes.empty()) mesh.groups.push_back(loads);
            std::map<std::string, std::vector<int>> byMaterial;
            auto materialOf = [&](const std::string& src) -> std::string {
                if (const auto* m = geo.member(src)) return m->material;
                if (const auto* s = geo.surface(src)) return s->material;
                if (const auto* v = geo.volume(src)) return v->material;
                return {};
            };
            for (const auto& e : mesh.elements)
                if (!e.boundary)
                    if (const std::string mat = materialOf(e.source); !mat.empty()) byMaterial[mat].push_back(e.id);
            for (const auto& [mat, els] : byMaterial)
                mesh.groups.push_back({ "matériau:" + mat, feTypeDimension(mesh.elements[std::size_t(els.front() - 1)].type), els, {} });
            for (const auto& [tag, users] : curveUsers)
            {
                if (users.size() < 2) continue;
                std::vector<std::size_t> t;
                std::vector<double> c, pc;
                gmsh::model::mesh::getNodes(t, c, pc, 1, tag, true, false);
                FeGroup g;
                g.name = "interface:";
                for (std::size_t i = 0; i < users.size(); ++i) g.name += (i ? "|" : "") + users[i];
                g.dimension = 1;
                for (auto x : t) g.nodes.push_back(idOf[x]);
                mesh.groups.push_back(g);
            }
            // Groupes physiques Gmsh (créés par physicalGroups() avant l'extraction) : étiquettes conservées,
            // associées par nom et dimension (l'étiquette Gmsh n'est pas un identifiant TSA).
            DimTags phys;
            gmsh::model::getPhysicalGroups(phys);
            for (const auto& [pd, pt] : phys)
            {
                std::string pname;
                gmsh::model::getPhysicalName(pd, pt, pname);
                for (auto& g : mesh.groups)
                    if (g.name == pname && g.dimension == pd)
                    {
                        g.gmshPhysicalDim = pd;
                        g.gmshPhysicalTag = pt;
                    }
            }
        }
    }

    /// Groupes physiques Gmsh (export .msh) : noms = références TSA.
    void physicalGroups()
    {
        for (const auto& [ref, segs] : memberSegs)
        {
            std::vector<int> l;
            for (const auto& s : segs) l.push_back(s.line);
            gmsh::model::addPhysicalGroup(1, l, -1, ref);
        }
        for (const auto& [ref, surf] : surfaceTag)
        {
            gmsh::model::addPhysicalGroup(2, { surf }, -1, ref);
            if (set.global.boundaryElements)
            {
                std::vector<int> l;
                for (int x : surfaceLines[surf])
                    if (!memberLines.count(x)) l.push_back(x);
                if (!l.empty()) gmsh::model::addPhysicalGroup(1, l, -1, ref + ":contour");
            }
        }
        for (const auto& [ref, vols] : volumeTags)
        {
            gmsh::model::addPhysicalGroup(3, vols, -1, ref);
            if (set.global.boundaryElements && !volumeFaces[ref].empty()) gmsh::model::addPhysicalGroup(2, volumeFaces[ref], -1, ref + ":contour");
        }
        std::vector<int> sp;
        for (const auto& p : geo.points)
            if (p.kind == CharacteristicPoint::Kind::Support)
                if (auto it = tsaPoint.find(p.nodeId); it != tsaPoint.end()) sp.push_back(it->second);
        if (!sp.empty()) gmsh::model::addPhysicalGroup(0, sp, -1, "appuis");
    }
};
} // namespace

MesherCapabilities capabilities()
{
    MesherCapabilities c;
    std::string why;
    c.available = dllLoaded(&why);
    c.unavailableReason = why;
    if (c.available) c.version = versionString();
    if (c.available && c.version.empty())
    {
        c.available = false;
        c.unavailableReason = "initialisation de Gmsh impossible";
    }
    c.algorithms2D = { Algorithm2D::Automatic, Algorithm2D::MeshAdapt, Algorithm2D::Delaunay, Algorithm2D::FrontalDelaunay, Algorithm2D::FrontalDelaunayQuads,
                       Algorithm2D::PackingParallelograms, Algorithm2D::QuasiStructuredQuad };
    c.algorithms3D = { Algorithm3D::Delaunay, Algorithm3D::Frontal, Algorithm3D::HXT };
    c.recombinationAlgorithms = { RecombinationAlgorithm::Simple, RecombinationAlgorithm::Blossom, RecombinationAlgorithm::SimpleFullQuad,
                                  RecombinationAlgorithm::BlossomFullQuad };
    c.exportFormats = { "msh", "vtk", "unv", "inp", "bdf", "mesh" };
    c.importFormats = { "msh", "unv", "inp", "bdf", "mesh", "vtk" };
    return c;
}

GenerationResult generate(const MeshGeometry& geometry, const MeshSettingsSet& settings, const GenerationOptions& options)
{
    QElapsedTimer timer;
    timer.start();
    GenerationResult r;
    auto& rec = r.record;
    rec.mesher = "Gmsh";
    rec.profile = settings.profile;
    rec.settingsHash = MeshGenerator::settingsHash(settings);
    rec.geometryHash = geometry.hash;
    std::string why;
    if (!dllLoaded(&why))
    {
        rec.errors.push_back(why);
        return r;
    }
    if (settings.global.minSize > settings.global.maxSize)
        rec.errors.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "taille minimale supérieure à la taille maximale").toStdString());
    const MeshEstimate est = MeshGenerator::estimate(geometry, settings);
    if (est.elements() > std::size_t(settings.global.maxElements))
        rec.errors.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine",
                                                         "estimation de %1 éléments, au-delà de la limite de %2 : augmenter la taille des éléments ou la limite")
                                 .arg(QString::number(est.elements()), QString::number(settings.global.maxElements))
                                 .toStdString());
    if (!rec.errors.empty()) return r;
    auto cancelled = [&] { return options.cancelled && options.cancelled(); };
    auto progress = [&](int p, const char* what) {
        if (options.progress) options.progress(p, QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", what).toStdString());   // what : QT_TRANSLATE_NOOP
    };
    try
    {
        Session session;
        gmsh::option::getString("General.Version", rec.mesherVersion);
        gmsh::model::add("tsa");
        Builder b(geometry, settings, options, rec);
        progress(5, QT_TRANSLATE_NOOP("TSA::Mesh::GmshMeshEngine", "géométrie"));
        b.buildMembers();
        b.buildSurfaces();
        b.buildVolumes();
        gmsh::model::geo::synchronize();
        b.embedAndCheck();
        b.options();
        b.transfinite();
        b.fields();
        if (cancelled()) r.cancelled = true;
        bool any2 = !b.surfaceTag.empty() || (settings.global.boundaryElements && !b.volumeTags.empty());
        if (!r.cancelled)
        {
            progress(15, QT_TRANSLATE_NOOP("TSA::Mesh::GmshMeshEngine", "maillage 1D"));
            gmsh::model::mesh::generate(1);
        }
        if (!r.cancelled && cancelled()) r.cancelled = true;
        if (!r.cancelled && (any2 || !b.volumeTags.empty()))
        {
            progress(35, QT_TRANSLATE_NOOP("TSA::Mesh::GmshMeshEngine", "maillage 2D"));
            gmsh::model::mesh::generate(2);
        }
        if (!r.cancelled && cancelled()) r.cancelled = true;
        if (!r.cancelled && !b.volumeTags.empty())
        {
            progress(65, QT_TRANSLATE_NOOP("TSA::Mesh::GmshMeshEngine", "maillage 3D"));
            gmsh::model::mesh::generate(3);
        }
        if (!r.cancelled && cancelled()) r.cancelled = true;
        if (!r.cancelled)
        {
            progress(85, QT_TRANSLATE_NOOP("TSA::Mesh::GmshMeshEngine", "extraction"));
            // Groupes physiques (noms = références TSA) créés avant l'extraction : leurs étiquettes Gmsh sont
            // conservées dans FeGroup ; sans effet sur le maillage.
            b.physicalGroups();
            b.extract(r.mesh);
            if (!options.writeMshTo.empty()) gmsh::write(options.writeMshTo);
        }
        for (const auto& l : session.log())
            if (l.rfind("Warning", 0) == 0 || l.rfind("Error", 0) == 0) rec.engineLog.push_back(l);
    }
    catch (const std::exception& e)
    {
        rec.errors.push_back(std::string("Gmsh : ") + e.what());
    }
    if (r.cancelled)
        rec.errors.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "génération annulée par l'utilisateur (entre deux étapes de Gmsh)").toStdString());
    if (r.mesh.elements.size() > std::size_t(settings.global.maxElements))
        rec.errors.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "maillage de %1 éléments, au-delà de la limite de %2")
                                 .arg(QString::number(r.mesh.elements.size()), QString::number(settings.global.maxElements))
                                 .toStdString());
    const auto& g = settings.global;
    auto num = [](double v) { return QString::number(v, 'g', 6).toStdString(); };
    rec.parameters["taille effective"] = num(g.effectiveSize()) + " m";
    rec.parameters["tailles min / max"] = num(g.minSize) + " / " + num(g.maxSize) + " m";
    rec.parameters["ordre"] = g.order == ElementOrder::Second ? "2 (Serendip)" : "1";
    rec.parameters["algorithme 2D"] = algorithm2DName(g.algorithm2D);
    rec.parameters["algorithme 3D"] = algorithm3DName(g.algorithm3D);
    rec.parameters["éléments de volume"] = g.volumeElement == VolumeElement::Hexahedra ? "hexaèdres (transfinis)" : "tétraèdres";
    rec.parameters["recombinaison"] = g.recombine ? recombinationName(g.recombination) : "non";
    rec.parameters["paramètres locaux"] = std::to_string(settings.locals.size());
    rec.generated = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    rec.durationMs = double(timer.elapsed());
    r.ok = !r.cancelled && rec.errors.empty() && !r.mesh.empty();
    if (!r.cancelled && rec.errors.empty() && r.mesh.empty())
        rec.errors.push_back(QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "aucun objet à mailler pour les dimensions activées").toStdString());
    return r;
}

bool exportMesh(const FeMesh& mesh, const std::string& path, std::string* error)
{
    std::string why;
    if (!dllLoaded(&why))
    {
        if (error) *error = why;
        return false;
    }
    try
    {
        Session session;
        gmsh::model::add("export");
        // Une entité discrète par (objet, dimension, frontière) ; nœuds rattachés à la première entité qui les utilise.
        std::map<std::tuple<std::string, int, bool>, std::vector<const FeElement*>> blocks;
        for (const auto& e : mesh.elements) blocks[{ e.source, feTypeDimension(e.type), e.boundary }].push_back(&e);
        std::vector<char> placed(mesh.nodes.size() + 1, 0);
        std::map<std::string, std::map<int, std::vector<int>>> groupEntities;   // nom → dim → entités
        for (const auto& [k, els] : blocks)
        {
            const auto& [src, dim, boundary] = k;
            const int ent = gmsh::model::addDiscreteEntity(dim);
            std::vector<std::size_t> nt;
            std::vector<double> nc;
            for (const auto* e : els)
                for (int n : e->nodes)
                    if (!placed[n])
                    {
                        placed[n] = 1;
                        const auto& fn = mesh.node(n);
                        nt.push_back(std::size_t(n));
                        nc.insert(nc.end(), { fn.x, fn.y, fn.z });
                    }
            if (!nt.empty()) gmsh::model::mesh::addNodes(dim, ent, nt, nc);
            std::map<int, std::pair<std::vector<std::size_t>, std::vector<std::size_t>>> byType;
            for (const auto* e : els)
            {
                auto& t = byType[toGmshType(e->type)];
                t.first.push_back(std::size_t(e->id));
                for (int n : e->nodes) t.second.push_back(std::size_t(n));
            }
            for (const auto& [gt, data] : byType) gmsh::model::mesh::addElementsByType(ent, gt, data.first, data.second);
            groupEntities[boundary ? src + ":contour" : src][dim].push_back(ent);
        }
        // Nœuds isolés et groupes de nœuds : entités ponctuelles
        for (const auto& g : mesh.groups)
        {
            if (g.nodes.empty()) continue;
            std::vector<int> pts;
            for (int n : g.nodes)
            {
                const int ent = gmsh::model::addDiscreteEntity(0);
                if (!placed[n])
                {
                    placed[n] = 1;
                    const auto& fn = mesh.node(n);
                    gmsh::model::mesh::addNodes(0, ent, { std::size_t(n) }, { fn.x, fn.y, fn.z });
                }
                pts.push_back(ent);
            }
            gmsh::model::addPhysicalGroup(0, pts, -1, g.name);
        }
        for (const auto& [name, dims] : groupEntities)
            for (const auto& [dim, ents] : dims) gmsh::model::addPhysicalGroup(dim, ents, -1, name);
        gmsh::option::setNumber("Mesh.SaveAll", 0);
        gmsh::write(path);
        return true;
    }
    catch (const std::exception& e)
    {
        if (error) *error = std::string("Gmsh : ") + e.what();
        return false;
    }
}

bool importMesh(const std::string& path, FeMesh* out, std::vector<std::string>* warnings, std::string* error)
{
    std::string why;
    if (!dllLoaded(&why))
    {
        if (error) *error = why;
        return false;
    }
    try
    {
        Session session;
        gmsh::open(path);
        FeMesh mesh;
        std::vector<std::size_t> tags;
        std::vector<double> coord, param;
        gmsh::model::mesh::getNodes(tags, coord, param, -1, -1, true, false);
        std::unordered_map<std::size_t, int> idOf;
        for (std::size_t i = 0; i < tags.size(); ++i)
        {
            if (idOf.count(tags[i])) continue;
            FeNode n;
            n.id = int(mesh.nodes.size()) + 1;
            n.x = coord[3 * i], n.y = coord[3 * i + 1], n.z = coord[3 * i + 2];
            n.gmshTag = tags[i];
            idOf[tags[i]] = n.id;
            mesh.nodes.push_back(n);
        }
        // Source des éléments : groupe physique (référence TSA si le fichier vient de TSA), sinon entité Gmsh.
        std::map<std::pair<int, int>, std::string> entityName;
        // Groupes physiques : chaque groupe garde TOUS les éléments de ses entités (une entité peut appartenir à
        // plusieurs groupes) et son étiquette Gmsh.
        std::vector<std::pair<FeGroup, std::vector<int>>> physicalGroups; // groupe, entités (dim > 0)
        DimTags groups;
        gmsh::model::getPhysicalGroups(groups);
        for (const auto& [dim, tag] : groups)
        {
            std::string name;
            gmsh::model::getPhysicalName(dim, tag, name);
            if (name.empty()) name = "groupe " + std::to_string(dim) + "-" + std::to_string(tag);
            std::vector<int> ents;
            gmsh::model::getEntitiesForPhysicalGroup(dim, tag, ents);
            if (dim > 0)
            {
                FeGroup pg;
                pg.name = name;
                pg.dimension = dim;
                pg.gmshPhysicalDim = dim;
                pg.gmshPhysicalTag = tag;
                physicalGroups.push_back({ pg, ents });
            }
            if (dim == 0)
            {
                FeGroup g { name, 0, {}, {} };
                g.gmshPhysicalDim = 0;
                g.gmshPhysicalTag = tag;
                for (int e : ents)
                {
                    std::vector<std::size_t> t;
                    std::vector<double> c, pc;
                    gmsh::model::mesh::getNodes(t, c, pc, 0, e, true, false);
                    for (auto x : t) g.nodes.push_back(idOf[x]);
                }
                mesh.groups.push_back(g);
                continue;
            }
            for (int e : ents)
                if (!entityName.count({ dim, e })) entityName[{ dim, e }] = name;
        }
        DimTags entities;
        gmsh::model::getEntities(entities);
        std::set<int> unknown;
        std::map<std::string, std::vector<int>> groupElements;   // entités sans groupe physique
        std::map<std::pair<int, int>, std::vector<int>> entityElements;
        std::unordered_map<std::size_t, bool> seen;
        for (const auto& [dim, tag] : entities)
        {
            if (dim == 0) continue;
            std::string name = entityName.count({ dim, tag }) ? entityName[{ dim, tag }] : "importé:" + std::to_string(dim) + "-" + std::to_string(tag);
            const bool boundary = name.size() > 8 && name.compare(name.size() - 8, 8, ":contour") == 0;
            const std::string source = boundary ? name.substr(0, name.size() - 8) : name;
            std::vector<int> types;
            std::vector<std::vector<std::size_t>> et, en;
            gmsh::model::mesh::getElements(types, et, en, dim, tag);
            for (std::size_t k = 0; k < types.size(); ++k)
            {
                FeType ft;
                if (!fromGmshType(types[k], &ft))
                {
                    unknown.insert(types[k]);
                    continue;
                }
                const std::size_t nn = en[k].size() / std::max<std::size_t>(1, et[k].size());
                for (std::size_t e = 0; e < et[k].size(); ++e)
                {
                    if (seen[et[k][e]]) continue;
                    seen[et[k][e]] = true;
                    FeElement fe;
                    fe.id = int(mesh.elements.size()) + 1;
                    fe.type = ft;
                    fe.source = source;
                    fe.boundary = boundary;
                    fe.gmshTag = et[k][e];
                    fe.gmshEntityDim = dim;
                    fe.gmshEntityTag = tag;
                    for (std::size_t j = 0; j < nn; ++j) fe.nodes.push_back(idOf[en[k][e * nn + j]]);
                    entityElements[{ dim, tag }].push_back(fe.id);
                    if (!entityName.count({ dim, tag })) groupElements[name].push_back(fe.id);
                    mesh.elements.push_back(fe);
                }
            }
        }
        for (auto& [pg, ents] : physicalGroups)
        {
            for (int ent : ents)
                if (auto it = entityElements.find({ pg.dimension, ent }); it != entityElements.end())
                    pg.elements.insert(pg.elements.end(), it->second.begin(), it->second.end());
            if (!pg.elements.empty()) mesh.groups.push_back(pg);
        }
        for (const auto& [name, els] : groupElements)
            mesh.groups.push_back({ name, feTypeDimension(mesh.elements[std::size_t(els.front() - 1)].type), els, {} });
        for (const auto& e : mesh.elements)
            for (int n : e.nodes)
                if (mesh.nodes[std::size_t(n - 1)].source.empty()) mesh.nodes[std::size_t(n - 1)].source = e.source;
        if (!unknown.empty() && warnings)
        {
            std::string t;
            for (int u : unknown) t += " " + std::to_string(u);
            warnings->push_back(
                QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "types d'éléments non pris en charge par TSA, ignorés :%1").arg(QString::fromStdString(t)).toStdString());
        }
        if (mesh.elements.empty())
        {
            if (error) *error = QCoreApplication::translate("TSA::Mesh::GmshMeshEngine", "aucun élément pris en charge dans le fichier").toStdString();
            return false;
        }
        *out = std::move(mesh);
        return true;
    }
    catch (const std::exception& e)
    {
        if (error) *error = std::string("Gmsh : ") + e.what();
        return false;
    }
}

#endif

} // namespace TSA::Mesh::GmshMeshEngine
