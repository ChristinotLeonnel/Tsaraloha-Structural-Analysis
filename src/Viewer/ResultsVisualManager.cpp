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
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include "../NDC/ResultAnalyzer.h"
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

void ResultsVisualManager::setDeformationScalePreset(ScalePreset preset, double customVal)
{
    m_deformationPreset = preset;
    switch (preset)
    {
    case ScalePreset::Auto:   autoComputeDeformationScale(); break;
    case ScalePreset::X1:     setDeformationScale(1.0); break;
    case ScalePreset::X10:    setDeformationScale(10.0); break;
    case ScalePreset::X100:   setDeformationScale(100.0); break;
    case ScalePreset::X1000:  setDeformationScale(1000.0); break;
    case ScalePreset::X10000: setDeformationScale(10000.0); break;
    case ScalePreset::Custom: setDeformationScale(customVal > 0.0 ? customVal : 1.0); break;
    }
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

void ResultsVisualManager::setDiagramScalePreset(ScalePreset preset, double customVal)
{
    m_diagramPreset = preset;
    switch (preset)
    {
    case ScalePreset::Auto:   autoComputeDiagramScale(); break;
    case ScalePreset::X1:     setDiagramScale(1.0); break;
    case ScalePreset::X10:    setDiagramScale(10.0); break;
    case ScalePreset::X100:   setDiagramScale(100.0); break;
    case ScalePreset::X1000:  setDiagramScale(1000.0); break;
    case ScalePreset::X10000: setDiagramScale(10000.0); break;
    case ScalePreset::Custom: setDiagramScale(customVal > 0.0 ? customVal : 0.05); break;
    }
}

int ResultsVisualManager::activeStep() const
{
    return m_results ? m_results->activeStep() : -1;
}

void ResultsVisualManager::setActiveStep(int step)
{
    if (!m_results) return;
    m_results->setActiveStep(step);
    updateAllVisuals();
    emit visualStateChanged();
}

void ResultsVisualManager::setLegendVisible(bool visible)
{
    if (m_legendVisible == visible) return;
    m_legendVisible = visible;
    updateLegend();
    emit visualStateChanged();
}

QString ResultsVisualManager::legendSummaryText() const
{
    if (!m_results || !m_results->isValid()) return QString();

    QString text;
    if (m_deformedVisible)
    {
        const auto& sum = m_results->summary();
        text += QString("DÉFORMÉE : δ_max = %1 mm (Nœud #%2) | Échelle ×%3\n")
            .arg(sum.maxDisplacement * 1000.0, 0, 'f', 2)
            .arg(sum.maxDisplacementNodeId)
            .arg(m_deformationScale, 0, 'f', 1);
    }
    if (m_diagramType != TSA::Geometry::DiagramType::None)
    {
        text += QString("DIAGRAMME : %1 (%2) | Échelle ×%3\n")
            .arg(TSA::Geometry::DiagramGeometry::diagramTypeName(m_diagramType))
            .arg(TSA::Geometry::DiagramGeometry::diagramUnit(m_diagramType))
            .arg(m_diagramScale, 0, 'f', 4);
    }
    return text;
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

void ResultsVisualManager::clearLegend()
{
    auto ctx = context();
    if (ctx && !m_legendLabel.IsNull())
    {
        ctx->Remove(m_legendLabel, false);
    }
    m_legendLabel.Nullify();
}

void ResultsVisualManager::updateLegend()
{
    auto ctx = context();
    if (!ctx) return;

    clearLegend();
    if (!m_legendVisible || !m_results || !m_results->isValid()) return;

    QString txt = legendSummaryText();
    if (txt.isEmpty()) return;

    double minX = 0, minY = 0, maxZ = 0;
    if (m_model && !m_model->nodes().empty())
    {
        minX = minY = 1e9;
        maxZ = -1e9;
        for (const auto& [id, n] : m_model->nodes())
        {
            minX = std::min(minX, n.x());
            minY = std::min(minY, n.y());
            maxZ = std::max(maxZ, n.z());
        }
    }

    gp_Pnt legendPos(minX, minY, maxZ + 0.4);

    m_legendLabel = new AIS_TextLabel();
    m_legendLabel->SetPosition(legendPos);
    m_legendLabel->SetText(TCollection_ExtendedString(txt.toUtf8().constData(), true));
    m_legendLabel->SetColor(Quantity_NOC_YELLOW);
    m_legendLabel->SetHeight(13.0);
    m_legendLabel->SetFont("Arial");
    ctx->Display(m_legendLabel, false);
}

void ResultsVisualManager::updateAllVisuals()
{
    updateDeformedShapes();
    updateDiagramShapes();
    updateReactionShapes();
    updateLegend();
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
    clearExtremumMarker();
    clearLegend();
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
            ctx->Display(ais, false);
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
            ctx->Display(ais, false);
            m_deformedElementShapes[100000 + colId] = ais;
        }
    }

    // 3. Treillis (Truss members)
    for (const auto& [trussId, truss] : m_model->trussMembers())
    {
        const auto* n1 = m_model->getNode(truss.startNodeId());
        const auto* n2 = m_model->getNode(truss.endNodeId());
        if (!n1 || !n2) continue;

        gp_Pnt p1(n1->x(), n1->y(), n1->z());
        gp_Pnt p2(n2->x(), n2->y(), n2->z());

        const auto* d1 = m_results->getNodeDisplacement(truss.startNodeId());
        const auto* d2 = m_results->getNodeDisplacement(truss.endNodeId());
        TSA::Analysis::NodeDisplacement disp1 = d1 ? *d1 : TSA::Analysis::NodeDisplacement{};
        TSA::Analysis::NodeDisplacement disp2 = d2 ? *d2 : TSA::Analysis::NodeDisplacement{};

        gp_Pnt defP1 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p1, disp1, m_deformationScale);
        gp_Pnt defP2 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p2, disp2, m_deformationScale);

        try
        {
            BRepBuilderAPI_MakeEdge edge(defP1, defP2);
            if (edge.IsDone())
            {
                Handle(AIS_Shape) ais = new AIS_Shape(edge.Shape());
                ais->SetColor(Quantity_NOC_CYAN1);
                ais->SetWidth(2.5);
                if (m_displayMode == DeformedDisplayMode::Both)
                {
                    ais->SetTransparency(0.2);
                }
                ctx->Display(ais, false);
                m_deformedElementShapes[200000 + trussId] = ais;
            }
        }
        catch (...) {}
    }

    // 4. Câbles
    for (const auto& [cableId, cable] : m_model->cables())
    {
        const auto* n1 = m_model->getNode(cable.startNodeId());
        const auto* n2 = m_model->getNode(cable.endNodeId());
        if (!n1 || !n2) continue;

        gp_Pnt p1(n1->x(), n1->y(), n1->z());
        gp_Pnt p2(n2->x(), n2->y(), n2->z());

        const auto* d1 = m_results->getNodeDisplacement(cable.startNodeId());
        const auto* d2 = m_results->getNodeDisplacement(cable.endNodeId());
        TSA::Analysis::NodeDisplacement disp1 = d1 ? *d1 : TSA::Analysis::NodeDisplacement{};
        TSA::Analysis::NodeDisplacement disp2 = d2 ? *d2 : TSA::Analysis::NodeDisplacement{};

        gp_Pnt defP1 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p1, disp1, m_deformationScale);
        gp_Pnt defP2 = TSA::Geometry::DeformedGeometry::computeDeformedPoint(p2, disp2, m_deformationScale);

        try
        {
            BRepBuilderAPI_MakeEdge edge(defP1, defP2);
            if (edge.IsDone())
            {
                Handle(AIS_Shape) ais = new AIS_Shape(edge.Shape());
                ais->SetColor(Quantity_NOC_ORANGE);
                ais->SetWidth(2.0);
                if (m_displayMode == DeformedDisplayMode::Both)
                {
                    ais->SetTransparency(0.2);
                }
                ctx->Display(ais, false);
                m_deformedElementShapes[300000 + cableId] = ais;
            }
        }
        catch (...) {}
    }
}

void ResultsVisualManager::clearDeformedShapes()
{
    auto ctx = context();
    if (ctx)
    {
        for (auto& [id, shape] : m_deformedElementShapes)
        {
            ctx->Remove(shape, false);
        }
        for (auto& [id, shape] : m_deformedNodeShapes)
        {
            ctx->Remove(shape, false);
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
            else
            {
                const auto* truss = m_model->getTrussMember(elId);
                if (truss)
                {
                    const auto* n1 = m_model->getNode(truss->startNodeId());
                    const auto* n2 = m_model->getNode(truss->endNodeId());
                    if (!n1 || !n2) continue;
                    p1 = gp_Pnt(n1->x(), n1->y(), n1->z());
                    p2 = gp_Pnt(n2->x(), n2->y(), n2->z());
                    rot = 0.0;
                }
                else
                {
                    const auto* cable = m_model->getCable(elId);
                    if (cable)
                    {
                        const auto* n1 = m_model->getNode(cable->startNodeId());
                        const auto* n2 = m_model->getNode(cable->endNodeId());
                        if (!n1 || !n2) continue;
                        p1 = gp_Pnt(n1->x(), n1->y(), n1->z());
                        p2 = gp_Pnt(n2->x(), n2->y(), n2->z());
                        rot = 0.0;
                    }
                    else continue;
                }
            }
        }

        TopoDS_Shape dShape = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, rot, stations, m_diagramType, m_diagramScale, true
        );

        if (!dShape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(dShape);
            // Couleurs par famille :
            // Moment (M) : Coral / Orange
            // Effort Normal (N) : Bleu
            // Effort Tranchant (V) : Vert
            // Flèche / Déplacement (U) : Magenta
            // Rotation (R) : Or / Jaune
            if (m_diagramType == TSA::Geometry::DiagramType::BendingMy ||
                m_diagramType == TSA::Geometry::DiagramType::BendingMz ||
                m_diagramType == TSA::Geometry::DiagramType::TorsionMx)
            {
                ais->SetColor(Quantity_NOC_CORAL);
            }
            else if (m_diagramType == TSA::Geometry::DiagramType::AxialForceN)
            {
                ais->SetColor(Quantity_NOC_BLUE1);
            }
            else if (m_diagramType == TSA::Geometry::DiagramType::ShearForceVy ||
                     m_diagramType == TSA::Geometry::DiagramType::ShearForceVz)
            {
                ais->SetColor(Quantity_NOC_GREEN1);
            }
            else if (m_diagramType == TSA::Geometry::DiagramType::DeflectionUx ||
                     m_diagramType == TSA::Geometry::DiagramType::DeflectionUy ||
                     m_diagramType == TSA::Geometry::DiagramType::DeflectionUz ||
                     m_diagramType == TSA::Geometry::DiagramType::DeflectionUres)
            {
                ais->SetColor(Quantity_NOC_MAGENTA1);
            }
            else
            {
                ais->SetColor(Quantity_NOC_GOLD);
            }
            ais->SetTransparency(0.25);
            ctx->Display(ais, false);
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
            if (offsetDir.Magnitude() < 1e-6) continue;
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
                ctx->Display(lbl, false);
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
            ctx->Remove(shape, false);
        }
        for (auto& [id, labels] : m_diagramLabels)
        {
            for (auto& lbl : labels)
            {
                ctx->Remove(lbl, false);
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
            ctx->Display(ais, false);
            m_reactionShapes[nodeId] = ais;
        }

        // Étiquette textuelle
        Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
        gp_Pnt labelPos = p.Translated(-dir * (arrowLen + 0.1));
        lbl->SetPosition(labelPos);
        QString lblStr;
        if (std::abs(react.rx) < 0.1 && std::abs(react.ry) < 0.1)
        {
            lblStr = QString("Rz: %1 kN").arg(react.rz, 0, 'f', 1);
        }
        else
        {
            lblStr = QString("R: %1 kN (Rx=%2, Ry=%3, Rz=%4)")
                .arg(fMag, 0, 'f', 1)
                .arg(react.rx, 0, 'f', 1)
                .arg(react.ry, 0, 'f', 1)
                .arg(react.rz, 0, 'f', 1);
        }
        lbl->SetText(TCollection_ExtendedString(lblStr.toUtf8().constData()));
        lbl->SetColor(Quantity_NOC_LIMEGREEN);
        lbl->SetHeight(12.0);
        ctx->Display(lbl, false);
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
            ctx->Remove(shape, false);
        }
        for (auto& [id, lbl] : m_reactionLabels)
        {
            ctx->Remove(lbl, false);
        }
    }
    m_reactionShapes.clear();
    m_reactionLabels.clear();
}

void ResultsVisualManager::showExtremumMarker(const TSA::NDC::ExtremumPoint& pt)
{
    auto ctx = context();
    if (!ctx) return;

    clearExtremumMarker();

    // 1. Marqueur 3D : sphère rouge vive positionnée aux coordonnées globales réelles
    double radius = 0.12; // 12 cm de rayon
    BRepPrimAPI_MakeSphere sphere(pt.globalCoords, radius);
    TopoDS_Shape shape = sphere.Shape();

    m_extremumMarkerShape = new AIS_Shape(shape);
    m_extremumMarkerShape->SetDisplayMode(AIS_Shaded);
    m_extremumMarkerShape->SetColor(Quantity_NOC_RED);
    m_extremumMarkerShape->SetMaterial(Graphic3d_NOM_PLASTIC);

    ctx->Display(m_extremumMarkerShape, false);

    // 2. Étiquette textuelle 3D flottante avec détails précis
    m_extremumLabel = new AIS_TextLabel();
    QString labelText = QString("%1: %2 %3\nx = %4 m (ID: %5)")
                            .arg(pt.quantityName)
                            .arg(pt.value, 0, 'f', 2)
                            .arg(pt.unit)
                            .arg(pt.localPositionX, 0, 'f', 2)
                            .arg(pt.elementId);
    if (!pt.loadCaseOrCombo.isEmpty())
    {
        labelText += QString(" [%1]").arg(pt.loadCaseOrCombo);
    }

    m_extremumLabel->SetText(TCollection_ExtendedString(labelText.toUtf8().constData(), true));
    gp_Pnt labelPos = pt.globalCoords.Translated(gp_Vec(0.0, 0.0, radius * 2.5));
    m_extremumLabel->SetPosition(labelPos);
    m_extremumLabel->SetColor(Quantity_NOC_YELLOW);
    m_extremumLabel->SetHeight(15.0);

    ctx->Display(m_extremumLabel, false);

    // 3. Mise en surbrillance de l'élément porteur dans OccView et centrage caméra
    if (m_occView && pt.elementId > 0)
    {
        if (pt.elementType == "Poutre")
        {
            m_occView->highlightBeam(pt.elementId);
        }
        else if (pt.elementType == "Poteau")
        {
            m_occView->highlightColumn(pt.elementId);
        }
        else if (pt.elementType == "Treillis")
        {
            m_occView->highlightTrussMember(pt.elementId);
        }
        else if (pt.elementType == "Câble")
        {
            m_occView->highlightCable(pt.elementId);
        }
        else
        {
            m_occView->highlightBeam(pt.elementId);
        }
        m_occView->fitSelection();
    }

    if (m_occView && m_occView->view())
    {
        m_occView->view()->Update();
    }
}

void ResultsVisualManager::clearExtremumMarker()
{
    auto ctx = context();
    if (!ctx) return;

    if (!m_extremumMarkerShape.IsNull())
    {
        ctx->Remove(m_extremumMarkerShape, false);
        m_extremumMarkerShape.Nullify();
    }
    if (!m_extremumLabel.IsNull())
    {
        ctx->Remove(m_extremumLabel, false);
        m_extremumLabel.Nullify();
    }
    if (m_occView && m_occView->view())
    {
        m_occView->view()->Update();
    }
}

} // namespace TSA::Viewer
