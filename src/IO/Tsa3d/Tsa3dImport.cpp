#include "Tsa3dInternal.h"

#include "../../BIM/Core/BimModel.h"
#include "../../Coordinate/CoordinateSystem.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <map>
#include <set>

namespace TSA::IO::Tsa3d
{

using namespace detail;
using namespace TSA::Model;

namespace
{
/// Attribution des identifiants internes : un identifiant TSA3D de la forme <préfixe><entier> (convention
/// des exports TSA, ex. « N12 ») conserve son entier ; les autres reçoivent le premier entier libre.
class IdAllocator
{
public:
    explicit IdAllocator(QString prefix) : m_prefix(std::move(prefix)) {}
    void reserve(const QString& external)
    {
        if (const int n = conventional(external); n > 0 && !m_used.count(n))
        {
            m_used.insert(n);
            m_map[external] = n;
        }
    }
    int assign(const QString& external)
    {
        auto it = m_map.find(external);
        if (it != m_map.end()) return it->second;
        while (m_used.count(m_next)) ++m_next;
        m_used.insert(m_next);
        return m_map[external] = m_next;
    }
    int find(const QString& external) const
    {
        auto it = m_map.find(external);
        return it != m_map.end() ? it->second : 0;
    }

private:
    int conventional(const QString& id) const
    {
        static const QRegularExpression digits(QStringLiteral("^\\d+$"));
        if (!id.startsWith(m_prefix)) return 0;
        const QString rest = id.mid(m_prefix.size());
        return digits.match(rest).hasMatch() ? rest.toInt() : 0;
    }
    QString m_prefix;
    std::map<QString, int> m_map;
    std::set<int> m_used;
    int m_next = 1;
};

DOFState dof(const QJsonObject& s, const char* key)
{
    return enumValue<DOFState>(dofStates(), s.value(key).toString()).value_or(DOFState::Free);
}

EndRelease release(const QJsonObject& o)
{
    EndRelease r;
    r.fx = o.value("fx").toBool();
    r.fy = o.value("fy").toBool();
    r.fz = o.value("fz").toBool();
    r.mx = o.value("mx").toBool();
    r.my = o.value("my").toBool();
    r.mz = o.value("mz").toBool();
    return r;
}

std::string str(const QJsonObject& o, const char* key)
{
    return o.value(key).toString().toStdString();
}

/// Champs non interprétés de l'objet (conservés pour l'export).
QJsonObject unknownOf(const QJsonObject& o, const QStringList& known)
{
    QJsonObject out;
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!known.contains(it.key()) || it.key() == "extensions" || it.key() == "properties") out.insert(it.key(), it.value());
    return out;
}
} // namespace

ImportResult importDocument(const QJsonObject& doc, TSA::Model::Model& model, const ImportOptions& options)
{
    ImportResult res;
    res.report = validate(doc);
    if (res.report.hasErrors()) return res; // modèle inchangé
    const Units u = readUnits(doc, nullptr);
    const double moment = u.force * u.length, lineLoad = u.force / u.length;

    QJsonObject preservedObjects;
    QJsonObject idMap;
    auto keep = [&](const QJsonObject& o, const QString& coll, const char* family, int internalId) {
        const QString ext = o.value("id").toString();
        idMap.insert(ext, QStringLiteral("%1:%2").arg(QString::fromLatin1(family)).arg(internalId));
        res.idMap[ext.toStdString()] = std::string(family) + ":" + std::to_string(internalId);
        if (!options.keepUnknownFields) return;
        const QJsonObject extra = unknownOf(o, knownKeys(coll));
        if (!extra.isEmpty()) preservedObjects.insert(ext, extra);
    };

    // --- Matériaux et sections (valeurs copiées dans chaque élément, comme dans le modèle TSA) -------
    std::map<QString, Material> materials;
    for (const auto& v : doc.value("materials").toArray())
    {
        const QJsonObject o = v.toObject();
        const auto type = enumValue<MaterialType>(materialTypes(), o.value("type").toString()).value_or(MaterialType::Custom);
        Material m = Material::findByType(type);  // aspect visuel du type
        m.name = str(o, "name").empty() ? o.value("id").toString().toStdString() : str(o, "name");
        m.type = type;
        m.mechanical.youngModulus = o.value("E").toDouble() * u.stress;
        m.mechanical.poissonRatio = o.value("nu").toDouble(m.mechanical.poissonRatio);
        m.mechanical.density = o.value("density").toDouble(m.mechanical.density);
        m.mechanical.yieldStrength = o.contains("fk") ? o.value("fk").toDouble() * u.stress : m.mechanical.yieldStrength;
        m.mechanical.thermalCoeff = o.value("thermalCoeff").toDouble(m.mechanical.thermalCoeff);
        m.E = m.mechanical.youngModulus;
        m.nu = m.mechanical.poissonRatio;
        m.density = m.mechanical.density;
        m.fk = m.mechanical.yieldStrength;
        m.thermalCoeff = m.mechanical.thermalCoeff;
        materials[o.value("id").toString()] = m;
        if (options.keepUnknownFields)
            if (const QJsonObject extra = unknownOf(o, knownKeys("materials")); !extra.isEmpty()) preservedObjects.insert(o.value("id").toString(), extra);
    }
    std::map<QString, Section> sections;
    for (const auto& v : doc.value("sections").toArray())
    {
        const QJsonObject o = v.toObject();
        const QJsonObject d = o.value("dimensions").toObject();
        Section s;
        s.name = str(o, "name").empty() ? o.value("id").toString().toStdString() : str(o, "name");
        s.shape = enumValue<SectionShape>(sectionShapes(), o.value("shape").toString()).value_or(SectionShape::Rectangular);
        s.width = d.value("b").toDouble(s.width / u.length) * u.length;
        s.height = d.value("h").toDouble(s.height / u.length) * u.length;
        s.diameter = d.value("d").toDouble(s.diameter / u.length) * u.length;
        s.tw = d.value("tw").toDouble(s.tw / u.length) * u.length;
        s.tf = d.value("tf").toDouble(s.tf / u.length) * u.length;
        sections[o.value("id").toString()] = s;
        if (options.keepUnknownFields)
            if (const QJsonObject extra = unknownOf(o, knownKeys("sections")); !extra.isEmpty()) preservedObjects.insert(o.value("id").toString(), extra);
    }
    auto materialOf = [&](const QJsonObject& o, const Material& fallback) {
        auto it = materials.find(o.value("material").toString());
        return it != materials.end() ? it->second : fallback;
    };
    auto sectionOf = [&](const QJsonObject& o, const Section& fallback) {
        auto it = sections.find(o.value("section").toString());
        return it != sections.end() ? it->second : fallback;
    };

    TSA::Model::Model::ModelStateSnapshot snap;
    std::map<std::pair<ElementKind, int>, QString> globalIds;

    // --- Nœuds ----------------------------------------------------------------------------------------
    IdAllocator nodeIds(QStringLiteral("N"));
    const QJsonArray nodes = doc.value("nodes").toArray();
    for (const auto& v : nodes) nodeIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : nodes)
    {
        const QJsonObject o = v.toObject();
        const int id = nodeIds.assign(o.value("id").toString());
        const QJsonArray p = o.value("position").toArray();
        Node n(id, p[0].toDouble() * u.length, p[1].toDouble() * u.length, p[2].toDouble() * u.length, str(o, "level"), str(o, "name"));
        if (o.contains("support"))
        {
            const QJsonObject s = o.value("support").toObject();
            const QJsonObject k = s.value("stiffness").toObject();
            const double kt = u.force / u.length, kr = u.force * u.length;
            SupportDefinition sd(dof(s, "tx"), dof(s, "ty"), dof(s, "tz"), dof(s, "rx"), dof(s, "ry"), dof(s, "rz"),
                                 k.value("kx").toDouble() * kt, k.value("ky").toDouble() * kt, k.value("kz").toDouble() * kt,
                                 k.value("krx").toDouble() * kr, k.value("kry").toDouble() * kr, k.value("krz").toDouble() * kr);
            const QJsonObject orient = s.value("orientation").toObject();
            if (!orient.isEmpty())
            {
                sd.setOrientationType(enumValue<SupportOrientationType>(orientationTypes(), orient.value("type").toString()).value_or(SupportOrientationType::Global));
                const QJsonArray d = orient.value("direction").toArray();
                if (d.size() == 3) sd.setCustomDirection(d[0].toDouble(), d[1].toDouble(), d[2].toDouble());
                sd.setReferenceElementId(orient.value("memberInternalId").toInt());
            }
            n.setSupport(sd);
        }
        n.setColor(str(o, "color"));
        snap.nodes[id] = n;
        if (o.contains("globalId")) globalIds[{ ElementKind::Node, id }] = o.value("globalId").toString();
        keep(o, "nodes", "node", id);
        ++res.nodes;
    }
    auto nodeId = [&](const QJsonValue& v) { return nodeIds.find(v.toString()); };

    // --- Barres ---------------------------------------------------------------------------------------
    IdAllocator beamIds(QStringLiteral("B")), columnIds(QStringLiteral("C")), trussIds(QStringLiteral("T")), cableIds(QStringLiteral("K"));
    const QJsonArray members = doc.value("members").toArray();
    for (const auto& v : members)
    {
        const QJsonObject o = v.toObject();
        const QString t = o.value("type").toString();
        (t == "column" ? columnIds : t == "truss" ? trussIds : t == "cable" ? cableIds : beamIds).reserve(o.value("id").toString());
    }
    std::map<QString, std::pair<MemberTargetType, int>> memberTargets;
    for (const auto& v : members)
    {
        const QJsonObject o = v.toObject();
        const QString t = o.value("type").toString();
        const QJsonArray mn = o.value("nodes").toArray();
        const int a = nodeId(mn[0]), b = nodeId(mn[1]);
        const double rot = o.value("rotation").toDouble() * u.angle;
        if (t == "column")
        {
            const int id = columnIds.assign(o.value("id").toString());
            Column c(id, a, b, sectionOf(o, Section::rectangular(0.30, 0.30)), materialOf(o, Material::concreteC25_30()), rot, str(o, "name"));
            c.setColor(str(o, "color"));
            snap.columns[id] = c;
            memberTargets[o.value("id").toString()] = { MemberTargetType::Column, id };
            if (o.contains("globalId")) globalIds[{ ElementKind::Column, id }] = o.value("globalId").toString();
            keep(o, "members", "column", id);
        }
        else if (t == "truss")
        {
            const int id = trussIds.assign(o.value("id").toString());
            const auto role = enumValue<TrussMemberRole>(trussRoles(), o.value("truss").toObject().value("role").toString()).value_or(TrussMemberRole::Diagonal);
            TrussMember tm(id, a, b, 0.10, str(o, "name"), role);
            tm.setSection(sectionOf(o, tm.section()));
            tm.setMaterial(materialOf(o, tm.material()));
            tm.setColor(str(o, "color"));
            snap.trussMembers[id] = tm;
            memberTargets[o.value("id").toString()] = { MemberTargetType::Truss, id };
            if (o.contains("globalId")) globalIds[{ ElementKind::TrussMember, id }] = o.value("globalId").toString();
            keep(o, "members", "truss", id);
        }
        else if (t == "cable")
        {
            const int id = cableIds.assign(o.value("id").toString());
            const QJsonObject co = o.value("cable").toObject();
            Cable k(id, a, b, str(o, "name"), enumValue<CableType>(cableTypes(), co.value("type").toString()).value_or(CableType::Generic));
            k.setSection(sectionOf(o, k.section()));
            k.setMaterial(materialOf(o, k.material()));
            k.setGeometryMode(enumValue<CableGeometryMode>(cableModes(), co.value("geometry").toString()).value_or(CableGeometryMode::Straight));
            k.setSag(co.value("sag").toDouble() * u.length);
            k.setInitialTension(co.value("initialTension").toDouble() * u.force * 1000.0); // kN → N (stockage TSA)
            if (co.contains("tensionOnly")) k.setTensionOnly(co.value("tensionOnly").toBool());
            k.setColor(str(o, "color"));
            snap.cables[id] = k;
            memberTargets[o.value("id").toString()] = { MemberTargetType::Cable, id };
            if (o.contains("globalId")) globalIds[{ ElementKind::Cable, id }] = o.value("globalId").toString();
            keep(o, "members", "cable", id);
        }
        else
        {
            const int id = beamIds.assign(o.value("id").toString());
            const auto role = enumValue<BarRole>(barRoles(), o.value("role").toString()).value_or(BarRole::Beam);
            Beam bm(id, a, b, sectionOf(o, Section::ipe(200)), materialOf(o, Material::steelS235()), role, rot, str(o, "name"));
            bm.setEccentricity(enumValue<BarEccentricity>(eccentricities(), o.value("eccentricity").toString()).value_or(BarEccentricity::None));
            const QJsonObject rel = o.value("releases").toObject();
            bm.setStartRelease(release(rel.value("start").toObject()));
            bm.setEndRelease(release(rel.value("end").toObject()));
            bm.setColor(str(o, "color"));
            snap.beams[id] = bm;
            memberTargets[o.value("id").toString()] = { MemberTargetType::Beam, id };
            if (o.contains("globalId")) globalIds[{ ElementKind::Beam, id }] = o.value("globalId").toString();
            keep(o, "members", "beam", id);
        }
        ++res.members;
    }

    // --- Surfaces, fondations ---------------------------------------------------------------------------
    IdAllocator slabIds(QStringLiteral("SL")), wallIds(QStringLiteral("W")), foundationIds(QStringLiteral("F"));
    const QJsonArray surfaces = doc.value("surfaces").toArray();
    for (const auto& v : surfaces)
        (v.toObject().value("type").toString() == "wall" ? wallIds : slabIds).reserve(v.toObject().value("id").toString());
    for (const auto& v : surfaces)
    {
        const QJsonObject o = v.toObject();
        const QJsonArray sn = o.value("nodes").toArray();
        if (o.value("type").toString() == "wall")
        {
            const int id = wallIds.assign(o.value("id").toString());
            Wall w(id, nodeId(sn[0]), nodeId(sn[1]), o.value("height").toDouble() * u.length, o.value("thickness").toDouble() * u.length, str(o, "name"));
            w.setOffset(o.value("offset").toDouble() * u.length);
            w.setMaterial(materialOf(o, w.material()));
            w.setColor(str(o, "color"));
            snap.walls[id] = w;
            if (o.contains("globalId")) globalIds[{ ElementKind::Wall, id }] = o.value("globalId").toString();
            keep(o, "surfaces", "wall", id);
        }
        else
        {
            const int id = slabIds.assign(o.value("id").toString());
            std::vector<int> ns;
            for (const auto& n : sn) ns.push_back(nodeId(n));
            Slab s(id, ns, o.value("thickness").toDouble() * u.length, str(o, "name"),
                   enumValue<SlabType>(slabTypes(), o.value("slabType").toString()).value_or(SlabType::TwoWay));
            s.setMaterial(materialOf(o, s.material()));
            s.setColor(str(o, "color"));
            snap.slabs[id] = s;
            if (o.contains("globalId")) globalIds[{ ElementKind::Slab, id }] = o.value("globalId").toString();
            keep(o, "surfaces", "slab", id);
        }
        ++res.surfaces;
    }
    const QJsonArray foundations = doc.value("foundations").toArray();
    for (const auto& v : foundations) foundationIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : foundations)
    {
        const QJsonObject o = v.toObject();
        const int id = foundationIds.assign(o.value("id").toString());
        const QJsonObject d = o.value("dimensions").toObject();
        Foundation f(id, nodeId(o.value("node")), d.value("a").toDouble() * u.length, d.value("b").toDouble() * u.length,
                     d.value("h").toDouble() * u.length, str(o, "name"),
                     enumValue<FoundationType>(foundationTypes(), o.value("type").toString()).value_or(FoundationType::IsolatedFooting));
        f.setMaterial(materialOf(o, f.material()));
        if (o.contains("soilBearingCapacity")) f.setSoilBearingCapacity(o.value("soilBearingCapacity").toDouble() * u.pressure);
        f.setColor(str(o, "color"));
        snap.foundations[id] = f;
        if (o.contains("globalId")) globalIds[{ ElementKind::Foundation, id }] = o.value("globalId").toString();
        keep(o, "foundations", "foundation", id);
        ++res.foundations;
    }

    // --- Charges --------------------------------------------------------------------------------------
    const QJsonObject loads = doc.value("loads").toObject();
    LoadManager::LoadSnapshot& ls = snap.loadSnapshot;
    IdAllocator caseIds(QStringLiteral("LC")), nodalIds(QStringLiteral("NL")), memberLoadIds(QStringLiteral("ML")), comboIds(QStringLiteral("CO"));
    for (const auto& v : loads.value("cases").toArray()) caseIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : loads.value("cases").toArray())
    {
        const QJsonObject o = v.toObject();
        const int id = caseIds.assign(o.value("id").toString());
        ls.loadCases[id] = LoadCase(id, str(o, "name"),
                                    enumValue<LoadCaseCategory>(loadCaseCategories(), o.value("category").toString()).value_or(LoadCaseCategory::Custom),
                                    o.value("selfWeight").toBool(), o.value("selfWeightFactor").toDouble(1.0), str(o, "description"));
        keep(o, "loadCases", "loadCase", id);
        ++res.loadCases;
    }
    auto caseId = [&](const QJsonValue& v) { return caseIds.find(v.toString()); };
    for (const auto& v : loads.value("nodal").toArray()) nodalIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : loads.value("nodal").toArray())
    {
        const QJsonObject o = v.toObject();
        const int id = nodalIds.assign(o.value("id").toString());
        const QJsonArray f = o.value("force").toArray(), m = o.value("moment").toArray();
        ls.nodalLoads[id] = NodalLoad(id, nodeId(o.value("node")), caseId(o.value("case")), f.at(0).toDouble() * u.force,
                                      f.at(1).toDouble() * u.force, f.at(2).toDouble() * u.force, m.at(0).toDouble() * moment,
                                      m.at(1).toDouble() * moment, m.at(2).toDouble() * moment,
                                      enumValue<LoadCoordSystem>(coordSystems(), o.value("coordinateSystem").toString()).value_or(LoadCoordSystem::Global),
                                      str(o, "name"));
        keep(o, "nodalLoads", "nodalLoad", id);
        ++res.loads;
    }
    for (const auto& v : loads.value("member").toArray()) memberLoadIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : loads.value("member").toArray())
    {
        const QJsonObject o = v.toObject();
        const int id = memberLoadIds.assign(o.value("id").toString());
        const auto target = memberTargets.at(o.value("member").toString());
        const QString kind = o.value("kind").toString();
        const bool relative = o.value("relative").toBool();
        const double pos = relative ? 1.0 : u.length;
        const double intensity = kind == "point" ? u.force : lineLoad;   // moment réparti : kN·m/m = kN
        MemberLoad l;
        l.setId(id);
        l.setElementId(target.second);
        l.setTargetType(target.first);
        l.setLoadCaseId(caseId(o.value("case")));
        l.setName(str(o, "name"));
        l.setType(kind == "trapezoidal" ? LoadType::MemberLinear : kind == "point" ? LoadType::MemberPoint
                  : kind == "moment" ? LoadType::MemberMoment : LoadType::MemberUniform);
        l.setDirection(enumValue<LoadDirection>(loadDirections(), o.value("direction").toString()).value_or(LoadDirection::Gravity));
        l.setCoordSystem(enumValue<LoadCoordSystem>(coordSystems(), o.value("coordinateSystem").toString()).value_or(LoadCoordSystem::Global));
        l.setQ1(o.value("q1").toDouble() * intensity);
        l.setQ2((o.contains("q2") ? o.value("q2").toDouble() : o.value("q1").toDouble()) * intensity);
        l.setRelativePosition(relative);
        l.setX1(o.value("x1").toDouble(0.0) * pos);
        l.setX2(o.contains("x2") ? o.value("x2").toDouble() * pos : (relative ? 1.0 : 0.0));
        ls.memberLoads[id] = l;
        keep(o, "memberLoads", "memberLoad", id);
        ++res.loads;
    }
    for (const auto& v : loads.value("combinations").toArray()) comboIds.reserve(v.toObject().value("id").toString());
    for (const auto& v : loads.value("combinations").toArray())
    {
        const QJsonObject o = v.toObject();
        const int id = comboIds.assign(o.value("id").toString());
        std::map<int, double> factors;
        for (const auto& f : o.value("factors").toArray()) factors[caseId(f.toObject().value("case"))] = f.toObject().value("factor").toDouble();
        ls.combinations[id] = LoadCombination(id, str(o, "name"),
                                              enumValue<LoadCombinationType>(combinationTypes(), o.value("type").toString()).value_or(LoadCombinationType::Custom),
                                              factors, str(o, "description"));
        keep(o, "combinations", "combination", id);
        ++res.combinations;
    }
    auto nextOf = [](const auto& map) {
        int m = 0;
        for (const auto& [id, _] : map) m = std::max(m, id);
        return m + 1;
    };
    ls.nextLoadCaseId = nextOf(ls.loadCases);
    ls.nextNodalLoadId = nextOf(ls.nodalLoads);
    ls.nextMemberLoadId = nextOf(ls.memberLoads);
    ls.nextCombinationId = nextOf(ls.combinations);
    ls.activeLoadCaseId = ls.loadCases.empty() ? 1 : ls.loadCases.begin()->first;
    if (ls.loadCases.empty())
    {
        // Aucun cas dans le fichier : cas et combinaisons par défaut, comme un nouveau projet.
        ls = LoadManager().createSnapshot();
        res.report.info("loads", QStringLiteral("aucun cas de charge : cas par défaut d'un nouveau projet créés"));
    }

    snap.nextNodeId = nextOf(snap.nodes);
    snap.nextBeamId = nextOf(snap.beams);
    snap.nextColumnId = nextOf(snap.columns);
    snap.nextSlabId = nextOf(snap.slabs);
    snap.nextWallId = nextOf(snap.walls);
    snap.nextFoundationId = nextOf(snap.foundations);
    snap.nextTrussMemberId = nextOf(snap.trussMembers);
    snap.nextCableId = nextOf(snap.cables);
    snap.actionName = "Import TSA3D";

    // --- Application (comme l'ouverture d'un projet) ---------------------------------------------------
    if (model.coordinateSystem() && model.coordinateSystem()->levelManager() && doc.contains("levels"))
    {
        auto* lm = model.coordinateSystem()->levelManager();
        lm->clear();
        for (const auto& v : doc.value("levels").toArray())
        {
            const QJsonObject o = v.toObject();
            lm->addLevelWithId(str(o, "id"), str(o, "name").empty() ? str(o, "id") : str(o, "name"), o.value("elevation").toDouble() * u.length);
        }
    }
    model.restoreSnapshot(snap);
    for (const auto& [key, gid] : globalIds) model.bimForEdit().setAnalyticalGlobalId({ key.first, key.second }, gid.toStdString());
    if (doc.value("analysis").toObject().contains("settings"))
        model.setAnalysisSettingsJson(QJsonDocument(doc.value("analysis").toObject().value("settings").toObject()).toJson(QJsonDocument::Compact).toStdString());

    // --- Données non interprétées conservées -----------------------------------------------------------
    QJsonObject preservedDoc;
    for (auto it = doc.begin(); it != doc.end(); ++it)
        if (!knownTopLevelKeys().contains(it.key()) || it.key() == "groups" || it.key() == "extensions" || it.key() == "mesh" ||
            it.key() == "results" || it.key() == "metadata")
            preservedDoc.insert(it.key(), it.value());
    QJsonObject preservedLoads;
    for (auto it = loads.begin(); it != loads.end(); ++it)
        if (it.key() != "cases" && it.key() != "nodal" && it.key() != "member" && it.key() != "combinations") preservedLoads.insert(it.key(), it.value());
    if (!preservedLoads.isEmpty()) preservedDoc.insert("loads", preservedLoads);
    if (options.keepUnknownFields)
    {
        const QJsonObject root { { "tsa3dVersion", doc.value("version") }, { "document", preservedDoc }, { "objects", preservedObjects }, { "idMap", idMap } };
        model.setExchangeExtensionsJson(QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString());
    }
    if (doc.contains("results"))
        res.report.warning("results", QStringLiteral("résultats du fichier conservés à titre d'information, non chargés comme résultats de calcul"));
    if (doc.contains("mesh"))
        res.report.info("mesh", QStringLiteral("maillage du fichier conservé, non appliqué (TSA recalcule et le moteur maille lui-même)"));
    res.ok = true;
    return res;
}

} // namespace TSA::IO::Tsa3d
