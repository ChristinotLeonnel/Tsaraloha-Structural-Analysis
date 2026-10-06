#include "IfcExporter.h"

#include "IfcExportContext.h"
#include "IfcGeometryMapper.h"
#include "IfcMapper.h"
#include "IfcPropertyMapper.h"
#include "IfcRelationshipMapper.h"
#include "../Core/BimModel.h"
#include "../Core/IfcGuid.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"

#include <QFile>
#include <QFileInfo>

#include <cmath>
#include <map>

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;
using TSA::Model::ElementKind;

std::string IfcExportReport::summary() const
{
    if (!ok) return "Export IFC échoué : " + error;
    return std::to_string(products) + " produit(s), " + std::to_string(storeys) + " étage(s), "
        + std::to_string(analyticalMembers) + " élément(s) analytique(s), " + std::to_string(analyticalNodes)
        + " nœud(s) analytique(s), " + std::to_string(entities) + " entités STEP";
}

namespace
{
/// Matériau / section du premier élément analytique (association de matériau du produit).
struct MaterialInfo
{
    const TSA::Model::Material* material = nullptr;
    const TSA::Model::Section* section = nullptr;   ///< non nul pour les éléments linéaires
};

MaterialInfo materialOf(const TSA::Model::Model& m, const AnalyticalRef& r)
{
    switch (r.kind)
    {
    case ElementKind::Beam: if (auto* b = m.getBeam(r.id)) return { &b->material(), &b->section() }; break;
    case ElementKind::Column: if (auto* c = m.getColumn(r.id)) return { &c->material(), &c->section() }; break;
    case ElementKind::TrussMember:
        if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end()) return { &it->second.material(), &it->second.section() };
        break;
    case ElementKind::Cable:
        if (auto it = m.cables().find(r.id); it != m.cables().end()) return { &it->second.material(), nullptr };
        break;
    case ElementKind::Slab:
        if (auto it = m.slabs().find(r.id); it != m.slabs().end()) return { &it->second.material(), nullptr };
        break;
    case ElementKind::Wall:
        if (auto it = m.walls().find(r.id); it != m.walls().end()) return { &it->second.material(), nullptr };
        break;
    case ElementKind::Foundation:
        if (auto it = m.foundations().find(r.id); it != m.foundations().end()) return { &it->second.material(), nullptr };
        break;
    default: break;
    }
    return {};
}

std::string stiffness(TSA::Model::DOFState s, double k, const char* measure)
{
    switch (s)
    {
    case TSA::Model::DOFState::Fixed: return "IFCBOOLEAN(.T.)";
    case TSA::Model::DOFState::Spring: return std::string(measure) + "(" + W::real(k) + ")";
    default: return "IFCBOOLEAN(.F.)";
    }
}

class Exporter
{
public:
    Exporter(const TSA::Model::Model& model, const IfcExportOptions& opt)
        : m_model(model), m_opt(opt), m_ctx { m_w, model, model.bim() }, m_mapper(m_ctx), m_geom(m_ctx, m_mapper),
          m_props(m_ctx), m_rels(m_ctx)
    {
    }

    IfcExportReport run(std::string& out)
    {
        writeContextAndProject();
        writeSpatial();
        writeProducts();
        if (m_opt.includeAnalyticalModel) writeAnalytical();
        m_report.relationships = m_rels.write();

        StepHeader h;
        h.fileName = m_opt.fileName.empty() ? "model.ifc" : m_opt.fileName;
        h.author = m_opt.author;
        h.organization = m_opt.organization;
        h.timeStamp = m_opt.timeStamp;
        out = m_w.document(h);
        m_report.entities = static_cast<int>(m_w.count());
        m_report.warnings = m_ctx.warnings;
        m_report.ok = true;
        return m_report;
    }

private:
    const SpatialStructure& spatial() const { return m_ctx.bim.spatial(); }

    void writeContextAndProject()
    {
        m_identity = m_geom.placement3({ 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 });
        m_ctx.modelContext = m_w.add("IFCGEOMETRICREPRESENTATIONCONTEXT", "$,'Model',3," + W::real(1e-5) + "," + W::ref(m_identity) + ",$");
        m_ctx.bodyContext = m_w.add("IFCGEOMETRICREPRESENTATIONSUBCONTEXT", "'Body','Model',*,*,*,*," + W::ref(m_ctx.modelContext) + ",$,.MODEL_VIEW.,$");
        m_ctx.axisContext = m_w.add("IFCGEOMETRICREPRESENTATIONSUBCONTEXT", "'Axis','Model',*,*,*,*," + W::ref(m_ctx.modelContext) + ",$,.GRAPH_VIEW.,$");

        // Unités SI du fichier = unités internes de TSA (src/Core/Units.h)
        std::vector<int> units = {
            m_w.add("IFCSIUNIT", "*,.LENGTHUNIT.,$,.METRE."),
            m_w.add("IFCSIUNIT", "*,.AREAUNIT.,$,.SQUARE_METRE."),
            m_w.add("IFCSIUNIT", "*,.VOLUMEUNIT.,$,.CUBIC_METRE."),
            m_w.add("IFCSIUNIT", "*,.PLANEANGLEUNIT.,$,.RADIAN."),
            m_w.add("IFCSIUNIT", "*,.MASSUNIT.,.KILO.,.GRAM."),
            m_w.add("IFCSIUNIT", "*,.FORCEUNIT.,$,.NEWTON."),
            m_w.add("IFCSIUNIT", "*,.PRESSUREUNIT.,$,.PASCAL."),
            m_w.add("IFCSIUNIT", "*,.TIMEUNIT.,$,.SECOND."),
            m_w.add("IFCSIUNIT", "*,.THERMODYNAMICTEMPERATUREUNIT.,$,.KELVIN."),
        };
        const int ua = m_w.add("IFCUNITASSIGNMENT", W::refs(units));
        const auto& p = spatial().project;
        const std::string name = m_opt.projectName.empty() ? p.name : m_opt.projectName;
        m_project = m_w.add("IFCPROJECT", W::str(p.globalId) + ",$," + W::str(name) + "," + W::optStr(p.description) + ",$,$,$,"
                                             + W::refs({ m_ctx.modelContext }) + "," + W::ref(ua));
    }

    void writeSpatial()
    {
        const auto& s = spatial();
        m_sitePlacement = m_geom.localPlacement(0, m_identity);
        m_site = m_w.add("IFCSITE", W::str(s.site.globalId) + ",$," + W::str(s.site.name) + "," + W::optStr(s.site.description)
                                       + ",$," + W::ref(m_sitePlacement) + ",$,$,.ELEMENT.,$,$,$,$,$");
        m_buildingPlacement = m_geom.localPlacement(m_sitePlacement, m_identity);
        m_building = m_w.add("IFCBUILDING", W::str(s.building.globalId) + ",$," + W::str(s.building.name) + ","
                                               + W::optStr(s.building.description) + ",$," + W::ref(m_buildingPlacement) + ",$,$,.ELEMENT.,$,$,$");
        m_rels.aggregate(m_project, m_site);
        m_rels.aggregate(m_site, m_building);

        if (const auto* lm = m_model.levelManager())
            for (const auto& lvl : lm->levels())
            {
                auto g = s.storeyGlobalIds.find(lvl.id);
                const std::string guid = g != s.storeyGlobalIds.end() ? g->second : IfcGuid::create();
                // Placement de l'étage à l'origine : les produits gardent les coordonnées du modèle ;
                // la cote est portée par l'attribut Elevation.
                const int pl = m_geom.localPlacement(m_buildingPlacement, m_identity);
                const int st = m_w.add("IFCBUILDINGSTOREY", W::str(guid) + ",$," + W::str(lvl.name) + ",$,$," + W::ref(pl)
                                                              + ",$,$,.ELEMENT.," + W::real(lvl.elevation));
                m_storeys[lvl.id] = { st, pl };
                m_rels.aggregate(m_building, st);
                ++m_report.storeys;
            }
    }

    void writeProducts()
    {
        std::map<std::string, int> classificationSources;
        std::map<std::string, int> classificationRefs;
        for (const auto& [id, e] : m_ctx.bim.elements())
        {
            const std::string entity = IfcMapper::stepEntity(e.category);
            if (entity.empty() || e.analytical.empty()) continue;

            const std::string storey = m_ctx.bim.resolvedStorey(m_model, e);
            auto st = m_storeys.find(storey);
            const int container = st != m_storeys.end() ? st->second.first : m_building;
            const int relTo = st != m_storeys.end() ? st->second.second : m_buildingPlacement;
            const int placement = m_geom.localPlacement(relTo, m_identity);
            const int shape = m_geom.productShape(e);
            if (!shape) m_ctx.warnings.push_back(defaultProductName(e) + " : géométrie non exportée (dimensions nulles ou nœuds manquants).");

            const std::string pdt = IfcMapper::predefinedType(e.category, e.predefinedType);
            std::string args = W::str(e.globalId) + ",$," + W::str(e.name.empty() ? defaultProductName(e) : e.name) + ","
                + W::optStr(e.description) + "," + W::optStr(e.objectType) + "," + W::ref(placement) + ","
                + (shape ? W::ref(shape) : "$") + "," + W::optStr((e.tag.empty() ? defaultProductTag(e) : e.tag)) + "," + W::enumeration(pdt);
            if (e.category == BimCategory::Pile) args += ",$";   // ConstructionType (déprécié en 4.3)
            const int product = m_w.add(entity, args);
            m_productIds[id] = product;
            m_rels.contain(container, product);
            ++m_report.products;

            const MaterialInfo mi = materialOf(m_model, e.analytical.front());
            if (mi.material)
                m_rels.associateMaterial(mi.section ? m_mapper.materialProfileSet(*mi.material, *mi.section) : m_mapper.material(*mi.material), product);
            for (int ps : m_props.write(e)) m_rels.defineByProperties(ps, product);

            for (const auto& c : e.classifications)
            {
                int& src = classificationSources[c.system];
                if (!src) src = m_w.add("IFCCLASSIFICATION", "$,$,$," + W::str(c.system) + ",$,$,$");
                int& ref = classificationRefs[c.system + "#" + c.identification];
                if (!ref)
                    ref = m_w.add("IFCCLASSIFICATIONREFERENCE", "$," + W::optStr(c.identification) + "," + W::optStr(c.name) + "," + W::ref(src) + ",$,$");
                m_rels.classify(ref, product);
            }
        }
    }

    int vertex(int nodeId)
    {
        if (auto it = m_vertices.find(nodeId); it != m_vertices.end()) return it->second;
        const auto* n = m_model.getNode(nodeId);
        if (!n) return 0;
        return m_vertices[nodeId] = m_w.add("IFCVERTEXPOINT", W::ref(m_geom.point({ n->x(), n->y(), n->z() })));
    }

    int connection(int nodeId)
    {
        if (auto it = m_connections.find(nodeId); it != m_connections.end()) return it->second;
        const auto* n = m_model.getNode(nodeId);
        const int v = vertex(nodeId);
        if (!n || !v) return 0;
        const auto& s = n->support();
        std::string cond = "$";
        using D = TSA::Model::DOFState;
        if (s.tx() != D::Free || s.ty() != D::Free || s.tz() != D::Free || s.rx() != D::Free || s.ry() != D::Free || s.rz() != D::Free)
        {
            cond = W::ref(m_w.add("IFCBOUNDARYNODECONDITION",
                                  W::str("Appui N" + std::to_string(nodeId)) + ","
                                      + stiffness(s.tx(), s.kx(), "IFCLINEARSTIFFNESSMEASURE") + ","
                                      + stiffness(s.ty(), s.ky(), "IFCLINEARSTIFFNESSMEASURE") + ","
                                      + stiffness(s.tz(), s.kz(), "IFCLINEARSTIFFNESSMEASURE") + ","
                                      + stiffness(s.rx(), s.krx(), "IFCROTATIONALSTIFFNESSMEASURE") + ","
                                      + stiffness(s.ry(), s.kry(), "IFCROTATIONALSTIFFNESSMEASURE") + ","
                                      + stiffness(s.rz(), s.krz(), "IFCROTATIONALSTIFFNESSMEASURE")));
        }
        const int topo = m_w.add("IFCTOPOLOGYREPRESENTATION", W::ref(m_ctx.modelContext) + ",'Reference','Vertex'," + W::refs({ v }));
        const int shape = m_w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs({ topo }));
        std::string guid = m_ctx.bim.analyticalGlobalId({ ElementKind::Node, nodeId });
        if (guid.empty()) guid = IfcGuid::create();
        const int c = m_w.add("IFCSTRUCTURALPOINTCONNECTION", W::str(guid) + ",$," + W::str("N" + std::to_string(nodeId)) + ",$,$,"
                                                               + W::ref(m_ctx.analysisPlacement) + "," + W::ref(shape) + "," + cond + ",$");
        m_analyticalItems.push_back(c);
        ++m_report.analyticalNodes;
        return m_connections[nodeId] = c;
    }

    int curveMember(const AnalyticalRef& r, int na, int nb, const char* type, double rotationDeg)
    {
        const int ca = connection(na), cb = connection(nb);
        if (!ca || !cb || na == nb) return 0;
        const auto* a = m_model.getNode(na);
        const auto* b = m_model.getNode(nb);
        Vec3 x, y, z;
        IfcGeometryMapper::barFrame({ a->x(), a->y(), a->z() }, { b->x(), b->y(), b->z() }, rotationDeg, x, y, z);
        const int edge = m_w.add("IFCEDGE", W::ref(vertex(na)) + "," + W::ref(vertex(nb)));
        const int topo = m_w.add("IFCTOPOLOGYREPRESENTATION", W::ref(m_ctx.modelContext) + ",'Reference','Edge'," + W::refs({ edge }));
        const int shape = m_w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs({ topo }));
        const int m = m_w.add("IFCSTRUCTURALCURVEMEMBER", W::str(guidOf(r)) + ",$," + W::str(r.label()) + ",$,$,"
                                                          + W::ref(m_ctx.analysisPlacement) + "," + W::ref(shape) + ","
                                                          + W::enumeration(type) + "," + W::ref(m_geom.direction(y)));
        m_rels.connect(m, ca);
        m_rels.connect(m, cb);
        return m;
    }

    int surfaceMember(const AnalyticalRef& r, const std::vector<Vec3>& pts, const std::vector<int>& nodeIds, double thickness)
    {
        if (pts.size() < 3) return 0;
        std::vector<int> loop;
        for (const auto& p : pts) loop.push_back(m_geom.point(p));
        const int poly = m_w.add("IFCPOLYLOOP", W::refs(loop));
        const int bound = m_w.add("IFCFACEOUTERBOUND", W::ref(poly) + ",.T.");
        // Plan porteur : premier sommet, normale de Newell
        Vec3 n { 0, 0, 0 };
        for (std::size_t i = 0; i < pts.size(); ++i)
        {
            const Vec3& p = pts[i];
            const Vec3& q = pts[(i + 1) % pts.size()];
            n[0] += (p[1] - q[1]) * (p[2] + q[2]);
            n[1] += (p[2] - q[2]) * (p[0] + q[0]);
            n[2] += (p[0] - q[0]) * (p[1] + q[1]);
        }
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 1e-12) return 0;
        n = { n[0] / len, n[1] / len, n[2] / len };
        Vec3 x { pts[1][0] - pts[0][0], pts[1][1] - pts[0][1], pts[1][2] - pts[0][2] };
        const double lx = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        x = { x[0] / lx, x[1] / lx, x[2] / lx };
        const int plane = m_w.add("IFCPLANE", W::ref(m_geom.placement3(pts[0], n, x)));
        const int face = m_w.add("IFCFACESURFACE", W::refs({ bound }) + "," + W::ref(plane) + ",.T.");
        const int topo = m_w.add("IFCTOPOLOGYREPRESENTATION", W::ref(m_ctx.modelContext) + ",'Reference','Face'," + W::refs({ face }));
        const int shape = m_w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs({ topo }));
        const int m = m_w.add("IFCSTRUCTURALSURFACEMEMBER", W::str(guidOf(r)) + ",$," + W::str(r.label()) + ",$,$,"
                                                            + W::ref(m_ctx.analysisPlacement) + "," + W::ref(shape) + ",.SHELL.,"
                                                            + W::real(thickness));
        for (int nid : nodeIds)
            if (int c = connection(nid)) m_rels.connect(m, c);
        return m;
    }

    std::string guidOf(const AnalyticalRef& r) const
    {
        const std::string g = m_ctx.bim.analyticalGlobalId(r);
        return g.empty() ? IfcGuid::create() : g;
    }

    void writeAnalytical()
    {
        const auto& s = spatial();
        m_ctx.analysisPlacement = m_geom.localPlacement(0, m_identity);
        const int model = m_w.add("IFCSTRUCTURALANALYSISMODEL", W::str(s.analysisModel.globalId) + ",$," + W::str(s.analysisModel.name)
                                                                    + ",$,$,.LOADING_3D.,$,$,$," + W::ref(m_ctx.analysisPlacement));
        const auto& m = m_model;
        for (const auto& [id, e] : m_ctx.bim.elements())
        {
            std::vector<int> members;
            for (const auto& r : e.analytical)
            {
                int item = 0;
                switch (r.kind)
                {
                case ElementKind::Beam:
                    if (auto* b = m.getBeam(r.id)) item = curveMember(r, b->startNodeId(), b->endNodeId(), "RIGID_JOINED_MEMBER", b->rotation());
                    break;
                case ElementKind::Column:
                    if (auto* c = m.getColumn(r.id)) item = curveMember(r, c->startNodeId(), c->endNodeId(), "RIGID_JOINED_MEMBER", c->rotation());
                    break;
                case ElementKind::TrussMember:
                    if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end())
                        item = curveMember(r, it->second.startNodeId(), it->second.endNodeId(), "PIN_JOINED_MEMBER", 0.0);
                    break;
                case ElementKind::Cable:
                    if (auto it = m.cables().find(r.id); it != m.cables().end())
                        item = curveMember(r, it->second.startNodeId(), it->second.endNodeId(), "CABLE", 0.0);
                    break;
                case ElementKind::Slab:
                    if (auto it = m.slabs().find(r.id); it != m.slabs().end())
                    {
                        std::vector<Vec3> pts;
                        for (int nid : it->second.nodeIds())
                            if (const auto* n = m.getNode(nid)) pts.push_back({ n->x(), n->y(), n->z() });
                        item = surfaceMember(r, pts, it->second.nodeIds(), it->second.thickness());
                    }
                    break;
                case ElementKind::Wall:
                    if (auto it = m.walls().find(r.id); it != m.walls().end())
                    {
                        const auto* a = m.getNode(it->second.startNodeId());
                        const auto* b = m.getNode(it->second.endNodeId());
                        if (a && b)
                        {
                            const double h = it->second.height();
                            item = surfaceMember(r, { { a->x(), a->y(), a->z() }, { b->x(), b->y(), b->z() }, { b->x(), b->y(), b->z() + h }, { a->x(), a->y(), a->z() + h } },
                                                 { it->second.startNodeId(), it->second.endNodeId() }, it->second.thickness());
                        }
                    }
                    break;
                case ElementKind::Foundation:
                    // Une fondation est un appui du modèle analytique : son nœud devient une connexion
                    if (auto it = m.foundations().find(r.id); it != m.foundations().end()) connection(it->second.nodeId());
                    break;
                default:
                    break;
                }
                if (!item) continue;
                members.push_back(item);
                m_analyticalItems.push_back(item);
                ++m_report.analyticalMembers;

                const MaterialInfo mi = materialOf(m, r);
                if (mi.material)
                    m_rels.associateMaterial(mi.section ? m_mapper.materialProfileSet(*mi.material, *mi.section) : m_mapper.material(*mi.material), item);
            }
            if (auto p = m_productIds.find(id); p != m_productIds.end() && !members.empty()) m_rels.assignToProduct(p->second, members);
        }
        // Nœuds appuyés sans élément (appuis isolés) : exportés pour ne perdre aucune condition aux limites
        for (const auto& [nid, n] : m.nodes())
        {
            const auto& sp = n.support();
            using D = TSA::Model::DOFState;
            if (sp.tx() != D::Free || sp.ty() != D::Free || sp.tz() != D::Free || sp.rx() != D::Free || sp.ry() != D::Free || sp.rz() != D::Free)
                connection(nid);
        }
        for (int item : m_analyticalItems) m_rels.assignToGroup(model, item);
    }

    const TSA::Model::Model& m_model;
    IfcExportOptions m_opt;
    IfcStepWriter m_w;
    IfcExportContext m_ctx;
    IfcMapper m_mapper;
    IfcGeometryMapper m_geom;
    IfcPropertyMapper m_props;
    IfcRelationshipMapper m_rels;
    IfcExportReport m_report;

    int m_identity = 0, m_project = 0, m_site = 0, m_building = 0, m_sitePlacement = 0, m_buildingPlacement = 0;
    std::map<std::string, std::pair<int, int>> m_storeys;   ///< niveau → (étage, placement)
    std::map<int, int> m_productIds;                        ///< id produit BIM → #STEP
    std::map<int, int> m_vertices, m_connections;           ///< nœud → #STEP
    std::vector<int> m_analyticalItems;
};
} // namespace

IfcExportReport IfcExporter::exportToString(const TSA::Model::Model& model, const IfcExportOptions& options, std::string& out)
{
    Exporter ex(model, options);
    return ex.run(out);
}

IfcExportReport IfcExporter::exportToFile(const TSA::Model::Model& model, const std::string& path, IfcExportOptions options)
{
    const QString qpath = QString::fromStdString(path);
    if (options.fileName.empty()) options.fileName = QFileInfo(qpath).fileName().toStdString();
    std::string doc;
    IfcExportReport r = exportToString(model, options, doc);
    if (!r.ok) return r;
    QFile f(qpath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        r.ok = false;
        r.error = "impossible d'écrire « " + path + " » (" + f.errorString().toStdString() + ")";
        return r;
    }
    f.write(doc.data(), static_cast<qint64>(doc.size()));   // ASCII pur (Unicode encodé \X2\)
    return r;
}

} // namespace TSA::BIM::Ifc
