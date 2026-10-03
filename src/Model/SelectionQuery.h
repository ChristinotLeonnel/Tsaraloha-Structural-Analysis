#pragma once

#include <cstddef>
#include <set>

namespace TSA::Coordinate
{
class WorkPlane;
}

namespace TSA::Model
{

class Model;

/// Ensemble d'identifiants d'éléments structuraux, par type (sélection, requêtes).
struct ElementSet
{
    std::set<int> nodes;
    std::set<int> beams;
    std::set<int> columns;
    std::set<int> slabs;
    std::set<int> walls;
    std::set<int> foundations;
    std::set<int> trussMembers;
    std::set<int> cables;

    std::size_t size() const noexcept
    {
        return nodes.size() + beams.size() + columns.size() + slabs.size() + walls.size() +
               foundations.size() + trussMembers.size() + cables.size();
    }
    bool empty() const noexcept { return size() == 0; }
};

enum class ElementKind
{
    Node,
    Beam,
    Column,
    Slab,
    Wall,
    Foundation,
    TrussMember,
    Cable
};

/**
 * @brief Requêtes de sélection sur le modèle (fonctions pures, sans dépendance UI/OCCT).
 *
 * Le résultat est appliqué par TSA::Viewer::SelectionManager::selectElements(), qui reste le
 * seul système de sélection.
 */
namespace SelectionQuery
{
ElementSet all(const Model& model);
ElementSet byKind(const Model& model, ElementKind kind);
/// Tous les éléments du modèle qui ne sont PAS dans current.
ElementSet invert(const Model& model, const ElementSet& current);
/// Éléments linéaires (poutres, poteaux, treillis) dont la section porte le même nom qu'un
/// élément linéaire de reference.
ElementSet sameSection(const Model& model, const ElementSet& reference);
/// Éléments (linéaires, surfaciques, fondations, câbles) de même matériau qu'un élément de reference.
ElementSet sameMaterial(const Model& model, const ElementSet& reference);
/// Nœuds à la cote z, éléments dont toutes les extrémités / tous les nœuds sont à cette cote.
ElementSet atElevation(const Model& model, double z, double tol);
/// Nœuds sur le plan et éléments entièrement contenus dans le plan (toutes les extrémités / tous
/// les nœuds sur le plan). Plus strict que l'isolation 2D, qui garde aussi les éléments traversants.
ElementSet onWorkPlane(const Model& model, const TSA::Coordinate::WorkPlane& plane, double tol);
} // namespace SelectionQuery

} // namespace TSA::Model
