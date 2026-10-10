#pragma once

// Gestion des cotations dans le modèle : ajout, modification, suppression, réassociation, style.
// Chaque opération crée une entrée Annuler sans changer la révision du modèle (annotations : les
// résultats de calcul restent valides) et notifie les vues (IModelObserver::onDimensionsChanged).

#include "Dimension.h"

#include <set>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Annotation
{

/// Ajoute la cotation (identifiant attribué) ; 0 et *error si elle est invalide (points, géométrie).
int addDimension(TSA::Model::Model& model, Dimension dim, std::string* error = nullptr);

/// Remplace une cotation existante (position, texte, couleur, ancrages) ; false si absente ou invalide.
bool updateDimension(TSA::Model::Model& model, const Dimension& dim, std::string* error = nullptr);

/// Supprime les cotations ; retourne le nombre supprimé.
int removeDimensions(TSA::Model::Model& model, const std::set<int>& ids);

/// Supprime les cotations dont une référence est invalide (nœud supprimé).
int removeInvalidDimensions(TSA::Model::Model& model);

/// Réassocie l'ancrage `anchorIndex` au nœud `nodeId` (référence invalide corrigée).
bool reassociateAnchor(TSA::Model::Model& model, int dimensionId, int anchorIndex, int nodeId, std::string* error = nullptr);

/// Style du projet (unités, précision, tailles, couleurs, visibilité).
void setDimensionStyle(TSA::Model::Model& model, const DimensionStyle& style);

/// Cotations qui référencent ce nœud (mise à jour ciblée des vues).
std::vector<int> dimensionsReferencingNode(const TSA::Model::Model& model, int nodeId);

} // namespace TSA::Annotation
