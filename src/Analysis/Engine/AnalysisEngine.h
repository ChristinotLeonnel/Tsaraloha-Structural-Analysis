#pragma once

// Abstraction d'un moteur de calcul structurel. TSA ne connaît les moteurs QUE par cette interface :
// ajouter un moteur = écrire un adaptateur, déclarer ses capacités, l'enregistrer dans
// registerBuiltInEngines (src/Analysis/Engines/BuiltInEngines.cpp) et, s'il a des options propres,
// enregistrer leur panneau (src/UI/Analysis/AnalysisEngineOptions.cpp). Aucune autre partie de TSA
// (UI Analysis, résultats, arbre, propriétés, ruban) ne teste l'identité d'un moteur.
// Référence : docs/ANALYSIS_ENGINES.md

#include "AnalysisContext.h"
#include "AnalysisModel.h"
#include "AnalysisValidation.h"
#include "../ResultsModel.h"

#include <QJsonObject>

#include <functional>
#include <string>

namespace TSA::Analysis
{

struct EngineInfo
{
    EngineId id;
    std::string name;
    /// Version réellement constatée (ex. lue sur l'exécutable) ; vide si inconnue. Jamais inventée.
    std::string version;
    std::string description;
};

/// Que faire des dalles / voiles d'une portée quand le moteur ne sait pas les calculer.
enum class UnsupportedElementPolicy
{
    Reject,              ///< erreur de validation (le calcul serait faux sans eux)
    ExcludeWithWarning   ///< calcul des seuls éléments filaires, avec avertissement explicite
};

/// Capacités déclarées par un moteur. Elles pilotent l'UI (types d'analyse, dimensions, onglets de
/// résultats) et la validation générique. Ne déclarer que ce que le moteur fait réellement.
struct AnalysisCapabilities
{
    // Dimensions
    bool supports2D = false;
    bool supports3D = false;

    // Familles d'éléments
    bool supportsFrame = false;        ///< poutres / poteaux (barres fléchies)
    bool supportsTruss = false;        ///< barres de treillis (effort normal seul)
    bool supportsCable = false;        ///< câbles
    bool supportsShell = false;        ///< dalles / voiles (maillage EF)
    bool supportsSolid = false;
    bool supportsSprings = false;      ///< appuis élastiques
    UnsupportedElementPolicy planarElementPolicy = UnsupportedElementPolicy::Reject;

    // Types d'analyse
    bool supportsStatic = false;
    bool supportsNonlinear = false;
    bool supportsBuckling = false;

    // Résultats que le moteur sait produire
    bool providesDisplacements = false;
    bool providesReactions = false;
    bool providesElementForces = false;
    bool providesElementStiffness = false;
    bool providesGlobalStiffness = false;
    bool providesDofMapping = false;

    bool supportsMesh = false;
    bool supportsCustomOptions = false;    ///< le moteur fournit un panneau d'options propre

    bool supportsAnalysisType(AnalysisType t) const
    {
        switch (t)
        {
        case AnalysisType::LinearStatic: return supportsStatic;
        case AnalysisType::NonLinearStatic: return supportsNonlinear;
        }
        return false;
    }
    bool supportsDimension(AnalysisDimension d) const
    {
        return d == AnalysisDimension::Plane2D ? supports2D : supports3D;
    }
};

/// Disponibilité effective du moteur sur ce poste (exécutable présent, solveur connecté…).
struct EngineAvailability
{
    bool available = false;
    std::string message;          ///< raison explicite si indisponible
    bool canProvision = false;    ///< provision() peut le rendre disponible (ex. téléchargement)
};

struct AnalysisRunCallbacks
{
    std::function<void(int percent, const std::string& status)> progress;
    std::function<void(const std::string& line)> log;
    /// Annulation demandée par l'appelant (lisible depuis le thread de calcul). Un moteur dont le
    /// solveur est créé dans run() doit le consulter après cette création : un cancel() reçu avant
    /// ne trouve pas encore le solveur et serait sinon perdu.
    std::function<bool()> cancelRequested;
};

struct AnalysisRunResult
{
    bool success = false;
    std::string message;
    /// Résultats indexés par identifiants TSA (nœud, ElementKey) : le remappage depuis les indices
    /// du moteur est fait par l'adaptateur via AnalysisMapping.
    ResultsModel results;
};

class AnalysisEngine
{
public:
    virtual ~AnalysisEngine() = default;

    virtual EngineInfo info() const = 0;
    virtual AnalysisCapabilities capabilities() const = 0;
    virtual EngineAvailability availability() const = 0;
    /// Rend le moteur disponible si possible (téléchargement, installation). L'UI demande
    /// d'abord l'accord de l'utilisateur ; par défaut : impossible.
    virtual bool provision(std::string* error)
    {
        if (error) *error = "Ce moteur ne peut pas être installé automatiquement.";
        return false;
    }

    /// Réglages propres au moteur par défaut (bloc JSON de AnalysisContext::engineSettings).
    virtual QJsonObject defaultSettings() const { return {}; }

    /// Contrôles PROPRES au moteur. Les contrôles communs (portée, familles d'éléments, sections,
    /// appuis, charges) sont faits par AnalysisManager à partir des capacités.
    virtual ValidationResult validate(const AnalysisContext& context, const AnalysisModel& model) const = 0;

    /// Calcul synchrone sur le modèle d'analyse préparé (jamais sur le modèle TSA).
    virtual AnalysisRunResult run(const AnalysisContext& context, const AnalysisModel& model,
                                  const AnalysisRunCallbacks& callbacks) = 0;

    virtual void cancel() = 0;
};

} // namespace TSA::Analysis
