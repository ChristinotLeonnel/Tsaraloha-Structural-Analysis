#include "Model.h"
#include "ModelElementCopy.h"

#include <functional>
#include <map>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>

namespace TSA::Model
{

namespace
{
/// Copie transformée d'une sélection, répétée `repetitions` fois (transformation de l'étape k
/// fournie par trsfOf(k)). Les nœuds copiés gardent leurs appuis ; chaque élément copié reçoit
/// TOUS les attributs de l'original (section, matériau, rôle, relâchements…) puis est notifié :
/// à l'ajout, les vues ont construit la forme avec les valeurs par défaut (ex. section
/// rectangulaire affichée pour une copie de section circulaire).
std::vector<int> copyTransformed(Model& m,
                                 const std::set<int>& nodeIds, const std::set<int>& beamIds,
                                 const std::set<int>& columnIds, const std::set<int>& slabIds,
                                 const std::set<int>& cableIds, const std::set<int>& trussIds,
                                 int repetitions, const std::function<gp_Trsf(int)>& trsfOf)
{
    std::vector<int> created;
    if (repetitions < 1)
        return created;

    std::set<int> allNodeIds = nodeIds;
    auto addEnds = [&](int a, int b) { allNodeIds.insert(a); allNodeIds.insert(b); };
    for (int id : beamIds) if (const auto* e = m.getBeam(id)) addEnds(e->startNodeId(), e->endNodeId());
    for (int id : columnIds) if (const auto* e = m.getColumn(id)) addEnds(e->startNodeId(), e->endNodeId());
    for (int id : cableIds) if (const auto* e = m.getCable(id)) addEnds(e->startNodeId(), e->endNodeId());
    for (int id : trussIds) if (const auto* e = m.getTrussMember(id)) addEnds(e->startNodeId(), e->endNodeId());
    for (int id : slabIds) if (const auto* e = m.getSlab(id)) allNodeIds.insert(e->nodeIds().begin(), e->nodeIds().end());

    for (int step = 1; step <= repetitions; ++step)
    {
        const gp_Trsf trsf = trsfOf(step);
        std::map<int, int> newNode;
        std::vector<std::pair<TSA::BIM::AnalyticalRef, TSA::BIM::AnalyticalRef>> bimCopies;
        for (int nid : allNodeIds)
        {
            const auto* n = m.getNode(nid);
            if (!n) continue;
            const SupportDefinition support = n->support();
            const gp_Pnt p = gp_Pnt(n->x(), n->y(), n->z()).Transformed(trsf);
            const int id = m.addNode(p.X(), p.Y(), p.Z());
            if (auto* nn = m.getNode(id))
            {
                nn->setSupport(support);
                m.notifyNodeModified(id);
            }
            newNode[nid] = id;
            created.push_back(id);
        }

        for (int id : beamIds)
        {
            const auto* src = m.getBeam(id);
            if (!src) continue;
            const Beam o = *src;
            const int nid = m.addBeam(newNode[o.startNodeId()], newNode[o.endNodeId()], o.width(), o.height());
            if (auto* e = m.getBeam(nid)) { copyBeamAttributes(o, *e); m.notifyBeamModified(nid); }
            bimCopies.push_back({ { ElementKind::Beam, id }, { ElementKind::Beam, nid } });
            created.push_back(nid);
        }
        for (int id : columnIds)
        {
            const auto* src = m.getColumn(id);
            if (!src) continue;
            const Column o = *src;
            const int nid = m.addColumn(newNode[o.startNodeId()], newNode[o.endNodeId()], o.width(), o.height());
            if (auto* e = m.getColumn(nid)) { copyColumnAttributes(o, *e); m.notifyColumnModified(nid); }
            bimCopies.push_back({ { ElementKind::Column, id }, { ElementKind::Column, nid } });
            created.push_back(nid);
        }
        for (int id : trussIds)
        {
            const auto* src = m.getTrussMember(id);
            if (!src) continue;
            const TrussMember o = *src;
            const int nid = m.addTrussMember(newNode[o.startNodeId()], newNode[o.endNodeId()], 0.10, o.name(), o.role());
            if (auto* e = m.getTrussMember(nid)) { copyTrussAttributes(o, *e); m.notifyTrussMemberModified(nid); }
            bimCopies.push_back({ { ElementKind::TrussMember, id }, { ElementKind::TrussMember, nid } });
            created.push_back(nid);
        }
        for (int id : slabIds)
        {
            const auto* src = m.getSlab(id);
            if (!src) continue;
            const Slab o = *src;
            std::vector<int> nids;
            for (int n : o.nodeIds()) nids.push_back(newNode[n]);
            // Une transformation qui inverse l'orientation (symétrie) n'est pas utilisée ici :
            // translation et rotation conservent le sens de parcours du contour.
            const int nid = m.addSlab(nids, o.thickness());
            if (auto* e = m.getSlab(nid)) { copySlabAttributes(o, *e); m.notifySlabModified(nid); }
            bimCopies.push_back({ { ElementKind::Slab, id }, { ElementKind::Slab, nid } });
            created.push_back(nid);
        }
        for (int id : cableIds)
        {
            const auto* src = m.getCable(id);
            if (!src) continue;
            const Cable o = *src;
            const int nid = m.addCable(newNode[o.startNodeId()], newNode[o.endNodeId()], o.definition(), o.name(),
                                       o.geometryMode(), o.sag());
            if (auto* e = m.getCable(nid)) { copyCableAttributes(o, *e); m.notifyCableModified(nid); }
            bimCopies.push_back({ { ElementKind::Cable, id }, { ElementKind::Cable, nid } });
            created.push_back(nid);
        }
        // Les copies d'une poutre physique divisée forment une nouvelle poutre physique
        m.bimForEdit().registerCopies(bimCopies);
    }
    return created;
}
} // namespace


bool Model::moveNodes(const std::set<int>& nodeIds, double dx, double dy, double dz)
{
    if (nodeIds.empty())
        return false;

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            n->setCoordinates(n->x() + dx, n->y() + dy, n->z() + dz);
            notifyNodeModified(nid);
        }
    }

    return true;
}

std::vector<int> Model::copyElements(const std::set<int>& nodeIds,
                                     const std::set<int>& beamIds,
                                     const std::set<int>& columnIds,
                                     const std::set<int>& slabIds,
                                     double dx, double dy, double dz, int repetitions,
                                     const std::set<int>& cableIds,
                                     const std::set<int>& trussIds)
{
    return copyTransformed(*this, nodeIds, beamIds, columnIds, slabIds, cableIds, trussIds, repetitions,
                           [&](int step) {
                               gp_Trsf t;
                               t.SetTranslation(gp_Vec(dx * step, dy * step, dz * step));
                               return t;
                           });
}

bool Model::rotateNodes(const std::set<int>& nodeIds, const gp_Pnt& center, const gp_Dir& axis, double angleRad)
{
    if (nodeIds.empty() || std::abs(angleRad) < 1e-7)
        return false;

    gp_Trsf trsf;
    trsf.SetRotation(gp_Ax1(center, axis), angleRad);

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            gp_Pnt p(n->x(), n->y(), n->z());
            p.Transform(trsf);
            n->setCoordinates(p.X(), p.Y(), p.Z());
            notifyNodeModified(nid);
        }
    }
    return true;
}

std::vector<int> Model::copyAndRotateElements(const std::set<int>& nodeIds,
                                              const std::set<int>& beamIds,
                                              const std::set<int>& columnIds,
                                              const std::set<int>& slabIds,
                                              const gp_Pnt& center, const gp_Dir& axis,
                                              double angleRad, int repetitions,
                                              const std::set<int>& cableIds,
                                              const std::set<int>& trussIds)
{
    if (std::abs(angleRad) < 1e-7)
        return {};
    return copyTransformed(*this, nodeIds, beamIds, columnIds, slabIds, cableIds, trussIds, repetitions,
                           [&](int step) {
                               gp_Trsf t;
                               t.SetRotation(gp_Ax1(center, axis), angleRad * step);
                               return t;
                           });
}

} // namespace TSA::Model
