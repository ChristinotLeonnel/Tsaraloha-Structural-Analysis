#include "MeshDisplayModel.h"

#include <algorithm>

namespace TSA::Mesh
{

namespace
{
// Ordres de Gmsh (MLine, MTriangle, MQuadrangle, MTetrahedron::edges_tetra / faces_tetra,
// MHexahedron::edges_hexa / faces_hexa) : nœuds de coins puis un nœud milieu par arête, dans l'ordre des arêtes.
ElementTopology make(FeType type, int dim, int corners, std::vector<std::array<int, 2>> edges, bool secondOrder,
                     std::vector<std::vector<int>> faces)
{
    ElementTopology t;
    t.type = type;
    t.dimension = dim;
    t.cornerCount = corners;
    t.edges = std::move(edges);
    t.faces = std::move(faces);
    t.edgeMid.assign(t.edges.size(), -1);
    if (secondOrder)
        for (std::size_t k = 0; k < t.edges.size(); ++k) t.edgeMid[k] = corners + int(k);
    t.nodeCount = corners + (secondOrder ? int(t.edges.size()) : 0);
    return t;
}

const std::vector<std::array<int, 2>> kLine { { 0, 1 } };
const std::vector<std::array<int, 2>> kTri { { 0, 1 }, { 1, 2 }, { 2, 0 } };
const std::vector<std::array<int, 2>> kQuad { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 } };
const std::vector<std::array<int, 2>> kTet { { 0, 1 }, { 1, 2 }, { 2, 0 }, { 3, 0 }, { 3, 2 }, { 3, 1 } };
const std::vector<std::array<int, 2>> kHex { { 0, 1 }, { 0, 3 }, { 0, 4 }, { 1, 2 }, { 1, 5 }, { 2, 3 },
                                              { 2, 6 }, { 3, 7 }, { 4, 5 }, { 4, 7 }, { 5, 6 }, { 6, 7 } };
const std::vector<std::vector<int>> kTetFaces { { 0, 2, 1 }, { 0, 1, 3 }, { 0, 3, 2 }, { 3, 1, 2 } };
const std::vector<std::vector<int>> kHexFaces { { 0, 3, 2, 1 }, { 0, 1, 5, 4 }, { 0, 4, 7, 3 },
                                                { 1, 2, 6, 5 }, { 2, 3, 7, 6 }, { 4, 5, 6, 7 } };

const std::vector<ElementTopology>& table()
{
    static const std::vector<ElementTopology> t {
        make(FeType::Line2, 1, 2, kLine, false, {}),
        make(FeType::Line3, 1, 2, kLine, true, {}),
        make(FeType::Tri3, 2, 3, kTri, false, { { 0, 1, 2 } }),
        make(FeType::Tri6, 2, 3, kTri, true, { { 0, 1, 2 } }),
        make(FeType::Quad4, 2, 4, kQuad, false, { { 0, 1, 2, 3 } }),
        make(FeType::Quad8, 2, 4, kQuad, true, { { 0, 1, 2, 3 } }),
        make(FeType::Hex8, 3, 8, kHex, false, kHexFaces),
        make(FeType::Hex20, 3, 8, kHex, true, kHexFaces),
        make(FeType::Tet4, 3, 4, kTet, false, kTetFaces),
        make(FeType::Tet10, 3, 4, kTet, true, kTetFaces),
    };
    return t;
}

/// Clé d'une face : identifiants de ses coins triés (3 ou 4, complétés par 0).
struct FaceKey
{
    std::array<int, 4> ids {};
    bool operator==(const FaceKey& o) const { return ids == o.ids; }
};
struct FaceKeyHash
{
    std::size_t operator()(const FaceKey& k) const
    {
        std::size_t h = 1469598103934665603ull;
        for (int v : k.ids) h = (h ^ std::size_t(std::uint32_t(v))) * 1099511628211ull;
        return h;
    }
};
} // namespace

const ElementTopology* elementTopology(FeType type)
{
    for (const auto& t : table())
        if (t.type == type) return &t;
    return nullptr;
}

int feTypeNodeCount(FeType type)
{
    const ElementTopology* t = elementTopology(type);
    return t ? t->nodeCount : 0;
}

std::string feTypeLabel(FeType type)
{
    switch (type)
    {
    case FeType::Line2: return "Line2 — segment à 2 nœuds";
    case FeType::Line3: return "Line3 — segment à 3 nœuds";
    case FeType::Tri3: return "Tri3 — triangle à 3 nœuds";
    case FeType::Tri6: return "Tri6 — triangle à 6 nœuds";
    case FeType::Quad4: return "Quad4 — quadrangle à 4 nœuds";
    case FeType::Quad8: return "Quad8 — quadrangle à 8 nœuds";
    case FeType::Hex8: return "Hex8 — hexaèdre à 8 nœuds";
    case FeType::Hex20: return "Hex20 — hexaèdre à 20 nœuds";
    case FeType::Tet4: return "Tet4 — tétraèdre à 4 nœuds";
    case FeType::Tet10: return "Tet10 — tétraèdre à 10 nœuds";
    }
    return "type inconnu";
}

std::vector<int> faceOutline(const ElementTopology& topo, const std::vector<int>& face)
{
    std::vector<int> out;
    out.reserve(face.size() * 2);
    for (std::size_t i = 0; i < face.size(); ++i)
    {
        const int a = face[i], b = face[(i + 1) % face.size()];
        out.push_back(a);
        for (std::size_t k = 0; k < topo.edges.size(); ++k)
        {
            const auto& e = topo.edges[k];
            if ((e[0] == a && e[1] == b) || (e[0] == b && e[1] == a))
            {
                if (topo.edgeMid[k] >= 0) out.push_back(topo.edgeMid[k]);
                break;
            }
        }
    }
    return out;
}

bool MeshDisplayModel::build(const FeMesh& mesh, const MeshDisplayFilter& filter)
{
    m_mesh = &mesh;
    m_filter = filter;
    m_elementIndex.clear();
    m_nodeIndex.clear();
    m_visibleElements.clear();
    m_visibleNodes.clear();
    m_visibleFlag.assign(mesh.elements.size(), 0);
    m_volumeFaces.clear();
    m_exteriorFaces = m_interiorFaces = 0;
    m_edges.clear();
    m_groupsByElement.clear();
    m_groupsByNode.clear();
    m_issues.clear();
    m_countByDim = { 0, 0, 0, 0 };

    // 1. Correspondances identifiant métier → indice (jamais supposées consécutives ni égales).
    m_nodeIndex.reserve(mesh.nodes.size());
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
        if (!m_nodeIndex.emplace(mesh.nodes[i].id, int(i)).second)
            m_issues.push_back({ 0, "nœud " + std::to_string(mesh.nodes[i].id) + " : identifiant dupliqué (seul le premier est utilisé)" });
    m_elementIndex.reserve(mesh.elements.size());
    for (std::size_t i = 0; i < mesh.elements.size(); ++i)
        if (!m_elementIndex.emplace(mesh.elements[i].id, int(i)).second)
            m_issues.push_back({ mesh.elements[i].id, "identifiant d'élément dupliqué : élément écarté" });
    std::set<std::size_t> isolated;
    for (std::size_t g = 0; g < mesh.groups.size(); ++g)
    {
        for (int e : mesh.groups[g].elements) m_groupsByElement.emplace(e, int(g));
        for (int n : mesh.groups[g].nodes) m_groupsByNode.emplace(n, int(g));
        if (filter.isolatedGroups.count(mesh.groups[g].name)) isolated.insert(g);
    }

    // 2. Éléments affichables : type pris en charge, nombre de nœuds exact, nœuds connus, filtre.
    std::vector<char> nodeUsed(mesh.nodes.size(), 0);
    for (std::size_t i = 0; i < mesh.elements.size(); ++i)
    {
        const FeElement& e = mesh.elements[i];
        if (m_elementIndex.at(e.id) != int(i)) continue;
        const ElementTopology* topo = elementTopology(e.type);
        if (e.source.rfind(kFeUnknownTypePrefix, 0) == 0)
        {
            m_issues.push_back({ e.id, "type d'élément inconnu dans le projet (élément écarté) : " + e.source });
            continue;
        }
        if (!topo)
        {
            m_issues.push_back({ e.id, "type d'élément non pris en charge par l'affichage" });
            continue;
        }
        if (int(e.nodes.size()) != topo->nodeCount)
        {
            m_issues.push_back({ e.id, std::string(feTypeName(e.type)) + " : " + std::to_string(e.nodes.size()) + " nœuds au lieu de " +
                                           std::to_string(topo->nodeCount) + " (élément écarté)" });
            continue;
        }
        bool known = true;
        for (int n : e.nodes) known &= m_nodeIndex.count(n) > 0;
        if (!known)
        {
            m_issues.push_back({ e.id, "nœud inexistant dans la connectivité (élément écarté)" });
            continue;
        }
        if (e.boundary && !filter.boundaryElements) continue;
        const int dim = topo->dimension;
        if ((dim == 1 && !filter.show1D) || (dim == 2 && !filter.show2D) || (dim == 3 && !filter.show3D)) continue;
        if (!isolated.empty())
        {
            bool inGroup = false;
            auto [lo, hi] = m_groupsByElement.equal_range(e.id);
            for (auto it = lo; it != hi && !inGroup; ++it) inGroup = isolated.count(std::size_t(it->second)) > 0;
            if (!inGroup) continue;
        }
        if (filter.cut)
        {
            double d = 0.0;
            for (int k = 0; k < topo->cornerCount; ++k)
            {
                const FeNode& n = mesh.nodes[std::size_t(m_nodeIndex.at(e.nodes[std::size_t(k)]))];
                d += filter.cutNormal[0] * n.x + filter.cutNormal[1] * n.y + filter.cutNormal[2] * n.z;
            }
            if (d / topo->cornerCount > filter.cutOffset) continue;
        }
        m_visibleFlag[i] = 1;
        m_visibleElements.push_back(e.id);
        ++m_countByDim[std::size_t(dim)];
        for (int n : e.nodes) nodeUsed[std::size_t(m_nodeIndex.at(n))] = 1;
    }
    std::sort(m_visibleElements.begin(), m_visibleElements.end());
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
        if (nodeUsed[i]) m_visibleNodes.push_back(mesh.nodes[i].id);
    std::sort(m_visibleNodes.begin(), m_visibleNodes.end());

    // 3. Faces des volumes : une face partagée par deux volumes visibles est intérieure.
    std::unordered_map<FaceKey, int, FaceKeyHash> faceUse;
    auto keyOf = [&](const FeElement& e, const std::vector<int>& face) {
        FaceKey k;
        for (std::size_t j = 0; j < face.size() && j < 4; ++j) k.ids[j] = e.nodes[std::size_t(face[j])];
        std::sort(k.ids.begin(), k.ids.begin() + std::min<std::size_t>(face.size(), 4));
        return k;
    };
    if (m_countByDim[3] > 0)
    {
        faceUse.reserve(m_countByDim[3] * 4);
        for (int id : m_visibleElements)
        {
            const FeElement& e = mesh.elements[std::size_t(m_elementIndex.at(id))];
            const ElementTopology* topo = elementTopology(e.type);
            if (topo->dimension != 3) continue;
            for (const auto& f : topo->faces) ++faceUse[keyOf(e, f)];
        }
    }

    // 4. Faces affichées et arêtes uniques (éléments 2D, faces affichées des volumes).
    std::unordered_map<std::uint64_t, std::size_t> edgeSeen;
    auto addEdge = [&](int a, int b, int mid) {
        const int lo = std::min(a, b), hi = std::max(a, b);
        const std::uint64_t key = (std::uint64_t(std::uint32_t(lo)) << 32) | std::uint32_t(hi);
        if (edgeSeen.emplace(key, m_edges.size()).second) m_edges.push_back({ lo, hi, mid });
    };
    for (int id : m_visibleElements)
    {
        const FeElement& e = mesh.elements[std::size_t(m_elementIndex.at(id))];
        const ElementTopology* topo = elementTopology(e.type);
        auto edgeOf = [&](int a, int b) {
            for (std::size_t k = 0; k < topo->edges.size(); ++k)
            {
                const auto& ed = topo->edges[k];
                if ((ed[0] == a && ed[1] == b) || (ed[0] == b && ed[1] == a))
                    addEdge(e.nodes[std::size_t(a)], e.nodes[std::size_t(b)], topo->edgeMid[k] >= 0 ? e.nodes[std::size_t(topo->edgeMid[k])] : 0);
            }
        };
        if (topo->dimension == 2)
        {
            for (const auto& ed : topo->edges) edgeOf(ed[0], ed[1]);
        }
        else if (topo->dimension == 3)
        {
            auto& shown = m_volumeFaces[id];
            for (const auto& f : topo->faces)
            {
                const bool exterior = faceUse[keyOf(e, f)] == 1;
                exterior ? ++m_exteriorFaces : ++m_interiorFaces;
                if (filter.volumeExteriorOnly && !exterior) continue;
                shown.push_back(faceOutline(*topo, f));
                for (std::size_t j = 0; j < f.size(); ++j) edgeOf(f[j], f[(j + 1) % f.size()]);
            }
        }
    }
    // Faces intérieures comptées deux fois (une par volume) : nombre de faces géométriques.
    m_interiorFaces /= 2;
    return !m_visibleElements.empty();
}

bool MeshDisplayModel::isElementVisible(int id) const
{
    const int i = elementIndex(id);
    return i >= 0 && m_visibleFlag[std::size_t(i)] != 0;
}

int MeshDisplayModel::elementIndex(int elementId) const
{
    const auto it = m_elementIndex.find(elementId);
    return it == m_elementIndex.end() ? -1 : it->second;
}

int MeshDisplayModel::nodeIndex(int nodeId) const
{
    const auto it = m_nodeIndex.find(nodeId);
    return it == m_nodeIndex.end() ? -1 : it->second;
}

const FeElement* MeshDisplayModel::element(int elementId) const
{
    const int i = elementIndex(elementId);
    return i < 0 ? nullptr : &m_mesh->elements[std::size_t(i)];
}

const FeNode* MeshDisplayModel::node(int nodeId) const
{
    const int i = nodeIndex(nodeId);
    return i < 0 ? nullptr : &m_mesh->nodes[std::size_t(i)];
}

const std::vector<std::vector<int>>& MeshDisplayModel::volumeFaces(int elementId) const
{
    static const std::vector<std::vector<int>> kNone;
    const auto it = m_volumeFaces.find(elementId);
    return it == m_volumeFaces.end() ? kNone : it->second;
}

std::vector<std::string> MeshDisplayModel::groupsOfElement(int elementId) const
{
    std::vector<std::string> out;
    auto [lo, hi] = m_groupsByElement.equal_range(elementId);
    for (auto it = lo; it != hi; ++it) out.push_back(m_mesh->groups[std::size_t(it->second)].name);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> MeshDisplayModel::groupsOfNode(int nodeId) const
{
    std::vector<std::string> out;
    auto [lo, hi] = m_groupsByNode.equal_range(nodeId);
    for (auto it = lo; it != hi; ++it) out.push_back(m_mesh->groups[std::size_t(it->second)].name);
    std::sort(out.begin(), out.end());
    return out;
}

bool MeshDisplayModel::labelSubset(const std::vector<int>& ids, std::size_t maxLabels, std::vector<int>* out)
{
    out->clear();
    if (ids.size() <= maxLabels)
    {
        *out = ids;
        return false;
    }
    // Répartition régulière sur toute la liste (pas seulement les premiers identifiants).
    const double step = double(ids.size()) / double(std::max<std::size_t>(1, maxLabels));
    for (std::size_t k = 0; k < maxLabels; ++k) out->push_back(ids[std::min(ids.size() - 1, std::size_t(k * step))]);
    return true;
}

} // namespace TSA::Mesh
