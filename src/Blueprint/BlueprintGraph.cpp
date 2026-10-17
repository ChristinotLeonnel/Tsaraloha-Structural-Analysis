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
    if (from.type == ValueType::Object || to.type == ValueType::Object)
        return from.type == to.type && (from.objectType.empty() || to.objectType.empty() || from.objectType == to.objectType);
    if (from.type == to.type) return true;
    return from.type == ValueType::Integer && (to.type == ValueType::Real || to.type == ValueType::IdList);
}

std::string pinTypeName(const PinSpec& pin)
{
    if (pin.kind == PinKind::Exec) return "exécution";
    if (pin.any) return "générique";
    if (pin.type == ValueType::Object) return pin.objectType.empty() ? std::string("objet") : pin.objectType;
    return TSA::Automation::typeName(pin.type);
}

const char* statusName(NodeStatus status)
{
    switch (status)
    {
    case NodeStatus::Standard: return "standard";
    case NodeStatus::Validated: return "validé";
    case NodeStatus::Experimental: return "expérimental";
    }
    return "?";
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

int Graph::addComment(const Comment& comment)
{
    Comment c = comment;
    if (c.id <= 0 || m_comments.count(c.id))
    {
        while (m_comments.count(m_nextComment)) ++m_nextComment;
        c.id = m_nextComment++;
    }
    m_nextComment = std::max(m_nextComment, c.id + 1);
    m_comments[c.id] = c;
    return c.id;
}

void Graph::removeComment(int id)
{
    m_comments.erase(id);
}

Comment* Graph::comment(int id)
{
    const auto it = m_comments.find(id);
    return it == m_comments.end() ? nullptr : &it->second;
}

const Comment* Graph::comment(int id) const
{
    const auto it = m_comments.find(id);
    return it == m_comments.end() ? nullptr : &it->second;
}

std::vector<int> Graph::nodesInComment(int id) const
{
    std::vector<int> out;
    const Comment* c = comment(id);
    if (!c) return out;
    for (const auto& [nid, n] : m_nodes)
        if (n.x >= c->x && n.x <= c->x + c->width && n.y >= c->y && n.y <= c->y + c->height) out.push_back(nid);
    return out;
}

const SubgraphDefinition* Graph::subgraph(const std::string& id) const
{
    for (const auto& d : subgraphs)
        if (d.id == id) return &d;
    return nullptr;
}

namespace
{
bool samePin(const PinSpec& a, const PinSpec& b)
{
    return a.name == b.name && a.label == b.label && a.kind == b.kind && a.type == b.type && a.any == b.any && a.quantity == b.quantity
           && a.required == b.required && a.objectType == b.objectType && TSA::Automation::sameValue(a.defaultValue, b.defaultValue);
}

bool samePins(const std::vector<PinSpec>& a, const std::vector<PinSpec>& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), samePin);
}
} // namespace

bool SubgraphDefinition::sameContent(const SubgraphDefinition& o) const
{
    if (id != o.id || name != o.name || description != o.description || category != o.category || documentation != o.documentation
        || version != o.version || !samePins(inputs, o.inputs) || !samePins(outputs, o.outputs))
        return false;
    if (!body || !o.body) return !body && !o.body;
    return body->sameContent(*o.body);
}

void Graph::clear()
{
    subgraphs.clear();
    m_nodes.clear();
    m_links.clear();
    m_comments.clear();
    m_nextId = 1;
    m_nextComment = 1;
    name.clear();
    description.clear();
}

bool Graph::sameContent(const Graph& other) const
{
    if (name != other.name || description != other.description || m_nodes != other.m_nodes || m_comments != other.m_comments
        || m_links.size() != other.m_links.size())
        return false;
    for (const auto& l : m_links)
        if (std::find(other.m_links.begin(), other.m_links.end(), l) == other.m_links.end()) return false;
    if (subgraphs.size() != other.subgraphs.size()) return false;
    for (const auto& d : subgraphs)
    {
        const SubgraphDefinition* o = other.subgraph(d.id);
        if (!o || !d.sameContent(*o)) return false;
    }
    return true;
}

} // namespace TSA::Blueprint
