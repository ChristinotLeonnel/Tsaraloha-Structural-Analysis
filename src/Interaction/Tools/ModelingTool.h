#pragma once

// Outils de modification et de dessin, communs à la saisie dans la vue 3D et à la saisie par
// fenêtre. Un outil possède des PARAMÈTRES (vecteur, angle, nombre, points…) :
//  - en saisie 3D, les clics (points ou barres sous le curseur) et la valeur tapée au clavier
//    renseignent ces paramètres, étape par étape, avec un aperçu ;
//  - en saisie par fenêtre, une boîte générique (ModelingToolDialog) les édite directement.
// Dans les deux cas, apply() exécute la même opération à partir des paramètres : un seul code
// métier par outil. Les outils ne connaissent ni OCCT AIS, ni Qt Widgets.
// Référence : docs/MODELING_TOOLS.md

#include "../../Model/CreationPresets.h"
#include "../../Model/SelectionQuery.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Interaction
{

enum class ToolCategory
{
    Modify,
    Draw
};

/// Ce que l'étape courante attend d'un clic dans la vue.
enum class PickKind
{
    None,    ///< aucune saisie (outil prêt, ex. fusion de nœuds)
    Point,   ///< un point (accrochage aux nœuds, grilles, plan de travail)
    Bar      ///< une barre (poutre, poteau, treillis) sous le curseur
};

struct BarRef
{
    TSA::Model::ElementKind kind = TSA::Model::ElementKind::Beam;
    int id = 0;
};

struct ToolPick
{
    gp_Pnt point;                  ///< point cliqué (sur la barre pour PickKind::Bar)
    int nodeId = -1;               ///< nœud accroché, s'il y en a un
    std::optional<BarRef> bar;     ///< barre sous le curseur (PickKind::Bar)
    double barParameter = 0.0;     ///< position du clic sur la barre, 0 = nœud de début
    bool modifier = false;         ///< Ctrl enfoncé : variante de l'outil (ex. copie au lieu de déplacement)
};

enum class ParamType
{
    Length,   ///< m
    Angle,    ///< degrés
    Factor,   ///< sans unité
    Count,    ///< entier ≥ 1
    Bool,
    Point     ///< (x, y, z) en m
};

struct ToolParameter
{
    std::string key;
    std::string label;
    ParamType type = ParamType::Length;
    double value = 0.0;
    gp_Pnt point;          ///< ParamType::Point
    double min = -1e9;
    double max = 1e9;
};

/// Données de l'application nécessaires aux outils (copie : l'outil n'y garde pas de pointeur
/// vers une vue).
struct ToolContext
{
    TSA::Model::ElementSet selection;
    gp_Pnt planeOrigin { 0, 0, 0 };          ///< plan de travail actif
    gp_Dir planeNormal { 0, 0, 1 };
    gp_Dir planeX { 1, 0, 0 };
    TSA::Model::StructurePresets presets;    ///< sections / matériaux des éléments dessinés
    const TSA::Grid::GridManager* grids = nullptr;
    /// Position courante du curseur (saisie 3D) : une valeur tapée au clavier est alors une
    /// distance dans la direction du curseur.
    std::optional<gp_Pnt> cursor;
};

/// Aperçu en saisie 3D : traits de construction et, pour les transformations, déplacement
/// fantôme de la sélection.
struct ToolPreview
{
    std::vector<std::pair<gp_Pnt, gp_Pnt>> lines;
    std::optional<gp_Trsf> selectionTransform;
};

struct ToolResult
{
    bool success = false;
    std::string message;
    TSA::Model::ElementSet created;   ///< à sélectionner après l'opération
};

class ModelingTool
{
public:
    virtual ~ModelingTool() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual ToolCategory category() const = 0;
    virtual bool needsSelection() const { return false; }
    /// false : l'opération exige des clics dans la vue (prolonger, ajuster…).
    virtual bool supportsDialog() const { return true; }
    /// L'outil reste actif après une opération (copier, dessiner…).
    virtual bool continuesAfterApply() const { return false; }

    // --- Paramètres (communs aux deux modes de saisie) ---
    std::vector<ToolParameter>& parameters() { return m_params; }
    const std::vector<ToolParameter>& parameters() const { return m_params; }
    double param(const std::string& key) const;
    gp_Pnt pointParam(const std::string& key) const;
    void setParam(const std::string& key, double value);
    void setPointParam(const std::string& key, const gp_Pnt& p);

    /// Initialise les paramètres dépendant du contexte (ex. axe = normale du plan de travail).
    virtual void prepare(const ToolContext& /*ctx*/) {}

    // --- Saisie 3D ---
    /// Remet la saisie à zéro (les paramètres gardent leurs dernières valeurs).
    virtual void reset() { m_picks.clear(); m_targets = {}; }
    virtual std::string prompt() const = 0;
    virtual PickKind nextPick() const = 0;
    virtual void addPick(const ToolPick& pick, const ToolContext& ctx) = 0;
    /// Valeur tapée au clavier (distance, angle, nombre…) ; false si l'étape n'en attend pas.
    virtual bool acceptValue(double /*value*/, const ToolContext& /*ctx*/) { return false; }
    /// Entrée : termine une saisie à nombre libre de points (chaîne de poutres).
    virtual bool finish() { return false; }
    /// Assez de données pour apply() depuis la saisie 3D.
    virtual bool ready() const = 0;
    virtual ToolPreview preview(const gp_Pnt& /*cursor*/, const ToolContext& /*ctx*/) const { return {}; }

    // --- Exécution (saisie 3D ou fenêtre) ---
    /// Opération sur le modèle à partir des paramètres et des cibles cliquées (à défaut : la
    /// sélection du contexte). L'appelant encadre l'appel d'une transaction (une entrée Undo).
    virtual ToolResult apply(TSA::Model::Model& model, const ToolContext& ctx) = 0;

    const std::vector<ToolPick>& picks() const { return m_picks; }
    std::size_t pickCount() const { return m_picks.size(); }

protected:
    void addParam(ToolParameter p) { m_params.push_back(std::move(p)); }
    /// Cibles désignées dans la vue, sinon sélection courante.
    TSA::Model::ElementSet targets(const ToolContext& ctx) const { return m_targets.empty() ? ctx.selection : m_targets; }

    std::vector<ToolParameter> m_params;
    std::vector<ToolPick> m_picks;
    TSA::Model::ElementSet m_targets;
};

class ModelingToolRegistry
{
public:
    using Factory = std::function<std::unique_ptr<ModelingTool>()>;
    void registerTool(Factory factory);
    std::unique_ptr<ModelingTool> create(const std::string& id) const;
    /// Un exemplaire de chaque outil (pour construire menus et ruban).
    std::vector<std::unique_ptr<ModelingTool>> instances() const;
    std::vector<std::string> ids() const;

private:
    std::vector<std::pair<std::string, Factory>> m_factories;
};

/// Outils intégrés. SEUL endroit à modifier pour ajouter un outil.
void registerBuiltInModelingTools(ModelingToolRegistry& registry);

// --- Aides géométriques partagées par les outils (pures, testées) ---
namespace ToolGeometry
{
/// Extrémités d'une barre ; false si absente.
bool barEnds(const TSA::Model::Model& model, const BarRef& bar, gp_Pnt& a, gp_Pnt& b, int* startId = nullptr, int* endId = nullptr);
/// Points les plus proches de deux droites (a1,a2) et (b1,b2) : paramètres ta, tb ; false si parallèles.
bool closestParameters(const gp_Pnt& a1, const gp_Pnt& a2, const gp_Pnt& b1, const gp_Pnt& b2, double& ta, double& tb);
/// Nœud existant à tol près de p, sinon nouveau nœud.
int nodeAt(TSA::Model::Model& model, const gp_Pnt& p, double tol = 1e-3);
/// Nœuds d'une sélection : nœuds + extrémités / sommets des éléments.
std::set<int> nodeClosure(const TSA::Model::Model& model, const TSA::Model::ElementSet& set);
} // namespace ToolGeometry

} // namespace TSA::Interaction
