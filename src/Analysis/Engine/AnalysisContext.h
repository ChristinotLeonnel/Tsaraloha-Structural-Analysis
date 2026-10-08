#pragma once

// Contexte d'analyse : CE QUE l'utilisateur veut calculer, indépendamment du moteur.
// Il ne contient ni widget, ni objet OCCT, ni détail d'un solveur : la portée désigne des objets
// du modèle TSA (axe de grille, niveau, plan de travail, sélection) par leurs identifiants, et les
// options propres à un moteur sont un bloc JSON opaque que seul ce moteur interprète.
// Référence : docs/ANALYSIS_ENGINES.md

#include "../ResultsModel.h"   // AnalysisType

#include <QJsonObject>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace TSA::Analysis
{

/// Identifiant stable d'un moteur (« opensees », « custom2d »…), utilisé dans le registre et le .tsa.
using EngineId = std::string;

enum class AnalysisDimension
{
    Plane2D,   ///< ossature plane : 3 DDL par nœud (u, v, θ) dans le plan de la portée
    Space3D    ///< ossature spatiale : 6 DDL par nœud
};

enum class ScopeType
{
    EntireModel,        ///< tout le modèle
    SelectedElements,   ///< éléments sélectionnés (SelectionManager → ElementSet)
    GridAxis,           ///< plan vertical d'un axe de grille (« A », « B », « 1 », « 2 »…)
    Level,              ///< plan horizontal d'un niveau (LevelManager)
    WorkPlane,          ///< plan de travail (WorkPlaneManager)
    ModelPlane          ///< plan contenant tout le modèle (détecté : vertical quelconque ou horizontal)
};

/// Direction de l'axe de grille : les lignes X (« 1, 2, 3… ») sont à x = cte, les lignes Y
/// (« A, B, C… ») à y = cte, dans le repère de la grille (origine + rotation).
enum class GridAxisFamily
{
    X,
    Y
};

struct AnalysisScope
{
    ScopeType type = ScopeType::EntireModel;

    // GridAxis
    std::string gridId;              ///< GridDefinition::id()
    GridAxisFamily axisFamily = GridAxisFamily::Y;
    std::string axisLabel;           ///< libellé de l'axe (« B »)

    // Level (portée Level, ou restriction d'une portée plane : intersection avec la cote du niveau)
    std::string levelId;

    // WorkPlane
    int workPlaneId = 0;

    // SelectedElements (identifiants TSA par famille)
    std::set<int> nodes, beams, columns, trussMembers, cables, slabs, walls, foundations;

    bool hasLevelRestriction() const { return type != ScopeType::Level && !levelId.empty(); }
    bool isPlanar() const
    {
        return type == ScopeType::GridAxis || type == ScopeType::Level || type == ScopeType::WorkPlane
            || type == ScopeType::ModelPlane;
    }
    bool operator==(const AnalysisScope& o) const;
};

/// Réglages communs à tous les moteurs (les réglages propres à un moteur sont dans engineSettings).
struct CommonAnalysisSettings
{
    bool includeSelfWeight = true;
};

struct AnalysisContext
{
    /// Version du schéma JSON (toJson / fromJson). À incrémenter pour tout changement non additif.
    static constexpr int kSchemaVersion = 1;

    EngineId engineId;
    AnalysisDimension dimension = AnalysisDimension::Space3D;
    AnalysisType type = AnalysisType::LinearStatic;
    AnalysisScope scope;

    /// Cas de charge à calculer. Vide = tous les cas. Ignoré si combinationId > 0.
    std::vector<int> loadCaseIds;
    /// Combinaison à calculer (0 = aucune : on calcule les cas de loadCaseIds).
    int combinationId = 0;

    CommonAnalysisSettings common;

    /// Réglages propres à chaque moteur, conservés par moteur (changer de moteur dans l'UI ne perd
    /// pas les réglages de l'autre). Le contenu n'est interprété que par le moteur concerné.
    std::map<EngineId, QJsonObject> engineSettings;

    QJsonObject settingsFor(const EngineId& id) const
    {
        auto it = engineSettings.find(id);
        return it != engineSettings.end() ? it->second : QJsonObject();
    }

    QJsonObject toJson() const;
    /// Lecture tolérante : un champ absent garde sa valeur par défaut, un champ inconnu est ignoré.
    static AnalysisContext fromJson(const QJsonObject& json, bool* ok = nullptr);
};

const char* analysisTypeKey(AnalysisType t);
const char* scopeTypeKey(ScopeType t);
const char* dimensionKey(AnalysisDimension d);

} // namespace TSA::Analysis
