#include "EngineeringContext.h"

#include "../../Analysis/ResultsContext.h"
#include "../../Analysis/ResultsModel.h"
#include "../../Model/Model.h"

#include <cmath>
#include <map>

namespace TSA::AI
{

using TSA::Model::LoadCombinationType;
using TSA::Model::LoadType;

namespace
{

double round4(double v) { return std::abs(v) < 1e-12 ? 0.0 : std::round(v * 1e4) / 1e4; }
double sig(double v, int digits = 4)
{
    if (v == 0.0 || !std::isfinite(v)) return v;
    const double scale = std::pow(10.0, digits - 1 - static_cast<int>(std::floor(std::log10(std::abs(v)))));
    return std::round(v * scale) / scale;
}

QString comboTypeName(LoadCombinationType t)
{
    switch (t)
    {
    case LoadCombinationType::ULS_Fundamental: return "ELU fondamental";
    case LoadCombinationType::ULS_Accidental: return "ELU accidentel";
    case LoadCombinationType::ULS_Seismic: return "ELU sismique";
    case LoadCombinationType::SLS_Characteristic: return "ELS caractéristique";
    case LoadCombinationType::SLS_Frequent: return "ELS fréquent";
    case LoadCombinationType::SLS_QuasiPermanent: return "ELS quasi-permanent";
    case LoadCombinationType::Custom: return "Personnalisée";
    }
    return "?";
}

QString loadTypeName(LoadType t)
{
    switch (t)
    {
    case LoadType::NodalForce: return "force nodale";
    case LoadType::NodalMoment: return "moment nodal";
    case LoadType::MemberUniform: return "linéique uniforme";
    case LoadType::MemberLinear: return "linéique variable";
    case LoadType::MemberPoint: return "ponctuelle sur barre";
    case LoadType::MemberMoment: return "moment réparti";
    case LoadType::SelfWeight: return "poids propre";
    }
    return "?";
}

QJsonObject sectionJson(const TSA::Model::Section& s)
{
    return QJsonObject{ { "name", QString::fromStdString(s.name) },
                        { "A_m2", sig(s.area()) }, { "Iy_m4", sig(s.iy()) }, { "Iz_m4", sig(s.iz()) },
                        { "It_m4", sig(s.it()) }, { "Wy_m3", sig(s.wy()) }, { "Wz_m3", sig(s.wz()) } };
}

QJsonObject materialJson(const TSA::Model::Material& m)
{
    return QJsonObject{ { "name", QString::fromStdString(m.name) },
                        { "E_GPa", sig(m.E / 1e9) }, { "nu", sig(m.nu) },
                        { "density_kg_m3", sig(m.density) }, { "fk_MPa", sig(m.fk / 1e6) } };
}

QJsonObject supportJson(const TSA::Model::SupportDefinition& s)
{
    using TSA::Model::DOFState;
    auto st = [](DOFState d) { return d == DOFState::Fixed ? "fixe" : (d == DOFState::Spring ? "ressort" : "libre"); };
    return QJsonObject{ { "UX", st(s.tx()) }, { "UY", st(s.ty()) }, { "UZ", st(s.tz()) },
                        { "RX", st(s.rx()) }, { "RY", st(s.ry()) }, { "RZ", st(s.rz()) } };
}

TSA::Analysis::StructuralElementKind kindOf(const QString& type, bool* ok)
{
    using K = TSA::Analysis::StructuralElementKind;
    *ok = true;
    if (type == "beam") return K::Beam;
    if (type == "column") return K::Column;
    if (type == "truss") return K::Truss;
    if (type == "cable") return K::Cable;
    *ok = false;
    return K::Beam;
}

} // namespace

QString EngineeringContextBuilder::typeFromKey(const QString& anyCase)
{
    const QString k = anyCase.trimmed().toLower();
    if (k == "node" || k == "noeud" || k == "nœud" || k == "n") return "node";
    if (k == "beam" || k == "poutre" || k == "b" || k == "bar" || k == "barre") return "beam";
    if (k == "column" || k == "poteau" || k == "c") return "column";
    if (k == "slab" || k == "dalle" || k == "s") return "slab";
    if (k == "wall" || k == "voile" || k == "mur" || k == "w") return "wall";
    if (k == "foundation" || k == "fondation" || k == "semelle" || k == "f") return "foundation";
    if (k == "truss" || k == "treillis" || k == "t") return "truss";
    if (k == "cable" || k == "câble" || k == "k") return "cable";
    return k;
}

QJsonObject EngineeringContextBuilder::units()
{
    return QJsonObject{ { "length", "m" }, { "force", "kN" }, { "moment", "kN·m" },
                        { "linearLoad", "kN/m" }, { "modulus", "GPa" }, { "strength", "MPa" },
                        { "note", "Géométrie en m ; charges du modèle en kN et kN/m ; unités des résultats indiquées dans results.units" } };
}

QJsonObject EngineeringContextBuilder::projectInfo(const EngineeringSources& src)
{
    const auto& m = *src.model;
    QJsonObject counts{
        { "nodes", static_cast<int>(m.nodes().size()) }, { "beams", static_cast<int>(m.beams().size()) },
        { "columns", static_cast<int>(m.columns().size()) }, { "slabs", static_cast<int>(m.slabs().size()) },
        { "walls", static_cast<int>(m.walls().size()) }, { "foundations", static_cast<int>(m.foundations().size()) },
        { "trussMembers", static_cast<int>(m.trussMembers().size()) }, { "cables", static_cast<int>(m.cables().size()) },
        { "supportedNodes", static_cast<int>(m.supportedNodeIds().size()) },
        { "loadCases", static_cast<int>(m.loadManager().loadCases().size()) },
        { "loadCombinations", static_cast<int>(m.loadManager().combinations().size()) },
        { "nodalLoads", static_cast<int>(m.loadManager().nodalLoads().size()) },
        { "memberLoads", static_cast<int>(m.loadManager().memberLoads().size()) } };

    // Emprise géométrique
    double minX = 1e300, minY = 1e300, minZ = 1e300, maxX = -1e300, maxY = -1e300, maxZ = -1e300;
    for (const auto& [id, n] : m.nodes())
    {
        minX = std::min(minX, n.x()); maxX = std::max(maxX, n.x());
        minY = std::min(minY, n.y()); maxY = std::max(maxY, n.y());
        minZ = std::min(minZ, n.z()); maxZ = std::max(maxZ, n.z());
    }
    QJsonObject extent;
    if (!m.nodes().empty())
        extent = QJsonObject{ { "X_m", QJsonArray{ round4(minX), round4(maxX) } },
                              { "Y_m", QJsonArray{ round4(minY), round4(maxY) } },
                              { "Z_m", QJsonArray{ round4(minZ), round4(maxZ) } } };

    return QJsonObject{ { "name", src.projectName.isEmpty() ? QStringLiteral("Sans titre") : src.projectName },
                        { "modelRevision", static_cast<double>(m.revision()) },
                        { "counts", counts }, { "extent", extent },
                        { "analysisScope", "Le calcul OpenSees porte sur les barres (poutres, poteaux), treillis, câbles et appuis ; dalles, voiles et fondations n'y sont pas transmis." } };
}

QJsonArray EngineeringContextBuilder::materials(const TSA::Model::Model& model)
{
    std::map<QString, std::pair<QJsonObject, int>> used;
    auto add = [&](const TSA::Model::Material& mat) {
        auto& e = used[QString::fromStdString(mat.name)];
        if (e.second == 0) e.first = materialJson(mat);
        ++e.second;
    };
    for (const auto& [id, b] : model.beams()) add(b.material());
    for (const auto& [id, c] : model.columns()) add(c.material());
    QJsonArray arr;
    for (auto& [name, e] : used)
    {
        e.first["usedByMembers"] = e.second;
        arr.append(e.first);
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::sections(const TSA::Model::Model& model)
{
    std::map<QString, std::pair<QJsonObject, int>> used;
    auto add = [&](const TSA::Model::Section& s) {
        auto& e = used[QString::fromStdString(s.name)];
        if (e.second == 0) e.first = sectionJson(s);
        ++e.second;
    };
    for (const auto& [id, b] : model.beams()) add(b.section());
    for (const auto& [id, c] : model.columns()) add(c.section());
    QJsonArray arr;
    for (auto& [name, e] : used)
    {
        e.first["usedByMembers"] = e.second;
        arr.append(e.first);
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::supports(const TSA::Model::Model& model, int limit)
{
    QJsonArray arr;
    for (const auto& [id, n] : model.nodes())
    {
        if (!n.support().isSupported()) continue;
        if (arr.size() >= limit) break;
        QJsonObject o = supportJson(n.support());
        o["node"] = QStringLiteral("N%1").arg(id);
        o["position_m"] = QJsonArray{ round4(n.x()), round4(n.y()), round4(n.z()) };
        arr.append(o);
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::loadCases(const TSA::Model::Model& model)
{
    const auto& lm = model.loadManager();
    std::map<int, int> counts;
    for (const auto& [id, l] : lm.nodalLoads()) ++counts[l.loadCaseId()];
    for (const auto& [id, l] : lm.memberLoads()) ++counts[l.loadCaseId()];
    QJsonArray arr;
    for (const auto& [id, lc] : lm.loadCases())
    {
        arr.append(QJsonObject{ { "id", id }, { "name", QString::fromStdString(lc.name()) },
                                { "category", QString::fromStdString(TSA::Model::loadCategoryToString(lc.category())) },
                                { "selfWeight", lc.isSelfWeightIncluded() ? lc.selfWeightFactor() : 0.0 },
                                { "loadCount", counts[id] } });
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::loadCombinations(const TSA::Model::Model& model)
{
    const auto& lm = model.loadManager();
    QJsonArray arr;
    for (const auto& [id, co] : lm.combinations())
    {
        QJsonArray factors;
        for (const auto& [caseId, f] : co.caseFactors())
        {
            const auto it = lm.loadCases().find(caseId);
            factors.append(QJsonObject{ { "case", it != lm.loadCases().end() ? QString::fromStdString(it->second.name()) : QStringLiteral("? (%1)").arg(caseId) },
                                        { "factor", f } });
        }
        arr.append(QJsonObject{ { "id", id }, { "name", QString::fromStdString(co.name()) },
                                { "type", comboTypeName(co.type()) },
                                { "formula", QString::fromStdString(co.formula(lm.loadCases())) }, { "factors", factors } });
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::loads(const TSA::Model::Model& model, int loadCaseId, int limit)
{
    const auto& lm = model.loadManager();
    QJsonArray arr;
    for (const auto& [id, l] : lm.nodalLoads())
    {
        if (loadCaseId > 0 && l.loadCaseId() != loadCaseId) continue;
        if (arr.size() >= limit) return arr;
        arr.append(QJsonObject{ { "id", id }, { "kind", "nodale" }, { "node", QStringLiteral("N%1").arg(l.nodeId()) },
                                { "loadCase", l.loadCaseId() },
                                { "F_kN", QJsonArray{ sig(l.fx()), sig(l.fy()), sig(l.fz()) } },
                                { "M_kNm", QJsonArray{ sig(l.mx()), sig(l.my()), sig(l.mz()) } } });
    }
    for (const auto& [id, l] : lm.memberLoads())
    {
        if (loadCaseId > 0 && l.loadCaseId() != loadCaseId) continue;
        if (arr.size() >= limit) return arr;
        arr.append(QJsonObject{ { "id", id }, { "kind", loadTypeName(l.type()) },
                                { "element", QString::fromLatin1(TSA::Analysis::elementKindPrefix(
                                                 static_cast<TSA::Analysis::StructuralElementKind>(static_cast<int>(l.targetType())))) + QString::number(l.elementId()) },
                                { "loadCase", l.loadCaseId() },
                                { "direction", QString::fromStdString(TSA::Model::loadDirectionToString(l.direction())) },
                                { "q1", sig(l.q1()) }, { "q2", sig(l.q2()) },
                                { "unit", (l.type() == LoadType::MemberPoint) ? "kN" : "kN/m" } });
    }
    return arr;
}

QJsonArray EngineeringContextBuilder::members(const TSA::Model::Model& model, const QString& typeFilter, int offset, int limit)
{
    QJsonArray arr;
    int index = 0;
    auto push = [&](const QString& type, const QString& prefix, int id, int a, int b, double L,
                    const QString& sec, const QString& mat) {
        if (!typeFilter.isEmpty() && typeFilter != type) return;
        if (index++ < offset || arr.size() >= limit) return;
        arr.append(QJsonObject{ { "id", prefix + QString::number(id) }, { "type", type },
                                { "nodes", QJsonArray{ QStringLiteral("N%1").arg(a), QStringLiteral("N%1").arg(b) } },
                                { "length_m", round4(L) }, { "section", sec }, { "material", mat } });
    };
    for (const auto& [id, e] : model.beams())
        push("beam", "B", id, e.startNodeId(), e.endNodeId(), e.length(model), QString::fromStdString(e.section().name), QString::fromStdString(e.material().name));
    for (const auto& [id, e] : model.columns())
        push("column", "C", id, e.startNodeId(), e.endNodeId(), e.length(model), QString::fromStdString(e.section().name), QString::fromStdString(e.material().name));
    for (const auto& [id, e] : model.trussMembers())
        push("truss", "T", id, e.startNodeId(), e.endNodeId(), e.length(model), QString(), QString());
    for (const auto& [id, e] : model.cables())
        push("cable", "K", id, e.startNodeId(), e.endNodeId(), e.length(model), QString(), QString());
    return arr;
}

QJsonArray EngineeringContextBuilder::nodes(const TSA::Model::Model& model, int offset, int limit)
{
    QJsonArray arr;
    int index = 0;
    for (const auto& [id, n] : model.nodes())
    {
        if (index++ < offset) continue;
        if (arr.size() >= limit) break;
        QJsonObject o{ { "id", QStringLiteral("N%1").arg(id) },
                       { "xyz_m", QJsonArray{ round4(n.x()), round4(n.y()), round4(n.z()) } } };
        if (n.support().isSupported()) o["support"] = supportJson(n.support());
        arr.append(o);
    }
    return arr;
}

QJsonObject EngineeringContextBuilder::resultsSummary(const EngineeringSources& src)
{
    const auto* r = src.results;
    if (!r || !r->isValid() || !r->hasResults())
        return QJsonObject{ { "available", false }, { "note", "Aucun résultat de calcul disponible : lancer le calcul pour obtenir efforts et déplacements." } };

    const auto s = r->summary();
    const auto& u = r->units();
    auto kindPrefix = [](TSA::Analysis::StructuralElementKind k) { return QString::fromLatin1(TSA::Analysis::elementKindPrefix(k)); };

    double sumRz = 0.0, sumRx = 0.0, sumRy = 0.0;
    for (const auto& [nid, re] : r->allReactions()) { sumRx += re.rx; sumRy += re.ry; sumRz += re.rz; }

    // Effort tranchant maximal (non fourni par ResultsSummary)
    double maxV = 0.0;
    QString maxVElem;
    for (const auto& [key, er] : r->allElementResults())
    {
        const double v = er.maxShearForce();
        if (std::abs(v) > std::abs(maxV)) { maxV = v; maxVElem = kindPrefix(key.kind) + QString::number(key.id); }
    }

    return QJsonObject{
        { "available", true },
        { "upToDate", src.resultsUpToDate },
        { "loadCaseOrCombination", QString::fromStdString(r->caseOrComboName()) },
        { "units", QJsonObject{ { "force", QString::fromStdString(u.force) }, { "length", QString::fromStdString(u.length) },
                                { "moment", QString::fromStdString(u.moment) } } },
        { "maxDisplacement", QJsonObject{ { "value", sig(s.maxDisplacement) }, { "node", QStringLiteral("N%1").arg(s.maxDisplacementNodeId) } } },
        { "maxBendingMoment", QJsonObject{ { "value", sig(s.maxBendingMoment) }, { "element", kindPrefix(s.maxBendingMomentElementKind) + QString::number(s.maxBendingMomentElementId) } } },
        { "maxShear", QJsonObject{ { "value", sig(maxV) }, { "element", maxVElem } } },
        { "maxTension", QJsonObject{ { "value", sig(s.maxTension) }, { "element", kindPrefix(s.maxTensionElementKind) + QString::number(s.maxTensionElementId) } } },
        { "maxCompression", QJsonObject{ { "value", sig(s.maxCompression) }, { "element", kindPrefix(s.maxCompressionElementKind) + QString::number(s.maxCompressionElementId) } } },
        { "maxReaction", QJsonObject{ { "value", sig(s.maxReactionForce) }, { "node", QStringLiteral("N%1").arg(s.maxReactionNodeId) } } },
        { "sumReactions", QJsonObject{ { "RX", sig(sumRx) }, { "RY", sig(sumRy) }, { "RZ", sig(sumRz) } } },
        { "modal", r->modalModes().empty() ? QJsonValue() : QJsonValue(QJsonObject{ { "T1_s", sig(s.fundamentalPeriod) }, { "f1_Hz", sig(s.fundamentalFrequency) } }) },
    };
}

QJsonObject EngineeringContextBuilder::objectDetails(const EngineeringSources& src, const ObjectRef& ref)
{
    const auto& m = *src.model;
    const QString type = typeFromKey(ref.type);
    QJsonObject o{ { "ref", ObjectRef{ type, ref.id }.label() }, { "type", type } };

    auto attachLoads = [&](TSA::Model::MemberTargetType target) {
        QJsonArray arr;
        for (const auto& [id, l] : m.loadManager().memberLoads())
            if (l.elementId() == ref.id && l.targetType() == target)
                arr.append(QJsonObject{ { "loadCase", l.loadCaseId() }, { "kind", loadTypeName(l.type()) },
                                        { "direction", QString::fromStdString(TSA::Model::loadDirectionToString(l.direction())) },
                                        { "q1", sig(l.q1()) }, { "q2", sig(l.q2()) } });
        o["appliedLoads"] = arr;
    };
    auto attachResults = [&](const QString& t) {
        bool ok = false;
        const auto kind = kindOf(t, &ok);
        if (ok && src.results && src.results->isValid() && src.results->getElementResults(kind, ref.id))
        {
            o["results"] = TSA::Analysis::ResultsContext::elementContext(m, *src.results, { kind, ref.id });
            o["resultsUpToDate"] = src.resultsUpToDate;
        }
    };

    if (type == "node")
    {
        const auto* n = m.getNode(ref.id);
        if (!n) return {};
        o["xyz_m"] = QJsonArray{ round4(n->x()), round4(n->y()), round4(n->z()) };
        o["level"] = QString::fromStdString(n->levelId());
        if (n->support().isSupported()) o["support"] = supportJson(n->support());
        QJsonArray loadsArr;
        for (const auto& [id, l] : m.loadManager().nodalLoads())
            if (l.nodeId() == ref.id)
                loadsArr.append(QJsonObject{ { "loadCase", l.loadCaseId() }, { "F_kN", QJsonArray{ sig(l.fx()), sig(l.fy()), sig(l.fz()) } },
                                             { "M_kNm", QJsonArray{ sig(l.mx()), sig(l.my()), sig(l.mz()) } } });
        o["appliedLoads"] = loadsArr;
        if (src.results && src.results->isValid() && src.results->hasNodeDisplacement(ref.id))
        {
            o["results"] = TSA::Analysis::ResultsContext::nodeContext(m, *src.results, ref.id);
            o["resultsUpToDate"] = src.resultsUpToDate;
        }
        return o;
    }
    if (type == "beam" || type == "column")
    {
        const TSA::Model::Section* sec = nullptr;
        const TSA::Model::Material* mat = nullptr;
        int a = 0, b = 0;
        double L = 0.0, rot = 0.0;
        if (type == "beam")
        {
            const auto* e = m.getBeam(ref.id);
            if (!e) return {};
            sec = &e->section(); mat = &e->material(); a = e->startNodeId(); b = e->endNodeId(); L = e->length(m); rot = e->rotation();
        }
        else
        {
            const auto* e = m.getColumn(ref.id);
            if (!e) return {};
            sec = &e->section(); mat = &e->material(); a = e->startNodeId(); b = e->endNodeId(); L = e->length(m); rot = e->rotation();
        }
        o["nodes"] = QJsonArray{ QStringLiteral("N%1").arg(a), QStringLiteral("N%1").arg(b) };
        o["length_m"] = round4(L);
        o["rotation_deg"] = round4(rot);
        o["section"] = sectionJson(*sec);
        o["material"] = materialJson(*mat);
        const auto* na = m.getNode(a);
        const auto* nb = m.getNode(b);
        if (na && na->support().isSupported()) o["startSupport"] = supportJson(na->support());
        if (nb && nb->support().isSupported()) o["endSupport"] = supportJson(nb->support());
        attachLoads(type == "beam" ? TSA::Model::MemberTargetType::Beam : TSA::Model::MemberTargetType::Column);
        attachResults(type);
        return o;
    }
    if (type == "truss" || type == "cable")
    {
        int a = 0, b = 0;
        double L = 0.0;
        if (type == "truss") { const auto* e = m.getTrussMember(ref.id); if (!e) return {}; a = e->startNodeId(); b = e->endNodeId(); L = e->length(m); }
        else { const auto* e = m.getCable(ref.id); if (!e) return {}; a = e->startNodeId(); b = e->endNodeId(); L = e->length(m); }
        o["nodes"] = QJsonArray{ QStringLiteral("N%1").arg(a), QStringLiteral("N%1").arg(b) };
        o["length_m"] = round4(L);
        attachLoads(type == "truss" ? TSA::Model::MemberTargetType::Truss : TSA::Model::MemberTargetType::Cable);
        attachResults(type);
        return o;
    }
    if (type == "slab")
    {
        const auto* e = m.getSlab(ref.id);
        if (!e) return {};
        QJsonArray ns;
        for (int n : e->nodeIds()) ns.append(QStringLiteral("N%1").arg(n));
        o["nodes"] = ns;
        o["thickness_m"] = round4(e->thickness());
        o["note"] = "Les dalles ne sont pas transmises au calcul OpenSees.";
        return o;
    }
    if (type == "wall")
    {
        const auto* e = m.getWall(ref.id);
        if (!e) return {};
        o["nodes"] = QJsonArray{ QStringLiteral("N%1").arg(e->startNodeId()), QStringLiteral("N%1").arg(e->endNodeId()) };
        o["height_m"] = round4(e->height());
        o["thickness_m"] = round4(e->thickness());
        o["note"] = "Les voiles ne sont pas transmis au calcul OpenSees.";
        return o;
    }
    if (type == "foundation")
    {
        const auto* e = m.getFoundation(ref.id);
        if (!e) return {};
        o["node"] = QStringLiteral("N%1").arg(e->nodeId());
        o["note"] = "Les fondations ne sont pas transmises au calcul OpenSees.";
        return o;
    }
    return {};
}

QJsonObject EngineeringContextBuilder::selectionContext(const EngineeringSources& src)
{
    QJsonArray items;
    for (const auto& ref : src.selection)
    {
        if (items.size() >= 5) break;
        const QJsonObject d = objectDetails(src, ref);
        if (!d.isEmpty()) items.append(d);
    }
    return QJsonObject{ { "selectedCount", static_cast<int>(src.selection.size()) }, { "details", items } };
}

QJsonObject EngineeringContextBuilder::summary(const EngineeringSources& src, const ContextOptions& options)
{
    if (!src.model) return {};
    QJsonObject ctx{
        { "source", "Données extraites du modèle TSA (lecture seule)." },
        { "project", projectInfo(src) },
        { "units", units() },
        { "materials", materials(*src.model) },
        { "sections", sections(*src.model) },
        { "supports", supports(*src.model, options.maxListedNodes) },
        { "loadCases", loadCases(*src.model) },
        { "loadCombinations", loadCombinations(*src.model) },
        { "results", resultsSummary(src) },
    };
    const int memberCount = static_cast<int>(src.model->beams().size() + src.model->columns().size()
                                             + src.model->trussMembers().size() + src.model->cables().size());
    if (memberCount <= options.maxListedMembers)
        ctx["members"] = members(*src.model, QString(), 0, options.maxListedMembers);
    else
        ctx["members"] = QJsonObject{ { "listed", false }, { "count", memberCount }, { "hint", "Utiliser l'outil list_members pour les détails." } };
    if (!src.selection.empty()) ctx["selection"] = selectionContext(src);
    if (options.includeCheckReport)
        ctx["checks"] = StructuralChecker::check(*src.model, src.results, CheckOptions{ .resultsUpToDate = src.resultsUpToDate }).toJson();
    return ctx;
}

} // namespace TSA::AI
