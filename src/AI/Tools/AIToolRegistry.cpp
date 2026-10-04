#include "AIToolRegistry.h"

#include "../RAG/EngineeringKnowledgeBase.h"
#include "../../Model/Model.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace TSA::AI
{

namespace
{

QJsonObject fn(const QString& name, const QString& description, const QJsonObject& properties, const QStringList& required = {})
{
    return QJsonObject{
        { "type", "function" },
        { "function", QJsonObject{
              { "name", name },
              { "description", description },
              { "parameters", QJsonObject{ { "type", "object" }, { "properties", properties },
                                           { "required", QJsonArray::fromStringList(required) } } } } } };
}

QJsonObject prop(const QString& type, const QString& description)
{
    return QJsonObject{ { "type", type }, { "description", description } };
}

int clampLimit(const QJsonObject& args, int def, int max)
{
    return std::clamp(args.value("limit").toInt(def), 1, max);
}

// Identifiant d'objet tolérant : 23, "23", "B23", "N5"
int parseId(const QJsonValue& v)
{
    if (v.isDouble()) return v.toInt();
    static const QRegularExpression digits(QStringLiteral("(\\d+)"));
    const auto m = digits.match(v.toString());
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

} // namespace

QJsonObject ToolOutcome::toModelJson() const
{
    if (!ok) return QJsonObject{ { "error", error } };
    QJsonObject o = data;
    if (proposal)
        o["status"] = "Proposition transmise à l'ingénieur pour validation. Rien n'a été modifié : ne pas affirmer que la modification est faite.";
    return o;
}

AIToolRegistry::AIToolRegistry(SourcesProvider sources, const EngineeringKnowledgeBase* knowledge)
    : m_sources(std::move(sources))
    , m_knowledge(knowledge)
{
}

QStringList AIToolRegistry::whitelist()
{
    return { "get_project_info", "list_members", "list_nodes", "get_object", "list_load_cases",
             "list_load_combinations", "list_loads", "get_results_summary", "check_model", "search_knowledge",
             "propose_section_change", "propose_run_analysis" };
}

ToolKind AIToolRegistry::kindOf(const QString& tool)
{
    return tool.startsWith("propose_") ? ToolKind::Proposal : ToolKind::Read;
}

QJsonArray AIToolRegistry::toolDefinitions() const
{
    const QJsonObject typeEnum{ { "type", "string" }, { "enum", QJsonArray{ "beam", "column", "truss", "cable" } },
                                { "description", "Famille d'éléments" } };
    QJsonArray tools{
        fn("get_project_info", "Informations générales du projet TSA : effectifs, emprise, portée du calcul.", {}),
        fn("list_members", "Liste paginée des barres (poutres, poteaux, treillis, câbles) avec nœuds, longueur, section, matériau.",
           { { "type", typeEnum }, { "offset", prop("integer", "Premier élément (pagination)") }, { "limit", prop("integer", "Nombre maximal (≤ 100)") } }),
        fn("list_nodes", "Liste paginée des nœuds avec coordonnées (m) et appuis.",
           { { "offset", prop("integer", "Premier nœud") }, { "limit", prop("integer", "Nombre maximal (≤ 100)") } }),
        fn("get_object", "Détail d'un objet : propriétés, appuis, charges appliquées et résultats de calcul s'ils existent.",
           { { "type", prop("string", "node, beam, column, slab, wall, foundation, truss ou cable") },
             { "id", prop("integer", "Identifiant numérique (ex. 23 pour B23)") } }, { "type", "id" }),
        fn("list_load_cases", "Cas de charge (catégorie, poids propre, nombre de charges).", {}),
        fn("list_load_combinations", "Combinaisons de charges avec leurs coefficients.", {}),
        fn("list_loads", "Charges appliquées (nodales et sur barres), filtrables par cas de charge.",
           { { "load_case_id", prop("integer", "Identifiant du cas (optionnel)") }, { "limit", prop("integer", "≤ 100") } }),
        fn("get_results_summary", "Synthèse des résultats du dernier calcul (déplacement, moment, tranchant, efforts normaux, réactions).", {}),
        fn("check_model", "Contrôles automatiques du modèle (topologie, stabilité, propriétés, charges, résultats).", {}),
        fn("search_knowledge", "Recherche dans la base documentaire locale (documentation TSA, documents de l'ingénieur). Renvoie des extraits avec leur source.",
           { { "query", prop("string", "Question ou mots-clés") } }, { "query" }),
        fn("propose_section_change", "PROPOSE à l'ingénieur de changer la section de poutres ou poteaux. Ne modifie rien sans son accord.",
           { { "type", QJsonObject{ { "type", "string" }, { "enum", QJsonArray{ "beam", "column" } } } },
             { "ids", QJsonObject{ { "type", "array" }, { "items", QJsonObject{ { "type", "integer" } } } } },
             { "section", prop("string", "Profil TSA : « IPE 300 », « HEA 240 », « HEB 200 », « UPN 160 » ou « RECT 300x500 » (mm)") },
             { "rationale", prop("string", "Justification technique") } }, { "type", "ids", "section", "rationale" }),
        fn("propose_run_analysis", "PROPOSE à l'ingénieur de lancer le calcul OpenSees (résultats absents ou obsolètes).",
           { { "rationale", prop("string", "Pourquoi le calcul est utile") } }, { "rationale" }),
    };
    return tools;
}

std::optional<TSA::Model::Section> AIToolRegistry::parseSection(const QString& text, QString* error)
{
    using TSA::Model::Section;
    const QString t = text.trimmed().toUpper();
    static const QRegularExpression profile(QStringLiteral(R"(^(IPE|HEA|HEB|UPN)\s*(\d{2,3})$)"));
    static const QRegularExpression rect(QStringLiteral(R"(^(RECT|R)\s*(\d{2,4})\s*[X×]\s*(\d{2,4})$)"));

    if (const auto m = profile.match(t); m.hasMatch())
    {
        const QString family = m.captured(1);
        const int size = m.captured(2).toInt();
        // Tailles réellement tabulées dans Section.cpp (pas d'extrapolation).
        static const QList<int> ipe = { 100, 120, 140, 160, 180, 200, 220, 240, 270, 300, 330, 360, 400 };
        static const QList<int> he = { 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300 };
        static const QList<int> upn = { 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300 };
        const QList<int>& table = family == "IPE" ? ipe : (family == "UPN" ? upn : he);
        if (!table.contains(size))
        {
            if (error) *error = QStringLiteral("%1 %2 n'existe pas dans la bibliothèque TSA.").arg(family).arg(size);
            return std::nullopt;
        }
        if (family == "IPE") return Section::ipe(size);
        if (family == "HEA") return Section::hea(size);
        if (family == "HEB") return Section::heb(size);
        return Section::upn(size);
    }
    if (const auto m = rect.match(t); m.hasMatch())
    {
        const double b = m.captured(2).toDouble() / 1000.0, h = m.captured(3).toDouble() / 1000.0;
        if (b < 0.05 || h < 0.05 || b > 3.0 || h > 3.0)
        {
            if (error) *error = QStringLiteral("Dimensions rectangulaires hors plage (50 à 3000 mm).");
            return std::nullopt;
        }
        return Section::rectangular(b, h);
    }
    if (error) *error = QStringLiteral("Profil non reconnu : « %1 ». Formats : IPE 300, HEA 240, HEB 200, UPN 160, RECT 300x500.").arg(text);
    return std::nullopt;
}

ToolOutcome AIToolRegistry::proposeSectionChange(const QJsonObject& args, const EngineeringSources& src) const
{
    ToolOutcome out;
    const QString type = EngineeringContextBuilder::typeFromKey(args.value("type").toString());
    if (type != "beam" && type != "column")
    {
        out.error = "type doit valoir beam ou column";
        return out;
    }
    QString err;
    const auto section = parseSection(args.value("section").toString(), &err);
    if (!section) { out.error = err; return out; }

    QJsonArray validIds;
    QStringList labels, oldSections;
    for (const auto& v : args.value("ids").toArray())
    {
        const int id = parseId(v);
        const TSA::Model::Section* current = nullptr;
        if (type == "beam") { if (const auto* e = src.model->getBeam(id)) current = &e->section(); }
        else { if (const auto* e = src.model->getColumn(id)) current = &e->section(); }
        if (!current) { out.error = QStringLiteral("%1 %2 introuvable").arg(type).arg(id); return out; }
        validIds.append(id);
        labels << ObjectRef{ type, id }.label();
        if (!oldSections.contains(QString::fromStdString(current->name))) oldSections << QString::fromStdString(current->name);
    }
    if (validIds.isEmpty()) { out.error = "ids vide"; return out; }

    ActionProposal p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.tool = "propose_section_change";
    p.title = QStringLiteral("Section de %1 : %2 → %3")
                  .arg(labels.size() <= 4 ? labels.join(", ") : QStringLiteral("%1 éléments").arg(labels.size()),
                       oldSections.join(" / "), QString::fromStdString(section->name));
    p.rationale = args.value("rationale").toString();
    p.arguments = QJsonObject{ { "type", type }, { "ids", validIds }, { "section", args.value("section").toString() } };
    p.impacts << QStringLiteral("Les résultats de calcul existants deviendront obsolètes.")
              << QStringLiteral("Modification annulable (Ctrl+Z).");
    out.ok = true;
    out.data = QJsonObject{ { "proposal", p.title } };
    out.proposal = p;
    return out;
}

ToolOutcome AIToolRegistry::execute(const QString& name, const QString& argumentsJson) const
{
    ToolOutcome out;
    if (!whitelist().contains(name))
    {
        out.error = QStringLiteral("Outil non autorisé : %1").arg(name);
        return out;
    }
    QJsonObject args;
    if (!argumentsJson.trimmed().isEmpty())
    {
        const auto doc = QJsonDocument::fromJson(argumentsJson.toUtf8());
        if (!doc.isObject())
        {
            out.error = "Arguments JSON invalides";
            return out;
        }
        args = doc.object();
    }
    const EngineeringSources src = m_sources ? m_sources() : EngineeringSources{};
    if (!src.model && name != "search_knowledge")
    {
        out.error = "Aucun modèle ouvert";
        return out;
    }

    out.ok = true;
    if (name == "get_project_info")
        out.data = EngineeringContextBuilder::projectInfo(src);
    else if (name == "list_members")
    {
        const QString type = args.contains("type") ? EngineeringContextBuilder::typeFromKey(args["type"].toString()) : QString();
        out.data = QJsonObject{ { "members", EngineeringContextBuilder::members(*src.model, type, std::max(0, args.value("offset").toInt()), clampLimit(args, 30, 100)) } };
    }
    else if (name == "list_nodes")
        out.data = QJsonObject{ { "nodes", EngineeringContextBuilder::nodes(*src.model, std::max(0, args.value("offset").toInt()), clampLimit(args, 30, 100)) } };
    else if (name == "get_object")
    {
        const ObjectRef ref{ EngineeringContextBuilder::typeFromKey(args.value("type").toString()), parseId(args.value("id")) };
        out.data = EngineeringContextBuilder::objectDetails(src, ref);
        if (out.data.isEmpty()) { out.ok = false; out.error = QStringLiteral("Objet introuvable : %1").arg(ref.label()); }
    }
    else if (name == "list_load_cases")
        out.data = QJsonObject{ { "loadCases", EngineeringContextBuilder::loadCases(*src.model) } };
    else if (name == "list_load_combinations")
        out.data = QJsonObject{ { "loadCombinations", EngineeringContextBuilder::loadCombinations(*src.model) } };
    else if (name == "list_loads")
        out.data = QJsonObject{ { "loads", EngineeringContextBuilder::loads(*src.model, args.value("load_case_id").toInt(0), clampLimit(args, 40, 100)) } };
    else if (name == "get_results_summary")
        out.data = EngineeringContextBuilder::resultsSummary(src);
    else if (name == "check_model")
    {
        CheckOptions opt;
        opt.resultsUpToDate = src.resultsUpToDate;
        out.data = StructuralChecker::check(*src.model, src.results, opt).toJson();
    }
    else if (name == "search_knowledge")
    {
        QJsonArray hits;
        if (m_knowledge)
            for (const auto& h : m_knowledge->search(args.value("query").toString(), 4))
                hits.append(QJsonObject{ { "source", h.chunk->source }, { "section", h.chunk->heading },
                                         { "excerpt", h.chunk->text.left(1200) } });
        out.data = QJsonObject{ { "results", hits } };
        if (hits.isEmpty())
            out.data["note"] = "Aucune source documentaire pertinente : ne pas citer de norme ou de règle non sourcée.";
    }
    else if (name == "propose_section_change")
        return proposeSectionChange(args, src);
    else if (name == "propose_run_analysis")
    {
        ActionProposal p;
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.tool = name;
        p.title = QStringLiteral("Lancer le calcul OpenSees");
        p.rationale = args.value("rationale").toString();
        p.impacts << QStringLiteral("Le calcul utilise le modèle courant ; aucune donnée du modèle n'est modifiée.");
        out.data = QJsonObject{ { "proposal", p.title } };
        out.proposal = p;
    }
    return out;
}

bool AIToolRegistry::applyProposal(const ActionProposal& proposal, TSA::Model::Model& model, QString* error)
{
    if (proposal.tool != "propose_section_change")
    {
        if (error) *error = QStringLiteral("Cette proposition n'est pas une modification du modèle.");
        return false;
    }
    const auto section = parseSection(proposal.arguments.value("section").toString(), error);
    if (!section) return false;
    const QString type = proposal.arguments.value("type").toString();
    std::vector<int> ids;
    for (const auto& v : proposal.arguments.value("ids").toArray())
    {
        const int id = v.toInt();
        const bool exists = type == "beam" ? model.getBeam(id) != nullptr : model.getColumn(id) != nullptr;
        if (!exists)
        {
            if (error) *error = QStringLiteral("Élément %1 introuvable : le modèle a changé depuis la proposition.").arg(id);
            return false;
        }
        ids.push_back(id);
    }

    // Une seule entrée Undo pour l'ensemble, puis notification par élément (vues, résultats).
    model.pushUndoState(QStringLiteral("IA : section %1").arg(QString::fromStdString(section->name)).toStdString());
    for (int id : ids)
    {
        if (type == "beam")
        {
            model.getBeam(id)->setSection(*section);
            model.notifyBeamModified(id);
        }
        else
        {
            model.getColumn(id)->setSection(*section);
            model.notifyColumnModified(id);
        }
    }
    return true;
}

} // namespace TSA::AI
