#pragma once

// Stratégies de numérotation : chaque stratégie produit un ORDRE des entités (et, le cas échéant, un
// repère de grille ou un numéro de couche) ; la génération des étiquettes (préfixe, numéro de départ,
// pas, format) est commune et appliquée par TopologyNumberingService. Ajouter une stratégie = une
// classe + une ligne dans le registre : la fenêtre et les solveurs n'ont rien à modifier.
//
// Aucune stratégie ne lit ni n'écrit de géométrie ou de connectivité : elles ne font que trier.

#include "TopologySettings.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Topology
{

/// Grille d'axes cartésienne (copie des données de la grille active, sans dépendance vers Grid).
struct GridAxes
{
    double originX = 0.0, originY = 0.0;
    double rotationDeg = 0.0;
    std::vector<double> xPositions; ///< positions locales des axes parallèles à Y (coordonnée x locale)
    std::vector<std::string> xLabels;
    std::vector<double> yPositions; ///< positions locales des axes parallèles à X
    std::vector<std::string> yLabels;
};

/// Entrées de la numérotation (hors modèle).
struct NumberingInput
{
    std::optional<GridAxes> grid;              ///< grille active (stratégie « grille »)
    std::vector<double> levelElevations;       ///< niveaux du projet (stratégie « par niveau »)
    std::set<int> selectedNodes;               ///< portée Sélection
    std::map<EntityFamily, std::set<int>> selectedElements;
};

/// Point d'une entité à ordonner (nœud, ou centre d'un élément).
struct OrderItem
{
    int id = 0;
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Availability
{
    bool available = true;
    std::string reason; ///< pourquoi la stratégie est indisponible pour ce modèle
};

/// Résultat d'une stratégie : ordre, et informations facultatives par entité.
struct Ordering
{
    std::vector<int> order;                  ///< chaque entité de la portée exactement une fois
    std::map<int, std::string> gridRef;      ///< repère de grille ({g}) ; absent = hors grille
    std::map<int, int> layer;                ///< couche ou niveau ({l}), 1..n
    std::vector<std::string> warnings;
};

struct NodeContext
{
    const TSA::Model::Model& model;
    const NodeNumberingSettings& settings;
    const NumberingInput& input;
    std::vector<OrderItem> items;            ///< nœuds de la portée
};

class NodeNumberingStrategy
{
public:
    virtual ~NodeNumberingStrategy() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual Availability availability(const TSA::Model::Model& model, const NumberingInput& input) const;
    /// Jetons de format conseillés (ex. « {g} » pour la grille).
    virtual std::string suggestedPattern() const { return "{p}{n}"; }
    virtual Ordering order(const NodeContext& ctx) const = 0;
};

struct ElementItem
{
    EntityFamily family = EntityFamily::Beam;
    int id = 0;
    double cx = 0.0, cy = 0.0, cz = 0.0;     ///< centre géométrique
    std::vector<int> nodeIds;                ///< nœuds de l'élément (ordre et orientation conservés)
};

struct ElementContext
{
    const TSA::Model::Model& model;
    const ElementNumberingSettings& settings;
    const NumberingInput& input;
    std::vector<ElementItem> items;
    std::map<int, int> nodeRank;             ///< rang des nœuds dans la nouvelle numérotation (stratégie topologique)
};

struct ElementOrdering
{
    std::vector<std::pair<EntityFamily, int>> order;
    std::vector<std::string> warnings;
};

class ElementNumberingStrategy
{
public:
    virtual ~ElementNumberingStrategy() = default;
    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual Availability availability(const TSA::Model::Model& model, const NumberingInput& input) const;
    virtual ElementOrdering order(const ElementContext& ctx) const = 0;
};

/// Registres (ordre d'affichage). Les stratégies sans prérequis dans TSA (groupes, maillage) y
/// figurent avec une disponibilité négative expliquée : elles ne sont jamais présentées comme
/// disponibles.
const std::vector<std::unique_ptr<NodeNumberingStrategy>>& nodeStrategies();
const std::vector<std::unique_ptr<ElementNumberingStrategy>>& elementStrategies();
const NodeNumberingStrategy* findNodeStrategy(const std::string& id);
const ElementNumberingStrategy* findElementStrategy(const std::string& id);

/// Outils partagés (testés séparément).
namespace detail
{
/// Rang de regroupement par axe : valeurs à moins de `tol` d'une valeur voisine dans le même groupe
/// (chaînage) ; jamais d'égalité exacte entre flottants.
std::map<int, int> clusterRanks(const std::vector<std::pair<int, double>>& values, double tol);
/// Tri déterministe par axes regroupés (sens par axe), puis identifiant.
std::vector<int> sortByAxes(const std::vector<OrderItem>& items, const AxisOrder& order, double tol);
/// Graphe de connectivité (barres, câbles, treillis, voiles, contours de dalles) restreint à `scope`.
std::map<int, std::vector<int>> adjacency(const TSA::Model::Model& model, const std::set<int>& scope);
/// Dimension du nuage de nœuds : 0 (vide ou un point), 1 (aligné), 2 (plan), 3.
int modelDimension(const TSA::Model::Model& model, double tol = 1e-6);
} // namespace detail

} // namespace TSA::Topology
