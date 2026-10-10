#include "ReportDataBuilder.h"

#include "../Analysis/ResultsModel.h"
#include "../Coordinate/CoordinateSystem.h"
#include "../Coordinate/LevelManager.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"
#include "App/ProductInfo.h"

#include <QDateTime>
#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace TSA::Reports
{

using namespace TSA::Model;
using TSA::Analysis::StructuralElementKind;

namespace
{
QString f(double v, int decimals) { return QString::number(v, 'f', decimals); }
QString s(const std::string& v) { return QString::fromStdString(v); }

QString supportLabel(const SupportDefinition& sd, bool* supported)
{
    const DOFState d[] = { sd.tx(), sd.ty(), sd.tz(), sd.rx(), sd.ry(), sd.rz() };
    int fixed = 0, spring = 0;
    for (auto x : d)
    {
        fixed += x == DOFState::Fixed;
        spring += x == DOFState::Spring;
    }
    *supported = fixed + spring > 0;
    if (spring) return QStringLiteral("élastique");
    if (fixed == 6) return QStringLiteral("encastrement");
    if (fixed == 3 && d[0] == DOFState::Fixed && d[1] == DOFState::Fixed && d[2] == DOFState::Fixed) return QStringLiteral("articulation");
    if (fixed == 0) return QStringLiteral("libre");
    static const char* n[] = { "Tx", "Ty", "Tz", "Rx", "Ry", "Rz" };
    QStringList blocked;
    for (int i = 0; i < 6; ++i)
        if (d[i] == DOFState::Fixed) blocked << QLatin1String(n[i]);
    return QStringLiteral("bloqué : %1").arg(blocked.join(' '));
}

QString caseCategoryLabel(int i)
{
    static const char* l[] = { "permanente", "exploitation", "vent", "neige", "séisme", "température", "accidentelle", "autre" };
    return i >= 0 && i < 8 ? QString::fromUtf8(l[i]) : QStringLiteral("autre");
}

QString combinationLabel(int i)
{
    static const char* l[] = { "ELU fondamental", "ELU accidentel", "ELU sismique", "ELS caractéristique", "ELS fréquent", "ELS quasi-permanent", "personnalisée" };
    return i >= 0 && i < 7 ? QString::fromUtf8(l[i]) : QStringLiteral("personnalisée");
}

QString familyLabel(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return QStringLiteral("poutre");
    case StructuralElementKind::Column: return QStringLiteral("poteau");
    case StructuralElementKind::Truss: return QStringLiteral("barre de treillis");
    case StructuralElementKind::Cable: return QStringLiteral("câble");
    }
    return {};
}
const char* familyKey(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return "beam";
    case StructuralElementKind::Column: return "column";
    case StructuralElementKind::Truss: return "truss";
    case StructuralElementKind::Cable: return "cable";
    }
    return "";
}
QString refPrefix(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return QStringLiteral("B");
    case StructuralElementKind::Column: return QStringLiteral("C");
    case StructuralElementKind::Truss: return QStringLiteral("T");
    case StructuralElementKind::Cable: return QStringLiteral("K");
    }
    return {};
}

QJsonObject unavailable(const QString& reason) { return { { "available", false }, { "reason", reason } }; }

/// Extrema d'une barre sur toutes les stations calculées.
QJsonObject memberResults(const TSA::Analysis::ElementResults& er, const TSA::Analysis::UnitSystem& u)
{
    std::vector<const TSA::Analysis::StationForces*> st { &er.startForces };
    for (const auto& x : er.intermediateStations) st.push_back(&x);
    st.push_back(&er.endForces);
    double nMax = -std::numeric_limits<double>::infinity(), nMin = std::numeric_limits<double>::infinity();
    double vy = 0, vz = 0, t = 0, my = 0, mz = 0, dy = 0, dz = 0;
    for (const auto* p : st)
    {
        nMax = std::max(nMax, p->N);
        nMin = std::min(nMin, p->N);
        vy = std::max(vy, std::abs(p->Vy));
        vz = std::max(vz, std::abs(p->Vz));
        t = std::max(t, std::abs(p->Mx));
        my = std::max(my, std::abs(p->My));
        mz = std::max(mz, std::abs(p->Mz));
        dy = std::max(dy, std::abs(p->uy));
        dz = std::max(dz, std::abs(p->uz));
    }
    const QString F = s(u.force), M = s(u.moment);
    return { { "available", true },
             { "stations", int(st.size()) },
             { "Nmax", f(nMax, 2) + ' ' + F },
             { "Nmin", f(nMin, 2) + ' ' + F },
             { "Vy", f(vy, 2) + ' ' + F },
             { "Vz", f(vz, 2) + ' ' + F },
             { "T", f(t, 2) + ' ' + M },
             { "My", f(my, 2) + ' ' + M },
             { "Mz", f(mz, 2) + ' ' + M },
             { "deflection", st.size() > 2 ? QJsonValue(u.length == "m" ? f(std::max(dy, dz) * 1000.0, 2) + QStringLiteral(" mm") : f(std::max(dy, dz), 4) + ' ' + s(u.length)) : QJsonValue(QStringLiteral("non calculée (aucune station intermédiaire)")) } };
}
} // namespace

QJsonObject ReportDataBuilder::build(const TSA::Model::Model& model, const TSA::Analysis::ResultsModel* results, const ReportProjectInfo& p)
{
    const bool haveResults = results && results->isValid() && results->hasResults();

    // --- Nœuds, géométrie -------------------------------------------------------------------------------
    QJsonArray nodes, supports;
    double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
    for (const auto& [id, n] : model.nodes())
    {
        const double c[3] = { n.x(), n.y(), n.z() };
        for (int i = 0; i < 3; ++i)
        {
            mn[i] = std::min(mn[i], c[i]);
            mx[i] = std::max(mx[i], c[i]);
        }
        bool supported = false;
        const QString label = supportLabel(n.support(), &supported);
        const QJsonObject o { { "id", QStringLiteral("N%1").arg(id) }, { "name", s(n.name()) }, { "x", f(n.x(), 3) }, { "y", f(n.y(), 3) },
                              { "z", f(n.z(), 3) }, { "support", label }, { "supported", supported } };
        nodes.append(o);
        if (supported) supports.append(o);
    }
    const bool any = !model.nodes().empty();
    const double ext[3] = { any ? mx[0] - mn[0] : 0.0, any ? mx[1] - mn[1] : 0.0, any ? mx[2] - mn[2] : 0.0 };
    const double tol = 1e-6 * std::max({ 1.0, ext[0], ext[1], ext[2] });
    QString plane;
    if (any && ext[1] <= tol) plane = QStringLiteral("XZ");
    else if (any && ext[0] <= tol) plane = QStringLiteral("YZ");
    else if (any && ext[2] <= tol) plane = QStringLiteral("XY");

    // --- Barres -------------------------------------------------------------------------------------------
    QJsonArray members, beams, columns;
    std::map<QString, QJsonObject> materials, sections;
    auto length = [&](int a, int b) {
        const auto* na = model.getNode(a);
        const auto* nb = model.getNode(b);
        return na && nb ? std::sqrt(std::pow(nb->x() - na->x(), 2) + std::pow(nb->y() - na->y(), 2) + std::pow(nb->z() - na->z(), 2)) : 0.0;
    };
    auto addMember = [&](StructuralElementKind kind, int id, int a, int b, const std::string& name, const Section& sec, const Material& mat) {
        const QString secName = s(sec.name), matName = s(mat.name);
        sections.emplace(secName, QJsonObject { { "name", secName },
                                                { "A", f(sec.area() * 1e4, 2) + QStringLiteral(" cm²") },
                                                { "Iy", f(sec.iy() * 1e8, 1) + QStringLiteral(" cm⁴") },
                                                { "Iz", f(sec.iz() * 1e8, 1) + QStringLiteral(" cm⁴") } });
        materials.emplace(matName, QJsonObject { { "name", matName },
                                                 { "E", f(mat.mechanical.youngModulus / 1e9, 1) + QStringLiteral(" GPa") },
                                                 { "nu", f(mat.mechanical.poissonRatio, 2) },
                                                 { "density", f(mat.mechanical.density, 0) + QStringLiteral(" kg/m³") },
                                                 { "fk", f(mat.mechanical.yieldStrength / 1e6, 1) + QStringLiteral(" MPa") } });
        QJsonObject o { { "ref", refPrefix(kind) + QString::number(id) },
                        { "family", QLatin1String(familyKey(kind)) },
                        { "familyLabel", familyLabel(kind) },
                        { "name", s(name) },
                        { "nodeI", QStringLiteral("N%1").arg(a) },
                        { "nodeJ", QStringLiteral("N%1").arg(b) },
                        { "length", f(length(a, b), 3) + QStringLiteral(" m") },
                        { "section", secName },
                        { "material", matName } };
        const auto* er = haveResults ? results->getElementResults(kind, id) : nullptr;
        o.insert("results", er ? memberResults(*er, results->units())
                               : unavailable(haveResults ? QStringLiteral("élément non transmis au moteur ou sans efforts") : QStringLiteral("aucun résultat de calcul")));
        members.append(o);
        if (kind == StructuralElementKind::Beam) beams.append(o);
        if (kind == StructuralElementKind::Column) columns.append(o);
    };
    for (const auto& [id, b] : model.beams()) addMember(StructuralElementKind::Beam, id, b.startNodeId(), b.endNodeId(), b.name(), b.section(), b.material());
    for (const auto& [id, c] : model.columns()) addMember(StructuralElementKind::Column, id, c.startNodeId(), c.endNodeId(), c.name(), c.section(), c.material());
    for (const auto& [id, t] : model.trussMembers()) addMember(StructuralElementKind::Truss, id, t.startNodeId(), t.endNodeId(), t.name(), t.section(), t.material());
    for (const auto& [id, k] : model.cables()) addMember(StructuralElementKind::Cable, id, k.startNodeId(), k.endNodeId(), k.name(), k.section(), k.material());
    QJsonArray matArr, secArr;
    for (const auto& [k, v] : materials) matArr.append(v);
    for (const auto& [k, v] : sections) secArr.append(v);

    // --- Niveaux, charges ---------------------------------------------------------------------------------
    QJsonArray levels;
    if (model.coordinateSystem() && model.coordinateSystem()->levelManager())
        for (const auto& l : model.coordinateSystem()->levelManager()->levels())
            levels.append(QJsonObject { { "name", s(l.name) }, { "elevation", f(l.elevation, 3) + QStringLiteral(" m") } });
    const LoadManager::LoadSnapshot ls = model.loadManager().createSnapshot();
    QJsonArray cases, combos, nodalLoads, memberLoads;
    std::map<int, QString> caseName;
    for (const auto& [id, lc] : ls.loadCases)
    {
        caseName[id] = s(lc.name());
        cases.append(QJsonObject { { "id", QStringLiteral("LC%1").arg(id) }, { "name", s(lc.name()) },
                                   { "category", caseCategoryLabel(static_cast<int>(lc.category())) }, { "selfWeight", lc.isSelfWeightIncluded() } });
    }
    for (const auto& [id, co] : ls.combinations)
    {
        QStringList terms;
        for (const auto& [caseId, factor] : co.caseFactors()) terms << QStringLiteral("%1 × %2").arg(f(factor, 2), caseName.count(caseId) ? caseName[caseId] : QStringLiteral("LC%1").arg(caseId));
        combos.append(QJsonObject { { "name", s(co.name()) }, { "type", combinationLabel(static_cast<int>(co.type())) }, { "expression", terms.join(QStringLiteral(" + ")) } });
    }
    for (const auto& [id, l] : ls.nodalLoads)
        nodalLoads.append(QJsonObject { { "node", QStringLiteral("N%1").arg(l.nodeId()) }, { "case", caseName[l.loadCaseId()] },
                                        { "F", QStringLiteral("(%1 ; %2 ; %3) kN").arg(f(l.fx(), 2), f(l.fy(), 2), f(l.fz(), 2)) },
                                        { "M", QStringLiteral("(%1 ; %2 ; %3) kN·m").arg(f(l.mx(), 2), f(l.my(), 2), f(l.mz(), 2)) } });
    for (const auto& [id, l] : ls.memberLoads)
    {
        static const QString prefix[] = { "B", "C", "T", "K" };
        QString kind = QStringLiteral("répartie uniforme");
        QString value = f(l.q1(), 2) + QStringLiteral(" kN/m");
        if (l.type() == LoadType::MemberLinear)
            kind = QStringLiteral("trapézoïdale"), value = QStringLiteral("%1 → %2 kN/m").arg(f(l.q1(), 2), f(l.q2(), 2));
        else if (l.type() == LoadType::MemberPoint)
            kind = QStringLiteral("ponctuelle"), value = f(l.q1(), 2) + QStringLiteral(" kN");
        else if (l.type() == LoadType::MemberMoment)
            kind = QStringLiteral("moment"), value = f(l.q1(), 2) + QStringLiteral(" kN·m");
        memberLoads.append(QJsonObject { { "member", prefix[static_cast<int>(l.targetType())] + QString::number(l.elementId()) },
                                         { "case", caseName[l.loadCaseId()] }, { "kind", kind }, { "value", value } });
    }

    // --- Résultats ----------------------------------------------------------------------------------------
    QJsonObject res;
    QJsonObject mesh = unavailable(QStringLiteral("aucun maillage de calcul : lancer un calcul"));
    if (!haveResults)
        res = unavailable(QStringLiteral("aucun résultat de calcul valide pour le modèle actuel"));
    else
    {
        const auto& u = results->units();
        const auto& meta = results->executionMetadata();
        res = QJsonObject { { "available", true },
                            { "status", QStringLiteral("calculés par %1, non validés par un ingénieur").arg(s(meta.solverEngine)) },
                            { "engine", s(meta.solverEngine) + (meta.solverVersion.empty() ? QString() : ' ' + s(meta.solverVersion)) },
                            { "case", s(results->caseOrComboName()) },
                            { "timestamp", s(results->timestamp()) },
                            { "analysisType", results->analysisType() == TSA::Analysis::AnalysisType::LinearStatic ? QStringLiteral("statique linéaire")
                                                                                                                   : QStringLiteral("statique non linéaire") },
                            { "dimension", s(meta.analysisDimension) },
                            { "units", QJsonObject { { "force", s(u.force) }, { "moment", s(u.moment) }, { "length", s(u.length) } } } };
        // Déplacements en mm si le moteur travaille en mètres, sinon dans l'unité du moteur (aucune conversion supposée).
        const double k = u.length == "m" ? 1000.0 : 1.0;
        const QString du = u.length == "m" ? QStringLiteral("mm") : s(u.length);
        QJsonArray disp;
        double maxD = -1;
        QString maxNode;
        for (const auto& [nid, d] : results->allDisplacements())
        {
            const double mag = d.translationMagnitude();
            disp.append(QJsonObject { { "node", QStringLiteral("N%1").arg(nid) }, { "ux", f(d.ux * k, 3) }, { "uy", f(d.uy * k, 3) },
                                      { "uz", f(d.uz * k, 3) }, { "magnitude", f(mag * k, 3) } });
            if (mag > maxD) maxD = mag, maxNode = QStringLiteral("N%1").arg(nid);
        }
        res.insert("displacements", disp.isEmpty() ? unavailable(QStringLiteral("déplacements non fournis par le moteur"))
                                                   : QJsonObject { { "available", true }, { "unit", du }, { "rows", disp },
                                                                   { "max", QJsonObject { { "node", maxNode }, { "value", f(maxD * k, 3) + ' ' + du } } } });
        QJsonArray reac;
        double sx = 0, sy = 0, sz = 0;
        for (const auto& [nid, r] : results->allReactions())
        {
            reac.append(QJsonObject { { "node", QStringLiteral("N%1").arg(nid) }, { "fx", f(r.rx, 2) }, { "fy", f(r.ry, 2) }, { "fz", f(r.rz, 2) },
                                      { "mx", f(r.mx, 2) }, { "my", f(r.my, 2) }, { "mz", f(r.mz, 2) } });
            sx += r.rx, sy += r.ry, sz += r.rz;
        }
        res.insert("reactions", reac.isEmpty() ? unavailable(QStringLiteral("réactions non fournies par le moteur"))
                                               : QJsonObject { { "available", true }, { "forceUnit", s(u.force) }, { "momentUnit", s(u.moment) }, { "rows", reac },
                                                               { "sum", QJsonObject { { "fx", f(sx, 2) }, { "fy", f(sy, 2) }, { "fz", f(sz, 2) } } } });
        QJsonArray forces;
        for (const auto& m : members)
            if (m.toObject().value("results").toObject().value("available").toBool()) forces.append(m);
        res.insert("memberForces", forces.isEmpty() ? unavailable(QStringLiteral("efforts des barres non fournis par le moteur"))
                                                    : QJsonObject { { "available", true }, { "rows", forces } });
        const auto& sm = results->solverMesh();
        if (!sm.empty())
        {
            QJsonArray byType;
            for (auto t : { TSA::Analysis::SolverCellType::Line, TSA::Analysis::SolverCellType::ZeroLength, TSA::Analysis::SolverCellType::Triangle,
                            TSA::Analysis::SolverCellType::Quadrilateral, TSA::Analysis::SolverCellType::Tetrahedron, TSA::Analysis::SolverCellType::Hexahedron })
                if (sm.cellCount(t)) byType.append(QJsonObject { { "type", QString::fromLatin1(TSA::Analysis::solverCellTypeName(t)) }, { "count", int(sm.cellCount(t)) } });
            mesh = QJsonObject { { "available", true }, { "engine", s(sm.engineId) }, { "description", s(sm.description) }, { "nodes", int(sm.nodes.size()) },
                                 { "internalNodes", int(sm.internalNodeCount()) }, { "cells", int(sm.cells.size()) }, { "byType", byType } };
        }
        else
            mesh = unavailable(QStringLiteral("maillage non fourni par le moteur %1").arg(s(meta.solverEngine)));
    }

    const QJsonObject counts { { "nodes", int(model.nodes().size()) },     { "members", int(members.size()) },  { "beams", int(model.beams().size()) },
                               { "columns", int(model.columns().size()) }, { "trusses", int(model.trussMembers().size()) },
                               { "cables", int(model.cables().size()) },   { "slabs", int(model.slabs().size()) }, { "walls", int(model.walls().size()) },
                               { "foundations", int(model.foundations().size()) }, { "supports", int(supports.size()) },
                               { "loadCases", int(cases.size()) },         { "combinations", int(combos.size()) } };
    return {
        { "schema", QLatin1String(kReportDataSchema) },
        { "generated", QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")) },
        { "application", QJsonObject { { "name", TSA::Product::name() }, { "version", TSA::Product::version() } } },
        { "project",
          QJsonObject { { "title", p.title }, { "description", p.description }, { "number", p.number }, { "documentNumber", p.documentNumber },
                        { "revision", p.revision }, { "status", p.status }, { "engineer", p.engineer }, { "organization", p.organization },
                        { "client", p.client }, { "date", p.date.isEmpty() ? QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy")) : p.date } } },
        { "model",
          QJsonObject { { "counts", counts },
                        { "geometry",
                          QJsonObject { { "dx", f(ext[0], 3) + QStringLiteral(" m") }, { "dy", f(ext[1], 3) + QStringLiteral(" m") },
                                        { "dz", f(ext[2], 3) + QStringLiteral(" m") }, { "planar", !plane.isEmpty() }, { "plane", plane } } },
                        { "levels", levels },
                        { "nodes", nodes },
                        { "supports", supports },
                        { "members", members },
                        { "beams", beams },
                        { "columns", columns },
                        { "materials", matArr },
                        { "sections", secArr },
                        { "loadCases", cases },
                        { "combinations", combos },
                        { "nodalLoads", nodalLoads },
                        { "memberLoads", memberLoads } } },
        { "results", res },
        { "mesh", mesh },
        { "modal", unavailable(QStringLiteral("analyse modale non disponible : les moteurs de %1 réalisent des calculs statiques").arg(TSA::Product::name())) },
        { "verification", unavailable(QStringLiteral("aucune vérification normative structurée dans les résultats : les vérifications Eurocode "
                                                     "figurent dans la note de calcul standard lorsqu'elles sont activées")) },
    };
}

} // namespace TSA::Reports
