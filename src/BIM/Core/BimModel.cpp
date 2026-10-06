#include "BimModel.h"

#include "IfcGuid.h"
#include "../../Model/Model.h"
#include "../../Coordinate/LevelManager.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace TSA::BIM
{

using TSA::Model::ElementKind;

namespace
{
constexpr int kSchemaVersion = 1;

QString qs(const std::string& s) { return QString::fromStdString(s); }
std::string ss(const QJsonValue& v) { return v.toString().toStdString(); }

std::vector<int> nodeIdsOf(const TSA::Model::Model& m, const AnalyticalRef& r)
{
    switch (r.kind)
    {
    case ElementKind::Node:
        return { r.id };
    case ElementKind::Beam:
        if (auto* e = m.getBeam(r.id)) return { e->startNodeId(), e->endNodeId() };
        break;
    case ElementKind::Column:
        if (auto* e = m.getColumn(r.id)) return { e->startNodeId(), e->endNodeId() };
        break;
    case ElementKind::Slab:
        if (auto it = m.slabs().find(r.id); it != m.slabs().end()) return it->second.nodeIds();
        break;
    case ElementKind::Wall:
        if (auto it = m.walls().find(r.id); it != m.walls().end()) return { it->second.startNodeId(), it->second.endNodeId() };
        break;
    case ElementKind::Foundation:
        if (auto it = m.foundations().find(r.id); it != m.foundations().end()) return { it->second.nodeId() };
        break;
    case ElementKind::TrussMember:
        if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end()) return { it->second.startNodeId(), it->second.endNodeId() };
        break;
    case ElementKind::Cable:
        if (auto it = m.cables().find(r.id); it != m.cables().end()) return { it->second.startNodeId(), it->second.endNodeId() };
        break;
    }
    return {};
}

/// Tous les éléments analytiques existants du modèle (nœuds compris).
std::vector<AnalyticalRef> analyticalRefs(const TSA::Model::Model& m)
{
    std::vector<AnalyticalRef> out;
    auto add = [&out](ElementKind k, const auto& map) {
        for (const auto& [id, e] : map) out.push_back({ k, id });
    };
    add(ElementKind::Node, m.nodes());
    add(ElementKind::Beam, m.beams());
    add(ElementKind::Column, m.columns());
    add(ElementKind::Slab, m.slabs());
    add(ElementKind::Wall, m.walls());
    add(ElementKind::Foundation, m.foundations());
    add(ElementKind::TrussMember, m.trussMembers());
    add(ElementKind::Cable, m.cables());
    return out;
}

const char* kValueTypes[] = { "text", "label", "real", "integer", "boolean", "length", "force", "pressure", "ratio" };

QJsonObject valueToJson(const PropertyValue& v)
{
    QJsonObject o;
    o["type"] = kValueTypes[static_cast<int>(v.type)];
    switch (v.type)
    {
    case PropertyValue::Type::Text:
    case PropertyValue::Type::Label: o["value"] = qs(v.text); break;
    case PropertyValue::Type::Boolean: o["value"] = v.flag; break;
    default: o["value"] = v.number; break;
    }
    return o;
}

PropertyValue valueFromJson(const QJsonObject& o)
{
    PropertyValue v;
    const std::string t = ss(o["type"]);
    for (int i = 0; i < int(std::size(kValueTypes)); ++i)
        if (t == kValueTypes[i]) v.type = static_cast<PropertyValue::Type>(i);
    switch (v.type)
    {
    case PropertyValue::Type::Text:
    case PropertyValue::Type::Label: v.text = ss(o["value"]); break;
    case PropertyValue::Type::Boolean: v.flag = o["value"].toBool(); break;
    default: v.number = o["value"].toDouble(); break;
    }
    return v;
}

std::string refKey(const AnalyticalRef& r) { return std::string(elementKindKey(r.kind)) + ":" + std::to_string(r.id); }

bool refFromKey(const std::string& key, AnalyticalRef& r)
{
    const auto colon = key.find(':');
    if (colon == std::string::npos || !elementKindFromKey(key.substr(0, colon), r.kind)) return false;
    try
    {
        r.id = std::stoi(key.substr(colon + 1));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

QJsonObject spatialToJson(const SpatialElement& s)
{
    return QJsonObject { { "guid", qs(s.globalId) }, { "name", qs(s.name) }, { "description", qs(s.description) } };
}

void spatialFromJson(const QJsonObject& o, SpatialElement& s)
{
    if (o.isEmpty()) return;
    s.globalId = ss(o["guid"]);
    s.name = ss(o["name"]);
    s.description = ss(o["description"]);
}

bool ensureGuid(std::string& g)
{
    if (IfcGuid::isValid(g)) return false;
    g = IfcGuid::create();
    return true;
}
} // namespace

const PropertySet* PhysicalElement::propertySet(const std::string& psetName) const
{
    for (const auto& p : propertySets)
        if (p.name == psetName) return &p;
    return nullptr;
}

std::string defaultProductName(const PhysicalElement& e)
{
    std::string base;
    switch (e.category)
    {
    case BimCategory::Beam: base = "Poutre"; break;
    case BimCategory::Column: base = "Poteau"; break;
    case BimCategory::Member: base = "Barre"; break;
    case BimCategory::Slab: base = "Dalle"; break;
    case BimCategory::Wall: base = "Voile"; break;
    case BimCategory::Footing: base = "Semelle"; break;
    case BimCategory::Pile: base = "Pieu"; break;
    case BimCategory::Plate: base = "Plaque"; break;
    default: base = "Élément"; break;
    }
    return e.analytical.empty() ? base : base + " " + e.analytical.front().label();
}

std::string defaultProductTag(const PhysicalElement& e)
{
    std::string t;
    for (const auto& r : e.analytical) t += (t.empty() ? "" : ",") + r.label();
    return t;
}

std::pair<BimCategory, std::string> defaultCategory(const TSA::Model::Model& m, const AnalyticalRef& r)
{
    using TSA::Model::BarRole;
    switch (r.kind)
    {
    case ElementKind::Node:
        return { BimCategory::AnalyticalNode, "" };
    case ElementKind::Beam:
        if (auto* b = m.getBeam(r.id))
        {
            switch (b->role())
            {
            case BarRole::Column: return { BimCategory::Column, "COLUMN" };
            case BarRole::Brace: return { BimCategory::Member, "BRACE" };
            case BarRole::Tie: return { BimCategory::Member, "TIEBAR" };
            case BarRole::Truss: return { BimCategory::Member, "MEMBER" };
            case BarRole::Cable: return { BimCategory::Member, "STRUCTURALCABLE" };
            default: break;
            }
        }
        return { BimCategory::Beam, "BEAM" };
    case ElementKind::Column:
        return { BimCategory::Column, "COLUMN" };
    case ElementKind::Slab:
        return { BimCategory::Slab, "FLOOR" };
    case ElementKind::Wall:
        return { BimCategory::Wall, "SOLIDWALL" };
    case ElementKind::Foundation:
        if (auto it = m.foundations().find(r.id); it != m.foundations().end())
        {
            switch (it->second.foundationType())
            {
            case TSA::Model::FoundationType::StripFooting: return { BimCategory::Footing, "STRIP_FOOTING" };
            case TSA::Model::FoundationType::Raft: return { BimCategory::Slab, "BASESLAB" };
            case TSA::Model::FoundationType::Pile: return { BimCategory::Pile, "NOTDEFINED" };
            default: break;
            }
        }
        return { BimCategory::Footing, "PAD_FOOTING" };
    case ElementKind::TrussMember:
        if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end())
        {
            switch (it->second.role())
            {
            case TSA::Model::TrussMemberRole::TopChord:
            case TSA::Model::TrussMemberRole::BottomChord: return { BimCategory::Member, "CHORD" };
            case TSA::Model::TrussMemberRole::Brace: return { BimCategory::Member, "BRACE" };
            default: return { BimCategory::Member, "STRUT" };
            }
        }
        return { BimCategory::Member, "MEMBER" };
    case ElementKind::Cable:
        if (auto it = m.cables().find(r.id); it != m.cables().end())
        {
            switch (it->second.type())
            {
            case TSA::Model::CableType::StayCable: return { BimCategory::Member, "STAY_CABLE" };
            case TSA::Model::CableType::SuspensionCable: return { BimCategory::Member, "SUSPENSION_CABLE" };
            case TSA::Model::CableType::Hanger: return { BimCategory::Member, "SUSPENDER" };
            default: break;
            }
        }
        return { BimCategory::Member, "STRUCTURALCABLE" };
    }
    return { BimCategory::Member, "NOTDEFINED" };
}

const PhysicalElement* BimModel::element(int id) const
{
    auto it = m_elements.find(id);
    return it == m_elements.end() ? nullptr : &it->second;
}

PhysicalElement* BimModel::element(int id)
{
    auto it = m_elements.find(id);
    return it == m_elements.end() ? nullptr : &it->second;
}

const PhysicalElement* BimModel::elementByGlobalId(const std::string& globalId) const
{
    for (const auto& [id, e] : m_elements)
        if (e.globalId == globalId) return &e;
    return nullptr;
}

const PhysicalElement* BimModel::physicalOf(const AnalyticalRef& ref) const
{
    auto it = m_index.find(ref);
    return it == m_index.end() ? nullptr : element(it->second);
}

std::string BimModel::analyticalGlobalId(const AnalyticalRef& ref) const
{
    auto it = m_analyticalGuids.find(ref);
    return it == m_analyticalGuids.end() ? std::string() : it->second;
}

int BimModel::create(const PhysicalElement& templ)
{
    PhysicalElement e = templ;
    e.id = m_nextId++;
    e.globalId = IfcGuid::create();
    m_elements[e.id] = std::move(e);
    return m_nextId - 1;
}

void BimModel::rebuildIndex()
{
    m_index.clear();
    for (const auto& [id, e] : m_elements)
        for (const auto& r : e.analytical) m_index[r] = id;
    for (const auto& [id, e] : m_elements) m_nextId = std::max(m_nextId, id + 1);
}

bool BimModel::synchronize(const TSA::Model::Model& model)
{
    bool changed = false;
    const auto refs = analyticalRefs(model);
    const std::set<AnalyticalRef> existing(refs.begin(), refs.end());

    // 1. Références orphelines (élément analytique supprimé) et produits vidés
    for (auto it = m_elements.begin(); it != m_elements.end();)
    {
        auto& an = it->second.analytical;
        std::set<AnalyticalRef> seen;
        const auto before = an.size();
        an.erase(std::remove_if(an.begin(), an.end(),
                                [&](const AnalyticalRef& r) {
                                    return r.kind == ElementKind::Node || !existing.count(r) || !seen.insert(r).second;
                                }),
                 an.end());
        changed |= an.size() != before;
        if (an.empty())
        {
            it = m_elements.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    for (auto it = m_analyticalGuids.begin(); it != m_analyticalGuids.end();)
    {
        if (!existing.count(it->first))
        {
            it = m_analyticalGuids.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    rebuildIndex();

    // 2. Un produit physique (1:1) pour tout élément porteur non rattaché ; GlobalId analytiques
    for (const auto& r : refs)
    {
        changed |= ensureGuid(m_analyticalGuids[r]);
        if (r.kind == ElementKind::Node || m_index.count(r)) continue;
        PhysicalElement e;
        std::tie(e.category, e.predefinedType) = defaultCategory(model, r);
        e.analytical.push_back(r);
        m_index[r] = create(e);
        changed = true;
    }
    for (auto& [id, e] : m_elements) changed |= ensureGuid(e.globalId);

    // 3. Structure spatiale : projet / site / bâtiment, un étage par niveau
    changed |= ensureGuid(m_spatial.project.globalId);
    changed |= ensureGuid(m_spatial.site.globalId);
    changed |= ensureGuid(m_spatial.building.globalId);
    changed |= ensureGuid(m_spatial.analysisModel.globalId);
    std::set<std::string> levelIds;
    if (const auto* lm = model.levelManager())
        for (const auto& lvl : lm->levels())
        {
            levelIds.insert(lvl.id);
            changed |= ensureGuid(m_spatial.storeyGlobalIds[lvl.id]);
        }
    for (auto it = m_spatial.storeyGlobalIds.begin(); it != m_spatial.storeyGlobalIds.end();)
    {
        if (!levelIds.count(it->first))
        {
            it = m_spatial.storeyGlobalIds.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    return changed;
}

void BimModel::attachSplit(const AnalyticalRef& original, const AnalyticalRef& created)
{
    auto target = m_index.find(original);
    if (target == m_index.end() || original == created) return;
    const int targetId = target->second;

    // Le tronçon a pu recevoir son propre produit (synchronisation intermédiaire) : on l'en retire
    if (auto cur = m_index.find(created); cur != m_index.end() && cur->second != targetId)
    {
        auto& an = m_elements[cur->second].analytical;
        an.erase(std::remove(an.begin(), an.end(), created), an.end());
        if (an.empty()) m_elements.erase(cur->second);
    }
    auto& an = m_elements[targetId].analytical;
    if (std::find(an.begin(), an.end(), created) == an.end())
    {
        auto pos = std::find(an.begin(), an.end(), original);
        an.insert(pos == an.end() ? an.end() : pos + 1, created);
    }
    rebuildIndex();
}

void BimModel::registerCopies(const std::vector<std::pair<AnalyticalRef, AnalyticalRef>>& originalToCopy)
{
    std::map<AnalyticalRef, AnalyticalRef> copyOf;
    for (const auto& [o, c] : originalToCopy)
        if (o.kind != ElementKind::Node) copyOf[o] = c;
    if (copyOf.empty()) return;

    // Retire les copies de tout produit existant (créé par une synchronisation intermédiaire)
    for (auto it = m_elements.begin(); it != m_elements.end();)
    {
        auto& an = it->second.analytical;
        an.erase(std::remove_if(an.begin(), an.end(),
                                [&](const AnalyticalRef& r) {
                                    return std::any_of(copyOf.begin(), copyOf.end(), [&](const auto& p) { return p.second == r; });
                                }),
                 an.end());
        it = an.empty() ? m_elements.erase(it) : std::next(it);
    }
    rebuildIndex();

    // Un nouveau produit par produit source, mêmes métadonnées, éléments dans le même ordre
    std::set<int> done;
    for (const auto& [orig, copy] : copyOf)
    {
        auto src = m_index.find(orig);
        if (src == m_index.end() || done.count(src->second)) continue;
        done.insert(src->second);
        PhysicalElement e = m_elements[src->second];
        e.name.clear();
        e.tag.clear();
        e.storeyLevelId.clear();
        std::vector<AnalyticalRef> copies;
        for (const auto& r : e.analytical)
            if (auto c = copyOf.find(r); c != copyOf.end()) copies.push_back(c->second);
        e.analytical = std::move(copies);
        create(e);
    }
    rebuildIndex();
}

int BimModel::group(const std::vector<AnalyticalRef>& refs)
{
    int targetId = 0;
    for (const auto& r : refs)
        if (auto it = m_index.find(r); it != m_index.end())
        {
            targetId = it->second;
            break;
        }
    if (targetId == 0)
    {
        if (refs.empty()) return 0;
        PhysicalElement e;
        e.analytical.push_back(refs.front());
        targetId = create(e);
        rebuildIndex();
    }
    for (const auto& r : refs)
    {
        if (r.kind == ElementKind::Node) continue;
        auto it = m_index.find(r);
        if (it != m_index.end() && it->second == targetId) continue;
        if (it != m_index.end())
        {
            auto& an = m_elements[it->second].analytical;
            an.erase(std::remove(an.begin(), an.end(), r), an.end());
            if (an.empty()) m_elements.erase(it->second);
        }
        m_elements[targetId].analytical.push_back(r);
        m_index[r] = targetId;
    }
    rebuildIndex();
    return targetId;
}

int BimModel::ungroup(const AnalyticalRef& ref)
{
    auto it = m_index.find(ref);
    if (it == m_index.end()) return 0;
    PhysicalElement& src = m_elements[it->second];
    if (src.analytical.size() <= 1) return src.id;
    src.analytical.erase(std::remove(src.analytical.begin(), src.analytical.end(), ref), src.analytical.end());
    PhysicalElement e = src;
    e.name.clear();
    e.tag.clear();
    e.analytical = { ref };
    const int id = create(e);
    rebuildIndex();
    return id;
}

int BimModel::insert(PhysicalElement e)
{
    // Éléments analytiques retirés de leurs produits actuels (synchronisation intermédiaire)
    for (auto it = m_elements.begin(); it != m_elements.end();)
    {
        auto& an = it->second.analytical;
        an.erase(std::remove_if(an.begin(), an.end(),
                                [&](const AnalyticalRef& r) { return std::find(e.analytical.begin(), e.analytical.end(), r) != e.analytical.end(); }),
                 an.end());
        it = an.empty() ? m_elements.erase(it) : std::next(it);
    }
    const bool keepGuid = IfcGuid::isValid(e.globalId) && !elementByGlobalId(e.globalId);
    const std::string guid = e.globalId;
    const int id = create(e);
    if (keepGuid) m_elements[id].globalId = guid;
    rebuildIndex();
    return id;
}

void BimModel::setAnalyticalGlobalId(const AnalyticalRef& ref, const std::string& globalId)
{
    if (IfcGuid::isValid(globalId)) m_analyticalGuids[ref] = globalId;
}

std::string BimModel::resolvedStorey(const TSA::Model::Model& model, const PhysicalElement& e) const
{
    const auto* lm = model.levelManager();
    if (!lm || lm->levels().empty()) return {};
    if (!e.storeyLevelId.empty() && lm->getLevel(e.storeyLevelId)) return e.storeyLevelId;

    const TSA::Model::Node* lowest = nullptr;
    for (const auto& r : e.analytical)
        for (int nid : nodeIdsOf(model, r))
            if (const auto* n = model.getNode(nid); n && (!lowest || n->z() < lowest->z())) lowest = n;
    if (!lowest) return lm->levels().front().id;
    if (!lowest->levelId().empty() && lm->getLevel(lowest->levelId())) return lowest->levelId();

    // Niveau de cote ≤ z la plus haute ; sinon le plus bas
    const TSA::Coordinate::Level* best = nullptr;
    const TSA::Coordinate::Level* bottom = nullptr;
    for (const auto& lvl : lm->levels())
    {
        if (!bottom || lvl.elevation < bottom->elevation) bottom = &lvl;
        if (lvl.elevation <= lowest->z() + 1e-6 && (!best || lvl.elevation > best->elevation)) best = &lvl;
    }
    return (best ? best : bottom)->id;
}

QJsonObject BimModel::toJson() const
{
    QJsonObject root;
    root["schema"] = kSchemaVersion;
    root["nextId"] = m_nextId;
    root["project"] = spatialToJson(m_spatial.project);
    root["site"] = spatialToJson(m_spatial.site);
    root["building"] = spatialToJson(m_spatial.building);
    root["analysisModel"] = spatialToJson(m_spatial.analysisModel);
    QJsonObject storeys;
    for (const auto& [lvl, g] : m_spatial.storeyGlobalIds) storeys[qs(lvl)] = qs(g);
    root["storeys"] = storeys;

    QJsonArray elements;
    for (const auto& [id, e] : m_elements)
    {
        QJsonObject o;
        o["id"] = e.id;
        o["guid"] = qs(e.globalId);
        o["category"] = categoryName(e.category);
        o["predefinedType"] = qs(e.predefinedType);
        o["name"] = qs(e.name);
        o["objectType"] = qs(e.objectType);
        o["tag"] = qs(e.tag);
        o["description"] = qs(e.description);
        o["storey"] = qs(e.storeyLevelId);
        QJsonArray an;
        for (const auto& r : e.analytical) an.append(qs(refKey(r)));
        o["analytical"] = an;
        QJsonArray psets;
        for (const auto& p : e.propertySets)
        {
            QJsonObject props;
            for (const auto& [k, v] : p.properties) props[qs(k)] = valueToJson(v);
            psets.append(QJsonObject { { "name", qs(p.name) }, { "properties", props } });
        }
        o["psets"] = psets;
        QJsonArray cls;
        for (const auto& c : e.classifications)
            cls.append(QJsonObject { { "system", qs(c.system) }, { "id", qs(c.identification) }, { "name", qs(c.name) } });
        o["classifications"] = cls;
        elements.append(o);
    }
    root["elements"] = elements;

    QJsonObject guids;
    for (const auto& [r, g] : m_analyticalGuids) guids[qs(refKey(r))] = qs(g);
    root["analytical"] = guids;
    return root;
}

bool BimModel::fromJson(const QJsonObject& root, BimModel& out)
{
    out = BimModel();
    if (root["schema"].toInt(1) > kSchemaVersion) return false;
    spatialFromJson(root["project"].toObject(), out.m_spatial.project);
    spatialFromJson(root["site"].toObject(), out.m_spatial.site);
    spatialFromJson(root["building"].toObject(), out.m_spatial.building);
    spatialFromJson(root["analysisModel"].toObject(), out.m_spatial.analysisModel);
    const QJsonObject storeys = root["storeys"].toObject();
    for (auto it = storeys.begin(); it != storeys.end(); ++it) out.m_spatial.storeyGlobalIds[it.key().toStdString()] = ss(it.value());

    for (const auto& v : root["elements"].toArray())
    {
        const QJsonObject o = v.toObject();
        PhysicalElement e;
        e.id = o["id"].toInt();
        if (e.id <= 0 || out.m_elements.count(e.id)) continue;
        e.globalId = ss(o["guid"]);
        categoryFromName(ss(o["category"]), e.category);
        e.predefinedType = ss(o["predefinedType"]);
        e.name = ss(o["name"]);
        e.objectType = ss(o["objectType"]);
        e.tag = ss(o["tag"]);
        e.description = ss(o["description"]);
        e.storeyLevelId = ss(o["storey"]);
        for (const auto& a : o["analytical"].toArray())
        {
            AnalyticalRef r;
            if (refFromKey(ss(a), r)) e.analytical.push_back(r);
        }
        for (const auto& pv : o["psets"].toArray())
        {
            const QJsonObject po = pv.toObject();
            PropertySet p;
            p.name = ss(po["name"]);
            const QJsonObject props = po["properties"].toObject();
            for (auto it = props.begin(); it != props.end(); ++it) p.properties[it.key().toStdString()] = valueFromJson(it.value().toObject());
            e.propertySets.push_back(std::move(p));
        }
        for (const auto& cv : o["classifications"].toArray())
        {
            const QJsonObject co = cv.toObject();
            e.classifications.push_back({ ss(co["system"]), ss(co["id"]), ss(co["name"]) });
        }
        out.m_elements[e.id] = std::move(e);
    }
    const QJsonObject guids = root["analytical"].toObject();
    for (auto it = guids.begin(); it != guids.end(); ++it)
    {
        AnalyticalRef r;
        if (refFromKey(it.key().toStdString(), r)) out.m_analyticalGuids[r] = ss(it.value());
    }
    out.m_nextId = std::max(1, root["nextId"].toInt(1));
    out.rebuildIndex();
    return true;
}

} // namespace TSA::BIM
