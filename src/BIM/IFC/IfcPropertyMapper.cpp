#include "IfcPropertyMapper.h"

#include "../Core/IfcGuid.h"
#include "../../Model/Model.h"

#include <algorithm>
#include <cmath>

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;
using TSA::Model::ElementKind;
using Type = PropertyValue::Type;

namespace
{
const char* commonPset(BimCategory c)
{
    switch (c)
    {
    case BimCategory::Beam: return "Pset_BeamCommon";
    case BimCategory::Column: return "Pset_ColumnCommon";
    case BimCategory::Member: return "Pset_MemberCommon";
    case BimCategory::Slab: return "Pset_SlabCommon";
    case BimCategory::Wall: return "Pset_WallCommon";
    case BimCategory::Plate: return "Pset_PlateCommon";
    default: return nullptr;
    }
}

double barLength(const TSA::Model::Model& m, int a, int b)
{
    const auto* p = m.getNode(a);
    const auto* q = m.getNode(b);
    if (!p || !q) return 0.0;
    return std::sqrt((p->x() - q->x()) * (p->x() - q->x()) + (p->y() - q->y()) * (p->y() - q->y()) + (p->z() - q->z()) * (p->z() - q->z()));
}
} // namespace

std::string IfcPropertyMapper::stepValue(const PropertyValue& v)
{
    switch (v.type)
    {
    case Type::Text: return "IFCTEXT(" + W::str(v.text) + ")";
    case Type::Label: return "IFCLABEL(" + W::str(v.text) + ")";
    case Type::Boolean: return "IFCBOOLEAN(" + W::boolean(v.flag) + ")";
    case Type::Integer: return "IFCINTEGER(" + std::to_string(static_cast<long long>(std::llround(v.number))) + ")";
    case Type::Length: return "IFCLENGTHMEASURE(" + W::real(v.number) + ")";
    case Type::Force: return "IFCFORCEMEASURE(" + W::real(v.number) + ")";
    case Type::Pressure: return "IFCPRESSUREMEASURE(" + W::real(v.number) + ")";
    case Type::Ratio: return "IFCRATIOMEASURE(" + W::real(v.number) + ")";
    case Type::Real:
    default: return "IFCREAL(" + W::real(v.number) + ")";
    }
}

std::vector<PropertySet> IfcPropertyMapper::propertySets(const TSA::Model::Model& m, const PhysicalElement& e)
{
    std::vector<PropertySet> out;
    if (const char* common = commonPset(e.category))
    {
        PropertySet ps;
        ps.name = common;
        ps.properties["LoadBearing"] = PropertyValue::ofBool(true);
        out.push_back(ps);
    }

    // Pset_TSA_Structural : lien explicite vers le modèle analytique et données de calcul
    PropertySet tsa;
    tsa.name = "Pset_TSA_Structural";
    PropertyValue id;
    id.type = Type::Integer;
    id.number = e.id;
    tsa.properties["InternalId"] = id;
    std::string labels;
    for (const auto& r : e.analytical) labels += (labels.empty() ? "" : ",") + r.label();
    tsa.properties["AnalyticalElements"] = PropertyValue::ofText(labels);

    double length = 0.0;
    std::string section, material;
    for (const auto& r : e.analytical)
    {
        switch (r.kind)
        {
        case ElementKind::Beam:
            if (const auto* b = m.getBeam(r.id))
            {
                length += barLength(m, b->startNodeId(), b->endNodeId());
                section = b->section().name;
                material = b->material().name;
                tsa.properties["RotationAngleDeg"] = PropertyValue::ofReal(b->rotation());
            }
            break;
        case ElementKind::Column:
            if (const auto* c = m.getColumn(r.id))
            {
                length += barLength(m, c->startNodeId(), c->endNodeId());
                section = c->section().name;
                material = c->material().name;
                tsa.properties["RotationAngleDeg"] = PropertyValue::ofReal(c->rotation());
            }
            break;
        case ElementKind::TrussMember:
            if (auto it = m.trussMembers().find(r.id); it != m.trussMembers().end())
            {
                length += barLength(m, it->second.startNodeId(), it->second.endNodeId());
                section = it->second.section().name;
                material = it->second.material().name;
            }
            break;
        case ElementKind::Cable:
            if (auto it = m.cables().find(r.id); it != m.cables().end())
            {
                length += barLength(m, it->second.startNodeId(), it->second.endNodeId());
                section = it->second.section().name;
                material = it->second.material().name;
                tsa.properties["InitialTension"] = PropertyValue::ofReal(it->second.initialTension(), Type::Force);
            }
            break;
        case ElementKind::Slab:
            if (auto it = m.slabs().find(r.id); it != m.slabs().end())
            {
                material = it->second.material().name;
                tsa.properties["Thickness"] = PropertyValue::ofReal(it->second.thickness(), Type::Length);
            }
            break;
        case ElementKind::Wall:
            if (auto it = m.walls().find(r.id); it != m.walls().end())
            {
                material = it->second.material().name;
                tsa.properties["Thickness"] = PropertyValue::ofReal(it->second.thickness(), Type::Length);
                tsa.properties["Height"] = PropertyValue::ofReal(it->second.height(), Type::Length);
            }
            break;
        case ElementKind::Foundation:
            if (auto it = m.foundations().find(r.id); it != m.foundations().end())
            {
                material = it->second.material().name;
                tsa.properties["Width"] = PropertyValue::ofReal(it->second.widthA(), Type::Length);
                tsa.properties["Length"] = PropertyValue::ofReal(it->second.lengthB(), Type::Length);
                tsa.properties["Depth"] = PropertyValue::ofReal(it->second.heightH(), Type::Length);
                // Contrainte admissible du sol saisie en kPa → Pa (unité SI du fichier)
                tsa.properties["SoilBearingCapacity"] = PropertyValue::ofReal(it->second.soilBearingCapacity() * 1e3, Type::Pressure);
            }
            break;
        default:
            break;
        }
    }
    if (length > 0.0) tsa.properties["MemberLength"] = PropertyValue::ofReal(length, Type::Length);
    if (!section.empty()) tsa.properties["SectionName"] = PropertyValue::ofText(section, Type::Label);
    if (!material.empty()) tsa.properties["MaterialName"] = PropertyValue::ofText(material, Type::Label);
    out.push_back(tsa);

    if (length > 0.0 && e.category == BimCategory::Beam) out.front().properties["Span"] = PropertyValue::ofReal(length, Type::Length);

    // Psets utilisateur : valeurs prioritaires, Psets nouveaux ajoutés
    for (const auto& user : e.propertySets)
    {
        auto it = std::find_if(out.begin(), out.end(), [&](const PropertySet& p) { return p.name == user.name; });
        if (it == out.end()) out.push_back(user);
        else
            for (const auto& [k, v] : user.properties) it->properties[k] = v;
    }
    return out;
}

std::vector<int> IfcPropertyMapper::write(const PhysicalElement& e)
{
    std::vector<int> ids;
    for (const auto& ps : propertySets(m_ctx.model, e))
    {
        if (ps.properties.empty()) continue;
        std::vector<int> props;
        for (const auto& [k, v] : ps.properties)
            props.push_back(m_ctx.w.add("IFCPROPERTYSINGLEVALUE", W::str(k) + ",$," + stepValue(v) + ",$"));
        ids.push_back(m_ctx.w.add("IFCPROPERTYSET", W::str(IfcGuid::create()) + ",$," + W::str(ps.name) + ",$," + W::refs(props)));
    }
    return ids;
}

} // namespace TSA::BIM::Ifc
