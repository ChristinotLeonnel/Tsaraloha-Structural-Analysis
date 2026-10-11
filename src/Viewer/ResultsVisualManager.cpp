#include "ResultsVisualManager.h"
#include "MaterialVisual.h"
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

/// Section de l'élément de résultat (contraintes), nullptr si inconnue.
static const TSA::Model::Section* sectionOf(const TSA::Model::Model* m, const TSA::Analysis::ElementKey& key)
{
    if (!m) return nullptr;
    using K = TSA::Analysis::StructuralElementKind;
    switch (key.kind)
    {
    case K::Beam: { const auto* e = m->getBeam(key.id); return e ? &e->section() : nullptr; }
    case K::Column: { const auto* e = m->getColumn(key.id); return e ? &e->section() : nullptr; }
    case K::Truss: { const auto* e = m->getTrussMember(key.id); return e ? &e->section() : nullptr; }
    case K::Cable: { const auto* e = m->getCable(key.id); return e ? &e->section() : nullptr; }
    }
    return nullptr;
}

/// Stations à tracer : pour la contrainte normale, N est remplacé par σ (MPa) calculé avec la section.
static std::vector<TSA::Analysis::StationForces> displayStations(const TSA::Model::Model* m, const TSA::Analysis::ElementKey& key,
                                                                 const TSA::Analysis::ElementResults& el, TSA::Geometry::DiagramType type,
                                                                 bool kiloNewtons)
{
    auto stations = getAllStations(el);
    if (type != TSA::Geometry::DiagramType::StressSigma) return stations;
    const auto* sec = sectionOf(m, key);
    if (!sec) return {};
    for (auto& st : stations) st.N = TSA::Geometry::DiagramGeometry::normalStressMPa(st, sec->area(), sec->wy(), sec->wz(), kiloNewtons);
    return stations;
}
} // namespace

namespace TSA::Viewer
{

ResultsVisualManager::ResultsVisualManager(OccView* occView, QObject* parent)
    : QObject(parent)
    , m_occView(occView)
{
}

ResultsVisualManager::~ResultsVisualManager()
{
    m_shuttingDown = true; // la vue propriétaire est en cours de destruction : ne plus la piloter
    clearAllVisuals();
}

Handle(AIS_InteractiveContext) ResultsVisualManager::context() const
{
    return m_occView ? m_occView->context() : Handle(AIS_InteractiveContext)();
}

void ResultsVisualManager::setModel(TSA::Model::Model* model)
{
    m_model = model;
    m_deformedAxesValid = false;
}

void ResultsVisualManager::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    m_deformedAxesValid = false;
    if (m_results && m_results->isValid())
    {
        // Un facteur choisi par l'utilisateur (×1, ×100, manuel) est conservé d'un calcul à l'autre.
        if (m_deformationPreset == ScalePreset::Auto) autoComputeDeformationScale();
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
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setDeformedDisplayMode(DeformedDisplayMode mode)
{
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    updateDeformedShapes();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setDeformationScale(double scale)
{
    if (std::abs(m_deformationScale - scale) < 1e-4) return;
    m_deformationScale = scale;
    updateDeformedShapes();
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setAnimationPhase(double phase)
{
    phase = std::clamp(phase, 0.0, 1.0);
    if (std::abs(m_animationPhase - phase) < 1e-6) return;
    m_animationPhase = phase;
    if (m_deformedVisible) updateDeformedShapes();
    updateLegend(); // ligne « ANIMATION : … (pas une analyse dynamique) » pendant la lecture
    redrawView();
}

void ResultsVisualManager::setSectionRotationVisible(bool visible)
{
    if (m_sectionRotationVisible == visible) return;
    m_sectionRotationVisible = visible;
    if (m_deformationPreset == ScalePreset::Auto) autoComputeDeformationScale();
    updateDeformedShapes();
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

double ResultsVisualManager::maxTwist() const
{
    if (!m_deformedAxesValid) const_cast<ResultsVisualManager*>(this)->rebuildDeformedAxes();
    return m_maxTwist;
}

std::string ResultsVisualManager::maxTwistElement() const
{
    if (!m_deformedAxesValid) const_cast<ResultsVisualManager*>(this)->rebuildDeformedAxes();
    return m_maxTwistElement;
}

const TSA::Model::Section* ResultsVisualManager::memberSection(const TSA::Analysis::ElementKey& key, double* rotationDeg) const
{
    using TSA::Analysis::StructuralElementKind;
    if (!m_model) return nullptr;
    if (key.kind == StructuralElementKind::Beam)
    {
        if (const auto* b = m_model->getBeam(key.id)) { if (rotationDeg) *rotationDeg = b->rotation(); return &b->section(); }
    }
    else if (key.kind == StructuralElementKind::Column)
    {
        if (const auto* c = m_model->getColumn(key.id)) { if (rotationDeg) *rotationDeg = c->rotation(); return &c->section(); }
    }
    return nullptr;
}

void ResultsVisualManager::autoComputeDeformationScale()
{
    if (!m_results || !m_model || m_results->allDisplacements().empty()) return;

    // Plus grand déplacement réellement calculé : nœuds ET points le long des barres (une poutre sur
    // deux appuis n'a aucun déplacement nodal, sa flèche est en travée). Torsion : une section qui
    // tourne de θx déplace ses fibres extrêmes de |θx|·r ; une torsion pure ne déplace pas l'axe.
    double maxU = 0.0;
    for (const auto& [id, d] : m_results->allDisplacements())
    {
        double u = d.translationMagnitude();
        if (std::isfinite(u) && u > maxU) maxU = u;
    }
    if (!m_deformedAxesValid) rebuildDeformedAxes();
    double maxTwist = 0.0;
    for (const auto& [key, axis] : m_deformedAxes)
    {
        if (!axis.valid) continue;
        for (const auto& u : axis.displacement) maxU = std::max(maxU, u.Magnitude());
        if (!m_sectionRotationVisible || !axis.hasTwist()) continue;
        const auto* section = memberSection(key, nullptr);
        const double r = section ? TSA::Geometry::DeformedGeometry::sectionRadius(*section) : 0.0;
        const double t = axis.maxAbsTwist();
        maxTwist = std::max(maxTwist, t);
        maxU = std::max(maxU, t * r);
    }

    if (maxU < 1e-9)
    {
        m_deformationScale = 1.0; // résultats nuls : aucune déformée inventée
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
    double scale = 0.05 * span / maxU;
    // Rotation des sections affichée limitée à ≈ 30° : au-delà, la torsion amplifiée devient illisible.
    constexpr double kMaxDisplayedTwist = 0.5236;
    if (maxTwist > 1e-12) scale = std::min(scale, kMaxDisplayedTwist / maxTwist);
    m_deformationScale = std::clamp(scale, 1.0, 5000.0);
}

void ResultsVisualManager::setDeformationScalePreset(ScalePreset preset, double customVal)
{
    m_deformationPreset = preset;
    switch (preset)
    {
    case ScalePreset::Auto:
        autoComputeDeformationScale();
        updateDeformedShapes();
        updateLegend();
        redrawView();
        emit visualStateChanged();
        break;
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
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setDiagramScale(double scale)
{
    if (std::abs(m_diagramScale - scale) < 1e-6) return;
    m_diagramScale = scale;
    updateDiagramShapes();
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setDiagramScalePreset(ScalePreset preset, double customVal)
{
    m_diagramPreset = preset;
    switch (preset)
    {
    case ScalePreset::Auto:
        autoComputeDiagramScale();
        updateDiagramShapes();
        updateLegend();
        redrawView();
        emit visualStateChanged();
        break;
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
    m_deformedAxesValid = false;
    updateAllVisuals();
    emit visualStateChanged();
}

void ResultsVisualManager::setLegendVisible(bool visible)
{
    if (m_legendVisible == visible) return;
    m_legendVisible = visible;
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

QString ResultsVisualManager::legendSummaryText() const
{
    if (!m_results || !m_results->isValid()) return QString();

    QString text;
    if (m_deformedVisible)
    {
        const auto& sum = m_results->summary();
        const bool trueScale = std::abs(m_deformationScale - 1.0) < 1e-9;
        text += QString("DÉFORMÉE : δ_max = %1 mm (Nœud #%2) | Échelle ×%3 %4\n")
            .arg(sum.maxDisplacement * 1000.0, 0, 'f', 2)
            .arg(sum.maxDisplacementNodeId)
            .arg(m_deformationScale, 0, 'g', 4)
            .arg(trueScale ? QString("(échelle réelle)") : QString("(amplifiée pour l'affichage, résultats inchangés)"));
        if (m_animationPhase < 1.0 - 1e-9)
            text += QString("ANIMATION : %1 % du facteur ×%2 — interpolation visuelle de la déformée statique (pas une analyse dynamique)\n")
                        .arg(m_animationPhase * 100.0, 0, 'f', 0)
                        .arg(m_deformationScale, 0, 'g', 4);
        if (m_axesFromStations + m_axesFromHermite > 0)
            text += QString("Courbe des barres : %1 depuis les stations du solveur, %2 par interpolation cubique nodale\n")
                        .arg(m_axesFromStations).arg(m_axesFromHermite);
        if (m_axesInvalid > 0)
            text += QString("⚠ %1 barre(s) sans déformée (%2)\n")
                        .arg(m_axesInvalid).arg(QString::fromStdString(m_firstAxisProblem));
        if (m_maxTwist > 1e-12)
        {
            text += QString("TORSION : θx max = %1 mrad (%2) | %3\n")
                        .arg(m_maxTwist * 1000.0, 0, 'g', 4)
                        .arg(QString::fromStdString(m_maxTwistElement))
                        .arg(m_sectionRotationVisible ? QString("sections tournées ×%1").arg(m_deformationScale, 0, 'g', 4)
                                                      : QString("rotation des sections masquée"));
        }
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
    const bool kN = m_results->units().force == "kN";
    for (const auto& [id, el] : m_results->allElementResults())
    {
        auto stations = displayStations(m_model, id, el, m_diagramType, kN);
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
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setReactionsVisible(bool visible)
{
    if (m_reactionsVisible == visible) return;
    m_reactionsVisible = visible;
    updateReactionShapes();
    redrawView();
    emit visualStateChanged();
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
    redrawView();
}

void ResultsVisualManager::clearAllVisuals()
{
    clearDeformedShapes();
    applyStructureDisplay(false); // structure d'origine rétablie (opacité, visibilité)
    clearDiagramShapes();
    clearReactionShapes();
    clearExtremumMarker();
    clearLegend();
    redrawView();
}

void ResultsVisualManager::redrawView()
{
    // Les Display/Remove AIS sont faits sans mise à jour immédiate (false) : sans ce redessin,
    // une déformée ou un diagramme masqué reste à l'écran jusqu'au prochain rafraîchissement fortuit.
    if (m_occView && m_occView->view())
    {
        m_occView->view()->Update();
    }
}

void ResultsVisualManager::rebuildDeformedAxes()
{
    using TSA::Analysis::StructuralElementKind;
    m_deformedAxes.clear();
    m_axesFromStations = m_axesFromHermite = m_axesInvalid = 0;
    m_firstAxisProblem.clear();
    m_maxTwist = 0.0;
    m_maxTwistElement.clear();
    m_deformedAxesValid = true;
    if (!m_results || !m_results->isValid() || !m_model) return;

    auto build = [&](StructuralElementKind kind, int id, int startId, int endId, double beta, bool flexural) {
        const auto* n1 = m_model->getNode(startId);
        const auto* n2 = m_model->getNode(endId);
        if (!n1 || !n2) return;
        auto axis = TSA::Geometry::DeformedGeometry::computeMemberAxis(
            gp_Pnt(n1->x(), n1->y(), n1->z()), gp_Pnt(n2->x(), n2->y(), n2->z()), beta,
            m_results->getNodeDisplacement(startId), m_results->getNodeDisplacement(endId),
            m_results->getElementResults(kind, id), flexural);
        if (!axis.valid)
        {
            ++m_axesInvalid;
            if (m_firstAxisProblem.empty())
                m_firstAxisProblem = std::string(TSA::Analysis::elementKindName(kind)) + " " + std::to_string(id) + " : " + axis.problem;
        }
        else if (axis.source == TSA::Geometry::DeformedAxisSource::SolverStations) ++m_axesFromStations;
        else if (axis.source == TSA::Geometry::DeformedAxisSource::CubicHermite) ++m_axesFromHermite;
        if (axis.valid && axis.maxAbsTwist() > m_maxTwist)
        {
            m_maxTwist = axis.maxAbsTwist();
            m_maxTwistElement = std::string(TSA::Analysis::elementKindName(kind)) + " " + std::to_string(id);
        }
        m_deformedAxes[{ kind, id }] = std::move(axis);
    };

    for (const auto& [id, b] : m_model->beams()) build(StructuralElementKind::Beam, id, b.startNodeId(), b.endNodeId(), b.rotation(), true);
    for (const auto& [id, c] : m_model->columns()) build(StructuralElementKind::Column, id, c.startNodeId(), c.endNodeId(), c.rotation(), true);
    for (const auto& [id, t] : m_model->trussMembers()) build(StructuralElementKind::Truss, id, t.startNodeId(), t.endNodeId(), 0.0, false);
    for (const auto& [id, c] : m_model->cables()) build(StructuralElementKind::Cable, id, c.startNodeId(), c.endNodeId(), 0.0, false);
}

void ResultsVisualManager::applyStructureDisplay(bool deformedShown)
{
    if (!m_occView || m_shuttingDown) return;
    using SD = OccView::ResultsStructureDisplay;
    SD mode = SD::Normal;
    if (deformedShown)
        mode = m_displayMode == DeformedDisplayMode::DeformedOnly ? SD::Hidden : SD::Ghosted;
    m_occView->setResultsStructureDisplay(mode);
}

void ResultsVisualManager::updateDeformedShapes()
{
    using TSA::Analysis::StructuralElementKind;
    auto ctx = context();
    if (!ctx) return;

    clearDeformedShapes();

    const bool show = m_deformedVisible && m_results && m_results->isValid() && m_model &&
                      m_displayMode != DeformedDisplayMode::UndeformedOnly;
    applyStructureDisplay(show);
    if (!show) return;
    if (!m_deformedAxesValid) rebuildDeformedAxes();
    // Facteur affiché : amplification × phase d'animation (1 hors animation)
    const double displayScale = m_deformationScale * m_animationPhase;

    // Une barre = un objet continu (axe déformé balayé par la section) ; barres articulées : polyligne.
    for (const auto& [key, axis] : m_deformedAxes)
    {
        if (!axis.valid) continue; // signalé dans la légende, jamais remplacé par une forme inventée
        TopoDS_Shape shape;
        bool solid = false;
        int mapKey = key.id;
        if (key.kind == StructuralElementKind::Beam || key.kind == StructuralElementKind::Column)
        {
            if (key.kind == StructuralElementKind::Column) mapKey = 100000 + key.id;
            double rotation = 0.0;
            const TSA::Model::Section* section = memberSection(key, &rotation);
            if (section)
                shape = TSA::Geometry::DeformedGeometry::createMemberSolid(axis, *section, rotation, displayScale,
                                                                           m_sectionRotationVisible);
            solid = !shape.IsNull();
            if (!solid) shape = TSA::Geometry::DeformedGeometry::createAxisWire(axis, displayScale);
            // Torsion : contours de section et génératrice (rotation visible même pour une section ronde).
            if (section && m_sectionRotationVisible && axis.hasTwist()
                && axis.maxAbsTwist() > std::max(1e-12, 1e-3 * m_maxTwist))
            {
                const TopoDS_Shape markers = TSA::Geometry::DeformedGeometry::createSectionMarkers(
                    axis, *section, rotation, displayScale, true);
                if (!markers.IsNull())
                {
                    Handle(AIS_Shape) m = new AIS_Shape(markers);
                    m->SetColor(Quantity_NOC_ORANGERED);
                    m->SetWidth(2.0);
                    ctx->Display(m, false);
                    m_twistMarkerShapes.push_back(m);
                }
            }
        }
        else
        {
            mapKey = (key.kind == StructuralElementKind::Truss ? 200000 : 300000) + key.id;
            shape = TSA::Geometry::DeformedGeometry::createAxisWire(axis, displayScale);
        }
        if (shape.IsNull()) continue;

        Handle(AIS_Shape) ais = new AIS_Shape(shape);
        // La déformée garde l'apparence de l'élément : matériau (couleur, aspect, texture) ou couleur
        // choisie par l'utilisateur dans les propriétés. Auparavant : cyan uniforme, câbles orange.
        const TSA::Model::Material* material = nullptr;
        std::string userColor;
        switch (key.kind)
        {
        case StructuralElementKind::Beam:
            if (const auto* b = m_model->getBeam(key.id)) { material = &b->material(); userColor = b->color(); }
            break;
        case StructuralElementKind::Column:
            if (const auto* c = m_model->getColumn(key.id)) { material = &c->material(); userColor = c->color(); }
            break;
        case StructuralElementKind::Truss:
            if (const auto* t = m_model->getTrussMember(key.id)) { material = &t->material(); userColor = t->color(); }
            break;
        case StructuralElementKind::Cable:
            if (const auto* k = m_model->getCable(key.id)) { material = &k->material(); userColor = k->color(); }
            break;
        default:
            break;
        }
        if (material && solid)
            TSA::Viewer::MaterialVisual::instance().applyToShape(ais, *material, userColor, m_occView->renderDisplayMode());
        else if (material)
            ais->SetColor(TSA::Viewer::MaterialVisual::instance().getOcctColor(*material, userColor));
        else
            ais->SetColor(key.kind == StructuralElementKind::Cable ? Quantity_NOC_ORANGE : Quantity_NOC_CYAN1);
        if (solid)
            ais->SetDisplayMode(AIS_Shaded);
        else
            ais->SetWidth(key.kind == StructuralElementKind::Cable ? 2.0 : 2.5);
        ctx->Display(ais, false);
        m_deformedElementShapes[mapKey] = ais;
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
        for (auto& shape : m_twistMarkerShapes) ctx->Remove(shape, false);
    }
    m_deformedElementShapes.clear();
    m_deformedNodeShapes.clear();
    m_twistMarkerShapes.clear();
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

    const bool kN = m_results->units().force == "kN";
    for (const auto& [elKey, elemRes] : m_results->allElementResults())
    {
        const int elId = elKey.id;
        auto stations = displayStations(m_model, elKey, elemRes, m_diagramType, kN);
        if (stations.empty()) continue;

        gp_Pnt p1, p2;
        double rot = 0.0;

        const auto* b = elKey.kind == TSA::Analysis::StructuralElementKind::Beam ? m_model->getBeam(elId) : nullptr;
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
            const auto* col = elKey.kind == TSA::Analysis::StructuralElementKind::Column ? m_model->getColumn(elId) : nullptr;
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
                const auto* truss = elKey.kind == TSA::Analysis::StructuralElementKind::Truss ? m_model->getTrussMember(elId) : nullptr;
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
                    const auto* cable = elKey.kind == TSA::Analysis::StructuralElementKind::Cable ? m_model->getCable(elId) : nullptr;
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
            else if (m_diagramType == TSA::Geometry::DiagramType::StressSigma)
            {
                ais->SetColor(Quantity_NOC_PURPLE);
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
