#pragma once

// Types de produits partagés (BimModel::types) : création depuis le modèle (migration d'un projet sans types),
// application d'un type à toutes ses occurrences, reprise d'une occurrence modifiée dans son type, écarts.
// Les fonctions modifient le modèle sans ouvrir de transaction : l'appelant les encadre (EditTransaction), une
// opération = une entrée Annuler. Référence : docs/BIM_ARCHITECTURE.md, « Types de produits ».

#include "BimModel.h"

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::BIM
{

/// Crée les types manquants : un type par (catégorie, section, matériau) pour les éléments linéaires, par
/// (catégorie, épaisseur, matériau) pour les dalles et voiles, par (catégorie, matériau) pour les fondations. Les
/// produits sans type y sont rattachés ; les produits typés, importés sans élément de calcul et câbles sont laissés.
/// Retourne le nombre de types créés.
int createTypesFromModel(TSA::Model::Model& model);

/// Applique le type à toutes ses occurrences (section / épaisseur et matériau de chaque élément de calcul).
/// Retourne le nombre d'éléments de calcul modifiés (0 si tous étaient conformes).
int applyType(TSA::Model::Model& model, int typeId);

/// Reprend dans son type la définition d'une occurrence (premier élément de calcul du produit), puis l'applique aux
/// autres occurrences. Retourne le nombre d'éléments modifiés, -1 si le produit n'a pas de type.
int propagateFromOccurrence(TSA::Model::Model& model, int productId);

/// Types importés (seuls nom, catégorie et GlobalId sont lus dans le fichier) : section, matériau et épaisseur repris
/// de leur première occurrence. Retourne le nombre de types complétés.
int completeTypeDefinitions(TSA::Model::Model& model, const std::vector<int>& typeIds);

/// Écart entre un produit et son type (« section R30x60 au lieu de R30x50 ») ; vide s'il est conforme ou sans type.
std::string typeDeviation(const TSA::Model::Model& model, const PhysicalElement& product);

} // namespace TSA::BIM
