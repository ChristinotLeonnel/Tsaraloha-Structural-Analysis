#include "BlueprintScript.h"

#include "../Automation/CommandRegistry.h"

#include <QCoreApplication>
#include <QString>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

// Traduction : erreurs de lecture d'un script et avertissements d'export affichés (contexte « TSA::Blueprint::Script »).
// Le texte du script, le nom du graphe créé et describe() (destiné à l'assistant IA) restent inchangés.

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
        if (error) *error = QCoreApplication::translate("TSA::Blueprint::Script", "ligne %1 : %2").arg(QString::number(line), QString::fromStdString(message)).toStdString();
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
        if (tokens.empty()) return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "commande attendue").toStdString());
        const CommandSpec* spec = commands.find(tokens[0]);
        if (!spec) return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "commande inconnue « %1 »").arg(QString::fromStdString(tokens[0])).toStdString());
        const std::string type = "cmd." + spec->id;
        if (!library.find(type))
            return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "aucun nœud pour la commande « %1 »").arg(QString::fromStdString(spec->id)).toStdString());

        const int node = g.addNode(type, kStepX * column, (column % 2) * 60.0);
        ++column;
        std::string why;
        if (!library.connect(g, { previous, previousPin, node, "exec" }, &why)) return failAt(lineNo, why);
        for (std::size_t i = 1; i < tokens.size(); ++i)
        {
            const auto pos = tokens[i].find('=');
            if (pos == std::string::npos)
                return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "argument « %1 » : syntaxe nom=valeur attendue").arg(QString::fromStdString(tokens[i])).toStdString());
            const std::string name = tokens[i].substr(0, pos), value = tokens[i].substr(pos + 1);
            const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(),
                                        [&](const ParameterSpec& s) { return s.name == name; });
            if (p == spec->parameters.end())
                return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "paramètre inconnu « %1 » pour %2").arg(QString::fromStdString(name), QString::fromStdString(spec->id)).toStdString());

            // Référence « variable.sortie » : lien de données depuis la commande nommée.
            const auto dot = value.find('.');
            if (dot != std::string::npos && variables.count(value.substr(0, dot)))
            {
                const Link link { variables.at(value.substr(0, dot)), value.substr(dot + 1), node, name };
                if (!library.connect(g, link, &why))
                    return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "« %1 » : %2").arg(QString::fromStdString(value), QString::fromStdString(why)).toStdString());
                continue;
            }
            Value v;
            if (!parseArgument(value, p->type, v))
                return failAt(lineNo, QCoreApplication::translate("TSA::Blueprint::Script", "paramètre « %1 » : %2 attendu, reçu « %3 »")
                                          .arg(QString::fromStdString(name),
                                               QCoreApplication::translate("TSA::Automation::Commands", typeName(p->type)),   // nom marqué dans le registre
                                               QString::fromStdString(value))
                                          .toStdString());
            g.setValue(node, name, v);
        }
        if (!variable.empty()) variables[variable] = node;
        previous = node;
        previousPin = "then";
    }
    if (column == 0)
    {
        if (error) *error = QCoreApplication::translate("TSA::Blueprint::Script", "script vide : aucune commande").toStdString();
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
        warn(QCoreApplication::translate("TSA::Blueprint::Script", "aucun nœud « Début »").toStdString());
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
            warn(QCoreApplication::translate("TSA::Blueprint::Script", "boucle d'exécution non exprimable en script").toStdString());
            break;
        }
        const NodeInstance* n = graph.node(id);
        const NodeDefinition* d = n ? library.find(n->type) : nullptr;
        if (!d || n->type.rfind("cmd.", 0) != 0)
        {
            warn(QCoreApplication::translate("TSA::Blueprint::Script", "nœud « %1 » (#%2) non exprimable en script : chaîne arrêtée")
                     .arg(QString::fromStdString(d ? displayText(d->title) : n ? n->type : std::string("?")), QString::number(id))
                     .toStdString());
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
                    warn(QCoreApplication::translate("TSA::Blueprint::Script", "entrée « %1 » de #%2 reliée à un nœud non exprimable en script")
                             .arg(QString::fromStdString(pin.name), QString::number(id))
                             .toStdString());
                continue;
            }
            const auto it = n->values.find(pin.name);
            if (it != n->values.end() && !std::holds_alternative<std::monostate>(it->second))
                o << ' ' << pin.name << '=' << formatArgument(it->second);
        }
        o << '\n';
        if (d->outputs.size() > 1 && graph.linksFrom(id, "then").size() > 1) warn(QCoreApplication::translate("TSA::Blueprint::Script", "plusieurs suites d'exécution : seule la première est suivie").toStdString());
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
