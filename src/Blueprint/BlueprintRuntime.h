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
// Débogage : un Debugger (facultatif) est consulté avant chaque nœud d'action ; il peut suspendre
// l'exécution (point d'arrêt, pas à pas), inspecter les valeurs déjà produites, ou l'interrompre.

#include "BlueprintGraph.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <set>

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

    /// Vrai si un nœud du graphe lit ou modifie le projet (exécution obligatoire dans le thread de l'interface).
    bool usesProject(const Graph& g) const;

    /// Bibliothèque standard : événements, paramètres, math, logique, flux, texte, débogage,
    /// commandes du registre central (cmd.*), nœuds scientifiques TSALab (science.*), nœuds des plugins.
    static const NodeLibrary& standard();
    /// Bibliothèque standard modifiable (chargement des plugins au démarrage, avant toute exécution).
    static NodeLibrary& global();

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
/// Nœud d'action d'une commande du registre (« cmd.<id> ») ; faux si la commande est inconnue ou le nœud existe.
bool registerCommandNode(NodeLibrary& library, const TSA::Automation::CommandRegistry& commands, const std::string& commandId);

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

/// Ce que voit un débogueur quand l'exécution atteint un nœud d'action.
struct DebugState
{
    int node = 0;                                                   ///< nœud sur le point d'être exécuté
    int step = 0;                                                   ///< étapes déjà exécutées
    const std::map<int, std::map<std::string, Value>>* outputs = nullptr;   ///< sorties déjà publiées
    const std::vector<std::string>* log = nullptr;
};

enum class DebugAction
{
    Continue,   ///< reprendre jusqu'au prochain point d'arrêt
    Step,       ///< exécuter ce nœud puis s'arrêter au nœud d'action suivant
    Abort       ///< interrompre l'exécution (erreur « interrompue »)
};

class Debugger
{
public:
    virtual ~Debugger() = default;
    /// Appelé avant chaque nœud d'action (pas les nœuds purs).
    virtual DebugAction beforeNode(const DebugState& state) = 0;
};

/// Débogueur à points d'arrêt : onPause est appelé (et peut bloquer, ex. boucle d'événements de
/// l'interface) quand un point d'arrêt est atteint ou en pas à pas.
class BreakpointDebugger final : public Debugger
{
public:
    using PauseHandler = std::function<DebugAction(const DebugState&)>;
    explicit BreakpointDebugger(PauseHandler onPause, std::set<int> breakpoints = {}, bool stepFromStart = false)
        : m_onPause(std::move(onPause)), m_breakpoints(std::move(breakpoints)), m_stepping(stepFromStart) {}

    void setBreakpoints(std::set<int> breakpoints) { m_breakpoints = std::move(breakpoints); }
    DebugAction beforeNode(const DebugState& state) override;

private:
    PauseHandler m_onPause;
    std::set<int> m_breakpoints;
    bool m_stepping = false;
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
    /// Débogueur consulté avant chaque nœud d'action (nul : aucun). Non possédé.
    void setDebugger(Debugger* debugger);
    /// Demande l'arrêt (appelable depuis un autre thread) : l'exécution s'arrête avant le nœud suivant.
    void requestStop();

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
