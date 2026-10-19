#pragma once

#include "ResultsModel.h"
#include "CalculationSnapshot.h"
#include "OpenSeesAnalysisBuilder.h"
#include <string>

namespace TSA::Analysis
{

class OpenSeesModelMap;

/**
 * @brief Lecteur et désérialiseur des fichiers de résultats générés par OpenSees.
 * Reconstitue les déplacements nodaux, réactions aux appuis, efforts intérieurs aux barres,
 * et synthèses d'équilibre. L'ordre des colonnes de chaque fichier est celui
 * d'OpenSeesModelMap (le même que celui utilisé pour écrire les recorders) ; toute ligne dont
 * la taille ne correspond pas est rejetée (mapping incohérent) plutôt que lue décalée.
 */
class OpenSeesResultsReader
{
public:
    static bool readResults(const std::string& workingDirectory,
                            const CalculationSnapshot& snapshot,
                            const AnalysisParameters& params,
                            ResultsModel& outResults,
                            std::string* errorMessage = nullptr);

    static bool readResults(const std::string& workingDirectory,
                            const CalculationSnapshot& snapshot,
                            const OpenSeesModelMap& map,
                            const AnalysisParameters& params,
                            ResultsModel& outResults,
                            std::string* errorMessage = nullptr);

    /// Lit les sorties du passage « matrices » (mapping DDL, rigidités basiques, K_global) et
    /// remplit outResults.advanced(). Retourne false si le mapping DDL est absent ou incohérent.
    static bool readMatrixResults(const std::string& workingDirectory,
                                  const CalculationSnapshot& snapshot,
                                  const OpenSeesModelMap& map,
                                  const AnalysisParameters& params,
                                  bool globalStiffnessRequested,
                                  ResultsModel& outResults,
                                  std::string* errorMessage = nullptr);

    /// Analyse modale : valeurs propres (fréquences, périodes), formes propres de chaque nœud (normalisées à une
    /// translation maximale unitaire, une forme par pas), masses et participation (modalProperties).
    static bool readModalResults(const std::string& workingDirectory,
                                 const CalculationSnapshot& snapshot,
                                 const OpenSeesModelMap& map,
                                 const AnalysisParameters& params,
                                 ResultsModel& outResults,
                                 std::string* errorMessage = nullptr);

    /// Résultats des poutres et poteaux (extrémités et stations, convention de ResultsModel) à partir d'efforts
    /// d'extrémité au format localForce d'OpenSees (12 valeurs par barre, dans l'ordre de map.beamColumnTags(),
    /// forces exercées SUR l'élément, repère local) qui incluent DÉJÀ toutes les charges de loads. Même
    /// reconstruction que pour OpenSees : utilisée par les autres moteurs 3D (Fem3D) pour des résultats comparables.
    static std::map<ElementKey, ElementResults> beamResultsFromEndForces(const CalculationSnapshot& snapshot,
                                                                         const OpenSeesModelMap& map,
                                                                         const AnalysisParameters& params,
                                                                         const std::vector<double>& localForces,
                                                                         const std::map<int, NodeDisplacement>& displacements,
                                                                         const std::map<int, std::vector<BeamElementLoad>>& loads);

    /// Efforts repris par les blocages automatiques d'appuis (CalculationSnapshot::automaticRestraints) :
    /// avertissement dans les métadonnées et le journal s'ils ne sont pas négligeables. Public : commun à
    /// tous les moteurs 3D qui calculent le CalculationSnapshot.
    static void checkAutomaticRestraints(const CalculationSnapshot& snapshot, const AnalysisParameters& params,
                                         ResultsModel& outResults);
    static void computeGlobalEquilibrium(const CalculationSnapshot& snapshot,
                                         const AnalysisParameters& params,
                                         ResultsModel& outResults);
};

} // namespace TSA::Analysis
