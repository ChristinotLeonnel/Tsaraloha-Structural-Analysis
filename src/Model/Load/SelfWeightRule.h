#pragma once
// Poids propre : règle unique de TSA (BUG-156), appliquée par tous les moteurs (OpenSees, Fem3D, Custom2D), par l'équilibre
// recalculé et par la note de calcul. Avant : pour un cas seul la case du calcul décidait et le drapeau du cas était ignoré ;
// pour une combinaison le drapeau décidait et la case était ignorée ; le coefficient du cas n'était appliqué par aucun moteur.
//
//  1. La case « Inclure le poids propre » du calcul est l'interrupteur général : décochée, aucun poids propre, nulle part.
//  2. Cochée, le poids propre est porté par le(s) cas marqué(s) « poids propre » (G par défaut), avec leur coefficient
//     (LoadCase::selfWeightFactor) ; dans une combinaison, multiplié par le facteur du cas. Un cas calculé seul qui n'est pas
//     marqué n'a pas de poids propre (un cas d'exploitation calculé seul ne le reçoit plus).
//  3. Compatibilité : si AUCUN cas du modèle n'est marqué (projets antérieurs aux drapeaux), un calcul de cas applique le poids
//     propre avec le coefficient 1, comme avant ; une combinaison n'en reçoit pas, comme avant.
#include "LoadCase.h"

#include <map>
#include <vector>

namespace TSA::Model
{

/// Un cas au moins est-il marqué « poids propre » ?
inline bool anyCaseCarriesSelfWeight(const std::map<int, LoadCase>& cases)
{
    for (const auto& [id, lc] : cases)
        if (lc.isSelfWeightIncluded()) return true;
    return false;
}

/// Coefficient du poids propre porté par le cas caseId dans une COMBINAISON (hors facteur de combinaison) ; 0 : aucun.
inline double combinationCaseSelfWeightFactor(const std::map<int, LoadCase>& cases, bool includeSelfWeight, int caseId)
{
    if (!includeSelfWeight) return 0.0;
    const auto it = cases.find(caseId);
    return it != cases.end() && it->second.isSelfWeightIncluded() ? it->second.selfWeightFactor() : 0.0;
}

/// Coefficient du poids propre d'un calcul de CAS (un seul, ou plusieurs superposés ; caseIds vide = tous) ; 0 : aucun.
inline double loadCasesSelfWeightFactor(const std::map<int, LoadCase>& cases, bool includeSelfWeight, const std::vector<int>& caseIds)
{
    if (!includeSelfWeight) return 0.0;
    if (!anyCaseCarriesSelfWeight(cases)) return 1.0;   // règle 3 (compatibilité)
    double factor = 0.0;
    for (const auto& [id, lc] : cases)
    {
        bool selected = caseIds.empty();
        for (int c : caseIds) selected = selected || c == id;
        if (selected && lc.isSelfWeightIncluded()) factor += lc.selfWeightFactor();
    }
    return factor;
}

} // namespace TSA::Model
