#include "CommandRegistry.h"

#include "../Analysis/AnalysisController.h"
#include "../Analysis/ResultsModel.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/Load/LoadCase.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Load/MemberLoad.h"
#include "../Model/Load/NodalLoad.h"
#include "../Model/Material.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Section.h"
#include "../Model/SupportDefinition.h"
#include "../Project/ProjectSession.h"
#include "../UndoRedo/EditTransaction.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace TSA::Automation
{

// -----------------------------------------------------------------------------
// Registre
// -----------------------------------------------------------------------------

bool CommandRegistry::add(CommandSpec spec, Handler handler)
{
    if (spec.id.empty() || !handler || m_index.count(spec.id)) return false;
    m_index[spec.id] = m_entries.size();
    m_entries.push_back({ std::move(spec), std::move(handler) });
    return true;
}

const CommandSpec* CommandRegistry::find(const std::string& id) const
{
    const auto it = m_index.find(id);
    return it == m_index.end() ? nullptr : &m_entries[it->second].spec;
}

std::vector<const CommandSpec*> CommandRegistry::commands() const
{
    std::vector<const CommandSpec*> list;
    for (const auto& e : m_entries) list.push_back(&e.spec);
    return list;
}

namespace
{
bool matches(const Value& v, ValueType t)
{
    switch (t)
    {
    case ValueType::Bool: return std::holds_alternative<bool>(v);
    case ValueType::Integer: return std::holds_alternative<long long>(v);
    case ValueType::Real: return std::holds_alternative<double>(v) || std::holds_alternative<long long>(v);
    case ValueType::Text: return std::holds_alternative<std::string>(v);
    case ValueType::Point3: return std::holds_alternative<Point3>(v);
    case ValueType::IdList: return std::holds_alternative<std::vector<int>>(v) || std::holds_alternative<long long>(v);
    }
    return false;
}

/// Conversions sans perte admises (entier → réel, entier → liste d'un identifiant).
Value normalized(const Value& v, ValueType t)
{
    if (t == ValueType::Real && std::holds_alternative<long long>(v)) return static_cast<double>(std::get<long long>(v));
    if (t == ValueType::IdList && std::holds_alternative<long long>(v))
        return std::vector<int> { static_cast<int>(std::get<long long>(v)) };
    return v;
}
} // namespace

CommandResult CommandRegistry::execute(TSA::Project::ProjectSession& session, const std::string& id, Arguments args) const
{
    CommandResult r;
    const auto it = m_index.find(id);
    if (it == m_index.end())
    {
        r.message = "Commande inconnue : « " + id + " ».";
        return r;
    }
    const Entry& e = m_entries[it->second];
    for (const auto& [name, value] : args)
    {
        const auto p = std::find_if(e.spec.parameters.begin(), e.spec.parameters.end(),
                                    [&](const ParameterSpec& s) { return s.name == name; });
        if (p == e.spec.parameters.end())
        {
            r.message = "Paramètre inconnu « " + name + " » pour " + id + ".";
            return r;
        }
    }
    for (const auto& p : e.spec.parameters)
    {
        auto a = args.find(p.name);
        if (a == args.end() || std::holds_alternative<std::monostate>(a->second))
        {
            if (p.required)
            {
                r.message = "Paramètre requis manquant : « " + p.name + " » (" + p.label + ").";
                return r;
            }
            args[p.name] = p.defaultValue;
            continue;
        }
        if (!matches(a->second, p.type))
        {
            r.message = "Paramètre « " + p.name + " » : " + typeName(p.type) + " attendu, reçu « " + formatValue(a->second) + " ».";
            return r;
        }
        a->second = normalized(a->second, p.type);
    }

    if (!e.spec.modifiesModel) return e.handler(session, args);

    TSA::UndoRedo::EditTransaction tx(session.model(), e.spec.title);
    r = e.handler(session, args);
    if (r.ok)
    {
        tx.commit();
        session.model().setModified(true);
    }
    // sinon : rollback automatique (destructeur) — le modèle reste inchangé
    return r;
}

// -----------------------------------------------------------------------------
// Commandes intégrées
// -----------------------------------------------------------------------------

namespace
{
using TSA::Model::Model;

double real(const Arguments& a, const std::string& n) { return std::get<double>(a.at(n)); }
long long integer(const Arguments& a, const std::string& n) { return std::get<long long>(a.at(n)); }
const std::string& text(const Arguments& a, const std::string& n) { return std::get<std::string>(a.at(n)); }

CommandResult fail(const std::string& message)
{
    CommandResult r;
    r.message = message;
    return r;
}

CommandResult done(const std::string& message, Arguments outputs = {})
{
    CommandResult r;
    r.ok = true;
    r.message = message;
    r.outputs = std::move(outputs);
    return r;
}

std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

/// « IPE 300 », « HEA 200 », « HEB 240 », « UPN 200 », « RECT 0.3x0.5 », « CIRC 0.4 ».
bool parseSection(const std::string& spec, TSA::Model::Section& out, std::string* error)
{
    std::istringstream in(upper(spec));
    std::string family, size;
    in >> family >> size;
    try
    {
        if (family == "IPE") out = TSA::Model::Section::ipe(std::stoi(size));
        else if (family == "HEA") out = TSA::Model::Section::hea(std::stoi(size));
        else if (family == "HEB") out = TSA::Model::Section::heb(std::stoi(size));
        else if (family == "UPN") out = TSA::Model::Section::upn(std::stoi(size));
        else if (family == "CIRC") out = TSA::Model::Section::circular(std::stod(size));
        else if (family == "RECT")
        {
            const auto x = size.find('X');
            if (x == std::string::npos) throw std::invalid_argument("dimensions");
            out = TSA::Model::Section::rectangular(std::stod(size.substr(0, x)), std::stod(size.substr(x + 1)));
        }
        else
        {
            if (error) *error = "Section inconnue « " + spec + " » (IPE n, HEA n, HEB n, UPN n, RECT bxh, CIRC d).";
            return false;
        }
    }
    catch (const std::exception&)
    {
        if (error) *error = "Dimensions de section invalides : « " + spec + " ».";
        return false;
    }
    return true;
}

bool parseMaterial(const std::string& name, TSA::Model::Material& out)
{
    const std::string n = upper(name);
    if (n == "S235") out = TSA::Model::Material::steelS235();
    else if (n == "S355") out = TSA::Model::Material::steelS355();
    else if (n == "C25/30" || n == "C25_30") out = TSA::Model::Material::concreteC25_30();
    else if (n == "C30/37" || n == "C30_37") out = TSA::Model::Material::concreteC30_37();
    else if (n == "C24") out = TSA::Model::Material::timberC24();
    else return false;
    return true;
}

ParameterSpec param(std::string name, std::string label, ValueType type, Quantity q = Quantity::None)
{
    ParameterSpec p;
    p.name = std::move(name);
    p.label = std::move(label);
    p.type = type;
    p.quantity = q;
    return p;
}

ParameterSpec optional(ParameterSpec p, Value def)
{
    p.required = false;
    p.defaultValue = std::move(def);
    return p;
}

CommandResult createBar(TSA::Project::ProjectSession& s, const Arguments& a, TSA::Model::BarRole role, const char* kind)
{
    Model& m = s.model();
    const int start = static_cast<int>(integer(a, "start")), end = static_cast<int>(integer(a, "end"));
    if (!m.getNode(start) || !m.getNode(end)) return fail(std::string(kind) + " : nœud d'extrémité inexistant.");
    if (start == end) return fail(std::string(kind) + " : les deux extrémités sont le même nœud.");
    TSA::Model::Section section;
    std::string error;
    if (!parseSection(text(a, "section"), section, &error)) return fail(error);
    TSA::Model::Material material;
    if (!parseMaterial(text(a, "material"), material)) return fail("Matériau inconnu « " + text(a, "material") + " » (S235, S355, C25/30, C30/37, C24).");
    const int id = m.addBar(start, end, section, material, role, real(a, "rotation"), text(a, "name"));
    if (id <= 0) return fail(std::string(kind) + " non créé(e).");
    return done(std::string(kind) + " " + std::to_string(id) + " créé(e) entre N" + std::to_string(start) + " et N" + std::to_string(end) + ".",
                { { "id", static_cast<long long>(id) } });
}
} // namespace

void registerBuiltInCommands(CommandRegistry& reg)
{
    using VT = ValueType;
    using Q = Quantity;

    // --- Modèle -----------------------------------------------------------------------------------
    reg.add({ "model.create_node", "Créer un nœud", "Modèle", "Ajoute un nœud aux coordonnées données (m).",
              { param("position", "Position", VT::Point3, Q::Length), optional(param("name", "Nom", VT::Text), std::string()) },
              { param("id", "Nœud créé", VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const auto p = std::get<Point3>(a.at("position"));
                const int id = s.model().addNode(p[0], p[1], p[2], "", text(a, "name"));
                return done("Nœud N" + std::to_string(id) + " créé.", { { "id", static_cast<long long>(id) } });
            });

    const std::vector<ParameterSpec> barParams = {
        param("start", "Nœud de départ", VT::Integer), param("end", "Nœud d'arrivée", VT::Integer),
        optional(param("section", "Section", VT::Text), std::string("IPE 300")),
        optional(param("material", "Matériau", VT::Text), std::string("S235")),
        optional(param("rotation", "Rotation de section", VT::Real, Q::Angle), 0.0),
        optional(param("name", "Nom", VT::Text), std::string()),
    };
    reg.add({ "model.create_beam", "Créer une poutre", "Modèle", "Ajoute une poutre entre deux nœuds existants.",
              barParams, { param("id", "Poutre créée", VT::Integer) }, true },
            [](auto& s, const Arguments& a) { return createBar(s, a, TSA::Model::BarRole::Beam, "Poutre"); });
    reg.add({ "model.create_column", "Créer un poteau", "Modèle", "Ajoute un poteau entre deux nœuds existants.",
              barParams, { param("id", "Poteau créé", VT::Integer) }, true },
            [](auto& s, const Arguments& a) { return createBar(s, a, TSA::Model::BarRole::Column, "Poteau"); });

    reg.add({ "model.set_support", "Définir un appui", "Modèle",
              "Appui d'un nœud : fixed (encastrement), pinned (articulation), roller (appui simple), free (aucun).",
              { param("node", "Nœud", VT::Integer), param("type", "Type", VT::Text) }, {}, true },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "node"));
                auto* n = s.model().getNode(id);
                if (!n) return fail("Nœud N" + std::to_string(id) + " inexistant.");
                const std::string t = upper(text(a, "type"));
                if (t == "FIXED") n->setSupport(TSA::Model::SupportDefinition::fixed());
                else if (t == "PINNED") n->setSupport(TSA::Model::SupportDefinition::pinned());
                else if (t == "ROLLER") n->setSupport(TSA::Model::SupportDefinition::roller());
                else if (t == "FREE") n->setSupport(TSA::Model::SupportDefinition::free());
                else return fail("Type d'appui inconnu « " + text(a, "type") + " » (fixed, pinned, roller, free).");
                s.model().notifyNodeModified(id);
                return done("Appui « " + text(a, "type") + " » sur N" + std::to_string(id) + ".");
            });

    // --- Charges ----------------------------------------------------------------------------------
    reg.add({ "loads.create_case", "Créer un cas de charge", "Charges", "Ajoute un cas de charge (catégorie : dead, live, wind, snow).",
              { param("name", "Nom", VT::Text), optional(param("category", "Catégorie", VT::Text), std::string("dead")) },
              { param("id", "Cas créé", VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                using C = TSA::Model::LoadCaseCategory;
                const std::string c = upper(text(a, "category"));
                C cat = C::Dead;
                if (c == "LIVE") cat = C::Live;
                else if (c == "WIND") cat = C::Wind;
                else if (c == "SNOW") cat = C::Snow;
                else if (c != "DEAD") return fail("Catégorie inconnue « " + text(a, "category") + " » (dead, live, wind, snow).");
                const int id = s.model().loadManager().addLoadCase(TSA::Model::LoadCase(0, text(a, "name"), cat));
                s.model().notifyLoadCaseChanged(id);
                return done("Cas de charge " + std::to_string(id) + " « " + text(a, "name") + " » créé.", { { "id", static_cast<long long>(id) } });
            });

    reg.add({ "loads.add_nodal", "Ajouter une charge nodale", "Charges", "Force (kN) appliquée à un nœud, repère global.",
              { param("node", "Nœud", VT::Integer), param("case", "Cas de charge", VT::Integer),
                optional(param("fx", "Fx", VT::Real, Q::Force), 0.0), optional(param("fy", "Fy", VT::Real, Q::Force), 0.0),
                optional(param("fz", "Fz", VT::Real, Q::Force), 0.0) },
              { param("id", "Charge créée", VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const int node = static_cast<int>(integer(a, "node")), lc = static_cast<int>(integer(a, "case"));
                if (!s.model().getNode(node)) return fail("Nœud N" + std::to_string(node) + " inexistant.");
                if (!s.model().loadManager().getLoadCase(lc)) return fail("Cas de charge " + std::to_string(lc) + " inexistant.");
                const double fx = real(a, "fx"), fy = real(a, "fy"), fz = real(a, "fz");
                if (std::abs(fx) + std::abs(fy) + std::abs(fz) == 0.0) return fail("Charge nulle.");
                const int id = s.model().loadManager().addNodalLoad(TSA::Model::NodalLoad(0, node, lc, fx, fy, fz));
                s.model().notifyNodalLoadAdded(id);
                return done("Charge nodale " + std::to_string(id) + " sur N" + std::to_string(node) + ".", { { "id", static_cast<long long>(id) } });
            });

    reg.add({ "loads.add_uniform", "Ajouter une charge répartie", "Charges",
              "Charge uniforme (kN/m) sur une poutre, sens de la pesanteur pour q > 0.",
              { param("beam", "Poutre", VT::Integer), param("case", "Cas de charge", VT::Integer),
                param("q", "Intensité", VT::Real, Q::ForcePerLength) },
              { param("id", "Charge créée", VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const int beam = static_cast<int>(integer(a, "beam")), lc = static_cast<int>(integer(a, "case"));
                if (!s.model().getBeam(beam)) return fail("Poutre " + std::to_string(beam) + " inexistante.");
                if (!s.model().loadManager().getLoadCase(lc)) return fail("Cas de charge " + std::to_string(lc) + " inexistant.");
                const int id = s.model().loadManager().addMemberLoad(TSA::Model::MemberLoad::uniform(beam, lc, real(a, "q")));
                s.model().notifyMemberLoadAdded(id);
                return done("Charge répartie " + std::to_string(id) + " sur la poutre " + std::to_string(beam) + ".", { { "id", static_cast<long long>(id) } });
            });

    // --- Requêtes (lecture seule) -----------------------------------------------------------------
    reg.add({ "query.model_summary", "Résumé du modèle", "Requêtes", "Effectifs du modèle.", {},
              { param("nodes", "Nœuds", VT::Integer), param("beams", "Poutres", VT::Integer),
                param("columns", "Poteaux", VT::Integer), param("load_cases", "Cas de charge", VT::Integer) }, false },
            [](auto& s, const Arguments&) {
                const Model& m = s.model();
                // Les barres créées par addBar sont rangées ensemble et distinguées par leur rôle.
                long long b = 0, c = static_cast<long long>(m.columns().size());
                for (const auto& [id, bar] : m.beams()) (bar.role() == TSA::Model::BarRole::Column ? c : b) += 1;
                const auto n = static_cast<long long>(m.nodes().size()),
                           lc = static_cast<long long>(m.loadManager().loadCases().size());
                std::ostringstream o;
                o << n << " nœud(s), " << b << " poutre(s), " << c << " poteau(x), " << lc << " cas de charge.";
                return done(o.str(), { { "nodes", n }, { "beams", b }, { "columns", c }, { "load_cases", lc } });
            });

    reg.add({ "query.node", "Lire un nœud", "Requêtes", "Position d'un nœud.", { param("id", "Nœud", VT::Integer) },
              { param("position", "Position", VT::Point3, Q::Length) }, false },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "id"));
                const auto* n = s.model().getNode(id);
                if (!n) return fail("Nœud N" + std::to_string(id) + " inexistant.");
                const Point3 p { n->x(), n->y(), n->z() };
                return done("N" + std::to_string(id) + " : " + formatValue(p), { { "position", p } });
            });

    // --- Analyse (contrôleur partagé de la session) ----------------------------------------------
    reg.add({ "analysis.run", "Calculer", "Analyse",
              "Calcule le modèle avec les réglages d'analyse du projet ; moteur et axe de grille facultatifs "
              "(« custom2d » + axe « B » : portique plan de l'axe B ; axe « * » : modèle complet ; axe « plan » : plan "
              "du modèle, s'il est plan). Les réglages "
              "modifiés sont enregistrés avec le projet. Calcul synchrone : les résultats sont publiés à la fin.",
              { optional(param("engine", "Moteur", VT::Text), std::string()),
                optional(param("axis", "Axe de grille", VT::Text), std::string()),
                optional(param("export_system", "Exporter K·U = F", VT::Bool), false) },
              { param("max_displacement", "Déplacement max", VT::Real, Q::Length),
                param("max_moment", "Moment max", VT::Real, Q::Moment),
                param("equations", "Équations", VT::Integer) }, false },
            [](auto& s, const Arguments& a) {
                using namespace TSA::Analysis;
                AnalysisController& ac = s.analysis();
                if (ac.isRunning()) return fail("Un calcul est déjà en cours.");
                AnalysisContext ctx = ac.context();
                const std::string engineId = text(a, "engine");
                if (!engineId.empty())
                {
                    if (!ac.registry().engine(engineId)) return fail("Moteur « " + engineId + " » inconnu.");
                    ctx.engineId = engineId;
                }
                const std::string axis = text(a, "axis");
                if (axis == "*")
                {
                    ctx.scope = AnalysisScope {};
                }
                else if (upper(axis) == "PLAN")
                {
                    ctx.scope = AnalysisScope {};
                    ctx.scope.type = ScopeType::ModelPlane;
                }
                else if (!axis.empty())
                {
                    bool found = false;
                    for (const auto& o : AnalysisScopeResolver::availableScopes(s.model(), &s.grids()))
                        if (o.scope.type == ScopeType::GridAxis && upper(o.scope.axisLabel) == upper(axis))
                        {
                            ctx.scope = o.scope;
                            found = true;
                            break;
                        }
                    if (!found) return fail("Axe de grille « " + axis + " » introuvable.");
                }
                const AnalysisEngine* engine = ac.registry().engine(ctx.engineId);
                if (!engine) return fail("Aucun moteur d'analyse sélectionné.");
                const AnalysisCapabilities caps = engine->capabilities();
                // Dimension : celle que le moteur sait calculer (plan pour un moteur 2D seul).
                if (!caps.supportsDimension(ctx.dimension))
                    ctx.dimension = caps.supports2D ? AnalysisDimension::Plane2D : AnalysisDimension::Space3D;
                if (std::get<bool>(a.at("export_system")))
                {
                    QJsonObject settings = ctx.settingsFor(ctx.engineId);
                    settings.insert("exportSystem", true);
                    ctx.engineSettings[ctx.engineId] = settings;
                }
                ac.setContext(ctx);
                ac.storeContextInModel();

                const PreparedAnalysis prepared = ac.prepare(ctx);
                if (!prepared.canRun())
                {
                    std::string why;
                    for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) why += (why.empty() ? "" : " ; ") + e;
                    return fail("Analyse impossible : " + (why.empty() ? std::string("modèle d'analyse non extrait.") : why));
                }
                const AnalysisRunResult run = ac.runBlocking(ctx, prepared);
                if (!run.success || !ac.results()) return fail("Échec du calcul : " + run.message);
                const ResultsModel& r = *ac.results();
                const double toKN = r.units().force == "N" ? 1.0e-3 : 1.0;
                const ResultsSummary sum = r.summary();
                const long long equations = r.advanced().available ? r.advanced().dofMap.equationCount() : 0;
                std::ostringstream o;
                o << engine->info().name << " : δmax = " << sum.maxDisplacement * 1000.0 << " mm (N" << sum.maxDisplacementNodeId
                  << "), Mmax = " << sum.maxBendingMoment * toKN << " kN·m";
                if (equations > 0) o << ", " << equations << " équation(s)";
                return done(o.str(), { { "max_displacement", sum.maxDisplacement },
                                       { "max_moment", sum.maxBendingMoment * toKN },
                                       { "equations", equations } });
            });

    reg.add({ "results.summary", "Résumé des résultats", "Résultats",
              "Valeurs extrêmes du dernier calcul (déplacement, moment, réaction verticale totale) et validité.",
              {},
              { param("max_displacement", "Déplacement max", VT::Real, Q::Length),
                param("max_displacement_node", "Nœud du déplacement max", VT::Integer),
                param("max_moment", "Moment max", VT::Real, Q::Moment),
                param("reaction_fz", "Réaction verticale totale", VT::Real, Q::Force),
                param("up_to_date", "À jour", VT::Bool) }, false },
            [](auto& s, const Arguments&) {
                const auto r = s.analysis().results();
                if (!r) return fail("Aucun résultat : lancez d'abord un calcul (analysis.run).");
                const double toKN = r->units().force == "N" ? 1.0e-3 : 1.0;
                const auto sum = r->summary();
                const bool upToDate = s.analysis().resultsUpToDate();
                std::ostringstream o;
                o << "δmax = " << sum.maxDisplacement * 1000.0 << " mm (N" << sum.maxDisplacementNodeId << "), Mmax = "
                  << sum.maxBendingMoment * toKN << " kN·m, ΣRz = " << r->equilibrium().reactionFz * toKN << " kN"
                  << (upToDate ? "" : " — RÉSULTATS OBSOLÈTES");
                return done(o.str(), { { "max_displacement", sum.maxDisplacement },
                                       { "max_displacement_node", static_cast<long long>(sum.maxDisplacementNodeId) },
                                       { "max_moment", sum.maxBendingMoment * toKN },
                                       { "reaction_fz", r->equilibrium().reactionFz * toKN },
                                       { "up_to_date", upToDate } });
            });

    reg.add({ "results.node_displacement", "Déplacement d'un nœud", "Résultats",
              "Translation (m) et rotation (rad) calculées d'un nœud, repère global.",
              { param("id", "Nœud", VT::Integer) },
              { param("displacement", "Translation", VT::Point3, Q::Length),
                param("rotation", "Rotation (rad)", VT::Point3),
                param("magnitude", "Norme", VT::Real, Q::Length) }, false },
            [](auto& s, const Arguments& a) {
                const auto r = s.analysis().results();
                if (!r) return fail("Aucun résultat : lancez d'abord un calcul (analysis.run).");
                const int id = static_cast<int>(integer(a, "id"));
                if (!r->getNodeDisplacement(id)) return fail("Aucun résultat pour le nœud N" + std::to_string(id) + ".");
                const auto d = r->nodeDisplacement(id);
                const Point3 u { d.ux, d.uy, d.uz }, rot { d.rx, d.ry, d.rz };
                const double norm = std::sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz);
                return done("N" + std::to_string(id) + " : u = " + formatValue(u) + " m, |u| = " + formatValue(norm * 1000.0) + " mm",
                            { { "displacement", u }, { "rotation", rot }, { "magnitude", norm } });
            });
}

const CommandRegistry& CommandRegistry::builtIn()
{
    return global();
}

CommandRegistry& CommandRegistry::global()
{
    static CommandRegistry registry = [] {
        CommandRegistry r;
        registerBuiltInCommands(r);
        return r;
    }();
    return registry;
}

// -----------------------------------------------------------------------------
// Ligne de commande
// -----------------------------------------------------------------------------

namespace
{
std::vector<std::string> tokenize(const std::string& line, std::string* error)
{
    std::vector<std::string> tokens;
    std::string cur;
    bool quoted = false, any = false;
    for (char c : line)
    {
        if (c == '"') { quoted = !quoted; any = true; continue; }
        if (!quoted && std::isspace(static_cast<unsigned char>(c)))
        {
            if (any) tokens.push_back(cur);
            cur.clear();
            any = false;
            continue;
        }
        cur += c;
        any = true;
    }
    if (quoted && error) *error = "Guillemet non fermé.";
    if (any) tokens.push_back(cur);
    return tokens;
}

bool parseValue(const std::string& raw, ValueType type, Value& out)
{
    try
    {
        std::size_t used = 0;
        switch (type)
        {
        case ValueType::Bool:
        {
            const std::string v = upper(raw);
            if (v == "TRUE" || v == "VRAI" || v == "1") out = true;
            else if (v == "FALSE" || v == "FAUX" || v == "0") out = false;
            else return false;
            return true;
        }
        case ValueType::Integer: out = std::stoll(raw, &used); return used == raw.size();
        case ValueType::Real: out = std::stod(raw, &used); return used == raw.size();
        case ValueType::Text: out = raw; return true;
        case ValueType::Point3:
        case ValueType::IdList:
        {
            std::vector<std::string> parts;
            std::stringstream ss(raw);
            for (std::string p; std::getline(ss, p, ',');) parts.push_back(p);
            if (type == ValueType::Point3)
            {
                if (parts.size() != 3) return false;
                Point3 p {};
                for (int i = 0; i < 3; ++i) p[i] = std::stod(parts[i]);
                out = p;
            }
            else
            {
                std::vector<int> ids;
                for (const auto& p : parts) ids.push_back(std::stoi(p));
                out = ids;
            }
            return true;
        }
        }
    }
    catch (const std::exception&)
    {
    }
    return false;
}
} // namespace

bool parseCommandLine(const CommandRegistry& registry, const std::string& line, std::string& commandId, Arguments& args,
                      std::string* errorOut)
{
    auto error = [errorOut](const std::string& e) {
        if (errorOut) *errorOut = e;
        return false;
    };
    std::string tokError;
    const auto tokens = tokenize(line, &tokError);
    if (!tokError.empty()) return error(tokError);
    if (tokens.empty()) return error("Ligne vide.");
    const CommandSpec* spec = registry.find(tokens[0]);
    if (!spec) return error("Commande inconnue : « " + tokens[0] + " ». Tapez « help » pour la liste.");

    args.clear();
    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const auto eq = tokens[i].find('=');
        if (eq == std::string::npos) return error("Argument « " + tokens[i] + " » : syntaxe nom=valeur attendue.");
        const std::string name = tokens[i].substr(0, eq), raw = tokens[i].substr(eq + 1);
        const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(),
                                    [&](const ParameterSpec& s) { return s.name == name; });
        if (p == spec->parameters.end()) return error("Paramètre inconnu « " + name + " » pour " + spec->id + ".");
        Value v;
        if (!parseValue(raw, p->type, v))
            return error("Paramètre « " + name + " » : " + typeName(p->type) + " attendu, reçu « " + raw + " ».");
        args[name] = v;
    }
    commandId = spec->id;
    return true;
}

CommandResult executeCommandLine(const CommandRegistry& registry, TSA::Project::ProjectSession& session, const std::string& line)
{
    std::string id, error;
    Arguments args;
    if (!parseCommandLine(registry, line, id, args, &error)) return fail(error);
    return registry.execute(session, id, std::move(args));
}

bool tokenizeCommandLine(const std::string& line, std::vector<std::string>& tokens, std::string* error)
{
    std::string e;
    tokens = tokenize(line, &e);
    if (error) *error = e;
    return e.empty();
}

bool parseArgument(const std::string& raw, ValueType type, Value& out)
{
    return parseValue(raw, type, out);
}

std::string formatArgument(const Value& v)
{
    std::ostringstream o;
    o.precision(15);
    if (std::holds_alternative<bool>(v)) o << (std::get<bool>(v) ? "true" : "false");
    else if (std::holds_alternative<long long>(v)) o << std::get<long long>(v);
    else if (std::holds_alternative<double>(v)) o << std::get<double>(v);
    else if (std::holds_alternative<std::string>(v))
    {
        const std::string& s = std::get<std::string>(v);
        const bool quote = s.empty() || s.find_first_of(" \t=") != std::string::npos;
        o << (quote ? "\"" + s + "\"" : s);
    }
    else if (std::holds_alternative<Point3>(v))
    {
        const auto& p = std::get<Point3>(v);
        o << p[0] << ',' << p[1] << ',' << p[2];
    }
    else if (std::holds_alternative<std::vector<int>>(v))
    {
        const auto& ids = std::get<std::vector<int>>(v);
        for (std::size_t i = 0; i < ids.size(); ++i) o << (i ? "," : "") << ids[i];
    }
    return o.str();
}

std::string formatValue(const Value& v)
{
    std::ostringstream o;
    if (std::holds_alternative<bool>(v)) o << (std::get<bool>(v) ? "vrai" : "faux");
    else if (std::holds_alternative<long long>(v)) o << std::get<long long>(v);
    else if (std::holds_alternative<double>(v)) o << std::get<double>(v);
    else if (std::holds_alternative<std::string>(v)) o << std::get<std::string>(v);
    else if (std::holds_alternative<Point3>(v))
    {
        const auto& p = std::get<Point3>(v);
        o << p[0] << ", " << p[1] << ", " << p[2];
    }
    else if (std::holds_alternative<std::vector<int>>(v))
    {
        const auto& ids = std::get<std::vector<int>>(v);
        for (std::size_t i = 0; i < ids.size(); ++i) o << (i ? "," : "") << ids[i];
    }
    return o.str();
}

const char* typeName(ValueType t)
{
    switch (t)
    {
    case ValueType::Bool: return "booléen";
    case ValueType::Integer: return "entier";
    case ValueType::Real: return "réel";
    case ValueType::Text: return "texte";
    case ValueType::Point3: return "point x,y,z";
    case ValueType::IdList: return "liste d'identifiants";
    }
    return "?";
}

const char* quantityUnit(Quantity q)
{
    switch (q)
    {
    case Quantity::Length: return "m";
    case Quantity::Force: return "kN";
    case Quantity::Moment: return "kN·m";
    case Quantity::ForcePerLength: return "kN/m";
    case Quantity::Angle: return "°";
    case Quantity::None: break;
    }
    return "";
}

} // namespace TSA::Automation
