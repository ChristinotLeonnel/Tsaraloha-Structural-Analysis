#include "SolverMesh.h"

#include <QCoreApplication>

#include <algorithm>
#include <set>
#include <sstream>

namespace TSA::Analysis
{

const char* solverCellTypeName(SolverCellType t)
{
    switch (t)
    {
    case SolverCellType::Line: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "linéaire");
    case SolverCellType::ZeroLength: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "longueur nulle");
    case SolverCellType::Triangle: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "triangle");
    case SolverCellType::Quadrilateral: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "quadrangle");
    case SolverCellType::Tetrahedron: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "tétraèdre");
    case SolverCellType::Hexahedron: return QT_TRANSLATE_NOOP("TSA::Analysis::SolverMesh", "hexaèdre");
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
    // Texte affiché (barre d'état de la vue, journal du maillage) : contexte « TSA::Analysis::SolverMesh ».
    QString s = QCoreApplication::translate("TSA::Analysis::SolverMesh", "%1 : %2 nœud(s)")
                    .arg(engineId.empty() ? QCoreApplication::translate("TSA::Analysis::SolverMesh", "moteur") : QString::fromStdString(engineId),
                         QString::number(nodes.size()));
    if (const auto internal = internalNodeCount())
        s += QCoreApplication::translate("TSA::Analysis::SolverMesh", " dont %1 propre(s) au moteur").arg(internal);
    s += QCoreApplication::translate("TSA::Analysis::SolverMesh", ", %1 élément(s) fini(s)").arg(cells.size());
    bool first = true;
    for (auto t : { SolverCellType::Line, SolverCellType::ZeroLength, SolverCellType::Triangle, SolverCellType::Quadrilateral,
                    SolverCellType::Tetrahedron, SolverCellType::Hexahedron })
    {
        if (const auto n = cellCount(t))
        {
            s += QString::fromLatin1(first ? " (" : ", ") + QString::number(n) + QLatin1Char(' ')
                 + QCoreApplication::translate("TSA::Analysis::SolverMesh", solverCellTypeName(t));
            first = false;
        }
    }
    if (!first) s += QLatin1Char(')');
    // Subdivisions : nombre maximal d'éléments finis par élément TSA.
    std::map<ElementKey, std::size_t> perSource;
    for (const auto& c : cells)
        if (c.source) ++perSource[*c.source];
    std::size_t maxPer = 0;
    for (const auto& [k, n] : perSource) maxPer = std::max(maxPer, n);
    if (maxPer > 1) s += QCoreApplication::translate("TSA::Analysis::SolverMesh", " ; jusqu'à %1 éléments finis par élément TSA").arg(maxPer);
    else if (maxPer == 1) s += QCoreApplication::translate("TSA::Analysis::SolverMesh", " ; 1 élément fini par élément TSA (aucune subdivision)");
    return s.toStdString();
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
