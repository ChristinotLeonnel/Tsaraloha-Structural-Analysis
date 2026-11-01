#pragma once

// Convention du repère local des barres verticales : avant le format 3, y = +X à rotation nulle et « vertical » à 2,56° près ;
// depuis, y = +Y et vertical à sin 1e-3 près, comme Robot Structural Analysis. Un ancien projet garde son orientation physique :
// l'angle des poutres et poteaux concernés est recalculé ; les barres sans angle (treillis, câbles) voient leurs charges locales
// réexprimées. Résultats de calcul inchangés.

namespace TSA::Model
{
class Model;

/// Convertit un modèle lu d'un fichier d'ancienne convention. Retourne le nombre de barres dont l'angle ou les charges ont changé.
int migrateLegacyBarFrames(Model& model);

} // namespace TSA::Model
