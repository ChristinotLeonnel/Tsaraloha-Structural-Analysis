#pragma once

// Source de données MeshVS (OCCT) lue dans le modèle d'affichage TSA (docs/MESH_VISUALIZATION.md §3).
// Les identifiants MeshVS sont les identifiants métier FeNode::id / FeElement::id (jamais des indices) ;
// éléments 1D → liens (MeshVS_ET_Link, nœuds d'extrémité), 2D → faces (contour avec nœuds milieux),
// 3D → volumes (faces affichées par MeshDisplayModel : extérieures, ou toutes en inspection).
// Les positions peuvent être déformées (déplacements × facteur) sans modifier le maillage.

#include "../Mesh/MeshDisplayModel.h"

#include <MeshVS_DataSource.hxx>
#include <TColStd_PackedMapOfInteger.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <unordered_map>

namespace TSA::Viewer
{

class MeshVSDataSource : public MeshVS_DataSource
{
public:
    /// @param model doit survivre à la source (possédé par MeshVisual)
    explicit MeshVSDataSource(const TSA::Mesh::MeshDisplayModel& model);

    /// Déplacements nodaux (identifiant → vecteur global) ; nullptr : géométrie initiale.
    void setDisplacements(const std::unordered_map<int, std::array<double, 3>>* displacements, double scale);
    /// Position affichée d'un nœud (déformée le cas échéant).
    gp_Pnt position(int nodeId) const;
    /// Identifiants des nœuds dans l'ordre des coordonnées de GetGeom (contour pour une face).
    bool displayNodes(int elementId, std::vector<int>& ids) const;

    bool GetGeom(const int ID, const bool IsElement, NCollection_Array1<double>& Coords, int& NbNodes,
                 MeshVS_EntityType& Type) const override;
    bool GetGeomType(const int ID, const bool IsElement, MeshVS_EntityType& Type) const override;
    bool Get3DGeom(const int ID, int& NbNodes, occ::handle<NCollection_HArray1<NCollection_Sequence<int>>>& Data) const override;
    void* GetAddr(const int ID, const bool IsElement) const override;
    bool GetNodesByElement(const int ID, NCollection_Array1<int>& NodeIDs, int& NbNodes) const override;
    const TColStd_PackedMapOfInteger& GetAllNodes() const override { return m_nodes; }
    const TColStd_PackedMapOfInteger& GetAllElements() const override { return m_elements; }

    DEFINE_STANDARD_RTTI_INLINE(MeshVSDataSource, MeshVS_DataSource)

private:
    const TSA::Mesh::MeshDisplayModel& m_model;
    TColStd_PackedMapOfInteger m_nodes, m_elements;
    const std::unordered_map<int, std::array<double, 3>>* m_displacements = nullptr;
    double m_scale = 0.0;
};

} // namespace TSA::Viewer
