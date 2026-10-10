#include "TopologyNumberingService.h"

#include "../Analysis/CalculationSnapshot.h"
#include "../Model/Model.h"
#include "../Model/ModelDiff.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>

namespace TSA::Topology
{

namespace
{

constexpr EntityFamily kElementFamilies[] = { EntityFamily::Beam,  EntityFamily::Column, EntityFamily::Truss,
                                              EntityFamily::Cable, EntityFamily::Slab,   EntityFamily::Wall,
                                              EntityFamily::Foundation };

std::string escapeRegex(const std::string& s)
{
    static const std::string special = R"(\^$.|?*+()[]{})";
    std::string out;
    for (char c : s)
    {
        if (special.find(c) != std::string::npos) out += '\\';
        out += c;
    }
    return out;
}

std::string patternToRegex(const std::string& pattern, const std::string& prefix)
{
    std::string re;
    for (size_t i = 0; i < pattern.size();)
    {
        if (pattern.compare(i, 3, "{p}") == 0) { re += escapeRegex(prefix); i += 3; }
        else if (pattern.compare(i, 3, "{n}") == 0) { re += "[0-9]+"; i += 3; }
        else if (pattern.compare(i, 3, "{g}") == 0) { re += "[A-Za-z]+[0-9]+(-[0-9]+)?([.][0-9]+)?"; i += 3; }
        else if (pattern.compare(i, 3, "{l}") == 0) { re += "[0-9]+"; i += 3; }
        else { re += escapeRegex(std::string(1, pattern[i])); ++i; }
    }
    return re;
}

bool hasToken(const std::string& pattern, const char* token)
{
    return pattern.find(token) != std::string::npos;
}

/// Étiquette actuelle et préfixe historique (formattedName du modèle) d'un élément.
struct ElementInfo
{
    std::string label;
    std::string historicPrefix;
    std::string prefixFamilyOverride; // barres de rôle poteau : préfixe des poteaux
    ElementItem item;
};

std::vector<ElementInfo> collectElements(const TSA::Model::Model& model)
{
    std::vector<ElementInfo> out;
    auto center = [&](const std::vector<int>& ids, ElementItem& it) {
        double sx = 0, sy = 0, sz = 0;
        int n = 0;
        for (int id : ids)
            if (const auto* nd = model.getNode(id)) { sx += nd->x(); sy += nd->y(); sz += nd->z(); ++n; }
        if (n) { it.cx = sx / n; it.cy = sy / n; it.cz = sz / n; }
    };
    auto add = [&](EntityFamily f, int id, const std::string& label, const std::string& hist, std::vector<int> nodes) {
        ElementInfo e;
        e.label = label;
        e.historicPrefix = hist;
        e.item.family = f;
        e.item.id = id;
        e.item.nodeIds = std::move(nodes);
        center(e.item.nodeIds, e.item);
        out.push_back(std::move(e));
    };
    for (const auto& [id, b] : model.beams())
    {
        const char* hist = b.role() == TSA::Model::BarRole::Column ? "C" : (b.role() == TSA::Model::BarRole::Brace ? "D" : "B");
        add(EntityFamily::Beam, id, b.formattedName(), hist, { b.startNodeId(), b.endNodeId() });
        if (b.role() == TSA::Model::BarRole::Column) out.back().prefixFamilyOverride = "column";
    }
    for (const auto& [id, c] : model.columns()) add(EntityFamily::Column, id, c.formattedName(), "C", { c.startNodeId(), c.endNodeId() });
    for (const auto& [id, t] : model.trussMembers()) add(EntityFamily::Truss, id, t.formattedName(), "TR", { t.startNodeId(), t.endNodeId() });
    for (const auto& [id, c] : model.cables()) add(EntityFamily::Cable, id, c.name(), "K", { c.startNodeId(), c.endNodeId() });
    for (const auto& [id, s] : model.slabs()) add(EntityFamily::Slab, id, s.formattedName(), "S", s.nodeIds());
    for (const auto& [id, w] : model.walls()) add(EntityFamily::Wall, id, w.formattedName(), "W", { w.startNodeId(), w.endNodeId() });
    for (const auto& [id, f] : model.foundations()) add(EntityFamily::Foundation, id, f.formattedName(), "F", { f.nodeId() });
    return out;
}

std::map<std::pair<EntityFamily, int>, int> solverTags(const TSA::Model::Model& model)
{
    std::map<std::pair<EntityFamily, int>, int> tags;
    const auto snap = TSA::Analysis::CalculationSnapshot::capture(model);
    for (const auto& [tag, e] : snap.elements())
    {
        EntityFamily f = EntityFamily::Beam;
        switch (e.type)
        {
        case TSA::Analysis::StructuralElementKind::Beam: f = EntityFamily::Beam; break;
        case TSA::Analysis::StructuralElementKind::Column: f = EntityFamily::Column; break;
        case TSA::Analysis::StructuralElementKind::Truss: f = EntityFamily::Truss; break;
        case TSA::Analysis::StructuralElementKind::Cable: f = EntityFamily::Cable; break;
        }
        tags[{ f, e.id }] = e.tag;
    }
    return tags;
}

void reportDuplicates(const std::map<std::string, std::vector<std::pair<EntityFamily, int>>>& byLabel,
                      const std::set<std::pair<EntityFamily, int>>& changed, const std::string& what,
                      NumberingPreview& preview)
{
    int shown = 0, total = 0;
    std::string examples;
    for (const auto& [label, owners] : byLabel)
    {
        if (owners.size() < 2) continue;
        const bool involvesChange = std::any_of(owners.begin(), owners.end(), [&](const auto& o) { return changed.count(o); });
        if (!involvesChange) continue;
        ++total;
        if (shown < 5)
        {
            examples += (shown ? ", « " : "« ") + label + " »";
            ++shown;
        }
    }
    if (total)
        preview.errors.push_back(std::to_string(total) + " étiquette(s) de " + what + " en double après renumérotation (" +
                                 examples + (total > shown ? "…" : "") +
                                 ") : modifiez le format, le préfixe, ou désactivez la conservation des étiquettes personnalisées.");
}

} // namespace

int NumberingPreview::changedCount() const
{
    return static_cast<int>(std::count_if(entries.begin(), entries.end(), [](const PreviewEntry& e) { return e.changed(); }));
}

std::vector<std::string> validateSettings(const TopologySettings& s)
{
    std::vector<std::string> errors;
    auto checkFormat = [&](const LabelFormat& f, const std::string& what, bool allowGrid) {
        if (f.start < 0) errors.push_back(what + " : le numéro de départ doit être positif ou nul.");
        if (f.increment < 1) errors.push_back(what + " : l'incrément doit être au moins 1.");
        if (f.width < 0 || f.width > 9) errors.push_back(what + " : le nombre de chiffres doit être compris entre 0 et 9.");
        if (f.start > 100000000) errors.push_back(what + " : numéro de départ hors limites.");
        if (f.pattern.empty()) errors.push_back(what + " : le format d'étiquette est vide.");
        else if (!hasToken(f.pattern, "{n}") && !(allowGrid && hasToken(f.pattern, "{g}")))
            errors.push_back(what + " : le format doit contenir {n}" + std::string(allowGrid ? " ou {g}" : "") +
                             ", sinon toutes les étiquettes seraient identiques.");
        if (!allowGrid && hasToken(f.pattern, "{g}")) errors.push_back(what + " : le jeton {g} (grille) est réservé aux nœuds.");
        if (f.pattern.size() > 40) errors.push_back(what + " : format trop long (40 caractères au plus).");
    };
    auto checkPrefix = [&](const std::string& p, const std::string& what) {
        if (p.size() > 16) errors.push_back(what + " : préfixe trop long (16 caractères au plus).");
        if (std::any_of(p.begin(), p.end(), [](unsigned char c) { return std::isspace(c) || std::iscntrl(c) || c == '{' || c == '}'; }))
            errors.push_back(what + " : le préfixe ne doit contenir ni espace ni accolade.");
    };
    checkFormat(s.nodes.format, "Nœuds", true);
    checkPrefix(s.nodes.format.prefix, "Nœuds");
    checkFormat(s.elements.format, "Éléments", false);
    for (EntityFamily f : kElementFamilies) checkPrefix(s.elementPrefix(f), familyDisplayName(f));
    if (!(s.nodes.tolerance > 0.0) || s.nodes.tolerance > 10.0) errors.push_back("Nœuds : la tolérance doit être comprise entre 0 et 10 m.");
    if (!(s.elements.tolerance > 0.0) || s.elements.tolerance > 10.0) errors.push_back("Éléments : la tolérance doit être comprise entre 0 et 10 m.");
    if (!findNodeStrategy(s.nodes.strategy)) errors.push_back("Stratégie de numérotation des nœuds inconnue : « " + s.nodes.strategy + " ».");
    if (!findElementStrategy(s.elements.strategy)) errors.push_back("Stratégie de numérotation des éléments inconnue : « " + s.elements.strategy + " ».");
    if (!s.elements.sharedSequence && hasToken(s.elements.format.pattern, "{p}"))
    {
        // Suites séparées : deux familles au même préfixe produiraient les mêmes étiquettes.
        std::map<std::string, std::string> seen;
        for (EntityFamily f : kElementFamilies)
        {
            const std::string p = s.elementPrefix(f);
            auto [it, inserted] = seen.emplace(p, familyDisplayName(f));
            if (!inserted)
                errors.push_back("Préfixe « " + p + " » utilisé par " + it->second + " et " + familyDisplayName(f) +
                                 " avec des suites de numéros séparées : étiquettes en double.");
        }
    }
    if (!s.elements.sharedSequence && !hasToken(s.elements.format.pattern, "{p}"))
        errors.push_back("Éléments : sans {p} dans le format, les suites séparées par famille produisent des étiquettes en "
                         "double ; ajoutez {p} ou choisissez une suite commune.");
    return errors;
}

std::string formatLabel(const LabelFormat& format, const std::string& prefix, int number, const std::string& gridRef, int layer)
{
    std::ostringstream num;
    num << std::setw(format.width) << std::setfill('0') << number;
    std::string out;
    const std::string& p = format.pattern;
    for (size_t i = 0; i < p.size();)
    {
        if (p.compare(i, 3, "{p}") == 0) { out += prefix; i += 3; }
        else if (p.compare(i, 3, "{n}") == 0) { out += num.str(); i += 3; }
        else if (p.compare(i, 3, "{g}") == 0) { out += gridRef; i += 3; }
        else if (p.compare(i, 3, "{l}") == 0) { out += std::to_string(std::max(layer, 1)); i += 3; }
        else { out += p[i]; ++i; }
    }
    return out;
}

bool isAutomaticLabel(const std::string& label, const std::string& prefix, const LabelFormat& format, int id)
{
    if (label.empty()) return true;
    std::ostringstream hist;
    hist << prefix << std::setw(3) << std::setfill('0') << id;
    if (label == hist.str()) return true;
    try
    {
        return std::regex_match(label, std::regex(patternToRegex(format.pattern, prefix)));
    }
    catch (const std::regex_error&)
    {
        return false;
    }
}

NumberingPreview computePreview(const TSA::Model::Model& model, const TopologySettings& s, const NumberingInput& input)
{
    NumberingPreview preview;
    preview.dimension = detail::modelDimension(model, 1e-6);
    preview.errors = validateSettings(s);
    if (!preview.errors.empty()) return preview;

    const TopologySettings previous = TopologySettings::fromJson(model.topologySettingsJson());
    const TopologySettings defaults = TopologySettings::defaults();
    const bool selection = s.scope == NumberingScope::Selection;
    const auto tags = solverTags(model);

    // ─── Nœuds ─────────────────────────────────────────────────────────────────────────────
    const NodeNumberingStrategy* nodeStrategy = findNodeStrategy(s.nodes.strategy);
    const Availability nodeAvail = nodeStrategy->availability(model, input);
    if (!nodeAvail.available)
    {
        preview.errors.push_back("Nœuds : « " + nodeStrategy->name() + " » indisponible — " + nodeAvail.reason);
        return preview;
    }
    std::vector<OrderItem> items;
    for (const auto& [id, n] : model.nodes())
        if (!selection || input.selectedNodes.count(id)) items.push_back({ id, n.x(), n.y(), n.z() });
    if (selection && items.empty() && !input.selectedNodes.empty())
        preview.warnings.push_back("Aucun des nœuds sélectionnés n'existe dans le modèle.");

    NodeContext nctx{ model, s.nodes, input, items };
    const Ordering nodeOrder = nodeStrategy->order(nctx);
    preview.warnings.insert(preview.warnings.end(), nodeOrder.warnings.begin(), nodeOrder.warnings.end());
    {
        // Chaque nœud de la portée exactement une fois.
        std::vector<int> a = nodeOrder.order, b;
        for (const auto& it : items) b.push_back(it.id);
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        if (a != b)
        {
            preview.errors.push_back("Erreur interne de la stratégie « " + nodeStrategy->name() + " » : ordre incomplet.");
            return preview;
        }
    }

    std::map<int, int> nodeRank;
    const bool keepNodes = s.nodes.strategy == "keep";
    std::map<int, int> layerCounter;
    int globalCounter = 0;
    for (size_t i = 0; i < nodeOrder.order.size(); ++i)
    {
        const int id = nodeOrder.order[i];
        nodeRank[id] = static_cast<int>(i);
        const auto* n = model.getNode(id);
        PreviewEntry e;
        e.family = EntityFamily::Node;
        e.id = id;
        e.currentLabel = n->formattedName();
        e.x = n->x(); e.y = n->y(); e.z = n->z();
        e.solverTag = id; // OpenSees : tag du nœud = identifiant interne
        auto g = nodeOrder.gridRef.find(id);
        if (g != nodeOrder.gridRef.end()) e.gridRef = g->second;
        const auto lit = nodeOrder.layer.find(id);
        const int layer = lit != nodeOrder.layer.end() ? lit->second : 1;

        const bool automatic = isAutomaticLabel(e.currentLabel, defaults.nodes.format.prefix, defaults.nodes.format, id) ||
                               isAutomaticLabel(e.currentLabel, s.nodes.format.prefix, s.nodes.format, id) ||
                               isAutomaticLabel(e.currentLabel, previous.nodes.format.prefix, previous.nodes.format, id);
        if (keepNodes || (s.nodes.preserveCustomNames && !automatic))
        {
            e.newLabel = e.currentLabel;
            e.preserved = !keepNodes;
        }
        else
        {
            int& k = s.nodes.restartPerLayer ? layerCounter[layer] : globalCounter;
            const int number = s.nodes.format.start + k * s.nodes.format.increment;
            ++k;
            LabelFormat f = s.nodes.format;
            if (hasToken(f.pattern, "{g}") && e.gridRef.empty()) f.pattern = "{p}{n}"; // pas de faux repère
            e.newLabel = formatLabel(f, s.nodes.format.prefix, number, e.gridRef, layer);
        }
        preview.entries.push_back(std::move(e));
    }
    // Rang des nœuds hors portée (stratégie topologique des éléments) : après ceux de la portée.
    {
        int r = static_cast<int>(nodeRank.size());
        for (const auto& [id, n] : model.nodes())
            if (!nodeRank.count(id)) nodeRank[id] = r++;
    }
    const auto preservedNodes = std::count_if(preview.entries.begin(), preview.entries.end(), [](const PreviewEntry& e) { return e.preserved; });
    if (preservedNodes)
        preview.warnings.push_back(std::to_string(preservedNodes) + " étiquette(s) de nœud personnalisée(s) conservée(s).");

    // ─── Éléments ──────────────────────────────────────────────────────────────────────────
    const ElementNumberingStrategy* elemStrategy = findElementStrategy(s.elements.strategy);
    const Availability elemAvail = elemStrategy->availability(model, input);
    if (!elemAvail.available)
    {
        preview.errors.push_back("Éléments : « " + elemStrategy->name() + " » indisponible — " + elemAvail.reason);
        return preview;
    }
    const std::vector<ElementInfo> all = collectElements(model);
    std::map<std::pair<EntityFamily, int>, const ElementInfo*> infoByKey;
    std::vector<ElementItem> elemItems;
    for (const auto& info : all)
    {
        infoByKey[{ info.item.family, info.item.id }] = &info;
        if (selection)
        {
            auto sel = input.selectedElements.find(info.item.family);
            if (sel == input.selectedElements.end() || !sel->second.count(info.item.id)) continue;
        }
        elemItems.push_back(info.item);
    }
    ElementContext ectx{ model, s.elements, input, elemItems, nodeRank };
    const ElementOrdering elemOrder = elemStrategy->order(ectx);
    preview.warnings.insert(preview.warnings.end(), elemOrder.warnings.begin(), elemOrder.warnings.end());
    if (elemOrder.order.size() != elemItems.size())
    {
        preview.errors.push_back("Erreur interne de la stratégie « " + elemStrategy->name() + " » : ordre incomplet.");
        return preview;
    }
    const bool keepElements = s.elements.strategy == "keep";
    std::map<std::string, int> counters; // par préfixe effectif (ou suite commune)
    int preservedElements = 0;
    for (const auto& key : elemOrder.order)
    {
        const ElementInfo& info = *infoByKey.at(key);
        PreviewEntry e;
        e.family = key.first;
        e.id = key.second;
        e.currentLabel = info.label;
        e.x = info.item.cx; e.y = info.item.cy; e.z = info.item.cz;
        auto t = tags.find(key);
        e.solverTag = t != tags.end() ? t->second : -1;
        const std::string prefix = info.prefixFamilyOverride == "column" ? s.elementPrefix(EntityFamily::Column)
                                                                         : s.elementPrefix(key.first);
        const std::string prevPrefix = info.prefixFamilyOverride == "column" ? previous.elementPrefix(EntityFamily::Column)
                                                                             : previous.elementPrefix(key.first);
        const bool automatic = isAutomaticLabel(info.label, info.historicPrefix, defaults.elements.format, e.id) ||
                               isAutomaticLabel(info.label, prefix, s.elements.format, e.id) ||
                               isAutomaticLabel(info.label, prevPrefix, previous.elements.format, e.id);
        if (keepElements || (s.elements.preserveCustomNames && !automatic))
        {
            e.newLabel = e.currentLabel;
            e.preserved = !keepElements;
            if (e.preserved) ++preservedElements;
        }
        else
        {
            int& k = counters[s.elements.sharedSequence ? std::string("\x01shared") : prefix];
            const int number = s.elements.format.start + k * s.elements.format.increment;
            ++k;
            e.newLabel = formatLabel(s.elements.format, prefix, number, std::string(), 1);
        }
        preview.entries.push_back(std::move(e));
    }
    if (preservedElements)
        preview.warnings.push_back(std::to_string(preservedElements) + " étiquette(s) d'élément personnalisée(s) conservée(s).");

    // ─── Doublons (étiquettes finales, entités hors portée comprises) ──────────────────────
    std::map<std::pair<EntityFamily, int>, std::string> finalLabel;
    std::set<std::pair<EntityFamily, int>> changed;
    for (const auto& [id, n] : model.nodes()) finalLabel[{ EntityFamily::Node, id }] = n.formattedName();
    for (const auto& info : all) finalLabel[{ info.item.family, info.item.id }] = info.label;
    for (const auto& e : preview.entries)
    {
        finalLabel[{ e.family, e.id }] = e.newLabel;
        if (e.changed()) changed.insert({ e.family, e.id });
    }
    std::map<std::string, std::vector<std::pair<EntityFamily, int>>> nodeLabels, elemLabels;
    for (const auto& [key, label] : finalLabel)
        (key.first == EntityFamily::Node ? nodeLabels : elemLabels)[label].push_back(key);
    reportDuplicates(nodeLabels, changed, "nœud", preview);
    reportDuplicates(elemLabels, changed, "élément", preview);
    return preview;
}

bool applyPreview(TSA::Model::Model& model, const NumberingPreview& preview, const std::string& undoLabel, std::string* error)
{
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (!preview.valid()) return fail(preview.errors.front());

    // La prévisualisation doit correspondre au modèle actuel (aucune modification entre-temps).
    const auto all = collectElements(model);
    std::map<std::pair<EntityFamily, int>, std::string> current;
    for (const auto& [id, n] : model.nodes()) current[{ EntityFamily::Node, id }] = n.formattedName();
    for (const auto& info : all) current[{ info.item.family, info.item.id }] = info.label;
    for (const auto& e : preview.entries)
    {
        auto it = current.find({ e.family, e.id });
        if (it == current.end() || it->second != e.currentLabel)
            return fail("Le modèle a changé depuis la prévisualisation : prévisualisez à nouveau.");
    }
    if (preview.changedCount() == 0)
    {
        if (error) error->clear();
        return true;
    }

    model.pushLabelUndoState(undoLabel);
    TSA::Model::ModelDiff diff;
    for (const auto& e : preview.entries)
    {
        if (!e.changed()) continue;
        switch (e.family)
        {
        case EntityFamily::Node:
            model.getNode(e.id)->setName(e.newLabel);
            diff.nodes.modified.push_back(e.id);
            diff.modifiedNodeIds.push_back(e.id);
            break;
        case EntityFamily::Beam:
            model.getBeam(e.id)->setName(e.newLabel);
            diff.beams.modified.push_back(e.id);
            diff.modifiedBeamIds.push_back(e.id);
            break;
        case EntityFamily::Column:
            model.getColumn(e.id)->setName(e.newLabel);
            diff.columns.modified.push_back(e.id);
            diff.modifiedColumnIds.push_back(e.id);
            break;
        case EntityFamily::Truss:
            model.getTrussMember(e.id)->setName(e.newLabel);
            diff.trussMembers.modified.push_back(e.id);
            diff.modifiedTrussMemberIds.push_back(e.id);
            break;
        case EntityFamily::Cable:
            model.getCable(e.id)->setName(e.newLabel);
            diff.cables.modified.push_back(e.id);
            diff.modifiedCableIds.push_back(e.id);
            break;
        case EntityFamily::Slab:
            model.getSlab(e.id)->setName(e.newLabel);
            diff.slabs.modified.push_back(e.id);
            diff.modifiedSlabIds.push_back(e.id);
            break;
        case EntityFamily::Wall:
            model.getWall(e.id)->setName(e.newLabel);
            diff.walls.modified.push_back(e.id);
            diff.modifiedWallIds.push_back(e.id);
            break;
        case EntityFamily::Foundation:
            model.getFoundation(e.id)->setName(e.newLabel);
            diff.foundations.modified.push_back(e.id);
            diff.modifiedFoundationIds.push_back(e.id);
            break;
        }
    }
    model.notifyLabelsChanged(diff);
    if (error) error->clear();
    return true;
}

std::vector<SolverMappingRow> solverMapping(const TSA::Model::Model& model)
{
    std::vector<SolverMappingRow> rows;
    const auto tags = solverTags(model);
    for (const auto& [id, n] : model.nodes()) rows.push_back({ EntityFamily::Node, id, n.formattedName(), id });
    for (const auto& info : collectElements(model))
    {
        auto t = tags.find({ info.item.family, info.item.id });
        rows.push_back({ info.item.family, info.item.id, info.label, t != tags.end() ? t->second : -1 });
    }
    return rows;
}

} // namespace TSA::Topology
