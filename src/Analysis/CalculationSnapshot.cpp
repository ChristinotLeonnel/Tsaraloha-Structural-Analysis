#include "CalculationSnapshot.h"
#include <set>
#include <tuple>
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"
#include <gp_Vec.hxx>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace TSA::Analysis
{

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model)
{
    return capture(model, nullptr);
}

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model, const TSA::Model::ElementSet* scope)
{
    return capture(model, scope, SupportRestraintMode::Fork);
}

CalculationSnapshot CalculationSnapshot::capture(const TSA::Model::Model& model, const TSA::Model::ElementSet* scope,
                                                 SupportRestraintMode supportRestraints)
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

        // Blocages anti-singularité des appuis articulés et simples : appliqués après la capture des
        // barres (ils dépendent de leur direction), voir applyAutomaticRestraints.
        sn.pinnedSupport = supp.isPinned();
        sn.rollerSupport = supp.isRoller();

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

    applyAutomaticRestraints(snap, supportRestraints);
    return snap;
}

void CalculationSnapshot::applyAutomaticRestraints(CalculationSnapshot& snap, SupportRestraintMode mode)
{
    snap.m_supportRestraintMode = mode;
    if (mode == SupportRestraintMode::None) return; // rotules sphériques réelles : aucun blocage ajouté
    // Un appui articulé (rotule sphérique) ou simple laisse les trois rotations libres. Dans le solveur 3D,
    // une barre portée par de tels appuis tourne librement autour de son axe (mécanisme de torsion,
    // matrice singulière). Blocages ajoutés, tous tracés (automaticRestraints) et contrôlés après calcul :
    //  1. appui à fourche : rotation autour de l'axe de la barre lorsque toutes les barres fléchies du nœud
    //     sont alignées sur un axe global (barre inclinée : non bloquable par « fix », signalé) ;
    //  2. structure plane verticale (comportement historique) : plan XZ → Rx et, appui simple, Ty ;
    //     plan YZ → Ry et, appui simple, Tx (rotations / translation hors plan) ;
    //  3. structure non plane : appui simple → Ty (comportement historique), Rx n'est plus bloqué : il
    //     encastrait en flexion les barres non parallèles à X.
    // Un ressort défini par l'utilisateur sur le DDL concerné est toujours prioritaire.
    std::set<int> used;
    for (const auto& [tag, e] : snap.m_elements)
    {
        used.insert(e.startNodeId);
        used.insert(e.endNodeId);
    }
    if (used.empty()) return;
    double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
    for (int id : used)
        if (const auto* n = snap.getNode(id))
        {
            const double c[3] = { n->x, n->y, n->z };
            for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], c[k]); hi[k] = std::max(hi[k], c[k]); }
        }
    const double span = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1.0 });
    const double tol = 1e-6 * span;
    const bool planeXZ = hi[1] - lo[1] <= tol;
    const bool planeYZ = !planeXZ && hi[0] - lo[0] <= tol;

    std::map<int, std::vector<gp_Vec>> directions; // barres fléchies par nœud
    for (const auto& [tag, e] : snap.m_elements)
    {
        if (e.type == SnapshotElement::ElementType::Truss || e.type == SnapshotElement::ElementType::Cable) continue;
        const auto* a = snap.getNode(e.startNodeId);
        const auto* b = snap.getNode(e.endNodeId);
        if (!a || !b) continue;
        const gp_Vec d(b->x - a->x, b->y - a->y, b->z - a->z);
        if (d.Magnitude() < 1e-12) continue;
        directions[e.startNodeId].push_back(d.Normalized());
        directions[e.endNodeId].push_back(d.Normalized());
    }

    static const char* kDof[6] = { "TX", "TY", "TZ", "RX", "RY", "RZ" };
    if (mode == SupportRestraintMode::Legacy)
    {
        std::vector<int> bendingLocked; // nœuds dont une barre fléchie n'est pas parallèle à X : RX y encastre la flexion
        for (auto& [id, n] : snap.m_nodes)
        {
            if (!(n.pinnedSupport || n.rollerSupport) || !used.count(id)) continue;
            auto lockLegacy = [&](int dof, bool* f, double k, const QString& why) {
                if (*f || k > 0.0) return;
                *f = true;
                snap.m_automaticRestraints.push_back({ id, dof, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "%1 bloquée au nœud N%2 : %3")
                                                                     .arg(QString::fromLatin1(kDof[dof]), QString::number(id), why).toStdString() });
            };
            lockLegacy(3, &n.fixRx, n.kRx, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "mode historique (RX globale bloquée aux appuis articulés et simples)"));
            if (n.rollerSupport) lockLegacy(1, &n.fixTy, n.kTy, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "mode historique (TY bloquée aux appuis simples)"));
            const auto it = directions.find(id);
            if (it != directions.end()
                && std::any_of(it->second.begin(), it->second.end(), [](const gp_Vec& d) { return std::abs(d.X()) < 1.0 - 1e-9; }))
                bendingLocked.push_back(id);
        }
        if (!bendingLocked.empty())
        {
            std::string list;
            for (std::size_t i = 0; i < bendingLocked.size() && i < 8; ++i) list += (i ? ", N" : "N") + std::to_string(bendingLocked[i]);
            if (bendingLocked.size() > 8) list += "…";
            snap.m_restraintWarnings.push_back(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot",
                "Appuis en mode historique : la rotation RX globale est bloquée aux appuis %1, où une barre n'est pas parallèle "
                "à X. Risque : pour ces barres, ce blocage retient la flexion (moments d'appui fictifs, flèches sous-estimées) et "
                "laisse libre la torsion autour de leur axe (mécanisme possible). Correction : choisir « appui à fourche » ou "
                "« aucun blocage » dans les paramètres d'analyse, ou définir ces appuis explicitement.")
                .arg(QString::fromStdString(list)).toStdString());
        }
        return;
    }
    for (auto& [id, n] : snap.m_nodes)
    {
        if (!(n.pinnedSupport || n.rollerSupport) || !used.count(id)) continue;
        bool* fix[6] = { &n.fixTx, &n.fixTy, &n.fixTz, &n.fixRx, &n.fixRy, &n.fixRz };
        const double spring[6] = { n.kTx, n.kTy, n.kTz, n.kRx, n.kRy, n.kRz };
        auto lock = [&](int dof, const QString& why) {
            if (*fix[dof] || spring[dof] > 0.0) return;
            *fix[dof] = true;
            snap.m_automaticRestraints.push_back({ id, dof, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "%1 bloquée au nœud N%2 : %3")
                                                                 .arg(QString::fromLatin1(kDof[dof]), QString::number(id), why).toStdString() });
        };
        // 1. Appui à fourche
        const auto it = directions.find(id);
        if (it != directions.end())
        {
            const gp_Vec& d0 = it->second.front();
            const bool collinear = std::all_of(it->second.begin(), it->second.end(),
                                               [&](const gp_Vec& d) { return d.Crossed(d0).Magnitude() < 1e-9; });
            if (collinear)
            {
                const double c[3] = { std::abs(d0.X()), std::abs(d0.Y()), std::abs(d0.Z()) };
                const int k = static_cast<int>(std::max_element(c, c + 3) - c);
                if (c[k] > 1.0 - 1e-9)
                    lock(3 + k, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "appui à fourche (rotation autour de l'axe de la barre, sinon libre : mécanisme de torsion)"));
                else if (!planeXZ && !planeYZ)
                    snap.m_restraintNotes.push_back(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "Appui N%1 : la barre est inclinée, la rotation autour "
                                                    "de son axe n'est pas bloquée automatiquement.").arg(id).toStdString());
            }
        }
        // 2. et 3. Structure plane / appui simple
        if (planeXZ)
        {
            lock(3, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "structure plane XZ (rotation hors plan)"));
            if (n.rollerSupport) lock(1, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "structure plane XZ, appui simple (translation hors plan)"));
        }
        else if (planeYZ)
        {
            lock(4, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "structure plane YZ (rotation hors plan)"));
            if (n.rollerSupport) lock(0, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "structure plane YZ, appui simple (translation hors plan)"));
        }
        else if (n.rollerSupport)
        {
            lock(1, QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "appui simple (translation horizontale Y, comportement historique)"));
        }
    }
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

namespace TSA::Analysis
{

std::size_t SubdivisionMap::subElementCount() const
{
    std::size_t n = 0;
    for (const auto& [k, p] : parts) n += p.size();
    return n;
}

CalculationSnapshot CalculationSnapshot::subdivided(const CalculationSnapshot& src, const std::map<ElementKey, std::vector<double>>& interior,
                                                    SubdivisionMap* map, std::vector<std::string>* notes)
{
    CalculationSnapshot out;
    out.m_nodes = src.m_nodes;
    out.m_automaticRestraints = src.m_automaticRestraints;
    out.m_restraintNotes = src.m_restraintNotes;
    out.m_restraintWarnings = src.m_restraintWarnings;
    out.m_supportRestraintMode = src.m_supportRestraintMode;
    out.m_loadCases = src.m_loadCases;
    out.m_combinations = src.m_combinations;
    out.m_nodalLoads = src.m_nodalLoads;
    SubdivisionMap m;
    int nextNode = src.m_nodes.empty() ? 1 : src.m_nodes.rbegin()->first + 1;
    int nextSub = SubdivisionMap::kSubElementIdBase;
    int nextTag = 1;
    int skippedAxial = 0;
    for (const auto& [tag, el] : src.m_elements)
    {
        auto it = interior.find(el.key());
        std::vector<double> s;
        if (it != interior.end())
            for (double v : it->second)
                if (v > 1e-9 && v < 1.0 - 1e-9) s.push_back(v);
        std::sort(s.begin(), s.end());
        s.erase(std::unique(s.begin(), s.end(), [](double a, double b) { return std::abs(a - b) < 1e-9; }), s.end());
        const bool flexural = el.type == StructuralElementKind::Beam || el.type == StructuralElementKind::Column;
        if (!s.empty() && !flexural)
        {
            ++skippedAxial;
            s.clear();
        }
        const SnapshotNode* a = src.getNode(el.startNodeId);
        const SnapshotNode* b = src.getNode(el.endNodeId);
        if (s.empty() || !a || !b)
        {
            SnapshotElement e = el;
            e.tag = nextTag++;
            out.m_tagByKey[e.key()] = e.tag;
            out.m_elements[e.tag] = e;
            continue;
        }
        std::vector<int> chain { el.startNodeId };
        for (double v : s)
        {
            SnapshotNode n;
            n.id = nextNode++;
            n.x = a->x + (b->x - a->x) * v;
            n.y = a->y + (b->y - a->y) * v;
            n.z = a->z + (b->z - a->z) * v;
            out.m_nodes[n.id] = n;
            m.internalNodes.insert(n.id);
            chain.push_back(n.id);
        }
        chain.push_back(el.endNodeId);
        std::vector<double> rel { 0.0 };
        rel.insert(rel.end(), s.begin(), s.end());
        rel.push_back(1.0);
        auto& parts = m.parts[el.key()];
        for (std::size_t k = 0; k + 1 < chain.size(); ++k)
        {
            SnapshotElement e = el;
            e.id = nextSub++;
            e.tag = nextTag++;
            e.startNodeId = chain[k];
            e.endNodeId = chain[k + 1];
            e.length = el.length * (rel[k + 1] - rel[k]);
            e.startRelease = k == 0 ? el.startRelease : TSA::Model::EndRelease {};
            e.endRelease = k + 2 == chain.size() ? el.endRelease : TSA::Model::EndRelease {};
            out.m_tagByKey[e.key()] = e.tag;
            out.m_elements[e.tag] = e;
            parts.push_back({ e.key(), el.length * rel[k], el.length * rel[k + 1] });
        }
    }
    // Charges sur barres : portions exactes sur chaque sous-élément.
    for (const auto& load : src.m_memberLoads)
    {
        const SnapshotElement* el = src.findElementForLoad(load);
        auto pit = el ? m.parts.find(el->key()) : m.parts.end();
        if (pit == m.parts.end())
        {
            out.m_memberLoads.push_back(load);
            continue;
        }
        const double L = el->length;
        const auto range = load.appliedRange(L);
        const double a = range.first, b = range.second;
        if (load.type() == TSA::Model::LoadType::MemberPoint)
        {
            const double x = std::clamp(a, 0.0, L);
            for (std::size_t k = 0; k < pit->second.size(); ++k)
            {
                const auto& part = pit->second[k];
                const bool last = k + 1 == pit->second.size();
                if (x >= part.x0 - 1e-12 && (x < part.x1 - 1e-12 || last))
                {
                    TSA::Model::MemberLoad l = load;
                    l.setElementId(part.key.id);
                    l.setRelativePosition(false);
                    l.setX1(std::clamp(x - part.x0, 0.0, part.x1 - part.x0));
                    l.setX2(0.0);
                    out.m_memberLoads.push_back(l);
                    break;
                }
            }
            continue;
        }
        for (const auto& part : pit->second)
        {
            const double lo = std::max(a, part.x0), hi = std::min(b, part.x1);
            if (hi - lo <= 1e-12) continue;
            TSA::Model::MemberLoad l = load;
            l.setElementId(part.key.id);
            l.setRelativePosition(false);
            const double q1 = load.intensityAt(lo, L), q2 = load.intensityAt(hi, L);
            const double len = part.x1 - part.x0;
            const bool full = lo - part.x0 < 1e-9 && part.x1 - hi < 1e-9;
            if (full && std::abs(q1 - q2) < 1e-12 && load.type() != TSA::Model::LoadType::MemberMoment)
            {
                l.setType(TSA::Model::LoadType::MemberUniform);
                l.setQ1(q1);
                l.setQ2(q1);
                l.setX1(0.0);
                l.setX2(0.0);
            }
            else
            {
                if (load.type() != TSA::Model::LoadType::MemberMoment) l.setType(TSA::Model::LoadType::MemberLinear);
                l.setQ1(q1);
                l.setQ2(q2);
                l.setX1(lo - part.x0);
                l.setX2(std::min(hi - part.x0, len));
            }
            out.m_memberLoads.push_back(l);
        }
    }
    if (skippedAxial && notes)
        notes->push_back(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "%1 barre(s) de treillis ou câble(s) non subdivisée(s) : une chaîne de barres articulées "
                                                         "serait un mécanisme (un seul élément transmis)")
                             .arg(skippedAxial).toStdString());
    if (map) *map = m;
    return out;
}

CalculationSnapshot CalculationSnapshot::withSurfaces(const CalculationSnapshot& src, const SurfaceAnalysisModel& surfaces, SurfaceNodeMap* map,
                                                      std::vector<std::string>* notes, std::string* error)
{
    CalculationSnapshot out = src;
    SurfaceNodeMap m;
    auto fail = [&](const std::string& e) {
        if (error) *error = e;
        return src;
    };
    for (const auto& e : surfaces.elements)
        if (e.formulation != TSA::Model::SurfaceFormulation::Shell)
            return fail(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "Surface %1 : formulation « %2 » non prise en charge par ce moteur (seule la coque l'est).")
                            .arg(QString::fromStdString(e.source), QString::fromStdString(std::string(TSA::Model::surfaceFormulationName(e.formulation))))
                            .toStdString());

    // Recherche par position : grille de pas 1 mm sur les nœuds du snapshot (TSA et subdivisions des barres)
    auto key = [](double x, double y, double z) { return std::make_tuple(std::llround(x * 1000.0), std::llround(y * 1000.0), std::llround(z * 1000.0)); };
    std::map<std::tuple<long long, long long, long long>, std::vector<int>> grid;
    for (const auto& [id, n] : out.m_nodes) grid[key(n.x, n.y, n.z)].push_back(id);
    auto findAt = [&](double x, double y, double z) {
        const auto k = key(x, y, z);
        for (long long dx = -1; dx <= 1; ++dx)
            for (long long dy = -1; dy <= 1; ++dy)
                for (long long dz = -1; dz <= 1; ++dz)
                {
                    auto it = grid.find({ std::get<0>(k) + dx, std::get<1>(k) + dy, std::get<2>(k) + dz });
                    if (it == grid.end()) continue;
                    for (int id : it->second)
                    {
                        const auto& n = out.m_nodes.at(id);
                        if (std::abs(n.x - x) <= 1e-6 && std::abs(n.y - y) <= 1e-6 && std::abs(n.z - z) <= 1e-6) return id;
                    }
                }
        return 0;
    };

    std::set<int> used;
    for (const auto& e : surfaces.elements) used.insert(e.meshNodes.begin(), e.meshNodes.end());
    int nextNode = out.m_nodes.empty() ? 1 : out.m_nodes.rbegin()->first + 1;
    std::size_t merged = 0, created = 0;
    for (const auto& n : surfaces.nodes)
    {
        if (!used.count(n.meshNode)) continue;
        int id = 0;
        if (n.tsaNode > 0 && out.m_nodes.count(n.tsaNode)) id = n.tsaNode;
        if (!id) id = findAt(n.x, n.y, n.z);
        if (id)
            ++merged;
        else
        {
            SnapshotNode sn;
            sn.id = nextNode++;
            sn.x = n.x;
            sn.y = n.y;
            sn.z = n.z;
            out.m_nodes[sn.id] = sn;
            grid[key(n.x, n.y, n.z)].push_back(sn.id);
            m.surfaceOnlyNodes.insert(sn.id);
            id = sn.id;
            ++created;
        }
        m.meshToNode[n.meshNode] = id;
    }

    int nextTag = out.m_elements.empty() ? 1 : out.m_elements.rbegin()->first + 1;
    for (const auto& e : surfaces.elements)
    {
        if (e.meshNodes.size() != 3 && e.meshNodes.size() != 4) return fail(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "Élément %1 de %2 : 3 ou 4 nœuds attendus.")
                                                                             .arg(QString::number(e.meshElement), QString::fromStdString(e.source)).toStdString());
        SnapshotShell sh;
        sh.tag = nextTag++;
        sh.meshElement = e.meshElement;
        sh.source = e.source;
        sh.thickness = e.thickness;
        sh.material = e.material;
        for (int mn : e.meshNodes)
        {
            auto it = m.meshToNode.find(mn);
            if (it == m.meshToNode.end()) return fail(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "Élément %1 de %2 : nœud du maillage %3 inconnu.")
                                                          .arg(QString::number(e.meshElement), QString::fromStdString(e.source), QString::number(mn))
                                                          .toStdString());
            sh.nodes.push_back(it->second);
        }
        if (std::set<int>(sh.nodes.begin(), sh.nodes.end()).size() != sh.nodes.size())
            return fail(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "Élément %1 de %2 : nœuds confondus (élément dégénéré).")
                            .arg(QString::number(e.meshElement), QString::fromStdString(e.source)).toStdString());
        out.m_shells.push_back(sh);
    }
    auto addForces = [&](const std::vector<SurfaceNodalForce>& list) {
        for (const auto& f : list)
        {
            auto it = m.meshToNode.find(f.meshNode);
            if (it == m.meshToNode.end()) continue;
            out.m_surfaceForces.push_back({ it->second, f.loadCaseId, f.fx, f.fy, f.fz, f.label });
        }
    };
    addForces(surfaces.loads);
    addForces(surfaces.selfWeight);
    // Appuis linéiques de bord : DDL bloqués (ou) et raideurs (somme) ajoutés aux nœuds ; un DDL bloqué l'emporte sur une raideur.
    for (const auto& s : surfaces.supports)
    {
        auto it = m.meshToNode.find(s.meshNode);
        if (it == m.meshToNode.end()) continue;
        auto& n = out.m_nodes.at(it->second);
        bool* fix[6] = { &n.fixTx, &n.fixTy, &n.fixTz, &n.fixRx, &n.fixRy, &n.fixRz };
        double* k[6] = { &n.kTx, &n.kTy, &n.kTz, &n.kRx, &n.kRy, &n.kRz };
        for (int d = 0; d < 6; ++d)
        {
            if (s.fixed[d]) *fix[d] = n.definedFix[d] = true;
            else if (s.stiffness[d] > 0.0) *k[d] += s.stiffness[d];
        }
        for (int d = 0; d < 6; ++d)
            if (*fix[d]) *k[d] = 0.0;
        if (n.supportType == TSA::Model::SupportType::Free) n.supportType = TSA::Model::SupportType::Custom;
    }
    if (notes)
        notes->push_back(QCoreApplication::translate("TSA::Analysis::CalculationSnapshot", "%1 élément(s) de coque, %2 nœud(s) partagé(s) avec les barres ou les nœuds TSA, "
                                                     "%3 nœud(s) propres aux surfaces.")
                             .arg(QString::number(out.m_shells.size()), QString::number(merged), QString::number(created)).toStdString());
    if (map) *map = m;
    return out;
}

} // namespace TSA::Analysis
