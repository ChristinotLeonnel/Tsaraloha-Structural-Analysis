#include "MeshVSDataSource.h"

#include <NCollection_HArray1.hxx>
#include <NCollection_Sequence.hxx>

namespace TSA::Viewer
{

using TSA::Mesh::ElementTopology;
using TSA::Mesh::elementTopology;

MeshVSDataSource::MeshVSDataSource(const TSA::Mesh::MeshDisplayModel& model)
    : m_model(model)
{
    for (int id : model.visibleNodes()) m_nodes.Add(id);
    for (int id : model.visibleElements())
    {
        const auto* e = model.element(id);
        // Volume entièrement intérieur (aucune face affichée) : rien à dessiner ni à sélectionner.
        if (e && TSA::Mesh::feTypeDimension(e->type) == 3 && model.volumeFaces(id).empty()) continue;
        m_elements.Add(id);
    }
}

void MeshVSDataSource::setDisplacements(const std::unordered_map<int, std::array<double, 3>>* displacements, double scale)
{
    m_displacements = displacements;
    m_scale = scale;
}

gp_Pnt MeshVSDataSource::position(int nodeId) const
{
    const auto* n = m_model.node(nodeId);
    if (!n) return gp_Pnt();
    gp_Pnt p(n->x, n->y, n->z);
    if (m_displacements && m_scale != 0.0)
        if (const auto it = m_displacements->find(nodeId); it != m_displacements->end())
            p.Translate(gp_Vec(it->second[0], it->second[1], it->second[2]) * m_scale);
    return p;
}

bool MeshVSDataSource::displayNodes(int elementId, std::vector<int>& ids) const
{
    ids.clear();
    if (!m_elements.Contains(elementId)) return false;
    const auto* e = m_model.element(elementId);
    const ElementTopology* topo = e ? elementTopology(e->type) : nullptr;
    if (!topo) return false;
    switch (topo->dimension)
    {
    case 1: // lien entre les nœuds d'extrémité (Line3 : le nœud milieu est un nœud du maillage, pas un sommet du lien)
        ids = { e->nodes[0], e->nodes[1] };
        break;
    case 2:
        for (int local : TSA::Mesh::faceOutline(*topo, topo->faces.front())) ids.push_back(e->nodes[std::size_t(local)]);
        break;
    default: // volume : tous les nœuds dans l'ordre de l'élément (les faces les indexent)
        ids = e->nodes;
        break;
    }
    return true;
}

bool MeshVSDataSource::GetGeom(const int ID, const bool IsElement, NCollection_Array1<double>& Coords, int& NbNodes,
                               MeshVS_EntityType& Type) const
{
    if (!IsElement)
    {
        if (!m_nodes.Contains(ID) || Coords.Length() < 3) return false;
        const gp_Pnt p = position(ID);
        Coords(Coords.Lower()) = p.X();
        Coords(Coords.Lower() + 1) = p.Y();
        Coords(Coords.Lower() + 2) = p.Z();
        NbNodes = 1;
        Type = MeshVS_ET_Node;
        return true;
    }
    std::vector<int> ids;
    if (!displayNodes(ID, ids) || Coords.Length() < int(ids.size()) * 3) return false;
    if (!GetGeomType(ID, true, Type)) return false;
    int k = Coords.Lower();
    for (int n : ids)
    {
        const gp_Pnt p = position(n);
        Coords(k++) = p.X();
        Coords(k++) = p.Y();
        Coords(k++) = p.Z();
    }
    NbNodes = int(ids.size());
    return true;
}

bool MeshVSDataSource::GetGeomType(const int ID, const bool IsElement, MeshVS_EntityType& Type) const
{
    if (!IsElement)
    {
        Type = MeshVS_ET_Node;
        return m_nodes.Contains(ID);
    }
    if (!m_elements.Contains(ID)) return false;
    const auto* e = m_model.element(ID);
    switch (TSA::Mesh::feTypeDimension(e->type))
    {
    case 1: Type = MeshVS_ET_Link; break;
    case 2: Type = MeshVS_ET_Face; break;
    default: Type = MeshVS_ET_Volume; break;
    }
    return true;
}

bool MeshVSDataSource::Get3DGeom(const int ID, int& NbNodes, occ::handle<NCollection_HArray1<NCollection_Sequence<int>>>& Data) const
{
    if (!m_elements.Contains(ID)) return false;
    const auto* e = m_model.element(ID);
    if (TSA::Mesh::feTypeDimension(e->type) != 3) return false;
    const auto& faces = m_model.volumeFaces(ID);
    if (faces.empty()) return false;
    // Indices locaux (base 0) dans les coordonnées de GetGeom (ordre des nœuds de l'élément).
    Data = new NCollection_HArray1<NCollection_Sequence<int>>(1, int(faces.size()));
    for (std::size_t f = 0; f < faces.size(); ++f)
        for (int local : faces[f]) Data->ChangeValue(int(f) + 1).Append(local);
    NbNodes = int(e->nodes.size());
    return true;
}

void* MeshVSDataSource::GetAddr(const int, const bool) const { return nullptr; }

bool MeshVSDataSource::GetNodesByElement(const int ID, NCollection_Array1<int>& NodeIDs, int& NbNodes) const
{
    std::vector<int> ids;
    if (!displayNodes(ID, ids) || NodeIDs.Length() < int(ids.size())) return false;
    int k = NodeIDs.Lower();
    for (int n : ids) NodeIDs(k++) = n;
    NbNodes = int(ids.size());
    return true;
}

} // namespace TSA::Viewer
