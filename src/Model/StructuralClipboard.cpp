#include "StructuralClipboard.h"
#include "Model.h"
#include "Node.h"
#include "Beam.h"
#include "Column.h"
#include "Slab.h"
#include "Wall.h"
#include "TrussMember.h"
#include "Foundation.h"
#include <cmath>
#include <unordered_map>
#include "Load/LoadManager.h"
#include "../UndoRedo/EditTransaction.h"
#include <QCoreApplication>
#include <QtGlobal>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

namespace TSA::Model
{

StructuralClipboard& StructuralClipboard::shared()
{
    static StructuralClipboard clipboard;
    return clipboard;
}

void StructuralClipboard::clear() noexcept
{
    m_hasData = false;
    m_refOriginX = 0.0;
    m_refOriginY = 0.0;
    m_refOriginZ = 0.0;
    m_nodes.clear();
    m_beams.clear();
    m_columns.clear();
    m_slabs.clear();
    m_cables.clear();
    m_walls.clear();
    m_trusses.clear();
    m_nodalLoads.clear();
    m_memberLoads.clear();
    m_surfaceLoads.clear();
    m_loadCases.clear();
    m_combinations.clear();
    m_foundations.clear();
    m_dimensions.clear();
    m_copyWarnings.clear();
    m_bimProducts.clear();
}

void StructuralClipboard::copyFrom(const Model& model,
                                   const std::set<int>& selectedNodes,
                                   const std::set<int>& selectedBeams,
                                   const std::set<int>& selectedColumns,
                                   const std::set<int>& selectedSlabs,
                                   const std::set<int>& selectedCables)
{
    std::vector<int> vNodes(selectedNodes.begin(), selectedNodes.end());
    std::vector<int> vBeams(selectedBeams.begin(), selectedBeams.end());
    std::vector<int> vCols(selectedColumns.begin(), selectedColumns.end());
    std::vector<int> vSlabs(selectedSlabs.begin(), selectedSlabs.end());
    std::vector<int> vCabs(selectedCables.begin(), selectedCables.end());

    copyFrom(model, vNodes, vBeams, vCols, vSlabs, vCabs);
}

void StructuralClipboard::copyFrom(const Model& model,
                                   const std::vector<int>& selectedNodes,
                                   const std::vector<int>& selectedBeams,
                                   const std::vector<int>& selectedColumns,
                                   const std::vector<int>& selectedSlabs,
                                   const std::vector<int>& selectedCables)
{
    ClipboardSelection sel;
    sel.nodes.insert(selectedNodes.begin(), selectedNodes.end());
    sel.beams.insert(selectedBeams.begin(), selectedBeams.end());
    sel.columns.insert(selectedColumns.begin(), selectedColumns.end());
    sel.slabs.insert(selectedSlabs.begin(), selectedSlabs.end());
    sel.cables.insert(selectedCables.begin(), selectedCables.end());
    copyFrom(model, sel, ClipboardOptions {});
}

void StructuralClipboard::copyFrom(const Model& model, const ClipboardSelection& sel, const ClipboardOptions& options)
{
    clear();
    m_options = options;

    std::set<int> allNodeIds(sel.nodes.begin(), sel.nodes.end());
    auto addEnds = [&](int a, int b) {
        allNodeIds.insert(a);
        allNodeIds.insert(b);
    };
    for (int id : sel.beams)
        if (const auto* b = model.getBeam(id)) addEnds(b->startNodeId(), b->endNodeId());
    for (int id : sel.columns)
        if (const auto* c = model.getColumn(id)) addEnds(c->startNodeId(), c->endNodeId());
    for (int id : sel.slabs)
        if (const auto* s = model.getSlab(id)) allNodeIds.insert(s->nodeIds().begin(), s->nodeIds().end());
    for (int id : sel.walls)
        if (const auto* w = model.getWall(id)) addEnds(w->startNodeId(), w->endNodeId());
    for (int id : sel.trusses)
        if (const auto* t = model.getTrussMember(id)) addEnds(t->startNodeId(), t->endNodeId());
    for (int id : sel.cables)
        if (const auto* c = model.getCable(id)) addEnds(c->startNodeId(), c->endNodeId());
    for (int id : sel.foundations)
        if (const auto* f = model.getFoundation(id)) allNodeIds.insert(f->nodeId());

    // Cotations : ancrage associé à un nœud copié → relié au nœud collé ; à un nœud non copié → figé à la position
    // actuelle du nœud (la cotation reste cohérente) ; ancrage déjà figé → point absolu, translaté au collage.
    for (int id : sel.dimensions)
    {
        auto it = model.dimensions().items.find(id);
        if (it == model.dimensions().items.end()) continue;
        TSA::Annotation::Dimension d = it->second;
        for (auto& a : d.anchors)
        {
            if (a.associative())
            {
                if (const auto* n = model.getNode(a.nodeId)) a.point = { n->x(), n->y(), n->z() };
                if (!allNodeIds.count(a.nodeId) || !model.getNode(a.nodeId)) a.nodeId = -1;
            }
            else
            {
                a.nodeId = -1;
                a.orphaned = false;
            }
        }
        m_dimensions.push_back(d);
    }

    double minX = 1e300, minY = 1e300, minZ = 1e300;
    auto widen = [&](double x, double y, double z) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);
    };
    for (int nid : allNodeIds)
        if (const auto* node = model.getNode(nid)) widen(node->x(), node->y(), node->z());
    if (allNodeIds.empty())   // cotations seules : origine prise sur leurs points
        for (const auto& d : m_dimensions)
            for (const auto& a : d.anchors) widen(a.point[0], a.point[1], a.point[2]);
    if (minX == 1e300)   // rien d'existant : presse-papier vide
    {
        m_dimensions.clear();
        return;
    }

    m_hasData = true;
    m_refOriginX = minX;
    m_refOriginY = minY;
    m_refOriginZ = minZ;

    for (int nid : allNodeIds)
        if (const auto* node = model.getNode(nid))
        {
            ClipboardNode cn;
            cn.originalId = nid;
            cn.relX = node->x() - minX;
            cn.relY = node->y() - minY;
            cn.relZ = node->z() - minZ;
            if (options.supports && node->support().isSupported())
            {
                cn.hasSupport = true;
                cn.support = node->support();
            }
            m_nodes.push_back(cn);
        }

    for (int bId : sel.beams)
        if (const auto* b = model.getBeam(bId))
            m_beams.push_back({ bId, b->startNodeId(), b->endNodeId(), b->properties() });
    for (int cId : sel.columns)
        if (const auto* c = model.getColumn(cId))
            m_columns.push_back({ cId, c->startNodeId(), c->endNodeId(), c->properties() });
    for (int sId : sel.slabs)
        if (const auto* s = model.getSlab(sId))
        {
            ClipboardSlab cs;
            cs.originalId = sId;
            cs.originalNodeIds = s->nodeIds();
            cs.thickness = s->thickness();
            cs.material = s->material();
            cs.slab = *s;
            m_slabs.push_back(cs);
        }
    for (int wId : sel.walls)
        if (const auto* w = model.getWall(wId))
            m_walls.push_back({ wId, w->startNodeId(), w->endNodeId(), *w });
    for (int tId : sel.trusses)
        if (const auto* t = model.getTrussMember(tId))
            m_trusses.push_back({ tId, t->startNodeId(), t->endNodeId(), *t });
    for (int fId : sel.foundations)
        if (const auto* f = model.getFoundation(fId))
            m_foundations.push_back({ fId, f->nodeId(), *f });

    for (int cabId : sel.cables)
    {
        const auto* c = model.getCable(cabId);
        if (c)
        {
            ClipboardCable ccab;
            ccab.originalId = cabId;
            ccab.originalStartNodeId = c->startNodeId();
            ccab.originalEndNodeId = c->endNodeId();
            ccab.definition = c->definition();
            ccab.type = c->type();
            ccab.geometryMode = c->geometryMode();
            ccab.sag = c->sag();
            ccab.section = c->section();
            ccab.material = c->material();
            ccab.prestress = c->prestress();
            ccab.analysis = c->analysisProperties();
            ccab.startAnchor = c->startAnchor();
            ccab.endAnchor = c->endAnchor();
            ccab.color = c->color();
            ccab.name = c->name();
            m_cables.push_back(ccab);
        }
    }

    // Charges (option) : celles des nœuds et éléments copiés, avec leurs cas de charge (rapprochés par nom au collage)
    if (options.loads)
    {
        const auto& lm = model.loadManager();
        auto keepCase = [&](int caseId) {
            if (const auto* lc = lm.getLoadCase(caseId)) m_loadCases.emplace(caseId, *lc);
        };
        for (const auto& [id, nl] : lm.nodalLoads())
            if (allNodeIds.count(nl.nodeId()))
            {
                m_nodalLoads.push_back(nl);
                keepCase(nl.loadCaseId());
            }
        for (const auto& [id, ml] : lm.memberLoads())
        {
            const std::set<int>* owners = nullptr;
            switch (ml.targetType())
            {
            case MemberTargetType::Beam: owners = &sel.beams; break;
            case MemberTargetType::Column: owners = &sel.columns; break;
            case MemberTargetType::Truss: owners = &sel.trusses; break;
            case MemberTargetType::Cable: owners = &sel.cables; break;
            }
            if (owners && owners->count(ml.elementId()))
            {
                m_memberLoads.push_back(ml);
                keepCase(ml.loadCaseId());
            }
        }
        for (const auto& [id, sl] : lm.surfaceLoads())
            if ((sl.targetKind == SurfaceTargetKind::Slab ? sel.slabs : sel.walls).count(sl.surfaceId))
            {
                m_surfaceLoads.push_back(sl);
                keepCase(sl.loadCaseId);
            }
        // Combinaisons pertinentes : celles qui utilisent au moins un cas de charge des charges copiées. Leurs autres
        // cas sont emportés aussi (sans charge) pour que la combinaison reste complète ; une combinaison qui référence
        // un cas absent du projet source n'est pas copiée (avertissement).
        const std::map<int, LoadCase> usedCases = m_loadCases;
        for (const auto& [cid, combo] : lm.combinations())
        {
            bool relevant = false;
            for (const auto& [caseId, f] : combo.caseFactors()) relevant = relevant || usedCases.count(caseId);
            if (!relevant) continue;
            std::string missing;
            for (const auto& [caseId, f] : combo.caseFactors())
                if (!lm.getLoadCase(caseId)) missing += (missing.empty() ? "" : ", ") + std::to_string(caseId);
            if (!missing.empty())
            {
                m_copyWarnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                     "Combinaison « %1 » non copiée : cas de charge n° %2 introuvable dans le projet source")
                                             .arg(QString::fromStdString(combo.name()), QString::fromStdString(missing))
                                             .toStdString());
                continue;
            }
            for (const auto& [caseId, f] : combo.caseFactors()) keepCase(caseId);
            m_combinations.push_back(combo);
        }
    }

    // Produits physiques des éléments copiés (une seule fois par produit)
    using TSA::BIM::AnalyticalRef;
    const auto& bim = model.bim();
    std::set<int> seenProducts;
    auto captureProduct = [&](ElementKind kind, int id) {
        if (const auto* p = bim.physicalOf(AnalyticalRef{ kind, id }); p && seenProducts.insert(p->id).second)
            m_bimProducts.push_back(*p);
    };
    for (const auto& e : m_beams) captureProduct(ElementKind::Beam, e.originalId);
    for (const auto& e : m_columns) captureProduct(ElementKind::Column, e.originalId);
    for (const auto& e : m_slabs) captureProduct(ElementKind::Slab, e.originalId);
    for (const auto& e : m_walls) captureProduct(ElementKind::Wall, e.originalId);
    for (const auto& e : m_trusses) captureProduct(ElementKind::TrussMember, e.originalId);
    for (const auto& e : m_cables) captureProduct(ElementKind::Cable, e.originalId);
    for (const auto& e : m_foundations) captureProduct(ElementKind::Foundation, e.originalId);
}

namespace
{
/// Recherche des nœuds existants proches : grille de cellules de la taille de la tolérance (27 cellules voisines).
class NodeGrid
{
public:
    NodeGrid(const Model& model, double tol) : m_tol(std::max(tol, 1e-9))
    {
        for (const auto& [id, n] : model.nodes()) m_cells[key(cell(n.x()), cell(n.y()), cell(n.z()))].push_back({ id, n.x(), n.y(), n.z() });
    }
    /// Nœud le plus proche à une distance ≤ tolérance (0 si aucun)
    int nearest(double x, double y, double z, double* distance = nullptr) const
    {
        int best = 0;
        double bestD = m_tol;
        const long long cx = cell(x), cy = cell(y), cz = cell(z);
        for (long long i = -1; i <= 1; ++i)
            for (long long j = -1; j <= 1; ++j)
                for (long long k = -1; k <= 1; ++k)
                {
                    auto it = m_cells.find(key(cx + i, cy + j, cz + k));
                    if (it == m_cells.end()) continue;
                    for (const auto& e : it->second)
                    {
                        const double d = std::sqrt((e.x - x) * (e.x - x) + (e.y - y) * (e.y - y) + (e.z - z) * (e.z - z));
                        if (d <= bestD && (best == 0 || d < bestD || e.id < best))
                        {
                            best = e.id;
                            bestD = d;
                        }
                    }
                }
        if (distance) *distance = best ? bestD : 0.0;
        return best;
    }

private:
    struct Entry
    {
        int id;
        double x, y, z;
    };
    long long cell(double v) const { return static_cast<long long>(std::floor(v / m_tol)); }
    static long long key(long long a, long long b, long long c) { return (a * 73856093LL) ^ (b * 19349663LL) ^ (c * 83492791LL); }
    double m_tol;
    std::unordered_map<long long, std::vector<Entry>> m_cells;
};

/// Désignation affichée « poutre 12 » ; what est marqué par QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", …)
std::string fmtId(const char* what, int id)
{
    return QCoreApplication::translate("TSA::Model::StructuralClipboard", what).toStdString() + " " + std::to_string(id);
}
QString qFmtId(const char* what, int id) { return QString::fromStdString(fmtId(what, id)); }

/// Paire non ordonnée de nœuds
std::pair<int, int> edge(int a, int b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); }
} // namespace

PastePlan StructuralClipboard::analyzePaste(const Model& model, double targetX, double targetY, double targetZ, const PasteOptions& options) const
{
    PastePlan plan;
    if (!hasData()) return plan;
    if (options.merge == NodeMergeMode::MergeCoincident)
    {
        if (!(options.tolerance > 0.0) || !std::isfinite(options.tolerance))
        {
            plan.degenerate.push_back({ PasteConflict::Kind::Degenerate, 0, ElementKind::Node, 0,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "tolérance de fusion invalide (elle doit être > 0 m)").toStdString() });
            return plan;
        }
        const NodeGrid grid(model, options.tolerance);
        for (const auto& cn : m_nodes)
        {
            double d = 0.0;
            const int target = grid.nearest(targetX + cn.relX, targetY + cn.relY, targetZ + cn.relZ, &d);
            if (!target) continue;
            const Node* existing = model.getNode(target);
            const SupportDefinition copied = cn.hasSupport ? cn.support : SupportDefinition::free();
            const bool conflict = existing && existing->support().isSupported() && copied.isSupported() && !(existing->support() == copied);
            if (conflict)
            {
                switch (options.supportConflicts)
                {
                case SupportConflictPolicy::Ask:
                    plan.supportConflicts.push_back({ PasteConflict::Kind::Support, cn.originalId, ElementKind::Node, target,
                                                      QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                                  "nœud %1 : appui existant et appui copié différents (nœud source %2, écart %3 m)")
                                                          .arg(QString::number(target), QString::number(cn.originalId), QString::fromStdString(std::to_string(d)))
                                                          .toStdString() });
                    break;
                case SupportConflictPolicy::DontMerge:
                    plan.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                        "nœud source %1 non fusionné avec le nœud %2 (appuis différents) : nœuds coïncidents séparés")
                                                .arg(cn.originalId)
                                                .arg(target)
                                                .toStdString());
                    continue;
                case SupportConflictPolicy::KeepTarget:
                    plan.warnings.push_back(
                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "nœud %1 : appui existant conservé, appui copié ignoré").arg(target).toStdString());
                    break;
                case SupportConflictPolicy::UseCopied:
                    plan.warnings.push_back(
                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "nœud %1 : appui existant remplacé par l'appui copié").arg(target).toStdString());
                    break;
                }
            }
            plan.merges[cn.originalId] = target;
        }
    }

    // Nœud cible de chaque nœud source : existant (> 0) ou nouveau (< 0, unique par nœud source)
    auto mapped = [&](int src) {
        auto it = plan.merges.find(src);
        return it != plan.merges.end() ? it->second : -src - 1;
    };
    auto isExisting = [](int m) { return m > 0; };
    // Arêtes existantes par famille (doublons)
    std::set<std::pair<int, int>> beamEdges, columnEdges, wallEdges, trussEdges, cableEdges;
    std::set<std::vector<int>> slabSets;
    std::set<int> foundationNodes;
    if (!plan.merges.empty())
    {
        for (const auto& [id, b] : model.beams()) beamEdges.insert(edge(b.startNodeId(), b.endNodeId()));
        for (const auto& [id, c] : model.columns()) columnEdges.insert(edge(c.startNodeId(), c.endNodeId()));
        for (const auto& [id, w] : model.walls()) wallEdges.insert(edge(w.startNodeId(), w.endNodeId()));
        for (const auto& [id, t] : model.trussMembers()) trussEdges.insert(edge(t.startNodeId(), t.endNodeId()));
        for (const auto& [id, c] : model.cables()) cableEdges.insert(edge(c.startNodeId(), c.endNodeId()));
        for (const auto& [id, sl] : model.slabs())
        {
            std::vector<int> v = sl.nodeIds();
            std::sort(v.begin(), v.end());
            slabSets.insert(v);
        }
        for (const auto& [id, f] : model.foundations()) foundationNodes.insert(f.nodeId());
    }
    auto checkBar = [&](ElementKind kind, const char* what, int id, int a, int b, const std::set<std::pair<int, int>>& existing) {
        const int ma = mapped(a), mb = mapped(b);
        if (ma == mb)
            plan.degenerate.push_back({ PasteConflict::Kind::Degenerate, id, kind, ma,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : ses deux nœuds seraient fusionnés en un seul (longueur nulle)")
                                            .arg(qFmtId(what, id))
                                            .toStdString() });
        else if (isExisting(ma) && isExisting(mb) && existing.count(edge(ma, mb)))
            plan.duplicates.push_back({ PasteConflict::Kind::Duplicate, id, kind, 0,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : identique à un élément existant entre les nœuds %2 et %3")
                                            .arg(qFmtId(what, id), QString::number(ma), QString::number(mb))
                                            .toStdString() });
    };
    for (const auto& e : m_beams)
        checkBar(ElementKind::Beam, QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poutre"), e.originalId, e.originalStartNodeId, e.originalEndNodeId, beamEdges);
    for (const auto& e : m_columns)
        checkBar(ElementKind::Column, QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poteau"), e.originalId, e.originalStartNodeId, e.originalEndNodeId, columnEdges);
    for (const auto& e : m_walls)
        checkBar(ElementKind::Wall, QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "voile"), e.originalId, e.originalStartNodeId, e.originalEndNodeId, wallEdges);
    for (const auto& e : m_trusses)
        checkBar(ElementKind::TrussMember, QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "barre de treillis"), e.originalId, e.originalStartNodeId,
                 e.originalEndNodeId, trussEdges);
    for (const auto& e : m_cables)
        checkBar(ElementKind::Cable, QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "câble"), e.originalId, e.originalStartNodeId, e.originalEndNodeId, cableEdges);
    for (const auto& e : m_slabs)
    {
        std::vector<int> v;
        for (int n : e.originalNodeIds) v.push_back(mapped(n));
        std::vector<int> sorted = v;
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
            plan.degenerate.push_back({ PasteConflict::Kind::Degenerate, e.originalId, ElementKind::Slab, 0,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : deux de ses sommets seraient fusionnés")
                                            .arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "dalle"), e.originalId))
                                            .toStdString() });
        else if (std::all_of(v.begin(), v.end(), isExisting) && slabSets.count(sorted))
            plan.duplicates.push_back({ PasteConflict::Kind::Duplicate, e.originalId, ElementKind::Slab, 0,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : identique à une dalle existante")
                                            .arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "dalle"), e.originalId))
                                            .toStdString() });
    }
    for (const auto& e : m_foundations)
    {
        const int m = mapped(e.originalNodeId);
        if (isExisting(m) && foundationNodes.count(m))
            plan.duplicates.push_back({ PasteConflict::Kind::Duplicate, e.originalId, ElementKind::Foundation, m,
                                        QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : le nœud %2 porte déjà une fondation")
                                            .arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "fondation"), e.originalId), QString::number(m))
                                            .toStdString() });
    }
    return plan;
}

PasteResult StructuralClipboard::pasteTo(Model& model, double targetX, double targetY, double targetZ, const PasteOptions& options) const
{
    PasteResult result;
    if (!hasData())
        return result;

    const PastePlan plan = analyzePaste(model, targetX, targetY, targetZ, options);
    result.warnings = plan.warnings;
    if (plan.blocked() || plan.needsDecision())
    {
        // Rien n'est modifié : élément dégénéré (bloquant) ou appuis incompatibles (décision de l'utilisateur)
        result.complete = false;
        result.needsDecision = !plan.blocked() && plan.needsDecision();
        result.conflicts = plan.blocked() ? plan.degenerate : plan.supportConflicts;
        result.error = result.conflicts.size() > 1
                           ? QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 (+ %2 autre(s))")
                                 .arg(QString::fromStdString(result.conflicts.front().message), QString::number(result.conflicts.size() - 1))
                                 .toStdString()
                           : result.conflicts.front().message;
        return result;
    }
    // Éléments en double non collés (politique Skip) : identifiants source par famille
    std::set<std::pair<ElementKind, int>> skip;   // (famille, id source)
    if (options.duplicates == DuplicateElementPolicy::Skip)
        for (const auto& c : plan.duplicates)
        {
            skip.insert({ c.element, c.clipboardId });
            result.warnings.push_back(
                QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 — non collé").arg(QString::fromStdString(c.message)).toStdString());
            ++result.skippedDuplicates;
        }
    else
        for (const auto& c : plan.duplicates)
            result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 — collé quand même (rigidité doublée)")
                                          .arg(QString::fromStdString(c.message))
                                          .toStdString());
    auto skipped = [&](ElementKind fam, int id) { return skip.count({ fam, id }) > 0; };

    auto& nodeMap = result.nodeMap;
    auto& pasted = result.elementMap;   // original → collé
    auto fail = [&result](const std::string& what) {
        result.complete = false;
        if (result.error.empty()) result.error = what;
    };

    for (const auto& cn : m_nodes)
    {
        if (auto m = plan.merges.find(cn.originalId); m != plan.merges.end())
        {
            // Nœud existant réutilisé : appui fusionné (libre + appui → appui ; conflit tranché par la politique)
            if (auto* n = model.getNode(m->second))
            {
                const bool copiedSupported = cn.hasSupport && cn.support.isSupported();
                const bool apply = copiedSupported && (!n->support().isSupported() || options.supportConflicts == SupportConflictPolicy::UseCopied);
                if (apply && !(n->support() == cn.support))
                {
                    n->setSupport(cn.support);
                    model.notifyNodeModified(m->second);
                }
            }
            nodeMap[cn.originalId] = m->second;
            result.mergedNodeIds.push_back(m->second);
            continue;
        }
        double nx = targetX + cn.relX;
        double ny = targetY + cn.relY;
        double nz = targetZ + cn.relZ;
        int newNId = model.addNode(nx, ny, nz);
        if (newNId <= 0)
        {
            fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "nœud"), cn.originalId));
            continue;
        }
        if (cn.hasSupport)
            if (auto* n = model.getNode(newNId))
            {
                n->setSupport(cn.support);
                model.notifyNodeModified(newNId);
            }
        nodeMap[cn.originalId] = newNId;
        result.nodeIds.push_back(newNId);
    }

    for (const auto& cb : m_beams)
    {
        if (skipped(ElementKind::Beam, cb.originalId)) continue;
        auto itS = nodeMap.find(cb.originalStartNodeId);
        auto itE = nodeMap.find(cb.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            BarProperties p = cb.props;
            int bId = model.addBar(p, itS->second, itE->second);
            if (bId <= 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poutre"), cb.originalId));
                continue;
            }
            result.beamIds.push_back(bId);
            pasted[{ ElementKind::Beam, cb.originalId }] = { ElementKind::Beam, bId };
        }
    }

    for (const auto& cc : m_columns)
    {
        if (skipped(ElementKind::Column, cc.originalId)) continue;
        auto itS = nodeMap.find(cc.originalStartNodeId);
        auto itE = nodeMap.find(cc.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            BarProperties p = cc.props;
            int cId = model.addColumn(itS->second, itE->second, p.section, p.material, p.rotation, p.name);
            if (cId <= 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poteau"), cc.originalId));
                continue;
            }
            result.columnIds.push_back(cId);
            pasted[{ ElementKind::Column, cc.originalId }] = { ElementKind::Column, cId };
        }
    }

    for (const auto& cs : m_slabs)
    {
        if (skipped(ElementKind::Slab, cs.originalId)) continue;
        std::vector<int> sNodes;
        sNodes.reserve(cs.originalNodeIds.size());
        for (int onid : cs.originalNodeIds)
        {
            auto it = nodeMap.find(onid);
            if (it != nodeMap.end())
            {
                sNodes.push_back(it->second);
            }
        }
        const int sId = sNodes.size() >= 3 && sNodes.size() == cs.originalNodeIds.size() ? model.addSlab(sNodes, cs.thickness) : -1;
        if (sId <= 0)
        {
            fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "dalle"), cs.originalId));
            continue;
        }
        pasted[{ ElementKind::Slab, cs.originalId }] = { ElementKind::Slab, sId };
        if (auto* s = model.getSlab(sId))
        {
            // Dalle complète (ouvertures, formulation, appuis…) ; ancien contenu sans dalle complète : épaisseur et matériau
            if (cs.slab.nodeIds().size() == cs.originalNodeIds.size())
                *s = cs.slab;
            else
                s->setMaterial(cs.material);
            s->setId(sId);
            s->setNodeIds(sNodes);
            model.notifySlabModified(sId);   // vue créée à l'ajout avec le matériau par défaut
        }
        result.slabIds.push_back(sId);
    }

    for (const auto& cw : m_walls)
    {
        if (skipped(ElementKind::Wall, cw.originalId)) continue;
        auto itS = nodeMap.find(cw.originalStartNodeId);
        auto itE = nodeMap.find(cw.originalEndNodeId);
        const int wId = itS != nodeMap.end() && itE != nodeMap.end() ? model.addWall(itS->second, itE->second, cw.wall.height(), cw.wall.thickness(), cw.wall.name()) : -1;
        if (wId <= 0)
        {
            fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "voile"), cw.originalId));
            continue;
        }
        if (auto* w = model.getWall(wId))
        {
            *w = cw.wall;   // matériau, ouvertures, formulation, appuis
            w->setId(wId);
            w->setStartNodeId(itS->second);
            w->setEndNodeId(itE->second);
            model.notifyWallModified(wId);
        }
        result.wallIds.push_back(wId);
        pasted[{ ElementKind::Wall, cw.originalId }] = { ElementKind::Wall, wId };
    }

    for (const auto& ct : m_trusses)
    {
        if (skipped(ElementKind::TrussMember, ct.originalId)) continue;
        auto itS = nodeMap.find(ct.originalStartNodeId);
        auto itE = nodeMap.find(ct.originalEndNodeId);
        const int tId = itS != nodeMap.end() && itE != nodeMap.end() ? model.addTrussMember(itS->second, itE->second, 0.10, ct.member.name(), ct.member.role()) : -1;
        if (tId <= 0)
        {
            fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "barre de treillis"), ct.originalId));
            continue;
        }
        if (auto* t = model.getTrussMember(tId))
        {
            *t = ct.member;   // section, matériau, rôle, couleur
            t->setId(tId);
            t->setStartNodeId(itS->second);
            t->setEndNodeId(itE->second);
            model.notifyTrussMemberModified(tId);
        }
        result.trussIds.push_back(tId);
        pasted[{ ElementKind::TrussMember, ct.originalId }] = { ElementKind::TrussMember, tId };
    }

    for (const auto& ccab : m_cables)
    {
        if (skipped(ElementKind::Cable, ccab.originalId)) continue;
        auto itS = nodeMap.find(ccab.originalStartNodeId);
        auto itE = nodeMap.find(ccab.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            int cId = model.addCable(itS->second, itE->second, ccab.definition, ccab.name, ccab.geometryMode, ccab.sag);
            if (cId <= 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "câble"), ccab.originalId));
                continue;
            }
            if (auto* nc = model.getCable(cId))
            {
                nc->setType(ccab.type);
                nc->setSection(ccab.section);
                nc->setMaterial(ccab.material);
                nc->setPrestress(ccab.prestress);
                nc->setAnalysisProperties(ccab.analysis);
                nc->setStartAnchor(ccab.startAnchor);
                nc->setEndAnchor(ccab.endAnchor);
                nc->setColor(ccab.color);
                model.notifyCableModified(cId);  // vue créée à l'ajout avec la section par défaut
            }
            result.cableIds.push_back(cId);
            pasted[{ ElementKind::Cable, ccab.originalId }] = { ElementKind::Cable, cId };
        }
    }

    // Fondations : sur le nœud collé (ou fusionné)
    for (const auto& cf : m_foundations)
    {
        if (skipped(ElementKind::Foundation, cf.originalId)) continue;
        auto itN = nodeMap.find(cf.originalNodeId);
        const Foundation& f0 = cf.foundation;
        const int fId = itN != nodeMap.end() ? model.addFoundation(itN->second, f0.widthA(), f0.lengthB(), f0.heightH(), f0.name(), f0.foundationType()) : -1;
        if (fId <= 0)
        {
            fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "fondation"), cf.originalId));
            continue;
        }
        if (auto* f = model.getFoundation(fId))
        {
            *f = f0;   // matériau, portance du sol, couleur…
            f->setId(fId);
            f->setNodeId(itN->second);
            model.notifyFoundationModified(fId);
        }
        result.foundationIds.push_back(fId);
        pasted[{ ElementKind::Foundation, cf.originalId }] = { ElementKind::Foundation, fId };
    }

    // Éléments dont un nœud manque (ne devrait pas arriver : les extrémités sont toujours copiées)
    auto countSkipped = [&](ElementKind fam) { return static_cast<std::size_t>(std::count_if(skip.begin(), skip.end(), [fam](const auto& k) { return k.first == fam; })); };
    if (result.beamIds.size() + countSkipped(ElementKind::Beam) != m_beams.size() || result.columnIds.size() + countSkipped(ElementKind::Column) != m_columns.size() ||
        result.cableIds.size() + countSkipped(ElementKind::Cable) != m_cables.size())
        fail(QCoreApplication::translate("TSA::Model::StructuralClipboard", "élément sans ses nœuds").toStdString());

    // Cotations : ancrages associés remappés sur les nœuds collés, points figés et position translatés
    if (!m_dimensions.empty())
    {
        const double dx = targetX - m_refOriginX, dy = targetY - m_refOriginY, dz = targetZ - m_refOriginZ;
        auto& set = model.dimensionsForEdit();
        for (TSA::Annotation::Dimension d : m_dimensions)
        {
            for (auto& a : d.anchors)
            {
                a.point = { a.point[0] + dx, a.point[1] + dy, a.point[2] + dz };
                if (a.nodeId > 0)
                {
                    auto it = nodeMap.find(a.nodeId);
                    a.nodeId = it != nodeMap.end() ? it->second : -1;
                    if (const Node* n = a.nodeId > 0 ? model.getNode(a.nodeId) : nullptr) a.point = { n->x(), n->y(), n->z() };
                }
                a.orphaned = false;
            }
            d.position = { d.position[0] + dx, d.position[1] + dy, d.position[2] + dz };
            d.id = set.nextId++;
            set.items[d.id] = d;
            result.dimensionIds.push_back(d.id);
        }
        model.notifyDimensionsChanged(result.dimensionIds);
    }

    // Charges : cas de charge rapprochés par nom et catégorie dans le projet cible, créés s'ils n'existent pas
    if (!m_nodalLoads.empty() || !m_memberLoads.empty() || !m_surfaceLoads.empty() || !m_combinations.empty())
    {
        auto& lm = model.loadManager();
        for (const auto& [srcId, lc] : m_loadCases)
        {
            int target = 0;
            bool nameTaken = false;
            for (const auto& [id, existing] : lm.loadCases())
            {
                nameTaken = nameTaken || existing.name() == lc.name();
                if (existing.name() == lc.name() && existing.category() == lc.category())
                {
                    target = id;
                    break;
                }
            }
            if (target == 0 && nameTaken)
            {
                // Copie renommée lors d'un collage précédent (« Q (copie) », « Q (copie 2) ») de même catégorie : réutilisée
                const std::string prefix = lc.name() + " (copie";
                for (const auto& [id, existing] : lm.loadCases())
                    if (existing.category() == lc.category() && existing.name().rfind(prefix, 0) == 0 && existing.name().back() == ')')
                    {
                        target = id;
                        break;
                    }
            }
            if (target == 0)
            {
                LoadCase created = lc;
                if (nameTaken)
                {
                    // Même nom, autre catégorie : cas distinct, renommé (jamais deux cas homonymes)
                    std::string name = lc.name() + " (copie)";
                    for (int k = 2;; ++k)
                    {
                        bool taken = false;
                        for (const auto& [id, existing] : lm.loadCases()) taken = taken || existing.name() == name;
                        if (!taken) break;
                        name = lc.name() + " (copie " + std::to_string(k) + ")";
                    }
                    result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                          "cas de charge « %1 » : un cas de même nom et d'autre catégorie existe ; créé sous le nom « %2 »")
                                                  .arg(QString::fromStdString(lc.name()), QString::fromStdString(name))
                                                  .toStdString());
                    created.setName(name);
                }
                target = lm.addLoadCase(created);
                result.createdLoadCaseIds.push_back(target);
                model.notifyLoadCaseChanged(target);
            }
            result.loadCaseMap[srcId] = target;
        }
        auto caseOf = [&](int src) {
            auto it = result.loadCaseMap.find(src);
            return it != result.loadCaseMap.end() ? it->second : 0;
        };
        for (NodalLoad nl : m_nodalLoads)
        {
            auto it = nodeMap.find(nl.nodeId());
            if (it == nodeMap.end() || caseOf(nl.loadCaseId()) == 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge nodale"), nl.id()));
                continue;
            }
            nl.setId(0);
            nl.setNodeId(it->second);
            nl.setLoadCaseId(caseOf(nl.loadCaseId()));
            const int id = lm.addNodalLoad(nl);
            result.nodalLoadIds.push_back(id);
            model.notifyNodalLoadAdded(id);
        }
        auto kindOf = [](MemberTargetType t) {
            switch (t)
            {
            case MemberTargetType::Column: return ElementKind::Column;
            case MemberTargetType::Truss: return ElementKind::TrussMember;
            case MemberTargetType::Cable: return ElementKind::Cable;
            default: return ElementKind::Beam;
            }
        };
        for (MemberLoad ml : m_memberLoads)
        {
            auto it = pasted.find({ kindOf(ml.targetType()), ml.elementId() });
            if (it == pasted.end() && skipped(kindOf(ml.targetType()), ml.elementId()))
            {
                result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                      "charge sur barre %1 non collée : son élément, en double, n'a pas été collé")
                                              .arg(ml.id())
                                              .toStdString());
                continue;
            }
            if (it == pasted.end() || caseOf(ml.loadCaseId()) == 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge sur barre"), ml.id()));
                continue;
            }
            ml.setId(0);
            ml.setElementId(it->second.id);
            ml.setLoadCaseId(caseOf(ml.loadCaseId()));
            const int id = lm.addMemberLoad(ml);
            result.memberLoadIds.push_back(id);
            model.notifyMemberLoadAdded(id);
        }
        for (SurfaceLoad sl : m_surfaceLoads)
        {
            const ElementKind kind = sl.targetKind == SurfaceTargetKind::Slab ? ElementKind::Slab : ElementKind::Wall;
            auto it = pasted.find({ kind, sl.surfaceId });
            if (it == pasted.end() && skipped(kind, sl.surfaceId))
            {
                result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                      "charge surfacique %1 non collée : sa surface, en double, n'a pas été collée")
                                              .arg(sl.id)
                                              .toStdString());
                continue;
            }
            if (it == pasted.end() || caseOf(sl.loadCaseId) == 0)
            {
                fail(fmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge surfacique"), sl.id));
                continue;
            }
            sl.id = 0;
            sl.surfaceId = it->second.id;
            sl.loadCaseId = caseOf(sl.loadCaseId);
            result.surfaceLoadIds.push_back(lm.addSurfaceLoad(sl));
        }
        // Combinaisons : facteurs reportés sur les cas cibles. Une combinaison identique (même nom, même type, mêmes
        // facteurs) est réutilisée ; même nom mais contenu différent : renommée ; cas non résolu : non copiée.
        const std::size_t mergedLoads = std::count_if(m_nodalLoads.begin(), m_nodalLoads.end(), [&](const NodalLoad& l) { return plan.merges.count(l.nodeId()) > 0; });
        if (mergedLoads > 0)
            result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                  "%1 charge(s) nodale(s) ajoutée(s) à des nœuds existants (fusion) : elles s'ajoutent à leurs charges")
                                          .arg(QString::number(mergedLoads))
                                          .toStdString());
        for (const auto& combo : m_combinations)
        {
            std::map<int, double> factors;
            bool resolved = true;
            for (const auto& [caseId, f] : combo.caseFactors())
            {
                const int t = caseOf(caseId);
                if (t == 0) resolved = false;
                else factors[t] = f;
            }
            if (!resolved)
            {
                result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                      "combinaison « %1 » non copiée : un de ses cas de charge n'a pas pu être rapproché")
                                              .arg(QString::fromStdString(combo.name()))
                                              .toStdString());
                continue;
            }
            const LoadCombination* sameName = nullptr;
            for (const auto& [id, existing] : lm.combinations())
                if (existing.name() == combo.name()) sameName = &existing;
            if (sameName && sameName->type() == combo.type() && sameName->caseFactors() == factors)
                continue;   // déjà présente, identique
            LoadCombination c = combo;
            c.setCaseFactors(factors);
            if (sameName)
            {
                std::string name = combo.name() + " (copie)";
                for (int k = 2;; ++k)
                {
                    bool taken = false;
                    for (const auto& [id, existing] : lm.combinations()) taken = taken || existing.name() == name;
                    if (!taken) break;
                    name = combo.name() + " (copie " + std::to_string(k) + ")";
                }
                result.warnings.push_back(QCoreApplication::translate("TSA::Model::StructuralClipboard",
                                                                      "combinaison « %1 » : une combinaison de même nom et de contenu différent existe ; copiée sous "
                                                                      "le nom « %2 »")
                                              .arg(QString::fromStdString(combo.name()), QString::fromStdString(name))
                                              .toStdString());
                c.setName(name);
            }
            const int id = lm.addCombination(c);
            if (id > 0) result.combinationIds.push_back(id);
            else fail(QCoreApplication::translate("TSA::Model::StructuralClipboard", "combinaison « %1 »").arg(QString::fromStdString(combo.name())).toStdString());
        }
    }

    // Métadonnées BIM : un produit par produit source, éléments dans l'ordre de l'axe (BUG-029)
    if (!pasted.empty() && !m_bimProducts.empty())
    {
        std::vector<std::pair<TSA::BIM::PhysicalElement, std::vector<TSA::BIM::AnalyticalRef>>> groups;
        for (const auto& product : m_bimProducts)
        {
            std::vector<TSA::BIM::AnalyticalRef> refs;
            for (const auto& r : product.analytical)
                if (auto it = pasted.find(r); it != pasted.end()) refs.push_back(it->second);
            if (!refs.empty()) groups.emplace_back(product, std::move(refs));
        }
        model.bimForEdit().registerPasted(groups);
    }

    return result;
}

std::string StructuralClipboard::validate() const
{
    // Toutes les dépendances doivent être dans le presse-papier : sinon rien n'est collé
    std::set<int> nodes;
    for (const auto& n : m_nodes) nodes.insert(n.originalId);
    auto has = [&](int id) { return nodes.count(id) > 0; };
    for (const auto& e : m_beams)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poutre"), e.originalId)).toStdString();
    for (const auto& e : m_columns)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "poteau"), e.originalId)).toStdString();
    for (const auto& e : m_walls)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "voile"), e.originalId)).toStdString();
    for (const auto& e : m_trusses)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "barre de treillis"), e.originalId)).toStdString();
    for (const auto& e : m_cables)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "câble"), e.originalId)).toStdString();
    for (const auto& e : m_slabs)
    {
        if (e.originalNodeIds.size() < 3) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : moins de 3 nœuds").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "dalle"), e.originalId)).toStdString();
        for (int id : e.originalNodeIds)
            if (!has(id)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "dalle"), e.originalId)).toStdString();
    }
    for (const auto& e : m_foundations)
        if (!has(e.originalNodeId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "fondation"), e.originalId)).toStdString();
    for (const auto& l : m_nodalLoads)
        if (!has(l.nodeId()) || !m_loadCases.count(l.loadCaseId())) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : nœud ou cas de charge absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge nodale"), l.id())).toStdString();
    for (const auto& l : m_memberLoads)
        if (!m_loadCases.count(l.loadCaseId())) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : cas de charge absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge sur barre"), l.id())).toStdString();
    for (const auto& l : m_surfaceLoads)
        if (!m_loadCases.count(l.loadCaseId)) return QCoreApplication::translate("TSA::Model::StructuralClipboard", "%1 : cas de charge absent").arg(qFmtId(QT_TRANSLATE_NOOP("TSA::Model::StructuralClipboard", "charge surfacique"), l.id)).toStdString();
    return {};
}

PasteResult StructuralClipboard::pasteUndoable(Model& model, double targetX, double targetY, double targetZ, const std::string& actionName,
                                               const PasteOptions& options) const
{
    if (!hasData()) return {};
    PasteResult result;
    if (std::string error = validate(); !error.empty())
    {
        result.complete = false;   // rien n'a été modifié
        result.error = error;
        return result;
    }
    // Conflits à trancher ou élément dégénéré : rien n'est modifié, aucune entrée d'historique
    if (const PastePlan plan = analyzePaste(model, targetX, targetY, targetZ, options); plan.blocked() || plan.needsDecision())
        return pasteTo(model, targetX, targetY, targetZ, options);   // renvoie le diagnostic sans rien modifier
    // Une transaction = une étape d'annulation ; en cas d'échec, modèle restauré sans entrée Annuler ni Rétablir
    TSA::UndoRedo::EditTransaction tx(model, actionName);
    result = pasteTo(model, targetX, targetY, targetZ, options);
    if (!result.complete)
    {
        tx.rollback();
        PasteResult failed;
        failed.complete = false;
        failed.error = result.error;
        failed.conflicts = result.conflicts;
        return failed;
    }
    tx.commit();
    return result;
}

} // namespace TSA::Model
