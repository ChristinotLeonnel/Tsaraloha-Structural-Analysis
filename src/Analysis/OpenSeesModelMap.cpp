#include "OpenSeesModelMap.h"
#include "OpenSeesAnalysisBuilder.h"
#include "LoadResolver.h"

#include <algorithm>
#include <set>

namespace TSA::Analysis
{

namespace
{
bool isFixed(const SnapshotNode& n)
{
    return n.fixTx || n.fixTy || n.fixTz || n.fixRx || n.fixRy || n.fixRz;
}
} // namespace

OpenSeesModelMap OpenSeesModelMap::build(const CalculationSnapshot& snapshot, const AnalysisParameters& params)
{
    OpenSeesModelMap map;

    int maxNodeId = 0;
    for (const auto& [id, n] : snapshot.nodes())
    {
        map.m_structuralNodeTags.push_back(id);
        maxNodeId = std::max(maxNodeId, id);
        const bool fixes[6] = { n.fixTx, n.fixTy, n.fixTz, n.fixRx, n.fixRy, n.fixRz };
        for (bool f : fixes)
            if (!f) ++map.m_estimatedFreeDofs;
    }

    const bool corotTruss = params.trussFormulation == TrussFormulation::CorotTruss
                         || params.trussFormulation == TrussFormulation::CorotTrussSection;

    int transfTag = 1;
    int maxElementTag = 0;
    for (const auto& [tag, el] : snapshot.elements())
    {
        OpsElementEntry e;
        e.key = el.key();
        e.tag = tag;
        e.nodeI = el.startNodeId;
        e.nodeJ = el.endNodeId;
        maxElementTag = std::max(maxElementTag, tag);

        switch (el.type)
        {
        case StructuralElementKind::Truss: e.opsClass = corotTruss ? "corotTruss" : "truss"; break;
        case StructuralElementKind::Cable: e.opsClass = "corotTruss"; break;
        default: e.opsClass = "elasticBeamColumn"; break;
        }

        const auto* n1 = snapshot.getNode(el.startNodeId);
        const auto* n2 = snapshot.getNode(el.endNodeId);
        if (n1 && n2)
        {
            const gp_Pnt p1(n1->x, n1->y, n1->z);
            const gp_Pnt p2(n2->x, n2->y, n2->z);
            // Même repère que celui transmis à geomTransf (axe z local TSA = vecxz).
            const double beta = e.isBeamColumn() ? el.rotation : 0.0;
            const gp_Dir z = LoadResolver::computeElementLocalAxes(p1, p2, beta).Direction();
            e.vecxz = { z.X(), z.Y(), z.Z() };
            e.axes = ElementTransformation::openSeesAxes({ n1->x, n1->y, n1->z }, { n2->x, n2->y, n2->z }, e.vecxz);
        }
        if (e.isBeamColumn())
            e.transfTag = transfTag++;

        map.m_indexByTag[tag] = map.m_elements.size();
        map.m_indexByKey[e.key] = map.m_elements.size();
        map.m_allElementTags.push_back(tag);
        if (e.isBeamColumn())
        {
            map.m_beamColumnTags.push_back(tag);
            map.m_kbBeamTags.push_back(tag);
        }
        else
        {
            map.m_axialTags.push_back(tag);
            if (e.opsClass == "truss") map.m_kbTrussTags.push_back(tag);
        }
        map.m_elements.push_back(e);
    }

    // Ressorts : tags placés au-delà des tags existants (aucune collision possible).
    int auxTag = maxNodeId + 1;
    int springTag = maxElementTag + 1;
    for (const auto& [id, n] : snapshot.nodes())
    {
        const double k[6] = { n.kTx, n.kTy, n.kTz, n.kRx, n.kRy, n.kRz };
        SpringSupportInfo s;
        for (int d = 0; d < 6; ++d)
            if (k[d] > 0.0) { s.dofs.push_back(d); s.stiffness.push_back(k[d]); }
        if (s.dofs.empty()) continue;
        s.nodeId = id;
        s.auxNodeTag = auxTag++;
        s.elementTag = springTag++;
        map.m_auxToTsa[s.auxNodeTag] = id;
        map.m_springs.push_back(s);
    }

    for (const auto& [id, n] : snapshot.nodes())
        if (isFixed(n)) map.m_reactionNodeTags.push_back(id);
    for (const auto& s : map.m_springs)
        map.m_reactionNodeTags.push_back(s.auxNodeTag);

    return map;
}

const OpsElementEntry* OpenSeesModelMap::byTag(int tag) const
{
    auto it = m_indexByTag.find(tag);
    return it != m_indexByTag.end() ? &m_elements[it->second] : nullptr;
}

const OpsElementEntry* OpenSeesModelMap::byKey(const ElementKey& key) const
{
    auto it = m_indexByKey.find(key);
    return it != m_indexByKey.end() ? &m_elements[it->second] : nullptr;
}

int OpenSeesModelMap::tsaNodeOfAux(int auxTag) const
{
    auto it = m_auxToTsa.find(auxTag);
    return it != m_auxToTsa.end() ? it->second : 0;
}

std::vector<std::string> OpenSeesModelMap::validate(const CalculationSnapshot& snapshot) const
{
    std::vector<std::string> errors;
    std::set<int> tags;
    std::set<ElementKey> keys;
    for (const auto& e : m_elements)
    {
        if (e.tag <= 0 || !tags.insert(e.tag).second)
            errors.push_back("Tag OpenSees invalide ou dupliqué : " + std::to_string(e.tag));
        if (!keys.insert(e.key).second)
            errors.push_back("Élément TSA présent deux fois : " + e.key.label());
        if (!snapshot.getNode(e.nodeI) || !snapshot.getNode(e.nodeJ))
            errors.push_back("Élément " + e.key.label() + " : nœud d'extrémité absent du modèle de calcul.");
        else if (!e.axes.valid)
            errors.push_back("Élément " + e.key.label() + " : longueur nulle ou repère local indéfini.");
    }
    std::set<int> nodeTags(m_structuralNodeTags.begin(), m_structuralNodeTags.end());
    for (const auto& s : m_springs)
    {
        if (nodeTags.count(s.auxNodeTag))
            errors.push_back("Collision de tag entre nœud auxiliaire de ressort et nœud TSA : " + std::to_string(s.auxNodeTag));
        if (tags.count(s.elementTag))
            errors.push_back("Collision de tag entre élément ressort et élément structural : " + std::to_string(s.elementTag));
    }
    return errors;
}

} // namespace TSA::Analysis
