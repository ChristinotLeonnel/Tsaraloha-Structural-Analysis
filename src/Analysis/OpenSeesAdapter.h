#pragma once

// ============================================================
// EXTERNAL LIBRARY
// Library    : OpenSees (Open System for Earthquake Engineering Simulation)
// Version    : 3.4.0 - 3.8.0+
// Role       : Structural Analysis Backend (FEM)
// Interface  : Adapter converting TSA StructuralModel / Snapshot to Tcl scripts
// Constraint : TSA must not expose OpenSees implementation details to UI/Model
// Standard   : ISO/IEC 25010 §4.2.7 / Requirement: REQ-EXT-LIB-001
// ============================================================

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{

struct OpenSeesOptions
{
    bool includeSelfWeight = true;          ///< Inclut le poids propre calculé automatiquement
    bool includeAnalysisCommands = true;    ///< Inclut les commandes de résolution (analysis Static, etc.)
    int targetLoadCaseId = 0;              ///< Cas de charge spécifique (0 = tous les cas actifs)
    int targetCombinationId = 0;           ///< Combinaison spécifique (0 = cas séparés)
    bool useKiloNewtons = true;            ///< true: unités kN, m | false: N, m
};

using OpenSeesAdapterOptions = OpenSeesOptions;

/**
 * @brief Adaptateur structural et générateur de scripts de calcul pour OpenSees.
 * Traduit le modèle structural TSA (nœuds, éléments, matériaux, sections, charges)
 * en commandes OpenSees normalisées (Tcl).
 */
class OpenSeesAdapter
{
public:
    OpenSeesAdapter() = default;

    /**
     * @brief Génère le script complet d'analyse OpenSees (Tcl).
     */
    static std::string generateTclScript(const TSA::Model::Model& model,
                                         const OpenSeesOptions& options = {});
    static std::string generateScript(const TSA::Model::Model& model,
                                      const OpenSeesOptions& options = {})
    {
        return generateTclScript(model, options);
    }

    /**
     * @brief Exporte le script vers un fichier .tcl sur le disque.
     */
    static bool exportToFile(const std::string& filePath,
                             const TSA::Model::Model& model,
                             const OpenSeesOptions& options = {});

    static bool exportToFile(const TSA::Model::Model& model,
                             const std::string& filePath,
                             const OpenSeesOptions& options = {})
    {
        return exportToFile(filePath, model, options);
    }
};

} // namespace TSA::Analysis
