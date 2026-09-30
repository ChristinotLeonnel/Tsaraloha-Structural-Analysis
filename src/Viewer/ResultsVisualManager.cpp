#include "ResultsVisualManager.h"
#include "OccView.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Section.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Quantity_Color.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>
#include <algorithm>

namespace
{
static TopoDS_Shape makeArrowShape(const gp_Pnt& targetPnt, const gp_Vec& dir, double length, double shaftRadius, double headRadius, double headLength)
{
    if (dir.Magnitude() < 1e-6 || length <= headLength) return TopoDS_Shape();
    gp_Dir d(dir);
    gp_Pnt basePnt = targetPnt.Translated(-gp_Vec(d) * length);
    gp_Ax2 cylAxes(basePnt, d);
    BRepPrimAPI_MakeCylinder cyl(cylAxes, shaftRadius, length - headLength);

    gp_Pnt headBase = basePnt.Translated(gp_Vec(d) * (length - headLength));
    gp_Ax2 coneAxes(headBase, d);
    BRepPrimAPI_MakeCone cone(coneAxes, headRadius, 0.0, headLength);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    bb.Add(comp, cyl.Shape());
    bb.Add(comp, cone.Shape());
    return comp;
}

static std::vector<TSA::Analysis::StationForces> getAllStations(const TSA::Analysis::ElementResults& el)
{
    std::vector<TSA::Analysis::StationForces> res;
    res.reserve(2 + el.intermediateStations.size());
    res.push_back(el.startForces);
    res.insert(res.end(), el.intermediateStations.begin(), el.intermediateStations.end());
    res.push_back(el.endForces);
    return res;
}
} // namespace

namespace TSA::Viewer
{

ResultsVisualManager::ResultsVisualManager(OccView* occView, QObject* parent)
    : QObject(parent)
    , m_occView(occView)
{
    connect(&m_modalAnimationTimer, &QTimer::timeout, this, &ResultsVisualManager::onModalTimerTick);
}

ResultsVisualManager::~ResultsVisualManager()
{
    clearAllVisuals();
}

Handle(AIS_InteractiveContext) ResultsVisualManager::context() const
{
    return m_occView ? m_occView->context() : Handle(AIS_InteractiveContext)();
}

void ResultsVisualManager::setModel(TSA::Model::Model* model)
{
    m_model = model;
}

void ResultsVisualManager::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    if (m_results && m_results->isValid())
    {
        autoComputeDeformationScale();
        autoComputeDiagramScale();
    }
    updateAllVisuals();
}

bool ResultsVisualManager::hasResults() const
{
    return m_results && m_results->isValid();
}

void ResultsVisualManager::setDeformedVisible(bool visible)
{
    if (m_deformedVisible == visible) return;
    m_deformedVisible = visible;
    updateDeformedShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::setDeformedDisplayMode(DeformedDisplayMode mode)
{
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    updateDeformedShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::setDeformationScale(double scale)
{
    if (std::abs(m_deformationScale - scale) < 1e-4) return;
    m_deformationScale = scale;
    updateDeformedShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::autoComputeDeformationScale()
{
    if (!m_results || !m_model || m_results->allDisplacements().empty()) return;

    double maxU = 0.0;
    for (const auto& [id, d] : m_results->allDisplacements())
    {
        double u = d.translationMagnitude();
        if (u > maxU) maxU = u;
    }

    if (maxU < 1e-9)
    {
        m_deformationScale = 1.0;
        return;
    }

    // Calcul de l'envergure du modèle
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
    for (const auto& [id, node] : m_model->nodes())
    {
        minX = std::min(minX, node.x()); maxX = std::max(maxX, node.x());
        minY = std::min(minY, node.y()); maxY = std::max(maxY, node.y());
        minZ = std::min(minZ, node.z()); maxZ = std::max(maxZ, node.z());
    }

    double span = std::max({maxX - minX, maxY - minY, maxZ - minZ, 5.0});
    // Viser une déformation visuelle max de 5% de la portée globale
    m_deformationScale = std::clamp(0.05 * span / maxU, 1.0, 5000.0);
}

void ResultsVisualManager::setDiagramType(TSA::Geometry::DiagramType type)
{
    if (m_diagramType == type) return;
    m_diagramType = type;
    autoComputeDiagramScale();
    updateDiagramShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::setDiagramScale(double scale)
{
    if (std::abs(m_diagramScale - scale) < 1e-6) return;
    m_diagramScale = scale;
    updateDiagramShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::autoComputeDiagramScale()
{
    if (!m_results || !m_model || m_diagramType == TSA::Geometry::DiagramType::None) return;

    double maxVal = 0.0;
    for (const auto& [id, el] : m_results->allElementResults())
    {
        auto stations = getAllStations(el);
        for (const auto& st : stations)
        {
            double v = std::abs(TSA::Geometry::DiagramGeometry::getStationValue(st, m_diagramType));
            if (v > maxVal) maxVal = v;
        }
    }

    if (maxVal < 1e-6)
    {
        m_diagramScale = 0.05;
        return;
    }

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9, minZ = 1e9, maxZ = -1e9;
    for (const auto& [id, node] : m_model->nodes())
    {
        minX = std::min(minX, node.x()); maxX = std::max(maxX, node.x());
        minY = std::min(minY, node.y()); maxY = std::max(maxY, node.y());
        minZ = std::min(minZ, node.z()); maxZ = std::max(maxZ, node.z());
    }
    double span = std::max({maxX - minX, maxY - minY, maxZ - minZ, 5.0});

    // Hauteur crête de diagramme cible ~ 8% de la taille de structure
    m_diagramScale = std::clamp((0.08 * span) / maxVal, 1e-5, 10.0);
}

void ResultsVisualManager::setDiagramLabelsVisible(bool visible)
{
    if (m_diagramLabelsVisible == visible) return;
    m_diagramLabelsVisible = visible;
    updateDiagramShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::setReactionsVisible(bool visible)
{
    if (m_reactionsVisible == visible) return;
    m_reactionsVisible = visible;
    updateReactionShapes();
    emit visualStateChanged();
}

void ResultsVisualManager::startModalAnimation(int modeIndex, double speed)
{
    m_activeModalModeIndex = modeIndex;
    m_modalSpeed = speed > 0.0 ? speed : 1.0;
    m_modalPhase = 0.0;
    m_modalAnimationTimer.start(30); // ~33 FPS
    updateDeformedShapes();
}

void ResultsVisualManager::stopModalAnimation()
{
    m_modalAnimationTimer.stop();
    m_modalPhase = 0.0;
    updateDeformedShapes();
}

void ResultsVisualManager::onModalTimerTick()
{
    m_modalPhase += 0.12 * m_modalSpeed;
    if (m_modalPhase > 2.0 * M_PI)
    {
        m_modalPhase -= 2.0 * M_PI;
    }
    emit modalPhaseChanged(m_modalPhase);
    updateDeformedShapes();
}

void ResultsVisualManager::updateAllVisuals()
{
    updateDeformedShapes();
    updateDiagramShapes();
    updateReactionShapes();
    if (m_occView && m_occView->view())
    {
        m_occView->view()->Update();
    }
}

void ResultsVisualManager::clearAllVisuals()
{
    clearDeformedShapes();
    clearDiagramShapes();
    clearReactionShapes();
    if (m_occView && m_occView->view())
    {
        m_occView->view()->Update();
    }
}

void ResultsVisualManager::updateDeformedShapes()
{
    auto ctx = context();
    if (!ctx) return;

    clearDeformedShapes();

    if (!m_deformedVisible || !m_results || !m_results->isValid() || !m_model ||
        m_displayMode == DeformedDisplayMode::UndeformedOnly)
    {
        return;
    }

    bool isModal = isModalAnimationRunning() && !m_results->modalModes().empty();
    const TSA::Analysis::ModalMode* activeMode = nullptr;
    if (isModal)
    {
        for (const auto& m : m_results->modalModes())
        {
            if (m.modeNumber == m_activeModalModeIndex)
            {
                activeMode = &m;
                break;
            }
        }
        if (!activeMode && !m_results->modalModes().empty())
        {
            activeMode = &m_results->modalModes().front();
        }
    }

    // 1. Poutres et barres
    for (const auto& [beamId, beam] : m_model->beams())
    {
        const auto* n1 = m_model->getNode(beam.startNodeId());
        const auto* n2 = m_model->getNode(beam.endNodeId());
        if (!n1 || !n2) continue;

        gp_Pnt p1(n1->x(), n1->y(), n1->z());
        gp_Pnt p2(n2->x(), n2->y(), n2->z());

        TopoDS_Shape shape;

        if (activeMode)
        {
            auto it1 = activeMode->shape.find(beam.startNodeId());
            auto it2 = activeMode->shape.find(beam.endNodeId());
            TSA::Analysis::NodeDisplacement phi1 = (it1 != activeMode->shape.end()) ? it1->second : TSA::Analysis::NodeDisplacement{};
            TSA::Analysis::NodeDisplacement phi2 = (it2 != activeMode->shape.end()) ? it2->second : TSA::Analysis::NodeDisplacement{};

            shape = TSA::Geometry::DeformedGeometry::createModalDeformedBeamShape(
                p1, p2, phi1, phi2, beam.section(), m_deformationScale, m_modalPhase, beam.rotation()
            );
        }
        else
        {
            const auto* d1 = m_results->getNodeDisplacement(beam.startNodeId());
            const auto* d2 = m_results->getNodeDisplacement(beam.endNodeId());
            TSA::Analysis::NodeDisplacement disp1 = d1 ? *d1 : TSA::Analysis::NodeDisplacement{};
            TSA::Analysis::NodeDisplacement disp2 = d2 ? *d2 : TSA::Analysis::NodeDisplacement{};

            shape = TSA::Geometry::DeformedGeometry::createDeformedBeamShape(
                p1, p2, disp1, disp2, beam.section(), m_deformationScale, beam.rotation()
            );
        }

        if (!shape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(shape);
            ais->SetColor(Quantity_NOC_CYAN1);
            if (m_displayMode == DeformedDisplayMode::Both)
            {
                ais->SetTransparency(0.2);
            }
            ctx->Display(ais, Standard_False);
            m_deformedElementShapes[beamId] = ais;
        }
    }

    // 2. Poteaux
    for (const auto& [colId, col] : m_model->columns())
    {
        const auto* n1 = m_model->getNode(col.startNodeId());
        const auto* n2 = m_model->getNode(col.endNodeId());
        if (!n1 || !n2) continue;

        gp_Pnt p1(n1->x(), n1->y(), n1->z());
        gp_Pnt p2(n2->x(), n2->y(), n2->z());

        TopoDS_Shape shape;

        if (activeMode)
        {
            auto it1 = activeMode->shape.find(col.startNodeId());
            auto it2 = activeMode->shape.find(col.endNodeId());
            TSA::Analysis::NodeDisplacement phi1 = (it1 != activeMode->shape.end()) ? it1->second : TSA::Analysis::NodeDisplacement{};
            TSA::Analysis::NodeDisplacement phi2 = (it2 != activeMode->shape.end()) ? it2->second : TSA::Analysis::NodeDisplacement{};

            shape = TSA::Geometry::DeformedGeometry::createModalDeformedBeamShape(
                p1, p2, phi1, phi2, col.section(), m_deformationScale, m_modalPhase, col.rotation()
            );
        }
        else
        {
            const auto* d1 = m_results->getNodeDisplacement(col.startNodeId());
            const auto* d2 = m_results->getNodeDisplacement(col.endNodeId());
            TSA::Analysis::NodeDisplacement disp1 = d1 ? *d1 : TSA::Analysis::NodeDisplacement{};
            TSA::Analysis::NodeDisplacement disp2 = d2 ? *d2 : TSA::Analysis::NodeDisplacement{};

            shape = TSA::Geometry::DeformedGeometry::createDeformedBeamShape(
                p1, p2, disp1, disp2, col.section(), m_deformationScale, col.rotation()
            );
        }

        if (!shape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(shape);
            ais->SetColor(Quantity_NOC_CYAN1);
            if (m_displayMode == DeformedDisplayMode::Both)
            {
                ais->SetTransparency(0.2);
            }
            ctx->Display(ais, Standard_False);
            m_deformedElementShapes[100000 + colId] = ais;
        }
    }
}

void ResultsVisualManager::clearDeformedShapes()
{
    auto ctx = context();
    if (ctx)
    {
        for (auto& [id, shape] : m_deformedElementShapes)
        {
            ctx->Remove(shape, Standard_False);
        }
        for (auto& [id, shape] : m_deformedNodeShapes)
        {
            ctx->Remove(shape, Standard_False);
        }
    }
    m_deformedElementShapes.clear();
    m_deformedNodeShapes.clear();
}

void ResultsVisualManager::updateDiagramShapes()
{
    auto ctx = context();
    if (!ctx) return;

    clearDiagramShapes();

    if (m_diagramType == TSA::Geometry::DiagramType::None || !m_results || !m_results->isValid() || !m_model)
    {
        return;
    }

    for (const auto& [elId, elemRes] : m_results->allElementResults())
    {
        auto stations = getAllStations(elemRes);
        if (stations.empty()) continue;

        gp_Pnt p1, p2;
        double rot = 0.0;

        const auto* b = m_model->getBeam(elId);
        if (b)
        {
            const auto* n1 = m_model->getNode(b->startNodeId());
            const auto* n2 = m_model->getNode(b->endNodeId());
            if (!n1 || !n2) continue;
            p1 = gp_Pnt(n1->x(), n1->y(), n1->z());
            p2 = gp_Pnt(n2->x(), n2->y(), n2->z());
            rot = b->rotation();
        }
        else
        {
            const auto* col = m_model->getColumn(elId);
            if (col)
            {
                const auto* n1 = m_model->getNode(col->startNodeId());
                const auto* n2 = m_model->getNode(col->endNodeId());
                if (!n1 || !n2) continue;
                p1 = gp_Pnt(n1->x(), n1->y(), n1->z());
                p2 = gp_Pnt(n2->x(), n2->y(), n2->z());
                rot = col->rotation();
            }
            else continue;
        }

        TopoDS_Shape dShape = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, rot, stations, m_diagramType, m_diagramScale, true
        );

        if (!dShape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(dShape);
            // Couleurs : Rouge/Orange pour Moment, Bleu pour Effort Normal, Vert pour Tranchant
            if (m_diagramType == TSA::Geometry::DiagramType::BendingMy ||
                m_diagramType == TSA::Geometry::DiagramType::BendingMz)
            {
                ais->SetColor(Quantity_NOC_CORAL);
            }
            else if (m_diagramType == TSA::Geometry::DiagramType::AxialForceN)
            {
                ais->SetColor(Quantity_NOC_BLUE1);
            }
            else
            {
                ais->SetColor(Quantity_NOC_GREEN1);
            }
            ais->SetTransparency(0.25);
            ctx->Display(ais, Standard_False);
            m_diagramShapes[elId] = ais;
        }

        // Étiquettes de valeurs extrêmes
        if (m_diagramLabelsVisible)
        {
            double maxVal = -1e9;
            double minVal = 1e9;
            size_t maxIdx = 0;
            size_t minIdx = 0;

            for (size_t s = 0; s < stations.size(); ++s)
            {
                double v = TSA::Geometry::DiagramGeometry::getStationValue(stations[s], m_diagramType);
                if (v > maxVal) { maxVal = v; maxIdx = s; }
                if (v < minVal) { minVal = v; minIdx = s; }
            }

            gp_Vec offsetDir = TSA::Geometry::DiagramGeometry::getDiagramOffsetDirection(p1, p2, rot, m_diagramType);
            offsetDir.Normalize();
            gp_Vec vAB(p1, p2);
            double length = vAB.Magnitude();
            QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(m_diagramType);

            auto addLabel = [&](size_t idx, double val)
            {
                if (std::abs(val) < 1e-4) return;
                double pos = (length > 1e-6) ? (stations[idx].position / length) : 0.0;
                gp_Pnt base = p1.Translated(vAB * pos);
                gp_Pnt posLabel = base.Translated(offsetDir * (val * m_diagramScale + 0.08));

                Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
                lbl->SetPosition(posLabel);
                lbl->SetText(TCollection_ExtendedString(QString("%1 %2").arg(val, 0, 'f', 1).arg(unit).toUtf8().constData()));
                lbl->SetColor(Quantity_NOC_WHITE);
                lbl->SetHeight(12.0);
                lbl->SetFont("Arial");
                ctx->Display(lbl, Standard_False);
                m_diagramLabels[elId].push_back(lbl);
            };

            addLabel(maxIdx, maxVal);
            if (minIdx != maxIdx && std::abs(minVal - maxVal) > 0.1)
            {
                addLabel(minIdx, minVal);
            }
        }
    }
}

void ResultsVisualManager::clearDiagramShapes()
{
    auto ctx = context();
    if (ctx)
    {
        for (auto& [id, shape] : m_diagramShapes)
        {
            ctx->Remove(shape, Standard_False);
        }
        for (auto& [id, labels] : m_diagramLabels)
        {
            for (auto& lbl : labels)
            {
                ctx->Remove(lbl, Standard_False);
            }
        }
    }
    m_diagramShapes.clear();
    m_diagramLabels.clear();
}

void ResultsVisualManager::updateReactionShapes()
{
    auto ctx = context();
    if (!ctx) return;

    clearReactionShapes();

    if (!m_reactionsVisible || !m_results || !m_results->isValid() || !m_model)
    {
        return;
    }

    for (const auto& [nodeId, react] : m_results->allReactions())
    {
        const auto* n = m_model->getNode(nodeId);
        if (!n) continue;

        gp_Pnt p(n->x(), n->y(), n->z());
        gp_Vec fVec(react.rx, react.ry, react.rz);
        double fMag = fVec.Magnitude();
        if (fMag < 0.05) continue;

        // Longueur proportionnelle à la force, normalisée entre 0.4m et 1.2m
        double arrowLen = std::clamp(fMag * 0.05, 0.4, 1.2);
        // Flèche dirigée dans le sens de la réaction appliquée au nœud
        gp_Vec dir = fVec;
        dir.Normalize();

        TopoDS_Shape arrShape = makeArrowShape(p, dir, arrowLen, 0.025, 0.06, 0.15);
        if (!arrShape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(arrShape);
            ais->SetColor(Quantity_NOC_LIMEGREEN);
            ctx->Display(ais, Standard_False);
            m_reactionShapes[nodeId] = ais;
        }

        // Étiquette textuelle
        Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
        gp_Pnt labelPos = p.Translated(-dir * (arrowLen + 0.1));
        lbl->SetPosition(labelPos);
        lbl->SetText(TCollection_ExtendedString(QString("Rz: %1 kN").arg(react.rz, 0, 'f', 1).toUtf8().constData()));
        lbl->SetColor(Quantity_NOC_LIMEGREEN);
        lbl->SetHeight(12.0);
        ctx->Display(lbl, Standard_False);
        m_reactionLabels[nodeId] = lbl;
    }
}

void ResultsVisualManager::clearReactionShapes()
{
    auto ctx = context();
    if (ctx)
    {
        for (auto& [id, shape] : m_reactionShapes)
        {
            ctx->Remove(shape, Standard_False);
        }
        for (auto& [id, lbl] : m_reactionLabels)
        {
            ctx->Remove(lbl, Standard_False);
        }
    }
    m_reactionShapes.clear();
    m_reactionLabels.clear();
}

} // namespace TSA::Viewer
