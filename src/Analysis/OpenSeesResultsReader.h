#pragma once

#include "ResultsModel.h"
#include "CalculationSnapshot.h"
#include "OpenSeesAnalysisBuilder.h"
#include <string>

namespace TSA::Analysis
{

/**
 * @brief Lecteur et désérialiseur des fichiers de résultats générés par OpenSees.
 * Reconstitue les déplacements nodaux, réactions aux appuis, efforts intérieurs aux barres,
 * modes propres et synthèses d'équilibre.
 */
class OpenSeesResultsReader
{
public:
    static bool readResults(const std::string& workingDirectory,
                            const CalculationSnapshot& snapshot,
                            const AnalysisParameters& params,
                            const std::string& solverStdOut,
                            ResultsModel& outResults,
                            std::string* errorMessage = nullptr);

private:
    static bool readDisplacements(const std::string& filePath,
                                 const CalculationSnapshot& snapshot,
                                 ResultsModel& outResults,
                                 bool* hasNonFinite = nullptr);

    static bool readReactions(const std::string& filePath,
                              const CalculationSnapshot& snapshot,
                              ResultsModel& outResults,
                              bool* hasNonFinite = nullptr);

    static bool readElementForces(const std::string& filePath,
                                  const CalculationSnapshot& snapshot,
                                  ResultsModel& outResults,
                                  bool* hasNonFinite = nullptr);

    static void parseModalOutput(const std::string& solverStdOut,
                                 ResultsModel& outResults);

    static void computeGlobalEquilibrium(const CalculationSnapshot& snapshot,
                                         const AnalysisParameters& params,
                                         ResultsModel& outResults);
};

} // namespace TSA::Analysis
