#pragma once

// Fichier Blueprint .tsbp : JSON versionné (format « tsbp »).
//   { "format": "tsbp", "version": 2, "name", "description",
//     "nodes": [ { "id", "type", "v", "x", "y", "values": { "<entrée>": <valeur typée> } } ],
//     "links": [ { "from": id, "out": "<sortie>", "to": id, "in": "<entrée>" } ],
//     "comments": [ { "id", "x", "y", "w", "h", "text", "color" } ] }
// Valeur typée : { "bool" | "int" | "real" | "text" | "point" | "ids" : … } — le type est conservé
// sans dépendre de la bibliothèque de nœuds (un nœud inconnu est chargé puis signalé à la validation).
// « v » : version du type de nœud à l'enregistrement (NodeDefinition::version) ; un nœud d'une version plus récente
// que la bibliothèque est signalé à la validation. Les objets (matrices, modèles…) ne sont jamais enregistrés : ce sont
// des résultats d'exécution, recalculés à chaque exécution.
// Version 1 (sans « v » ni commentaires) : lue telle quelle, nœuds en version 1.
// Les positions graphiques ne sont qu'une partie du fichier : la sémantique est portée par les types,
// les valeurs et les liens.

#include "BlueprintGraph.h"

#include <QString>

#include <set>

namespace TSA::Blueprint
{

inline constexpr int kFileVersion = 2;

QByteArray toJson(const Graph& graph);
/// Faux (et message) si le document n'est pas un Blueprint lisible ou d'une version future.
bool fromJson(const QByteArray& json, Graph& graph, QString* error = nullptr);

bool saveFile(const Graph& graph, const QString& path, QString* error = nullptr);
bool loadFile(const QString& path, Graph& graph, QString* error = nullptr);

/// Fragment de graphe (copier / coller) : nœuds choisis, liens entre ces nœuds, commentaires choisis.
QByteArray fragmentToJson(const Graph& graph, const std::set<int>& nodes, const std::set<int>& comments = {});
/// Insère un fragment décalé de (dx, dy) avec de nouveaux identifiants. Faux si le fragment est illisible.
bool pasteFragment(Graph& graph, const QByteArray& json, double dx, double dy, std::vector<int>* newNodes = nullptr,
                   std::vector<int>* newComments = nullptr, QString* error = nullptr);

} // namespace TSA::Blueprint
