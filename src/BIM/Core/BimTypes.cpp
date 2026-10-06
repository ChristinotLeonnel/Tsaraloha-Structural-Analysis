#include "BimTypes.h"

#include <cstdio>

namespace TSA::BIM
{

using TSA::Model::ElementKind;

namespace
{
struct CategoryInfo
{
    BimCategory category;
    const char* name;
    const char* ifc;
};

constexpr CategoryInfo kCategories[] = {
    { BimCategory::Project, "Project", "IfcProject" },
    { BimCategory::Site, "Site", "IfcSite" },
    { BimCategory::Building, "Building", "IfcBuilding" },
    { BimCategory::BuildingStorey, "BuildingStorey", "IfcBuildingStorey" },
    { BimCategory::Grid, "Grid", "IfcGrid" },
    { BimCategory::Space, "Space", "IfcSpace" },
    { BimCategory::Column, "Column", "IfcColumn" },
    { BimCategory::Beam, "Beam", "IfcBeam" },
    { BimCategory::Member, "Member", "IfcMember" },
    { BimCategory::Slab, "Slab", "IfcSlab" },
    { BimCategory::Wall, "Wall", "IfcWall" },
    { BimCategory::Footing, "Footing", "IfcFooting" },
    { BimCategory::Pile, "Pile", "IfcPile" },
    { BimCategory::Plate, "Plate", "IfcPlate" },
    { BimCategory::AnalyticalNode, "AnalyticalNode", "IfcStructuralPointConnection" },
    { BimCategory::AnalyticalMember, "AnalyticalMember", "IfcStructuralCurveMember" },
    { BimCategory::AnalyticalSurface, "AnalyticalSurface", "IfcStructuralSurfaceMember" },
    { BimCategory::BoundaryCondition, "BoundaryCondition", "IfcBoundaryNodeCondition" },
    { BimCategory::Release, "Release", "" },
    { BimCategory::LoadApplication, "LoadApplication", "IfcStructuralPointAction" },
};

struct KindInfo
{
    ElementKind kind;
    const char* key;
    const char* prefix;
};

constexpr KindInfo kKinds[] = {
    { ElementKind::Node, "node", "N" },
    { ElementKind::Beam, "beam", "B" },
    { ElementKind::Column, "column", "C" },
    { ElementKind::Slab, "slab", "S" },
    { ElementKind::Wall, "wall", "W" },
    { ElementKind::Foundation, "foundation", "F" },
    { ElementKind::TrussMember, "truss", "T" },
    { ElementKind::Cable, "cable", "K" },
};
} // namespace

const char* ifcEntity(BimCategory c)
{
    for (const auto& info : kCategories)
        if (info.category == c) return info.ifc;
    return "";
}

const char* categoryName(BimCategory c)
{
    for (const auto& info : kCategories)
        if (info.category == c) return info.name;
    return "";
}

bool categoryFromName(const std::string& name, BimCategory& out)
{
    for (const auto& info : kCategories)
        if (name == info.name)
        {
            out = info.category;
            return true;
        }
    return false;
}

const char* elementKindKey(ElementKind k)
{
    for (const auto& info : kKinds)
        if (info.kind == k) return info.key;
    return "";
}

bool elementKindFromKey(const std::string& key, ElementKind& out)
{
    for (const auto& info : kKinds)
        if (key == info.key)
        {
            out = info.kind;
            return true;
        }
    return false;
}

std::string AnalyticalRef::label() const
{
    for (const auto& info : kKinds)
        if (info.kind == kind) return info.prefix + std::to_string(id);
    return "?" + std::to_string(id);
}

std::string PropertyValue::toString() const
{
    switch (type)
    {
    case Type::Text:
    case Type::Label:
        return text;
    case Type::Boolean:
        return flag ? "true" : "false";
    case Type::Integer:
        return std::to_string(static_cast<long long>(number));
    default:
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.10g", number);
        return buf;
    }
    }
}

} // namespace TSA::BIM
