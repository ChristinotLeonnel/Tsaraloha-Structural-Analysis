#include "StructuralClipboard.h"
#include "Model.h"
#include "Node.h"
#include "Beam.h"
#include "Column.h"
#include "Slab.h"
#include "Wall.h"
#include "TrussMember.h"
#include "Load/LoadManager.h"
#include "../UndoRedo/EditTransaction.h"
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

    double minX = 1e300, minY = 1e300, minZ = 1e300;
    for (int nid : allNodeIds)
        if (const auto* node = model.getNode(nid))
        {
            minX = std::min(minX, node->x());
            minY = std::min(minY, node->y());
            minZ = std::min(minZ, node->z());
        }
    if (minX == 1e300) return;   // aucun nœud existant : presse-papier vide

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
}

PasteResult StructuralClipboard::pasteTo(Model& model, double targetX, double targetY, double targetZ) const
{
    PasteResult result;
    if (!m_hasData || m_nodes.empty())
        return result;

    auto& nodeMap = result.nodeMap;
    auto& pasted = result.elementMap;   // original → collé
    auto fail = [&result](const std::string& what) {
        result.complete = false;
        if (result.error.empty()) result.error = what;
    };

    for (const auto& cn : m_nodes)
    {
        double nx = targetX + cn.relX;
        double ny = targetY + cn.relY;
        double nz = targetZ + cn.relZ;
        int newNId = model.addNode(nx, ny, nz);
        if (newNId <= 0)
        {
            fail("nœud " + std::to_string(cn.originalId));
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
        auto itS = nodeMap.find(cb.originalStartNodeId);
        auto itE = nodeMap.find(cb.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            BarProperties p = cb.props;
            int bId = model.addBar(p, itS->second, itE->second);
            if (bId <= 0)
            {
                fail("poutre " + std::to_string(cb.originalId));
                continue;
            }
            result.beamIds.push_back(bId);
            pasted[{ ElementKind::Beam, cb.originalId }] = { ElementKind::Beam, bId };
        }
    }

    for (const auto& cc : m_columns)
    {
        auto itS = nodeMap.find(cc.originalStartNodeId);
        auto itE = nodeMap.find(cc.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            BarProperties p = cc.props;
            int cId = model.addColumn(itS->second, itE->second, p.section, p.material, p.rotation, p.name);
            if (cId <= 0)
            {
                fail("poteau " + std::to_string(cc.originalId));
                continue;
            }
            result.columnIds.push_back(cId);
            pasted[{ ElementKind::Column, cc.originalId }] = { ElementKind::Column, cId };
        }
    }

    for (const auto& cs : m_slabs)
    {
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
            fail("dalle " + std::to_string(cs.originalId));
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
        auto itS = nodeMap.find(cw.originalStartNodeId);
        auto itE = nodeMap.find(cw.originalEndNodeId);
        const int wId = itS != nodeMap.end() && itE != nodeMap.end() ? model.addWall(itS->second, itE->second, cw.wall.height(), cw.wall.thickness(), cw.wall.name()) : -1;
        if (wId <= 0)
        {
            fail("voile " + std::to_string(cw.originalId));
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
        auto itS = nodeMap.find(ct.originalStartNodeId);
        auto itE = nodeMap.find(ct.originalEndNodeId);
        const int tId = itS != nodeMap.end() && itE != nodeMap.end() ? model.addTrussMember(itS->second, itE->second, 0.10, ct.member.name(), ct.member.role()) : -1;
        if (tId <= 0)
        {
            fail("barre de treillis " + std::to_string(ct.originalId));
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
        auto itS = nodeMap.find(ccab.originalStartNodeId);
        auto itE = nodeMap.find(ccab.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            int cId = model.addCable(itS->second, itE->second, ccab.definition, ccab.name, ccab.geometryMode, ccab.sag);
            if (cId <= 0)
            {
                fail("câble " + std::to_string(ccab.originalId));
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

    // Barres dont un nœud manque (ne devrait pas arriver : les extrémités sont toujours copiées)
    if (result.beamIds.size() != m_beams.size() || result.columnIds.size() != m_columns.size() || result.cableIds.size() != m_cables.size())
        fail("élément sans ses nœuds");

    // Charges : cas de charge rapprochés par nom et catégorie dans le projet cible, créés s'ils n'existent pas
    if (!m_nodalLoads.empty() || !m_memberLoads.empty() || !m_surfaceLoads.empty())
    {
        auto& lm = model.loadManager();
        for (const auto& [srcId, lc] : m_loadCases)
        {
            int target = 0;
            for (const auto& [id, existing] : lm.loadCases())
                if (existing.name() == lc.name() && existing.category() == lc.category())
                {
                    target = id;
                    break;
                }
            if (target == 0)
            {
                target = lm.addLoadCase(lc);
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
                fail("charge nodale " + std::to_string(nl.id()));
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
            if (it == pasted.end() || caseOf(ml.loadCaseId()) == 0)
            {
                fail("charge sur barre " + std::to_string(ml.id()));
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
            if (it == pasted.end() || caseOf(sl.loadCaseId) == 0)
            {
                fail("charge surfacique " + std::to_string(sl.id));
                continue;
            }
            sl.id = 0;
            sl.surfaceId = it->second.id;
            sl.loadCaseId = caseOf(sl.loadCaseId);
            result.surfaceLoadIds.push_back(lm.addSurfaceLoad(sl));
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
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return "poutre " + std::to_string(e.originalId) + " : nœud absent";
    for (const auto& e : m_columns)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return "poteau " + std::to_string(e.originalId) + " : nœud absent";
    for (const auto& e : m_walls)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return "voile " + std::to_string(e.originalId) + " : nœud absent";
    for (const auto& e : m_trusses)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return "barre de treillis " + std::to_string(e.originalId) + " : nœud absent";
    for (const auto& e : m_cables)
        if (!has(e.originalStartNodeId) || !has(e.originalEndNodeId)) return "câble " + std::to_string(e.originalId) + " : nœud absent";
    for (const auto& e : m_slabs)
    {
        if (e.originalNodeIds.size() < 3) return "dalle " + std::to_string(e.originalId) + " : moins de 3 nœuds";
        for (int id : e.originalNodeIds)
            if (!has(id)) return "dalle " + std::to_string(e.originalId) + " : nœud absent";
    }
    for (const auto& l : m_nodalLoads)
        if (!has(l.nodeId()) || !m_loadCases.count(l.loadCaseId())) return "charge nodale " + std::to_string(l.id()) + " : nœud ou cas de charge absent";
    for (const auto& l : m_memberLoads)
        if (!m_loadCases.count(l.loadCaseId())) return "charge sur barre " + std::to_string(l.id()) + " : cas de charge absent";
    for (const auto& l : m_surfaceLoads)
        if (!m_loadCases.count(l.loadCaseId)) return "charge surfacique " + std::to_string(l.id) + " : cas de charge absent";
    return {};
}

PasteResult StructuralClipboard::pasteUndoable(Model& model, double targetX, double targetY, double targetZ, const std::string& actionName) const
{
    if (!hasData()) return {};
    PasteResult result;
    if (std::string error = validate(); !error.empty())
    {
        result.complete = false;   // rien n'a été modifié
        result.error = error;
        return result;
    }
    // Une transaction = une étape d'annulation ; en cas d'échec, modèle restauré sans entrée Annuler ni Rétablir
    TSA::UndoRedo::EditTransaction tx(model, actionName);
    result = pasteTo(model, targetX, targetY, targetZ);
    if (!result.complete)
    {
        tx.rollback();
        PasteResult failed;
        failed.complete = false;
        failed.error = result.error;
        return failed;
    }
    tx.commit();
    return result;
}

} // namespace TSA::Model
