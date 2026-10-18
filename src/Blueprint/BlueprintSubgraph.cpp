// Sous-graphes réutilisables : types d'instance, d'entrée et de sortie, exécution imbriquée, contrôles, encapsulation
// (voir BlueprintSubgraph.h).

#include "BlueprintSubgraph.h"

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cctype>
#include <deque>
#include <functional>
#include <map>

// Traduction : messages et textes composés des types de sous-graphe dans le contexte « TSA::Blueprint::Subgraph ».
// Les noms, identifiants, catégories et descriptions saisis par l'utilisateur (écrits dans les fichiers) restent tels quels.

namespace TSA::Blueprint
{

namespace
{
constexpr char kPrefix[] = "sub.";
constexpr char kArgument[] = "__sub.in.";   ///< paramètres de l'exécution imbriquée : valeurs des entrées

bool isUnset(const Value& v) { return std::holds_alternative<std::monostate>(v); }

PinSpec execInPin()
{
    PinSpec p;
    p.name = "exec";
    p.kind = PinKind::Exec;
    return p;
}

PinSpec execOutPin()
{
    PinSpec p;
    p.name = "then";
    p.kind = PinKind::Exec;
    return p;
}

std::string titleOf(const SubgraphDefinition& d)
{
    return d.name.empty() ? d.id : d.name;
}

/// Caractère du corps : pur (aucune action), réutilisable, accès au projet — sous-graphes imbriqués compris.
struct BodyTraits
{
    bool pure = true, cacheable = true, usesProject = false;
};

BodyTraits traits(const SubgraphDefinition& d, const NodeLibrary& base, const std::vector<SubgraphDefinition>& set, std::set<std::string>& visiting)
{
    BodyTraits t;
    if (!d.body || !visiting.insert(d.id).second) return { false, false, false };   // corps absent ou récursion (refusée par la validation)
    for (const auto& [id, n] : d.body->nodes())
    {
        if (n.type == subgraphInputType(d.id) || n.type == subgraphOutputType(d.id)) continue;
        const std::string sub = subgraphOfType(n.type);
        if (!sub.empty())
        {
            const auto it = std::find_if(set.begin(), set.end(), [&](const SubgraphDefinition& x) { return x.id == sub; });
            if (it == set.end())
            {
                t.pure = t.cacheable = false;
                continue;
            }
            const BodyTraits inner = traits(*it, base, set, visiting);
            t.pure = t.pure && inner.pure;
            t.cacheable = t.cacheable && inner.cacheable;
            t.usesProject = t.usesProject || inner.usesProject;
            continue;
        }
        const NodeDefinition* nd = base.find(n.type);
        if (!nd)
        {
            t.pure = t.cacheable = false;
            continue;
        }
        t.pure = t.pure && nd->pure;
        t.cacheable = t.cacheable && nd->pure && nd->cacheable;
        t.usesProject = t.usesProject || nd->usesProject;
    }
    visiting.erase(d.id);
    return t;
}

std::string pinList(const std::vector<PinSpec>& pins)
{
    if (pins.empty()) return QCoreApplication::translate("TSA::Blueprint::Subgraph", "aucune").toStdString();
    std::string s;
    for (const auto& p : pins)
    {
        const QString label = QString::fromStdString(p.label.empty() ? p.name : displayText(p.label)), type = QString::fromStdString(pinTypeName(p));
        s += (s.empty() ? "" : ", ")
             + (p.required ? QCoreApplication::translate("TSA::Blueprint::Subgraph", "%1 (%2, obligatoire)").arg(label, type) : QCoreApplication::translate("TSA::Blueprint::Subgraph", "%1 (%2)").arg(label, type)).toStdString();
    }
    return s;
}

/// Décrément garanti de la profondeur d'imbrication.
struct DepthGuard
{
    int& depth;
    explicit DepthGuard(int& d) : depth(d) { ++depth; }
    ~DepthGuard() { --depth; }
};

int& callDepth()
{
    thread_local int depth = 0;
    return depth;
}

/// Corps de la définition et des sous-graphes qu'elle appelle, directement ou non (par identifiant) : révision du type.
void collectBodies(const SubgraphDefinition& d, const std::vector<SubgraphDefinition>& set, std::map<std::string, std::shared_ptr<const void>>& out)
{
    if (!d.body || out.count(d.id)) return;
    out[d.id] = d.body;
    for (const auto& [id, n] : d.body->nodes())
        if (const std::string sub = subgraphOfType(n.type); !sub.empty())
            for (const auto& x : set)
                if (x.id == sub) collectBodies(x, set, out);
}

/// Types d'un sous-graphe dans la bibliothèque augmentée `self` (les exécuteurs la référencent : ils vivent en elle).
void registerTypes(NodeLibrary& lib, const NodeLibrary& base, const std::shared_ptr<const std::vector<SubgraphDefinition>>& set, std::size_t k)
{
    const SubgraphDefinition& d = (*set)[k];
    const NodeLibrary* self = &lib;
    std::set<std::string> visiting;
    const BodyTraits t = traits(d, base, *set, visiting);

    // Instance
    NodeDefinition inst;
    inst.id = subgraphInstanceType(d.id);
    inst.title = titleOf(d);
    inst.category = d.category.empty() ? QT_TRANSLATE_NOOP("TSA::Blueprint::Subgraph", "Sous-graphes") : d.category;   // texte source (clé de catégorie)
    inst.description = d.description.empty() ? QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sous-graphe « %1 ».").arg(QString::fromStdString(titleOf(d))).toStdString() : d.description;
    inst.pure = t.pure;
    inst.cacheable = t.pure && t.cacheable;
    inst.usesProject = t.usesProject;
    inst.status = NodeStatus::Standard;
    inst.version = d.version;
    if (!t.pure) inst.inputs.push_back(execInPin());
    for (PinSpec p : d.inputs)
    {
        p.kind = PinKind::Data;
        inst.inputs.push_back(p);
    }
    if (!t.pure) inst.outputs.push_back(execOutPin());
    for (PinSpec p : d.outputs)
    {
        p.kind = PinKind::Data;
        p.required = false;
        inst.outputs.push_back(p);
    }
    {
        const QString summary =
            t.pure ? QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sous-graphe « %1 », version %2 — pur (évalué à la demande).\nEntrées : %3.\nSorties : %4.\nCorps : %5 "
                                                         "nœud(s), exécuté dans une exécution imbriquée ; son journal est repris, préfixé de son "
                                                         "nom ; une erreur du corps fait échouer l'instance.")
                   : QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sous-graphe « %1 », version %2 — d'action (broches d'exécution).\nEntrées : %3.\nSorties : %4.\n"
                                                         "Corps : %5 nœud(s), exécuté dans une exécution imbriquée ; son journal est repris, "
                                                         "préfixé de son nom ; une erreur du corps fait échouer l'instance.");
        inst.documentation = (d.documentation.empty() ? std::string() : d.documentation + "\n\n")
                             + summary
                                   .arg(QString::fromStdString(d.id), QString::number(d.version), QString::fromStdString(pinList(d.inputs)), QString::fromStdString(pinList(d.outputs)),
                                        QString::number(d.body ? d.body->nodes().size() : 0))
                                   .toStdString();
    }
    inst.source = QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » (défini dans le graphe)").arg(QString::fromStdString(d.id)).toStdString();
    std::map<std::string, std::shared_ptr<const void>> bodies;
    collectBodies(d, *set, bodies);
    for (auto& [id, body] : bodies) inst.revision.push_back(std::move(body));
    const bool pure = t.pure;
    lib.add(std::move(inst), [self, set, k, pure](ExecutionContext& c) {
        const SubgraphDefinition& def = (*set)[k];
        if (callDepth() >= kMaxSubgraphDepth)
            return c.fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphes imbriqués sur plus de %1 niveaux (appel récursif ?)").arg(kMaxSubgraphDepth).toStdString());
        if (!def.body) return c.fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » sans corps").arg(QString::fromStdString(titleOf(def))).toStdString());
        std::map<std::string, Value> arguments;
        for (const auto& p : def.inputs)
        {
            Value v = c.input(p.name);
            if (c.failed()) return false;
            arguments[kArgument + p.name] = std::move(v);
        }
        ExecutionReport rep;
        {
            DepthGuard guard(callDepth());
            Runner inner(*self, *def.body, c.session());
            rep = inner.run(arguments);
        }
        for (const auto& line : rep.log) c.log("[" + titleOf(def) + "] " + line);
        if (!rep.ok)
            return c.fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : %2").arg(QString::fromStdString(titleOf(def)), QString::fromStdString(rep.message)).toStdString());
        int outNode = 0;
        for (const auto& [id, n] : def.body->nodes())
            if (n.type == subgraphOutputType(def.id)) outNode = id;
        const auto it = rep.values.find(outNode);
        for (const auto& p : def.outputs)
        {
            if (it == rep.values.end() || !it->second.count(p.name))
                return c.fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : sortie « %2 » non produite (la fin du corps n'a pas été atteinte)")
                                  .arg(QString::fromStdString(titleOf(def)), QString::fromStdString(p.label.empty() ? p.name : displayText(p.label)))
                                  .toStdString());
            c.setOutput(p.name, it->second.at(p.name));
        }
        return pure ? true : c.fire("then");
    });

    // Entrée du corps : point de départ, sorties = entrées du sous-graphe
    NodeDefinition in;
    in.id = subgraphInputType(d.id);
    in.title = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Entrées — %1").arg(QString::fromStdString(titleOf(d))).toStdString();
    in.category = QT_TRANSLATE_NOOP("TSA::Blueprint::Subgraph", "Sous-graphe (corps)");   // texte source (clé de catégorie)
    in.description = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Point de départ du corps du sous-graphe « %1 » : valeurs des entrées de l'instance.")
                         .arg(QString::fromStdString(titleOf(d)))
                         .toStdString();
    in.entry = true;
    in.status = NodeStatus::Standard;
    in.version = d.version;
    in.outputs.push_back(execOutPin());
    for (PinSpec p : d.inputs)
    {
        p.kind = PinKind::Data;
        p.required = false;
        in.outputs.push_back(p);
    }
    in.documentation = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Valeurs des entrées passées par l'instance. Corps exécuté seul (essai) : valeurs par "
                                                   "défaut des entrées ; une entrée obligatoire sans valeur par défaut fait alors "
                                                   "échouer l'exécution.")
                           .toStdString();
    in.source = QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » (défini dans le graphe)").arg(QString::fromStdString(d.id)).toStdString();
    lib.add(std::move(in), [set, k](ExecutionContext& c) {
        const SubgraphDefinition& def = (*set)[k];
        for (const auto& p : def.inputs)
        {
            if (const Value* v = c.parameterOverride(kArgument + p.name))
                c.setOutput(p.name, *v);
            else if (!isUnset(p.defaultValue))
                c.setOutput(p.name, p.defaultValue);
            else if (p.required)
                return c.fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "entrée « %1 » du sous-graphe non fournie (exécution du corps seul : donner une valeur "
                                                                "par défaut)")
                                  .arg(QString::fromStdString(p.label.empty() ? p.name : displayText(p.label)))
                                  .toStdString());
            else
                c.setOutput(p.name, Value {});
        }
        return c.fire("then");
    });

    // Sortie du corps : fin, entrées = sorties du sous-graphe (reprises en sorties, lues par l'instance)
    NodeDefinition out;
    out.id = subgraphOutputType(d.id);
    out.title = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sorties — %1").arg(QString::fromStdString(titleOf(d))).toStdString();
    out.category = QT_TRANSLATE_NOOP("TSA::Blueprint::Subgraph", "Sous-graphe (corps)");   // texte source (clé de catégorie)
    out.description = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Fin du corps du sous-graphe « %1 » : valeurs rendues par l'instance.")
                          .arg(QString::fromStdString(titleOf(d)))
                          .toStdString();
    out.status = NodeStatus::Standard;
    out.version = d.version;
    out.inputs.push_back(execInPin());
    for (PinSpec p : d.outputs)
    {
        p.kind = PinKind::Data;
        p.required = true;
        out.inputs.push_back(p);
        p.required = false;
        out.outputs.push_back(p);
    }
    out.documentation = QCoreApplication::translate("TSA::Blueprint::Subgraph", "Chaque entrée est obligatoire : c'est une sortie du sous-graphe. Le nœud doit être "
                                                    "atteint par les liens d'exécution depuis l'entrée du corps.")
                            .toStdString();
    out.source = QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » (défini dans le graphe)").arg(QString::fromStdString(d.id)).toStdString();
    lib.add(std::move(out), [set, k](ExecutionContext& c) {
        for (const auto& p : (*set)[k].outputs)
        {
            Value v = c.input(p.name);
            if (c.failed()) return false;
            c.setOutput(p.name, v);
        }
        return true;
    });
}

/// Structure d'un corps : un seul nœud d'entrée et un seul nœud de sortie, ni « Début » ni nœud d'entrée / de sortie d'un
/// autre sous-graphe, fin atteinte depuis l'entrée par les liens d'exécution. Faux (*complete) si l'entrée ou la sortie
/// manque ou est en double : les contrôles du contenu n'ont alors pas de sens.
std::vector<std::string> structureIssues(const NodeLibrary& library, const SubgraphDefinition& d, bool* complete)
{
    std::vector<std::string> out;
    const QString label = QString::fromStdString(titleOf(d));
    int in = 0, out_ = 0, ins = 0, outs = 0;
    for (const auto& [id, n] : d.body->nodes())
    {
        if (n.type == subgraphInputType(d.id)) in = id, ++ins;
        else if (n.type == subgraphOutputType(d.id)) out_ = id, ++outs;
        else if (n.type == "event.start")
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : nœud « Début » interdit dans un corps (le point de départ est "
                                                              "l'entrée du sous-graphe)")
                              .arg(label)
                              .toStdString());
        else if (n.type.rfind(kPrefix, 0) == 0 && subgraphOfType(n.type).empty())
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : nœud d'entrée ou de sortie d'un autre sous-graphe (%2)")
                              .arg(label, QString::fromStdString(n.type))
                              .toStdString());
    }
    if (complete) *complete = ins == 1 && outs == 1;
    if (ins != 1 || outs != 1)
    {
        out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : le corps doit contenir un nœud d'entrée et un nœud de sortie (%2 et %3)")
                          .arg(label, QString::number(ins), QString::number(outs))
                          .toStdString());
        return out;
    }
    std::set<int> reached { in };
    std::deque<int> todo { in };
    while (!todo.empty())
    {
        const int x = todo.front();
        todo.pop_front();
        const NodeInstance* nx = d.body->node(x);
        const NodeDefinition* dx = nx ? library.find(nx->type) : nullptr;
        if (!dx) continue;
        for (const auto& l : d.body->links())
        {
            if (l.fromNode != x) continue;
            const PinSpec* p = dx->output(l.fromPin);
            if (p && p->kind == PinKind::Exec && reached.insert(l.toNode).second) todo.push_back(l.toNode);
        }
    }
    if (!reached.count(out_))
        out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : la fin du corps n'est pas atteinte depuis son entrée (relier les liens "
                                                          "d'exécution)")
                          .arg(label)
                          .toStdString());
    return out;
}

/// Premier appel récursif (direct ou indirect) entre les définitions ; vide s'il n'y en a pas.
std::string recursionIssue(const std::vector<SubgraphDefinition>& set)
{
    std::map<std::string, std::set<std::string>> calls;   // sous-graphe → sous-graphes appelés par son corps
    for (const auto& d : set)
        if (d.body)
            for (const auto& [id, n] : d.body->nodes())
                if (const std::string sub = subgraphOfType(n.type); !sub.empty()) calls[d.id].insert(sub);
    std::map<std::string, int> color;
    std::vector<std::string> path;
    std::string found;
    std::function<bool(const std::string&)> visit = [&](const std::string& id) -> bool {
        color[id] = 1;
        path.push_back(id);
        for (const auto& callee : calls[id])
        {
            if (color[callee] == 1)
            {
                std::string cycle;
                bool on = false;
                for (const auto& p : path)
                    if (on || p == callee) on = true, cycle += p + " → ";
                found = QCoreApplication::translate("TSA::Blueprint::Subgraph", "appel récursif de sous-graphes : %1").arg(QString::fromStdString(cycle + callee)).toStdString();
                return true;
            }
            if (color[callee] == 0 && visit(callee)) return true;
        }
        path.pop_back();
        color[id] = 2;
        return false;
    };
    for (const auto& d : set)
        if (color[d.id] == 0 && visit(d.id)) break;
    return found;
}

std::string sanitizedName(const std::string& raw, std::set<std::string>& used)
{
    std::string s;
    for (char ch : raw)
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') s += ch;
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) s = "v" + s;
    if (s == "exec" || s == "then") s += "_";
    std::string name = s;
    for (int k = 2; used.count(name); ++k) name = s + "_" + std::to_string(k);
    used.insert(name);
    return name;
}
} // namespace

// -----------------------------------------------------------------------------
// Identifiants
// -----------------------------------------------------------------------------

std::string subgraphInstanceType(const std::string& id) { return kPrefix + id; }
std::string subgraphInputType(const std::string& id) { return kPrefix + id + ".in"; }
std::string subgraphOutputType(const std::string& id) { return kPrefix + id + ".out"; }

std::string subgraphOfType(const std::string& type)
{
    if (type.rfind(kPrefix, 0) != 0) return {};
    const std::string id = type.substr(sizeof(kPrefix) - 1);
    return isValidSubgraphId(id) ? id : std::string();   // « sub.x.in » : « x.in » n'est pas un identifiant
}

bool isValidSubgraphId(const std::string& id)
{
    if (id.empty() || id.size() > 64 || !std::isalpha(static_cast<unsigned char>(id[0]))) return false;
    return std::all_of(id.begin(), id.end(), [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-'; });
}

bool isValidPinName(const std::string& name)
{
    if (name.empty() || name.size() > 64 || !std::isalpha(static_cast<unsigned char>(name[0])) || name == "exec" || name == "then") return false;
    return std::all_of(name.begin(), name.end(), [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_'; });
}

const char* valueTypeId(ValueType t)
{
    switch (t)
    {
    case ValueType::Bool: return "bool";
    case ValueType::Integer: return "integer";
    case ValueType::Real: return "real";
    case ValueType::Text: return "text";
    case ValueType::Point3: return "point";
    case ValueType::IdList: return "ids";
    case ValueType::Object: return "object";
    }
    return "real";
}

bool valueTypeFromId(const std::string& id, ValueType& out)
{
    static const std::map<std::string, ValueType> table = { { "bool", ValueType::Bool },   { "integer", ValueType::Integer }, { "real", ValueType::Real },
                                                            { "text", ValueType::Text },   { "point", ValueType::Point3 },    { "ids", ValueType::IdList },
                                                            { "object", ValueType::Object } };
    const auto it = table.find(id);
    if (it == table.end()) return false;
    out = it->second;
    return true;
}

std::shared_ptr<const Graph> makeSubgraphBody(const std::string& id)
{
    auto body = std::make_shared<Graph>();
    const int in = body->addNode(subgraphInputType(id), 0.0, 0.0);
    const int out = body->addNode(subgraphOutputType(id), 500.0, 0.0);
    body->addLink({ in, "then", out, "exec" });
    return body;
}

// -----------------------------------------------------------------------------
// Bibliothèque augmentée (NodeLibrary::withSubgraphs)
// -----------------------------------------------------------------------------

std::unique_ptr<NodeLibrary> NodeLibrary::withSubgraphs(const NodeLibrary& base, const std::vector<SubgraphDefinition>& subgraphs)
{
    auto lib = std::make_unique<NodeLibrary>(&base);
    lib->setSubgraphs(subgraphs);
    return lib;
}

void NodeLibrary::setSubgraphs(const std::vector<SubgraphDefinition>& subgraphs)
{
    m_entries.clear();
    m_index.clear();
    m_subgraphTypes = true;
    if (!m_base) return;   // réservé aux bibliothèques augmentées (NodeLibrary(base))
    const auto set = std::make_shared<const std::vector<SubgraphDefinition>>(subgraphs);
    std::set<std::string> seen;
    for (std::size_t k = 0; k < set->size(); ++k)
        if (isValidSubgraphId((*set)[k].id) && seen.insert((*set)[k].id).second) registerTypes(*this, *m_base, set, k);
}

// -----------------------------------------------------------------------------
// Contrôles
// -----------------------------------------------------------------------------

std::vector<std::string> validateSubgraphs(const NodeLibrary& library, const std::vector<SubgraphDefinition>& set)
{
    std::vector<std::string> out;
    std::set<std::string> ids;
    for (const auto& d : set)
    {
        const QString label = QString::fromStdString(titleOf(d));
        if (!isValidSubgraphId(d.id))
        {
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : identifiant invalide « %2 » (lettre puis lettres, chiffres, _ ou -, 64 "
                                                              "caractères au plus)")
                              .arg(label, QString::fromStdString(d.id))
                              .toStdString());
            continue;
        }
        if (!ids.insert(d.id).second)
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "identifiant de sous-graphe en double « %1 »").arg(QString::fromStdString(d.id)).toStdString());
        if (d.version < 1)
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : version %2 invalide").arg(label, QString::number(d.version)).toStdString());
        for (const auto* pins : { &d.inputs, &d.outputs })
        {
            std::set<std::string> names;
            for (const auto& p : *pins)
            {
                if (!isValidPinName(p.name))
                    out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : nom de broche invalide « %2 »").arg(label, QString::fromStdString(p.name)).toStdString());
                if (!names.insert(p.name).second)
                    out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : broche « %2 » en double").arg(label, QString::fromStdString(p.name)).toStdString());
                if (p.kind != PinKind::Data)
                    out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : broche « %2 » : seules les broches de données sont configurables")
                                      .arg(label, QString::fromStdString(p.name))
                                      .toStdString());
            }
        }
        if (!d.body)
        {
            out.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : corps absent").arg(label).toStdString());
            continue;
        }
        bool complete = false;
        for (auto& m : structureIssues(library, d, &complete)) out.push_back(std::move(m));
        if (!complete) continue;
        for (const auto& issue : library.validate(*d.body))
            out.push_back((issue.node ? QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : #%2 %3").arg(label, QString::number(issue.node), QString::fromStdString(issue.message))
                                      : QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » : %2").arg(label, QString::fromStdString(issue.message)))
                              .toStdString());
    }
    if (std::string r = recursionIssue(set); !r.empty()) out.push_back(std::move(r));
    return out;
}

std::vector<Issue> validateBody(const NodeLibrary& base, const Graph& body, const std::string& id, const std::vector<SubgraphDefinition>& definitions)
{
    std::vector<SubgraphDefinition> trial = definitions;
    const auto it = std::find_if(trial.begin(), trial.end(), [&](const SubgraphDefinition& d) { return d.id == id; });
    if (it == trial.end()) return { { 0, QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » introuvable").arg(QString::fromStdString(id)).toStdString() } };
    auto copy = std::make_shared<Graph>(body);
    copy->subgraphs.clear();   // corps seul : les définitions ne sont pas reprises dans ses contrôles
    it->body = copy;
    const auto lib = NodeLibrary::withSubgraphs(base, trial);
    std::vector<Issue> issues = lib->validate(*copy);
    for (auto& m : structureIssues(*lib, *it, nullptr)) issues.push_back({ 0, std::move(m) });
    if (std::string r = recursionIssue(trial); !r.empty()) issues.push_back({ 0, std::move(r) });
    return issues;
}

std::vector<std::string> pruneStaleLinks(const NodeLibrary& library, Graph& g, const std::vector<int>& nodes)
{
    std::vector<std::string> removed;
    std::unique_ptr<NodeLibrary> holder;
    const NodeLibrary& lib = library.forGraph(g, holder);
    const std::set<int> targets(nodes.begin(), nodes.end());
    auto title = [&](int id) {
        const NodeInstance* n = g.node(id);
        const NodeDefinition* d = n ? lib.find(n->type) : nullptr;
        return "#" + std::to_string(id) + (d ? " « " + displayText(d->title) + " »" : std::string());
    };
    std::vector<Link> stale;
    for (const auto& l : g.links())
        if ((targets.count(l.toNode) || targets.count(l.fromNode)) && !lib.canConnect(g, l)) stale.push_back(l);
    for (const auto& l : stale)
    {
        g.removeLink(l);
        const bool in = targets.count(l.toNode) > 0;
        removed.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "%1 : lien « %2 » retiré (broche supprimée ou de type modifié)")
                              .arg(QString::fromStdString(title(in ? l.toNode : l.fromNode)), QString::fromStdString(in ? l.toPin : l.fromPin))
                              .toStdString());
    }
    for (int id : nodes)
    {
        NodeInstance* n = g.node(id);
        const NodeDefinition* d = n ? lib.find(n->type) : nullptr;
        if (!d) continue;
        for (auto v = n->values.begin(); v != n->values.end();)
        {
            if (!d->input(v->first))
            {
                removed.push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "%1 : valeur de l'entrée supprimée « %2 » effacée").arg(QString::fromStdString(title(id)), QString::fromStdString(v->first)).toStdString());
                v = n->values.erase(v);
            }
            else
                ++v;
        }
    }
    return removed;
}

std::vector<int> subgraphInstances(const Graph& g, const std::string& id)
{
    std::vector<int> out;
    for (const auto& [nid, n] : g.nodes())
        if (n.type == subgraphInstanceType(id)) out.push_back(nid);
    return out;
}

std::vector<std::string> usedSubgraphs(const Graph& g, const std::set<int>& nodes)
{
    std::vector<std::string> order;
    std::set<std::string> done;
    std::function<void(const std::string&)> add = [&](const std::string& id) {
        if (done.count(id)) return;
        done.insert(id);
        if (const SubgraphDefinition* d = g.subgraph(id); d && d->body)
            for (const auto& [nid, n] : d->body->nodes())
                if (const std::string sub = subgraphOfType(n.type); !sub.empty()) add(sub);
        order.push_back(id);   // dépendances d'abord
    };
    for (int id : nodes)
        if (const NodeInstance* n = g.node(id))
            if (const std::string sub = subgraphOfType(n->type); !sub.empty()) add(sub);
    return order;
}

// -----------------------------------------------------------------------------
// Encapsulation, définition, suppression
// -----------------------------------------------------------------------------

EncapsulationResult encapsulate(const NodeLibrary& base, Graph& g, const std::set<int>& selection, const std::string& id, const std::string& name)
{
    EncapsulationResult r;
    r.id = id;
    auto fail = [&r](const std::string& m) {
        r.message = m;
        return r;
    };
    if (selection.empty()) return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "aucun nœud sélectionné").toStdString());
    if (!isValidSubgraphId(id))
        return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "identifiant invalide « %1 » (lettre puis lettres, chiffres, _ ou -)").arg(QString::fromStdString(id)).toStdString());
    if (g.subgraph(id)) return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "un sous-graphe « %1 » existe déjà dans ce graphe").arg(QString::fromStdString(id)).toStdString());
    std::unique_ptr<NodeLibrary> holder;
    const NodeLibrary& lib = base.forGraph(g, holder);
    bool hasAction = false;
    double minX = 1e300, maxX = -1e300, sumX = 0.0, sumY = 0.0;
    for (int n : selection)
    {
        const NodeInstance* ni = g.node(n);
        if (!ni) return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "nœud #%1 inexistant").arg(n).toStdString());
        const NodeDefinition* d = lib.find(ni->type);
        if (!d) return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "nœud #%1 de type inconnu « %2 »").arg(QString::number(n), QString::fromStdString(ni->type)).toStdString());
        if (d->entry || (ni->type.rfind(kPrefix, 0) == 0 && subgraphOfType(ni->type).empty()))
            return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "« %1 » (#%2) ne peut pas être encapsulé (point de départ ou nœud d'entrée / de sortie de "
                                                            "sous-graphe)")
                            .arg(QString::fromStdString(displayText(d->title)), QString::number(n))
                            .toStdString());
        hasAction = hasAction || !d->pure;
        minX = std::min(minX, ni->x), maxX = std::max(maxX, ni->x);
        sumX += ni->x, sumY += ni->y;
    }
    auto inside = [&](int n) { return selection.count(n) > 0; };
    auto outputPin = [&](int node, const std::string& pin) -> const PinSpec* {
        const NodeInstance* n = g.node(node);
        const NodeDefinition* d = n ? lib.find(n->type) : nullptr;
        return d ? d->output(pin) : nullptr;
    };

    using Source = std::pair<int, std::string>;
    std::map<Source, std::string> inName, outName;
    std::vector<std::pair<Source, std::string>> inOrder, outOrder;   // ordre de création des broches
    std::vector<PinSpec> inputs, outputs;
    std::set<std::string> usedIn, usedOut;
    std::vector<Link> dataIn, dataOut, execIn, execOut;
    for (const auto& l : g.links())
    {
        const bool a = inside(l.fromNode), b = inside(l.toNode);
        if (a == b) continue;
        const PinSpec* src = outputPin(l.fromNode, l.fromPin);
        if (!src)
            return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "lien invalide depuis #%1.%2").arg(QString::number(l.fromNode), QString::fromStdString(l.fromPin)).toStdString());
        if (src->kind == PinKind::Exec)
        {
            (b ? execIn : execOut).push_back(l);
            continue;
        }
        const Source key { l.fromNode, l.fromPin };
        if (b)
        {
            if (!inName.count(key))
            {
                PinSpec p = *src;
                p.kind = PinKind::Data;
                p.name = sanitizedName(src->name, usedIn);
                p.label = src->label.empty() ? p.name : src->label;
                p.defaultValue = {};
                p.required = true;
                inName[key] = p.name;
                inOrder.push_back({ key, p.name });
                inputs.push_back(p);
            }
            dataIn.push_back(l);
        }
        else
        {
            if (!outName.count(key))
            {
                PinSpec p = *src;
                p.kind = PinKind::Data;
                p.name = sanitizedName(src->name, usedOut);
                p.label = src->label.empty() ? p.name : src->label;
                p.defaultValue = {};
                p.required = false;
                outName[key] = p.name;
                outOrder.push_back({ key, p.name });
                outputs.push_back(p);
            }
            dataOut.push_back(l);
        }
    }
    std::set<Source> execTargets, execSources;
    for (const auto& l : execIn) execTargets.insert({ l.toNode, l.toPin });
    for (const auto& l : execOut) execSources.insert({ l.fromNode, l.fromPin });
    if (execTargets.size() > 1)
        return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "plusieurs liens d'exécution entrent dans la sélection par des nœuds différents : un "
                                                        "sous-graphe a un seul point d'entrée")
                        .toStdString());
    if (execSources.size() > 1)
        return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "plusieurs sorties d'exécution quittent la sélection : un sous-graphe a une seule sortie "
                                                        "d'exécution")
                        .toStdString());
    if (hasAction && execTargets.empty())
        return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "la sélection contient des nœuds d'action mais aucun lien d'exécution n'y entre (elle ne "
                                                        "serait jamais exécutée)")
                        .toStdString());

    // Corps : copie des nœuds et des liens intérieurs, entrée et sortie
    auto body = std::make_shared<Graph>();
    body->name = name.empty() ? id : name;
    const double cx = sumX / static_cast<double>(selection.size()), cy = sumY / static_cast<double>(selection.size());
    for (int n : selection) body->insertNode(*g.node(n));
    for (const auto& l : g.links())
        if (inside(l.fromNode) && inside(l.toNode)) body->addLink(l);
    const int inNode = body->addNode(subgraphInputType(id), minX - 320.0, cy);
    const int outNode = body->addNode(subgraphOutputType(id), maxX + 320.0, cy);
    if (execTargets.empty())
        body->addLink({ inNode, "then", outNode, "exec" });   // corps pur
    else
    {
        const Source entry = *execTargets.begin();
        body->addLink({ inNode, "then", entry.first, entry.second });
        if (!execSources.empty())
            body->addLink({ execSources.begin()->first, execSources.begin()->second, outNode, "exec" });
        else
        {
            // Fin de la chaîne : on suit la dernière sortie d'exécution de chaque nœud jusqu'à une sortie libre
            std::set<int> visited;
            int x = entry.first;
            while (true)
            {
                if (!visited.insert(x).second)
                    return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "chaîne d'exécution encapsulée en boucle : fin introuvable").toStdString());
                const NodeInstance* nx = body->node(x);
                const NodeDefinition* dx = nx ? lib.find(nx->type) : nullptr;
                std::vector<std::string> execOuts;
                if (dx)
                    for (const auto& p : dx->outputs)
                        if (p.kind == PinKind::Exec) execOuts.push_back(p.name);
                if (execOuts.empty())
                    return fail(QCoreApplication::translate("TSA::Blueprint::Subgraph", "la chaîne d'exécution encapsulée se termine sur « %1 », sans sortie d'exécution")
                                    .arg(QString::fromStdString(dx ? displayText(dx->title) : nx->type))
                                    .toStdString());
                const auto next = body->linksFrom(x, execOuts.back());
                if (next.empty())
                {
                    body->addLink({ x, execOuts.back(), outNode, "exec" });
                    break;
                }
                x = next.front().toNode;
            }
        }
    }
    for (const auto& l : dataIn) body->addLink({ inNode, inName[{ l.fromNode, l.fromPin }], l.toNode, l.toPin });
    for (const auto& [src, pin] : outOrder) body->addLink({ src.first, src.second, outNode, pin });

    SubgraphDefinition def;
    def.id = id;
    def.name = name.empty() ? id : name;
    def.description = "Sous-graphe encapsulé (" + std::to_string(selection.size()) + " nœud(s)).";
    def.inputs = inputs;
    def.outputs = outputs;
    def.body = body;

    // Remplacement dans le graphe
    for (int n : selection) g.removeNode(n);
    g.subgraphs.push_back(def);
    const int inst = g.addNode(subgraphInstanceType(id), cx, cy);
    for (const auto& [src, pin] : inOrder) g.addLink({ src.first, src.second, inst, pin });
    for (const auto& l : dataOut) g.addLink({ inst, outName[{ l.fromNode, l.fromPin }], l.toNode, l.toPin });
    for (const auto& l : execIn) g.addLink({ l.fromNode, l.fromPin, inst, "exec" });
    for (const auto& l : execOut) g.addLink({ inst, "then", l.toNode, l.toPin });
    r.ok = true;
    r.instance = inst;
    r.message = (hasAction ? QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sous-graphe « %1 » créé : %2 nœud(s), %3 entrée(s), %4 sortie(s), d'action.")
                           : QCoreApplication::translate("TSA::Blueprint::Subgraph", "Sous-graphe « %1 » créé : %2 nœud(s), %3 entrée(s), %4 sortie(s), pur."))
                    .arg(QString::fromStdString(def.name), QString::number(selection.size()), QString::number(inputs.size()), QString::number(outputs.size()))
                    .toStdString();
    return r;
}

bool defineSubgraph(const NodeLibrary& base, Graph& g, SubgraphDefinition def, std::vector<std::string>* updates, std::string* error)
{
    auto fail = [error](const std::string& m) {
        if (error) *error = m;
        return false;
    };
    std::vector<SubgraphDefinition> trial = g.subgraphs;
    const auto it = std::find_if(trial.begin(), trial.end(), [&](const SubgraphDefinition& d) { return d.id == def.id; });
    if (it != trial.end())
    {
        SubgraphDefinition same = def;
        same.version = it->version;
        if (same.sameContent(*it)) return true;   // inchangé : version conservée
        def.version = it->version + 1;
        *it = def;
    }
    else
    {
        def.version = std::max(def.version, 1);
        trial.push_back(def);
    }
    const auto lib = NodeLibrary::withSubgraphs(base, trial);
    const auto issues = validateSubgraphs(*lib, trial);
    if (!issues.empty()) return fail(issues.front());
    g.subgraphs = std::move(trial);
    // Mise à jour des instances : liens et valeurs devenus invalides retirés, version courante à l'enregistrement
    const std::vector<int> instances = subgraphInstances(g, def.id);
    for (auto& m : pruneStaleLinks(*lib, g, instances))
        if (updates) updates->push_back(QCoreApplication::translate("TSA::Blueprint::Subgraph", "instance %1").arg(QString::fromStdString(m)).toStdString());
    for (int inst : instances) g.node(inst)->version = 0;
    return true;
}

bool removeSubgraph(Graph& g, const std::string& id, std::string* error)
{
    auto users = subgraphInstances(g, id);
    for (const auto& d : g.subgraphs)
        if (d.id != id && d.body && !subgraphInstances(*d.body, id).empty()) users.push_back(0);
    if (!users.empty())
    {
        if (error)
            *error = QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » encore utilisé (%2 instance(s))").arg(QString::fromStdString(id), QString::number(users.size())).toStdString();
        return false;
    }
    const auto before = g.subgraphs.size();
    g.subgraphs.erase(std::remove_if(g.subgraphs.begin(), g.subgraphs.end(), [&](const SubgraphDefinition& d) { return d.id == id; }), g.subgraphs.end());
    if (g.subgraphs.size() == before)
    {
        if (error) *error = QCoreApplication::translate("TSA::Blueprint::Subgraph", "sous-graphe « %1 » inexistant").arg(QString::fromStdString(id)).toStdString();
        return false;
    }
    return true;
}

} // namespace TSA::Blueprint
