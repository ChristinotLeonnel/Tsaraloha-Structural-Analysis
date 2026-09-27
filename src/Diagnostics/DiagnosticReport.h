#pragma once

#include <string>

namespace TSA::Model { class Model; }

namespace TSA::Diagnostics
{

/**
 * @brief Générateur de rapport de diagnostic complet et exportable pour TSA.
 */
class DiagnosticReport
{
public:
    /**
     * @brief Génère un fichier de rapport complet TSA_Diagnostic_Report_YYYY-MM-DD_HH-MM-SS.txt
     * @param model Pointeur optionnel vers le modèle structural courant pour inclure un résumé d'état.
     * @param targetDirectory Dossier de destination (par défaut logsDirectory()).
     * @return Chemin d'accès absolu au rapport généré, ou chaîne vide en cas d'erreur.
     */
    static std::string exportReport(const TSA::Model::Model* model = nullptr, const std::string& targetDirectory = "");
};

} // namespace TSA::Diagnostics
