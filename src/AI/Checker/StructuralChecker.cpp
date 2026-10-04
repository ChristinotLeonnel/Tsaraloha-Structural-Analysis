#include "StructuralChecker.h"

#include "../../Analysis/LoadValidation.h"
#include "../../Analysis/ResultsModel.h"
#include "../../Model/Model.h"

#include <cmath>
#include <map>
#include <numeric>
#include <set>

namespace TSA::AI
{

QString ObjectRef::label() const
{
    static const std::map<QString, QString> prefix = {
        { "node", "N" }, { "beam", "B" }, { "column", "C" }, { "slab", "S" }, { "wall", "W" },
        { "foundation", "F" }, { "truss", "T" }, { "cable", "K" }, { "loadCase", "LC" }, { "combination", "CO" } };
    const auto it = prefix.find(type);
    return (it != prefix.end() ? it->second : type + " ") + QString::number(id);
}

QString severityLabel(FindingSeverity s)
{
    switch (s)
    {
    case FindingSeverity::Error: return QStringLiteral("ERREUR");
    case FindingSeverity::Warning: return QStringLiteral("AVERTISSEMENT");
    case FindingSeverity::Info: return QStringLiteral("INFO");
    }
    return QString();
}

QJsonObject CheckFinding::toJson() const
{
    QJsonArray objs;
    for (const auto& o : objects) objs.append(QJsonObject{ { "type", o.type }, { "id", o.id }, { "label", o.label() } });
    return QJsonObject{ { "severity", severityLabel(severity) }, { "code", code }, { "category", category },
                        { "message", message }, { "basis", basis }, { "objects", objs } };
}

int CheckReport::errors() const
{
    return static_cast<int>(std::count_if(findings.begin(), findings.end(), [](const auto& f) { return f.severity == FindingSeverity::Error; }));
}

int CheckReport::warnings() const
{
    return static_cast<int>(std::count_if(findings.begin(), findings.end(), [](const auto& f) { return f.severity == FindingSeverity::Warning; }));
}

QJsonObject CheckReport::toJson() const
{
    QJsonArray arr;
    for (const auto& f : findings) arr.append(f.toJson());
    return QJsonObject{ { "errors", errors() }, { "warnings", warnings() }, { "findings", arr } };
}

QString CheckReport::toText() const
{
    if (findings.empty()) return QStringLiteral("✓ Aucun problème détecté par les contrôles automatiques.");
    QString t = QStringLiteral("%1 erreur(s), %2 avertissement(s), %3 information(s)\n")
                    .arg(errors()).arg(warnings()).arg(static_cast<int>(findings.size()) - errors() - warnings());
    int n = 1;
    for (const auto& f : findings)
    {
        t += QStringLiteral("\n%1. [%2] %3 — %4").arg(n++).arg(severityLabel(f.severity), f.category, f.message);
        if (!f.objects.empty())
        {
            QStringList labels;
            for (const auto& o : f.objects) labels << o.label();
            t += QStringLiteral(" (%1)").arg(labels.join(", "));
        }
    }
    return t;
}

namespace
{

struct UnionFind
{
    std::map<int, int> parent;
    int find(int x)
    {
        auto it = parent.find(x);
        if (it == parent.end()) { parent[x] = x; return x; }
        if (it->second == x) return x;
        return it->second = find(it->second);
    }
    void unite(int a, int b) { parent[find(a)] = find(b); }
};

} // namespace

CheckReport StructuralChecker::check(const TSA::Model::Model& model, const TSA::Analysis::ResultsModel* results,
                                     const CheckOptions& options)
{
    CheckReport report;
    auto add = [&](FindingSeverity sev, const QString& code, const QString& cat, const QString& msg,
                   std::vector<ObjectRef> objs = {}, const QString& basis = QStringLiteral("FAIT")) {
        if (static_cast<int>(objs.size()) > options.maxObjectsPerFinding) objs.resize(options.maxObjectsPerFinding);
        report.findings.push_back({ sev, code, cat, msg, basis, std::move(objs) });
    };

    const auto& nodes = model.nodes();
    const size_t elementCount = model.beams().size() + model.columns().size() + model.trussMembers().size()
                              + model.cables().size() + model.slabs().size() + model.walls().size();
    if (nodes.empty() && elementCount == 0)
    {
        add(FindingSeverity::Info, "MODEL_EMPTY", "Modèle", QStringLiteral("Le modèle est vide."));
        return report;
    }

    // --- Topologie : éléments du modèle d'analyse (barres, treillis, câbles) -------------------
    struct Linear { QString type; int id; int a; int b; double length; };
    std::vector<Linear> linear;
    for (const auto& [id, e] : model.beams()) linear.push_back({ "beam", id, e.startNodeId(), e.endNodeId(), e.length(model) });
    for (const auto& [id, e] : model.columns()) linear.push_back({ "column", id, e.startNodeId(), e.endNodeId(), e.length(model) });
    for (const auto& [id, e] : model.trussMembers()) linear.push_back({ "truss", id, e.startNodeId(), e.endNodeId(), e.length(model) });
    for (const auto& [id, e] : model.cables()) linear.push_back({ "cable", id, e.startNodeId(), e.endNodeId(), e.length(model) });

    std::set<int> usedNodes;
    std::vector<ObjectRef> missingNodeRefs, zeroLength;
    std::map<std::pair<int, int>, std::vector<ObjectRef>> byNodePair;
    for (const auto& e : linear)
    {
        const bool aOk = nodes.count(e.a) > 0, bOk = nodes.count(e.b) > 0;
        if (!aOk || !bOk) { missingNodeRefs.push_back({ e.type, e.id }); continue; }
        usedNodes.insert(e.a);
        usedNodes.insert(e.b);
        if (e.a == e.b || e.length < 1e-6) zeroLength.push_back({ e.type, e.id });
        byNodePair[{ std::min(e.a, e.b), std::max(e.a, e.b) }].push_back({ e.type, e.id });
    }
    for (const auto& [id, s] : model.slabs()) for (int n : s.nodeIds()) usedNodes.insert(n);
    for (const auto& [id, w] : model.walls()) { usedNodes.insert(w.startNodeId()); usedNodes.insert(w.endNodeId()); }
    for (const auto& [id, f] : model.foundations()) usedNodes.insert(f.nodeId());

    if (!missingNodeRefs.empty())
        add(FindingSeverity::Error, "TOPO_MISSING_NODE", "Topologie",
            QStringLiteral("%1 élément(s) référencent un nœud inexistant.").arg(missingNodeRefs.size()), missingNodeRefs);
    if (!zeroLength.empty())
        add(FindingSeverity::Error, "TOPO_ZERO_LENGTH", "Topologie",
            QStringLiteral("%1 élément(s) de longueur nulle.").arg(zeroLength.size()), zeroLength);

    std::vector<ObjectRef> duplicates;
    for (const auto& [pair, refs] : byNodePair)
        if (refs.size() > 1) duplicates.insert(duplicates.end(), refs.begin(), refs.end());
    if (!duplicates.empty())
        add(FindingSeverity::Warning, "TOPO_DUPLICATE_MEMBER", "Topologie",
            QStringLiteral("Éléments superposés : plusieurs barres relient les mêmes deux nœuds (rigidité comptée plusieurs fois)."), duplicates);

    std::vector<ObjectRef> isolated;
    for (const auto& [id, n] : nodes)
        if (!usedNodes.count(id)) isolated.push_back({ "node", id });
    if (!isolated.empty())
        add(FindingSeverity::Warning, "TOPO_ISOLATED_NODE", "Topologie",
            QStringLiteral("%1 nœud(s) isolé(s) : reliés à aucun élément.").arg(isolated.size()), isolated);

    const auto coincident = model.findCoincidentNodes(options.coincidentTolerance);
    if (!coincident.empty())
    {
        std::vector<ObjectRef> refs;
        for (const auto& [dup, keep] : coincident) refs.push_back({ "node", dup });
        add(FindingSeverity::Warning, "TOPO_COINCIDENT_NODES", "Topologie",
            QStringLiteral("%1 nœud(s) confondus avec un autre (tolérance %2 mm) : la continuité n'est pas assurée. Commande « Fusionner les nœuds confondus » disponible.")
                .arg(coincident.size()).arg(options.coincidentTolerance * 1000.0, 0, 'f', 1), refs);
    }

    // --- Stabilité : sous-structures connexes et appuis ----------------------------------------
    UnionFind uf;
    for (const auto& e : linear)
        if (nodes.count(e.a) && nodes.count(e.b)) uf.unite(e.a, e.b);
    std::map<int, std::vector<int>> components;
    for (int n : usedNodes)
        if (uf.parent.count(n)) components[uf.find(n)].push_back(n);

    int supportedTotal = 0;
    for (const auto& [id, n] : nodes) if (n.support().isSupported()) ++supportedTotal;
    if (!linear.empty() && supportedTotal == 0)
    {
        add(FindingSeverity::Error, "STAB_NO_SUPPORT", "Stabilité",
            QStringLiteral("Aucun appui : la structure est un mécanisme (calcul impossible)."));
    }
    else if (!linear.empty())
    {
        using TSA::Model::DOFState;
        if (components.size() > 1)
            add(FindingSeverity::Info, "TOPO_SUBSTRUCTURES", "Topologie",
                QStringLiteral("La structure comporte %1 sous-structures non reliées entre elles.").arg(components.size()));
        for (const auto& [root, compNodes] : components)
        {
            bool tx = false, ty = false, tz = false;
            int supportedInComp = 0;
            bool anyRotation = false;
            for (int nid : compNodes)
            {
                const auto& s = nodes.at(nid).support();
                if (!s.isSupported()) continue;
                ++supportedInComp;
                tx |= s.tx() != DOFState::Free;
                ty |= s.ty() != DOFState::Free;
                tz |= s.tz() != DOFState::Free;
                anyRotation |= s.rx() != DOFState::Free || s.ry() != DOFState::Free || s.rz() != DOFState::Free;
            }
            std::vector<ObjectRef> sample;
            for (int nid : compNodes) { sample.push_back({ "node", nid }); if (sample.size() >= 5) break; }
            if (supportedInComp == 0)
            {
                add(FindingSeverity::Error, "STAB_UNSUPPORTED_PART", "Stabilité",
                    QStringLiteral("Sous-structure de %1 nœud(s) sans aucun appui : mécanisme.").arg(compNodes.size()), sample);
                continue;
            }
            QStringList freeDirs;
            if (!tx) freeDirs << "X";
            if (!ty) freeDirs << "Y";
            if (!tz) freeDirs << "Z";
            if (!freeDirs.isEmpty())
                add(FindingSeverity::Error, "STAB_FREE_TRANSLATION", "Stabilité",
                    QStringLiteral("Translation d'ensemble non bloquée selon %1 : mécanisme.").arg(freeDirs.join(", ")), sample);
            else if (supportedInComp == 1 && !anyRotation)
                add(FindingSeverity::Warning, "STAB_SINGLE_PIN", "Stabilité",
                    QStringLiteral("Un seul appui, sans blocage des rotations : rotation d'ensemble probable (mécanisme)."), sample,
                    QStringLiteral("HYPOTHÈSE : pas d'autre liaison que les appuis déclarés"));
        }
    }

    // --- Propriétés des barres ----------------------------------------------------------------
    std::vector<ObjectRef> badSection, badMaterial;
    auto checkProps = [&](const QString& type, int id, const TSA::Model::Section& sec, const TSA::Model::Material& mat) {
        if (!(sec.area() > 0.0) || !(sec.iy() > 0.0) || !(sec.iz() > 0.0)) badSection.push_back({ type, id });
        if (!(mat.E > 0.0) || !(mat.density >= 0.0)) badMaterial.push_back({ type, id });
    };
    for (const auto& [id, e] : model.beams()) checkProps("beam", id, e.section(), e.material());
    for (const auto& [id, e] : model.columns()) checkProps("column", id, e.section(), e.material());
    if (!badSection.empty())
        add(FindingSeverity::Error, "PROP_SECTION", "Propriétés",
            QStringLiteral("%1 barre(s) avec une section nulle ou invalide (A, Iy ou Iz ≤ 0).").arg(badSection.size()), badSection);
    if (!badMaterial.empty())
        add(FindingSeverity::Error, "PROP_MATERIAL", "Propriétés",
            QStringLiteral("%1 barre(s) avec un matériau invalide (E ≤ 0).").arg(badMaterial.size()), badMaterial);

    if (!model.slabs().empty() || !model.walls().empty() || !model.foundations().empty())
        add(FindingSeverity::Info, "CALC_SURFACES_NOT_ANALYZED", "Calcul",
            QStringLiteral("Dalles, voiles et fondations ne sont pas transmis au calcul OpenSees (seuls barres, treillis, câbles et appuis le sont)."));

    // --- Charges ---------------------------------------------------------------------------------
    const auto& lm = model.loadManager();
    std::map<int, int> loadsPerCase;
    for (const auto& [id, l] : lm.nodalLoads()) ++loadsPerCase[l.loadCaseId()];
    for (const auto& [id, l] : lm.memberLoads()) ++loadsPerCase[l.loadCaseId()];
    std::vector<ObjectRef> emptyCases, orphanLoads;
    for (const auto& [id, lc] : lm.loadCases())
        if (loadsPerCase[id] == 0 && !lc.isSelfWeightIncluded()) emptyCases.push_back({ "loadCase", id });
    if (!emptyCases.empty())
        add(FindingSeverity::Warning, "LOAD_EMPTY_CASE", "Charges",
            QStringLiteral("%1 cas de charge sans aucune charge ni poids propre.").arg(emptyCases.size()), emptyCases);
    for (const auto& [caseId, count] : loadsPerCase)
        if (!lm.loadCases().count(caseId)) orphanLoads.push_back({ "loadCase", caseId });
    if (!orphanLoads.empty())
        add(FindingSeverity::Error, "LOAD_ORPHAN", "Charges",
            QStringLiteral("Des charges appartiennent à des cas de charge inexistants."), orphanLoads);
    if (lm.loadCases().empty() && !linear.empty())
        add(FindingSeverity::Warning, "LOAD_NO_CASE", "Charges", QStringLiteral("Aucun cas de charge défini."));
    if (lm.loadCases().size() > 1 && lm.combinations().empty())
        add(FindingSeverity::Info, "LOAD_NO_COMBINATION", "Charges",
            QStringLiteral("Plusieurs cas de charge mais aucune combinaison (ELU/ELS) définie."));
    std::vector<ObjectRef> badCombos;
    for (const auto& [id, co] : lm.combinations())
    {
        bool bad = co.caseFactors().empty();
        for (const auto& [caseId, f] : co.caseFactors())
            if (!lm.loadCases().count(caseId) || !std::isfinite(f)) bad = true;
        if (bad) badCombos.push_back({ "combination", id });
    }
    if (!badCombos.empty())
        add(FindingSeverity::Error, "LOAD_BAD_COMBINATION", "Charges",
            QStringLiteral("Combinaison vide ou référençant un cas de charge inexistant."), badCombos);

    // Contrôles de LoadValidation (moteur existant, utilisé avant tout calcul OpenSees)
    const auto validation = TSA::Analysis::LoadValidation::validateModel(model);
    const bool noSupportReported = std::any_of(report.findings.begin(), report.findings.end(),
                                               [](const CheckFinding& f) { return f.code == "STAB_NO_SUPPORT"; });
    for (const auto& m : validation.messages())
    {
        if (m.severity == TSA::Analysis::ValidationSeverity::Info) continue;
        // Même constat déjà formulé par le contrôle de stabilité : pas de doublon.
        if (noSupportReported && QString::fromStdString(m.message).contains(QStringLiteral("appui"), Qt::CaseInsensitive)) continue;
        add(m.severity == TSA::Analysis::ValidationSeverity::Error ? FindingSeverity::Error : FindingSeverity::Warning,
            "VALIDATION", QString::fromStdString(m.category), QString::fromStdString(m.message));
    }

    // --- Résultats ------------------------------------------------------------------------------
    if (results && results->isValid() && results->hasResults())
    {
        if (!options.resultsUpToDate)
            add(FindingSeverity::Warning, "RES_STALE", "Résultats",
                QStringLiteral("Le modèle a été modifié depuis le dernier calcul : les résultats ne correspondent plus au modèle."));

        std::vector<ObjectRef> nonFinite;
        for (const auto& [nid, d] : results->allDisplacements())
            if (!std::isfinite(d.translationMagnitude())) nonFinite.push_back({ "node", nid });
        if (!nonFinite.empty())
            add(FindingSeverity::Error, "RES_NON_FINITE", "Résultats",
                QStringLiteral("Déplacements non finis (NaN/infini) : instabilité numérique ou mécanisme."), nonFinite);

        // Contrôle indicatif : déplacement nodal maximal comparé à la plus grande portée du modèle.
        double maxSpan = 0.0;
        for (const auto& e : linear) maxSpan = std::max(maxSpan, e.length);
        const auto summary = results->summary();
        if (maxSpan > 0.0 && summary.maxDisplacement > maxSpan / options.displacementSpanRatio)
            add(FindingSeverity::Warning, "RES_LARGE_DISPLACEMENT", "Résultats",
                QStringLiteral("Déplacement maximal %1 %2 au nœud N%3, supérieur à Lmax/%4 = %5 %2 (Lmax = plus grande portée du modèle).")
                    .arg(summary.maxDisplacement, 0, 'g', 4).arg(QString::fromStdString(results->units().length))
                    .arg(summary.maxDisplacementNodeId).arg(options.displacementSpanRatio, 0, 'f', 0)
                    .arg(maxSpan / options.displacementSpanRatio, 0, 'g', 4),
                { { "node", summary.maxDisplacementNodeId } },
                QStringLiteral("HYPOTHÈSE : critère indicatif L/%1, à remplacer par le critère du projet").arg(options.displacementSpanRatio, 0, 'f', 0));
    }

    // Tri : erreurs, avertissements, informations
    std::stable_sort(report.findings.begin(), report.findings.end(),
                     [](const CheckFinding& a, const CheckFinding& b) { return static_cast<int>(a.severity) > static_cast<int>(b.severity); });
    return report;
}

} // namespace TSA::AI
