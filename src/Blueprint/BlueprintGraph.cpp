#include "BlueprintGraph.h"

#include <algorithm>

namespace TSA::Blueprint
{

const PinSpec* NodeDefinition::input(const std::string& name) const
{
    for (const auto& p : inputs)
        if (p.name == name) return &p;
    return nullptr;
}

const PinSpec* NodeDefinition::output(const std::string& name) const
{
    for (const auto& p : outputs)
        if (p.name == name) return &p;
    return nullptr;
}

bool dataCompatible(const PinSpec& from, const PinSpec& to)
{
    if (from.kind != PinKind::Data || to.kind != PinKind::Data) return false;
    if (from.any || to.any) return true;
    if (from.type == to.type) return true;
    return from.type == ValueType::Integer && (to.type == ValueType::Real || to.type == ValueType::IdList);
}

int Graph::addNode(const std::string& type, double x, double y)
{
    while (m_nodes.count(m_nextId)) ++m_nextId;
    NodeInstance n;
    n.id = m_nextId++;
    n.type = type;
    n.x = x;
    n.y = y;
    m_nodes[n.id] = n;
    return n.id;
}

bool Graph::insertNode(const NodeInstance& node)
{
    if (node.id <= 0 || m_nodes.count(node.id)) return false;
    m_nodes[node.id] = node;
    m_nextId = std::max(m_nextId, node.id + 1);
    return true;
}

void Graph::removeNode(int id)
{
    m_nodes.erase(id);
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                 [id](const Link& l) { return l.fromNode == id || l.toNode == id; }),
                  m_links.end());
}

NodeInstance* Graph::node(int id)
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : &it->second;
}

const NodeInstance* Graph::node(int id) const
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : &it->second;
}

void Graph::addLink(const Link& link)
{
    if (std::find(m_links.begin(), m_links.end(), link) == m_links.end()) m_links.push_back(link);
}

void Graph::removeLink(const Link& link)
{
    m_links.erase(std::remove(m_links.begin(), m_links.end(), link), m_links.end());
}

std::optional<Link> Graph::linkTo(int node, const std::string& pin) const
{
    for (const auto& l : m_links)
        if (l.toNode == node && l.toPin == pin) return l;
    return std::nullopt;
}

std::vector<Link> Graph::linksFrom(int node, const std::string& pin) const
{
    std::vector<Link> out;
    for (const auto& l : m_links)
        if (l.fromNode == node && l.fromPin == pin) out.push_back(l);
    return out;
}

void Graph::setValue(int node, const std::string& pin, const Value& value)
{
    if (auto* n = this->node(node)) n->values[pin] = value;
}

void Graph::clear()
{
    m_nodes.clear();
    m_links.clear();
    m_nextId = 1;
    name.clear();
    description.clear();
}

} // namespace TSA::Blueprint
