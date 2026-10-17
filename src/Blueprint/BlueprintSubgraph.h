#pragma once

// Blueprint — sous-graphes réutilisables : fonctions écrites en blocs, aux entrées et sorties typées.
//
// Une définition (SubgraphDefinition, BlueprintGraph.h) appartient au graphe qui l'utilise et s'enregistre avec lui ;
// elle se partage entre graphes par copier-coller (le fragment emporte les définitions utilisées) ou par fichier
// (.tsbps, BlueprintFile.h). Son corps va du nœud d'entrée « sub.<id>.in » (point de départ ; ses sorties sont les
// entrées du sous-graphe, valeurs par défaut quand le corps est exécuté seul pour essai) au nœud de sortie
// « sub.<id>.out » (fin ; ses entrées, obligatoires, sont les sorties du sous-graphe). Une instance « sub.<id> » exécute
// le corps dans une exécution imbriquée (Runner), ses entrées passées en paramètres :
//   - instance PURE (évaluée à la demande, réutilisable si tout le corps l'est) quand le corps ne contient que des nœuds
//     purs ; instance d'ACTION (broches d'exécution « exec » et « then ») sinon ;
//   - le journal du corps est repris, préfixé du nom du sous-graphe ; une erreur du corps fait échouer l'instance avec
//     le message du corps ; les points d'arrêt du débogueur ne s'appliquent pas à l'intérieur d'un corps ;
//   - les appels récursifs (directs ou indirects) sont refusés à la validation ; l'imbrication est bornée à l'exécution.
// Versions : une définition porte une version, incrémentée à chaque modification enregistrée (defineSubgraph) ; une
// instance enregistrée avec une version plus récente que la définition présente est signalée ; une définition absente
// est une erreur explicite (« sous-graphe introuvable »).

#include "BlueprintRuntime.h"

#include <set>

namespace TSA::Blueprint
{

inline constexpr int kMaxSubgraphDepth = 32;

std::string subgraphInstanceType(const std::string& id);   ///< « sub.<id> »
std::string subgraphInputType(const std::string& id);      ///< « sub.<id>.in »
std::string subgraphOutputType(const std::string& id);     ///< « sub.<id>.out »
/// Identifiant du sous-graphe d'un type d'INSTANCE (« sub.<id> » → id) ; vide pour tout autre type.
std::string subgraphOfType(const std::string& type);
/// Lettres ASCII, chiffres, « _ », « - » ; 1 à 64 caractères ; commence par une lettre.
bool isValidSubgraphId(const std::string& id);
/// Nom de broche : lettres ASCII, chiffres, « _ » ; commence par une lettre ; ni « exec » ni « then ».
bool isValidPinName(const std::string& name);

/// Identifiant stable d'un type de valeur (fichiers) : bool, integer, real, text, point, ids, object.
const char* valueTypeId(ValueType type);
bool valueTypeFromId(const std::string& id, ValueType& out);

/// Corps minimal d'une définition : nœud d'entrée relié au nœud de sortie.
std::shared_ptr<const Graph> makeSubgraphBody(const std::string& id);

/// Contrôles des définitions d'un graphe (bibliothèque augmentée de leurs types) : identifiants et broches, nœuds
/// d'entrée et de sortie uniques et propres, « Début » interdit, fin atteinte depuis l'entrée par les liens
/// d'exécution, corps valide (types, liens, entrées requises, cycles), appels récursifs.
std::vector<std::string> validateSubgraphs(const NodeLibrary& library, const std::vector<SubgraphDefinition>& set);
/// Contrôles d'un corps pendant son édition (éditeur) : `body` remplace le corps enregistré de la définition `id` de
/// `definitions` ; nœuds et liens du corps (problèmes rattachés à leurs nœuds), structure (entrée et sortie uniques, fin
/// atteinte, « Début » interdit) et appels récursifs (problèmes du graphe, nœud 0). Les autres définitions ne sont pas
/// recontrôlées.
std::vector<Issue> validateBody(const NodeLibrary& base, const Graph& body, const std::string& id, const std::vector<SubgraphDefinition>& definitions);
/// Retire de g les liens des nœuds `nodes` devenus invalides (broche supprimée ou de type modifié) et les valeurs saisies
/// d'entrées qui n'existent plus ; une ligne par retrait (« #12 « Titre » : lien « x » retiré … »).
std::vector<std::string> pruneStaleLinks(const NodeLibrary& library, Graph& g, const std::vector<int>& nodes);

/// Instances d'un sous-graphe dans g.
std::vector<int> subgraphInstances(const Graph& g, const std::string& id);
/// Sous-graphes utilisés par des nœuds de g, directement ou par d'autres sous-graphes (ordre : dépendances d'abord).
std::vector<std::string> usedSubgraphs(const Graph& g, const std::set<int>& nodes);

struct EncapsulationResult
{
    bool ok = false;
    std::string message;
    int instance = 0;
    std::string id;
};

/// Remplace les nœuds `nodes` de g par une instance d'un nouveau sous-graphe `id` : les liens de données qui entrent
/// dans la sélection deviennent des entrées (une par source, type de la source), ceux qui en sortent des sorties (une
/// par sortie intérieure) ; au plus un point d'entrée et une sortie d'exécution (sinon refus motivé) ; « Début » et
/// nœuds d'entrée / sortie de sous-graphe refusés ; sélection d'action sans lien d'exécution entrant refusée.
EncapsulationResult encapsulate(const NodeLibrary& library, Graph& g, const std::set<int>& nodes, const std::string& id, const std::string& name);

/// Ajoute la définition ou remplace celle de même identifiant (version = précédente + 1 si le contenu change ; inchangée
/// sinon), puis met à jour les instances : liens vers des broches supprimées ou devenues incompatibles retirés, valeurs
/// saisies d'entrées supprimées effacées (décrits dans *updates). Faux (*error) si la définition est invalide : le graphe
/// n'est alors pas modifié.
bool defineSubgraph(const NodeLibrary& library, Graph& g, SubgraphDefinition definition, std::vector<std::string>* updates = nullptr,
                    std::string* error = nullptr);
/// Supprime une définition ; refusé si une instance l'utilise encore (dans le graphe ou un autre corps).
bool removeSubgraph(Graph& g, const std::string& id, std::string* error = nullptr);

} // namespace TSA::Blueprint
