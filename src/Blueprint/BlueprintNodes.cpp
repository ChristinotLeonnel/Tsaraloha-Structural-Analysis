// Nœuds standard des Blueprints : événements, paramètres, maths, logique, flux, texte, débogage,
// commandes du registre central (une par commande, générées automatiquement) et science TSALab.
#include "BlueprintRuntime.h"

#include "tsalab/planar/PlanarSolvers.h"
#include "tsalab/validation/PlanarBenchmarks.h"

#include <cmath>

namespace TSA::Blueprint
{

namespace
{
using VT = ValueType;

PinSpec execIn() { return { "exec", "", PinKind::Exec }; }
PinSpec execOut(std::string name = "then", std::string label = "") { return { std::move(name), std::move(label), PinKind::Exec }; }

PinSpec data(std::string name, std::string label, VT type, Value def = {}, Quantity q = Quantity::None)
{
    PinSpec p;
    p.name = std::move(name);
    p.label = std::move(label);
    p.type = type;
    p.defaultValue = std::move(def);
    p.quantity = q;
    return p;
}

PinSpec anyData(std::string name, std::string label)
{
    PinSpec p = data(std::move(name), std::move(label), VT::Text);
    p.any = true;
    return p;
}

NodeDefinition def(std::string id, std::string title, std::string category, std::string description,
                   std::vector<PinSpec> in, std::vector<PinSpec> out, bool pure)
{
    return { std::move(id), std::move(title), std::move(category), std::move(description), std::move(in), std::move(out), pure };
}

void binaryMath(NodeLibrary& lib, const char* id, const char* title, const char* desc, double (*op)(double, double, bool&))
{
    lib.add(def(id, title, "Math", desc, { data("a", "A", VT::Real, 0.0), data("b", "B", VT::Real, 0.0) },
                { data("result", "Résultat", VT::Real) }, true),
            [op](ExecutionContext& c) {
                bool ok = true;
                const double r = op(c.real("a"), c.real("b"), ok);
                if (!ok) return c.fail("division par zéro");
                c.setOutput("result", r);
                return !c.failed();
            });
}

template <typename T>
void parameterNode(NodeLibrary& lib, const char* id, const char* title, VT type, Value def0, Quantity q = Quantity::None)
{
    lib.add(def(id, title, "Paramètres",
                "Paramètre nommé du Blueprint : sa valeur peut être modifiée sans toucher au graphe (reconstruction paramétrique).",
                { data("name", "Nom", VT::Text, std::string("p")), data("value", "Valeur", type, def0, q) },
                { data("value", "Valeur", type, {}, q) }, true),
            [](ExecutionContext& c) {
                const std::string name = c.text("name");
                if (const Value* o = c.parameterOverride(name))
                {
                    Value v = *o;
                    if constexpr (std::is_same_v<T, double>)
                        if (const auto* i = std::get_if<long long>(&v)) v = static_cast<double>(*i);
                    if (!std::holds_alternative<T>(v)) return c.fail("valeur imposée au paramètre « " + name + " » de type incorrect");
                    c.setOutput("value", v);
                    return true;
                }
                c.setOutput("value", c.input("value"));
                return !c.failed();
            });
}
} // namespace

void registerStandardNodes(NodeLibrary& lib, const TSA::Automation::CommandRegistry& commands)
{
    // --- Événements ------------------------------------------------------------------------------
    lib.add(def("event.start", "Début", "Événements", "Point de départ de l'exécution.", {}, { execOut() }, false),
            [](ExecutionContext& c) { return c.fire("then"); });

    // --- Paramètres ------------------------------------------------------------------------------
    parameterNode<double>(lib, "param.real", "Paramètre réel", VT::Real, 0.0);
    parameterNode<long long>(lib, "param.integer", "Paramètre entier", VT::Integer, 0LL);
    parameterNode<bool>(lib, "param.bool", "Paramètre booléen", VT::Bool, false);
    parameterNode<std::string>(lib, "param.text", "Paramètre texte", VT::Text, std::string());
    parameterNode<Point3>(lib, "param.point", "Paramètre point", VT::Point3, Point3 { 0, 0, 0 }, Quantity::Length);

    // --- Math --------------------------------------------------------------------------------------
    binaryMath(lib, "math.add", "Addition", "A + B", [](double a, double b, bool&) { return a + b; });
    binaryMath(lib, "math.subtract", "Soustraction", "A − B", [](double a, double b, bool&) { return a - b; });
    binaryMath(lib, "math.multiply", "Multiplication", "A × B", [](double a, double b, bool&) { return a * b; });
    binaryMath(lib, "math.divide", "Division", "A ÷ B (B non nul)", [](double a, double b, bool& ok) {
        ok = b != 0.0;
        return ok ? a / b : 0.0;
    });
    lib.add(def("math.compare", "Comparer", "Math", "A op B, op ∈ { <, <=, >, >=, ==, != }.",
                { data("a", "A", VT::Real, 0.0), data("b", "B", VT::Real, 0.0), data("op", "Opérateur", VT::Text, std::string("<")) },
                { data("result", "Résultat", VT::Bool) }, true),
            [](ExecutionContext& c) {
                const double a = c.real("a"), b = c.real("b");
                const std::string op = c.text("op");
                bool r = false;
                if (op == "<") r = a < b;
                else if (op == "<=") r = a <= b;
                else if (op == ">") r = a > b;
                else if (op == ">=") r = a >= b;
                else if (op == "==") r = a == b;
                else if (op == "!=") r = a != b;
                else return c.fail("opérateur inconnu « " + op + " »");
                c.setOutput("result", r);
                return !c.failed();
            });
    lib.add(def("math.to_integer", "Arrondir en entier", "Math", "Entier le plus proche.", { data("value", "Valeur", VT::Real, 0.0) },
                { data("result", "Entier", VT::Integer) }, true),
            [](ExecutionContext& c) {
                c.setOutput("result", static_cast<long long>(std::llround(c.real("value"))));
                return !c.failed();
            });
    lib.add(def("math.make_point", "Construire un point", "Math", "Point (x, y, z) en mètres.",
                { data("x", "X", VT::Real, 0.0, Quantity::Length), data("y", "Y", VT::Real, 0.0, Quantity::Length),
                  data("z", "Z", VT::Real, 0.0, Quantity::Length) },
                { data("point", "Point", VT::Point3, {}, Quantity::Length) }, true),
            [](ExecutionContext& c) {
                c.setOutput("point", Point3 { c.real("x"), c.real("y"), c.real("z") });
                return !c.failed();
            });
    lib.add(def("math.break_point", "Décomposer un point", "Math", "Coordonnées d'un point.",
                { data("point", "Point", VT::Point3, Point3 { 0, 0, 0 }, Quantity::Length) },
                { data("x", "X", VT::Real), data("y", "Y", VT::Real), data("z", "Z", VT::Real) }, true),
            [](ExecutionContext& c) {
                const Value v = c.input("point");
                const auto* p = std::get_if<Point3>(&v);
                if (!p) return c.fail("point attendu");
                c.setOutput("x", (*p)[0]);
                c.setOutput("y", (*p)[1]);
                c.setOutput("z", (*p)[2]);
                return true;
            });

    // --- Logique -----------------------------------------------------------------------------------
    lib.add(def("logic.and", "ET", "Logique", "A et B.", { data("a", "A", VT::Bool, false), data("b", "B", VT::Bool, false) },
                { data("result", "Résultat", VT::Bool) }, true),
            [](ExecutionContext& c) { c.setOutput("result", c.boolean("a") && c.boolean("b")); return !c.failed(); });
    lib.add(def("logic.or", "OU", "Logique", "A ou B.", { data("a", "A", VT::Bool, false), data("b", "B", VT::Bool, false) },
                { data("result", "Résultat", VT::Bool) }, true),
            [](ExecutionContext& c) { c.setOutput("result", c.boolean("a") || c.boolean("b")); return !c.failed(); });
    lib.add(def("logic.not", "NON", "Logique", "Négation.", { data("a", "A", VT::Bool, false) }, { data("result", "Résultat", VT::Bool) }, true),
            [](ExecutionContext& c) { c.setOutput("result", !c.boolean("a")); return !c.failed(); });

    // --- Flux d'exécution ----------------------------------------------------------------------------
    lib.add(def("flow.branch", "Si", "Flux", "Exécute « Vrai » ou « Faux » selon la condition.",
                { execIn(), data("condition", "Condition", VT::Bool, false) }, { execOut("true", "Vrai"), execOut("false", "Faux") }, false),
            [](ExecutionContext& c) {
                const bool cond = c.boolean("condition");
                if (c.failed()) return false;
                return c.fire(cond ? "true" : "false");
            });
    lib.add(def("flow.sequence", "Séquence", "Flux", "Exécute ses sorties dans l'ordre.",
                { execIn() }, { execOut("then_0", "1"), execOut("then_1", "2"), execOut("then_2", "3") }, false),
            [](ExecutionContext& c) { return c.fire("then_0") && c.fire("then_1") && c.fire("then_2"); });
    lib.add(def("flow.for", "Pour", "Flux", "Exécute « Corps » pour chaque index de Premier à Dernier (inclus), puis « Terminé ».",
                { execIn(), data("first", "Premier", VT::Integer, 0LL), data("last", "Dernier", VT::Integer, 0LL) },
                { execOut("body", "Corps"), data("index", "Index", VT::Integer), execOut("completed", "Terminé") }, false),
            [](ExecutionContext& c) {
                const long long first = c.integer("first"), last = c.integer("last");
                if (c.failed()) return false;
                if (last - first + 1 > c.maxIterations()) return c.fail("trop d'itérations (" + std::to_string(last - first + 1) + ")");
                for (long long i = first; i <= last; ++i)
                {
                    c.setOutput("index", i);
                    if (!c.fire("body")) return false;
                }
                return c.fire("completed");
            });
    lib.add(def("flow.while", "Tant que", "Flux", "Exécute « Corps » tant que la condition (réévaluée) est vraie.",
                { execIn(), data("condition", "Condition", VT::Bool, false) }, { execOut("body", "Corps"), execOut("completed", "Terminé") }, false),
            [](ExecutionContext& c) {
                for (int i = 0;; ++i)
                {
                    const bool cond = c.boolean("condition");
                    if (c.failed()) return false;
                    if (!cond) break;
                    if (i >= c.maxIterations()) return c.fail("trop d'itérations (boucle infinie ?)");
                    if (!c.fire("body")) return false;
                }
                return c.fire("completed");
            });

    // --- Texte et débogage ---------------------------------------------------------------------------
    lib.add(def("text.concat", "Concaténer", "Texte", "A suivi de B (toute valeur convertie en texte).",
                { anyData("a", "A"), anyData("b", "B") }, { data("result", "Texte", VT::Text) }, true),
            [](ExecutionContext& c) { c.setOutput("result", c.text("a") + c.text("b")); return !c.failed(); });
    lib.add(def("debug.print", "Afficher", "Débogage", "Écrit une valeur dans le journal d'exécution.",
                { execIn(), anyData("value", "Valeur") }, { execOut() }, false),
            [](ExecutionContext& c) {
                const std::string t = c.text("value");
                if (c.failed()) return false;
                c.log(t);
                return c.fire("then");
            });

    // --- Commandes du registre central : une commande = un nœud d'action -----------------------------
    for (const auto* spec : commands.commands())
    {
        NodeDefinition d;
        d.id = "cmd." + spec->id;
        d.title = spec->title;
        d.category = spec->category;
        d.description = spec->description + " (commande « " + spec->id + " »)";
        d.pure = false;
        d.inputs.push_back(execIn());
        for (const auto& p : spec->parameters)
        {
            PinSpec pin = data(p.name, p.label, p.type, p.required ? Value {} : p.defaultValue, p.quantity);
            pin.required = p.required;
            d.inputs.push_back(pin);
        }
        d.outputs.push_back(execOut());
        for (const auto& o : spec->outputs) d.outputs.push_back(data(o.name, o.label, o.type, {}, o.quantity));
        const std::string commandId = spec->id;
        lib.add(std::move(d), [&commands, commandId](ExecutionContext& c) {
            if (!c.session()) return c.fail("aucun projet ouvert");
            const auto* spec = commands.find(commandId);
            TSA::Automation::Arguments args;
            for (const auto& p : spec->parameters)
            {
                if (c.hasValue(p.name))
                {
                    const Value v = c.input(p.name);
                    if (c.failed()) return false;
                    args[p.name] = v;
                }
                else if (p.required)
                    return c.fail("paramètre requis « " + p.label + " » non renseigné");
            }
            const auto r = commands.execute(*c.session(), commandId, args);
            c.log(r.message);
            if (!r.ok) return c.fail(r.message);
            for (const auto& [name, value] : r.outputs) c.setOutput(name, value);
            return c.fire("then");
        });
    }

    // --- TSALab : science (cœur scientifique, sans Qt) -----------------------------------------------
    lib.add(def("science.validation_bench", "Banc de validation", "TSALab",
                "Exécute les benchmarks à solution analytique sur tous les solveurs d'ossatures planes "
                "(résultat, solution analytique, validation croisée K·U = F).",
                { execIn() },
                { execOut(), data("passed", "Validés", VT::Integer), data("total", "Total", VT::Integer), data("report", "Rapport", VT::Text) },
                false),
            [](ExecutionContext& c) {
                std::vector<tsalab::validation::BenchmarkReport> reports;
                for (const auto& solver : tsalab::planar::createBuiltInSolvers())
                    for (const auto& b : tsalab::validation::planarBenchmarks())
                        reports.push_back(tsalab::validation::runBenchmark(b, *solver));
                long long passed = 0;
                for (const auto& r : reports) passed += r.passed ? 1 : 0;
                c.setOutput("passed", passed);
                c.setOutput("total", static_cast<long long>(reports.size()));
                c.setOutput("report", tsalab::validation::formatReport(reports));
                return c.fire("then");
            });
}

} // namespace TSA::Blueprint
