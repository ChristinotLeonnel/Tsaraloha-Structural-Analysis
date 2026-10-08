#include "BlueprintRuntime.h"

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
    if (definition.id.empty() || !executor || m_index.count(definition.id)) return false;
    m_index[definition.id] = m_entries.size();
    m_entries.push_back({ std::move(definition), std::move(executor) });
    return true;
}

const NodeDefinition* NodeLibrary::find(const std::string& type) const
{
    const auto it = m_index.find(type);
    return it == m_index.end() ? nullptr : &m_entries[it->second].definition;
}

const NodeExecutor* NodeLibrary::executor(const std::string& type) const
{
    const auto it = m_index.find(type);
    return it == m_index.end() ? nullptr : &m_entries[it->second].executor;
}

std::vector<const NodeDefinition*> NodeLibrary::definitions() const
{
    std::vector<const NodeDefinition*> list;
    for (const auto& e : m_entries) list.push_back(&e.definition);
    return list;
}

bool NodeLibrary::canConnect(const Graph& g, const Link& link, std::string* why) const
{
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
        return fail(std::string("types incompatibles : ") + TSA::Automation::typeName(out->type) + " → "
                    + TSA::Automation::typeName(in->type));
    return true;
}

bool NodeLibrary::connect(Graph& g, const Link& link, std::string* why) const
{
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
    std::vector<Issue> issues;
    for (const auto& [id, n] : g.nodes())
    {
        const auto* def = find(n.type);
        if (!def)
        {
            issues.push_back({ id, "type de nœud inconnu « " + n.type + " »" });
            continue;
        }
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
    return issues;
}

const NodeLibrary& NodeLibrary::standard()
{
    static const NodeLibrary library = [] {
        NodeLibrary l;
        registerStandardNodes(l, TSA::Automation::CommandRegistry::builtIn());
        return l;
    }();
    return library;
}

// -----------------------------------------------------------------------------
// Exécution
// -----------------------------------------------------------------------------

struct Runner::State
{
    const NodeLibrary& library;
    const Graph& graph;
    TSA::Project::ProjectSession* session = nullptr;
    ExecutionLimits limits;
    std::map<std::string, Value> overrides;
    std::map<int, std::map<std::string, Value>> outputs;
    std::set<int> evaluating;
    ExecutionReport report;
    bool failed = false;
    double childMs = 0.0;   ///< temps des nœuds descendants (profil exclusif)

    State(const NodeLibrary& lib, const Graph& g, TSA::Project::ProjectSession* s, ExecutionLimits l)
        : library(lib), graph(g), session(s), limits(l) {}

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
        const NodeInstance* n = graph.node(id);
        const NodeDefinition* def = n ? library.find(n->type) : nullptr;
        const NodeExecutor* exec = n ? library.executor(n->type) : nullptr;
        if (!n || !def || !exec) return fail(id, "nœud ou type inconnu");

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
            const bool ok = run(node);
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

ExecutionReport Runner::run(const std::map<std::string, Value>& parameterOverrides)
{
    State& s = *m_state;
    s.overrides = parameterOverrides;
    s.outputs.clear();
    s.report = {};
    s.failed = false;
    const auto t0 = Clock::now();

    const auto issues = s.library.validate(s.graph);
    if (!issues.empty())
    {
        s.report.failedNode = issues.front().node;
        s.report.message = "Blueprint invalide : " + issues.front().message
                           + (issues.size() > 1 ? " (+" + std::to_string(issues.size() - 1) + " autre(s))" : "");
        return s.report;
    }
    std::vector<int> starts;
    for (const auto& [id, n] : s.graph.nodes())
        if (n.type == "event.start") starts.push_back(id);
    if (starts.empty())
    {
        s.report.message = "Aucun nœud « Début » : rien à exécuter.";
        return s.report;
    }
    for (int id : starts)
        if (!s.run(id)) break;
    s.report.ok = !s.failed;
    if (s.report.ok) s.report.message = "Exécution terminée (" + std::to_string(s.report.steps) + " étape(s)).";
    s.report.milliseconds = msSince(t0);
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
    if (const auto link = m_state.graph.linkTo(m_node.id, pin))
    {
        if (!m_state.read(link->fromNode, link->fromPin, v)) return {};
    }
    else if (const auto it = m_node.values.find(pin); it != m_node.values.end() && !isUnset(it->second))
        v = it->second;
    else
        v = spec->defaultValue;

    if (spec->any || isUnset(v)) return v;
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

bool ExecutionContext::isConnected(const std::string& pin) const
{
    return m_state.graph.linkTo(m_node.id, pin).has_value();
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
