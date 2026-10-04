#include "ResultsExport.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <iomanip>
#include <sstream>

namespace TSA::Analysis
{

namespace
{
const std::vector<std::string> kElementDofLabels {
    "UX_i", "UY_i", "UZ_i", "RX_i", "RY_i", "RZ_i", "UX_j", "UY_j", "UZ_j", "RX_j", "RY_j", "RZ_j" };

std::string num(double v)
{
    std::ostringstream ss;
    ss << std::setprecision(17) << v;
    return ss.str();
}

QJsonObject metaJson(const MatrixMetadata& m)
{
    QJsonObject o;
    o["name"] = QString::fromStdString(m.name);
    o["source"] = QString::fromStdString(m.source);
    o["type"] = QString::fromStdString(m.matrixType);
    o["coordinateSystem"] = QString::fromStdString(m.coordinateSystem);
    o["dofOrdering"] = QString::fromStdString(m.dofOrdering);
    o["constraints"] = QString::fromStdString(m.constraints);
    o["solver"] = QString::fromStdString(m.solver);
    o["storage"] = QString::fromStdString(m.storage);
    o["units"] = QString::fromStdString(m.units);
    o["symmetric"] = m.symmetric;
    o["exact"] = m.exact;
    o["significantDigits"] = m.significantDigits;
    o["notes"] = QString::fromStdString(m.notes);
    return o;
}

QJsonArray denseJson(const DenseMatrix& m)
{
    QJsonArray rows;
    for (int i = 0; i < m.rows; ++i)
    {
        QJsonArray r;
        for (int j = 0; j < m.cols; ++j) r.append(m(i, j));
        rows.append(r);
    }
    return rows;
}

QJsonArray vecJson(const std::vector<double>& v)
{
    QJsonArray a;
    for (double x : v) a.append(x);
    return a;
}

void metaTxt(std::ostringstream& out, const MatrixMetadata& m)
{
    out << "  Source            : " << m.source << "\n"
        << "  Type              : " << m.matrixType << "\n"
        << "  Coordinate system : " << m.coordinateSystem << "\n"
        << "  DOF ordering      : " << m.dofOrdering << "\n"
        << "  Constraints       : " << m.constraints << "\n"
        << "  Solver            : " << m.solver << "\n"
        << "  Storage           : " << m.storage << "\n"
        << "  Units             : " << m.units << "\n"
        << "  Symmetric         : " << (m.symmetric ? "yes" : "no") << "\n"
        << "  Exact             : " << (m.exact ? "true" : "false") << "\n";
    if (!m.notes.empty()) out << "  Notes             : " << m.notes << "\n";
}

QJsonObject unitsJson(const UnitSystem& u)
{
    QJsonObject o;
    o["force"] = QString::fromStdString(u.force);
    o["length"] = QString::fromStdString(u.length);
    o["moment"] = QString::fromStdString(u.moment);
    o["translationalStiffness"] = QString::fromStdString(u.translationalStiffness);
    o["rotationalStiffness"] = QString::fromStdString(u.rotationalStiffness);
    return o;
}

QJsonObject datasetJson(const ResultsModel& r, ResultsDataset ds)
{
    const auto& adv = r.advanced();
    QJsonObject root;
    root["units"] = unitsJson(r.units());
    root["timestamp"] = QString::fromStdString(r.timestamp());
    const auto& meta = r.executionMetadata();
    QJsonObject m;
    m["solverEngine"] = QString::fromStdString(meta.solverEngine);
    m["solverVersion"] = QString::fromStdString(meta.solverVersion);
    m["loadCase"] = QString::fromStdString(meta.loadCombinationType);
    m["system"] = QString::fromStdString(meta.systemSolver);
    m["constraints"] = QString::fromStdString(meta.constraintHandler);
    m["numberer"] = QString::fromStdString(meta.numberer);
    m["geomTransf"] = QString::fromStdString(meta.geomTransf);
    m["model"] = QString::fromStdString(meta.modelBuilder);
    m["extraction"] = meta.extractionLevel == ExtractionLevel::Advanced ? "ADVANCED" : "LIGHT";
    m["equilibriumRelativeResidual"] = meta.relativeEquilibriumResidual;
    root["metadata"] = m;

    const bool all = ds == ResultsDataset::All;
    if (all || ds == ResultsDataset::Displacements)
    {
        QJsonArray a;
        for (const auto& [id, d] : r.allDisplacements())
        {
            QJsonObject o;
            o["node"] = id;
            o["UX"] = d.ux; o["UY"] = d.uy; o["UZ"] = d.uz;
            o["RX"] = d.rx; o["RY"] = d.ry; o["RZ"] = d.rz;
            a.append(o);
        }
        root["displacements"] = a;
    }
    if (all || ds == ResultsDataset::Reactions)
    {
        QJsonArray a;
        for (const auto& [id, x] : r.allReactions())
        {
            QJsonObject o;
            o["node"] = id;
            o["FX"] = x.rx; o["FY"] = x.ry; o["FZ"] = x.rz;
            o["MX"] = x.mx; o["MY"] = x.my; o["MZ"] = x.mz;
            a.append(o);
        }
        root["reactions"] = a;
        const auto eq = r.equilibrium();
        QJsonObject e;
        e["appliedFx"] = eq.appliedFx; e["appliedFy"] = eq.appliedFy; e["appliedFz"] = eq.appliedFz;
        e["reactionFx"] = eq.reactionFx; e["reactionFy"] = eq.reactionFy; e["reactionFz"] = eq.reactionFz;
        root["equilibrium"] = e;
    }
    if (all || ds == ResultsDataset::ElementForces)
    {
        QJsonArray a;
        for (const auto& [key, er] : r.allElementResults())
        {
            QJsonObject o;
            o["element"] = QString::fromStdString(key.label());
            o["kind"] = elementKindName(key.kind);
            o["id"] = key.id;
            o["opsTag"] = er.opsTag;
            o["length"] = er.length;
            QJsonObject s, e;
            s["N"] = er.startForces.N; s["Vy"] = er.startForces.Vy; s["Vz"] = er.startForces.Vz;
            s["Mx"] = er.startForces.Mx; s["My"] = er.startForces.My; s["Mz"] = er.startForces.Mz;
            e["N"] = er.endForces.N; e["Vy"] = er.endForces.Vy; e["Vz"] = er.endForces.Vz;
            e["Mx"] = er.endForces.Mx; e["My"] = er.endForces.My; e["Mz"] = er.endForces.Mz;
            o["start"] = s;
            o["end"] = e;
            auto it = adv.elementForces.find(key);
            if (it != adv.elementForces.end())
            {
                o["localForce"] = vecJson(it->second.local);
                o["localForceSource"] = QString::fromStdString(it->second.localSource);
                o["globalForce"] = vecJson(it->second.global);
                o["basicForce"] = vecJson(it->second.basic);
            }
            a.append(o);
        }
        root["elementForces"] = a;
        root["elementForceConvention"] = "start/end : convention RDM TSA (N traction > 0). localForce/globalForce : "
                                         "forces exercées sur l'élément par les nœuds, ordre [Fx Fy Fz Mx My Mz]_i,j.";
    }
    if (all || ds == ResultsDataset::DofMap)
    {
        QJsonArray a;
        for (const auto& e : adv.dofMap.equations)
        {
            QJsonObject o;
            o["equation"] = e.equation;
            o["node"] = e.nodeId;
            o["dof"] = QString::fromStdString(adv.dofMap.dofLabels[e.dof]);
            a.append(o);
        }
        QJsonObject d;
        d["equations"] = a;
        d["ndm"] = adv.dofMap.ndm;
        d["ndf"] = adv.dofMap.ndf;
        d["numberer"] = QString::fromStdString(adv.dofMap.numberer);
        d["constraintHandler"] = QString::fromStdString(adv.dofMap.constraintHandler);
        d["source"] = QString::fromStdString(adv.dofMap.source);
        root["dofMap"] = d;
    }
    if (all || ds == ResultsDataset::GlobalStiffness)
    {
        QJsonObject k;
        k["available"] = adv.hasGlobalStiffness;
        if (adv.hasGlobalStiffness)
        {
            k["metadata"] = metaJson(adv.kGlobalMeta);
            k["size"] = adv.kGlobal.rows;
            QJsonArray rows, cols, vals;
            for (std::size_t q = 0; q < adv.kGlobal.values.size(); ++q)
            {
                rows.append(adv.kGlobal.rowIndex[q]);
                cols.append(adv.kGlobal.colIndex[q]);
                vals.append(adv.kGlobal.values[q]);
            }
            k["rowIndex"] = rows;
            k["colIndex"] = cols;
            k["values"] = vals;
        }
        else
        {
            k["reason"] = QString::fromStdString(adv.kGlobalUnavailableReason);
        }
        root["globalStiffness"] = k;
    }
    if (all || ds == ResultsDataset::ElementStiffness)
    {
        QJsonArray a;
        for (const auto& [key, em] : adv.elementMatrices)
        {
            QJsonObject o;
            o["element"] = QString::fromStdString(key.label());
            o["opsTag"] = em.opsTag;
            o["opsClass"] = QString::fromStdString(em.opsClass);
            o["available"] = em.available;
            if (!em.available)
            {
                o["reason"] = QString::fromStdString(em.unavailableReason);
            }
            else
            {
                QJsonArray rot;
                for (double v : em.rotation) rot.append(v);
                o["localAxes"] = rot;
                o["kBasic"] = denseJson(em.kBasic);
                o["kBasicMeta"] = metaJson(em.kBasicMeta);
                o["kLocal"] = denseJson(em.kLocal);
                o["kLocalMeta"] = metaJson(em.kLocalMeta);
                o["kGlobal"] = denseJson(em.kGlobal);
                o["kGlobalMeta"] = metaJson(em.kGlobalMeta);
            }
            a.append(o);
        }
        root["elementStiffness"] = a;
    }
    return root;
}

bool needsAdvanced(ResultsDataset ds)
{
    return ds == ResultsDataset::DofMap || ds == ResultsDataset::GlobalStiffness || ds == ResultsDataset::ElementStiffness;
}

std::string renderCsv(const ResultsModel& r, ResultsDataset ds, std::string* err)
{
    const auto& adv = r.advanced();
    const auto& u = r.units();
    std::ostringstream out;
    switch (ds)
    {
    case ResultsDataset::Displacements:
        out << "node,UX[" << u.length << "],UY[" << u.length << "],UZ[" << u.length << "],RX[rad],RY[rad],RZ[rad]\n";
        for (const auto& [id, d] : r.allDisplacements())
            out << id << ',' << num(d.ux) << ',' << num(d.uy) << ',' << num(d.uz) << ','
                << num(d.rx) << ',' << num(d.ry) << ',' << num(d.rz) << '\n';
        break;
    case ResultsDataset::Reactions:
        out << "node,FX[" << u.force << "],FY[" << u.force << "],FZ[" << u.force << "],MX[" << u.moment
            << "],MY[" << u.moment << "],MZ[" << u.moment << "]\n";
        for (const auto& [id, x] : r.allReactions())
            out << id << ',' << num(x.rx) << ',' << num(x.ry) << ',' << num(x.rz) << ','
                << num(x.mx) << ',' << num(x.my) << ',' << num(x.mz) << '\n';
        break;
    case ResultsDataset::ElementForces:
        out << "element,kind,id,opsTag,end,N,Vy,Vz,Mx,My,Mz\n";
        for (const auto& [key, er] : r.allElementResults())
        {
            for (int end = 0; end < 2; ++end)
            {
                const StationForces& f = end == 0 ? er.startForces : er.endForces;
                out << key.label() << ',' << elementKindName(key.kind) << ',' << key.id << ',' << er.opsTag << ','
                    << (end == 0 ? 'i' : 'j') << ',' << num(f.N) << ',' << num(f.Vy) << ',' << num(f.Vz) << ','
                    << num(f.Mx) << ',' << num(f.My) << ',' << num(f.Mz) << '\n';
            }
        }
        break;
    case ResultsDataset::DofMap:
        out << "equation,node,dof\n";
        for (const auto& e : adv.dofMap.equations)
            out << e.equation << ',' << e.nodeId << ',' << adv.dofMap.dofLabels[e.dof] << '\n';
        break;
    case ResultsDataset::GlobalStiffness:
        if (!adv.hasGlobalStiffness)
        {
            if (err) *err = "K_global indisponible : " + adv.kGlobalUnavailableReason;
            return {};
        }
        out << "# " << adv.kGlobalMeta.source << " | " << adv.kGlobalMeta.matrixType << " | exact="
            << (adv.kGlobalMeta.exact ? "true" : "false") << " | " << adv.kGlobalMeta.units << '\n';
        out << "row,col,row_dof,col_dof,value\n";
        for (std::size_t q = 0; q < adv.kGlobal.values.size(); ++q)
            out << adv.kGlobal.rowIndex[q] << ',' << adv.kGlobal.colIndex[q] << ','
                << adv.dofMap.equationLabel(adv.kGlobal.rowIndex[q]) << ','
                << adv.dofMap.equationLabel(adv.kGlobal.colIndex[q]) << ',' << num(adv.kGlobal.values[q]) << '\n';
        break;
    case ResultsDataset::ElementStiffness:
        out << "element,matrix,row,col,value,source,exact\n";
        for (const auto& [key, em] : adv.elementMatrices)
        {
            if (!em.available) continue;
            const std::pair<const DenseMatrix*, const MatrixMetadata*> mats[3] = {
                { &em.kBasic, &em.kBasicMeta }, { &em.kLocal, &em.kLocalMeta }, { &em.kGlobal, &em.kGlobalMeta } };
            const char* names[3] = { "k_basic", "k_local", "K_global" };
            for (int k = 0; k < 3; ++k)
                for (int i = 0; i < mats[k].first->rows; ++i)
                    for (int j = 0; j < mats[k].first->cols; ++j)
                        out << key.label() << ',' << names[k] << ',' << i << ',' << j << ','
                            << num((*mats[k].first)(i, j)) << ",\"" << mats[k].second->source << "\","
                            << (mats[k].second->exact ? "true" : "false") << '\n';
        }
        break;
    case ResultsDataset::All:
        if (err) *err = "L'export complet n'est disponible qu'en JSON ou TXT.";
        return {};
    }
    return out.str();
}

std::string renderTxt(const ResultsModel& r, ResultsDataset ds)
{
    const auto& adv = r.advanced();
    const auto& u = r.units();
    std::ostringstream out;
    out << std::setprecision(10);
    out << "=========================================\n"
        << "TSA — RÉSULTATS OPENSEES\n"
        << "=========================================\n"
        << "Unités : force " << u.force << ", longueur " << u.length << ", moment " << u.moment << "\n"
        << "Horodatage : " << r.timestamp() << "\n\n";
    const bool all = ds == ResultsDataset::All;
    if (all || ds == ResultsDataset::Displacements)
    {
        out << "DÉPLACEMENTS NODAUX\n";
        for (const auto& [id, d] : r.allDisplacements())
            out << "  N" << id << "  UX=" << d.ux << "  UY=" << d.uy << "  UZ=" << d.uz
                << "  RX=" << d.rx << "  RY=" << d.ry << "  RZ=" << d.rz << "\n";
        out << "\n";
    }
    if (all || ds == ResultsDataset::Reactions)
    {
        out << "RÉACTIONS\n";
        for (const auto& [id, x] : r.allReactions())
            out << "  N" << id << "  FX=" << x.rx << "  FY=" << x.ry << "  FZ=" << x.rz
                << "  MX=" << x.mx << "  MY=" << x.my << "  MZ=" << x.mz << "\n";
        out << "\n";
    }
    if (all || ds == ResultsDataset::ElementForces)
    {
        out << "EFFORTS D'EXTRÉMITÉ (convention RDM TSA)\n";
        for (const auto& [key, er] : r.allElementResults())
            out << "  " << key.label() << " (tag " << er.opsTag << ")  i: N=" << er.startForces.N << " Mz=" << er.startForces.Mz
                << " My=" << er.startForces.My << "   j: N=" << er.endForces.N << " Mz=" << er.endForces.Mz
                << " My=" << er.endForces.My << "\n";
        out << "\n";
    }
    if (all || ds == ResultsDataset::DofMap)
    {
        out << "MAPPING DES DDL (" << adv.dofMap.equationCount() << " équations, numberer "
            << adv.dofMap.numberer << ", constraints " << adv.dofMap.constraintHandler << ")\n";
        for (const auto& e : adv.dofMap.equations)
            out << "  Global DOF " << e.equation << " → " << adv.dofMap.equationLabel(e.equation) << "\n";
        out << "\n";
    }
    if (all || ds == ResultsDataset::GlobalStiffness)
    {
        out << "K_GLOBAL\n";
        if (adv.hasGlobalStiffness)
        {
            metaTxt(out, adv.kGlobalMeta);
            out << "  Taille : " << adv.kGlobal.rows << " × " << adv.kGlobal.cols << ", " << adv.kGlobal.nonZeros() << " non nuls\n";
            for (std::size_t q = 0; q < adv.kGlobal.values.size(); ++q)
                out << "  (" << adv.dofMap.equationLabel(adv.kGlobal.rowIndex[q]) << ", "
                    << adv.dofMap.equationLabel(adv.kGlobal.colIndex[q]) << ") = " << adv.kGlobal.values[q] << "\n";
        }
        else
        {
            out << "  Indisponible : " << adv.kGlobalUnavailableReason << "\n";
        }
        out << "\n";
    }
    if (all || ds == ResultsDataset::ElementStiffness)
    {
        for (const auto& [key, em] : adv.elementMatrices)
        {
            out << "Element " << key.label() << " (" << em.opsClass << ", tag " << em.opsTag << ")\n";
            if (!em.available)
            {
                out << "  Indisponible : " << em.unavailableReason << "\n\n";
                continue;
            }
            out << "LOCAL STIFFNESS\n";
            metaTxt(out, em.kLocalMeta);
            out << ResultsExport::formatMatrix(em.kLocal, kElementDofLabels) << "\n";
            out << "GLOBAL STIFFNESS\n";
            metaTxt(out, em.kGlobalMeta);
            out << ResultsExport::formatMatrix(em.kGlobal, kElementDofLabels) << "\n";
            auto it = adv.elementForces.find(key);
            if (it != adv.elementForces.end())
            {
                out << "LOCAL FORCES  (" << it->second.localSource << ")\n ";
                for (double v : it->second.local) out << ' ' << v;
                out << "\nGLOBAL FORCES (OpenSees API: globalForce)\n ";
                for (double v : it->second.global) out << ' ' << v;
                out << "\n";
            }
            out << "\n";
        }
    }
    return out.str();
}
} // namespace

std::string ResultsExport::formatMatrix(const DenseMatrix& m, const std::vector<std::string>& labels, int precision)
{
    std::ostringstream out;
    out << std::setw(8) << "DOF";
    for (int j = 0; j < m.cols; ++j)
        out << std::setw(15) << (j < static_cast<int>(labels.size()) ? labels[j] : std::to_string(j + 1));
    out << "\n";
    out << std::setprecision(precision);
    for (int i = 0; i < m.rows; ++i)
    {
        out << std::setw(8) << (i < static_cast<int>(labels.size()) ? labels[i] : std::to_string(i + 1));
        for (int j = 0; j < m.cols; ++j)
            out << std::setw(15) << m(i, j);
        out << "\n";
    }
    return out.str();
}

std::string ResultsExport::render(const ResultsModel& results, ResultsDataset dataset, ResultsExportFormat format,
                                  std::string* errorMessage)
{
    if (needsAdvanced(dataset) && !results.advanced().available)
    {
        if (errorMessage)
            *errorMessage = "Données avancées absentes : relancer le calcul en mode ADVANCED (matrices et mapping DDL).";
        return {};
    }
    switch (format)
    {
    case ResultsExportFormat::Csv:
        return renderCsv(results, dataset, errorMessage);
    case ResultsExportFormat::Json:
        return QJsonDocument(datasetJson(results, dataset)).toJson(QJsonDocument::Indented).toStdString();
    case ResultsExportFormat::Txt:
        return renderTxt(results, dataset);
    }
    return {};
}

bool ResultsExport::write(const ResultsModel& results, ResultsDataset dataset, ResultsExportFormat format,
                          const std::string& path, std::string* errorMessage)
{
    std::string err;
    const std::string content = render(results, dataset, format, &err);
    if (content.empty())
    {
        if (errorMessage) *errorMessage = err.empty() ? "Aucune donnée à exporter." : err;
        return false;
    }
    QSaveFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::WriteOnly))
    {
        if (errorMessage) *errorMessage = "Impossible d'écrire " + path;
        return false;
    }
    f.write(content.data(), static_cast<qint64>(content.size()));
    if (!f.commit())
    {
        if (errorMessage) *errorMessage = "Échec de l'écriture de " + path;
        return false;
    }
    return true;
}

} // namespace TSA::Analysis
