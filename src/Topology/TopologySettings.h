#pragma once

// Paramètres de topologie et de numérotation d'un projet TSA (persistés dans le chunk TOPO du .tsa).
//
// Distinction fondamentale :
//   - identifiant interne : clé stable de l'entité dans le modèle (Node::id, Beam::id…), jamais
//     modifiée par une renumérotation ; c'est elle que les solveurs, les charges, les appuis et les
//     résultats référencent ;
//   - étiquette visible : nom affiché (Node::name…), seule donnée modifiée par une renumérotation.
// Changer la numérotation ne déplace aucun nœud, ne modifie aucune connectivité ni propriété.

#include <array>
#include <map>
#include <string>

namespace TSA::Topology
{

/// Familles d'entités numérotées.
enum class EntityFamily
{
    Node,
    Beam,
    Column,
    Truss,
    Cable,
    Slab,
    Wall,
    Foundation
};

const char* familyKey(EntityFamily f);           ///< clé JSON stable (« node », « beam »…)
std::string familyDisplayName(EntityFamily f);   ///< libellé français
bool familyFromKey(const std::string& key, EntityFamily& out);

/// Ordre de parcours des axes (permutation de X, Y, Z) et sens par axe.
struct AxisOrder
{
    std::array<char, 3> axes{ 'Z', 'Y', 'X' };      ///< axe le plus significatif en premier
    std::array<bool, 3> descending{ false, false, false };

    std::string code() const { return std::string(axes.begin(), axes.end()); }
    static bool fromCode(const std::string& code, AxisOrder& out); ///< « XYZ », « ZYX »… (permutation)
};

/// Format des étiquettes : jetons {p} préfixe, {n} numéro complété à `width` chiffres, {g} repère
/// de grille (ex. A1), {l} numéro de couche ou de niveau (1, 2…).
struct LabelFormat
{
    std::string prefix = "N";
    std::string pattern = "{p}{n}";
    int width = 3;       ///< chiffres minimum de {n} (0 : sans complément)
    int start = 1;       ///< premier numéro
    int increment = 1;   ///< pas entre deux numéros
};

/// Portée d'une renumérotation.
enum class NumberingScope
{
    Project,    ///< toutes les entités du projet
    Selection   ///< entités sélectionnées seulement (les autres gardent leur étiquette)
};

struct NodeNumberingSettings
{
    std::string strategy = "sequential";
    AxisOrder order;
    double tolerance = 0.001;   ///< m : regroupement des coordonnées (rangées, colonnes, couches, grille)
    int startNodeId = 0;        ///< nœud de départ des parcours (0 : choix automatique déterministe)
    bool restartPerLayer = false;
    bool preserveCustomNames = true;
    LabelFormat format;
};

struct ElementNumberingSettings
{
    std::string strategy = "sequential";
    bool sharedSequence = false; ///< une seule suite de numéros pour toutes les familles
    AxisOrder order;
    double tolerance = 0.001;
    bool preserveCustomNames = true;
    LabelFormat format{ "", "{p}{n}", 3, 1, 1 };
    std::map<EntityFamily, std::string> prefixes; ///< préfixe par famille (barres de rôle poteau : préfixe Column)
};

struct TopologySettings
{
    static constexpr int kSchemaVersion = 1;

    NumberingScope scope = NumberingScope::Project;
    bool showNodeLabels = false;     ///< étiquettes des nœuds dans la vue 3D
    NodeNumberingSettings nodes;
    ElementNumberingSettings elements;

    /// Valeurs par défaut : celles qui reproduisent les étiquettes historiques de TSA (N001, B001…).
    static TopologySettings defaults();

    /// JSON UTF-8 versionné (« schema »). Lecture tolérante : champ absent ou invalide → défaut,
    /// signalé dans *warnings. Chaîne vide (ancien projet sans chunk TOPO) → défauts sans avertissement.
    std::string toJson() const;
    static TopologySettings fromJson(const std::string& json, std::string* warnings = nullptr);

    std::string elementPrefix(EntityFamily f) const;
};

} // namespace TSA::Topology
