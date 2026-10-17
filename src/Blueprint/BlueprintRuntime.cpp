#include "BlueprintRuntime.h"

#include "BlueprintSubgraph.h"

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>

namespace TSA::Blueprint
{

namespace
{
using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

bool isUnset(const Value& v) { return std::holds_alternative<std::monostate>(v); }
} // namespace

std::string toText(const Value& v)
{
    if (isUnset(v)) return "(vide)";
    return TSA::Automation::formatValue(v);
}

// -----------------------------------------------------------------------------
// Bibliothèque
// -----------------------------------------------------------------------------

bool NodeLibrary::add(NodeDefinition definition, NodeExecutor executor)
{
    if (definition.id.empty() || !executor || m_index.count(definition.id) || (m_base && m_base->find(definition.id))) return false;
    // Réutilisation réservée aux fonctions pures de leurs entrées
    if (!definition.pure || definition.usesProject) definition.cacheable = false;
    m_index[definition.id] = m_entries.size();
    m_entries.push_back({ std::move(definition), std::move(executor) });
    return true;
}

const NodeDefinition* NodeLibrary::find(const std::string& type) const
{
    const auto it = m_index.find(type);
    if (it != m_index.end()) return &m_entries[it->second].definition;
    return m_base ? m_base->find(type) : nullptr;
}

const NodeExecutor* NodeLibrary::executor(const std::string& type) const
{
    const auto it = m_index.find(type);
    if (it != m_index.end()) return &m_entries[it->second].executor;
    return m_base ? m_base->executor(type) : nullptr;
}

std::vector<const NodeDefinition*> NodeLibrary::definitions() const
{
    std::vector<const NodeDefinition*> list = m_base ? m_base->definitions() : std::vector<const NodeDefinition*> {};
    for (const auto& e : m_entries) list.push_back(&e.definition);
    return list;
}

const NodeLibrary& NodeLibrary::forGraph(const Graph& g, std::unique_ptr<NodeLibrary>& holder) const
{
    if (g.subgraphs.empty() || m_subgraphTypes) return *this;
    holder = withSubgraphs(*this, g.subgraphs);
    return *holder;
}

bool NodeLibrary::canConnect(const Graph& g, const Link& link, std::string* why) const
{
    if (!g.subgraphs.empty() && !m_subgraphTypes)
    {
        std::unique_ptr<NodeLibrary> holder;
        return forGraph(g, holder).canConnect(g, link, why);
    }
    auto fail = [why](const std::string& m) {
        if (why) *why = m;
        return false;
    };
    const auto* a = g.node(link.fromNode);
    const auto* b = g.node(link.toNode);
    if (!a || !b) return fail("nœud inexistant");
    if (link.fromNode == link.toNode) return fail("un nœud ne peut pas être relié à lui-même");
    const auto* da = find(a->type);
    const auto* db = find(b->type);
    if (!da || !db) return fail("type de nœud inconnu");
    const PinSpec* out = da->output(link.fromPin);
    const PinSpec* in = db->input(link.toPin);
    if (!out) return fail("sortie « " + link.fromPin + " » inexistante sur " + da->title);
    if (!in) return fail("entrée « " + link.toPin + " » inexistante sur " + db->title);
    if (out->kind != in->kind) return fail("une broche d'exécution ne se relie qu'à une broche d'exécution");
    if (out->kind == PinKind::Data && !dataCompatible(*out, *in))
        return fail("types incompatibles : " + pinTypeName(*out) + " → " + pinTypeName(*in));
    return true;
}

bool NodeLibrary::connect(Graph& g, const Link& link, std::string* why) const
{
    if (!g.subgraphs.empty() && !m_subgraphTypes)
    {
        std::unique_ptr<NodeLibrary> holder;
        return forGraph(g, holder).connect(g, link, why);
    }
    if (!canConnect(g, link, why)) return false;
    const auto* def = find(g.node(link.fromNode)->type);
    const bool exec = def->output(link.fromPin)->kind == PinKind::Exec;
    if (exec)
        for (const auto& l : g.linksFrom(link.fromNode, link.fromPin)) g.removeLink(l);   // une cible par sortie d'exécution
    else if (const auto existing = g.linkTo(link.toNode, link.toPin))
        g.removeLink(*existing);                                                           // une source par entrée de donnée
    g.addLink(link);
    return true;
}

std::vector<Issue> NodeLibrary::validate(const Graph& g) const
{
    if (!g.subgraphs.empty() && !m_subgraphTypes)
    {
        std::unique_ptr<NodeLibrary> holder;
        return forGraph(g, holder).validate(g);
    }
    std::vector<Issue> issues;
    for (const auto& message : validateSubgraphs(*this, g.subgraphs)) issues.push_back({ 0, message });
    for (const auto& [id, n] : g.nodes())
    {
        const auto* def = find(n.type);
        if (!def)
        {
            const std::string sub = subgraphOfType(n.type);
            issues.push_back({ id, sub.empty() ? "type de nœud inconnu « " + n.type + " »"
                                               : "sous-graphe « " + sub + " » introuvable : sa définition est absente du graphe (importer le sous-graphe)" });
            continue;
        }
        if (n.version > def->version)
            issues.push_back({ id, def->title + " : nœud enregistré en version " + std::to_string(n.version) + ", bibliothèque en version "
                                       + std::to_string(def->version) + " (fichier créé par une version plus récente)" });
        for (const auto& p : def->inputs)
        {
            if (p.kind != PinKind::Data || !p.required) continue;
            const bool linked = g.linkTo(id, p.name).has_value();
            const auto v = n.values.find(p.name);
            const bool valued = (v != n.values.end() && !isUnset(v->second)) || !isUnset(p.defaultValue);
            if (!linked && !valued) issues.push_back({ id, def->title + " : entrée requise « " + p.label + " » non renseignée" });
        }
    }
    for (const auto& l : g.links())
    {
        std::string why;
        if (!canConnect(g, l, &why)) issues.push_back({ l.toNode, "lien invalide : " + why });
    }
    // Cycles de données entre nœuds purs (évaluation infinie)
    std::map<int, int> color;   // 0 blanc, 1 en cours, 2 fini
    std::function<bool(int)> visit = [&](int id) -> bool {
        color[id] = 1;
        for (const auto& l : g.links())
        {
            if (l.toNode != id) continue;
            const auto* src = g.node(l.fromNode);
            const auto* d = src ? find(src->type) : nullptr;
            if (!d || !d->pure) continue;
            if (color[l.fromNode] == 1) return true;
            if (color[l.fromNode] == 0 && visit(l.fromNode)) return true;
        }
        color[id] = 2;
        return false;
    };
    for (const auto& [id, n] : g.nodes())
    {
        const auto* d = find(n.type);
        if (d && d->pure && color[id] == 0 && visit(id)) issues.push_back({ id, "cycle de données entre nœuds purs" });
    }
    // Cycles d'exécution (A → … → A par les liens d'exécution) : récursion infinie ; une boucle s'écrit avec Pour / Tant que
    std::map<int, int> state;   // 0 blanc, 1 en cours, 2 fini
    std::function<bool(int)> execVisit = [&](int id) -> bool {
        state[id] = 1;
        for (const auto& l : g.links())
        {
            if (l.fromNode != id) continue;
            const auto* src = g.node(l.fromNode);
            const auto* d = src ? find(src->type) : nullptr;
            const PinSpec* out = d ? d->output(l.fromPin) : nullptr;
            if (!out || out->kind != PinKind::Exec) continue;
            if (state[l.toNode] == 1) return true;
            if (state[l.toNode] == 0 && execVisit(l.toNode)) return true;
        }
        state[id] = 2;
        return false;
    };
    for (const auto& [id, n] : g.nodes())
        if (state[id] == 0 && execVisit(id))
        {
            issues.push_back({ id, "cycle d'exécution : les liens d'exécution reviennent à un nœud déjà exécuté (utiliser « Pour » ou « Tant que »)" });
            break;
        }
    return issues;
}

std::vector<Issue> NodeLibrary::warnings(const Graph& g) const
{
    if (!g.subgraphs.empty() && !m_subgraphTypes)
    {
        std::unique_ptr<NodeLibrary> holder;
        return forGraph(g, holder).warnings(g);
    }
    std::vector<Issue> out;
    auto defOf = [&](int id) -> const NodeDefinition* {
        const auto* n = g.node(id);
        return n ? find(n->type) : nullptr;
    };
    auto isExec = [&](const Link& l) {
        const auto* d = defOf(l.fromNode);
        const PinSpec* p = d ? d->output(l.fromPin) : nullptr;
        return p && p->kind == PinKind::Exec;
    };
    // Nœuds atteints par le flux à partir d'un nœud (liens d'exécution, toutes sorties)
    std::map<int, std::set<int>> reach;
    auto reachable = [&](int id) -> const std::set<int>& {
        const auto it = reach.find(id);
        if (it != reach.end()) return it->second;
        std::set<int> r;
        std::vector<int> stack { id };
        while (!stack.empty())
        {
            const int x = stack.back();
            stack.pop_back();
            for (const auto& l : g.links())
                if (l.fromNode == x && isExec(l) && r.insert(l.toNode).second) stack.push_back(l.toNode);
        }
        return reach[id] = std::move(r);
    };
    // A s'exécute avant B : B suit A dans le flux, ou A et B suivent deux sorties ordonnées d'un même nœud (corps puis terminé)
    auto before = [&](int a, int b) {
        if (reachable(a).count(b)) return true;
        for (const auto& [pid, pn] : g.nodes())
        {
            const auto* d = find(pn.type);
            if (!d || !d->orderedOutputs) continue;
            int ka = -1, kb = -1, k = 0;
            for (const auto& o : d->outputs)
            {
                if (o.kind != PinKind::Exec) continue;
                for (const auto& l : g.linksFrom(pid, o.name))
                {
                    if (ka < 0 && (l.toNode == a || reachable(l.toNode).count(a))) ka = k;
                    if (l.toNode == b || reachable(l.toNode).count(b)) kb = k;
                }
                ++k;
            }
            if (ka >= 0 && kb > ka) return true;
        }
        return false;
    };
    // Nœuds d'action qui consomment (directement ou par des nœuds purs) une valeur produite par un nœud
    std::function<void(int, std::set<int>&, std::set<int>&)> consumers = [&](int node, std::set<int>& seen, std::set<int>& actions) {
        for (const auto& l : g.links())
        {
            if (l.fromNode != node || isExec(l) || !seen.insert(l.toNode).second) continue;
            const auto* d = defOf(l.toNode);
            if (!d) continue;
            if (d->pure) consumers(l.toNode, seen, actions);
            else actions.insert(l.toNode);
        }
    };
    for (const auto& [id, n] : g.nodes())
    {
        const auto* d = find(n.type);
        if (!d || d->pure) continue;
        std::set<int> seen, actions;
        consumers(id, seen, actions);
        for (int b : actions)
        {
            if (b == id || before(id, b)) continue;
            const auto* db = defOf(b);
            out.push_back({ b, (db ? db->title : std::string("?")) + " (#" + std::to_string(b) + ") lit une sortie de « " + d->title + " » (#"
                                   + std::to_string(id) + ") qui n'est pas exécuté avant lui dans le flux" });
        }
    }
    return out;
}

void EvaluationCache::prune(const Graph& graph)
{
    for (auto it = m_entries.begin(); it != m_entries.end();)
    {
        const auto* n = graph.node(it->first);
        if (!n || n->type != it->second.type) it = m_entries.erase(it);
        else ++it;
    }
}

const NodeLibrary& NodeLibrary::standard()
{
    return global();
}

NodeLibrary& NodeLibrary::global()
{
    static NodeLibrary library = [] {
        NodeLibrary l;
        registerStandardNodes(l, TSA::Automation::CommandRegistry::builtIn());
        return l;
    }();
    return library;
}

bool NodeLibrary::usesProject(const Graph& g) const
{
    if (!g.subgraphs.empty() && !m_subgraphTypes)
    {
        std::unique_ptr<NodeLibrary> holder;
        return forGraph(g, holder).usesProject(g);
    }
    for (const auto& [id, n] : g.nodes())
        if (const auto* d = find(n.type); d && d->usesProject) return true;
    return false;
}

// -----------------------------------------------------------------------------
// Exécution
// -----------------------------------------------------------------------------

struct Runner::State
{
    std::unique_ptr<NodeLibrary> subgraphLibrary;   ///< bibliothèque augmentée des sous-graphes du graphe (s'il en a)
    const NodeLibrary& library;
    const Graph& graph;
    TSA::Project::ProjectSession* session = nullptr;
    ExecutionLimits limits;
    Debugger* debugger = nullptr;
    std::atomic<bool> stopRequested { false };
    std::map<std::string, Value> overrides;
    std::map<int, std::map<std::string, Value>> outputs;
    std::set<int> evaluating;
    EvaluationCache* cache = nullptr;      ///< réutilisation d'une exécution à l'autre (éditeur)
    EvaluationCache localCache;            ///< à défaut, réutilisation dans cette exécution
    std::map<int, std::map<std::string, Value>> prefetched;   ///< entrées déjà évaluées d'un nœud réutilisable en cours d'exécution
    std::map<std::string, Value> variables;
    ExecutionReport report;
    bool failed = false;
    double childMs = 0.0;   ///< temps des nœuds descendants (profil exclusif)

    State(const NodeLibrary& lib, const Graph& g, TSA::Project::ProjectSession* s, ExecutionLimits l)
        : library(lib.forGraph(g, subgraphLibrary)), graph(g), session(s), limits(l) {}

    bool fail(int node, const std::string& message)
    {
        if (!failed)
        {
            failed = true;
            report.failedNode = node;
            const auto* n = graph.node(node);
            const auto* d = n ? library.find(n->type) : nullptr;
            report.message = (d ? d->title + " (#" + std::to_string(node) + ") : " : std::string()) + message;
        }
        return false;
    }

    /// Exécute un nœud (action ou pur) ; profil en temps exclusif.
    bool run(int id)
    {
        if (failed) return false;
        if (++report.steps > limits.maxSteps) return fail(id, "nombre maximal d'étapes atteint (" + std::to_string(limits.maxSteps) + ")");
        if (stopRequested.load()) return fail(id, "exécution arrêtée par l'utilisateur");
        const NodeInstance* n = graph.node(id);
        const NodeDefinition* def = n ? library.find(n->type) : nullptr;
        const NodeExecutor* exec = n ? library.executor(n->type) : nullptr;
        if (!n || !def || !exec) return fail(id, "nœud ou type inconnu");
        if (debugger && !def->pure)
        {
            DebugState ds;
            ds.node = id;
            ds.step = report.steps - 1;
            ds.outputs = &outputs;
            ds.log = &report.log;
            if (debugger->beforeNode(ds) == DebugAction::Abort) return fail(id, "exécution interrompue (débogueur)");
        }

        const auto t0 = Clock::now();
        const double savedChild = childMs;
        childMs = 0.0;
        ExecutionContext ctx(*this, *n, *def);
        const bool ok = (*exec)(ctx) && !failed;
        if (!ok && !failed) fail(id, "échec");
        const double elapsed = msSince(t0);
        auto& st = report.profile[id];
        st.executions += 1;
        st.milliseconds += std::max(0.0, elapsed - childMs);
        childMs = savedChild + elapsed;
        return ok;
    }

    /// Nœud pur réutilisable : entrées évaluées puis comparées à la dernière évaluation (même type, mêmes entrées →
    /// sorties réutilisées) ; sinon exécution avec ces entrées déjà évaluées.
    bool evaluateCacheable(int node, const NodeInstance& n, const NodeDefinition& def)
    {
        std::map<std::string, Value> inputs;
        std::vector<Value> fingerprint;
        for (const auto& p : def.inputs)
        {
            if (p.kind != PinKind::Data) continue;
            Value v;
            if (const auto link = graph.linkTo(node, p.name))
            {
                if (!read(link->fromNode, link->fromPin, v)) return false;
            }
            else if (const auto it = n.values.find(p.name); it != n.values.end() && !isUnset(it->second))
                v = it->second;
            else
                v = p.defaultValue;
            fingerprint.push_back(v);
            inputs[p.name] = std::move(v);
        }
        EvaluationCache& c = cache ? *cache : localCache;
        const auto hit = c.m_entries.find(node);
        auto sameRevision = [&](const std::vector<std::shared_ptr<const void>>& r) {
            return r.size() == def.revision.size()
                   && std::equal(r.begin(), r.end(), def.revision.begin(), [](const auto& a, const auto& b) { return a.get() == b.get(); });
        };
        if (hit != c.m_entries.end() && hit->second.type == n.type && sameRevision(hit->second.revision) && hit->second.inputs.size() == fingerprint.size()
            && std::equal(fingerprint.begin(), fingerprint.end(), hit->second.inputs.begin(),
                          [](const Value& a, const Value& b) { return TSA::Automation::sameValue(a, b); }))
        {
            outputs[node] = hit->second.outputs;
            report.profile[node].reused += 1;
            report.reused += 1;
            return true;
        }
        prefetched[node] = std::move(inputs);
        const bool ok = run(node);
        prefetched.erase(node);
        if (!ok) return false;
        c.m_entries[node] = { n.type, def.revision, std::move(fingerprint), outputs[node] };
        return true;
    }

    /// Valeur d'une sortie d'un nœud source : nœud pur réévalué, nœud d'action lu dans sa dernière exécution.
    bool read(int node, const std::string& pin, Value& out)
    {
        const NodeInstance* n = graph.node(node);
        const NodeDefinition* def = n ? library.find(n->type) : nullptr;
        if (!def) return fail(node, "source inconnue");
        if (def->pure)
        {
            if (evaluating.count(node)) return fail(node, "cycle de données");
            evaluating.insert(node);
            const bool ok = def->cacheable ? evaluateCacheable(node, *n, *def) : run(node);
            evaluating.erase(node);
            if (!ok) return false;
        }
        const auto it = outputs.find(node);
        if (it == outputs.end() || !it->second.count(pin))
            return fail(node, "sortie « " + pin + " » non disponible (nœud d'action pas encore exécuté)");
        out = it->second.at(pin);
        return true;
    }
};

Runner::Runner(const NodeLibrary& library, const Graph& graph, TSA::Project::ProjectSession* session, ExecutionLimits limits)
    : m_state(std::make_unique<State>(library, graph, session, limits))
{
}

Runner::~Runner() = default;

void Runner::setDebugger(Debugger* debugger)
{
    m_state->debugger = debugger;
}

void Runner::requestStop()
{
    m_state->stopRequested = true;
}

void Runner::setCache(EvaluationCache* cache)
{
    m_state->cache = cache;
}

DebugAction BreakpointDebugger::beforeNode(const DebugState& state)
{
    if (!m_stepping && !m_breakpoints.count(state.node)) return DebugAction::Continue;
    const DebugAction a = m_onPause ? m_onPause(state) : DebugAction::Continue;
    m_stepping = a == DebugAction::Step;
    return a;
}

ExecutionReport Runner::run(const std::map<std::string, Value>& parameterOverrides)
{
    State& s = *m_state;
    s.overrides = parameterOverrides;
    s.outputs.clear();
    s.variables.clear();
    s.prefetched.clear();
    s.localCache.clear();
    s.report = {};
    s.failed = false;
    s.stopRequested = false;
    const auto t0 = Clock::now();

    const auto issues = s.library.validate(s.graph);
    if (!issues.empty())
    {
        s.report.failedNode = issues.front().node;
        s.report.message = "Blueprint invalide : " + issues.front().message
                           + (issues.size() > 1 ? " (+" + std::to_string(issues.size() - 1) + " autre(s))" : "");
        return s.report;
    }
    std::vector<int> starts;   // « Début » et entrées de sous-graphe
    for (const auto& [id, n] : s.graph.nodes())
        if (const auto* d = s.library.find(n.type); d && d->entry) starts.push_back(id);
    if (starts.empty())
    {
        s.report.message = "Aucun nœud « Début » : rien à exécuter.";
        return s.report;
    }
    for (int id : starts)
        if (!s.run(id)) break;
    s.report.ok = !s.failed;
    if (s.report.ok)
        s.report.message = "Exécution terminée (" + std::to_string(s.report.steps) + " étape(s)"
                           + (s.report.reused ? ", " + std::to_string(s.report.reused) + " résultat(s) réutilisé(s)" : std::string()) + ").";
    s.report.milliseconds = msSince(t0);
    s.report.values = s.outputs;        // valeurs intermédiaires, y compris après une erreur (débogage)
    s.report.variables = s.variables;
    return s.report;
}

// -----------------------------------------------------------------------------
// ExecutionContext
// -----------------------------------------------------------------------------

Value ExecutionContext::input(const std::string& pin)
{
    const PinSpec* spec = m_def.input(pin);
    if (!spec || spec->kind != PinKind::Data)
    {
        m_state.fail(m_node.id, "entrée « " + pin + " » inexistante");
        return {};
    }
    Value v;
    const auto pre = m_state.prefetched.find(m_node.id);
    if (pre != m_state.prefetched.end() && pre->second.count(pin))
        v = pre->second.at(pin);   // entrée déjà évaluée (nœud réutilisable)
    else if (const auto link = m_state.graph.linkTo(m_node.id, pin))
    {
        if (!m_state.read(link->fromNode, link->fromPin, v)) return {};
    }
    else if (const auto it = m_node.values.find(pin); it != m_node.values.end() && !isUnset(it->second))
        v = it->second;
    else
        v = spec->defaultValue;

    if (spec->any || isUnset(v)) return v;
    if (spec->type == ValueType::Object)
    {
        const auto* o = std::get_if<ObjectRef>(&v);
        if (!o || !*o)
        {
            m_state.fail(m_node.id, "entrée « " + spec->label + " » : objet " + pinTypeName(*spec) + " attendu, " + toText(v) + " reçu");
            return {};
        }
        if (!spec->objectType.empty() && (*o)->typeName() != spec->objectType)
        {
            m_state.fail(m_node.id, "entrée « " + spec->label + " » : objet " + spec->objectType + " attendu, " + (*o)->typeName() + " reçu");
            return {};
        }
        return v;
    }
    if (spec->type == ValueType::Real && std::holds_alternative<long long>(v)) return static_cast<double>(std::get<long long>(v));
    if (spec->type == ValueType::IdList && std::holds_alternative<long long>(v))
        return std::vector<int> { static_cast<int>(std::get<long long>(v)) };
    return v;
}

double ExecutionContext::real(const std::string& pin)
{
    const Value v = input(pin);
    if (const auto* d = std::get_if<double>(&v)) return *d;
    if (const auto* i = std::get_if<long long>(&v)) return static_cast<double>(*i);
    if (!failed()) fail("entrée « " + pin + " » : réel attendu");
    return 0.0;
}

long long ExecutionContext::integer(const std::string& pin)
{
    const Value v = input(pin);
    if (const auto* i = std::get_if<long long>(&v)) return *i;
    if (!failed()) fail("entrée « " + pin + " » : entier attendu");
    return 0;
}

bool ExecutionContext::boolean(const std::string& pin)
{
    const Value v = input(pin);
    if (const auto* b = std::get_if<bool>(&v)) return *b;
    if (!failed()) fail("entrée « " + pin + " » : booléen attendu");
    return false;
}

std::string ExecutionContext::text(const std::string& pin)
{
    const Value v = input(pin);
    if (const auto* t = std::get_if<std::string>(&v)) return *t;
    return toText(v);
}

const NodeLibrary& ExecutionContext::library() const
{
    return m_state.library;
}

bool ExecutionContext::isConnected(const std::string& pin) const
{
    return m_state.graph.linkTo(m_node.id, pin).has_value();
}

ObjectRef ExecutionContext::object(const std::string& pin)
{
    const Value v = input(pin);
    if (failed()) return nullptr;
    if (const auto* o = std::get_if<ObjectRef>(&v); o && *o) return *o;
    fail("entrée « " + pin + " » : objet attendu (relier une sortie de type objet)");
    return nullptr;
}

bool ExecutionContext::hasVariable(const std::string& name) const
{
    return m_state.variables.count(name) > 0;
}

Value ExecutionContext::variable(const std::string& name) const
{
    const auto it = m_state.variables.find(name);
    return it == m_state.variables.end() ? Value {} : it->second;
}

void ExecutionContext::setVariable(const std::string& name, const Value& value)
{
    m_state.variables[name] = value;
}

bool ExecutionContext::hasValue(const std::string& pin) const
{
    if (isConnected(pin)) return true;
    const auto it = m_node.values.find(pin);
    return it != m_node.values.end() && !isUnset(it->second);
}

void ExecutionContext::setOutput(const std::string& pin, const Value& value)
{
    m_state.outputs[m_node.id][pin] = value;
}

bool ExecutionContext::fire(const std::string& pin)
{
    if (m_state.failed) return false;
    for (const auto& l : m_state.graph.linksFrom(m_node.id, pin))
        if (!m_state.run(l.toNode)) return false;
    return !m_state.failed;
}

bool ExecutionContext::fail(const std::string& message)
{
    return m_state.fail(m_node.id, message);
}

void ExecutionContext::log(const std::string& line)
{
    m_state.report.log.push_back(line);
}

TSA::Project::ProjectSession* ExecutionContext::session() const
{
    return m_state.session;
}

int ExecutionContext::maxIterations() const
{
    return m_state.limits.maxIterations;
}

bool ExecutionContext::failed() const
{
    return m_state.failed;
}

const Value* ExecutionContext::parameterOverride(const std::string& name) const
{
    const auto it = m_state.overrides.find(name);
    return it == m_state.overrides.end() ? nullptr : &it->second;
}

} // namespace TSA::Blueprint
