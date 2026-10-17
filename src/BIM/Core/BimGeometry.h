#pragma once

// Géométrie physique d'un produit BIM importé (docs/IFC_MAPPING.md) : maillage triangulé en coordonnées du
// projet (m), produit par le noyau géométrique IFC à l'import. Elle représente l'objet physique tel qu'il est
// décrit dans le fichier d'origine — y compris quand aucun élément de calcul ne lui correspond — et sert à
// l'affichage, à la préparation du modèle de calcul et à la réexportation.
//
// Les maillages sont immuables et partagés (std::shared_ptr<const …>) : les instantanés d'Annuler copient le
// modèle BIM sans dupliquer les géométries. Persistance : chunk binaire BGEO du fichier .tsa (format ≥ 1.11).

#include <QByteArray>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace TSA::BIM
{

struct TriangleMesh
{
    /// Origine locale : les positions sont relatives à elle, en simple précision. Les coordonnées
    /// géoréférencées (centaines de kilomètres) gardent ainsi une précision submillimétrique.
    std::array<double, 3> origin { 0.0, 0.0, 0.0 };
    std::vector<float> positions;          ///< x, y, z relatifs à origin (m)
    std::vector<std::uint32_t> indices;    ///< 3 indices par triangle
    std::array<float, 4> color { 0.75f, 0.75f, 0.75f, 1.0f };   ///< style de surface IFC (RGBA), gris par défaut
    std::string representation;            ///< représentation d'origine (« SweptSolid », « Tessellation », « Brep »…)

    std::size_t vertexCount() const { return positions.size() / 3; }
    std::size_t triangleCount() const { return indices.size() / 3; }
    /// Indices dans les bornes, au moins un triangle, positions finies.
    bool valid() const;
    /// Boîte englobante monde : xmin, ymin, zmin, xmax, ymax, zmax.
    std::array<double, 6> bounds() const;
    /// Position monde du sommet i.
    std::array<double, 3> vertex(std::size_t i) const;
};

using MeshPtr = std::shared_ptr<const TriangleMesh>;

/// Sérialisation binaire versionnée des géométries (id de produit → maillage). Lecture tolérante :
/// une entrée illisible est ignorée, jamais le reste du projet.
QByteArray serializeGeometries(const std::map<int, MeshPtr>& meshes);
bool deserializeGeometries(const QByteArray& data, std::map<int, MeshPtr>& out, std::string* error = nullptr);

} // namespace TSA::BIM
