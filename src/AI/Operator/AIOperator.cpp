#include "AIOperator.h"

#include "../../Analysis/AnalysisController.h"
#include "../../Automation/CommandRegistry.h"
#include "../../Model/Model.h"
#include "../../Project/ProjectSession.h"

#include <QDateTime>
#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace TSA::AI
{

namespace
{
using TSA::Automation::CommandRegistry;
using TSA::Automation::ValueType;

constexpr double kPositionTolerance = 1e-9; // m : la commande stocke exactement les coordonnées reçues

QString qs(const std::string& s) { return QString::fromStdString(s); }

/// Valeur JSON reçue du modèle de langage → valeur typée du registre. Accepte aussi la syntaxe de la ligne de commande
/// (« 0,0,3 », « 1,2 ») que les petits modèles emploient volontiers.
bool fromJson(const QJsonValue& v, ValueType type, TSA::Automation::Value& out)
{
    using TSA::Automation::parseArgument;
    if (v.isString()) return parseArgument(v.toString().toStdString(), type, out);
    switch (type)
    {
    case ValueType::Bool:
        if (!v.isBool()) return false;
        out = v.toBool();
        return true;
    case ValueType::Integer:
        if (!v.isDouble() || v.toDouble() != std::floor(v.toDouble())) return false;
        out = static_cast<long long>(v.toDouble());
        return true;
    case ValueType::Real:
        if (!v.isDouble()) return false;
        out = v.toDouble();
        return true;
    case ValueType::Text:
        if (v.isDouble()) { out = QString::number(v.toDouble()).toStdString(); return true; } // « case »: 1
        return false;
    case ValueType::Point3:
    {
        TSA::Automation::Point3 p {};
        if (v.isArray() && v.toArray().size() == 3)
        {
            const QJsonArray a = v.toArray();
            for (int i = 0; i < 3; ++i)
            {
                if (!a[i].isDouble()) return false;
                p[static_cast<std::size_t>(i)] = a[i].toDouble();
            }
        }
        else if (v.isObject())
        {
            const QJsonObject o = v.toObject();
            if (!o.value("x").isDouble() || !o.value("y").isDouble() || !o.value("z").isDouble()) return false;
            p = { o.value("x").toDouble(), o.value("y").toDouble(), o.value("z").toDouble() };
        }
        else
            return false;
        out = p;
        return true;
    }
    case ValueType::IdList:
    {
        if (!v.isArray()) return false;
        std::vector<int> ids;
        for (const auto& e : v.toArray())
        {
            if (!e.isDouble()) return false;
            ids.push_back(e.toInt());
        }
        out = ids;
        return true;
    }
    case ValueType::Object:
        return false; // objets Blueprint : jamais depuis l'IA
    }
    return false;
}

QJsonValue toJson(const TSA::Automation::Value& v)
{
    if (const auto* b = std::get_if<bool>(&v)) return *b;
    if (const auto* i = std::get_if<long long>(&v)) return static_cast<double>(*i);
    if (const auto* d = std::get_if<double>(&v)) return *d;
    if (const auto* s = std::get_if<std::string>(&v)) return qs(*s);
    if (const auto* p = std::get_if<TSA::Automation::Point3>(&v)) return QJsonArray { (*p)[0], (*p)[1], (*p)[2] };
    if (const auto* l = std::get_if<std::vector<int>>(&v))
    {
        QJsonArray a;
        for (int id : *l) a.append(id);
        return a;
    }
    return QJsonValue();
}

int outputId(const OperatorAction& a) { return a.outputs.value("id").toInt(-1); }

long long typedInt(const TSA::Automation::Arguments& args, const char* name)
{
    const auto it = args.find(name);
    const auto* i = it == args.end() ? nullptr : std::get_if<long long>(&it->second);
    return i ? *i : -1;
}

QString typedText(const TSA::Automation::Arguments& args, const char* name)
{
    const auto it = args.find(name);
    const auto* t = it == args.end() ? nullptr : std::get_if<std::string>(&it->second);
    return t ? qs(*t) : QString();
}

/// Relit le modèle après une commande réussie et le compare aux arguments typés de la demande. Rend vrai si l'état
/// constaté est conforme.
bool verify(TSA::Project::ProjectSession& s, OperatorAction& a, const TSA::Automation::Arguments& args)
{
    const auto& m = s.model();
    QJsonObject& v = a.verification;
    const QString c = a.command;
    if (c == "model.create_node")
    {
        const auto* n = m.getNode(outputId(a));
        v["node_exists"] = n != nullptr;
        if (!n) return false;
        v["position"] = QJsonArray { n->x(), n->y(), n->z() };
        const auto it = args.find("position");
        const auto* p = it == args.end() ? nullptr : std::get_if<TSA::Automation::Point3>(&it->second);
        if (!p) return false;
        const bool same = std::abs(n->x() - (*p)[0]) <= kPositionTolerance && std::abs(n->y() - (*p)[1]) <= kPositionTolerance
                          && std::abs(n->z() - (*p)[2]) <= kPositionTolerance;
        v["position_matches"] = same;
        return same;
    }
    if (c == "model.create_beam" || c == "model.create_column")
    {
        // Poutres et poteaux sont des barres du même conteneur, distinguées par leur rôle (Model::addBar).
        const auto* bar = m.getBeam(outputId(a));
        const auto expectedRole = c == "model.create_beam" ? TSA::Model::BarRole::Beam : TSA::Model::BarRole::Column;
        v["element_exists"] = bar != nullptr;
        if (!bar) return false;
        v["start"] = bar->startNodeId();
        v["end"] = bar->endNodeId();
        v["role_matches"] = bar->role() == expectedRole;
        return bar->role() == expectedRole && bar->startNodeId() == typedInt(args, "start") && bar->endNodeId() == typedInt(args, "end");
    }
    if (c == "model.set_support")
    {
        using TSA::Model::SupportType;
        const auto* n = m.getNode(static_cast<int>(typedInt(args, "node")));
        if (!n) return false;
        const QString t = typedText(args, "type").toLower();
        const SupportType expected = t == "fixed" ? SupportType::Fixed : t == "pinned" ? SupportType::Pinned
                                                                   : t == "roller" ? SupportType::Roller : SupportType::Free;
        v["support_type"] = static_cast<int>(n->supportType());
        return n->supportType() == expected;
    }
    if (c == "model.set_section")
    {
        const auto* b = m.getBeam(static_cast<int>(typedInt(args, "bar")));
        v["bar_exists"] = b != nullptr;
        if (!b) return false;
        v["section"] = qs(b->section().name);
        return true;
    }
    if (c == "loads.create_case")
    {
        const auto* lc = m.loadManager().getLoadCase(outputId(a));
        v["case_exists"] = lc != nullptr;
        return lc != nullptr;
    }
    if (c == "loads.add_nodal")
    {
        const auto* l = m.loadManager().getNodalLoad(outputId(a));
        v["load_exists"] = l != nullptr;
        return l != nullptr;
    }
    if (c == "loads.add_uniform")
    {
        const bool exists = m.loadManager().memberLoads().count(outputId(a)) > 0;
        v["load_exists"] = exists;
        return exists;
    }
    if (c == "analysis.run")
    {
        const bool has = s.analysis().results() != nullptr;
        const bool upToDate = has && s.analysis().resultsUpToDate();
        v["results_published"] = has;
        v["results_up_to_date"] = upToDate;
        return upToDate;
    }
    // Lecture : aucun changement attendu, le résultat de la commande est la donnée relue.
    v["read_only"] = true;
    return true;
}
} // namespace

QJsonObject OperatorAction::toJson() const
{
    QJsonObject o { { "command", command }, { "executed", executed }, { "verified", verified }, { "message", message } };
    if (refused) o["refused"] = true;
    if (!outputs.isEmpty()) o["outputs"] = outputs;
    if (!verification.isEmpty()) o["verification"] = verification;
    if (executed && !verified)
        o["warning"] = QStringLiteral("Commande exécutée mais état du modèle NON conforme à la demande : ne pas annoncer de succès.");
    return o;
}

AIOperator::AIOperator(SessionProvider session) : m_session(std::move(session)) {}

bool AIOperator::isModifying(const QString& command)
{
    const auto* spec = CommandRegistry::builtIn().find(command.toStdString());
    return spec && (spec->modifiesModel || command == QLatin1String("analysis.run"));
}

QStringList AIOperator::commandIds()
{
    QStringList ids;
    for (const auto* spec : CommandRegistry::builtIn().commands()) ids << qs(spec->id);
    return ids;
}

QString AIOperator::commandCatalog()
{
    QStringList lines;
    for (const auto* spec : CommandRegistry::builtIn().commands())
    {
        QStringList params;
        for (const auto& p : spec->parameters)
        {
            QString t = QStringLiteral("%1:%2").arg(qs(p.name), QString::fromUtf8(TSA::Automation::typeName(p.type)));
            if (const char* unit = TSA::Automation::quantityUnit(p.quantity); unit && *unit) t += QStringLiteral("[%1]").arg(QString::fromUtf8(unit));
            if (!p.required) t += QLatin1Char('?');
            params << t;
        }
        lines << QStringLiteral("%1(%2)%3").arg(qs(spec->id), params.join(", "), isModifying(qs(spec->id)) ? QStringLiteral(" *") : QString());
    }
    return lines.join('\n');
}

OperatorAction AIOperator::finish(OperatorAction action)
{
    m_journal.push_back(action);
    if (m_observer) m_observer(action);
    return action;
}

OperatorAction AIOperator::execute(const QString& command, const QJsonObject& arguments)
{
    OperatorAction a;
    a.timestampMs = QDateTime::currentMSecsSinceEpoch();
    a.command = command;
    a.arguments = arguments;
    a.modifies = isModifying(command);

    const CommandRegistry& registry = CommandRegistry::builtIn();
    const auto* spec = registry.find(command.toStdString());
    auto refuse = [&](const QString& why) {
        a.refused = true;
        a.message = why;
        return finish(a);
    };
    if (!spec) return refuse(QStringLiteral("Commande inconnue « %1 » : seules les commandes du registre TSA sont autorisées.").arg(command));
    TSA::Project::ProjectSession* session = m_session ? m_session() : nullptr;
    if (!session) return refuse(QStringLiteral("Aucun projet ouvert : rien n'a été exécuté."));
    if (a.modifies && !m_actEnabled)
        return refuse(QStringLiteral("Mode opérateur désactivé : les commandes qui modifient le modèle ou lancent un calcul ne sont pas exécutées. "
                                     "Proposer plutôt un Blueprint (propose_blueprint) à valider par l'ingénieur."));
    if (a.modifies && m_actionsThisRequest >= m_maxActions)
        return refuse(QStringLiteral("Limite de %1 actions par demande atteinte : arrêt. L'utilisateur doit confirmer la suite par une nouvelle demande.")
                          .arg(m_maxActions));

    TSA::Automation::Arguments args;
    for (auto it = arguments.begin(); it != arguments.end(); ++it)
    {
        const std::string name = it.key().toStdString();
        const auto p = std::find_if(spec->parameters.begin(), spec->parameters.end(), [&](const auto& s) { return s.name == name; });
        if (p == spec->parameters.end())
        {
            QStringList valid;
            for (const auto& s : spec->parameters) valid << qs(s.name);
            return refuse(QStringLiteral("Paramètre inconnu « %1 » pour %2 (paramètres : %3).").arg(it.key(), command, valid.join(", ")));
        }
        TSA::Automation::Value value;
        if (!fromJson(it.value(), p->type, value))
            return refuse(QStringLiteral("Paramètre « %1 » : %2 attendu.").arg(it.key(), QString::fromUtf8(TSA::Automation::typeName(p->type))));
        args[name] = value;
    }

    if (a.modifies) ++m_actionsThisRequest;
    const TSA::Automation::Arguments requested = args;
    const TSA::Automation::CommandResult r = registry.execute(*session, spec->id, std::move(args));
    a.executed = r.ok;
    a.message = qs(r.message);
    for (const auto& [name, value] : r.outputs) a.outputs[qs(name)] = toJson(value);
    if (r.ok) a.verified = verify(*session, a, requested);
    return finish(a);
}

} // namespace TSA::AI
