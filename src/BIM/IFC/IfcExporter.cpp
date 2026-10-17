#include "IfcExporter.h"

#include "IfcExportContext.h"
#include "IfcGeometryMapper.h"
#include "IfcMapper.h"
#include "IfcPropertyMapper.h"
#include "IfcRelationshipMapper.h"
#include "../Core/BimModel.h"
#include "../Core/IfcGuid.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Mesh/MeshGeometry.h"
#include "../../Model/Model.h"

#include "IfcOpenShellImport.h"

#include <QFileInfo>
#include <QSaveFile>

#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;
using TSA::Model::ElementKind;

std::string IfcExportReport::summary() const
{
    if (!ok) return "Export IFC échoué : " + error;
    return std::to_string(products) + " produit(s), " + std::to_string(storeys) + " étage(s), "
        + std::to_string(analyticalMembers) + " élément(s) analytique(s), " + std::to_string(analyticalNodes)
        + " nœud(s) analytique(s), " + std::to_string(entities) + " entités STEP" + (verification.empty() ? "" : " ; " + verification);
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

/// Classe écrite pour un produit et attributs qui suivent Tag (schéma IFC4X3_ADD2).
struct ExportClass
{
    std::string entity;   ///< IFCBEAM, IFCDOOR… ; IFCBUILDINGELEMENTPROXY à défaut
    std::string tail;     ///< « ,.NOTDEFINED. » pour les classes à PredefinedType seul
    bool substituted = false;   ///< classe d'origine non écrite telle quelle (conservée en propriété)
};

std::string upper(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string optLength(const PhysicalElement& e, const char* key)
{
    if (const auto* ps = e.propertySet("TSA_Import"))
        if (auto it = ps->properties.find(key); it != ps->properties.end() && it->second.type == PropertyValue::Type::Length)
            return W::real(it->second.number);
    return "$";
}

/// Produits importés : classe d'origine ramenée au schéma écrit. Les « …StandardCase » et « …ElementedCase »
/// d'IFC4 n'existent plus en IFC4X3 (classe de base) ; les classes dont l'exporteur ne connaît pas les attributs
/// propres deviennent IfcBuildingElementProxy, la classe d'origine étant conservée en propriété (TSA_Import).
ExportClass exportClass(const PhysicalElement& e)
{
    const std::string pdt = W::enumeration(IfcMapper::predefinedType(e.category, e.predefinedType));
    std::string cls = upper(e.ifcClass);
    for (const char* suffix : { "STANDARDCASE", "ELEMENTEDCASE" })
        if (const std::string s(suffix); cls.size() > s.size() && cls.compare(cls.size() - s.size(), s.size(), s) == 0)
            cls.resize(cls.size() - s.size());
    if (cls.empty()) cls = IfcMapper::stepEntity(e.category);   // produit créé dans TSA
    if (cls == "IFCPILE") return { cls, "," + pdt + ",$" };     // ConstructionType (déprécié en 4.3)
    // Classes dont les attributs propres se limitent à PredefinedType (vérifié sur le schéma IFC4X3_ADD2)
    static const std::set<std::string> generic = {
        "IFCBEAM", "IFCCOLUMN", "IFCMEMBER", "IFCSLAB", "IFCWALL", "IFCFOOTING", "IFCPLATE", "IFCBUILDINGELEMENTPROXY",
        "IFCROOF", "IFCSTAIR", "IFCRAMP", "IFCRAMPFLIGHT", "IFCRAILING", "IFCCOVERING", "IFCCURTAINWALL", "IFCCHIMNEY",
        "IFCSHADINGDEVICE", "IFCBEARING", "IFCDISCRETEACCESSORY", "IFCBUILDINGELEMENTPART", "IFCFURNITURE",
    };
    if (generic.count(cls)) return { cls, "," + pdt };
    if (cls == "IFCDOOR" || cls == "IFCWINDOW")   // OverallHeight, OverallWidth, PredefinedType, Operation/PartitioningType, UserDefined…
        return { cls, "," + optLength(e, "OverallHeight") + "," + optLength(e, "OverallWidth") + ",.NOTDEFINED.,$,$" };
    if (cls == "IFCSTAIRFLIGHT") return { cls, ",$,$,$,$,.NOTDEFINED." };
    if (cls == "IFCELEMENTASSEMBLY") return { cls, ",$,.NOTDEFINED." };
    if (cls == "IFCMECHANICALFASTENER") return { cls, ",$,$,.NOTDEFINED." };
    return { "IFCBUILDINGELEMENTPROXY", ",.NOTDEFINED.", true };
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

    /// Décomposition d'origine (TSA_Import.Parent = « IfcRoof <GlobalId> ») : id produit → GlobalId du tout, si ce tout est
    /// un produit du modèle et que la chaîne des parents ne boucle pas.
    std::map<int, std::string> decomposition() const
    {
        std::map<std::string, std::string> parentByGuid;
        for (const auto& [id, e] : m_ctx.bim.elements())
            if (const auto* ps = e.propertySet("TSA_Import"))
                if (auto it = ps->properties.find("Parent"); it != ps->properties.end())
                {
                    const std::string& v = it->second.text;
                    const std::string guid = v.substr(v.find_last_of(' ') + 1);
                    if (guid != e.globalId && m_ctx.bim.elementByGlobalId(guid)) parentByGuid[e.globalId] = guid;
                }
        std::map<int, std::string> out;
        for (const auto& [id, e] : m_ctx.bim.elements())
        {
            auto it = parentByGuid.find(e.globalId);
            if (it == parentByGuid.end()) continue;
            std::set<std::string> seen { e.globalId };
            bool loops = false;
            for (auto up = it; up != parentByGuid.end(); up = parentByGuid.find(up->second))
                if (!seen.insert(up->second).second)
                {
                    loops = true;
                    break;
                }
            if (!loops) out[id] = it->second;
        }
        return out;
    }

    void writeProducts()
    {
        std::map<std::string, int> classificationSources;
        std::map<std::string, int> classificationRefs;
        // Une partie d'agrégat est rattachée à son tout ; seul le tout est contenu dans l'étage (règle IFC).
        const std::map<int, std::string> parentOf = decomposition();
        std::map<std::string, int> productByGuid;
        for (const auto& [id, e] : m_ctx.bim.elements())
        {
            const bool physicalOnly = e.analytical.empty();
            const std::string label = (e.name.empty() ? defaultProductName(e) : e.name) + (e.ifcClass.empty() ? "" : " (" + e.ifcClass + ")");
            const ExportClass ec = exportClass(e);
            if (ec.entity.empty()) continue;

            const std::string storey = m_ctx.bim.resolvedStorey(m_model, e);
            auto st = m_storeys.find(storey);
            const int container = st != m_storeys.end() ? st->second.first : m_building;
            const int relTo = st != m_storeys.end() ? st->second.second : m_buildingPlacement;
            const int placement = m_geom.localPlacement(relTo, m_identity);
            // Produit importé sans élément de calcul : géométrie d'origine (maillage) ; sans géométrie propre (agrégat dont
            // les parties portent la géométrie, objet non représenté dans le fichier) : sans représentation. Jamais omis.
            const int shape = !physicalOnly ? m_geom.productShape(e) : e.geometry ? meshShape(*e.geometry) : 0;
            if (!shape && (!physicalOnly || e.geometry))
                m_ctx.warnings.push_back(label + " : géométrie non exportée (dimensions nulles ou nœuds manquants).");

            // Nom : à défaut, nom par défaut des produits créés dans TSA (« Poutre B3 ») ; un produit importé sans nom le reste.
            const std::string name = !e.name.empty() ? W::str(e.name) : e.imported() ? std::string("$") : W::str(defaultProductName(e));
            std::string args = W::str(e.globalId) + ",$," + name + "," + W::optStr(e.description) + "," + W::optStr(e.objectType) + ","
                + W::ref(placement) + "," + (shape ? W::ref(shape) : "$") + "," + W::optStr((e.tag.empty() ? defaultProductTag(e) : e.tag))
                + ec.tail;
            const int product = m_w.add(ec.entity, args);
            m_productIds[id] = product;
            productByGuid[e.globalId] = product;
            if (!parentOf.count(id)) m_rels.contain(container, product);
            ++m_report.products;

            PhysicalElement written = e;   // copie légère : la géométrie est partagée
            if (!e.ifcClass.empty() && upper(e.ifcClass) != ec.entity)
            {
                // Classe d'origine non écrite telle quelle (classe absente d'IFC4X3 comme les …StandardCase, ou proxy) :
                // conservée en propriété, rétablie à la relecture par TSA.
                PropertySet* origin = nullptr;
                for (auto& ps : written.propertySets)
                    if (ps.name == "TSA_Import") origin = &ps;
                if (!origin)
                {
                    written.propertySets.push_back({ "TSA_Import", {} });
                    origin = &written.propertySets.back();
                }
                origin->properties["OriginalIfcClass"] = PropertyValue::ofText(e.ifcClass, PropertyValue::Type::Label);
                if (ec.substituted)
                    m_ctx.warnings.push_back(label + " : exporté en IfcBuildingElementProxy (classe d'origine conservée en propriété TSA_Import).");
            }
            if (!physicalOnly)
            {
                const MaterialInfo mi = materialOf(m_model, e.analytical.front());
                if (mi.material)
                    m_rels.associateMaterial(mi.section ? m_mapper.materialProfileSet(*mi.material, *mi.section) : m_mapper.material(*mi.material), product);
            }
            for (int ps : m_props.write(written)) m_rels.defineByProperties(ps, product);

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
        for (const auto& [id, parentGuid] : parentOf)
        {
            const auto part = m_productIds.find(id);
            const auto whole = productByGuid.find(parentGuid);
            if (part != m_productIds.end() && whole != productByGuid.end()) m_rels.aggregate(whole->second, part->second);
        }
    }

    /// Géométrie importée : IfcTriangulatedFaceSet (coordonnées du projet, m) et sa couleur (style de surface).
    int meshShape(const TriangleMesh& m)
    {
        if (!m.valid()) return 0;
        std::string coords;
        coords.reserve(m.vertexCount() * 36);
        for (std::size_t i = 0; i < m.vertexCount(); ++i)
        {
            const auto p = m.vertex(i);
            coords += (i ? ",(" : "(") + W::real(p[0]) + "," + W::real(p[1]) + "," + W::real(p[2]) + ")";
        }
        const int list = m_w.add("IFCCARTESIANPOINTLIST3D", "(" + coords + "),$");   // CoordList, TagList (IFC4X3)
        std::string faces;
        faces.reserve(m.indices.size() * 6);
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            faces += (t ? ",(" : "(") + std::to_string(m.indices[t] + 1) + "," + std::to_string(m.indices[t + 1] + 1) + ","
                + std::to_string(m.indices[t + 2] + 1) + ")";
        // Coordinates, Normals, Closed, CoordIndex, PnIndex
        const int tfs = m_w.add("IFCTRIANGULATEDFACESET", W::ref(list) + ",$,$,(" + faces + "),$");
        m_w.add("IFCSTYLEDITEM", W::ref(tfs) + "," + W::refs({ surfaceStyle(m.color) }) + ",$");
        const int rep = m_w.add("IFCSHAPEREPRESENTATION", W::ref(m_ctx.bodyContext) + ",'Body','Tessellation'," + W::refs({ tfs }));
        return m_w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs({ rep }));
    }

    int surfaceStyle(const std::array<float, 4>& c)
    {
        const auto key = std::make_tuple(std::lround(c[0] * 255), std::lround(c[1] * 255), std::lround(c[2] * 255), std::lround(c[3] * 255));
        if (auto it = m_styles.find(key); it != m_styles.end()) return it->second;
        const int colour = m_w.add("IFCCOLOURRGB", "$," + W::real(c[0]) + "," + W::real(c[1]) + "," + W::real(c[2]));
        const int shading = m_w.add("IFCSURFACESTYLESHADING", W::ref(colour) + "," + W::real(1.0 - c[3]));   // SurfaceColour, Transparency
        return m_styles[key] = m_w.add("IFCSURFACESTYLE", "$,.BOTH.," + W::refs({ shading }));
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

    /// Élément surfacique analytique : contour extérieur, ouvertures en contours intérieurs (IfcFaceBound), type prédéfini
    /// selon la formulation (SHELL, BENDING_ELEMENT, MEMBRANE_ELEMENT).
    int surfaceMember(const AnalyticalRef& r, const std::vector<Vec3>& pts, const std::vector<int>& nodeIds, double thickness,
                      const std::vector<std::vector<Vec3>>& holes = {}, const char* predefined = "SHELL")
    {
        if (pts.size() < 3) return 0;
        std::vector<int> loop;
        for (const auto& p : pts) loop.push_back(m_geom.point(p));
        const int poly = m_w.add("IFCPOLYLOOP", W::refs(loop));
        const int bound = m_w.add("IFCFACEOUTERBOUND", W::ref(poly) + ",.T.");
        std::vector<int> bounds { bound };
        for (const auto& h : holes)
        {
            if (h.size() < 3) continue;
            std::vector<int> hl;
            for (const auto& p : h) hl.push_back(m_geom.point(p));
            bounds.push_back(m_w.add("IFCFACEBOUND", W::ref(m_w.add("IFCPOLYLOOP", W::refs(hl))) + ",.T."));
        }
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
        const int face = m_w.add("IFCFACESURFACE", W::refs(bounds) + "," + W::ref(plane) + ",.T.");
        const int topo = m_w.add("IFCTOPOLOGYREPRESENTATION", W::ref(m_ctx.modelContext) + ",'Reference','Face'," + W::refs({ face }));
        const int shape = m_w.add("IFCPRODUCTDEFINITIONSHAPE", "$,$," + W::refs({ topo }));
        const int m = m_w.add("IFCSTRUCTURALSURFACEMEMBER", W::str(guidOf(r)) + ",$," + W::str(r.label()) + ",$,$,"
                                                            + W::ref(m_ctx.analysisPlacement) + "," + W::ref(shape) + "," + W::enumeration(predefined) + ","
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
        // Géométrie des surfaces partagée avec le maillage (contours, portes en encoche, ouvertures)
        const auto surfaces = TSA::Mesh::MeshGeometry::extract(m);
        auto vec = [](const TSA::Mesh::P3& p) { return Vec3 { p.x, p.y, p.z }; };
        auto loops = [&](const std::string& ref, std::vector<Vec3>* outer, std::vector<std::vector<Vec3>>* holes) {
            const auto* sg = surfaces.surface(ref);
            if (!sg) return false;
            outer->clear();
            for (const auto& p : sg->outer) outer->push_back(vec(p));
            for (const auto& h : sg->holes)
            {
                holes->emplace_back();
                for (const auto& p : h) holes->back().push_back(vec(p));
            }
            return true;
        };
        auto predefined = [](TSA::Model::SurfaceFormulation f) {
            return f == TSA::Model::SurfaceFormulation::Plate ? "BENDING_ELEMENT" : f == TSA::Model::SurfaceFormulation::Membrane ? "MEMBRANE_ELEMENT" : "SHELL";
        };
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
                        std::vector<std::vector<Vec3>> holes;
                        if (!loops("SL" + std::to_string(r.id), &pts, &holes))
                            for (int nid : it->second.nodeIds())
                                if (const auto* n = m.getNode(nid)) pts.push_back({ n->x(), n->y(), n->z() });
                        item = surfaceMember(r, pts, it->second.nodeIds(), it->second.thickness(), holes, predefined(it->second.formulation()));
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
                            std::vector<Vec3> pts;
                            std::vector<std::vector<Vec3>> holes;
                            if (!loops("W" + std::to_string(r.id), &pts, &holes))
                                pts = { { a->x(), a->y(), a->z() }, { b->x(), b->y(), b->z() }, { b->x(), b->y(), b->z() + h }, { a->x(), a->y(), a->z() + h } };
                            item = surfaceMember(r, pts, { it->second.startNodeId(), it->second.endNodeId() }, it->second.thickness(), holes,
                                                 predefined(it->second.formulation()));
                        }
                    }
                    break;
                case ElementKind::Foundation:
                    // Une fondation est un appui du modèle analytique : son nœud devient une connexion, reliée à la semelle
                    // (IfcRelAssignsToProduct) pour que la relecture retrouve la fondation sur ce nœud, sans déduction.
                    if (auto it = m.foundations().find(r.id); it != m.foundations().end())
                        if (const int c = connection(it->second.nodeId())) members.push_back(c);
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
    std::map<std::tuple<long, long, long, long>, int> m_styles;   ///< couleur (RVBA ×255) → IfcSurfaceStyle
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
    // Écriture atomique : un fichier existant n'est remplacé qu'une fois le nouveau entièrement écrit.
    QSaveFile f(qpath);
    if (!f.open(QIODevice::WriteOnly) || f.write(doc.data(), static_cast<qint64>(doc.size())) != static_cast<qint64>(doc.size())
        || !f.commit())   // ASCII pur (Unicode encodé \X2\)
    {
        r.ok = false;
        r.error = "impossible d'écrire « " + path + " » (" + f.errorString().toStdString() + ")";
        return r;
    }

    // Relecture par un lecteur indépendant de l'écrivain de TSA : jamais de succès annoncé sur un fichier illisible.
    if (options.verifyAfterWrite)
    {
        std::string schema, error;
        int elements = 0;
        if (detail::verifyFile(path, schema, elements, error))
        {
            r.verifiedElements = elements;
            r.verification = "relu par " + detail::fullEngineName() + " : schéma " + schema + ", " + std::to_string(elements) + " produit(s)";
            if (elements != r.products)
                r.warnings.push_back("Relecture : " + std::to_string(elements) + " produit(s) relu(s) pour " + std::to_string(r.products) + " écrit(s).");
            if (!error.empty()) r.warnings.push_back("Relecture : " + error);
        }
        else if (detail::fullEngineAvailable())
        {
            r.ok = false;
            r.error = "fichier écrit mais illisible par un lecteur IFC indépendant (" + error + ")";
        }
        else
            r.verification = "relecture indépendante non faite (" + error + ")";
    }
    return r;
}

} // namespace TSA::BIM::Ifc
