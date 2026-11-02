#pragma once

#include "AnalysisEngineRegistry.h"

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Analysis
{

/// Modèle d'analyse prêt à calculer + bilan de validation (générique + moteur).
struct PreparedAnalysis
{
    bool extracted = false;    ///< false : contexte invalide (moteur inconnu, portée introuvable…)
    AnalysisModel model;
    ValidationResult validation;

    bool canRun() const { return extracted && validation.isValid(); }
};

/// Orchestration commune à tous les moteurs :
///   contexte → portée résolue → AnalysisModel → validation (générique + moteur) → run → résultats
/// Ne contient aucune logique propre à un moteur : tout passe par AnalysisEngine et ses capacités.
class AnalysisManager
{
public:
    explicit AnalysisManager(AnalysisEngineRegistry& registry) : m_registry(registry) {}

    AnalysisEngineRegistry& registry() { return m_registry; }
    const AnalysisEngineRegistry& registry() const { return m_registry; }

    /// Résout la portée, extrait le modèle d'analyse et le valide. N'exécute aucun calcul.
    PreparedAnalysis prepare(const TSA::Model::Model& model,
                             const TSA::Grid::GridManager* grids,
                             const AnalysisContext& context) const;

    /// Calcule un modèle préparé. Refuse un modèle invalide ou un moteur indisponible ; renseigne
    /// la traçabilité des résultats (moteur, version, portée, dimension, catégories disponibles).
    AnalysisRunResult run(const AnalysisContext& context,
                          const PreparedAnalysis& prepared,
                          const AnalysisRunCallbacks& callbacks = {});

    void cancel(const EngineId& id);

    /// Fonctionnalités retirées demandées par ce contexte (ancien projet) : type d'analyse et réglages du moteur choisi.
    /// Liste non vide : prepare() rend une erreur et run() refuse, le calcul n'est jamais fait en linéaire à la place.
    std::vector<std::string> unsupportedFeatures(const AnalysisContext& context) const;
    /// Message destiné à l'utilisateur pour ces fonctionnalités.
    static std::string unsupportedFeaturesMessage(const std::vector<std::string>& features);
    /// Câbles comprimés ou détendus (effort normal minimal N ≤ 0) dans ces résultats : avertissement à l'utilisateur, vide
    /// s'il n'y en a aucun (BUG-151). Un câble ne reprend pas de compression, mais le calcul linéaire le fait travailler
    /// comme une barre : run() ajoute ce texte aux points à vérifier après calcul, quel que soit le moteur.
    static std::string slackCableWarning(const ResultsModel& results);
    /// Empreinte SHA-256 (hexadécimal) des données d'entrée d'un calcul (BUG-111) : moteur, réglages (contexte JSON
    /// compact) et description complète du modèle préparé (script OpenSees statique de référence : nœuds, appuis, barres,
    /// sections, matériaux, charges retenues). Indépendante du moteur effectif, de la date et du dossier de travail.
    static std::string inputFingerprint(const AnalysisContext& context, const AnalysisModel& model, std::string* contextJson = nullptr);

    /// Contrôles communs déduits des capacités du moteur (public pour les tests).
    static ValidationResult validateGeneric(const AnalysisContext& context,
                                            const AnalysisModel& model,
                                            const EngineInfo& info,
                                            const AnalysisCapabilities& caps);

private:
    // Corps réels de prepare() et run() ; les fonctions publiques les entourent d'une frontière d'exception (BUG-155) : une
    // exception d'un moteur, d'OCCT (gp_Vec::Normalize…) ou de la mémoire devient un message, jamais une fermeture brutale.
    PreparedAnalysis prepareUnguarded(const TSA::Model::Model& model, const TSA::Grid::GridManager* grids, const AnalysisContext& context) const;
    AnalysisRunResult runUnguarded(const AnalysisContext& context, const PreparedAnalysis& prepared, const AnalysisRunCallbacks& callbacks);
    AnalysisEngineRegistry& m_registry;
};

} // namespace TSA::Analysis
