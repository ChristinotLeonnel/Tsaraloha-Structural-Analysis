#pragma once

// Outils de cotation (catégorie Annotate) : saisie dans la vue 3D par le cadre des outils de
// modification — clics accrochés (nœuds, extrémités, grilles, plan de travail), aperçu, valeur tapée
// (décalage de la ligne de cote), Entrée (fin d'une chaîne), Échap. apply() ajoute la cotation par
// Annotation::addDimension (une entrée Annuler, résultats de calcul conservés).

#include "ModelingTool.h"

namespace TSA::Interaction
{

void registerDimensionTools(ModelingToolRegistry& registry);

} // namespace TSA::Interaction
