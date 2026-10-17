#pragma once

// Fichier Blueprint .tsbp : JSON versionné (format « tsbp »).
//   { "format": "tsbp", "version": 2 | 3, "name", "description",
//     "nodes": [ { "id", "type", "v", "x", "y", "values": { "<entrée>": <valeur typée> } } ],
//     "links": [ { "from": id, "out": "<sortie>", "to": id, "in": "<entrée>" } ],
//     "comments": [ { "id", "x", "y", "w", "h", "text", "color" } ],
//     "subgraphs": [ <sous-graphe> ] }                                         (version 3, s'il y a des sous-graphes)
// Sous-graphe : { "id", "name", "description", "category", "documentation", "version",
//                 "inputs": [ <broche> ], "outputs": [ <broche> ], "graph": { "nodes", "links", "comments" } }
// Broche : { "name", "label", "type": "bool" | "integer" | "real" | "text" | "point" | "ids" | "object" | "any",
//            "object": "<type d'objet>", "default": <valeur typée>, "required": true }
// Valeur typée : { "bool" | "int" | "real" | "text" | "point" | "ids" : … } — le type est conservé
// sans dépendre de la bibliothèque de nœuds (un nœud inconnu est chargé puis signalé à la validation).
// « v » : version du type de nœud à l'enregistrement (NodeDefinition::version, ou version de la définition pour une
// instance de sous-graphe) ; un nœud d'une version plus récente que la bibliothèque est signalé à la validation. Les
// objets (matrices, modèles…) ne sont jamais enregistrés : ce sont des résultats d'exécution, recalculés à chaque
// exécution. Version 1 (sans « v » ni commentaires) : lue telle quelle, nœuds en version 1 ; version 2 : sans
// sous-graphe (un graphe sans sous-graphe s'enregistre toujours en version 2, lisible par les versions antérieures).
// Fichier de sous-graphe .tsbps (format « tsbp-subgraph », version 1) : { "subgraphs": [ … ] }, dépendances d'abord,
// la définition principale en dernier.
// Les positions graphiques ne sont qu'une partie du fichier : la sémantique est portée par les types,
// les valeurs et les liens.

#include "BlueprintGraph.h"

#include <QString>

#include <set>

namespace TSA::Blueprint
{

inline constexpr int kFileVersion = 3;            ///< version la plus récente lue et écrite
inline constexpr int kSubgraphFileVersion = 1;

QByteArray toJson(const Graph& graph);
/// Faux (et message) si le document n'est pas un Blueprint lisible ou d'une version future.
bool fromJson(const QByteArray& json, Graph& graph, QString* error = nullptr);

bool saveFile(const Graph& graph, const QString& path, QString* error = nullptr);
bool loadFile(const QString& path, Graph& graph, QString* error = nullptr);

/// Fragment de graphe (copier / coller) : nœuds choisis, liens entre ces nœuds, commentaires choisis, et définitions
/// des sous-graphes utilisés par ces nœuds.
QByteArray fragmentToJson(const Graph& graph, const std::set<int>& nodes, const std::set<int>& comments = {});
/// Insère un fragment décalé de (dx, dy) avec de nouveaux identifiants. Faux si le fragment est illisible. Les
/// définitions de sous-graphes du fragment absentes du graphe sont ajoutées ; une définition présente est remplacée
/// si celle du fragment est d'une version plus récente (décrit dans *notes), conservée sinon.
bool pasteFragment(Graph& graph, const QByteArray& json, double dx, double dy, std::vector<int>* newNodes = nullptr,
                   std::vector<int>* newComments = nullptr, QString* error = nullptr, std::vector<std::string>* notes = nullptr);

/// Enregistre la définition `id` du graphe et les sous-graphes qu'elle utilise (fichier .tsbps).
bool saveSubgraphFile(const Graph& owner, const std::string& id, const QString& path, QString* error = nullptr);
/// Définitions d'un fichier .tsbps (dépendances d'abord, définition principale en dernier). Faux si illisible.
bool loadSubgraphFile(const QString& path, std::vector<SubgraphDefinition>& definitions, QString* error = nullptr);

} // namespace TSA::Blueprint
