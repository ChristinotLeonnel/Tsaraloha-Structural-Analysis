#include "IfcRelationshipMapper.h"

#include "../Core/IfcGuid.h"

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;

int IfcRelationshipMapper::write()
{
    auto& w = m_ctx.w;
    int n = 0;
    auto head = [] { return W::str(IfcGuid::create()) + ",$,$,$,"; };

    for (const auto& [parent, children] : m_aggregates)
        if (!children.empty()) w.add("IFCRELAGGREGATES", head() + W::ref(parent) + "," + W::refs(children)), ++n;
    for (const auto& [spatial, products] : m_contained)
        if (!products.empty()) w.add("IFCRELCONTAINEDINSPATIALSTRUCTURE", head() + W::refs(products) + "," + W::ref(spatial)), ++n;
    for (const auto& [mat, objects] : m_materials)
        if (!objects.empty()) w.add("IFCRELASSOCIATESMATERIAL", head() + W::refs(objects) + "," + W::ref(mat)), ++n;
    for (const auto& [pset, object] : m_psets)
        w.add("IFCRELDEFINESBYPROPERTIES", head() + W::refs({ object }) + "," + W::ref(pset)), ++n;
    for (const auto& [ref, objects] : m_classifications)
        if (!objects.empty()) w.add("IFCRELASSOCIATESCLASSIFICATION", head() + W::refs(objects) + "," + W::ref(ref)), ++n;
    for (const auto& [group, objects] : m_groups)
        if (!objects.empty()) w.add("IFCRELASSIGNSTOGROUP", head() + W::refs(objects) + ",$," + W::ref(group)), ++n;
    for (const auto& [product, analytical] : m_products)
        if (!analytical.empty()) w.add("IFCRELASSIGNSTOPRODUCT", head() + W::refs(analytical) + ",$," + W::ref(product)), ++n;
    for (const auto& [member, connection] : m_connections)
        w.add("IFCRELCONNECTSSTRUCTURALMEMBER", head() + W::ref(member) + "," + W::ref(connection) + ",$,$,$,$"), ++n;
    return n;
}

} // namespace TSA::BIM::Ifc
