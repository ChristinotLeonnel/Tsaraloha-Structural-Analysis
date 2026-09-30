#include "CalculationSnapshot.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"
#include <cmath>

namespace TSA::Analysis
{

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model)
{
    CalculationSnapshot snap;

    // 1. Capture des Nœuds
    for (const auto& [id, n] : model.nodes())
    {
        SnapshotNode sn;
        sn.id = id;
        sn.name = n.name();
        sn.x = n.x();
        sn.y = n.y();
        sn.z = n.z();
        sn.supportType = n.supportType();

        switch (n.supportType())
        {
        case TSA::Model::SupportType::Fixed:
            sn.fixTx = sn.fixTy = sn.fixTz = sn.fixRx = sn.fixRy = sn.fixRz = true;
            break;
        case TSA::Model::SupportType::Pinned:
            sn.fixTx = sn.fixTy = sn.fixTz = true;
            sn.fixRx = true; // Bloque la rotation autour de l'axe de la barre pour éviter le mécanisme de corps rigide en 3D
            sn.fixRy = sn.fixRz = false;
            break;
        case TSA::Model::SupportType::Roller:
            sn.fixTy = sn.fixTz = true; // Bloque le déplacement vertical et transversal hors-plan
            sn.fixRx = true;
            sn.fixTx = sn.fixRy = sn.fixRz = false; // Libre en translation axiale X
            break;
        case TSA::Model::SupportType::Free:
        default:
            sn.fixTx = sn.fixTy = sn.fixTz = sn.fixRx = sn.fixRy = sn.fixRz = false;
            break;
        }

        snap.m_nodes[id] = sn;
    }

    auto computeLength = [&](int n1, int n2) -> double {
        auto it1 = snap.m_nodes.find(n1);
        auto it2 = snap.m_nodes.find(n2);
        if (it1 == snap.m_nodes.end() || it2 == snap.m_nodes.end()) return 0.0;
        double dx = it2->second.x - it1->second.x;
        double dy = it2->second.y - it1->second.y;
        double dz = it2->second.z - it1->second.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    // 2. Capture des Poutres (Beams)
    for (const auto& [id, b] : model.beams())
    {
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Beam;
        elem.startNodeId = b.startNodeId();
        elem.endNodeId = b.endNodeId();
        elem.rotation = b.rotation();
        elem.section = b.section();
        elem.material = b.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        snap.m_elements[id] = elem;
    }

    // 3. Capture des Poteaux (Columns)
    for (const auto& [id, col] : model.columns())
    {
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Column;
        elem.startNodeId = col.startNodeId();
        elem.endNodeId = col.endNodeId();
        elem.rotation = col.rotation();
        elem.section = col.section();
        elem.material = col.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        snap.m_elements[id] = elem;
    }

    // 4. Capture des Bielles / Treillis (TrussMembers)
    for (const auto& [id, tr] : model.trussMembers())
    {
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Truss;
        elem.startNodeId = tr.startNodeId();
        elem.endNodeId = tr.endNodeId();
        elem.rotation = 0.0;
        elem.section = tr.section();
        elem.material = tr.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        snap.m_elements[id] = elem;
    }

    // 5. Capture des Câbles
    for (const auto& [id, cb] : model.cables())
    {
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Cable;
        elem.startNodeId = cb.startNodeId();
        elem.endNodeId = cb.endNodeId();
        elem.rotation = 0.0;
        elem.section = cb.section();
        elem.material = cb.material();
        elem.initialTension = cb.initialTension();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        snap.m_elements[id] = elem;
    }

    // 6. Capture des Charges & Cas de Charges
    const auto& lm = model.loadManager();
    for (const auto& [_, nl] : lm.nodalLoads())
    {
        snap.m_nodalLoads.push_back(nl);
    }
    for (const auto& [_, ml] : lm.memberLoads())
    {
        snap.m_memberLoads.push_back(ml);
    }
    snap.m_loadCases = lm.loadCases();
    snap.m_combinations = lm.combinations();

    return snap;
}

const SnapshotNode* CalculationSnapshot::getNode(int id) const
{
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

const SnapshotElement* CalculationSnapshot::getElement(int id) const
{
    auto it = m_elements.find(id);
    return it != m_elements.end() ? &it->second : nullptr;
}

} // namespace TSA::Analysis
