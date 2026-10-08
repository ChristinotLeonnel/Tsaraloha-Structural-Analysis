#pragma once

// Registre central des commandes exécutables de l'écosystème (TSA, TSALab) — ADR-024, phase 5.
//
// Une commande métier est implémentée UNE seule fois ici et appelée par tous les clients :
//   interface (actions), Blueprint (nœuds), IA (outils), console / scripts (ligne de commande).
// Chaque commande déclare ses paramètres typés (avec grandeur physique) et ses sorties ; le registre
// valide les arguments, applique les valeurs par défaut et enregistre UNE entrée Annuler / Rétablir par
// exécution modifiante (EditTransaction). Les vues sont notifiées par le modèle (observateurs).
//
// Couche modèle : aucun widget ; valeurs en types C++ standard (std::variant) pour rester utilisables
// par un moteur d'exécution (Blueprint) ou un interpréteur sans Qt.

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::Automation
{

enum class ValueType
{
    Bool,
    Integer,
    Real,
    Text,
    Point3,   ///< x, y, z (m)
    IdList
};

/// Grandeur physique d'un paramètre réel (affichage, contrôle d'unités, typage des broches Blueprint).
enum class Quantity
{
    None,
    Length,          ///< m
    Force,           ///< kN
    Moment,          ///< kN·m
    ForcePerLength,  ///< kN/m
    Angle            ///< degrés
};

using Point3 = std::array<double, 3>;
using Value = std::variant<std::monostate, bool, long long, double, std::string, Point3, std::vector<int>>;
using Arguments = std::map<std::string, Value>;

struct ParameterSpec
{
    std::string name;            ///< identifiant (ligne de commande, broche)
    std::string label;           ///< libellé affiché
    ValueType type = ValueType::Real;
    Quantity quantity = Quantity::None;
    bool required = true;
    Value defaultValue;          ///< utilisé si absent et non requis
    std::string description;
};

struct CommandSpec
{
    std::string id;              ///< « model.create_node »
    std::string title;           ///< « Créer un nœud »
    std::string category;        ///< « Modèle », « Charges », « Requêtes »
    std::string description;
    std::vector<ParameterSpec> parameters;
    std::vector<ParameterSpec> outputs;
    bool modifiesModel = false;  ///< exécution dans une transaction (une entrée Annuler)
};

struct CommandResult
{
    bool ok = false;
    std::string message;
    Arguments outputs;
};

class CommandRegistry
{
public:
    using Handler = std::function<CommandResult(TSA::Project::ProjectSession& session, const Arguments& args)>;

    /// Faux si l'identifiant existe déjà.
    bool add(CommandSpec spec, Handler handler);

    const CommandSpec* find(const std::string& id) const;
    /// Spécifications, dans l'ordre d'enregistrement.
    std::vector<const CommandSpec*> commands() const;

    /// Valide les arguments (présence, type ; entiers acceptés pour un réel), complète les valeurs par
    /// défaut puis exécute. Commande modifiante : transaction annulée si la commande échoue.
    CommandResult execute(TSA::Project::ProjectSession& session, const std::string& id, Arguments args) const;

    /// Registre des commandes intégrées (registerBuiltInCommands) et des plugins, partagé par toute l'application.
    static const CommandRegistry& builtIn();
    /// Même registre, modifiable (chargement des plugins au démarrage).
    static CommandRegistry& global();

private:
    struct Entry
    {
        CommandSpec spec;
        Handler handler;
    };
    std::vector<Entry> m_entries;
    std::map<std::string, std::size_t> m_index;
};

/// Commandes intégrées : création de nœuds et de barres, appuis, cas et charges, requêtes.
void registerBuiltInCommands(CommandRegistry& registry);

// --- Ligne de commande (console, scripts) -------------------------------------------------------
// Syntaxe : <id> nom=valeur … ; Point3 « x,y,z » ; IdList « 1,2,3 » ; texte entre guillemets si espaces.
// Exemple : model.create_node position=0,0,3   puis   model.create_beam start=1 end=2 section="IPE 300"

/// Analyse puis exécute une ligne. Les erreurs de syntaxe ou d'arguments sont rendues dans le résultat.
CommandResult executeCommandLine(const CommandRegistry& registry, TSA::Project::ProjectSession& session,
                                 const std::string& line);

/// Analyse une ligne sans l'exécuter (scripts, Blueprint, IA) : commande connue, arguments typés.
bool parseCommandLine(const CommandRegistry& registry, const std::string& line, std::string& commandId, Arguments& args,
                      std::string* error = nullptr);
/// Découpe une ligne en jetons (texte entre guillemets = un jeton).
bool tokenizeCommandLine(const std::string& line, std::vector<std::string>& tokens, std::string* error = nullptr);
/// Valeur brute d'un argument selon son type (« true », « 3 », « 0,0,3 », « 1,2 », texte).
bool parseArgument(const std::string& raw, ValueType type, Value& out);
/// Valeur écrite dans la syntaxe de la ligne de commande, relisible par parseArgument (« 0,0,3 », « "IPE 300" »).
std::string formatArgument(const Value& value);

/// Valeur lisible (« 1,2,3 », « 0, 0, 3 », « vrai »).
std::string formatValue(const Value& value);
const char* typeName(ValueType type);
const char* quantityUnit(Quantity quantity);

} // namespace TSA::Automation
