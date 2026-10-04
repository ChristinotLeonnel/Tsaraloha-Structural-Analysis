#pragma once

// Export unique des résultats de calcul (CSV / JSON / TXT). Les valeurs sont écrites dans les
// unités du ResultsModel (aucune conversion) avec 17 chiffres significatifs, et chaque matrice
// est accompagnée de ses métadonnées (source, type, repère, ordre des DDL, exactitude).

#include "ResultsModel.h"

#include <string>

namespace TSA::Analysis
{

enum class ResultsExportFormat
{
    Csv,
    Json,
    Txt
};

enum class ResultsDataset
{
    Displacements,
    Reactions,
    ElementForces,     ///< efforts d'extrémité ; + forces brutes local/global/basic si disponibles
    DofMap,
    GlobalStiffness,   ///< K_global (COO en CSV)
    ElementStiffness,  ///< k_basic / k_local / K_global de chaque élément
    All                ///< JSON ou TXT uniquement
};

namespace ResultsExport
{
/// Écrit le jeu de données. Retourne false (et un message) si le jeu de données est absent
/// (ex. matrices en mode Light) ou si le fichier ne peut pas être écrit.
bool write(const ResultsModel& results, ResultsDataset dataset, ResultsExportFormat format,
           const std::string& path, std::string* errorMessage = nullptr);

/// Contenu texte (même format que le fichier) — utilisé par les tests et l'aperçu.
std::string render(const ResultsModel& results, ResultsDataset dataset, ResultsExportFormat format,
                   std::string* errorMessage = nullptr);

/// Représentation texte d'une matrice dense (lignes/colonnes étiquetées).
std::string formatMatrix(const DenseMatrix& m, const std::vector<std::string>& labels, int precision = 6);
} // namespace ResultsExport

} // namespace TSA::Analysis
