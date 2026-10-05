#include "CalculationSnapshot.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"
#include <cmath>

namespace TSA::Analysis
{

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model)
{
    return capture(model, nullptr);
}

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model, const TSA::Model::ElementSet* scope)
{
    CalculationSnapshot snap;

    // Portée : barres retenues, puis nœuds qu'elles relient (un nœud isolé n'est pas calculable).
    std::set<int> scopedNodes;
    if (scope)
    {
        auto collect = [&](const auto& items, const std::set<int>& ids) {
            for (const auto& [id, e] : items)
                if (ids.count(id)) { scopedNodes.insert(e.startNodeId()); scopedNodes.insert(e.endNodeId()); }
        };
        collect(model.beams(), scope->beams);
        collect(model.columns(), scope->columns);
        collect(model.trussMembers(), scope->trussMembers);
        collect(model.cables(), scope->cables);
    }

    // 1. Capture des Nœuds
    for (const auto& [id, n] : model.nodes())
    {
        if (scope && !scopedNodes.count(id)) continue;
        SnapshotNode sn;
        sn.id = id;
        sn.name = n.name();
        sn.x = n.x();
        sn.y = n.y();
        sn.z = n.z();
        const auto& supp = n.support();
        // Utiliser supportType() du SupportDefinition (non tronqué) au lieu du legacy
        sn.supportType = supp.supportType();

        sn.fixTx = (supp.tx() == TSA::Model::DOFState::Fixed);
        sn.fixTy = (supp.ty() == TSA::Model::DOFState::Fixed);
        sn.fixTz = (supp.tz() == TSA::Model::DOFState::Fixed);
        sn.fixRx = (supp.rx() == TSA::Model::DOFState::Fixed);
        sn.fixRy = (supp.ry() == TSA::Model::DOFState::Fixed);
        sn.fixRz = (supp.rz() == TSA::Model::DOFState::Fixed);
        sn.definedFix = { sn.fixTx, sn.fixTy, sn.fixTz, sn.fixRx, sn.fixRy, sn.fixRz };

        // Anti-singularité 3D : pour les appuis articulés (pinned) et rouleaux (roller),
        // la rotation de forage (drill, Rx) est libre par définition mais crée une
        // matrice de rigidité singulière dans un solveur 3D. On la bloque sauf si
        // l'utilisateur a explicitement défini un ressort en rotation sur cet axe.
        if ((supp.isPinned() || supp.isRoller()) && supp.rx() != TSA::Model::DOFState::Spring)
        {
            sn.fixRx = true;
        }
        // Pour un appui simple (roller), le déplacement transversal hors-plan Ty est bloqué
        // pour empêcher le mécanisme de corps rigide en rotation horizontale dans l'espace 3D.
        if (supp.isRoller() && supp.ty() != TSA::Model::DOFState::Spring)
        {
            sn.fixTy = true;
        }

        // Raideurs élastiques (ressorts)
        if (supp.tx() == TSA::Model::DOFState::Spring) sn.kTx = supp.kx();
        if (supp.ty() == TSA::Model::DOFState::Spring) sn.kTy = supp.ky();
        if (supp.tz() == TSA::Model::DOFState::Spring) sn.kTz = supp.kz();
        if (supp.rx() == TSA::Model::DOFState::Spring) sn.kRx = supp.krx();
        if (supp.ry() == TSA::Model::DOFState::Spring) sn.kRy = supp.kry();
        if (supp.rz() == TSA::Model::DOFState::Spring) sn.kRz = supp.krz();

        snap.m_nodes[id] = sn;
    }

    // Tags OpenSees uniques : les ids TSA de familles différentes peuvent coïncider
    // (poutre 1 et poteau 1) et ne doivent jamais s'écraser.
    int nextTag = 1;
    auto registerElement = [&](SnapshotElement& elem) {
        elem.tag = nextTag++;
        snap.m_tagByKey[elem.key()] = elem.tag;
        snap.m_elements[elem.tag] = elem;
    };

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
        if (scope && !scope->beams.count(id)) continue;
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Beam;
        elem.startNodeId = b.startNodeId();
        elem.endNodeId = b.endNodeId();
        elem.rotation = b.rotation();
        elem.startRelease = b.startRelease();
        elem.endRelease = b.endRelease();
        elem.section = b.section();
        elem.material = b.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        registerElement(elem);
    }

    // 3. Capture des Poteaux (Columns)
    for (const auto& [id, col] : model.columns())
    {
        if (scope && !scope->columns.count(id)) continue;
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Column;
        elem.startNodeId = col.startNodeId();
        elem.endNodeId = col.endNodeId();
        elem.rotation = col.rotation();
        elem.section = col.section();
        elem.material = col.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        registerElement(elem);
    }

    // 4. Capture des Bielles / Treillis (TrussMembers)
    for (const auto& [id, tr] : model.trussMembers())
    {
        if (scope && !scope->trussMembers.count(id)) continue;
        SnapshotElement elem;
        elem.id = id;
        elem.type = SnapshotElement::ElementType::Truss;
        elem.startNodeId = tr.startNodeId();
        elem.endNodeId = tr.endNodeId();
        elem.rotation = 0.0;
        elem.section = tr.section();
        elem.material = tr.material();
        elem.length = computeLength(elem.startNodeId, elem.endNodeId);
        registerElement(elem);
    }

    // 5. Capture des Câbles
    for (const auto& [id, cb] : model.cables())
    {
        if (scope && !scope->cables.count(id)) continue;
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
        registerElement(elem);
    }

    // 6. Capture des Charges & Cas de Charges
    const auto& lm = model.loadManager();
    for (const auto& [_, nl] : lm.nodalLoads())
    {
        if (scope && !snap.hasNode(nl.nodeId())) continue;
        snap.m_nodalLoads.push_back(nl);
    }
    for (const auto& [_, ml] : lm.memberLoads())
    {
        if (scope && !snap.findElementForLoad(ml)) continue;
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

const SnapshotElement* CalculationSnapshot::getElementByTag(int tag) const
{
    auto it = m_elements.find(tag);
    return it != m_elements.end() ? &it->second : nullptr;
}

const SnapshotElement* CalculationSnapshot::findElement(StructuralElementKind kind, int id) const
{
    auto it = m_tagByKey.find(ElementKey{ kind, id });
    return it != m_tagByKey.end() ? getElementByTag(it->second) : nullptr;
}

StructuralElementKind CalculationSnapshot::kindOf(TSA::Model::MemberTargetType t)
{
    switch (t)
    {
    case TSA::Model::MemberTargetType::Column: return StructuralElementKind::Column;
    case TSA::Model::MemberTargetType::Truss: return StructuralElementKind::Truss;
    case TSA::Model::MemberTargetType::Cable: return StructuralElementKind::Cable;
    case TSA::Model::MemberTargetType::Beam:
    default: return StructuralElementKind::Beam;
    }
}

const SnapshotElement* CalculationSnapshot::findElementForLoad(const TSA::Model::MemberLoad& load) const
{
    if (const auto* e = findElement(kindOf(load.targetType()), load.elementId()))
        return e;
    if (load.targetType() != TSA::Model::MemberTargetType::Beam)
        return nullptr;

    // Compatibilité : avant le 2026-10-04, MemberLoadDialog n'enregistrait pas la famille cible
    // (toujours « Beam »). Une telle charge, sans poutre de cet id, est rattachée à l'unique
    // élément d'une autre famille portant cet id ; s'il y en a plusieurs, elle est ignorée
    // (ambiguïté signalée par ModelValidator::validateForAnalysis).
    const SnapshotElement* found = nullptr;
    int matches = 0;
    for (auto kind : { StructuralElementKind::Column, StructuralElementKind::Truss, StructuralElementKind::Cable })
    {
        if (const auto* e = findElement(kind, load.elementId()))
        {
            found = e;
            ++matches;
        }
    }
    return matches == 1 ? found : nullptr;
}

} // namespace TSA::Analysis
