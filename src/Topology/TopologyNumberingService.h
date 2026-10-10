#pragma once

// Service central de topologie et de numérotation : prévisualisation (sans toucher au modèle),
// validation (doublons, conflits, paramètres incompatibles), application atomique (une entrée
// Annuler) et correspondance identifiant interne ↔ étiquette ↔ numéro transmis au solveur.

#include "NumberingStrategies.h"
#include "TopologySettings.h"

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Topology
{

struct PreviewEntry
{
    EntityFamily family = EntityFamily::Node;
    int id = 0;                    ///< identifiant interne (jamais modifié)
    std::string currentLabel;
    std::string newLabel;
    std::string gridRef;           ///< repère de grille, s'il existe
    double x = 0.0, y = 0.0, z = 0.0; ///< nœud, ou centre de l'élément
    int solverTag = -1;            ///< numéro transmis au solveur OpenSees (-1 : non transmis)
    bool preserved = false;        ///< étiquette personnalisée conservée
    bool changed() const { return newLabel != currentLabel; }
};

struct NumberingPreview
{
    std::vector<PreviewEntry> entries;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;   ///< non vide : application refusée
    int dimension = 0;                 ///< 0..3 (nuage de nœuds)
    bool valid() const { return errors.empty(); }
    int changedCount() const;
};

/// Erreurs de paramètres (avant tout calcul) : numéros, pas, largeur, format, préfixes, stratégies.
std::vector<std::string> validateSettings(const TopologySettings& settings);

/// Étiquette d'un numéro selon un format (jetons {p}, {n}, {g}, {l}).
std::string formatLabel(const LabelFormat& format, const std::string& prefix, int number, const std::string& gridRef, int layer);

/// L'étiquette a-t-elle été produite automatiquement (défaut historique ou format donné) ? Une
/// étiquette qui ne l'est pas est « personnalisée » et peut être conservée.
bool isAutomaticLabel(const std::string& label, const std::string& prefix, const LabelFormat& format, int id);

/// Calcule la nouvelle numérotation sans modifier le modèle.
NumberingPreview computePreview(const TSA::Model::Model& model, const TopologySettings& settings, const NumberingInput& input);

/// Applique une prévisualisation valide : étiquettes seulement, une entrée Annuler, vues notifiées.
/// Les résultats de calcul restent valides (les solveurs référencent les identifiants internes).
/// Retourne false (modèle inchangé) si la prévisualisation est invalide ou périmée.
bool applyPreview(TSA::Model::Model& model, const NumberingPreview& preview, const std::string& undoLabel, std::string* error = nullptr);

/// Correspondance complète pour l'export : famille, identifiant interne, étiquette, tag solveur.
struct SolverMappingRow
{
    EntityFamily family;
    int id;
    std::string label;
    int solverTag; ///< -1 : entité non transmise au solveur (dalle, voile, fondation)
};
std::vector<SolverMappingRow> solverMapping(const TSA::Model::Model& model);

} // namespace TSA::Topology
