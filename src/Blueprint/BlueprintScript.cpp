#include "BlueprintScript.h"

#include "../Automation/CommandRegistry.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace TSA::Blueprint
{

namespace
{
std::string trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool isIdentifier(const std::string& s)
{
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
}

constexpr double kStepX = 280.0;
} // namespace

bool fromCommandScript(const std::string& script, const NodeLibrary& library, const TSA::Automation::CommandRegistry& commands,
                       Graph& out, std::string* error)
{
    using namespace TSA::Automation;
    auto failAt = [error](int line, const std::string& message) {
        if (error) *error = "ligne " + std::to_string(line) + " : " + message;
        return false;
    };

    Graph g;
    g.name = "Script de commandes";
    int previous = g.addNode("event.start", -kStepX, 0.0);
    std::string previousPin = "then";
    std::map<std::string, int> variables;   // nom → nœud
    std::istringstream in(script);
    std::string raw;
    int lineNo = 0, column = 0;
    while (std::getline(in, raw))
    {
        ++lineNo;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        // « nom = commande … » (le premier « = » avant tout espace désigne une variable)
        std::string variable;
        const auto eq = line.find('=');
        const auto sp = line.find_first_of(" \t");
        if (eq != std::string::npos && (sp == std::string::npos || eq < sp || trim(line.substr(0, eq)).find(' ') == std::string::npos))
        {
            const std::string name = trim(line.substr(0, eq));
            if (isIdentifier(name) && !commands.find(name))
            {
                variable = name;
                line = trim(line.substr(eq + 1));
            }
        }

        std::vector<std::string> tokens;
        std::string tokError;
        if (!tokenizeCommandLine(line, tokens, &tokError)) return failAt(lineNo, tokError);
        if (tokens.empty()) return failAt(lineNo, "commande attendue");
        const CommandSpec* spec = commands.find(tokens[0]);
        if (!spec) return failAt(lineNo, "commande inconnue « " + tokens[0] + " »");
        const std::string type = "cmd." + spec->id;
        if (!library.find(type)) return failAt(lineNo, "aucun nœud pour la commande « " + spec->id + " »");

        const int node = g.addNode(type, kStepX * column, (column % 2) * 60.0);
        ++column;
        std::string why;
        if (!library.connect(g, { previous, previousPin, node, "exec" }, &why)) return failAt(lineNo, why);
        for (std::size_t i = 1; i < tokens.size(); ++i)
        {
            const auto pos = tokens[i].find('=');
            if (pos == std::string::npos) return failAt(lineNo, "argument « " + tokens[i] + " » : syntaxe nom=valeur attendue");
            const std::string name = tokens[i].substr(0, pos), value = tokens[i].substr(pos + 1);
            const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(),
                                        [&](const ParameterSpec& s) { return s.name == name; });
            if (p == spec->parameters.end()) return failAt(lineNo, "paramètre inconnu « " + name + " » pour " + spec->id);

            // Référence « variable.sortie » : lien de données depuis la commande nommée.
            const auto dot = value.find('.');
            if (dot != std::string::npos && variables.count(value.substr(0, dot)))
            {
                const Link link { variables.at(value.substr(0, dot)), value.substr(dot + 1), node, name };
                if (!library.connect(g, link, &why)) return failAt(lineNo, "« " + value + " » : " + why);
                continue;
            }
            Value v;
            if (!parseArgument(value, p->type, v))
                return failAt(lineNo, "paramètre « " + name + " » : " + typeName(p->type) + " attendu, reçu « " + value + " »");
            g.setValue(node, name, v);
        }
        if (!variable.empty()) variables[variable] = node;
        previous = node;
        previousPin = "then";
    }
    if (column == 0)
    {
        if (error) *error = "script vide : aucune commande";
        return false;
    }
    const auto issues = library.validate(g);
    if (!issues.empty())
    {
        if (error) *error = issues.front().message;
        return false;
    }
    out = std::move(g);
    return true;
}

std::string toCommandScript(const Graph& graph, const NodeLibrary& library, std::vector<std::string>* warnings)
{
    using namespace TSA::Automation;
    auto warn = [warnings](const std::string& w) {
        if (warnings) warnings->push_back(w);
    };
    std::ostringstream o;
    if (!graph.name.empty()) o << "# " << graph.name << '\n';

    int start = 0;
    for (const auto& [id, n] : graph.nodes())
        if (n.type == "event.start") { start = id; break; }
    if (!start)
    {
        warn("aucun nœud « Début »");
        return o.str();
    }

    // Commandes dont une sortie de données est utilisée : nommées « n<id> ».
    std::set<int> referenced;
    for (const auto& l : graph.links())
        if (const auto* d = graph.node(l.fromNode) ? library.find(graph.node(l.fromNode)->type) : nullptr)
            if (const auto* pin = d->output(l.fromPin); pin && pin->kind == PinKind::Data) referenced.insert(l.fromNode);

    std::set<int> visited;
    std::vector<Link> next = graph.linksFrom(start, "then");
    while (!next.empty())
    {
        const int id = next.front().toNode;
        if (!visited.insert(id).second)
        {
            warn("boucle d'exécution non exprimable en script");
            break;
        }
        const NodeInstance* n = graph.node(id);
        const NodeDefinition* d = n ? library.find(n->type) : nullptr;
        if (!d || n->type.rfind("cmd.", 0) != 0)
        {
            warn("nœud « " + (d ? d->title : n ? n->type : std::string("?")) + " » (#" + std::to_string(id)
                 + ") non exprimable en script : chaîne arrêtée");
            break;
        }
        if (referenced.count(id)) o << 'n' << id << " = ";
        o << n->type.substr(4);
        for (const auto& pin : d->inputs)
        {
            if (pin.kind != PinKind::Data) continue;
            if (const auto link = graph.linkTo(id, pin.name))
            {
                const NodeInstance* src = graph.node(link->fromNode);
                if (src && src->type.rfind("cmd.", 0) == 0 && visited.count(link->fromNode))
                    o << ' ' << pin.name << "=n" << link->fromNode << '.' << link->fromPin;
                else
                    warn("entrée « " + pin.name + " » de #" + std::to_string(id) + " reliée à un nœud non exprimable en script");
                continue;
            }
            const auto it = n->values.find(pin.name);
            if (it != n->values.end() && !std::holds_alternative<std::monostate>(it->second))
                o << ' ' << pin.name << '=' << formatArgument(it->second);
        }
        o << '\n';
        if (d->outputs.size() > 1 && graph.linksFrom(id, "then").size() > 1) warn("plusieurs suites d'exécution : seule la première est suivie");
        next = graph.linksFrom(id, "then");
    }
    return o.str();
}

std::string describe(const Graph& graph, const NodeLibrary& library)
{
    std::ostringstream o;
    o << "Blueprint « " << graph.name << " » : " << graph.nodes().size() << " nœud(s), " << graph.links().size() << " lien(s).\n";
    if (!graph.description.empty()) o << graph.description << '\n';
    for (const auto& [id, n] : graph.nodes())
    {
        const NodeDefinition* d = library.find(n.type);
        o << "#" << id << " " << (d ? d->title : "type inconnu") << " [" << n.type << "]";
        bool first = true;
        for (const auto& [pin, v] : n.values)
        {
            if (std::holds_alternative<std::monostate>(v)) continue;
            o << (first ? " : " : ", ") << pin << " = " << toText(v);
            first = false;
        }
        o << '\n';
    }
    for (const auto& l : graph.links())
        o << "lien #" << l.fromNode << "." << l.fromPin << " → #" << l.toNode << "." << l.toPin << '\n';
    return o.str();
}

} // namespace TSA::Blueprint
