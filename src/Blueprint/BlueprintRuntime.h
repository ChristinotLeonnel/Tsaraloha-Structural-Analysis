#pragma once

// Bibliothèque de nœuds et moteur d'exécution des Blueprints.
//
// Exécution : chaque nœud « event.start » lance le flux. Un nœud d'action lit ses entrées de données
// (évaluation à la demande des nœuds purs reliés, ou sorties mémorisées des nœuds d'action déjà
// exécutés), agit, publie ses sorties puis déclenche une de ses sorties d'exécution (fire). Les boucles
// déclenchent leur corps plusieurs fois. Les commandes modifient le projet par le registre central :
// une entrée Annuler par commande, vues notifiées par le modèle.
// Garde-fous : nombre d'étapes et d'itérations bornés, erreur explicite avec le nœud fautif.
// Profilage : nombre d'exécutions, réutilisations et temps cumulé par nœud.
// Débogage : un Debugger (facultatif) est consulté avant chaque nœud d'action ; il peut suspendre
// l'exécution (point d'arrêt, pas à pas), inspecter les valeurs déjà produites, ou l'interrompre.
// Réutilisation : un nœud pur « cacheable » n'est réévalué que si ses entrées changent (EvaluationCache,
// conservé d'une exécution à l'autre par l'éditeur : seule la partie modifiée du graphe est recalculée).
// Variables : var.set / var.get (accumulateurs des boucles, par exemple K = K + Aᵀ·k·A).
// Valeurs intermédiaires : toutes les sorties produites sont rendues dans le rapport (ExecutionReport::values).

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

class NodeLibrary;

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
    /// Erreurs bloquantes : types inconnus, version de nœud plus récente que la bibliothèque, liens invalides
    /// (types, objets), entrées requises non renseignées, cycles de données entre nœuds purs, cycles d'exécution.
    std::vector<Issue> validate(const Graph& g) const;
    /// Avertissements (non bloquants) : un nœud d'action lit la sortie d'un nœud d'action qui n'est pas exécuté avant
    /// lui dans le flux (dépendance incorrecte : « sortie non disponible » à l'exécution, ou valeur d'un tour précédent).
    std::vector<Issue> warnings(const Graph& g) const;

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
    int reused = 0;                        ///< résultats réutilisés sans réévaluation (nœud pur « cacheable »)
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
    int reused = 0;                        ///< réutilisations au total
    double milliseconds = 0.0;
    /// Dernières valeurs produites par chaque nœud (valeurs intermédiaires : matrices, vecteurs, résultats…).
    std::map<int, std::map<std::string, Value>> values;
    std::map<std::string, Value> variables;   ///< variables du graphe en fin d'exécution
};

/// Résultats réutilisables des nœuds purs « cacheable » (NodeDefinition::cacheable) : un nœud n'est réévalué que si
/// son type ou l'une de ses entrées (valeur reliée ou saisie, comparée par sameValue) a changé. Une instance par
/// graphe, conservée d'une exécution à l'autre (éditeur) ; à vider si le graphe est remplacé. Non partagé entre
/// exécutions simultanées.
class EvaluationCache
{
public:
    void clear() { m_entries.clear(); }
    std::size_t size() const { return m_entries.size(); }
    /// Retire les entrées des nœuds absents du graphe ou dont le type a changé.
    void prune(const Graph& graph);

private:
    friend class Runner;
    struct Entry
    {
        std::string type;
        std::vector<Value> inputs;
        std::map<std::string, Value> outputs;
    };
    std::map<int, Entry> m_entries;
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
    /// Réutilisation des résultats d'une exécution à l'autre (nul : réutilisation limitée à cette exécution). Non possédé.
    void setCache(EvaluationCache* cache);
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
    /// Objet d'une entrée : nul (et erreur enregistrée) s'il manque ou n'a pas le type de la broche.
    ObjectRef object(const std::string& pin);
    /// Objet d'une entrée converti dans sa classe C++ : nul (et erreur) si absent ou d'une autre classe.
    template <typename T>
    std::shared_ptr<const T> objectAs(const std::string& pin)
    {
        const ObjectRef o = object(pin);
        if (!o) return nullptr;
        auto t = std::dynamic_pointer_cast<const T>(o);
        if (!t) fail("entrée « " + pin + " » : objet " + o->typeName() + " d'une classe inattendue");
        return t;
    }

    /// Variables du graphe (var.set / var.get) : partagées par tous les nœuds pendant une exécution.
    bool hasVariable(const std::string& name) const;
    Value variable(const std::string& name) const;
    void setVariable(const std::string& name, const Value& value);

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
