#include "ModelingTool.h"

#include "../../Model/Model.h"

#include <gp_Vec.hxx>

#include <cmath>

namespace TSA::Interaction
{

double ModelingTool::param(const std::string& key) const
{
    for (const auto& p : m_params)
        if (p.key == key) return p.value;
    return 0.0;
}

gp_Pnt ModelingTool::pointParam(const std::string& key) const
{
    for (const auto& p : m_params)
        if (p.key == key) return p.point;
    return gp_Pnt();
}

void ModelingTool::setParam(const std::string& key, double value)
{
    for (auto& p : m_params)
        if (p.key == key) p.value = std::min(p.max, std::max(p.min, value));
}

void ModelingTool::setPointParam(const std::string& key, const gp_Pnt& pt)
{
    for (auto& p : m_params)
        if (p.key == key) p.point = pt;
}

void ModelingToolRegistry::registerTool(Factory factory)
{
    auto probe = factory();
    if (!probe) return;
    const std::string id = probe->id();
    for (const auto& [existing, f] : m_factories)
        if (existing == id) return;
    m_factories.emplace_back(id, std::move(factory));
}

std::unique_ptr<ModelingTool> ModelingToolRegistry::create(const std::string& id) const
{
    for (const auto& [key, f] : m_factories)
        if (key == id) return f();
    return nullptr;
}

std::vector<std::unique_ptr<ModelingTool>> ModelingToolRegistry::instances() const
{
    std::vector<std::unique_ptr<ModelingTool>> out;
    for (const auto& [key, f] : m_factories) out.push_back(f());
    return out;
}

std::vector<std::string> ModelingToolRegistry::ids() const
{
    std::vector<std::string> out;
    for (const auto& [key, f] : m_factories) out.push_back(key);
    return out;
}

namespace ToolGeometry
{

bool barEnds(const TSA::Model::Model& model, const BarRef& bar, gp_Pnt& a, gp_Pnt& b, int* startId, int* endId)
{
    int s = 0, e = 0;
    using TSA::Model::ElementKind;
    switch (bar.kind)
    {
    case ElementKind::Beam: if (const auto* x = model.getBeam(bar.id)) { s = x->startNodeId(); e = x->endNodeId(); } break;
    case ElementKind::Column: if (const auto* x = model.getColumn(bar.id)) { s = x->startNodeId(); e = x->endNodeId(); } break;
    case ElementKind::TrussMember: if (const auto* x = model.getTrussMember(bar.id)) { s = x->startNodeId(); e = x->endNodeId(); } break;
    case ElementKind::Cable: if (const auto* x = model.getCable(bar.id)) { s = x->startNodeId(); e = x->endNodeId(); } break;
    default: return false;
    }
    const auto* ns = model.getNode(s);
    const auto* ne = model.getNode(e);
    if (!ns || !ne) return false;
    a = gp_Pnt(ns->x(), ns->y(), ns->z());
    b = gp_Pnt(ne->x(), ne->y(), ne->z());
    if (startId) *startId = s;
    if (endId) *endId = e;
    return true;
}

bool closestParameters(const gp_Pnt& a1, const gp_Pnt& a2, const gp_Pnt& b1, const gp_Pnt& b2, double& ta, double& tb)
{
    const gp_Vec u(a1, a2), v(b1, b2), w(b1, a1);
    const double a = u.Dot(u), b = u.Dot(v), c = v.Dot(v), d = u.Dot(w), e = v.Dot(w);
    const double den = a * c - b * b;
    if (a < 1e-12 || c < 1e-12 || std::abs(den) < 1e-12 * a * c) return false;
    ta = (b * e - c * d) / den;
    tb = (a * e - b * d) / den;
    return true;
}

int nodeAt(TSA::Model::Model& model, const gp_Pnt& p, double tol)
{
    for (const auto& [id, n] : model.nodes())
        if (p.Distance(gp_Pnt(n.x(), n.y(), n.z())) <= tol) return id;
    return model.addNode(p.X(), p.Y(), p.Z());
}

std::set<int> nodeClosure(const TSA::Model::Model& model, const TSA::Model::ElementSet& set)
{
    std::set<int> nodes = set.nodes;
    auto ends = [&](int a, int b) { nodes.insert(a); nodes.insert(b); };
    for (int id : set.beams) if (const auto* e = model.getBeam(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : set.columns) if (const auto* e = model.getColumn(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : set.trussMembers) if (const auto* e = model.getTrussMember(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : set.cables) if (const auto* e = model.getCable(id)) ends(e->startNodeId(), e->endNodeId());
    for (int id : set.slabs) if (const auto* e = model.getSlab(id)) nodes.insert(e->nodeIds().begin(), e->nodeIds().end());
    return nodes;
}

} // namespace ToolGeometry

} // namespace TSA::Interaction
