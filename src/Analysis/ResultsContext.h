#pragma once

// Contexte structuré d'un nœud ou d'un élément pour le diagnostic et le futur Co-Engineering :
// chaîne Charge → Structure → Rigidité → Déplacement → Efforts, avec unités et provenance.
// Lecture seule : ne modifie ni le modèle ni les résultats.

#include "ResultsModel.h"

#include <QJsonObject>

namespace TSA::Model { class Model; }

namespace TSA::Analysis
{

namespace ResultsContext
{
/// Nœud : coordonnées, appui, déplacement, réaction, éléments connectés (efforts d'extrémité au
/// nœud, forces locales/globales brutes si disponibles), termes diagonaux de K_global et blocs
/// nodaux des rigidités élémentaires (mode Advanced), validité et cas de charge.
QJsonObject nodeContext(const TSA::Model::Model& model, const ResultsModel& results, int nodeId);

/// Élément : nœuds, longueur, efforts, forces brutes, matrices et leurs métadonnées.
QJsonObject elementContext(const TSA::Model::Model& model, const ResultsModel& results, const ElementKey& key);
} // namespace ResultsContext

} // namespace TSA::Analysis
