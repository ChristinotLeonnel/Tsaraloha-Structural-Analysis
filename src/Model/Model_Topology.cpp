#include "Model.h"
#include "ModelElementCopy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

namespace TSA::Model
{

namespace
{

/// Charges sur barre recopiables telles quelles sur chaque tronçon : intensité constante sur
/// toute la portée. Retourne false si une charge de l'élément ne l'est pas (division refusée).
bool collectSplittableLoads(const LoadManager& lm, int elementId, MemberTargetType target,
                            double length, std::vector<MemberLoad>& out)
{
    constexpr double eps = 1e-9;
    for (const auto& [id, ml] : lm.memberLoads())
    {
        if (ml.elementId() != elementId || ml.targetType() != target)
            continue;

        const bool fullSpan = ml.isFullSpan()
            || (ml.isRelativePosition() && std::abs(ml.x1()) < eps && std::abs(ml.x2() - 1.0) < 1e-6)
            || (!ml.isRelativePosition() && std::abs(ml.x1()) < eps && std::abs(ml.x2() - length) < 1e-6);

        bool constant = false;
        switch (ml.type())
        {
        case LoadType::SelfWeight:
        case LoadType::MemberUniform:
            constant = true;
            break;
        case LoadType::MemberLinear:
        case LoadType::MemberMoment:
            constant = std::abs(ml.q1() - ml.q2()) < eps;
            break;
        default:
            constant = false;
            break;
        }

        if (!fullSpan || !constant)
            return false;
        out.push_back(ml);
    }
    return true;
}

} // namespace

// -----------------------------------------------------------------------------
// Symétrie
// -----------------------------------------------------------------------------

std::vector<int> Model::mirrorElements(const std::set<int>& nodeIds,
                                       const std::set<int>& beamIds,
                                       const std::set<int>& columnIds,
                                       const std::set<int>& slabIds,
                                       const gp_Pnt& planePoint, const gp_Dir& planeNormal,
                                       bool keepOriginal,
                                       const std::set<int>& cableIds,
                                       double planeTol)
{
    std::vector<int> result;

    std::set<int> allNodeIds = nodeIds;
    for (int bId : beamIds)
        if (const auto* b = getBeam(bId)) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    for (int cId : columnIds)
        if (const auto* c = getColumn(cId)) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    for (int sId : slabIds)
        if (const auto* s = getSlab(sId)) allNodeIds.insert(s->nodeIds().begin(), s->nodeIds().end());
    for (int cId : cableIds)
        if (const auto* c = getCable(cId)) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }

    if (allNodeIds.empty())
        return result;

    gp_Trsf trsf;
    trsf.SetMirror(gp_Ax2(planePoint, planeNormal));

    // Un nœud situé sur le plan est son propre symétrique : déplacement ≤ 2·tol.
    auto mirrored = [&](const Node& n, bool& onPlane) {
        gp_Pnt p(n.x(), n.y(), n.z());
        gp_Pnt q = p.Transformed(trsf);
        onPlane = p.Distance(q) <= 2.0 * planeTol;
        return q;
    };

    if (!keepOriginal)
    {
        for (int nid : allNodeIds)
        {
            auto* n = getNode(nid);
            if (!n) continue;
            bool onPlane = false;
            gp_Pnt q = mirrored(*n, onPlane);
            if (onPlane) continue;
            n->setCoordinates(q.X(), q.Y(), q.Z());
            notifyNodeModified(nid);
            result.push_back(nid);
        }
        // La symétrie inverse le sens de parcours du contour : on le rétablit.
        for (int sId : slabIds)
        {
            if (auto* s = getSlab(sId))
            {
                std::vector<int> nids = s->nodeIds();
                std::reverse(nids.begin(), nids.end());
                s->setNodeIds(nids);
                notifySlabModified(sId);
            }
        }
        return result;
    }

    std::map<int, int> oldToNew;
    for (int nid : allNodeIds)
    {
        const auto* n = getNode(nid);
        if (!n) continue;
        bool onPlane = false;
        gp_Pnt q = mirrored(*n, onPlane);
        if (onPlane)
        {
            oldToNew[nid] = nid; // partagé entre l'original et sa copie
            continue;
        }
        const int newId = addNode(q.X(), q.Y(), q.Z());
        oldToNew[nid] = newId;
        result.push_back(newId);
    }

    auto isOwnImage = [&](int a, int b) { return oldToNew[a] == a && oldToNew[b] == b; };

    for (int bId : beamIds)
    {
        const auto* src = getBeam(bId);
        if (!src || isOwnImage(src->startNodeId(), src->endNodeId())) continue;
        const Beam srcCopy = *src; // addBeam peut réallouer
        const int id = addBeam(oldToNew[srcCopy.startNodeId()], oldToNew[srcCopy.endNodeId()], srcCopy.width(), srcCopy.height());
        if (auto* nb = getBeam(id)) { copyBeamAttributes(srcCopy, *nb); notifyBeamModified(id); }
        result.push_back(id);
    }

    for (int cId : columnIds)
    {
        const auto* src = getColumn(cId);
        if (!src || isOwnImage(src->startNodeId(), src->endNodeId())) continue;
        const Column srcCopy = *src;
        const int id = addColumn(oldToNew[srcCopy.startNodeId()], oldToNew[srcCopy.endNodeId()], srcCopy.width(), srcCopy.height());
        if (auto* nc = getColumn(id)) { copyColumnAttributes(srcCopy, *nc); notifyColumnModified(id); }
        result.push_back(id);
    }

    for (int sId : slabIds)
    {
        const auto* src = getSlab(sId);
        if (!src) continue;
        const Slab srcCopy = *src;
        std::vector<int> nids;
        bool allOnPlane = true;
        for (int nid : srcCopy.nodeIds())
        {
            nids.push_back(oldToNew[nid]);
            allOnPlane = allOnPlane && oldToNew[nid] == nid;
        }
        if (allOnPlane) continue;
        std::reverse(nids.begin(), nids.end());
        const int id = addSlab(nids, srcCopy.thickness(), "", srcCopy.slabType());
        if (auto* ns = getSlab(id))
        {
            ns->setMaterial(srcCopy.material());
            ns->setColor(srcCopy.color());
            notifySlabModified(id);
        }
        result.push_back(id);
    }

    for (int cId : cableIds)
    {
        const auto* src = getCable(cId);
        if (!src || isOwnImage(src->startNodeId(), src->endNodeId())) continue;
        const Cable srcCopy = *src;
        const int id = addCable(oldToNew[srcCopy.startNodeId()], oldToNew[srcCopy.endNodeId()],
                                srcCopy.definition(), srcCopy.name(), srcCopy.geometryMode(), srcCopy.sag());
        if (auto* nc = getCable(id)) { copyCableAttributes(srcCopy, *nc); notifyCableModified(id); }
        result.push_back(id);
    }

    return result;
}

// -----------------------------------------------------------------------------
// Division de barres
// -----------------------------------------------------------------------------

std::vector<int> Model::splitBeam(int beamId, int segments)
{
    const auto* b = getBeam(beamId);
    if (!b || segments < 2) return {};
    const auto* s = getNode(b->startNodeId());
    const auto* e = getNode(b->endNodeId());
    if (!s || !e) return {};

    const gp_Pnt ps(s->x(), s->y(), s->z());
    const gp_Pnt pe(e->x(), e->y(), e->z());
    const double length = ps.Distance(pe);
    if (length < 1e-6) return {};

    std::vector<MemberLoad> loads;
    if (!collectSplittableLoads(m_loadManager, beamId, MemberTargetType::Beam, length, loads))
        return {};

    const Beam original = *b;
    const int endNodeId = original.endNodeId();

    std::vector<int> chain; // nœuds intermédiaires
    for (int k = 1; k < segments; ++k)
    {
        const double t = static_cast<double>(k) / segments;
        chain.push_back(addNode(ps.X() + (pe.X() - ps.X()) * t,
                                ps.Y() + (pe.Y() - ps.Y()) * t,
                                ps.Z() + (pe.Z() - ps.Z()) * t));
    }

    syncBim();
    std::vector<int> result{ beamId };
    if (auto* first = getBeam(beamId))
    {
        first->setEndNodeId(chain.front());
        first->setEndRelease(EndRelease{});
        notifyBeamModified(beamId);
    }

    for (int k = 1; k < segments; ++k)
    {
        const int from = chain[k - 1];
        const int to = (k == segments - 1) ? endNodeId : chain[k];
        const int id = addBeam(from, to, original.width(), original.height());
        if (auto* nb = getBeam(id))
        {
            copyBeamAttributes(original, *nb);
            nb->setStartRelease(EndRelease{});
            if (k != segments - 1) nb->setEndRelease(EndRelease{});
            notifyBeamModified(id);
        }
        m_bim.attachSplit({ ElementKind::Beam, result.back() }, { ElementKind::Beam, id });
        for (MemberLoad ml : loads)
        {
            ml.setId(0);
            ml.setElementId(id);
            notifyMemberLoadAdded(m_loadManager.addMemberLoad(ml));
        }
        result.push_back(id);
    }
    return result;
}

std::vector<int> Model::splitColumn(int columnId, int segments)
{
    const auto* c = getColumn(columnId);
    if (!c || segments < 2) return {};
    const auto* s = getNode(c->startNodeId());
    const auto* e = getNode(c->endNodeId());
    if (!s || !e) return {};

    const gp_Pnt ps(s->x(), s->y(), s->z());
    const gp_Pnt pe(e->x(), e->y(), e->z());
    const double length = ps.Distance(pe);
    if (length < 1e-6) return {};

    std::vector<MemberLoad> loads;
    if (!collectSplittableLoads(m_loadManager, columnId, MemberTargetType::Column, length, loads))
        return {};

    const Column original = *c;
    const int endNodeId = original.endNodeId();

    std::vector<int> chain;
    for (int k = 1; k < segments; ++k)
    {
        const double t = static_cast<double>(k) / segments;
        chain.push_back(addNode(ps.X() + (pe.X() - ps.X()) * t,
                                ps.Y() + (pe.Y() - ps.Y()) * t,
                                ps.Z() + (pe.Z() - ps.Z()) * t));
    }

    syncBim();
    std::vector<int> result{ columnId };
    if (auto* first = getColumn(columnId))
    {
        first->setEndNodeId(chain.front());
        notifyColumnModified(columnId);
    }

    for (int k = 1; k < segments; ++k)
    {
        const int from = chain[k - 1];
        const int to = (k == segments - 1) ? endNodeId : chain[k];
        const int id = addColumn(from, to, original.width(), original.height());
        if (auto* nc = getColumn(id))
        {
            copyColumnAttributes(original, *nc);
            notifyColumnModified(id);
        }
        m_bim.attachSplit({ ElementKind::Column, result.back() }, { ElementKind::Column, id });
        for (MemberLoad ml : loads)
        {
            ml.setId(0);
            ml.setElementId(id);
            notifyMemberLoadAdded(m_loadManager.addMemberLoad(ml));
        }
        result.push_back(id);
    }
    return result;
}

int Model::splitBarAt(ElementKind kind, int id, double t, int nodeId, int* newBarId)
{
    if (newBarId) *newBarId = 0;
    constexpr double kEnd = 1e-6;
    if (t <= kEnd || t >= 1.0 - kEnd) return 0;

    int startId = 0, endId = 0;
    MemberTargetType target = MemberTargetType::Beam;
    switch (kind)
    {
    case ElementKind::Beam: if (const auto* b = getBeam(id)) { startId = b->startNodeId(); endId = b->endNodeId(); } break;
    case ElementKind::Column: if (const auto* c = getColumn(id)) { startId = c->startNodeId(); endId = c->endNodeId(); } target = MemberTargetType::Column; break;
    case ElementKind::TrussMember: if (const auto* m = getTrussMember(id)) { startId = m->startNodeId(); endId = m->endNodeId(); } target = MemberTargetType::Truss; break;
    default: return 0;
    }
    const auto* s = getNode(startId);
    const auto* e = getNode(endId);
    if (!s || !e) return 0;
    if (nodeId == startId || nodeId == endId) return 0;

    const gp_Pnt ps(s->x(), s->y(), s->z()), pe(e->x(), e->y(), e->z());
    const double length = ps.Distance(pe);
    if (length < 1e-6) return 0;

    std::vector<MemberLoad> loads;
    if (!collectSplittableLoads(m_loadManager, id, target, length, loads)) return 0;

    const int mid = (nodeId > 0 && getNode(nodeId)) ? nodeId
        : addNode(ps.X() + (pe.X() - ps.X()) * t, ps.Y() + (pe.Y() - ps.Y()) * t, ps.Z() + (pe.Z() - ps.Z()) * t);

    syncBim();
    int created = 0;
    switch (kind)
    {
    case ElementKind::Beam:
    {
        const Beam original = *getBeam(id);
        if (auto* first = getBeam(id)) { first->setEndNodeId(mid); first->setEndRelease(EndRelease{}); notifyBeamModified(id); }
        created = addBeam(mid, endId, original.width(), original.height());
        if (auto* nb = getBeam(created)) { copyBeamAttributes(original, *nb); nb->setStartRelease(EndRelease{}); notifyBeamModified(created); }
        break;
    }
    case ElementKind::Column:
    {
        const Column original = *getColumn(id);
        if (auto* first = getColumn(id)) { first->setEndNodeId(mid); notifyColumnModified(id); }
        created = addColumn(mid, endId, original.width(), original.height());
        if (auto* nc = getColumn(created)) { copyColumnAttributes(original, *nc); notifyColumnModified(created); }
        break;
    }
    default:
    {
        const TrussMember original = *getTrussMember(id);
        if (auto* first = getTrussMember(id)) { first->setEndNodeId(mid); notifyTrussMemberModified(id); }
        created = addTrussMember(mid, endId, 0.10, original.name(), original.role());
        if (auto* nt = getTrussMember(created)) { copyTrussAttributes(original, *nt); notifyTrussMemberModified(created); }
        break;
    }
    }
    for (MemberLoad ml : loads)
    {
        ml.setId(0);
        ml.setElementId(created);
        notifyMemberLoadAdded(m_loadManager.addMemberLoad(ml));
    }
    m_bim.attachSplit({ kind, id }, { kind, created });   // une poutre physique, N barres
    if (newBarId) *newBarId = created;
    return mid;
}

bool Model::transformNodes(const std::set<int>& nodeIds, const gp_Trsf& trsf)
{
    bool any = false;
    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (!n) continue;
        const gp_Pnt p = gp_Pnt(n->x(), n->y(), n->z()).Transformed(trsf);
        n->setCoordinates(p.X(), p.Y(), p.Z());
        notifyNodeModified(nid);
        any = true;
    }
    return any;
}

// -----------------------------------------------------------------------------
// Nœuds confondus
// -----------------------------------------------------------------------------

std::map<int, int> Model::findCoincidentNodes(double tol) const
{
    std::map<int, int> duplicates;
    if (tol <= 0.0 || m_nodes.size() < 2)
        return duplicates;

    // Hachage spatial (cellules de côté tol) : seules les 27 cellules voisines sont comparées.
    using Cell = std::array<long long, 3>;
    std::map<Cell, std::vector<int>> grid;
    auto cellOf = [tol](double v) { return static_cast<long long>(std::floor(v / tol)); };

    for (const auto& [id, n] : m_nodes) // ordre croissant : le plus petit id est conservé
    {
        const Cell c{ cellOf(n.x()), cellOf(n.y()), cellOf(n.z()) };
        int keeper = 0;
        for (long long dx = -1; dx <= 1 && !keeper; ++dx)
            for (long long dy = -1; dy <= 1 && !keeper; ++dy)
                for (long long dz = -1; dz <= 1 && !keeper; ++dz)
                {
                    auto it = grid.find({ c[0] + dx, c[1] + dy, c[2] + dz });
                    if (it == grid.end()) continue;
                    for (int kid : it->second)
                    {
                        const Node& k = m_nodes.at(kid);
                        const double ddx = k.x() - n.x(), ddy = k.y() - n.y(), ddz = k.z() - n.z();
                        if (ddx * ddx + ddy * ddy + ddz * ddz <= tol * tol) { keeper = kid; break; }
                    }
                }

        if (keeper)
            duplicates[id] = keeper;
        else
            grid[c].push_back(id);
    }
    return duplicates;
}

int Model::mergeCoincidentNodes(double tol)
{
    const std::map<int, int> dup = findCoincidentNodes(tol);
    if (dup.empty())
        return 0;

    auto remap = [&dup](int id) {
        auto it = dup.find(id);
        return it == dup.end() ? id : it->second;
    };

    // Appuis : un nœud conservé libre reprend l'appui de son doublon.
    for (const auto& [d, k] : dup)
    {
        const auto* dn = getNode(d);
        auto* kn = getNode(k);
        if (dn && kn && dn->support().isSupported() && !kn->support().isSupported())
        {
            kn->setSupport(dn->support());
            notifyNodeModified(k);
        }
    }

    // Éléments linéaires : report des extrémités, suppression des éléments dégénérés.
    auto relink = [&](auto& elements, auto notifyModified, auto removeElement) {
        std::vector<int> modified, degenerate;
        for (auto& [id, el] : elements)
        {
            const int s = remap(el.startNodeId());
            const int e = remap(el.endNodeId());
            if (s == el.startNodeId() && e == el.endNodeId()) continue;
            el.setStartNodeId(s);
            el.setEndNodeId(e);
            (s == e ? degenerate : modified).push_back(id);
        }
        for (int id : modified) (this->*notifyModified)(id);
        for (int id : degenerate) (this->*removeElement)(id);
    };
    relink(m_beams, &Model::notifyBeamModified, &Model::removeBeam);
    relink(m_columns, &Model::notifyColumnModified, &Model::removeColumn);
    relink(m_walls, &Model::notifyWallModified, &Model::removeWall);
    relink(m_trussMembers, &Model::notifyTrussMemberModified, &Model::removeTrussMember);
    relink(m_cables, &Model::notifyCableModified, &Model::removeCable);

    {
        std::vector<int> modified, degenerate;
        for (auto& [id, slab] : m_slabs)
        {
            std::vector<int> nids;
            bool changed = false;
            for (int nid : slab.nodeIds())
            {
                const int r = remap(nid);
                changed = changed || r != nid;
                if (nids.empty() || nids.back() != r) nids.push_back(r);
            }
            if (!changed) continue;
            if (nids.size() > 1 && nids.front() == nids.back()) nids.pop_back();
            slab.setNodeIds(nids);
            (nids.size() < 3 ? degenerate : modified).push_back(id);
        }
        for (int id : modified) notifySlabModified(id);
        for (int id : degenerate) removeSlab(id);
    }

    {
        std::vector<int> modified;
        for (auto& [id, f] : m_foundations)
        {
            const int r = remap(f.nodeId());
            if (r == f.nodeId()) continue;
            f.setNodeId(r);
            modified.push_back(id);
        }
        for (int id : modified) notifyFoundationModified(id);
    }

    {
        std::vector<int> modified;
        for (const auto& [id, nl] : m_loadManager.nodalLoads())
            if (dup.count(nl.nodeId())) modified.push_back(id);
        for (int id : modified)
        {
            if (auto* nl = m_loadManager.getNodalLoad(id))
            {
                nl->setNodeId(remap(nl->nodeId()));
                notifyNodalLoadModified(id);
            }
        }
    }

    for (const auto& [d, k] : dup)
        removeNode(d);

    return static_cast<int>(dup.size());
}

} // namespace TSA::Model
