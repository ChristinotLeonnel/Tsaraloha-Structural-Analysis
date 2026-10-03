#include "SelectionQuery.h"
#include "Model.h"
#include "../Coordinate/WorkPlane.h"

#include <gp_Pnt.hxx>
#include <cmath>
#include <string>

namespace TSA::Model::SelectionQuery
{

namespace
{
template <typename Map>
void insertAllIds(const Map& map, std::set<int>& out)
{
    for (const auto& [id, _] : map)
        out.insert(id);
}

template <typename Map>
void insertMissingIds(const Map& map, const std::set<int>& exclude, std::set<int>& out)
{
    for (const auto& [id, _] : map)
        if (!exclude.count(id))
            out.insert(id);
}

/// Vrai si tous les nœuds existent et satisfont le prédicat (liste vide -> faux).
template <typename Pred>
bool allNodes(const Model& model, std::initializer_list<int> ids, Pred pred)
{
    bool any = false;
    for (int id : ids)
    {
        const Node* n = model.getNode(id);
        if (!n || !pred(*n)) return false;
        any = true;
    }
    return any;
}

template <typename Pred>
bool allNodes(const Model& model, const std::vector<int>& ids, Pred pred)
{
    if (ids.empty()) return false;
    for (int id : ids)
    {
        const Node* n = model.getNode(id);
        if (!n || !pred(*n)) return false;
    }
    return true;
}
} // namespace

ElementSet all(const Model& model)
{
    ElementSet s;
    insertAllIds(model.nodes(), s.nodes);
    insertAllIds(model.beams(), s.beams);
    insertAllIds(model.columns(), s.columns);
    insertAllIds(model.slabs(), s.slabs);
    insertAllIds(model.walls(), s.walls);
    insertAllIds(model.foundations(), s.foundations);
    insertAllIds(model.trussMembers(), s.trussMembers);
    insertAllIds(model.cables(), s.cables);
    return s;
}

ElementSet byKind(const Model& model, ElementKind kind)
{
    ElementSet s;
    switch (kind)
    {
    case ElementKind::Node:        insertAllIds(model.nodes(), s.nodes); break;
    case ElementKind::Beam:        insertAllIds(model.beams(), s.beams); break;
    case ElementKind::Column:      insertAllIds(model.columns(), s.columns); break;
    case ElementKind::Slab:        insertAllIds(model.slabs(), s.slabs); break;
    case ElementKind::Wall:        insertAllIds(model.walls(), s.walls); break;
    case ElementKind::Foundation:  insertAllIds(model.foundations(), s.foundations); break;
    case ElementKind::TrussMember: insertAllIds(model.trussMembers(), s.trussMembers); break;
    case ElementKind::Cable:       insertAllIds(model.cables(), s.cables); break;
    }
    return s;
}

ElementSet invert(const Model& model, const ElementSet& current)
{
    ElementSet s;
    insertMissingIds(model.nodes(), current.nodes, s.nodes);
    insertMissingIds(model.beams(), current.beams, s.beams);
    insertMissingIds(model.columns(), current.columns, s.columns);
    insertMissingIds(model.slabs(), current.slabs, s.slabs);
    insertMissingIds(model.walls(), current.walls, s.walls);
    insertMissingIds(model.foundations(), current.foundations, s.foundations);
    insertMissingIds(model.trussMembers(), current.trussMembers, s.trussMembers);
    insertMissingIds(model.cables(), current.cables, s.cables);
    return s;
}

ElementSet sameSection(const Model& model, const ElementSet& reference)
{
    std::set<std::string> names;
    for (int id : reference.beams)
        if (const auto* e = model.getBeam(id)) names.insert(e->section().name);
    for (int id : reference.columns)
        if (const auto* e = model.getColumn(id)) names.insert(e->section().name);
    for (int id : reference.trussMembers)
        if (const auto* e = model.getTrussMember(id)) names.insert(e->section().name);

    ElementSet s;
    if (names.empty()) return s;
    for (const auto& [id, e] : model.beams())
        if (names.count(e.section().name)) s.beams.insert(id);
    for (const auto& [id, e] : model.columns())
        if (names.count(e.section().name)) s.columns.insert(id);
    for (const auto& [id, e] : model.trussMembers())
        if (names.count(e.section().name)) s.trussMembers.insert(id);
    return s;
}

ElementSet sameMaterial(const Model& model, const ElementSet& reference)
{
    std::set<std::string> names;
    auto collect = [&](const std::set<int>& ids, auto getter) {
        for (int id : ids)
            if (const auto* e = getter(id)) names.insert(e->material().name);
    };
    collect(reference.beams, [&](int id) { return model.getBeam(id); });
    collect(reference.columns, [&](int id) { return model.getColumn(id); });
    collect(reference.slabs, [&](int id) { return model.getSlab(id); });
    collect(reference.walls, [&](int id) { return model.getWall(id); });
    collect(reference.foundations, [&](int id) { return model.getFoundation(id); });
    collect(reference.trussMembers, [&](int id) { return model.getTrussMember(id); });
    collect(reference.cables, [&](int id) { return model.getCable(id); });

    ElementSet s;
    if (names.empty()) return s;
    auto match = [&](const auto& map, std::set<int>& out) {
        for (const auto& [id, e] : map)
            if (names.count(e.material().name)) out.insert(id);
    };
    match(model.beams(), s.beams);
    match(model.columns(), s.columns);
    match(model.slabs(), s.slabs);
    match(model.walls(), s.walls);
    match(model.foundations(), s.foundations);
    match(model.trussMembers(), s.trussMembers);
    match(model.cables(), s.cables);
    return s;
}

namespace
{
template <typename Pred>
ElementSet byNodePredicate(const Model& model, Pred onTarget)
{
    ElementSet s;
    for (const auto& [id, n] : model.nodes())
        if (onTarget(n)) s.nodes.insert(id);
    for (const auto& [id, e] : model.beams())
        if (allNodes(model, { e.startNodeId(), e.endNodeId() }, onTarget)) s.beams.insert(id);
    for (const auto& [id, e] : model.columns())
        if (allNodes(model, { e.startNodeId(), e.endNodeId() }, onTarget)) s.columns.insert(id);
    for (const auto& [id, e] : model.trussMembers())
        if (allNodes(model, { e.startNodeId(), e.endNodeId() }, onTarget)) s.trussMembers.insert(id);
    for (const auto& [id, e] : model.cables())
        if (allNodes(model, { e.startNodeId(), e.endNodeId() }, onTarget)) s.cables.insert(id);
    for (const auto& [id, e] : model.walls())
        if (allNodes(model, { e.startNodeId(), e.endNodeId() }, onTarget)) s.walls.insert(id);
    for (const auto& [id, e] : model.slabs())
        if (allNodes(model, e.nodeIds(), onTarget)) s.slabs.insert(id);
    for (const auto& [id, e] : model.foundations())
        if (allNodes(model, { e.nodeId() }, onTarget)) s.foundations.insert(id);
    return s;
}
} // namespace

ElementSet atElevation(const Model& model, double z, double tol)
{
    return byNodePredicate(model, [&](const Node& n) { return std::abs(n.z() - z) <= tol; });
}

ElementSet onWorkPlane(const Model& model, const TSA::Coordinate::WorkPlane& plane, double tol)
{
    return byNodePredicate(model, [&](const Node& n) {
        return std::abs(plane.distanceTo(gp_Pnt(n.x(), n.y(), n.z()))) <= tol;
    });
}

} // namespace TSA::Model::SelectionQuery
