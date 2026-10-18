#include "MeshGenerator.h"

#include "Gmsh/GmshMeshEngine.h"
#include "MeshProject.h"
#include "MeshSizing.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace TSA::Mesh
{

// --- Types ------------------------------------------------------------------------------------------

double finenessFactor(Fineness f)
{
    switch (f)
    {
    case Fineness::VeryCoarse: return 2.0;
    case Fineness::Coarse: return 1.5;
    case Fineness::Standard: return 1.0;
    case Fineness::Fine: return 0.5;
    case Fineness::VeryFine: return 0.25;
    case Fineness::Custom: return 1.0;
    }
    return 1.0;
}

double GlobalMeshSettings::effectiveSize() const
{
    const double h = globalSize * finenessFactor(fineness) * (refinementFactor > 0 ? refinementFactor : 1.0);
    return std::clamp(h, std::max(minSize, 1e-6), std::max(maxSize, minSize));
}

bool LocalMeshSetting::appliesTo(const std::string& ref) const { return std::find(targets.begin(), targets.end(), ref) != targets.end(); }

const char* feTypeName(FeType t)
{
    switch (t)
    {
    case FeType::Line2: return "line2";
    case FeType::Line3: return "line3";
    case FeType::Tri3: return "tri3";
    case FeType::Tri6: return "tri6";
    case FeType::Quad4: return "quad4";
    case FeType::Quad8: return "quad8";
    case FeType::Hex8: return "hex8";
    case FeType::Hex20: return "hex20";
    case FeType::Tet4: return "tet4";
    case FeType::Tet10: return "tet10";
    }
    return "?";
}
int feTypeDimension(FeType t)
{
    switch (t)
    {
    case FeType::Line2:
    case FeType::Line3: return 1;
    case FeType::Hex8:
    case FeType::Hex20:
    case FeType::Tet4:
    case FeType::Tet10: return 3;
    default: return 2;
    }
}
int feTypeCornerCount(FeType t)
{
    switch (t)
    {
    case FeType::Line2:
    case FeType::Line3: return 2;
    case FeType::Tri3:
    case FeType::Tri6: return 3;
    case FeType::Quad4:
    case FeType::Quad8:
    case FeType::Tet4:
    case FeType::Tet10: return 4;
    default: return 8;
    }
}
bool feTypeIsSecondOrder(FeType t)
{
    return t == FeType::Line3 || t == FeType::Tri6 || t == FeType::Quad8 || t == FeType::Hex20 || t == FeType::Tet10;
}

std::size_t FeMesh::countOf(FeType t) const
{
    return std::size_t(std::count_if(elements.begin(), elements.end(), [t](const FeElement& e) { return !e.boundary && e.type == t; }));
}
std::size_t FeMesh::countByDimension(int dim) const
{
    return std::size_t(std::count_if(elements.begin(), elements.end(), [dim](const FeElement& e) { return !e.boundary && feTypeDimension(e.type) == dim; }));
}
std::vector<double> FeMesh::memberInteriorPositions(const std::string& ref) const
{
    std::vector<const FeElement*> els;
    for (const auto& e : elements)
        if (e.source == ref && feTypeDimension(e.type) == 1 && !e.boundary) els.push_back(&e);
    if (els.empty()) return {};
    std::map<int, int> degree;
    for (const auto* e : els) ++degree[e->nodes[0]], ++degree[e->nodes[1]];
    std::vector<int> ends;
    for (const auto& [n, d] : degree)
        if (d == 1) ends.push_back(n);
    if (ends.size() != 2) return {};
    const FeNode &A = node(ends[0]), &B = node(ends[1]);
    const P3 a { A.x, A.y, A.z }, ab = P3 { B.x, B.y, B.z } - a;
    const double L2 = ab.dot(ab);
    std::vector<double> out;
    for (const auto& [n, d] : degree)
        if (d == 2)
        {
            const FeNode& N = node(n);
            out.push_back((P3 { N.x, N.y, N.z } - a).dot(ab) / L2);
        }
    // Orientation : positions comptées depuis le nœud TSA de départ de la barre si les extrémités sont reliées.
    std::sort(out.begin(), out.end());
    return out;
}

const char* algorithm2DName(Algorithm2D a)
{
    switch (a)
    {
    case Algorithm2D::Automatic: return "Automatique (Gmsh)";
    case Algorithm2D::MeshAdapt: return "MeshAdapt";
    case Algorithm2D::Delaunay: return "Delaunay";
    case Algorithm2D::FrontalDelaunay: return "Frontal-Delaunay";
    case Algorithm2D::FrontalDelaunayQuads: return "Frontal-Delaunay pour quadrangles";
    case Algorithm2D::PackingParallelograms: return "Remplissage de parallélogrammes";
    case Algorithm2D::QuasiStructuredQuad: return "Quadrangles quasi structurés";
    }
    return "";
}
const char* algorithm3DName(Algorithm3D a)
{
    switch (a)
    {
    case Algorithm3D::Delaunay: return "Delaunay";
    case Algorithm3D::Frontal: return "Frontal (Netgen)";
    case Algorithm3D::HXT: return "HXT (parallèle)";
    }
    return "";
}
const char* recombinationName(RecombinationAlgorithm a)
{
    switch (a)
    {
    case RecombinationAlgorithm::Simple: return "Simple";
    case RecombinationAlgorithm::Blossom: return "Blossom";
    case RecombinationAlgorithm::SimpleFullQuad: return "Simple, quadrangles seuls";
    case RecombinationAlgorithm::BlossomFullQuad: return "Blossom, quadrangles seuls";
    }
    return "";
}

// --- Tailles ----------------------------------------------------------------------------------------

const LocalMeshSetting* localSettingFor(const MeshSettingsSet& s, const std::string& ref)
{
    for (auto it = s.locals.rbegin(); it != s.locals.rend(); ++it)
        if (it->appliesTo(ref)) return &*it;
    return nullptr;
}

double effectiveSizeFor(const MeshSettingsSet& s, const std::string& ref)
{
    const auto* l = localSettingFor(s, ref);
    const double h = l && l->size ? *l->size : s.global.effectiveSize();
    return std::clamp(h, std::max(s.global.minSize, 1e-6), std::max(s.global.maxSize, s.global.minSize));
}

SizeEvaluator::SizeEvaluator(const MeshSettingsSet& s, const MeshGeometry& g)
    : m_settings(s)
{
    auto point = [&](const std::string& name, const P3& c, double r, std::optional<double> size, std::optional<double> factor,
                     const std::vector<std::string>& targets) {
        ResolvedZone z;
        z.name = name;
        z.a = z.b = c;
        z.radius = std::max(0.0, r);
        z.size = size;
        z.factor = factor;
        z.targets = targets;
        m_zones.push_back(z);
    };
    for (const auto& z : s.zones)
        switch (z.kind)
        {
        case RefinementZone::Kind::Point: point(z.name, { z.x, z.y, z.z }, z.radius, z.size, z.factor, z.targets); break;
        case RefinementZone::Kind::Node:
            if (auto it = g.tsaNodes.find(z.nodeId); it != g.tsaNodes.end()) point(z.name, it->second, z.radius, z.size, z.factor, z.targets);
            break;
        case RefinementZone::Kind::Support:
            for (const auto& p : g.points)
                if (p.kind == CharacteristicPoint::Kind::Support && (z.nodeId == 0 || z.nodeId == p.nodeId)) point(z.name, p.p, z.radius, z.size, z.factor, z.targets);
            break;
        case RefinementZone::Kind::Load:
            for (const auto& p : g.points)
                if (p.kind == CharacteristicPoint::Kind::PointLoad) point(z.name, p.p, z.radius, z.size, z.factor, z.targets);
            break;
        }
    auto autoZones = [&](const AutoRefinement& a, CharacteristicPoint::Kind kind, const char* name) {
        if (!a.enabled) return;
        for (const auto& p : g.points)
            if (p.kind == kind) point(name, p.p, a.radius, a.size, std::nullopt, {});
    };
    autoZones(s.global.nearSupports, CharacteristicPoint::Kind::Support, "auto:appuis");
    autoZones(s.global.nearPointLoads, CharacteristicPoint::Kind::PointLoad, "auto:charges ponctuelles");
    autoZones(s.global.nearSectionChanges, CharacteristicPoint::Kind::SectionChange, "auto:changements de section");
    autoZones(s.global.nearConnections, CharacteristicPoint::Kind::Connection, "auto:connexions");
    if (s.global.nearOpenings.enabled)
        for (const auto& sf : g.surfaces)
            for (const auto& hole : sf.holes)
                for (std::size_t i = 0; i < hole.size(); ++i)
                {
                    ResolvedZone z;
                    z.name = "auto:ouvertures";
                    z.segment = true;
                    z.a = hole[i];
                    z.b = hole[(i + 1) % hole.size()];
                    z.radius = s.global.nearOpenings.radius;
                    z.size = s.global.nearOpenings.size;
                    z.targets = { sf.ref };
                    m_zones.push_back(z);
                }
}

double SizeEvaluator::clamp(double h) const
{
    return std::clamp(h, std::max(m_settings.global.minSize, 1e-6), std::max(m_settings.global.maxSize, m_settings.global.minSize));
}
double SizeEvaluator::base(const std::string& ref) const { return effectiveSizeFor(m_settings, ref); }
bool SizeEvaluator::zoneApplies(const ResolvedZone& z, const std::string& ref) const
{
    return z.targets.empty() || std::find(z.targets.begin(), z.targets.end(), ref) != z.targets.end();
}
double SizeEvaluator::zoneSize(const ResolvedZone& z, const std::string& ref) const
{
    return clamp(z.size ? *z.size : base(ref) * z.factor.value_or(1.0));
}
double SizeEvaluator::at(const P3& p, const std::string& ref) const
{
    const double b = base(ref);
    double h = b;
    const double rate = std::max(0.05, m_settings.global.transitionRate);
    for (const auto& z : m_zones)
    {
        if (!zoneApplies(z, ref)) continue;
        const double hz = zoneSize(z, ref);
        if (hz >= h) continue;
        const double d = z.segment ? distanceToSegment(p, z.a, z.b) : distance(p, z.a);
        h = std::min(h, d <= z.radius ? hz : hz + rate * (d - z.radius));
    }
    if (const auto* l = localSettingFor(m_settings, ref); l && l->maxLength) h = std::min(h, *l->maxLength);
    return clamp(h);
}
bool SizeEvaluator::refined(const std::string& ref, const std::vector<P3>& samples) const
{
    const double b = base(ref);
    for (const auto& p : samples)
        if (at(p, ref) < b * 0.999) return true;
    return false;
}
int SizeEvaluator::divisions(const P3& a, const P3& b, const std::vector<std::string>& refs) const
{
    const double L = distance(a, b);
    if (L <= 0) return 1;
    const int samples = 100;
    double integral = 0;
    for (int i = 0; i < samples; ++i)
    {
        const P3 p = a + (b - a) * ((i + 0.5) / samples);
        double h = std::numeric_limits<double>::infinity();
        for (const auto& r : refs) h = std::min(h, at(p, r));
        integral += (L / samples) / std::max(h, 1e-9);
    }
    return std::max(1, int(std::ceil(integral - 1e-6)));
}

// --- Façade -----------------------------------------------------------------------------------------

MesherCapabilities meshEngineCapabilities() { return GmshMeshEngine::capabilities(); }

std::string MeshGenerator::settingsHash(const MeshSettingsSet& s)
{
    MeshProject p;
    p.settings = s;
    p.settings.profile.clear();   // le nom du profil n'influence pas le maillage
    const QByteArray bytes = QJsonDocument(p.toJson(false).value("settings").toObject()).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString();
}

GenerationResult MeshGenerator::generate(const MeshGeometry& geometry, const MeshSettingsSet& settings, const GenerationOptions& options)
{
    return GmshMeshEngine::generate(geometry, settings, options);
}

bool MeshGenerator::exportMesh(const FeMesh& mesh, const std::string& path, std::string* error) { return GmshMeshEngine::exportMesh(mesh, path, error); }

bool MeshGenerator::importMesh(const std::string& path, FeMesh* mesh, std::vector<std::string>* warnings, std::string* error)
{
    return GmshMeshEngine::importMesh(path, mesh, warnings, error);
}

MeshEstimate MeshGenerator::estimate(const MeshGeometry& g, const MeshSettingsSet& s)
{
    MeshEstimate e;
    const SizeEvaluator ev(s, g);
    if (s.global.mesh1D)
        for (const auto& m : g.members)
        {
            const auto* l = localSettingFor(s, m.ref);
            const std::size_t n = l && l->divisions ? std::size_t(*l->divisions) : std::size_t(ev.divisions(m.a, m.b, { m.ref }));
            e.elements1D += n;
            e.nodes += n - 1;
        }
    if (s.global.mesh2D)
        for (const auto& sf : g.surfaces)
        {
            double sum = 0;
            int n = 0;
            P3 c;
            for (const auto& p : sf.outer) c = c + p, sum += ev.at(p, sf.ref), ++n;
            c = c * (1.0 / double(sf.outer.size()));
            sum += 2 * ev.at(c, sf.ref), n += 2;
            const double h = sum / n;
            const double cells = std::max(1.0, sf.area / (h * h));
            const auto* l = localSettingFor(s, sf.ref);
            const bool quads = s.global.recombine || (l && l->shape == SurfaceShape::Quadrilaterals) ||
                               (s.global.preferStructured && sf.outer.size() <= 4 && sf.holes.empty());
            e.elements2D += std::size_t(std::ceil(quads ? cells : 2.3 * cells));
            e.nodes += std::size_t(cells);
        }
    if (s.global.mesh3D)
        for (const auto& v : g.volumes)
        {
            const P3 d = v.max - v.min;
            const double h = ev.base(v.ref);
            const double cells = std::max(1.0, d.x * d.y * d.z / (h * h * h));
            e.elements3D += std::size_t(std::ceil(s.global.volumeElement == VolumeElement::Hexahedra ? cells : 5.5 * cells));
            e.nodes += std::size_t(cells);
        }
    e.nodes += g.tsaNodes.size();
    e.notes.push_back(QCoreApplication::translate("TSA::Mesh::MeshGenerator", "estimation indicative (longueurs, aires, volumes et tailles demandées) ; le nombre réel est donné par Gmsh à la génération").toStdString());
    return e;
}

} // namespace TSA::Mesh
