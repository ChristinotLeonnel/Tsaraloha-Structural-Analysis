#include "IfcImporter.h"

#include "IfcGeometryMapper.h"
#include "IfcPropertyMapper.h"
#include "IfcStepReader.h"
#include "../Core/BimModel.h"
#include "../Core/IfcGuid.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"

#include <QFile>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

namespace TSA::BIM::Ifc
{

using TSA::Model::ElementKind;
using Kind = StepValue::Kind;

std::string IfcImportReport::summary() const
{
    if (!ok) return "Import IFC échoué : " + error;
    return std::to_string(products) + " produit(s), " + std::to_string(nodes) + " nœud(s), " + std::to_string(members)
        + " barre(s), " + std::to_string(surfaces) + " surface(s), " + std::to_string(foundations) + " fondation(s), "
        + std::to_string(storeys) + " étage(s) — schéma " + schema;
}

namespace
{
constexpr double kPi = 3.14159265358979323846;

Vec3 add(const Vec3& a, const Vec3& b) { return { a[0] + b[0], a[1] + b[1], a[2] + b[2] }; }
Vec3 sub(const Vec3& a, const Vec3& b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
Vec3 mul(const Vec3& a, double k) { return { a[0] * k, a[1] * k, a[2] * k }; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b) { return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; }
Vec3 unit(const Vec3& a)
{
    const double n = std::sqrt(dot(a, a));
    return n > 0.0 ? mul(a, 1.0 / n) : Vec3 { 0, 0, 1 };
}

/// Repère (origine + axes orthonormés) : p_monde = o + x·u + y·v + z·w.
struct Frame
{
    Vec3 o { 0, 0, 0 }, x { 1, 0, 0 }, y { 0, 1, 0 }, z { 0, 0, 1 };
    Vec3 point(const Vec3& p) const { return add(o, add(mul(x, p[0]), add(mul(y, p[1]), mul(z, p[2])))); }
    Vec3 dir(const Vec3& d) const { return add(mul(x, d[0]), add(mul(y, d[1]), mul(z, d[2]))); }
    Frame operator*(const Frame& c) const { return { point(c.o), dir(c.x), dir(c.y), dir(c.z) }; }
};

PropertyValue propertyValue(const StepValue& v)
{
    if (v.kind != Kind::Typed) return PropertyValue::ofText(v.asText());
    const std::string& t = v.text;
    using T = PropertyValue::Type;
    if (t == "IFCBOOLEAN" || t == "IFCLOGICAL") return PropertyValue::ofBool(v.asBool());
    if (t == "IFCLABEL" || t == "IFCIDENTIFIER") return PropertyValue::ofText(v.asText(), T::Label);
    if (t == "IFCTEXT") return PropertyValue::ofText(v.asText(), T::Text);
    if (!v.list.empty() && (v.list.front().kind == Kind::String || v.list.front().kind == Kind::Enum)) return PropertyValue::ofText(v.asText(), T::Text);
    if (t == "IFCINTEGER" || t == "IFCCOUNTMEASURE") return PropertyValue::ofReal(v.asNumber(), T::Integer);
    if (t == "IFCLENGTHMEASURE" || t == "IFCPOSITIVELENGTHMEASURE" || t == "IFCNONNEGATIVELENGTHMEASURE") return PropertyValue::ofReal(v.asNumber(), T::Length);
    if (t == "IFCFORCEMEASURE") return PropertyValue::ofReal(v.asNumber(), T::Force);
    if (t == "IFCPRESSUREMEASURE") return PropertyValue::ofReal(v.asNumber(), T::Pressure);
    if (t == "IFCRATIOMEASURE" || t == "IFCPOSITIVERATIOMEASURE" || t == "IFCNORMALISEDRATIOMEASURE") return PropertyValue::ofReal(v.asNumber(), T::Ratio);
    return PropertyValue::ofReal(v.asNumber(), T::Real);
}

bool categoryOf(const std::string& entity, BimCategory& c)
{
    static const std::map<std::string, BimCategory> map = {
        { "IFCBEAM", BimCategory::Beam }, { "IFCBEAMSTANDARDCASE", BimCategory::Beam },
        { "IFCCOLUMN", BimCategory::Column }, { "IFCCOLUMNSTANDARDCASE", BimCategory::Column },
        { "IFCMEMBER", BimCategory::Member }, { "IFCMEMBERSTANDARDCASE", BimCategory::Member },
        { "IFCSLAB", BimCategory::Slab }, { "IFCSLABSTANDARDCASE", BimCategory::Slab },
        { "IFCWALL", BimCategory::Wall }, { "IFCWALLSTANDARDCASE", BimCategory::Wall },
        { "IFCFOOTING", BimCategory::Footing }, { "IFCPILE", BimCategory::Pile }, { "IFCPLATE", BimCategory::Plate },
    };
    auto it = map.find(entity);
    if (it == map.end()) return false;
    c = it->second;
    return true;
}

class Importer
{
public:
    Importer(const IfcStepReader& r, TSA::Model::Model& m, const IfcImportOptions& o) : m_r(r), m_m(m), m_opt(o) {}

    IfcImportReport run()
    {
        m_report.schema = m_r.schema();
        readUnits();
        indexRelationships();
        importSpatial();
        if (m_opt.useAnalyticalModel) importAnalytical();
        importProducts();
        finalizeBim();
        m_report.ok = true;
        return m_report;
    }

private:
    // ------------------------------------------------------------------ unités
    static double prefixFactor(const std::string& prefix)
    {
        static const std::map<std::string, double> f = { { "MILLI", 1e-3 }, { "CENTI", 1e-2 }, { "DECI", 1e-1 }, { "KILO", 1e3 },
                                                          { "MEGA", 1e6 }, { "GIGA", 1e9 }, { "HECTO", 1e2 }, { "DECA", 1e1 } };
        auto it = f.find(prefix);
        return it == f.end() ? 1.0 : it->second;
    }

    /// Facteur SI d'une unité (IfcSIUnit avec préfixe, IfcConversionBasedUnit : pied, pouce…).
    double unitFactor(const StepEntity* u, int depth = 0) const
    {
        if (!u || depth > 4) return 1.0;
        if (u->type == "IFCSIUNIT") return prefixFactor(u->arg(2).asText());
        if (u->type == "IFCCONVERSIONBASEDUNIT" || u->type == "IFCCONVERSIONBASEDUNITWITHOFFSET")
            if (const auto* mwu = m_r.get(u->arg(3)))
                return mwu->arg(0).asNumber(1.0) * unitFactor(m_r.get(mwu->arg(1)), depth + 1);
        return 1.0;
    }

    void readUnits()
    {
        for (const auto* ua : m_r.byType("IFCUNITASSIGNMENT"))
            for (int u : ua->arg(0).refs())
            {
                const auto* e = m_r.get(u);
                if (!e) continue;
                const std::string type = e->arg(1).asText();
                if (type == "LENGTHUNIT") m_len = unitFactor(e);
                else if (type == "PRESSUREUNIT") m_pressure = unitFactor(e);
            }
        if (m_len != 1.0)
            m_report.warnings.push_back("Unité de longueur du fichier convertie en mètres (facteur " + std::to_string(m_len) + ").");
    }

    // ------------------------------------------------------------------ relations inverses
    void indexRelationships()
    {
        for (const auto* e : m_r.byType("IFCRELASSOCIATESMATERIAL"))
            for (int o : e->arg(4).refs()) m_material[o] = e->arg(5).ref;
        for (const auto* e : m_r.byType("IFCRELDEFINESBYPROPERTIES"))
            for (int o : e->arg(4).refs()) m_psets[o].push_back(e->arg(5).ref);
        for (const auto* e : m_r.byType("IFCRELASSOCIATESCLASSIFICATION"))
            for (int o : e->arg(4).refs()) m_classes[o].push_back(e->arg(5).ref);
        for (const auto* e : m_r.byType("IFCRELASSIGNSTOPRODUCT"))
            for (int o : e->arg(4).refs())
            {
                m_productOf[o] = e->arg(6).ref;
                m_assigned[e->arg(6).ref].push_back(o);
            }
        for (const auto* e : m_r.byType("IFCRELCONTAINEDINSPATIALSTRUCTURE"))
            for (int o : e->arg(4).refs()) m_container[o] = e->arg(5).ref;
        for (const auto* e : m_r.byType("IFCMATERIALPROPERTIES")) m_materialProps[e->arg(3).ref].push_back(e->id);
        for (const auto* e : m_r.byType("IFCEXTENDEDMATERIALPROPERTIES")) m_materialProps[e->arg(3).ref].push_back(e->id);
    }

    // ------------------------------------------------------------------ géométrie de base
    Vec3 point(int id) const
    {
        Vec3 p { 0, 0, 0 };
        if (const auto* e = m_r.get(id))
        {
            const auto& c = e->arg(0).list;
            for (std::size_t i = 0; i < c.size() && i < 3; ++i) p[i] = c[i].asNumber() * m_len;
        }
        return p;
    }

    /// Points d'une courbe de profil fermée (IfcPolyline, IfcIndexedPolyCurve), point de fermeture retiré.
    std::vector<Vec3> curvePoints(const StepEntity* curve) const
    {
        std::vector<Vec3> pts;
        if (!curve) return pts;
        if (curve->type == "IFCPOLYLINE")
            for (int p : curve->arg(0).refs()) pts.push_back(point(p));
        else if (curve->type == "IFCINDEXEDPOLYCURVE")
        {
            if (!curve->arg(1).isNull())
                for (const auto& seg : curve->arg(1).list)
                    if (seg.kind == Kind::Typed && seg.text == "IFCARCINDEX") return {};   // arcs non pris en charge
            if (const auto* list = m_r.get(curve->arg(0)))
                for (const auto& c : list->arg(0).list)
                {
                    Vec3 p { 0, 0, 0 };
                    for (std::size_t i = 0; i < c.list.size() && i < 3; ++i) p[i] = c.list[i].asNumber() * m_len;
                    pts.push_back(p);
                }
        }
        if (pts.size() > 1 && dot(sub(pts.front(), pts.back()), sub(pts.front(), pts.back())) < 1e-18) pts.pop_back();
        return pts;
    }

    Vec3 direction(int id, const Vec3& fallback) const
    {
        const auto* e = m_r.get(id);
        if (!e) return fallback;
        Vec3 d { 0, 0, 0 };
        const auto& c = e->arg(0).list;
        for (std::size_t i = 0; i < c.size() && i < 3; ++i) d[i] = c[i].asNumber();
        return unit(d);
    }

    Frame axisPlacement(int id) const
    {
        Frame f;
        const auto* e = m_r.get(id);
        if (!e) return f;
        f.o = point(e->arg(0).ref);
        if (e->type == "IFCAXIS2PLACEMENT2D")
        {
            f.x = direction(e->arg(1).ref, { 1, 0, 0 });
            f.y = cross(f.z, f.x);
            return f;
        }
        f.z = direction(e->arg(1).ref, { 0, 0, 1 });
        Vec3 x = direction(e->arg(2).ref, std::abs(f.z[0]) < 0.9 ? Vec3 { 1, 0, 0 } : Vec3 { 0, 1, 0 });
        x = unit(sub(x, mul(f.z, dot(x, f.z))));
        f.x = x;
        f.y = cross(f.z, f.x);
        return f;
    }

    Frame placement(int id)
    {
        if (id <= 0) return {};
        if (auto it = m_frames.find(id); it != m_frames.end()) return it->second;
        Frame f;
        if (const auto* e = m_r.get(id); e && e->type == "IFCLOCALPLACEMENT")
            f = placement(e->arg(0).kind == Kind::Ref ? e->arg(0).ref : 0) * axisPlacement(e->arg(1).ref);
        return m_frames[id] = f;
    }

    // ------------------------------------------------------------------ nœuds
    int node(const Vec3& p)
    {
        const double q = std::max(m_opt.nodeTolerance, 1e-9);
        const std::array<long long, 3> key { std::llround(p[0] / q), std::llround(p[1] / q), std::llround(p[2] / q) };
        if (auto it = m_nodes.find(key); it != m_nodes.end()) return it->second;
        const int id = m_m.addNode(p[0], p[1], p[2]);
        ++m_report.nodes;
        return m_nodes[key] = id;
    }

    // ------------------------------------------------------------------ matériaux et profils
    TSA::Model::Material material(int matId)
    {
        if (auto it = m_materials.find(matId); it != m_materials.end()) return it->second;
        const auto* e = m_r.get(matId);
        TSA::Model::Material mat;
        if (!e) return mat;
        const std::string cat = e->arg(2).asText();
        if (cat == "steel") mat = TSA::Model::Material::steelS235();
        else if (cat == "wood") mat = TSA::Model::Material::timberC24();
        else if (cat == "aluminium") mat = TSA::Model::Material::aluminum();
        else if (cat == "block" || cat == "brick") mat = TSA::Model::Material::masonry();
        else mat = TSA::Model::Material::concreteC25_30();
        if (!e->arg(0).asText().empty()) mat.name = e->arg(0).asText();
        for (int pid : m_materialProps[matId])
        {
            const auto* p = m_r.get(pid);
            if (!p) continue;
            for (int sv : p->arg(2).refs())
            {
                const auto* s = m_r.get(sv);
                if (!s || s->type != "IFCPROPERTYSINGLEVALUE") continue;
                const std::string n = s->arg(0).asText();
                const double v = s->arg(2).asNumber(std::nan(""));
                if (std::isnan(v)) continue;
                if (n == "YoungModulus") mat.E = v * m_pressure;
                else if (n == "PoissonRatio") mat.nu = v;
                else if (n == "MassDensity") mat.density = v;
                else if (n == "ThermalExpansionCoefficient") mat.thermalCoeff = v;
                else if (n == "CharacteristicStrength" || n == "YieldStress" || n == "CompressiveStrength") mat.fk = v * m_pressure;
            }
        }
        mat.syncMechanical();
        return m_materials[matId] = mat;
    }

    bool section(int profId, TSA::Model::Section& s) const
    {
        using TSA::Model::Section;
        using TSA::Model::SectionShape;
        const auto* e = m_r.get(profId);
        if (!e) return false;
        const std::string name = e->arg(1).asText();
        auto n = [e, this](std::size_t i) { return e->arg(i).asNumber() * m_len; };
        const std::string& t = e->type;
        if (t == "IFCRECTANGLEPROFILEDEF") s = Section::rectangular(n(3), n(4));
        else if (t == "IFCCIRCLEPROFILEDEF") s = Section::circular(2.0 * n(3));
        else if (t == "IFCCIRCLEHOLLOWPROFILEDEF") s = Section::pipe(2.0 * n(3), n(4));
        else if (t == "IFCRECTANGLEHOLLOWPROFILEDEF") s = Section::boxHollow(n(3), n(4), n(5));
        else if (t == "IFCLSHAPEPROFILEDEF") s = Section::angle(n(3), n(4), n(5));
        else if (t == "IFCTSHAPEPROFILEDEF") s = Section::tSection(n(3), n(4), n(5), n(6));
        else if (t == "IFCISHAPEPROFILEDEF" || t == "IFCASYMMETRICISHAPEPROFILEDEF")
        {
            s = Section();
            s.shape = SectionShape::IShape;
            s.width = n(3);
            s.height = n(4);
            s.tw = n(5);
            s.tf = n(6);
        }
        else if (t == "IFCUSHAPEPROFILEDEF")
        {
            s = Section();
            s.shape = SectionShape::UPN;
            s.height = n(3);
            s.width = n(4);
            s.tw = n(5);
            s.tf = n(6);
        }
        else return false;
        if (!name.empty()) s.name = name;
        return true;
    }

    /// Matériau et profil associés à un objet (IfcMaterialProfileSet[Usage], IfcMaterial, couches).
    bool materialOf(int objectId, TSA::Model::Material& mat, TSA::Model::Section* sec, bool* hasSection = nullptr)
    {
        auto it = m_material.find(objectId);
        if (it == m_material.end()) return false;
        const auto* e = m_r.get(it->second);
        for (int guard = 0; e && guard < 4; ++guard)
        {
            if (e->type == "IFCMATERIAL")
            {
                mat = material(e->id);
                return true;
            }
            if (e->type == "IFCMATERIALPROFILESETUSAGE" || e->type == "IFCMATERIALLAYERSETUSAGE") e = m_r.get(e->arg(0));
            else if (e->type == "IFCMATERIALPROFILESET")
            {
                const auto profiles = e->arg(2).refs();
                const auto* mp = profiles.empty() ? nullptr : m_r.get(profiles.front());
                if (!mp) return false;
                if (sec && section(mp->arg(3).ref, *sec) && hasSection) *hasSection = true;
                e = m_r.get(mp->arg(2));
            }
            else if (e->type == "IFCMATERIALLAYERSET")
            {
                const auto layers = e->arg(0).refs();
                const auto* l = layers.empty() ? nullptr : m_r.get(layers.front());
                e = l ? m_r.get(l->arg(0)) : nullptr;
            }
            else if (e->type == "IFCMATERIALLIST")
            {
                const auto mats = e->arg(0).refs();
                e = mats.empty() ? nullptr : m_r.get(mats.front());
            }
            else return false;
        }
        return false;
    }

    // ------------------------------------------------------------------ structure spatiale
    void importSpatial()
    {
        auto spatial = [this](const char* type, SpatialElement& out) {
            const auto list = m_r.byType(type);
            if (list.empty()) return;
            out.globalId = list.front()->arg(0).asText();
            if (!list.front()->arg(2).asText().empty()) out.name = list.front()->arg(2).asText();
            out.description = list.front()->arg(3).asText();
        };
        auto& s = m_bim.spatial();
        spatial("IFCPROJECT", s.project);
        spatial("IFCSITE", s.site);
        spatial("IFCBUILDING", s.building);
        spatial("IFCSTRUCTURALANALYSISMODEL", s.analysisModel);

        auto* lm = m_m.levelManager();
        for (const auto* st : m_r.byType("IFCBUILDINGSTOREY"))
        {
            if (!lm) break;
            // Cote : attribut Elevation, sinon placement de l'étage
            const double z = st->arg(9).isNull() ? placement(st->arg(5).ref).o[2] : st->arg(9).asNumber() * m_len;
            const std::string name = st->arg(2).asText().empty() ? "Étage" : st->arg(2).asText();
            const TSA::Coordinate::Level* lvl = nullptr;
            for (const auto& l : lm->levels())
                if (std::abs(l.elevation - z) < 1e-6) lvl = &l;
            const std::string id = lvl ? lvl->id : lm->addLevel(name, z)->id;
            m_storeyLevel[st->id] = id;
            m_bim.spatial().storeyGlobalIds[id] = st->arg(0).asText();
            ++m_report.storeys;
        }
    }

    // ------------------------------------------------------------------ modèle analytique
    std::vector<Vec3> topologyPoints(const StepEntity& item, const char* repType)
    {
        std::vector<Vec3> pts;
        const Frame f = placement(item.arg(5).ref);
        const auto* shape = m_r.get(item.arg(6));
        if (!shape) return pts;
        for (int rep : shape->arg(2).refs())
        {
            const auto* r = m_r.get(rep);
            if (!r || r->arg(2).asText() != repType) continue;
            for (int it : r->arg(3).refs())
            {
                const auto* t = m_r.get(it);
                if (!t) continue;
                auto vertex = [&](int v) {
                    if (const auto* vp = m_r.get(v); vp && vp->type == "IFCVERTEXPOINT") pts.push_back(f.point(point(vp->arg(0).ref)));
                };
                if (t->type == "IFCVERTEXPOINT") vertex(t->id);
                else if (t->type == "IFCEDGE") { vertex(t->arg(0).ref); vertex(t->arg(1).ref); }
                else if (t->type == "IFCFACESURFACE" || t->type == "IFCFACE")
                    for (int b : t->arg(0).refs())
                        if (const auto* bound = m_r.get(b))
                            if (const auto* loop = m_r.get(bound->arg(0)); loop && loop->type == "IFCPOLYLOOP")
                                for (int p : loop->arg(0).refs()) pts.push_back(f.point(point(p)));
            }
        }
        return pts;
    }

    static TSA::Model::DOFState dof(const StepValue& v, double& k)
    {
        if (v.kind == Kind::Typed && v.text != "IFCBOOLEAN" && v.text != "IFCLOGICAL")
        {
            k = v.asNumber();
            return TSA::Model::DOFState::Spring;
        }
        return v.asBool() ? TSA::Model::DOFState::Fixed : TSA::Model::DOFState::Free;
    }

    void importAnalytical()
    {
        using D = TSA::Model::DOFState;
        for (const auto* c : m_r.byType("IFCSTRUCTURALPOINTCONNECTION"))
        {
            const auto pts = topologyPoints(*c, "Vertex");
            if (pts.empty()) continue;
            const int n = node(pts.front());
            m_bim.setAnalyticalGlobalId({ ElementKind::Node, n }, c->arg(0).asText());
            const auto* bc = m_r.get(c->arg(7));
            if (!bc || bc->type != "IFCBOUNDARYNODECONDITION") continue;
            double k[6] = {};
            D s[6];
            for (int i = 0; i < 6; ++i) s[i] = dof(bc->arg(1 + i), k[i]);
            TSA::Model::SupportDefinition sup;
            const bool allFixed = std::all_of(s, s + 6, [](D d) { return d == D::Fixed; });
            const bool pinned = s[0] == D::Fixed && s[1] == D::Fixed && s[2] == D::Fixed && s[3] == D::Free && s[4] == D::Free && s[5] == D::Free;
            if (allFixed) sup = TSA::Model::SupportDefinition::fixed();
            else if (pinned) sup = TSA::Model::SupportDefinition::pinned();
            else
            {
                sup = TSA::Model::SupportDefinition::custom(s[0], s[1], s[2], s[3], s[4], s[5]);
                sup.setStiffnesses(k[0], k[1], k[2], k[3], k[4], k[5]);
            }
            if (auto* nd = m_m.getNode(n)) nd->setSupport(sup);
        }

        for (const auto* c : m_r.byType("IFCSTRUCTURALCURVEMEMBER"))
        {
            const auto pts = topologyPoints(*c, "Edge");
            if (pts.size() < 2) continue;
            const int a = node(pts[0]), b = node(pts[1]);
            if (a == b) continue;
            const std::string type = c->arg(7).asText();
            const auto* product = m_r.get(m_productOf.count(c->id) ? m_productOf[c->id] : 0);
            BimCategory cat = BimCategory::Beam;
            if (product) categoryOf(product->type, cat);
            const std::string pdt = product ? product->arg(8).asText() : "";

            TSA::Model::Material mat = TSA::Model::Material::steelS235();
            TSA::Model::Section sec = TSA::Model::Section::rectangular(0.3, 0.5);
            bool hasSection = false;
            if (!materialOf(c->id, mat, &sec, &hasSection) && product) materialOf(product->id, mat, &sec, &hasSection);
            if (!hasSection) m_report.warnings.push_back(c->arg(2).asText() + " : profil absent, section rectangulaire 300×500 par défaut.");

            // Angle de rotation γ : l'axe local z IFC (Axis) correspond à l'axe « hauteur » du profil TSA
            double rot = 0.0;
            if (const auto* ax = m_r.get(c->arg(8)))
            {
                Vec3 x0, y0, z;
                IfcGeometryMapper::barFrame(pts[0], pts[1], 0.0, x0, y0, z);
                const Vec3 y = direction(ax->id, y0);
                rot = std::atan2(-dot(y, x0), dot(y, y0)) * 180.0 / kPi;
                if (std::abs(rot) < 1e-9) rot = 0.0;
            }

            AnalyticalRef ref;
            if (type == "PIN_JOINED_MEMBER")
            {
                using R = TSA::Model::TrussMemberRole;
                const R role = pdt == "CHORD" ? R::TopChord : pdt == "BRACE" ? R::Brace : R::Diagonal;
                ref = { ElementKind::TrussMember, m_m.addTrussMember(a, b, 0.10, "", role) };
                if (auto* t = m_m.getTrussMember(ref.id)) { t->setSection(sec); t->setMaterial(mat); }
            }
            else if (type == "CABLE")
            {
                ref = { ElementKind::Cable, m_m.addCable(a, b, sec.shape == TSA::Model::SectionShape::Circular ? sec.diameter : 0.02) };
                if (auto* k = m_m.getCable(ref.id)) { k->setMaterial(mat); k->setSection(sec); }
            }
            else if (cat == BimCategory::Column)
                ref = { ElementKind::Column, m_m.addColumn(a, b, sec, mat, rot) };
            else
            {
                using TSA::Model::BarRole;
                BarRole role = BarRole::Beam;
                if (cat == BimCategory::Member)
                    role = pdt == "BRACE" ? BarRole::Brace : pdt == "TIEBAR" ? BarRole::Tie
                        : pdt == "STRUCTURALCABLE" ? BarRole::Cable : BarRole::Truss;
                ref = { ElementKind::Beam, m_m.addBar(a, b, sec, mat, role, rot) };
            }
            if (ref.id <= 0) continue;
            m_bim.setAnalyticalGlobalId(ref, c->arg(0).asText());
            m_analyticalRef[c->id] = ref;
            ++m_report.members;
        }

        for (const auto* c : m_r.byType("IFCSTRUCTURALSURFACEMEMBER"))
        {
            const auto* product = m_r.get(m_productOf.count(c->id) ? m_productOf[c->id] : 0);
            BimCategory cat = BimCategory::Slab;
            if (product) categoryOf(product->type, cat);
            if (cat != BimCategory::Slab) continue;   // voiles : reconstruits depuis le produit (hauteur, décalage)
            auto pts = topologyPoints(*c, "Face");
            if (pts.size() < 3) continue;
            std::vector<int> ids;
            for (const auto& p : pts) ids.push_back(node(p));
            const int s = m_m.addSlab(ids, c->arg(8).isNull() ? 0.2 : c->arg(8).asNumber() * m_len);
            if (s <= 0) continue;
            TSA::Model::Material mat;
            if (materialOf(c->id, mat, nullptr) || (product && materialOf(product->id, mat, nullptr)))
                if (auto* sl = m_m.getSlab(s)) sl->setMaterial(mat);
            const AnalyticalRef ref { ElementKind::Slab, s };
            m_bim.setAnalyticalGlobalId(ref, c->arg(0).asText());
            m_analyticalRef[c->id] = ref;
            ++m_report.surfaces;
        }
    }

    // ------------------------------------------------------------------ produits physiques
    struct Solid
    {
        const StepEntity* profile = nullptr;
        Frame frame;      ///< repère du solide (placement objet ∘ Position)
        Vec3 dir;         ///< direction d'extrusion (monde)
        double depth = 0;
    };

    void representation(const StepEntity& product, std::vector<Solid>& solids, std::vector<Vec3>& axis)
    {
        const Frame f = placement(product.arg(5).ref);
        const auto* shape = m_r.get(product.arg(6));
        if (!shape) return;
        for (int rep : shape->arg(2).refs())
        {
            const auto* r = m_r.get(rep);
            if (!r) continue;
            const std::string id = r->arg(1).asText();
            for (int it : r->arg(3).refs())
            {
                const auto* item = m_r.get(it);
                if (!item) continue;
                if (id == "Axis" && item->type == "IFCPOLYLINE" && axis.empty())
                    for (int p : item->arg(0).refs()) axis.push_back(f.point(point(p)));
                else if (item->type == "IFCEXTRUDEDAREASOLID")
                {
                    Solid s;
                    s.profile = m_r.get(item->arg(0));
                    s.frame = f * axisPlacement(item->arg(1).ref);
                    s.dir = s.frame.dir(direction(item->arg(2).ref, { 0, 0, 1 }));
                    s.depth = item->arg(3).asNumber() * m_len;
                    solids.push_back(s);
                }
                else if (id == "Body")
                    m_report.warnings.push_back(product.arg(2).asText() + " : représentation " + item->type + " non prise en charge.");
            }
        }
    }

    /// Repère 2D du profil (IfcParameterizedProfileDef.Position), dans le repère du solide.
    Frame profileFrame(const Solid& s) const
    {
        if (!s.profile || s.profile->arg(2).kind != Kind::Ref) return s.frame;
        return s.frame * axisPlacement(s.profile->arg(2).ref);
    }

    /// Profil rectangulaire (IfcRectangleProfileDef ou contour à 4 côtés orthogonaux) : repère au
    /// centre (x selon la cote dx) et dimensions, dans le repère monde.
    bool rectangle(const Solid& s, Frame& pf, double& dx, double& dy) const
    {
        if (!s.profile) return false;
        if (s.profile->type == "IFCRECTANGLEPROFILEDEF")
        {
            pf = profileFrame(s);
            dx = s.profile->arg(3).asNumber() * m_len;
            dy = s.profile->arg(4).asNumber() * m_len;
            return dx > 0 && dy > 0;
        }
        if (s.profile->type != "IFCARBITRARYCLOSEDPROFILEDEF") return false;
        const auto pts = curvePoints(m_r.get(s.profile->arg(2)));
        if (pts.size() != 4) return false;
        const Vec3 e0 = sub(pts[1], pts[0]), e1 = sub(pts[2], pts[1]);
        const double l0 = std::sqrt(dot(e0, e0)), l1 = std::sqrt(dot(e1, e1));
        if (l0 < 1e-9 || l1 < 1e-9 || std::abs(dot(e0, e1)) > 1e-6 * l0 * l1) return false;
        const Vec3 d = sub(add(pts[0], e1), pts[3]);
        if (dot(d, d) > 1e-12) return false;
        Frame local;
        local.o = mul(add(pts[0], pts[2]), 0.5);
        local.x = unit(e0);
        local.y = cross(local.z, local.x);
        pf = s.frame * local;
        dx = l0;
        dy = l1;
        return true;
    }

    std::vector<AnalyticalRef> fromGeometry(const StepEntity& product, BimCategory cat, const std::string& pdt)
    {
        std::vector<AnalyticalRef> refs;
        std::vector<Solid> solids;
        std::vector<Vec3> axis;
        representation(product, solids, axis);
        TSA::Model::Material mat = cat == BimCategory::Slab || cat == BimCategory::Wall || cat == BimCategory::Footing || cat == BimCategory::Pile
            ? TSA::Model::Material::concreteC25_30()
            : TSA::Model::Material::steelS235();
        TSA::Model::Section sec = TSA::Model::Section::rectangular(0.3, 0.5);
        bool hasSection = false;
        materialOf(product.id, mat, &sec, &hasSection);

        const bool linear = cat == BimCategory::Beam || cat == BimCategory::Column || cat == BimCategory::Member;
        if (linear)
        {
            if (!hasSection && !solids.empty()) hasSection = section(solids.front().profile ? solids.front().profile->id : 0, sec);
            std::vector<std::pair<Vec3, Vec3>> segments;
            if (axis.size() >= 2)
                for (std::size_t i = 1; i < axis.size(); ++i) segments.push_back({ axis[i - 1], axis[i] });
            else
                for (const auto& s : solids) segments.push_back({ s.frame.o, add(s.frame.o, mul(s.dir, s.depth)) });
            for (std::size_t i = 0; i < segments.size(); ++i)
            {
                const auto& [p, q] = segments[i];
                double rot = 0.0;
                if (i < solids.size())
                {
                    Vec3 x0, y0, z;
                    IfcGeometryMapper::barFrame(p, q, 0.0, x0, y0, z);
                    const Vec3 x = profileFrame(solids[i]).x;
                    rot = std::atan2(dot(x, y0), dot(x, x0)) * 180.0 / kPi;
                    if (std::abs(rot) < 1e-9) rot = 0.0;
                }
                const int a = node(p), b = node(q);
                if (a == b) continue;
                if (cat == BimCategory::Column) refs.push_back({ ElementKind::Column, m_m.addColumn(a, b, sec, mat, rot) });
                else
                {
                    using TSA::Model::BarRole;
                    const BarRole role = cat == BimCategory::Beam ? BarRole::Beam
                        : pdt == "BRACE" ? BarRole::Brace : pdt == "TIEBAR" ? BarRole::Tie : BarRole::Truss;
                    refs.push_back({ ElementKind::Beam, m_m.addBar(a, b, sec, mat, role, rot) });
                }
                ++m_report.members;
            }
            if (!hasSection && !refs.empty()) m_report.warnings.push_back(product.arg(2).asText() + " : profil non reconnu, section 300×500 par défaut.");
            return refs;
        }

        for (const auto& s : solids)
        {
            if (!s.profile) continue;
            const Frame pf = profileFrame(s);
            const bool up = s.dir[2] > 1e-9;
            Frame rectFrame;
            double rx = 0, ry = 0;
            const bool isRect = rectangle(s, rectFrame, rx, ry);
            if (cat == BimCategory::Footing || cat == BimCategory::Pile || (cat == BimCategory::Slab && pdt == "BASESLAB" && isRect))
            {
                // Semelle / pieu / radier : nœud au centre de la face supérieure
                const Vec3 centre = isRect ? rectFrame.o : pf.o;
                const Vec3 top = up ? add(centre, mul(s.dir, s.depth)) : centre;
                double a = 0, b = 0;
                if (s.profile->type == "IFCCIRCLEPROFILEDEF") a = b = 2.0 * s.profile->arg(3).asNumber() * m_len;
                else if (isRect) { a = rx; b = ry; }
                if (a <= 0 || b <= 0) continue;
                using F = TSA::Model::FoundationType;
                const F type = cat == BimCategory::Pile ? F::Pile : cat == BimCategory::Slab ? F::Raft : pdt == "STRIP_FOOTING" ? F::StripFooting : F::IsolatedFooting;
                const int f = m_m.addFoundation(node(top), a, b, s.depth, "", type);
                if (auto* fd = m_m.getFoundation(f)) fd->setMaterial(mat);
                refs.push_back({ ElementKind::Foundation, f });
                ++m_report.foundations;
            }
            else if (cat == BimCategory::Slab || cat == BimCategory::Plate)
            {
                std::vector<Vec3> pts;
                if (s.profile->type == "IFCARBITRARYCLOSEDPROFILEDEF")
                {
                    for (const auto& p : curvePoints(m_r.get(s.profile->arg(2)))) pts.push_back(s.frame.point(p));
                }
                else if (s.profile->type == "IFCRECTANGLEPROFILEDEF")
                {
                    const double x = s.profile->arg(3).asNumber() * m_len / 2, y = s.profile->arg(4).asNumber() * m_len / 2;
                    for (const Vec3& c : { Vec3 { -x, -y, 0 }, Vec3 { x, -y, 0 }, Vec3 { x, y, 0 }, Vec3 { -x, y, 0 } }) pts.push_back(pf.point(c));
                }
                if (pts.size() < 3) { m_report.warnings.push_back(product.arg(2).asText() + " : contour de dalle non pris en charge."); continue; }
                // Les nœuds TSA portent la face supérieure de la dalle
                if (up) for (auto& p : pts) p = add(p, mul(s.dir, s.depth));
                std::vector<int> ids;
                for (const auto& p : pts) ids.push_back(node(p));
                const int sl = m_m.addSlab(ids, s.depth);
                if (auto* slab = m_m.getSlab(sl)) slab->setMaterial(mat);
                refs.push_back({ ElementKind::Slab, sl });
                ++m_report.surfaces;
            }
            else if (cat == BimCategory::Wall)
            {
                Frame wf;
                double dx = 0, dy = 0;
                if (!rectangle(s, wf, dx, dy)) { m_report.warnings.push_back(product.arg(2).asText() + " : contour de voile non rectangulaire non pris en charge."); continue; }
                // Axe du voile selon la plus grande dimension ; décalage = distance du centre à l'axe du placement
                const Frame base = s.frame;
                Vec3 u = dx >= dy ? wf.x : wf.y;
                if (dot(u, base.x) < -1e-9 || (std::abs(dot(u, base.x)) <= 1e-9 && dot(u, base.y) < 0)) u = mul(u, -1.0);   // sens du placement
                const double len = std::max(dx, dy), thick = std::min(dx, dy);
                const Vec3 n = cross(base.z, u);
                const Vec3 c = wf.o;
                const double offset = dot(sub(c, base.o), n);
                Vec3 mid = sub(c, mul(n, offset));
                if (!up) mid = add(mid, mul(s.dir, s.depth));
                const int a = node(sub(mid, mul(u, len / 2))), b = node(add(mid, mul(u, len / 2)));
                const int w = m_m.addWall(a, b, s.depth, thick);
                if (auto* wall = m_m.getWall(w)) { wall->setOffset(offset); wall->setMaterial(mat); }
                refs.push_back({ ElementKind::Wall, w });
                ++m_report.surfaces;
            }
        }
        return refs;
    }

    void importProducts()
    {
        static const char* types[] = { "IFCBEAM", "IFCBEAMSTANDARDCASE", "IFCCOLUMN", "IFCCOLUMNSTANDARDCASE", "IFCMEMBER",
                                       "IFCMEMBERSTANDARDCASE", "IFCSLAB", "IFCSLABSTANDARDCASE", "IFCWALL", "IFCWALLSTANDARDCASE",
                                       "IFCFOOTING", "IFCPILE", "IFCPLATE" };
        for (const char* t : types)
            for (const auto* p : m_r.byType(t))
            {
                BimCategory cat;
                if (!categoryOf(p->type, cat)) continue;
                const std::string pdt = p->arg(8).asText();
                std::vector<AnalyticalRef> refs;
                for (int a : m_assigned[p->id])
                    if (auto it = m_analyticalRef.find(a); it != m_analyticalRef.end()) refs.push_back(it->second);
                if (refs.empty()) refs = fromGeometry(*p, cat, pdt);
                // GlobalId de la surface analytique d'un voile reconstruit depuis sa géométrie
                if (cat == BimCategory::Wall)
                    for (int a : m_assigned[p->id])
                        if (const auto* sm = m_r.get(a); sm && refs.size() == 1) m_bim.setAnalyticalGlobalId(refs.front(), sm->arg(0).asText());
                refs.erase(std::remove_if(refs.begin(), refs.end(), [](const AnalyticalRef& r) { return r.id <= 0; }), refs.end());
                if (refs.empty())
                {
                    m_report.warnings.push_back(p->arg(2).asText() + " (" + p->type + ") : aucun élément analytique déduit, produit ignoré.");
                    continue;
                }

                PhysicalElement e;
                e.globalId = p->arg(0).asText();
                e.category = cat;
                e.predefinedType = pdt == "NOTDEFINED" ? "" : pdt;
                e.name = p->arg(2).asText();
                e.description = p->arg(3).asText();
                e.objectType = p->arg(4).asText();
                e.tag = p->arg(7).asText();
                e.analytical = refs;
                for (int ps : m_psets[p->id])
                {
                    const auto* set = m_r.get(ps);
                    if (!set || set->type != "IFCPROPERTYSET") continue;
                    PropertySet out;
                    out.name = set->arg(2).asText();
                    for (int pr : set->arg(4).refs())
                        if (const auto* sv = m_r.get(pr); sv && sv->type == "IFCPROPERTYSINGLEVALUE" && !sv->arg(2).isNull())
                            out.properties[sv->arg(0).asText()] = propertyValue(sv->arg(2));
                    e.propertySets.push_back(out);
                }
                for (int c : m_classes[p->id])
                {
                    const auto* ref = m_r.get(c);
                    if (!ref || ref->type != "IFCCLASSIFICATIONREFERENCE") continue;
                    const auto* src = m_r.get(ref->arg(3));
                    e.classifications.push_back({ src ? src->arg(3).asText() : "", ref->arg(1).asText(), ref->arg(2).asText() });
                }
                if (auto it = m_container.find(p->id); it != m_container.end())
                    if (auto lv = m_storeyLevel.find(it->second); lv != m_storeyLevel.end()) e.storeyLevelId = lv->second;
                m_products.push_back(std::move(e));
                ++m_report.products;
            }
    }

    void finalizeBim()
    {
        for (auto& e : m_products)
        {
            // Valeurs par défaut de l'export (nom, repère, étage déduit, Psets calculés) non dupliquées
            if (e.name == defaultProductName(e)) e.name.clear();
            if (e.tag == defaultProductTag(e)) e.tag.clear();
            PhysicalElement bare = e;
            bare.propertySets.clear();
            bare.storeyLevelId.clear();
            if (!e.storeyLevelId.empty() && m_bim.resolvedStorey(m_m, bare) == e.storeyLevelId) e.storeyLevelId.clear();
            const auto computed = IfcPropertyMapper::propertySets(m_m, bare);
            std::vector<PropertySet> kept;
            for (auto& ps : e.propertySets)
            {
                if (ps.name == "Pset_TSA_Structural") continue;
                auto c = std::find_if(computed.begin(), computed.end(), [&](const PropertySet& x) { return x.name == ps.name; });
                for (auto it = ps.properties.begin(); it != ps.properties.end();)
                {
                    const bool same = c != computed.end() && c->properties.count(it->first) && c->properties.at(it->first).toString() == it->second.toString();
                    it = same ? ps.properties.erase(it) : std::next(it);
                }
                if (!ps.properties.empty()) kept.push_back(std::move(ps));
            }
            e.propertySets = std::move(kept);
            if (e.predefinedType.empty()) e.predefinedType = "NOTDEFINED";
            m_bim.insert(e);
        }
        m_m.setBim(m_bim);
        m_m.setModified(true);
    }

    const IfcStepReader& m_r;
    TSA::Model::Model& m_m;
    IfcImportOptions m_opt;
    IfcImportReport m_report;
    double m_len = 1.0;        ///< facteur de longueur du fichier → m
    double m_pressure = 1.0;   ///< facteur de contrainte du fichier → Pa
    BimModel m_bim;
    std::vector<PhysicalElement> m_products;

    std::map<int, int> m_material, m_productOf, m_container;
    std::map<int, std::vector<int>> m_psets, m_classes, m_assigned, m_materialProps;
    std::map<int, Frame> m_frames;
    std::map<std::array<long long, 3>, int> m_nodes;
    std::map<int, TSA::Model::Material> m_materials;
    std::map<int, AnalyticalRef> m_analyticalRef;   ///< #objet analytique → élément TSA
    std::map<int, std::string> m_storeyLevel;       ///< #étage → id de niveau
};
} // namespace

IfcImportReport IfcImporter::importString(const std::string& content, TSA::Model::Model& model, const IfcImportOptions& options)
{
    IfcStepReader reader;
    IfcImportReport r;
    if (!reader.parse(content, &r.error)) return r;
    const std::string& schema = reader.schema();
    if (schema.rfind("IFC", 0) != 0)
    {
        r.error = "schéma « " + schema + " » non IFC";
        return r;
    }
    Importer imp(reader, model, options);
    return imp.run();
}

IfcImportReport IfcImporter::importFile(const std::string& path, TSA::Model::Model& model, const IfcImportOptions& options)
{
    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::ReadOnly))
    {
        IfcImportReport r;
        r.error = "impossible d'ouvrir « " + path + " »";
        return r;
    }
    const QByteArray bytes = f.readAll();
    return importString(std::string(bytes.constData(), static_cast<std::size_t>(bytes.size())), model, options);
}

} // namespace TSA::BIM::Ifc
