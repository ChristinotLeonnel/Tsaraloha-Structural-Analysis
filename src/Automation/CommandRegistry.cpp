#include "CommandRegistry.h"

#include "../Analysis/AnalysisController.h"
#include "../Analysis/ResultsModel.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Mesh/MeshGeometry.h"
#include "../Mesh/MeshProject.h"
#include "../Mesh/Solvers/OpenSeesMeshAdapter.h"
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

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace TSA::Automation
{

// Traduction : titres, descriptions et libellés du registre marqués QT_TRANSLATE_NOOP dans le contexte
// « TSA::Automation::Commands » (texte source conservé dans les spécifications, traduit à l'affichage : recherche
// globale, Blueprint) ; messages des commandes traduits dans le même contexte. Les identifiants ne sont jamais traduits.
namespace
{
QString qs(const std::string& s) { return QString::fromStdString(s); }

/// Nombre écrit comme par le flux (même format qu'avant la traduction des messages).
QString qn(double v)
{
    std::ostringstream o;
    o << v;
    return QString::fromStdString(o.str());
}

/// Texte source du registre (titre, libellé, nom de type) dans la langue de l'interface.
QString shown(const std::string& source)
{
    return QCoreApplication::translate("TSA::Automation::Commands", source.c_str());
}
} // namespace

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
    case ValueType::Object: return std::holds_alternative<ObjectRef>(v) && std::get<ObjectRef>(v) != nullptr;
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
        r.message = QCoreApplication::translate("TSA::Automation::Commands", "Commande inconnue : « %1 ».").arg(qs(id)).toStdString();
        return r;
    }
    const Entry& e = m_entries[it->second];
    for (const auto& [name, value] : args)
    {
        const auto p = std::find_if(e.spec.parameters.begin(), e.spec.parameters.end(),
                                    [&](const ParameterSpec& s) { return s.name == name; });
        if (p == e.spec.parameters.end())
        {
            r.message = QCoreApplication::translate("TSA::Automation::Commands", "Paramètre inconnu « %1 » pour %2.").arg(qs(name), qs(id)).toStdString();
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
                r.message = QCoreApplication::translate("TSA::Automation::Commands", "Paramètre requis manquant : « %1 » (%2).")
                                .arg(qs(p.name), shown(p.label))
                                .toStdString();
                return r;
            }
            args[p.name] = p.defaultValue;
            continue;
        }
        if (!matches(a->second, p.type))
        {
            r.message = QCoreApplication::translate("TSA::Automation::Commands", "Paramètre « %1 » : %2 attendu, reçu « %3 ».")
                            .arg(qs(p.name), shown(typeName(p.type)), qs(formatValue(a->second)))
                            .toStdString();
            return r;
        }
        a->second = normalized(a->second, p.type);
    }

    if (!e.spec.modifiesModel) return e.handler(session, args);

    TSA::UndoRedo::EditTransaction tx(session.model(), shown(e.spec.title).toStdString());   // libellé Annuler / Rétablir affiché
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
    // Tailles du catalogue (Section.h) : une taille absente n'est jamais remplacée par un profil par défaut
    static const std::vector<int> ipe { 100, 120, 140, 160, 180, 200, 220, 240, 270, 300, 330, 360, 400 };
    static const std::vector<int> he { 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300 };
    static const std::vector<int> upn { 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300 };
    const std::vector<int>* sizes = family == "IPE" ? &ipe : (family == "HEA" || family == "HEB") ? &he : family == "UPN" ? &upn : nullptr;
    if (sizes)
    {
        std::size_t used = 0;
        int n = 0;
        try { n = std::stoi(size, &used); } catch (const std::exception&) { used = 0; }
        if (used == 0 || used != size.size() || std::find(sizes->begin(), sizes->end(), n) == sizes->end())
        {
            if (error)
                *error = QCoreApplication::translate("TSA::Automation::Commands", "Profil « %1 » absent du catalogue (%2 : %3).")
                             .arg(qs(spec), qs(family), qs([&] {
                                      std::string l;
                                      for (int v : *sizes) l += (l.empty() ? "" : ", ") + std::to_string(v);
                                      return l;
                                  }()))
                             .toStdString();
            return false;
        }
    }
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
            if (error)
                *error = QCoreApplication::translate("TSA::Automation::Commands", "Section inconnue « %1 » (IPE n, HEA n, HEB n, UPN n, RECT bxh, CIRC d).")
                             .arg(qs(spec))
                             .toStdString();
            return false;
        }
    }
    catch (const std::exception&)
    {
        if (error) *error = QCoreApplication::translate("TSA::Automation::Commands", "Dimensions de section invalides : « %1 ».").arg(qs(spec)).toStdString();
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
    const QString shownKind = QCoreApplication::translate("TSA::Automation::Commands", kind);   // kind : texte source marqué
    if (!m.getNode(start) || !m.getNode(end))
        return fail(QCoreApplication::translate("TSA::Automation::Commands", "%1 : nœud d'extrémité inexistant.").arg(shownKind).toStdString());
    if (start == end)
        return fail(QCoreApplication::translate("TSA::Automation::Commands", "%1 : les deux extrémités sont le même nœud.").arg(shownKind).toStdString());
    TSA::Model::Section section;
    std::string error;
    if (!parseSection(text(a, "section"), section, &error)) return fail(error);
    TSA::Model::Material material;
    if (!parseMaterial(text(a, "material"), material))
        return fail(QCoreApplication::translate("TSA::Automation::Commands", "Matériau inconnu « %1 » (S235, S355, C25/30, C30/37, C24).")
                        .arg(qs(text(a, "material")))
                        .toStdString());
    const int id = m.addBar(start, end, section, material, role, real(a, "rotation"), text(a, "name"));
    if (id <= 0) return fail(QCoreApplication::translate("TSA::Automation::Commands", "%1 non créé(e).").arg(shownKind).toStdString());
    return done(QCoreApplication::translate("TSA::Automation::Commands", "%1 %2 créé(e) entre N%3 et N%4.")
                    .arg(shownKind, QString::number(id), QString::number(start), QString::number(end))
                    .toStdString(),
                { { "id", static_cast<long long>(id) } });
}
} // namespace

namespace
{
void registerLabCommands(CommandRegistry& reg);
} // namespace

void registerBuiltInCommands(CommandRegistry& reg)
{
    using VT = ValueType;
    using Q = Quantity;

    // --- Modèle -----------------------------------------------------------------------------------
    reg.add({ "model.create_node", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Créer un nœud"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajoute un nœud aux coordonnées données (m)."),
              { param("position", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Position"), VT::Point3, Q::Length),
                optional(param("name", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nom"), VT::Text), std::string()) },
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud créé"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const auto p = std::get<Point3>(a.at("position"));
                const int id = s.model().addNode(p[0], p[1], p[2], "", text(a, "name"));
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Nœud N%1 créé.").arg(id).toStdString(),
                            { { "id", static_cast<long long>(id) } });
            });

    const std::vector<ParameterSpec> barParams = {
        param("start", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud de départ"), VT::Integer),
        param("end", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud d'arrivée"), VT::Integer),
        optional(param("section", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Section"), VT::Text), std::string("IPE 300")),
        optional(param("material", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Matériau"), VT::Text), std::string("S235")),
        optional(param("rotation", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Rotation de section"), VT::Real, Q::Angle), 0.0),
        optional(param("name", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nom"), VT::Text), std::string()),
    };
    reg.add({ "model.create_beam", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Créer une poutre"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajoute une poutre entre deux nœuds existants."),
              barParams, { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poutre créée"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                return createBar(s, a, TSA::Model::BarRole::Beam, QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poutre"));
            });
    reg.add({ "model.create_column", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Créer un poteau"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajoute un poteau entre deux nœuds existants."),
              barParams, { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poteau créé"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                return createBar(s, a, TSA::Model::BarRole::Column, QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poteau"));
            });

    reg.add({ "model.set_support", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Définir un appui"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands",
                                "Appui d'un nœud : fixed (encastrement), pinned (articulation), roller (appui simple), free (aucun)."),
              { param("node", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud"), VT::Integer),
                param("type", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Type"), VT::Text) }, {}, true },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "node"));
                auto* n = s.model().getNode(id);
                if (!n) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Nœud N%1 inexistant.").arg(id).toStdString());
                const std::string t = upper(text(a, "type"));
                if (t == "FIXED") n->setSupport(TSA::Model::SupportDefinition::fixed());
                else if (t == "PINNED") n->setSupport(TSA::Model::SupportDefinition::pinned());
                else if (t == "ROLLER") n->setSupport(TSA::Model::SupportDefinition::roller());
                else if (t == "FREE") n->setSupport(TSA::Model::SupportDefinition::free());
                else
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Type d'appui inconnu « %1 » (fixed, pinned, roller, free).")
                                    .arg(qs(text(a, "type")))
                                    .toStdString());
                s.model().notifyNodeModified(id);
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Appui « %1 » sur N%2.")
                                .arg(qs(text(a, "type")), QString::number(id))
                                .toStdString());
            });

    reg.add({ "model.set_section", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Affecter section et matériau"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands",
                                "Remplace la section (IPE n, HEA n, HEB n, UPN n, RECT bxh, CIRC d) et, si donné, le matériau d'une barre."),
              { param("bar", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Barre"), VT::Integer),
                param("section", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Section"), VT::Text),
                optional(param("material", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Matériau"), VT::Text), std::string()) },
              {}, true },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "bar"));
                auto* bar = s.model().getBar(id);
                if (!bar) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Barre %1 inexistante.").arg(id).toStdString());
                TSA::Model::Section section;
                std::string error;
                if (!parseSection(text(a, "section"), section, &error)) return fail(error);
                TSA::Model::Material material = bar->material();
                const std::string mat = text(a, "material");
                if (!mat.empty() && !parseMaterial(mat, material))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Matériau inconnu « %1 » (S235, S355, C25/30, C30/37, C24).")
                                    .arg(qs(mat))
                                    .toStdString());
                bar->setSection(section);
                bar->setMaterial(material);
                s.model().notifyBeamModified(id);
                if (mat.empty())
                    return done(QCoreApplication::translate("TSA::Automation::Commands", "Barre %1 : section « %2 ».")
                                    .arg(QString::number(id), qs(text(a, "section")))
                                    .toStdString());
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Barre %1 : section « %2 », matériau %3.")
                                .arg(QString::number(id), qs(text(a, "section")), qs(mat))
                                .toStdString());
            });

    // --- Charges ----------------------------------------------------------------------------------
    reg.add({ "loads.create_case", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Créer un cas de charge"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charges"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajoute un cas de charge (catégorie : dead, live, wind, snow)."),
              { param("name", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nom"), VT::Text),
                optional(param("category", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Catégorie"), VT::Text), std::string("dead")) },
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Cas créé"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                using C = TSA::Model::LoadCaseCategory;
                const std::string c = upper(text(a, "category"));
                C cat = C::Dead;
                if (c == "LIVE") cat = C::Live;
                else if (c == "WIND") cat = C::Wind;
                else if (c == "SNOW") cat = C::Snow;
                else if (c != "DEAD")
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Catégorie inconnue « %1 » (dead, live, wind, snow).")
                                    .arg(qs(text(a, "category")))
                                    .toStdString());
                const int id = s.model().loadManager().addLoadCase(TSA::Model::LoadCase(0, text(a, "name"), cat));
                s.model().notifyLoadCaseChanged(id);
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Cas de charge %1 « %2 » créé.")
                                .arg(QString::number(id), qs(text(a, "name")))
                                .toStdString(),
                            { { "id", static_cast<long long>(id) } });
            });

    reg.add({ "loads.add_nodal", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajouter une charge nodale"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charges"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Force (kN) appliquée à un nœud, repère global."),
              { param("node", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud"), VT::Integer),
                param("case", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Cas de charge"), VT::Integer),
                optional(param("fx", "Fx", VT::Real, Q::Force), 0.0), optional(param("fy", "Fy", VT::Real, Q::Force), 0.0),
                optional(param("fz", "Fz", VT::Real, Q::Force), 0.0) },
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charge créée"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const int node = static_cast<int>(integer(a, "node")), lc = static_cast<int>(integer(a, "case"));
                if (!s.model().getNode(node))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Nœud N%1 inexistant.").arg(node).toStdString());
                if (!s.model().loadManager().getLoadCase(lc))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Cas de charge %1 inexistant.").arg(lc).toStdString());
                const double fx = real(a, "fx"), fy = real(a, "fy"), fz = real(a, "fz");
                if (std::abs(fx) + std::abs(fy) + std::abs(fz) == 0.0)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Charge nulle.").toStdString());
                const int id = s.model().loadManager().addNodalLoad(TSA::Model::NodalLoad(0, node, lc, fx, fy, fz));
                s.model().notifyNodalLoadAdded(id);
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Charge nodale %1 sur N%2.")
                                .arg(QString::number(id), QString::number(node))
                                .toStdString(),
                            { { "id", static_cast<long long>(id) } });
            });

    reg.add({ "loads.add_uniform", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Ajouter une charge répartie"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charges"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charge uniforme (kN/m) sur une poutre, sens de la pesanteur pour q > 0."),
              { param("beam", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poutre"), VT::Integer),
                param("case", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Cas de charge"), VT::Integer),
                param("q", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Intensité"), VT::Real, Q::ForcePerLength) },
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Charge créée"), VT::Integer) }, true },
            [](auto& s, const Arguments& a) {
                const int beam = static_cast<int>(integer(a, "beam")), lc = static_cast<int>(integer(a, "case"));
                if (!s.model().getBeam(beam))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Poutre %1 inexistante.").arg(beam).toStdString());
                if (!s.model().loadManager().getLoadCase(lc))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Cas de charge %1 inexistant.").arg(lc).toStdString());
                const int id = s.model().loadManager().addMemberLoad(TSA::Model::MemberLoad::uniform(beam, lc, real(a, "q")));
                s.model().notifyMemberLoadAdded(id);
                return done(QCoreApplication::translate("TSA::Automation::Commands", "Charge répartie %1 sur la poutre %2.")
                                .arg(QString::number(id), QString::number(beam))
                                .toStdString(),
                            { { "id", static_cast<long long>(id) } });
            });

    // --- Requêtes (lecture seule) -----------------------------------------------------------------
    reg.add({ "query.model_summary", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résumé du modèle"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Requêtes"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Effectifs du modèle."), {},
              { param("nodes", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœuds"), VT::Integer),
                param("beams", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poutres"), VT::Integer),
                param("columns", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poteaux"), VT::Integer),
                param("load_cases", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Cas de charge"), VT::Integer) }, false },
            [](auto& s, const Arguments&) {
                const Model& m = s.model();
                // Les barres créées par addBar sont rangées ensemble et distinguées par leur rôle.
                long long b = 0, c = static_cast<long long>(m.columns().size());
                for (const auto& [id, bar] : m.beams()) (bar.role() == TSA::Model::BarRole::Column ? c : b) += 1;
                const auto n = static_cast<long long>(m.nodes().size()),
                           lc = static_cast<long long>(m.loadManager().loadCases().size());
                return done(QCoreApplication::translate("TSA::Automation::Commands", "%1 nœud(s), %2 poutre(s), %3 poteau(x), %4 cas de charge.")
                                .arg(QString::number(n), QString::number(b), QString::number(c), QString::number(lc))
                                .toStdString(),
                            { { "nodes", n }, { "beams", b }, { "columns", c }, { "load_cases", lc } });
            });

    reg.add({ "query.node", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Lire un nœud"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Requêtes"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Position d'un nœud."),
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud"), VT::Integer) },
              { param("position", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Position"), VT::Point3, Q::Length) }, false },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "id"));
                const auto* n = s.model().getNode(id);
                if (!n) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Nœud N%1 inexistant.").arg(id).toStdString());
                const Point3 p { n->x(), n->y(), n->z() };
                return done("N" + std::to_string(id) + " : " + formatValue(p), { { "position", p } });
            });

    reg.add({ "query.bar", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Lire une barre"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Requêtes"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœuds, longueur et propriétés de section et de matériau d'une barre."),
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Barre"), VT::Integer) },
              { param("start", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud de départ"), VT::Integer),
                param("end", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud d'arrivée"), VT::Integer),
                param("length", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Longueur"), VT::Real, Q::Length),
                param("area", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Aire (m²)"), VT::Real),
                param("iy", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Iy, axe fort (m⁴)"), VT::Real),
                param("iz", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Iz, axe faible (m⁴)"), VT::Real),
                param("e", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Module d'Young (kN/m²)"), VT::Real),
                param("rotation", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Rotation de section"), VT::Real, Q::Angle) }, false },
            [](auto& s, const Arguments& a) {
                const int id = static_cast<int>(integer(a, "id"));
                const auto* b = s.model().getBar(id);
                if (!b) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Barre %1 inexistante.").arg(id).toStdString());
                const double L = b->length(s.model()), E = b->material().E / 1000.0;
                const std::string msg = QCoreApplication::translate("TSA::Automation::Commands",
                                                                     "Barre %1 : N%2 → N%3, L = %4 m, A = %5 m², Iy = %6 m⁴, E = %7 kN/m²")
                                             .arg(QString::number(id), QString::number(b->startNodeId()), QString::number(b->endNodeId()), qn(L),
                                                  qn(b->section().area()), qn(b->section().iy()), qn(E))
                                             .toStdString();
                return done(msg, { { "start", static_cast<long long>(b->startNodeId()) }, { "end", static_cast<long long>(b->endNodeId()) },
                                       { "length", L }, { "area", b->section().area() }, { "iy", b->section().iy() }, { "iz", b->section().iz() },
                                       { "e", E }, { "rotation", b->rotation() } });
            });

    // --- Analyse (contrôleur partagé de la session) ----------------------------------------------
    reg.add({ "analysis.run", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Calculer"), QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Analyse"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands",
                                "Calcule le modèle avec les réglages d'analyse du projet ; moteur et axe de grille facultatifs "
                                "(« custom2d » + axe « B » : portique plan de l'axe B ; axe « * » : modèle complet ; axe « plan » : plan "
                                "du modèle, s'il est plan). Poids propre : « oui » / « non », vide = réglage du projet. Cas de charge ou "
                                "combinaison : nom ou numéro (l'un ou l'autre), vide = réglage du projet. Les réglages "
                                "modifiés sont enregistrés avec le projet. Calcul synchrone : les résultats sont publiés à la fin."),
              { optional(param("engine", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Moteur"), VT::Text), std::string()),
                optional(param("axis", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Axe de grille"), VT::Text), std::string()),
                optional(param("export_system", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Exporter K·U = F"), VT::Bool), false),
                optional(param("self_weight", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Poids propre"), VT::Text), std::string()),
                optional(param("case", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Cas de charge (nom ou numéro)"), VT::Text), std::string()),
                optional(param("combination", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Combinaison (nom ou numéro)"), VT::Text), std::string()) },
              { param("max_displacement", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Déplacement max"), VT::Real, Q::Length),
                param("max_moment", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Moment max"), VT::Real, Q::Moment),
                param("equations", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Équations"), VT::Integer) }, false },
            [](auto& s, const Arguments& a) {
                using namespace TSA::Analysis;
                AnalysisController& ac = s.analysis();
                if (ac.isRunning()) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Un calcul est déjà en cours.").toStdString());
                AnalysisContext ctx = ac.context();
                const std::string engineId = text(a, "engine");
                if (!engineId.empty())
                {
                    if (!ac.registry().engine(engineId))
                        return fail(QCoreApplication::translate("TSA::Automation::Commands", "Moteur « %1 » inconnu.").arg(qs(engineId)).toStdString());
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
                    if (!found)
                        return fail(QCoreApplication::translate("TSA::Automation::Commands", "Axe de grille « %1 » introuvable.").arg(qs(axis)).toStdString());
                }
                const AnalysisEngine* engine = ac.registry().engine(ctx.engineId);
                if (!engine) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun moteur d'analyse sélectionné.").toStdString());
                const AnalysisCapabilities caps = engine->capabilities();
                // Dimension : celle que le moteur sait calculer (plan pour un moteur 2D seul).
                if (!caps.supportsDimension(ctx.dimension))
                    ctx.dimension = caps.supports2D ? AnalysisDimension::Plane2D : AnalysisDimension::Space3D;
                const std::string sw = upper(text(a, "self_weight"));
                if (sw == "OUI" || sw == "YES" || sw == "TRUE" || sw == "1") ctx.common.includeSelfWeight = true;
                else if (sw == "NON" || sw == "NO" || sw == "FALSE" || sw == "0") ctx.common.includeSelfWeight = false;
                else if (!sw.empty())
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Poids propre : « %1 » (oui ou non attendu).")
                                    .arg(qs(text(a, "self_weight")))
                                    .toStdString());
                // Chargement : un cas ou une combinaison, désigné par son nom exact ou son numéro
                const std::string caseRef = text(a, "case"), comboRef = text(a, "combination");
                if (!caseRef.empty() && !comboRef.empty())
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Cas de charge ou combinaison : l'un ou l'autre, pas les deux.")
                                    .toStdString());
                auto isNumber = [](const std::string& t) { return !t.empty() && std::all_of(t.begin(), t.end(), [](unsigned char ch) { return std::isdigit(ch); }); };
                const auto& lm = s.model().loadManager();
                if (!caseRef.empty())
                {
                    int id = 0;
                    for (const auto& [cid, lc] : lm.loadCases())
                        if (lc.name() == caseRef || (isNumber(caseRef) && cid == std::stoi(caseRef))) id = cid;
                    if (!id)
                        return fail(QCoreApplication::translate("TSA::Automation::Commands", "Cas de charge « %1 » introuvable.").arg(qs(caseRef)).toStdString());
                    ctx.loadCaseIds = { id };
                    ctx.combinationId = 0;
                }
                if (!comboRef.empty())
                {
                    int id = 0;
                    for (const auto& [cid, cb] : lm.loadCombinations())
                        if (cb.name() == comboRef || (isNumber(comboRef) && cid == std::stoi(comboRef))) id = cid;
                    if (!id)
                        return fail(QCoreApplication::translate("TSA::Automation::Commands", "Combinaison « %1 » introuvable.").arg(qs(comboRef)).toStdString());
                    ctx.combinationId = id;
                    ctx.loadCaseIds.clear();
                }
                if (std::get<bool>(a.at("export_system")))
                {
                    QJsonObject settings = ctx.settingsFor(ctx.engineId);
                    settings.insert("exportSystem", true);
                    ctx.engineSettings[ctx.engineId] = settings;
                }
                ac.setContext(ctx);
                ac.storeContextInModel();

                // Maillage enregistré dans le projet (Gmsh) : transmis s'il est à jour et compatible, comme le fait le
                // bouton Calculer (UI/Mesh/MeshAnalysisBridge) ; sans dialogue ici : obsolète ou non transmissible → refus motivé
                ctx.memberSubdivisions.clear();
                ctx.surfaces = {};
                if (ctx.common.useGeneratedMesh)
                {
                    const auto mp = TSA::Mesh::MeshProject::load(s.model());
                    if (mp.hasMesh)
                    {
                        const auto geometry = TSA::Mesh::MeshGeometry::extract(s.model());
                        if (mp.isStale(geometry.hash))
                            return fail(QCoreApplication::translate("TSA::Automation::Commands",
                                                                    "Maillage obsolète (géométrie modifiée depuis sa génération) : régénérer le maillage, ou "
                                                                    "désactiver « utiliser le maillage généré » dans les réglages du calcul.")
                                            .toStdString());
                        const auto compat = TSA::Mesh::OpenSeesMeshAdapter::check(mp.mesh, geometry, engine->info().name, caps, 3000000, &s.model());
                        if (compat.blocking())
                        {
                            std::string why;
                            for (const auto& i : compat.items)
                                if (i.level == TSA::Mesh::SolverCheckItem::Level::Blocking) why += (why.empty() ? "" : " ; ") + i.message;
                            return fail(QCoreApplication::translate("TSA::Automation::Commands", "Maillage non transmissible à %1 : %2")
                                            .arg(qs(engine->info().name), qs(why))
                                            .toStdString());
                        }
                        ctx.memberSubdivisions = compat.memberSubdivisions;
                        ctx.surfaces = compat.surfaces;
                        ctx.surfaces.meshStamp = mp.record.generated + "|" + mp.record.settingsHash;
                        ctx.meshLabel = QCoreApplication::translate("TSA::Automation::Commands", "maillage %1 du %2")
                                            .arg(qs(mp.record.mesher), qs(mp.record.generated))
                                            .toStdString();
                    }
                }

                const PreparedAnalysis prepared = ac.prepare(ctx);
                if (!prepared.canRun())
                {
                    std::string why;
                    for (const auto& e : prepared.validation.texts(ValidationSeverity::Error)) why += (why.empty() ? "" : " ; ") + e;
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Analyse impossible : %1")
                                    .arg(why.empty() ? QCoreApplication::translate("TSA::Automation::Commands", "modèle d'analyse non extrait.") : qs(why))
                                    .toStdString());
                }
                const AnalysisRunResult run = ac.runBlocking(ctx, prepared);
                if (!run.success || !ac.results())
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Échec du calcul : %1").arg(qs(run.message)).toStdString());
                const ResultsModel& r = *ac.results();
                const double toKN = r.units().force == "N" ? 1.0e-3 : 1.0;
                const ResultsSummary sum = r.summary();
                const long long equations = r.advanced().available ? r.advanced().dofMap.equationCount() : 0;
                const QString engineName = qs(engine->info().name), dmax = qn(sum.maxDisplacement * 1000.0),
                              dnode = QString::number(sum.maxDisplacementNodeId), mmax = qn(sum.maxBendingMoment * toKN);
                const std::string msg =
                    equations > 0 ? QCoreApplication::translate("TSA::Automation::Commands", "%1 : δmax = %2 mm (N%3), Mmax = %4 kN·m, %5 équation(s)")
                                        .arg(engineName, dmax, dnode, mmax, QString::number(equations))
                                        .toStdString()
                                  : QCoreApplication::translate("TSA::Automation::Commands", "%1 : δmax = %2 mm (N%3), Mmax = %4 kN·m")
                                        .arg(engineName, dmax, dnode, mmax)
                                        .toStdString();
                return done(msg, { { "max_displacement", sum.maxDisplacement },
                                       { "max_moment", sum.maxBendingMoment * toKN },
                                       { "equations", equations } });
            });

    reg.add({ "results.summary", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résumé des résultats"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résultats"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands",
                                "Valeurs extrêmes du dernier calcul (déplacement, moment, réaction verticale totale) et validité."),
              {},
              { param("max_displacement", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Déplacement max"), VT::Real, Q::Length),
                param("max_displacement_node", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud du déplacement max"), VT::Integer),
                param("max_moment", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Moment max"), VT::Real, Q::Moment),
                param("reaction_fz", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Réaction verticale totale"), VT::Real, Q::Force),
                param("up_to_date", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "À jour"), VT::Bool) }, false },
            [](auto& s, const Arguments&) {
                const auto r = s.analysis().results();
                if (!r)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun résultat : lancez d'abord un calcul (analysis.run).")
                                    .toStdString());
                const double toKN = r->units().force == "N" ? 1.0e-3 : 1.0;
                const auto sum = r->summary();
                const bool upToDate = s.analysis().resultsUpToDate();
                const QString dmax = qn(sum.maxDisplacement * 1000.0), dnode = QString::number(sum.maxDisplacementNodeId),
                              mmax = qn(sum.maxBendingMoment * toKN), rz = qn(r->equilibrium().reactionFz * toKN);
                const std::string msg =
                    upToDate ? QCoreApplication::translate("TSA::Automation::Commands", "δmax = %1 mm (N%2), Mmax = %3 kN·m, ΣRz = %4 kN")
                                   .arg(dmax, dnode, mmax, rz)
                                   .toStdString()
                             : QCoreApplication::translate("TSA::Automation::Commands",
                                                           "δmax = %1 mm (N%2), Mmax = %3 kN·m, ΣRz = %4 kN — RÉSULTATS OBSOLÈTES")
                                   .arg(dmax, dnode, mmax, rz)
                                   .toStdString();
                return done(msg, { { "max_displacement", sum.maxDisplacement },
                                       { "max_displacement_node", static_cast<long long>(sum.maxDisplacementNodeId) },
                                       { "max_moment", sum.maxBendingMoment * toKN },
                                       { "reaction_fz", r->equilibrium().reactionFz * toKN },
                                       { "up_to_date", upToDate } });
            });

    reg.add({ "results.node_displacement", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Déplacement d'un nœud"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résultats"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Translation (m) et rotation (rad) calculées d'un nœud, repère global."),
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud"), VT::Integer) },
              { param("displacement", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Translation"), VT::Point3, Q::Length),
                param("rotation", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Rotation (rad)"), VT::Point3),
                param("magnitude", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Norme"), VT::Real, Q::Length) }, false },
            [](auto& s, const Arguments& a) {
                const auto r = s.analysis().results();
                if (!r)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun résultat : lancez d'abord un calcul (analysis.run).")
                                    .toStdString());
                const int id = static_cast<int>(integer(a, "id"));
                if (!r->getNodeDisplacement(id))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun résultat pour le nœud N%1.").arg(id).toStdString());
                const auto d = r->nodeDisplacement(id);
                const Point3 u { d.ux, d.uy, d.uz }, rot { d.rx, d.ry, d.rz };
                const double norm = std::sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz);
                return done("N" + std::to_string(id) + " : u = " + formatValue(u) + " m, |u| = " + formatValue(norm * 1000.0) + " mm",
                            { { "displacement", u }, { "rotation", rot }, { "magnitude", norm } });
            });

    registerLabCommands(reg);
}

namespace
{
// Lecture d'efforts et comparaison : enregistrées après les autres résultats (ordre d'affichage du registre)
void registerLabCommands(CommandRegistry& reg)
{
    using VT = ValueType;
    using Q = Quantity;

    reg.add({ "results.node_reaction", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Réaction d'un appui"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résultats"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Réaction calculée d'un nœud appuyé (kN, kN·m), repère global."),
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Nœud"), VT::Integer) },
              { param("force", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Force"), VT::Point3, Q::Force),
                param("moment", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Moment"), VT::Point3, Q::Moment) }, false },
            [](auto& s, const Arguments& a) {
                const auto r = s.analysis().results();
                if (!r)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun résultat : lancez d'abord un calcul (analysis.run).")
                                    .toStdString());
                const int id = static_cast<int>(integer(a, "id"));
                const auto* re = r->getNodeReaction(id);
                if (!re)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucune réaction pour le nœud N%1 (nœud non appuyé ou hors du calcul).")
                                    .arg(id)
                                    .toStdString());
                const double k = r->units().force == "N" ? 1.0e-3 : 1.0;
                const Point3 f { re->rx * k, re->ry * k, re->rz * k }, m { re->mx * k, re->my * k, re->mz * k };
                return done("N" + std::to_string(id) + " : R = " + formatValue(f) + " kN, M = " + formatValue(m) + " kN·m", { { "force", f }, { "moment", m } });
            });

    reg.add({ "results.member_forces", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Efforts d'une barre"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Résultats"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Efforts extrêmes calculés le long d'une barre (stations du moteur, repère local) : N, V, M."),
              { param("id", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Barre"), VT::Integer) },
              { param("n_max", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "N max (traction)"), VT::Real, Q::Force),
                param("n_min", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "N min (compression)"), VT::Real, Q::Force),
                param("v_max", "|V| max", VT::Real, Q::Force), param("m_max", "|M| max", VT::Real, Q::Moment),
                param("stations", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Stations"), VT::Integer) }, false },
            [](auto& s, const Arguments& a) {
                const auto r = s.analysis().results();
                if (!r)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun résultat : lancez d'abord un calcul (analysis.run).")
                                    .toStdString());
                const int id = static_cast<int>(integer(a, "id"));
                const TSA::Analysis::ElementResults* er = nullptr;
                int found = 0;
                for (const auto& [key, e] : r->allElementResults())
                    if (key.id == id && key.kind != TSA::Analysis::StructuralElementKind::Cable) er = &e, ++found;
                if (!er)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Aucun effort pour la barre %1 (hors du calcul).").arg(id).toStdString());
                if (found > 1)
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Barre %1 ambiguë dans les résultats.").arg(id).toStdString());
                std::vector<TSA::Analysis::StationForces> st { er->startForces };
                st.insert(st.end(), er->intermediateStations.begin(), er->intermediateStations.end());
                st.push_back(er->endForces);
                const double k = r->units().force == "N" ? 1.0e-3 : 1.0;
                double nMax = -1e300, nMin = 1e300, vMax = 0.0, mMax = 0.0;
                for (const auto& f : st)
                {
                    nMax = std::max(nMax, f.N * k), nMin = std::min(nMin, f.N * k);
                    vMax = std::max(vMax, std::hypot(f.Vy, f.Vz) * k), mMax = std::max(mMax, std::hypot(f.My, f.Mz) * k);
                }
                nMax += 0.0, nMin += 0.0;   // −0 → 0 (affichage)
                const std::string msg = QCoreApplication::translate("TSA::Automation::Commands",
                                                                    "Barre %1 : N ∈ [%2, %3] kN, |V|max = %4 kN, |M|max = %5 kN·m (%6 stations)")
                                            .arg(QString::number(id), qn(nMin), qn(nMax), qn(vMax), qn(mMax), QString::number(st.size()))
                                            .toStdString();
                return done(msg, { { "n_max", nMax }, { "n_min", nMin }, { "v_max", vMax }, { "m_max", mMax }, { "stations", static_cast<long long>(st.size()) } });
            });

    reg.add({ "compare.values", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Comparer à une référence"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Laboratoire"),
              QT_TRANSLATE_NOOP("TSA::Automation::Commands",
                                "Écarts d'une valeur à sa référence et verdict selon la tolérance mixte |x − réf| ≤ ε_abs + ε_rel·|réf|. "
                                "L'écart relatif n'est pas défini si |réf| ≈ 0 (sortie absente)."),
              { param("value", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Valeur"), VT::Real),
                param("reference", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Référence"), VT::Real),
                optional(param("rel_tol", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Tolérance relative"), VT::Real), 1.0e-6),
                optional(param("abs_tol", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Tolérance absolue"), VT::Real), 1.0e-9) },
              { param("abs_error", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Écart absolu"), VT::Real),
                param("rel_error", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Écart relatif"), VT::Real),
                param("passed", QT_TRANSLATE_NOOP("TSA::Automation::Commands", "Dans la tolérance"), VT::Bool) }, false },
            [](auto&, const Arguments& a) {
                const double x = real(a, "value"), ref = real(a, "reference"), rel = real(a, "rel_tol"), abs = real(a, "abs_tol");
                if (!std::isfinite(x) || !std::isfinite(ref))
                    return fail(QCoreApplication::translate("TSA::Automation::Commands", "Valeur ou référence non finie.").toStdString());
                if (rel < 0.0 || abs < 0.0) return fail(QCoreApplication::translate("TSA::Automation::Commands", "Tolérances négatives.").toStdString());
                const double e = std::abs(x - ref);
                const bool ok = e <= abs + rel * std::abs(ref);
                Arguments out { { "abs_error", e }, { "passed", ok } };
                QString msg;
                if (std::abs(ref) > 1e-12)
                {
                    out["rel_error"] = e / std::abs(ref);
                    msg = ok ? QCoreApplication::translate("TSA::Automation::Commands", "|Δ| = %1, écart relatif %2 % — dans la tolérance")
                             : QCoreApplication::translate("TSA::Automation::Commands", "|Δ| = %1, écart relatif %2 % — HORS TOLÉRANCE");
                    msg = msg.arg(qn(e), qn(e / std::abs(ref) * 100.0));
                }
                else
                    msg = (ok ? QCoreApplication::translate("TSA::Automation::Commands", "|Δ| = %1, réf. ≈ 0 : écart relatif non défini — dans la tolérance")
                              : QCoreApplication::translate("TSA::Automation::Commands", "|Δ| = %1, réf. ≈ 0 : écart relatif non défini — HORS TOLÉRANCE"))
                              .arg(qn(e));
                return done(msg.toStdString(), out);
            });
}

} // namespace

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
    if (quoted && error) *error = QCoreApplication::translate("TSA::Automation::Commands", "Guillemet non fermé.").toStdString();
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
        case ValueType::Object: return false;   // un objet n'a pas de forme textuelle : il vient d'un lien Blueprint
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
    if (tokens.empty()) return error(QCoreApplication::translate("TSA::Automation::Commands", "Ligne vide.").toStdString());
    const CommandSpec* spec = registry.find(tokens[0]);
    if (!spec) return error(QCoreApplication::translate("TSA::Automation::Commands", "Commande inconnue : « %1 ». Tapez « help » pour la liste.").arg(qs(tokens[0])).toStdString());

    args.clear();
    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const auto eq = tokens[i].find('=');
        if (eq == std::string::npos) return error(QCoreApplication::translate("TSA::Automation::Commands", "Argument « %1 » : syntaxe nom=valeur attendue.").arg(qs(tokens[i])).toStdString());
        const std::string name = tokens[i].substr(0, eq), raw = tokens[i].substr(eq + 1);
        const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(),
                                    [&](const ParameterSpec& s) { return s.name == name; });
        if (p == spec->parameters.end()) return error(QCoreApplication::translate("TSA::Automation::Commands", "Paramètre inconnu « %1 » pour %2.").arg(qs(name), qs(spec->id)).toStdString());
        Value v;
        if (!parseValue(raw, p->type, v))
            return error(QCoreApplication::translate("TSA::Automation::Commands", "Paramètre « %1 » : %2 attendu, reçu « %3 ».")
                             .arg(qs(name), shown(typeName(p->type)), qs(raw))
                             .toStdString());
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
    else if (const auto* obj = std::get_if<ObjectRef>(&v); obj && *obj)
        o << '"' << (*obj)->summary() << '"';   // lisible, non relisible : un objet n'a pas de forme textuelle
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
    else if (const auto* obj = std::get_if<ObjectRef>(&v))
        o << (*obj ? (*obj)->summary() : std::string("(objet vide)"));
    return o.str();
}

bool catalogSection(const std::string& spec, TSA::Model::Section& out, std::string* error)
{
    return parseSection(spec, out, error);
}

bool catalogMaterial(const std::string& name, TSA::Model::Material& out)
{
    return parseMaterial(name, out);
}

bool sameValue(const Value& a, const Value& b)
{
    if (a.index() != b.index()) return false;
    if (const auto* oa = std::get_if<ObjectRef>(&a))
    {
        const auto& ob = std::get<ObjectRef>(b);
        if (*oa == ob) return true;
        return *oa && ob && (*oa)->typeName() == ob->typeName() && (*oa)->equals(*ob);
    }
    return a == b;
}

const char* typeName(ValueType t)
{
    switch (t)
    {
    case ValueType::Bool: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "booléen");
    case ValueType::Integer: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "entier");
    case ValueType::Real: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "réel");
    case ValueType::Text: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "texte");
    case ValueType::Point3: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "point x,y,z");
    case ValueType::IdList: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "liste d'identifiants");
    case ValueType::Object: return QT_TRANSLATE_NOOP("TSA::Automation::Commands", "objet");
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
