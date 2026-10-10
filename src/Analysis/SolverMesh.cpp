#include "SolverMesh.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace TSA::Analysis
{

const char* solverCellTypeName(SolverCellType t)
{
    switch (t)
    {
    case SolverCellType::Line: return "linéaire";
    case SolverCellType::ZeroLength: return "longueur nulle";
    case SolverCellType::Triangle: return "triangle";
    case SolverCellType::Quadrilateral: return "quadrangle";
    case SolverCellType::Tetrahedron: return "tétraèdre";
    case SolverCellType::Hexahedron: return "hexaèdre";
    }
    return "?";
}

const SolverMeshNode* SolverMesh::node(int tag) const
{
    auto it = nodes.find(tag);
    return it != nodes.end() ? &it->second : nullptr;
}

std::size_t SolverMesh::cellCount(SolverCellType t) const
{
    std::size_t n = 0;
    for (const auto& c : cells) n += c.type == t ? 1 : 0;
    return n;
}

std::size_t SolverMesh::internalNodeCount() const
{
    std::size_t n = 0;
    for (const auto& [tag, nd] : nodes) n += nd.tsaNodeId == 0 ? 1 : 0;
    return n;
}

std::size_t SolverMesh::cellsFor(const ElementKey& key) const
{
    std::size_t n = 0;
    for (const auto& c : cells) n += (c.source && *c.source == key) ? 1 : 0;
    return n;
}

std::string SolverMesh::summary() const
{
    std::ostringstream s;
    s << (engineId.empty() ? std::string("moteur") : engineId) << " : " << nodes.size() << " nœud(s)";
    if (const auto internal = internalNodeCount()) s << " dont " << internal << " propre(s) au moteur";
    s << ", " << cells.size() << " élément(s) fini(s)";
    bool first = true;
    for (auto t : { SolverCellType::Line, SolverCellType::ZeroLength, SolverCellType::Triangle, SolverCellType::Quadrilateral,
                    SolverCellType::Tetrahedron, SolverCellType::Hexahedron })
    {
        if (const auto n = cellCount(t))
        {
            s << (first ? " (" : ", ") << n << " " << solverCellTypeName(t);
            first = false;
        }
    }
    if (!first) s << ")";
    // Subdivisions : nombre maximal d'éléments finis par élément TSA.
    std::map<ElementKey, std::size_t> perSource;
    for (const auto& c : cells)
        if (c.source) ++perSource[*c.source];
    std::size_t maxPer = 0;
    for (const auto& [k, n] : perSource) maxPer = std::max(maxPer, n);
    if (maxPer > 1) s << " ; jusqu'à " << maxPer << " éléments finis par élément TSA";
    else if (maxPer == 1) s << " ; 1 élément fini par élément TSA (aucune subdivision)";
    return s.str();
}

std::vector<std::string> SolverMesh::validate() const
{
    std::vector<std::string> out;
    std::set<int> tags;
    for (const auto& c : cells)
    {
        if (!tags.insert(c.tag).second) out.push_back("élément " + std::to_string(c.tag) + " en double");
        const std::size_t expected = c.type == SolverCellType::Line || c.type == SolverCellType::ZeroLength ? 2
                                   : c.type == SolverCellType::Triangle                                    ? 3
                                   : c.type == SolverCellType::Quadrilateral || c.type == SolverCellType::Tetrahedron ? 4
                                                                                                                       : 8;
        if (c.nodes.size() < expected)
            out.push_back("élément " + std::to_string(c.tag) + " : " + std::to_string(c.nodes.size()) + " nœud(s), "
                          + std::to_string(expected) + " attendu(s)");
        for (int n : c.nodes)
            if (!node(n)) out.push_back("élément " + std::to_string(c.tag) + " : nœud " + std::to_string(n) + " inconnu");
    }
    return out;
}

} // namespace TSA::Analysis
