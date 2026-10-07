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

private:
    static void computeGlobalEquilibrium(const CalculationSnapshot& snapshot,
                                         const AnalysisParameters& params,
                                         ResultsModel& outResults);
};

} // namespace TSA::Analysis
