#include "Model.h"

#include <algorithm>
#include <cmath>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>

namespace TSA::Model
{

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
                                     const std::set<int>& cableIds)
{
    std::vector<int> newElementIds;
    if (repetitions < 1)
        return newElementIds;

    // Déterminer l'ensemble de tous les nœuds impliqués (explicites + connectés aux barres/dalles/câbles)
    std::set<int> allNodeIds = nodeIds;
    for (int bId : beamIds)
    {
        const auto* b = getBeam(bId);
        if (b) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    }
    for (int cId : columnIds)
    {
        const auto* c = getColumn(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }
    for (int sId : slabIds)
    {
        const auto* s = getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) allNodeIds.insert(nid);
        }
    }
    for (int cId : cableIds)
    {
        const auto* c = getCable(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }

    for (int step = 1; step <= repetitions; ++step)
    {
        double curDx = dx * step;
        double curDy = dy * step;
        double curDz = dz * step;

        std::map<int, int> oldToNewNodes;
        for (int oldNid : allNodeIds)
        {
            const auto* origNode = getNode(oldNid);
            if (origNode)
            {
                int newNid = addNode(origNode->x() + curDx, origNode->y() + curDy, origNode->z() + curDz);
                oldToNewNodes[oldNid] = newNid;
                newElementIds.push_back(newNid);
            }
        }

        for (int bId : beamIds)
        {
            const auto* origBeam = getBeam(bId);
            if (origBeam)
            {
                int newStart = oldToNewNodes[origBeam->startNodeId()];
                int newEnd = oldToNewNodes[origBeam->endNodeId()];
                int newBId = addBeam(newStart, newEnd, origBeam->width(), origBeam->height());
                if (auto* nb = getBeam(newBId))
                {
                    nb->setSection(origBeam->section());
                    nb->setMaterial(origBeam->material());
                    nb->setRotation(origBeam->rotation());
                    nb->setEccentricity(origBeam->eccentricity());
                    nb->setStartRelease(origBeam->startRelease());
                    nb->setEndRelease(origBeam->endRelease());
                    nb->setColor(origBeam->color());
                }
                newElementIds.push_back(newBId);
            }
        }

        for (int cId : columnIds)
        {
            const auto* origCol = getColumn(cId);
            if (origCol)
            {
                int newStart = oldToNewNodes[origCol->startNodeId()];
                int newEnd = oldToNewNodes[origCol->endNodeId()];
                int newCId = addColumn(newStart, newEnd, origCol->width(), origCol->height());
                if (auto* nc = getColumn(newCId))
                {
                    nc->setSection(origCol->section());
                    nc->setMaterial(origCol->material());
                    nc->setRotation(origCol->rotation());
                    nc->setColor(origCol->color());
                }
                newElementIds.push_back(newCId);
            }
        }

        for (int sId : slabIds)
        {
            const auto* origSlab = getSlab(sId);
            if (origSlab)
            {
                std::vector<int> newSlabNodes;
                for (int nid : origSlab->nodeIds())
                {
                    newSlabNodes.push_back(oldToNewNodes[nid]);
                }
                int newSId = addSlab(newSlabNodes, origSlab->thickness());
                if (auto* ns = getSlab(newSId))
                {
                    ns->setMaterial(origSlab->material());
                    ns->setSlabType(origSlab->slabType());
                    ns->setColor(origSlab->color());
                }
                newElementIds.push_back(newSId);
            }
        }

        for (int cId : cableIds)
        {
            const auto* origCable = getCable(cId);
            if (origCable)
            {
                int newStart = oldToNewNodes[origCable->startNodeId()];
                int newEnd = oldToNewNodes[origCable->endNodeId()];
                int newCId = addCable(newStart, newEnd, origCable->definition(), origCable->name(), origCable->geometryMode(), origCable->sag());
                if (auto* nc = getCable(newCId))
                {
                    nc->setType(origCable->type());
                    nc->setSection(origCable->section());
                    nc->setMaterial(origCable->material());
                    nc->setPrestress(origCable->prestress());
                    nc->setAnalysisProperties(origCable->analysisProperties());
                    nc->setStartAnchor(origCable->startAnchor());
                    nc->setEndAnchor(origCable->endAnchor());
                    nc->setColor(origCable->color());
                }
                newElementIds.push_back(newCId);
            }
        }
    }

    return newElementIds;
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
                                              const std::set<int>& cableIds)
{
    std::vector<int> newElementIds;
    if (repetitions < 1 || std::abs(angleRad) < 1e-7)
        return newElementIds;

    std::set<int> allNodeIds = nodeIds;
    for (int bId : beamIds)
    {
        const auto* b = getBeam(bId);
        if (b) { allNodeIds.insert(b->startNodeId()); allNodeIds.insert(b->endNodeId()); }
    }
    for (int cId : columnIds)
    {
        const auto* c = getColumn(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }
    for (int sId : slabIds)
    {
        const auto* s = getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds()) allNodeIds.insert(nid);
        }
    }
    for (int cId : cableIds)
    {
        const auto* c = getCable(cId);
        if (c) { allNodeIds.insert(c->startNodeId()); allNodeIds.insert(c->endNodeId()); }
    }

    for (int step = 1; step <= repetitions; ++step)
    {
        double curAngle = angleRad * step;
        gp_Trsf trsf;
        trsf.SetRotation(gp_Ax1(center, axis), curAngle);

        std::map<int, int> oldToNewNodes;
        for (int oldNid : allNodeIds)
        {
            const auto* origNode = getNode(oldNid);
            if (origNode)
            {
                gp_Pnt p(origNode->x(), origNode->y(), origNode->z());
                p.Transform(trsf);
                int newNid = addNode(p.X(), p.Y(), p.Z());
                oldToNewNodes[oldNid] = newNid;
                newElementIds.push_back(newNid);
            }
        }

        for (int bId : beamIds)
        {
            const auto* origBeam = getBeam(bId);
            if (origBeam)
            {
                int newStart = oldToNewNodes[origBeam->startNodeId()];
                int newEnd = oldToNewNodes[origBeam->endNodeId()];
                int newBId = addBeam(newStart, newEnd, origBeam->width(), origBeam->height());
                if (auto* nb = getBeam(newBId))
                {
                    nb->setSection(origBeam->section());
                    nb->setMaterial(origBeam->material());
                    nb->setRotation(origBeam->rotation());
                    nb->setEccentricity(origBeam->eccentricity());
                    nb->setStartRelease(origBeam->startRelease());
                    nb->setEndRelease(origBeam->endRelease());
                    nb->setColor(origBeam->color());
                }
                newElementIds.push_back(newBId);
            }
        }

        for (int cId : columnIds)
        {
            const auto* origCol = getColumn(cId);
            if (origCol)
            {
                int newStart = oldToNewNodes[origCol->startNodeId()];
                int newEnd = oldToNewNodes[origCol->endNodeId()];
                int newCId = addColumn(newStart, newEnd, origCol->width(), origCol->height());
                if (auto* nc = getColumn(newCId))
                {
                    nc->setSection(origCol->section());
                    nc->setMaterial(origCol->material());
                    nc->setRotation(origCol->rotation());
                    nc->setColor(origCol->color());
                }
                newElementIds.push_back(newCId);
            }
        }

        for (int sId : slabIds)
        {
            const auto* origSlab = getSlab(sId);
            if (origSlab)
            {
                std::vector<int> newSlabNodes;
                for (int nid : origSlab->nodeIds())
                {
                    newSlabNodes.push_back(oldToNewNodes[nid]);
                }
                int newSId = addSlab(newSlabNodes, origSlab->thickness());
                if (auto* ns = getSlab(newSId))
                {
                    ns->setMaterial(origSlab->material());
                    ns->setSlabType(origSlab->slabType());
                    ns->setColor(origSlab->color());
                }
                newElementIds.push_back(newSId);
            }
        }

        for (int cId : cableIds)
        {
            const auto* origCable = getCable(cId);
            if (origCable)
            {
                int newStart = oldToNewNodes[origCable->startNodeId()];
                int newEnd = oldToNewNodes[origCable->endNodeId()];
                int newCId = addCable(newStart, newEnd, origCable->definition(), origCable->name(), origCable->geometryMode(), origCable->sag());
                if (auto* nc = getCable(newCId))
                {
                    nc->setType(origCable->type());
                    nc->setSection(origCable->section());
                    nc->setMaterial(origCable->material());
                    nc->setPrestress(origCable->prestress());
                    nc->setAnalysisProperties(origCable->analysisProperties());
                    nc->setStartAnchor(origCable->startAnchor());
                    nc->setEndAnchor(origCable->endAnchor());
                    nc->setColor(origCable->color());
                }
                newElementIds.push_back(newCId);
            }
        }
    }

    return newElementIds;
}

} // namespace TSA::Model
