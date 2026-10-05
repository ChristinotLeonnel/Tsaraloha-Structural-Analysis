#include "StructuralClipboard.h"
#include "Model.h"
#include "Node.h"
#include "Beam.h"
#include "Column.h"
#include "Slab.h"
#include <algorithm>
#include <unordered_set>

namespace TSA::Model
{

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
    clear();

    std::unordered_set<int> allNodeIds(selectedNodes.begin(), selectedNodes.end());

    for (int bId : selectedBeams)
    {
        const auto* b = model.getBeam(bId);
        if (b)
        {
            allNodeIds.insert(b->startNodeId());
            allNodeIds.insert(b->endNodeId());
        }
    }
    for (int cId : selectedColumns)
    {
        const auto* c = model.getColumn(cId);
        if (c)
        {
            allNodeIds.insert(c->startNodeId());
            allNodeIds.insert(c->endNodeId());
        }
    }
    for (int sId : selectedSlabs)
    {
        const auto* s = model.getSlab(sId);
        if (s)
        {
            for (int nid : s->nodeIds())
            {
                allNodeIds.insert(nid);
            }
        }
    }
    for (int cabId : selectedCables)
    {
        const auto* c = model.getCable(cabId);
        if (c)
        {
            allNodeIds.insert(c->startNodeId());
            allNodeIds.insert(c->endNodeId());
        }
    }

    if (allNodeIds.empty())
        return;

    double minX = 1e9, minY = 1e9, minZ = 1e9;
    for (int nid : allNodeIds)
    {
        const auto* node = model.getNode(nid);
        if (node)
        {
            minX = std::min(minX, node->x());
            minY = std::min(minY, node->y());
            minZ = std::min(minZ, node->z());
        }
    }

    m_hasData = true;
    m_refOriginX = minX;
    m_refOriginY = minY;
    m_refOriginZ = minZ;

    for (int nid : allNodeIds)
    {
        const auto* node = model.getNode(nid);
        if (node)
        {
            ClipboardNode cn;
            cn.originalId = nid;
            cn.relX = node->x() - minX;
            cn.relY = node->y() - minY;
            cn.relZ = node->z() - minZ;
            m_nodes.push_back(cn);
        }
    }

    for (int bId : selectedBeams)
    {
        const auto* b = model.getBeam(bId);
        if (b)
        {
            ClipboardBeam cb;
            cb.originalStartNodeId = b->startNodeId();
            cb.originalEndNodeId = b->endNodeId();
            cb.props = b->properties();
            m_beams.push_back(cb);
        }
    }

    for (int cId : selectedColumns)
    {
        const auto* c = model.getColumn(cId);
        if (c)
        {
            ClipboardColumn cc;
            cc.originalStartNodeId = c->startNodeId();
            cc.originalEndNodeId = c->endNodeId();
            cc.props = c->properties();
            m_columns.push_back(cc);
        }
    }

    for (int sId : selectedSlabs)
    {
        const auto* s = model.getSlab(sId);
        if (s)
        {
            ClipboardSlab cs;
            cs.originalNodeIds = s->nodeIds();
            cs.thickness = s->thickness();
            cs.material = s->material();
            m_slabs.push_back(cs);
        }
    }

    for (int cabId : selectedCables)
    {
        const auto* c = model.getCable(cabId);
        if (c)
        {
            ClipboardCable ccab;
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
}

PasteResult StructuralClipboard::pasteTo(Model& model, double targetX, double targetY, double targetZ) const
{
    PasteResult result;
    if (!m_hasData || m_nodes.empty())
        return result;

    std::unordered_map<int, int> nodeMap;

    for (const auto& cn : m_nodes)
    {
        double nx = targetX + cn.relX;
        double ny = targetY + cn.relY;
        double nz = targetZ + cn.relZ;
        int newNId = model.addNode(nx, ny, nz);
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
            result.beamIds.push_back(bId);
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
            result.columnIds.push_back(cId);
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
        if (sNodes.size() >= 3)
        {
            int sId = model.addSlab(sNodes, cs.thickness);
            auto* s = model.getSlab(sId);
            if (s)
            {
                s->setMaterial(cs.material);
                model.notifySlabModified(sId);   // vue créée à l'ajout avec le matériau par défaut
            }
            result.slabIds.push_back(sId);
        }
    }

    for (const auto& ccab : m_cables)
    {
        auto itS = nodeMap.find(ccab.originalStartNodeId);
        auto itE = nodeMap.find(ccab.originalEndNodeId);
        if (itS != nodeMap.end() && itE != nodeMap.end())
        {
            int cId = model.addCable(itS->second, itE->second, ccab.definition, ccab.name, ccab.geometryMode, ccab.sag);
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
        }
    }

    return result;
}

} // namespace TSA::Model
