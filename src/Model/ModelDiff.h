#pragma once

#include "Model.h"
#include <vector>
#include <set>
#include <map>
#include <string>

namespace TSA::Model
{

/**
 * @brief Différentiel pour une catégorie d'éléments (créés, modifiés, supprimés).
 */
struct ElementDiff
{
    std::vector<int> created;   ///< Identifiants des éléments nouvellement créés
    std::vector<int> modified;  ///< Identifiants des éléments existants dont la géométrie ou propriétés ont changé
    std::vector<int> deleted;   ///< Identifiants des éléments supprimés

    bool isEmpty() const
    {
        return created.empty() && modified.empty() && deleted.empty();
    }

    size_t totalChanges() const
    {
        return created.size() + modified.size() + deleted.size();
    }
};

/**
 * @brief Rapport complet de différence entre deux états du modèle structural.
 * Utilisé par le synchroniseur de vue pour mettre à jour de manière incrémentale
 * le viewport OCCT, l'arbre du modèle et la sélection sans jamais reconstruire
 * les objets non modifiés.
 */
struct ModelDiff
{
    ElementDiff nodes;
    ElementDiff beams;
    ElementDiff columns;
    ElementDiff slabs;
    ElementDiff walls;
    ElementDiff foundations;
    ElementDiff trussMembers;
    ElementDiff cables;

    // Listes d'identifiants à accès direct pour les vues et les tests
    std::vector<int> createdNodeIds;
    std::vector<int> modifiedNodeIds;
    std::vector<int> deletedNodeIds;

    std::vector<int> createdBeamIds;
    std::vector<int> modifiedBeamIds;
    std::vector<int> deletedBeamIds;

    std::vector<int> createdColumnIds;
    std::vector<int> modifiedColumnIds;
    std::vector<int> deletedColumnIds;

    std::vector<int> createdSlabIds;
    std::vector<int> modifiedSlabIds;
    std::vector<int> deletedSlabIds;

    std::vector<int> createdWallIds;
    std::vector<int> modifiedWallIds;
    std::vector<int> deletedWallIds;

    std::vector<int> createdFoundationIds;
    std::vector<int> modifiedFoundationIds;
    std::vector<int> deletedFoundationIds;

    std::vector<int> createdTrussMemberIds;
    std::vector<int> modifiedTrussMemberIds;
    std::vector<int> deletedTrussMemberIds;

    std::vector<int> createdCableIds;
    std::vector<int> modifiedCableIds;
    std::vector<int> deletedCableIds;

    bool isEmpty() const
    {
        return nodes.isEmpty() && beams.isEmpty() && columns.isEmpty() &&
               slabs.isEmpty() && walls.isEmpty() && foundations.isEmpty() &&
               trussMembers.isEmpty() && cables.isEmpty();
    }

    size_t totalChanges() const
    {
        return nodes.totalChanges() + beams.totalChanges() + columns.totalChanges() +
               slabs.totalChanges() + walls.totalChanges() + foundations.totalChanges() +
               trussMembers.totalChanges() + cables.totalChanges();
    }

    /**
     * @brief Calcule le différentiel exact entre l'état initial (before) et le nouvel état (after).
     * Règle de dépendance géométrique :
     * Si un nœud est créé, supprimé ou déplacé, tout élément connecté (poutre, poteau,
     * voile, dalle, semelle, treillis) est automatiquement classé MODIFIED afin que son solide 3D
     * soit mis à jour, tandis que tous les éléments non connectés restent UNCHANGED.
     */
    static ModelDiff compute(const Model::ModelStateSnapshot& before, const Model::ModelStateSnapshot& after);
};

} // namespace TSA::Model
