#include "ResultAnalyzer.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Analysis/ResultsModel.h"

#include <cmath>
#include <limits>

namespace TSA::NDC
{

gp_Pnt ResultAnalyzer::computeGlobalPoint(
    const TSA::Model::Model& model,
    int elementId,
    double localX)
{
    int sNodeId = 0, eNodeId = 0;
    
    if (const auto* b = model.getBeam(elementId)) {
        sNodeId = b->startNodeId();
        eNodeId = b->endNodeId();
    } else if (const auto* col = model.getColumn(elementId)) {
        sNodeId = col->startNodeId();
        eNodeId = col->endNodeId();
    } else if (const auto* tr = model.getTrussMember(elementId)) {
        sNodeId = tr->startNodeId();
        eNodeId = tr->endNodeId();
    } else if (const auto* cb = model.getCable(elementId)) {
        sNodeId = cb->startNodeId();
        eNodeId = cb->endNodeId();
    }
    
    const auto* n1 = model.getNode(sNodeId);
    const auto* n2 = model.getNode(eNodeId);
    if (!n1 || !n2) {
        return gp_Pnt(0.0, 0.0, 0.0);
    }
    
    gp_Pnt p1(n1->x(), n1->y(), n1->z());
    gp_Pnt p2(n2->x(), n2->y(), n2->z());
    double L = p1.Distance(p2);
    if (L < 1e-6) {
        return p1;
    }
    
    double t = std::clamp(localX / L, 0.0, 1.0);
    return gp_Pnt(
        p1.X() + t * (p2.X() - p1.X()),
        p1.Y() + t * (p2.Y() - p1.Y()),
        p1.Z() + t * (p2.Z() - p1.Z())
    );
}

MostStressedSummary ResultAnalyzer::analyzeExtrema(
    const TSA::Model::Model& model,
    const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
    const QString& governingCombination)
{
    MostStressedSummary summary;
    if (!results || !results->isValid()) {
        return summary;
    }

    auto fillExtremum = [&](ExtremumPoint& pt, const QString& qName, double val, const QString& unit,
                            int elId, const QString& elType, const QString& sec, const QString& mat,
                            double x, double L, const gp_Pnt& coords) {
        pt.quantityName = qName;
        pt.value = val;
        pt.absValue = std::abs(val);
        pt.unit = unit;
        pt.elementId = elId;
        pt.elementType = elType;
        pt.sectionName = sec;
        pt.materialName = mat;
        pt.localPositionX = x;
        pt.memberLength = L;
        pt.relativePosition = (L > 1e-5) ? std::clamp(x / L, 0.0, 1.0) : 0.0;
        pt.globalCoords = coords;
        pt.loadCaseOrCombo = governingCombination;
    };

    double maxMz = -std::numeric_limits<double>::infinity();
    double minMz = std::numeric_limits<double>::infinity();
    double absMz = 0.0;

    double maxMy = -std::numeric_limits<double>::infinity();
    double absMy = 0.0;

    double maxVz = -std::numeric_limits<double>::infinity();
    double absVz = 0.0;
    double absVy = 0.0;

    double maxTension = 0.0;
    double maxComp = 0.0; // valeur absolue de la compression négative

    double absMx = 0.0;
    double maxDefl = 0.0; // en mm

    // 1. Parcours de tous les éléments
    for (const auto& [elId, elRes] : results->allElementResults())
    {
        QString elType = QStringLiteral("Barre");
        QString secName, matName;
        int sNodeId = 0, eNodeId = 0;

        if (const auto* b = model.getBeam(elId)) {
            elType = QStringLiteral("Poutre");
            secName = QString::fromStdString(b->section().name);
            matName = QString::fromStdString(b->material().name);
            sNodeId = b->startNodeId();
            eNodeId = b->endNodeId();
        } else if (const auto* col = model.getColumn(elId)) {
            elType = QStringLiteral("Poteau");
            secName = QString::fromStdString(col->section().name);
            matName = QString::fromStdString(col->material().name);
            sNodeId = col->startNodeId();
            eNodeId = col->endNodeId();
        } else if (const auto* tr = model.getTrussMember(elId)) {
            elType = QStringLiteral("Treillis");
            secName = QString::fromStdString(tr->section().name);
            matName = QString::fromStdString(tr->material().name);
            sNodeId = tr->startNodeId();
            eNodeId = tr->endNodeId();
        } else if (const auto* cb = model.getCable(elId)) {
            elType = QStringLiteral("Câble");
            secName = QString::fromStdString(cb->section().name);
            matName = QString::fromStdString(cb->material().name);
            sNodeId = cb->startNodeId();
            eNodeId = cb->endNodeId();
        }

        const auto* n1 = model.getNode(sNodeId);
        const auto* n2 = model.getNode(eNodeId);
        double L = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : elRes.length;
        if (L < 1e-6) L = 1.0;

        std::vector<TSA::Analysis::StationForces> stations;
        auto sfStart = elRes.startForces;
        sfStart.position = 0.0;
        stations.push_back(sfStart);

        for (const auto& st : elRes.intermediateStations) {
            stations.push_back(st);
        }

        auto sfEnd = elRes.endForces;
        sfEnd.position = L;
        stations.push_back(sfEnd);

        for (const auto& st : stations)
        {
            double x = std::clamp(st.position, 0.0, L);
            gp_Pnt pWcs = computeGlobalPoint(model, elId, x);

            // Moment fléchissant Mz
            if (st.Mz > maxMz) {
                maxMz = st.Mz;
                fillExtremum(summary.maxBendingMz, QStringLiteral("Moment Fléchissant Mz (Max Positif)"),
                             st.Mz, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }
            if (st.Mz < minMz) {
                minMz = st.Mz;
                fillExtremum(summary.minBendingMz, QStringLiteral("Moment Fléchissant Mz (Max Négatif / Appui)"),
                             st.Mz, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }
            if (std::abs(st.Mz) > absMz) {
                absMz = std::abs(st.Mz);
                fillExtremum(summary.absMaxBendingMz, QStringLiteral("Moment Fléchissant Mz (Max Absolu)"),
                             st.Mz, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Moment fléchissant My
            if (st.My > maxMy) {
                maxMy = st.My;
                fillExtremum(summary.maxBendingMy, QStringLiteral("Moment Fléchissant My (Max Positif)"),
                             st.My, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }
            if (std::abs(st.My) > absMy) {
                absMy = std::abs(st.My);
                fillExtremum(summary.absMaxBendingMy, QStringLiteral("Moment Fléchissant My (Max Absolu)"),
                             st.My, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Effort tranchant Vz
            if (st.Vz > maxVz) {
                maxVz = st.Vz;
                fillExtremum(summary.maxShearVz, QStringLiteral("Effort Tranchant Vz (Max)"),
                             st.Vz, QStringLiteral("kN"), elId, elType, secName, matName, x, L, pWcs);
            }
            if (std::abs(st.Vz) > absVz) {
                absVz = std::abs(st.Vz);
                fillExtremum(summary.absMaxShearVz, QStringLiteral("Effort Tranchant Vz (Max Absolu)"),
                             st.Vz, QStringLiteral("kN"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Effort tranchant Vy
            if (std::abs(st.Vy) > absVy) {
                absVy = std::abs(st.Vy);
                fillExtremum(summary.absMaxShearVy, QStringLiteral("Effort Tranchant Vy (Max Absolu)"),
                             st.Vy, QStringLiteral("kN"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Effort normal N
            if (st.N > maxTension) {
                maxTension = st.N;
                fillExtremum(summary.maxTensionN, QStringLiteral("Effort Normal N (Traction Maximale)"),
                             st.N, QStringLiteral("kN"), elId, elType, secName, matName, x, L, pWcs);
            }
            if (st.N < 0.0 && std::abs(st.N) > maxComp) {
                maxComp = std::abs(st.N);
                fillExtremum(summary.maxCompressionN, QStringLiteral("Effort Normal N (Compression Maximale)"),
                             st.N, QStringLiteral("kN"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Torsion Mx
            if (std::abs(st.Mx) > absMx) {
                absMx = std::abs(st.Mx);
                fillExtremum(summary.maxTorsionMx, QStringLiteral("Moment de Torsion Mx (Max Absolu)"),
                             st.Mx, QStringLiteral("kNm"), elId, elType, secName, matName, x, L, pWcs);
            }

            // Flèche transversale le long de l'élément (exprimée en mm)
            double deflM = std::sqrt(st.uy * st.uy + st.uz * st.uz);
            double deflMm = deflM * 1000.0;
            if (deflMm > maxDefl) {
                maxDefl = deflMm;
                fillExtremum(summary.maxDeflection, QStringLiteral("Flèche Maximale en Travée"),
                             deflMm, QStringLiteral("mm"), elId, elType, secName, matName, x, L, pWcs);
                if (deflM > 1e-6) {
                    summary.maxDeflection.spanToDeflectionRatio = L / deflM;
                    summary.maxDeflection.exceedsStandardLimit = (deflM > (L / 250.0));
                }
            }
        }
    }

    // 2. Déplacements nodaux globaux
    double maxDispMm = 0.0;
    for (const auto& [nId, disp] : results->allDisplacements())
    {
        double magM = disp.translationMagnitude();
        double magMm = magM * 1000.0;
        if (magMm > maxDispMm) {
            maxDispMm = magMm;
            summary.maxDisplacement.quantityName = QStringLiteral("Déplacement Nodal Résultant Maximal");
            summary.maxDisplacement.value = magMm;
            summary.maxDisplacement.absValue = magMm;
            summary.maxDisplacement.unit = QStringLiteral("mm");
            summary.maxDisplacement.nodeId = nId;
            summary.maxDisplacement.loadCaseOrCombo = governingCombination;
            if (const auto* n = model.getNode(nId)) {
                summary.maxDisplacement.globalCoords = gp_Pnt(n->x(), n->y(), n->z());
            }
        }
    }

    // 3. Réactions d'appuis globales
    double maxReactKn = 0.0;
    for (const auto& [nId, r] : results->allReactions())
    {
        double mag = r.forceMagnitude();
        if (mag > maxReactKn) {
            maxReactKn = mag;
            summary.maxReaction.quantityName = QStringLiteral("Réaction d'Appui Résultante Maximale");
            summary.maxReaction.value = mag;
            summary.maxReaction.absValue = mag;
            summary.maxReaction.unit = QStringLiteral("kN");
            summary.maxReaction.nodeId = nId;
            summary.maxReaction.loadCaseOrCombo = governingCombination;
            if (const auto* n = model.getNode(nId)) {
                summary.maxReaction.globalCoords = gp_Pnt(n->x(), n->y(), n->z());
            }
        }
    }

    return summary;
}

} // namespace TSA::NDC
