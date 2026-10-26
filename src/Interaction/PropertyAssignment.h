#pragma once

// Affectation de propriétés aux éléments existants (section…), sans vue : utilisée par le ruban, testée sans fenêtre
// (suite « drawing »). Toute affectation forme UNE entrée d'historique (annulable), aucune si rien ne change.

#include "../Model/Section.h"
#include "../Model/Slab.h"

#include <set>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Interaction
{

/// Affecte la section aux poutres et poteaux désignés (ids inexistants ignorés) en une seule transaction nommée
/// `action`. Retourne le nombre d'éléments modifiés.
int assignSection(TSA::Model::Model& model, const std::set<int>& beamIds, const std::set<int>& columnIds, const TSA::Model::Section& section,
                  const std::string& action);

/// Remplace les ouvertures de la dalle (ou du voile) en une transaction nommée `action`, puis prévient les vues une fois
/// (notifySlabModified / notifyWallModified : forme 3D, arbre, panneau Propriétés, validité des résultats).
/// Retourne faux si l'élément n'existe pas (aucune entrée d'historique).
bool setSlabOpenings(TSA::Model::Model& model, int slabId, const std::vector<TSA::Model::SlabOpening>& openings, const std::string& action);
bool setWallOpenings(TSA::Model::Model& model, int wallId, const std::vector<TSA::Model::SurfaceOpening>& openings, const std::string& action);

} // namespace TSA::Interaction
