#pragma once

// Conversion AnalysisModel (CalculationSnapshot 3D) ↔ moteur EF 3D Fem3D (dépôt voisin 3DDesplacement).
//
// La conversion réutilise les briques du chemin OpenSees, pour que les deux moteurs calculent EXACTEMENT le même
// modèle et que leurs résultats soient présentés de la même façon :
//  - OpenSeesModelMap : barres, vecteurs v_xz (même convention geomTransf), ressorts ;
//  - OpenSeesAnalysisBuilder::loadPatterns / beamElementLoads : cas, combinaison, poids propre, charges sur barre
//    résolues dans les axes locaux ;
//  - OpenSeesResultsReader : stations (convention de ResultsModel), équilibre global, blocages automatiques.
// Périmètre de Fem3D (docs/ANALYSIS_ENGINES.md, 3DDesplacement/docs/validation/QUALIFICATION.md) : poutres et poteaux
// d'Euler–Bernoulli, rotules My / Mz, appuis et ressorts, charges nodales, charges réparties uniformes ou linéaires
// sur TOUTE la longueur (poids propre compris), statique linéaire. Tout le reste est signalé, jamais approché.

#include "../../Engine/AnalysisEngine.h"
#include "../../OpenSeesAnalysisBuilder.h"
#include "../../OpenSeesModelMap.h"

#include "Fem3D/Model/StructuralModel3D.h"
#include "Fem3D/Results/AnalysisResults.h"

#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis::Fem3DIntegration
{

/// Modèle Fem3D construit depuis la portée, avec ce qu'il faut pour remapper ses résultats.
struct Conversion
{
    AnalysisParameters params;                                   ///< unités kN, m ; cas / combinaison du contexte
    OpenSeesModelMap map;                                        ///< barres (tag = id d'élément Fem3D), ressorts
    Fem3D::StructuralModel3D model;
    std::map<int, std::vector<BeamElementLoad>> memberLoads;     ///< charges des barres, facteurs inclus (stations)
    std::vector<std::string> notes;                              ///< informations de conversion (sans perte)
    /// Éléments hors du périmètre de Fem3D (charge ponctuelle, partielle, couple réparti…) : le calcul Fem3D est
    /// impossible ; un moteur de secours explicitement choisi peut le faire (Fem3DEngine).
    std::vector<std::string> outOfScope;
    /// Données refusées par Fem3D ET sans secours possible (modèle incohérent pour tout moteur).
    std::vector<std::string> errors;

    bool computable() const { return outOfScope.empty() && errors.empty(); }
};

/// Paramètres de calcul déduits du contexte (statique linéaire, kN, cas ou combinaison).
AnalysisParameters parametersFromContext(const AnalysisContext& context);

/// Construit le modèle Fem3D. Ne lève pas d'exception : les pertes sont listées dans outOfScope / errors.
Conversion convert(const AnalysisContext& context, const AnalysisModel& model);

/// Remappe les résultats Fem3D vers les objets TSA (déplacements, réactions avec ressorts, barres, équilibre).
ResultsModel mapResults(const Conversion& conversion, const AnalysisModel& model, const Fem3D::LoadCaseResults& results);

} // namespace TSA::Analysis::Fem3DIntegration
