#include "Tsa3dInternal.h"

#include "../../Analysis/ResultsModel.h"
#include "../../BIM/Core/BimModel.h"
#include "../../Coordinate/CoordinateSystem.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"
#include "App/ProductInfo.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <map>

namespace TSA::IO::Tsa3d
{

using namespace detail;
using namespace TSA::Model;

namespace
{
QJsonArray arr3(double a, double b, double c)
{
    return QJsonArray { a, b, c };
}

/// Données conservées d'un import TSA3D précédent (Model::exchangeExtensionsJson).
struct Preserved
{
    QJsonObject document;                     ///< blocs et champs de premier niveau inconnus
    QJsonObject objects;                      ///< identifiant TSA3D → champs inconnus de l'objet
    std::map<std::string, QString> externalId; ///< « famille:id interne » → identifiant TSA3D d'origine
};

Preserved preservedOf(const TSA::Model::Model& model)
{
    Preserved p;
    if (model.exchangeExtensionsJson().empty()) return p;
    const QJsonObject root = QJsonDocument::fromJson(QByteArray::fromStdString(model.exchangeExtensionsJson())).object();
    p.document = root.value("document").toObject();
    p.objects = root.value("objects").toObject();
    const QJsonObject map = root.value("idMap").toObject();
    for (auto it = map.begin(); it != map.end(); ++it) p.externalId[it.value().toString().toStdString()] = it.key();
    return p;
}

struct Ctx
{
    const TSA::Model::Model& model;
    Preserved preserved;
    Report& report;
    std::map<QString, QString> materialIds; ///< clé de contenu → identifiant
    std::map<QString, QString> sectionIds;
    QJsonArray materials, sections;

    QString idFor(const char* family, int internalId, const QString& prefix)
    {
        const auto it = preserved.externalId.find(std::string(family) + ":" + std::to_string(internalId));
        return it != preserved.externalId.end() ? it->second : prefix + QString::number(internalId);
    }
    /// Champs conservés de l'objet (import précédent) fusionnés, sans écraser les champs exportés.
    QJsonObject withPreserved(QJsonObject o)
    {
        const QJsonObject extra = preserved.objects.value(o.value("id").toString()).toObject();
        for (auto it = extra.begin(); it != extra.end(); ++it)
            if (!o.contains(it.key())) o.insert(it.key(), it.value());
        return o;
    }
    QString material(const Material& m)
    {
        const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                                .arg(QString::fromStdString(m.name)).arg(static_cast<int>(m.type))
                                .arg(m.mechanical.youngModulus, 0, 'g', 17).arg(m.mechanical.poissonRatio, 0, 'g', 17)
                                .arg(m.mechanical.density, 0, 'g', 17).arg(m.mechanical.yieldStrength, 0, 'g', 17)
                                .arg(m.mechanical.thermalCoeff, 0, 'g', 17);
        auto it = materialIds.find(key);
        if (it != materialIds.end()) return it->second;
        const QString id = QStringLiteral("MAT%1").arg(materialIds.size() + 1);
        materialIds[key] = id;
        QJsonObject o { { "id", id },
                        { "name", QString::fromStdString(m.name) },
                        { "type", enumName(materialTypes(), m.type) },
                        { "E", m.mechanical.youngModulus },
                        { "nu", m.mechanical.poissonRatio },
                        { "density", m.mechanical.density },
                        { "fk", m.mechanical.yieldStrength },
                        { "thermalCoeff", m.mechanical.thermalCoeff },
                        { "library", QJsonObject { { "source", "TSA" }, { "ref", QString::fromStdString(m.name) } } } };
        materials.append(withPreserved(o));
        return id;
    }
    QString section(const Section& s)
    {
        const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7").arg(QString::fromStdString(s.name)).arg(static_cast<int>(s.shape))
                                .arg(s.width, 0, 'g', 17).arg(s.height, 0, 'g', 17).arg(s.diameter, 0, 'g', 17)
                                .arg(s.tw, 0, 'g', 17).arg(s.tf, 0, 'g', 17);
        auto it = sectionIds.find(key);
        if (it != sectionIds.end()) return it->second;
        const QString id = QStringLiteral("SEC%1").arg(sectionIds.size() + 1);
        sectionIds[key] = id;
        QJsonObject dims { { "b", s.width }, { "h", s.height }, { "d", s.diameter }, { "tw", s.tw }, { "tf", s.tf } };
        // Propriétés calculées par TSA : informatives (recalculées à l'import à partir des dimensions).
        QJsonObject props { { "A", s.area() }, { "Iy", s.iy() }, { "Iz", s.iz() }, { "It", s.it() }, { "Wy", s.wy() }, { "Wz", s.wz() } };
        QJsonObject o { { "id", id },
                        { "name", QString::fromStdString(s.name) },
                        { "shape", enumName(sectionShapes(), s.shape) },
                        { "dimensions", dims },
                        { "properties", props },
                        { "library", QJsonObject { { "source", "TSA" }, { "ref", QString::fromStdString(s.name) } } } };
        sections.append(withPreserved(o));
        return id;
    }
    QString globalId(TSA::Model::ElementKind kind, int id) const
    {
        return QString::fromStdString(model.bim().analyticalGlobalId({ kind, id }));
    }
};

QJsonObject releaseJson(const EndRelease& r)
{
    return { { "fx", r.fx }, { "fy", r.fy }, { "fz", r.fz }, { "mx", r.mx }, { "my", r.my }, { "mz", r.mz } };
}

QJsonObject supportJson(const SupportDefinition& s)
{
    QJsonObject o;
    const DOFState dofs[] = { s.tx(), s.ty(), s.tz(), s.rx(), s.ry(), s.rz() };
    const char* names[] = { "tx", "ty", "tz", "rx", "ry", "rz" };
    for (int i = 0; i < 6; ++i) o.insert(names[i], enumName(dofStates(), dofs[i]));
    const double k[] = { s.kx(), s.ky(), s.kz(), s.krx(), s.kry(), s.krz() };
    const char* kn[] = { "kx", "ky", "kz", "krx", "kry", "krz" };
    QJsonObject stiff;
    for (int i = 0; i < 6; ++i)
        if (dofs[i] == DOFState::Spring) stiff.insert(kn[i], k[i]);
    if (!stiff.isEmpty()) o.insert("stiffness", stiff);
    if (s.orientationType() != SupportOrientationType::Global)
    {
        QJsonObject orient { { "type", enumName(orientationTypes(), s.orientationType()) } };
        if (s.orientationType() == SupportOrientationType::CustomVector) orient.insert("direction", arr3(s.customDirX(), s.customDirY(), s.customDirZ()));
        if (s.orientationType() == SupportOrientationType::LocalBar) orient.insert("memberInternalId", s.referenceElementId());
        o.insert("orientation", orient);
    }
    return o;
}

bool isFree(const SupportDefinition& s)
{
    return s.tx() == DOFState::Free && s.ty() == DOFState::Free && s.tz() == DOFState::Free && s.rx() == DOFState::Free &&
           s.ry() == DOFState::Free && s.rz() == DOFState::Free;
}

void putCommon(QJsonObject& o, const std::string& name, const std::string& color, const QString& globalId)
{
    if (!name.empty()) o.insert("name", QString::fromStdString(name));
    if (!color.empty()) o.insert("color", QString::fromStdString(color));
    if (!globalId.isEmpty()) o.insert("globalId", globalId);
}
} // namespace

ExportResult exportModel(const TSA::Model::Model& model, const ExportOptions& options)
{
    ExportResult res;
    Ctx c { model, preservedOf(model), res.report, {}, {}, {}, {} };
    QJsonObject doc;
    doc.insert("format", QString::fromLatin1(kFormatName));
    doc.insert("version", versionString());

    // --- Métadonnées -------------------------------------------------------------------------------
    QJsonObject meta = c.preserved.document.value("metadata").toObject();
    if (!meta.contains("id")) meta.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!options.projectName.isEmpty()) meta.insert("name", options.projectName);
    if (!options.description.isEmpty()) meta.insert("description", options.description);
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!meta.contains("created")) meta.insert("created", now);
    meta.insert("modified", now);
    meta.insert("producer", QJsonObject { { "name", TSA::Product::name() }, { "version", TSA::Product::version() } });
    doc.insert("metadata", meta);
    doc.insert("units", tsaUnits());
    doc.insert("coordinateSystem", QJsonObject { { "type", "cartesian" }, { "up", "Z" }, { "handedness", "right" } });

    // --- Niveaux --------------------------------------------------------------------------------------
    QJsonArray levels;
    if (model.coordinateSystem() && model.coordinateSystem()->levelManager())
        for (const auto& l : model.coordinateSystem()->levelManager()->levels())
            levels.append(c.withPreserved(QJsonObject { { "id", QString::fromStdString(l.id) }, { "name", QString::fromStdString(l.name) }, { "elevation", l.elevation } }));
    if (!levels.isEmpty()) doc.insert("levels", levels);

    // --- Nœuds ----------------------------------------------------------------------------------------
    QJsonArray nodes;
    for (const auto& [id, n] : model.nodes())
    {
        QJsonObject o { { "id", c.idFor("node", id, "N") }, { "position", arr3(n.x(), n.y(), n.z()) } };
        putCommon(o, n.name(), n.color(), c.globalId(ElementKind::Node, id));
        if (!n.levelId().empty()) o.insert("level", QString::fromStdString(n.levelId()));
        if (!isFree(n.support())) o.insert("support", supportJson(n.support()));
        nodes.append(c.withPreserved(o));
    }
    doc.insert("nodes", nodes);
    auto nodeRef = [&](int nodeId) { return c.idFor("node", nodeId, "N"); };

    // --- Barres ---------------------------------------------------------------------------------------
    QJsonArray members;
    for (const auto& [id, b] : model.beams())
    {
        QJsonObject o { { "id", c.idFor("beam", id, "B") }, { "type", "beam" }, { "role", enumName(barRoles(), b.role()) },
                        { "nodes", QJsonArray { nodeRef(b.startNodeId()), nodeRef(b.endNodeId()) } },
                        { "section", c.section(b.section()) }, { "material", c.material(b.material()) }, { "rotation", b.rotation() } };
        if (b.eccentricity() != BarEccentricity::None) o.insert("eccentricity", enumName(eccentricities(), b.eccentricity()));
        o.insert("releases", QJsonObject { { "start", releaseJson(b.startRelease()) }, { "end", releaseJson(b.endRelease()) } });
        putCommon(o, b.name(), b.color(), c.globalId(ElementKind::Beam, id));
        members.append(c.withPreserved(o));
    }
    for (const auto& [id, col] : model.columns())
    {
        QJsonObject o { { "id", c.idFor("column", id, "C") }, { "type", "column" }, { "role", "column" },
                        { "nodes", QJsonArray { nodeRef(col.startNodeId()), nodeRef(col.endNodeId()) } },
                        { "section", c.section(col.section()) }, { "material", c.material(col.material()) }, { "rotation", col.rotation() } };
        putCommon(o, col.name(), col.color(), c.globalId(ElementKind::Column, id));
        members.append(c.withPreserved(o));
    }
    for (const auto& [id, t] : model.trussMembers())
    {
        QJsonObject o { { "id", c.idFor("truss", id, "T") }, { "type", "truss" }, { "role", "truss" },
                        { "nodes", QJsonArray { nodeRef(t.startNodeId()), nodeRef(t.endNodeId()) } },
                        { "section", c.section(t.section()) }, { "material", c.material(t.material()) },
                        { "truss", QJsonObject { { "role", enumName(trussRoles(), t.role()) } } } };
        putCommon(o, t.name(), t.color(), c.globalId(ElementKind::TrussMember, id));
        members.append(c.withPreserved(o));
    }
    bool cableDetailLost = false;
    for (const auto& [id, k] : model.cables())
    {
        QJsonObject cable { { "type", enumName(cableTypes(), k.type()) },
                            { "geometry", enumName(cableModes(), k.geometryMode()) },
                            { "sag", k.sag() },
                            { "initialTension", k.initialTension() / 1000.0 },   // N → kN
                            { "tensionOnly", k.tensionOnly() } };
        QJsonObject o { { "id", c.idFor("cable", id, "K") }, { "type", "cable" }, { "role", "cable" },
                        { "nodes", QJsonArray { nodeRef(k.startNodeId()), nodeRef(k.endNodeId()) } },
                        { "section", c.section(k.section()) }, { "material", c.material(k.material()) }, { "cable", cable } };
        putCommon(o, k.name(), k.color(), c.globalId(ElementKind::Cable, id));
        members.append(c.withPreserved(o));
        cableDetailLost = true;
    }
    if (cableDetailLost)
        c.report.warning("members", QStringLiteral("câbles : définition détaillée (torons, ancrages, points intermédiaires) non "
                                                   "représentée en TSA3D v1 ; type, géométrie, flèche, tension initiale et section exportés"));
    doc.insert("members", members);

    // --- Surfaces -------------------------------------------------------------------------------------
    QJsonArray surfaces;
    for (const auto& [id, s] : model.slabs())
    {
        QJsonArray ns;
        for (int n : s.nodeIds()) ns.append(nodeRef(n));
        QJsonObject o { { "id", c.idFor("slab", id, "SL") }, { "type", "slab" }, { "nodes", ns }, { "thickness", s.thickness() },
                        { "material", c.material(s.material()) }, { "slabType", enumName(slabTypes(), s.slabType()) } };
        putCommon(o, s.name(), s.color(), c.globalId(ElementKind::Slab, id));
        surfaces.append(c.withPreserved(o));
    }
    for (const auto& [id, w] : model.walls())
    {
        QJsonObject o { { "id", c.idFor("wall", id, "W") }, { "type", "wall" },
                        { "nodes", QJsonArray { nodeRef(w.startNodeId()), nodeRef(w.endNodeId()) } }, { "height", w.height() },
                        { "thickness", w.thickness() }, { "offset", w.offset() }, { "material", c.material(w.material()) } };
        putCommon(o, w.name(), w.color(), c.globalId(ElementKind::Wall, id));
        surfaces.append(c.withPreserved(o));
    }
    if (!surfaces.isEmpty()) doc.insert("surfaces", surfaces);

    QJsonArray foundations;
    for (const auto& [id, f] : model.foundations())
    {
        QJsonObject o { { "id", c.idFor("foundation", id, "F") }, { "type", enumName(foundationTypes(), f.foundationType()) },
                        { "node", nodeRef(f.nodeId()) },
                        { "dimensions", QJsonObject { { "a", f.widthA() }, { "b", f.lengthB() }, { "h", f.heightH() } } },
                        { "material", c.material(f.material()) }, { "soilBearingCapacity", f.soilBearingCapacity() } };
        putCommon(o, f.name(), f.color(), c.globalId(ElementKind::Foundation, id));
        foundations.append(c.withPreserved(o));
    }
    if (!foundations.isEmpty()) doc.insert("foundations", foundations);

    // --- Charges ----------------------------------------------------------------------------------------
    const LoadManager::LoadSnapshot ls = model.loadManager().createSnapshot();
    QJsonArray cases, nodal, member, combos;
    auto caseRef = [&](int caseId) { return c.idFor("loadCase", caseId, "LC"); };
    for (const auto& [id, lc] : ls.loadCases)
        cases.append(c.withPreserved(QJsonObject { { "id", caseRef(id) }, { "name", QString::fromStdString(lc.name()) },
                                                   { "category", enumName(loadCaseCategories(), lc.category()) },
                                                   { "selfWeight", lc.isSelfWeightIncluded() }, { "selfWeightFactor", lc.selfWeightFactor() },
                                                   { "description", QString::fromStdString(lc.description()) } }));
    for (const auto& [id, l] : ls.nodalLoads)
        nodal.append(c.withPreserved(QJsonObject { { "id", c.idFor("nodalLoad", id, "NL") }, { "node", nodeRef(l.nodeId()) },
                                                   { "case", caseRef(l.loadCaseId()) }, { "force", arr3(l.fx(), l.fy(), l.fz()) },
                                                   { "moment", arr3(l.mx(), l.my(), l.mz()) },
                                                   { "coordinateSystem", enumName(coordSystems(), l.coordSystem()) } }));
    for (const auto& [id, l] : ls.memberLoads)
    {
        QString target;
        switch (l.targetType())
        {
        case MemberTargetType::Beam: target = c.idFor("beam", l.elementId(), "B"); break;
        case MemberTargetType::Column: target = c.idFor("column", l.elementId(), "C"); break;
        case MemberTargetType::Truss: target = c.idFor("truss", l.elementId(), "T"); break;
        case MemberTargetType::Cable: target = c.idFor("cable", l.elementId(), "K"); break;
        }
        QString kind = QStringLiteral("uniform");
        if (l.type() == LoadType::MemberLinear) kind = QStringLiteral("trapezoidal");
        else if (l.type() == LoadType::MemberPoint) kind = QStringLiteral("point");
        else if (l.type() == LoadType::MemberMoment) kind = QStringLiteral("moment");
        member.append(c.withPreserved(QJsonObject { { "id", c.idFor("memberLoad", id, "ML") }, { "member", target }, { "case", caseRef(l.loadCaseId()) },
                                                    { "kind", kind }, { "direction", enumName(loadDirections(), l.direction()) },
                                                    { "coordinateSystem", enumName(coordSystems(), l.coordSystem()) }, { "q1", l.q1() },
                                                    { "q2", l.q2() }, { "x1", l.x1() }, { "x2", l.x2() }, { "relative", l.isRelativePosition() } }));
    }
    for (const auto& [id, co] : ls.combinations)
    {
        QJsonArray factors;
        for (const auto& [caseId, factor] : co.caseFactors()) factors.append(QJsonObject { { "case", caseRef(caseId) }, { "factor", factor } });
        combos.append(c.withPreserved(QJsonObject { { "id", c.idFor("combination", id, "CO") }, { "name", QString::fromStdString(co.name()) },
                                                    { "type", enumName(combinationTypes(), co.type()) }, { "factors", factors } }));
    }
    QJsonObject loads;
    if (!cases.isEmpty()) loads.insert("cases", cases);
    if (!nodal.isEmpty()) loads.insert("nodal", nodal);
    if (!member.isEmpty()) loads.insert("member", member);
    if (!combos.isEmpty()) loads.insert("combinations", combos);
    for (const QString& k : c.preserved.document.value("loads").toObject().keys())   // ex. « surface » d'un fichier tiers
        if (!loads.contains(k)) loads.insert(k, c.preserved.document.value("loads").toObject().value(k));
    if (!loads.isEmpty()) doc.insert("loads", loads);

    if (!model.analysisSettingsJson().empty())
        doc.insert("analysis", QJsonObject { { "settings", QJsonDocument::fromJson(QByteArray::fromStdString(model.analysisSettingsJson())).object() } });

    // --- Maillage et résultats (seulement s'ils correspondent au modèle actuel) -----------------------
    const auto* r = options.results;
    if (r && r->isValid() && options.includeMesh && !r->solverMesh().empty())
    {
        const auto& mesh = r->solverMesh();
        QJsonArray mnodes, melems;
        for (const auto& [tag, n] : mesh.nodes)
        {
            QJsonObject o { { "id", QStringLiteral("MN%1").arg(tag) }, { "position", arr3(n.x, n.y, n.z) } };
            o.insert("origin", n.tsaNodeId > 0 ? QJsonValue(nodeRef(n.tsaNodeId)) : QJsonValue());
            if (!n.role.empty()) o.insert("role", QString::fromStdString(n.role));
            mnodes.append(o);
        }
        static const char* cellNames[] = { "line2", "zeroLength", "tri3", "quad4", "tet4", "hex8" };
        for (const auto& cell : mesh.cells)
        {
            QJsonArray en;
            for (int t : cell.nodes) en.append(QStringLiteral("MN%1").arg(t));
            QJsonObject o { { "id", QStringLiteral("ME%1").arg(cell.tag) }, { "type", cellNames[static_cast<int>(cell.type)] },
                            { "nodes", en }, { "class", QString::fromStdString(cell.solverClass) } };
            if (cell.source)
            {
                const char* fam = cell.source->kind == TSA::Analysis::StructuralElementKind::Beam ? "beam"
                                : cell.source->kind == TSA::Analysis::StructuralElementKind::Column ? "column"
                                : cell.source->kind == TSA::Analysis::StructuralElementKind::Truss ? "truss" : "cable";
                const char* pre = fam[0] == 'b' ? "B" : fam[0] == 'c' && fam[1] == 'o' ? "C" : fam[0] == 't' ? "T" : "K";
                o.insert("origin", c.idFor(fam, cell.source->id, QString::fromLatin1(pre)));
            }
            melems.append(o);
        }
        doc.insert("mesh", QJsonObject { { "source", QJsonObject { { "engine", QString::fromStdString(mesh.engineId) },
                                                                   { "description", QString::fromStdString(mesh.description) } } },
                                         { "nodes", mnodes }, { "elements", melems } });
    }
    else if (c.preserved.document.contains("mesh"))
        doc.insert("mesh", c.preserved.document.value("mesh"));
    if (r && r->isValid() && options.includeResults)
    {
        QJsonArray disp, reac;
        for (const auto& [nid, d] : r->allDisplacements())
            disp.append(QJsonObject { { "node", nodeRef(nid) }, { "u", arr3(d.ux, d.uy, d.uz) }, { "r", arr3(d.rx, d.ry, d.rz) } });
        for (const auto& [nid, re] : r->allReactions())
            reac.append(QJsonObject { { "node", nodeRef(nid) }, { "force", arr3(re.rx, re.ry, re.rz) }, { "moment", arr3(re.mx, re.my, re.mz) } });
        doc.insert("results", QJsonObject { { "status", "computed-not-validated" },
                                            { "engine", QString::fromStdString(r->executionMetadata().engineId) },
                                            { "case", QString::fromStdString(r->caseOrComboName()) },
                                            { "timestamp", QString::fromStdString(r->timestamp()) },
                                            { "displacements", disp }, { "reactions", reac } });
    }

    if (!c.materials.isEmpty()) doc.insert("materials", c.materials);
    if (!c.sections.isEmpty()) doc.insert("sections", c.sections);

    // --- Blocs conservés d'un import précédent (groupes, extensions, blocs inconnus) -----------------
    for (auto it = c.preserved.document.begin(); it != c.preserved.document.end(); ++it)
        if (it.key() != "metadata" && it.key() != "loads" && it.key() != "mesh" && !doc.contains(it.key())) doc.insert(it.key(), it.value());

    if (!model.dimensions().items.empty())
        c.report.info("annotations", QStringLiteral("cotations (annotations TSA) non exportées : hors du périmètre du format v1"));
    res.document = doc;
    return res;
}

} // namespace TSA::IO::Tsa3d
