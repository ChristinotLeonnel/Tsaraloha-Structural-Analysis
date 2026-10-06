#pragma once

// Nettoyage topologique du modèle avant calcul :
//  1. fusion des nœuds confondus (Model::mergeCoincidentNodes) ;
//  2. suppression des barres en double (même famille, mêmes deux nœuds) — leurs charges sont
//     reportées sur la barre conservée ;
//  3. connexion des nœuds posés sur une barre sans en être une extrémité (jonction en T non
//     modélisée) : la barre est divisée sur ce nœud (Model::splitBarAt) ;
//  4. (option) nœud commun aux croisements de barres ;
//  5. suppression des nœuds parasites : aucun élément, aucun appui, aucune charge nodale.
// Barres traitées : poutres, poteaux, treillis (les câbles, courbes, sont exclus).
// analyze() calcule le bilan sur une copie, sans toucher au modèle ; clean() l'applique.
// L'appelant encadre clean() d'une transaction (une entrée Annuler).

#include "SelectionQuery.h"

#include <string>
#include <vector>

namespace TSA::Model
{

class Model;

struct CleanupOptions
{
    double tolerance = 1e-3;              ///< m : nœuds confondus, nœud sur barre, croisement
    bool mergeCoincidentNodes = true;
    bool removeDuplicateBars = true;
    bool connectNodesOnBars = true;
    bool splitCrossingBars = false;       ///< désactivé par défaut : un contreventement en X ne se croise pas forcément
    bool removeOrphanNodes = true;
};

struct CleanupReport
{
    int mergedNodes = 0;                  ///< nœuds supprimés par fusion
    int duplicateBarsRemoved = 0;
    int nodesConnectedOnBars = 0;         ///< divisions de barre sur un nœud existant
    int crossingNodesCreated = 0;
    int orphanNodesRemoved = 0;
    std::vector<std::string> details;     ///< une ligne par opération (ids TSA)
    std::vector<std::string> refused;     ///< opérations non faites et pourquoi
    std::vector<std::string> warnings;    ///< points à vérifier, non corrigés automatiquement

    int total() const
    {
        return mergedNodes + duplicateBarsRemoved + nodesConnectedOnBars + crossingNodesCreated + orphanNodesRemoved;
    }
    bool changed() const { return total() > 0; }
    std::string summary() const;
};

namespace ModelCleanup
{
/// Bilan du nettoyage, calculé sur une copie du modèle (le modèle n'est pas modifié).
CleanupReport analyze(const Model& model, const CleanupOptions& options = {});
/// Applique le nettoyage. Ne crée pas d'entrée Annuler (voir EditTransaction).
CleanupReport clean(Model& model, const CleanupOptions& options = {});

/// Relie deux barres qui se croisent (ou dont l'une aboutit sur l'autre) par un nœud commun,
/// en divisant la ou les barres traversées. Retourne le nœud commun, 0 si elles ne se
/// croisent pas (à tol près) ou si une division est refusée (charges non redistribuables).
/// *created reçoit les barres créées par division.
int connectCrossingBars(Model& model, ElementKind kindA, int idA, ElementKind kindB, int idB, double tol,
                        std::vector<std::pair<ElementKind, int>>* created = nullptr);
} // namespace ModelCleanup

} // namespace TSA::Model
