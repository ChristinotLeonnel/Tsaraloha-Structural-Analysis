#include "ResultsContext.h"
#include "../Model/Model.h"

#include <QJsonArray>

namespace TSA::Analysis
{

namespace
{
QJsonArray vec(const std::vector<double>& v)
{
    QJsonArray a;
    for (double x : v) a.append(x);
    return a;
}

QJsonObject stationJson(const StationForces& f)
{
    QJsonObject o;
    o["N"] = f.N; o["Vy"] = f.Vy; o["Vz"] = f.Vz;
    o["Mx"] = f.Mx; o["My"] = f.My; o["Mz"] = f.Mz;
    return o;
}

QJsonObject metaJson(const MatrixMetadata& m)
{
    QJsonObject o;
    o["source"] = QString::fromStdString(m.source);
    o["type"] = QString::fromStdString(m.matrixType);
    o["coordinateSystem"] = QString::fromStdString(m.coordinateSystem);
    o["exact"] = m.exact;
    return o;
}

/// Éléments du modèle reliés au nœud (toutes familles calculées).
std::vector<std::pair<ElementKey, std::pair<int, int>>> connectedElements(const TSA::Model::Model& model, int nodeId)
{
    std::vector<std::pair<ElementKey, std::pair<int, int>>> out;
    auto add = [&](StructuralElementKind k, int id, int a, int b) {
        if (a == nodeId || b == nodeId) out.push_back({ ElementKey{ k, id }, { a, b } });
    };
    for (const auto& [id, e] : model.beams()) add(StructuralElementKind::Beam, id, e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : model.columns()) add(StructuralElementKind::Column, id, e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : model.trussMembers()) add(StructuralElementKind::Truss, id, e.startNodeId(), e.endNodeId());
    for (const auto& [id, e] : model.cables()) add(StructuralElementKind::Cable, id, e.startNodeId(), e.endNodeId());
    return out;
}

QJsonObject commonHeader(const ResultsModel& results)
{
    QJsonObject o;
    const auto& u = results.units();
    QJsonObject units;
    units["force"] = QString::fromStdString(u.force);
    units["length"] = QString::fromStdString(u.length);
    units["moment"] = QString::fromStdString(u.moment);
    units["stiffness"] = QString::fromStdString(u.translationalStiffness);
    o["units"] = units;
    o["resultsValid"] = results.isValid();
    o["loadCase"] = QString::fromStdString(results.executionMetadata().loadCombinationType);
    o["extraction"] = results.advanced().available ? "ADVANCED" : "LIGHT";
    o["timestamp"] = QString::fromStdString(results.timestamp());
    return o;
}
} // namespace

QJsonObject ResultsContext::nodeContext(const TSA::Model::Model& model, const ResultsModel& results, int nodeId)
{
    QJsonObject ctx = commonHeader(results);
    ctx["node"] = QString("N%1").arg(nodeId);
    const auto* n = model.getNode(nodeId);
    if (!n)
    {
        ctx["error"] = "Nœud inexistant dans le modèle.";
        return ctx;
    }
    ctx["coordinates"] = QJsonArray{ n->x(), n->y(), n->z() };
    ctx["supported"] = n->support().isSupported();

    if (const auto* d = results.getNodeDisplacement(nodeId))
    {
        QJsonObject o;
        o["UX"] = d->ux; o["UY"] = d->uy; o["UZ"] = d->uz;
        o["RX"] = d->rx; o["RY"] = d->ry; o["RZ"] = d->rz;
        o["translationMagnitude"] = d->translationMagnitude();
        ctx["displacement"] = o;
    }
    if (const auto* r = results.getNodeReaction(nodeId))
    {
        QJsonObject o;
        o["FX"] = r->rx; o["FY"] = r->ry; o["FZ"] = r->rz;
        o["MX"] = r->mx; o["MY"] = r->my; o["MZ"] = r->mz;
        ctx["reaction"] = o;
    }

    const auto& adv = results.advanced();
    QJsonArray elements;
    for (const auto& [key, ends] : connectedElements(model, nodeId))
    {
        QJsonObject e;
        e["element"] = QString::fromStdString(key.label());
        e["kind"] = elementKindName(key.kind);
        const bool atStart = ends.first == nodeId;
        e["nodeEnd"] = atStart ? "i" : "j";
        if (const auto* er = results.getElementResults(key))
        {
            e["forcesAtNode"] = stationJson(atStart ? er->startForces : er->endForces);
            e["length"] = er->length;
        }
        else
        {
            e["note"] = "Pas de résultat (élément non calculé).";
        }
        auto f = adv.elementForces.find(key);
        if (f != adv.elementForces.end())
        {
            e["localForce"] = vec(f->second.local);
            e["globalForce"] = vec(f->second.global);
        }
        auto m = adv.elementMatrices.find(key);
        if (m != adv.elementMatrices.end() && m->second.available)
        {
            // Bloc 6×6 du nœud dans la rigidité globale de l'élément.
            const int off = atStart ? 0 : 6;
            QJsonArray block;
            for (int i = 0; i < 6; ++i)
            {
                QJsonArray row;
                for (int j = 0; j < 6; ++j) row.append(m->second.kGlobal(off + i, off + j));
                block.append(row);
            }
            e["nodalStiffnessBlockGlobal"] = block;
            e["stiffnessMeta"] = metaJson(m->second.kGlobalMeta);
        }
        elements.append(e);
    }
    ctx["connectedElements"] = elements;

    if (adv.available)
    {
        QJsonArray dofs;
        for (int d = 0; d < 6; ++d)
        {
            QJsonObject o;
            o["dof"] = QString::fromStdString(adv.dofMap.dofLabels[d]);
            const int eq = adv.dofMap.equationOf(nodeId, d);
            o["equation"] = eq;
            if (eq < 0)
                o["status"] = "contraint (éliminé du système)";
            else if (adv.hasGlobalStiffness)
                o["K_diagonal"] = adv.kGlobal.at(eq, eq);
            dofs.append(o);
        }
        ctx["dofs"] = dofs;
        if (adv.hasGlobalStiffness) ctx["K_globalMeta"] = metaJson(adv.kGlobalMeta);
    }
    return ctx;
}

QJsonObject ResultsContext::elementContext(const TSA::Model::Model& model, const ResultsModel& results, const ElementKey& key)
{
    (void)model;
    QJsonObject ctx = commonHeader(results);
    ctx["element"] = QString::fromStdString(key.label());
    ctx["kind"] = elementKindName(key.kind);
    const auto* er = results.getElementResults(key);
    if (!er)
    {
        ctx["error"] = "Pas de résultat pour cet élément.";
        return ctx;
    }
    ctx["opsTag"] = er->opsTag;
    ctx["length"] = er->length;
    ctx["start"] = stationJson(er->startForces);
    ctx["end"] = stationJson(er->endForces);
    const auto& adv = results.advanced();
    auto f = adv.elementForces.find(key);
    if (f != adv.elementForces.end())
    {
        ctx["localForce"] = vec(f->second.local);
        ctx["localForceSource"] = QString::fromStdString(f->second.localSource);
        ctx["globalForce"] = vec(f->second.global);
        ctx["basicForce"] = vec(f->second.basic);
    }
    auto m = adv.elementMatrices.find(key);
    if (m != adv.elementMatrices.end())
    {
        ctx["stiffnessAvailable"] = m->second.available;
        if (!m->second.available)
            ctx["stiffnessReason"] = QString::fromStdString(m->second.unavailableReason);
        else
        {
            ctx["kBasic"] = vec(m->second.kBasic.data);
            ctx["kBasicMeta"] = metaJson(m->second.kBasicMeta);
            ctx["kGlobalMeta"] = metaJson(m->second.kGlobalMeta);
        }
    }
    return ctx;
}

} // namespace TSA::Analysis
