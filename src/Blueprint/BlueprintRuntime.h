#pragma once

// Bibliothèque de nœuds et moteur d'exécution des Blueprints.
//
// Exécution : chaque nœud « event.start » lance le flux. Un nœud d'action lit ses entrées de données
// (évaluation à la demande des nœuds purs reliés, ou sorties mémorisées des nœuds d'action déjà
// exécutés), agit, publie ses sorties puis déclenche une de ses sorties d'exécution (fire). Les boucles
// déclenchent leur corps plusieurs fois. Les commandes modifient le projet par le registre central :
// une entrée Annuler par commande, vues notifiées par le modèle.
// Garde-fous : nombre d'étapes et d'itérations bornés, erreur explicite avec le nœud fautif.
// Profilage : nombre d'exécutions et temps cumulé par nœud.

#include "BlueprintGraph.h"

#include <chrono>
#include <functional>
#include <memory>

namespace TSA::Project
{
class ProjectSession;
}

namespace TSA::Blueprint
{

class ExecutionContext;

/// Implémentation d'un nœud. Faux = échec (message dans ctx.fail()).
using NodeExecutor = std::function<bool(ExecutionContext& ctx)>;

struct Issue
{
    int node = 0;               ///< 0 : graphe entier
    std::string message;
};

class NodeLibrary
{
public:
    bool add(NodeDefinition definition, NodeExecutor executor);
    const NodeDefinition* find(const std::string& type) const;
    const NodeExecutor* executor(const std::string& type) const;
    std::vector<const NodeDefinition*> definitions() const;

    /// Contrôle un lien (broches existantes, sens, sortes, types, cardinalité) sans le créer.
    bool canConnect(const Graph& g, const Link& link, std::string* why = nullptr) const;
    /// Crée le lien s'il est valide ; un lien existant vers la même entrée de donnée (ou depuis la
    /// même sortie d'exécution) est remplacé.
    bool connect(Graph& g, const Link& link, std::string* why = nullptr) const;
    /// Types inconnus, liens invalides, entrées requises non renseignées, cycles de données.
    std::vector<Issue> validate(const Graph& g) const;

    /// Bibliothèque standard : événements, paramètres, math, logique, flux, texte, débogage,
    /// commandes du registre central (cmd.*), nœuds scientifiques TSALab (science.*).
    static const NodeLibrary& standard();

private:
    struct Entry
    {
        NodeDefinition definition;
        NodeExecutor executor;
    };
    std::vector<Entry> m_entries;
    std::map<std::string, std::size_t> m_index;
};

void registerStandardNodes(NodeLibrary& library, const TSA::Automation::CommandRegistry& commands);

struct NodeStats
{
    int executions = 0;
    double milliseconds = 0.0;
};

struct ExecutionReport
{
    bool ok = false;
    std::string message;
    int failedNode = 0;                    ///< nœud en erreur (0 : aucun)
    std::vector<std::string> log;          ///< sorties de debug.print et des commandes
    std::map<int, NodeStats> profile;      ///< profileur
    int steps = 0;
    double milliseconds = 0.0;
};

struct ExecutionLimits
{
    int maxSteps = 100000;                 ///< exécutions de nœuds au total
    int maxIterations = 100000;            ///< tours d'une boucle
};

class Runner
{
public:
    Runner(const NodeLibrary& library, const Graph& graph, TSA::Project::ProjectSession* session,
           ExecutionLimits limits = {});
    ~Runner();

    /// Valide puis exécute à partir de chaque « event.start ». Les paramètres (param.*) peuvent être
    /// surchargés par nom (reconstruction paramétrique sans modifier le graphe).
    ExecutionReport run(const std::map<std::string, Value>& parameterOverrides = {});

private:
    friend class ExecutionContext;
    struct State;
    std::unique_ptr<State> m_state;
};

/// Vue d'un nœud pendant son exécution (passée à NodeExecutor).
class ExecutionContext
{
public:
    /// Valeur d'une entrée de donnée : source reliée, sinon valeur saisie, sinon valeur par défaut.
    /// Entier converti en réel si l'entrée est réelle. Échec → monostate et erreur enregistrée.
    Value input(const std::string& pin);
    double real(const std::string& pin);
    long long integer(const std::string& pin);
    bool boolean(const std::string& pin);
    std::string text(const std::string& pin);
    bool isConnected(const std::string& pin) const;
    bool hasValue(const std::string& pin) const;   ///< reliée ou saisie

    void setOutput(const std::string& pin, const Value& value);
    /// Déclenche la sortie d'exécution (exécute la chaîne aval, puis revient). Faux si erreur en aval.
    bool fire(const std::string& pin);

    bool fail(const std::string& message);
    void log(const std::string& line);
    TSA::Project::ProjectSession* session() const;
    const NodeInstance& node() const { return m_node; }
    const NodeDefinition& definition() const { return m_def; }
    int maxIterations() const;
    bool failed() const;
    /// Valeur imposée au paramètre `name` pour cette exécution (Runner::run), nulle si aucune.
    const Value* parameterOverride(const std::string& name) const;

private:
    friend class Runner;
    ExecutionContext(Runner::State& state, const NodeInstance& node, const NodeDefinition& def)
        : m_state(state), m_node(node), m_def(def) {}
    Runner::State& m_state;
    const NodeInstance& m_node;
    const NodeDefinition& m_def;
};

/// Texte d'une valeur (débogage, journal).
std::string toText(const Value& v);

} // namespace TSA::Blueprint
