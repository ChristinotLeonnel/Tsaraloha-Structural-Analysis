#pragma once

// Fichier Blueprint .tsbp : JSON versionné (format « tsbp », version 1).
//   { "format": "tsbp", "version": 1, "name", "description",
//     "nodes": [ { "id", "type", "x", "y", "values": { "<entrée>": <valeur typée> } } ],
//     "links": [ { "from": id, "out": "<sortie>", "to": id, "in": "<entrée>" } ] }
// Valeur typée : { "bool" | "int" | "real" | "text" | "point" | "ids" : … } — le type est conservé
// sans dépendre de la bibliothèque de nœuds (un nœud inconnu est chargé puis signalé à la validation).
// Les positions graphiques ne sont qu'une partie du fichier : la sémantique est portée par les types,
// les valeurs et les liens.

#include "BlueprintGraph.h"

#include <QString>

namespace TSA::Blueprint
{

inline constexpr int kFileVersion = 1;

QByteArray toJson(const Graph& graph);
/// Faux (et message) si le document n'est pas un Blueprint lisible ou d'une version future.
bool fromJson(const QByteArray& json, Graph& graph, QString* error = nullptr);

bool saveFile(const Graph& graph, const QString& path, QString* error = nullptr);
bool loadFile(const QString& path, Graph& graph, QString* error = nullptr);

} // namespace TSA::Blueprint
