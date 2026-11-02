#include "ResultsVisualManager.h"
#include "LoadDisplayStyle.h"
#include "AnnotationStyle.h"
#include "MaterialVisual.h"
#include "OccView.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Section.h"

#include <Graphic3d_TransformPers.hxx>
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
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_PointAspect.hxx>
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

namespace
{
constexpr double kLeaderLength = 0.3;   ///< m : écart minimal entre un texte et son point de courbe (ligne de rappel lisible)

/// Couleur d'une grandeur : distincte d'une grandeur à l'autre (Vy et Vz, My et Mz affichés ensemble se reconnaissent).
Quantity_Color diagramColor(TSA::Geometry::DiagramType type)
{
    using T = TSA::Geometry::DiagramType;
    switch (type)
    {
    case T::BendingMy: return Quantity_Color(Quantity_NOC_CORAL);
    case T::BendingMz: return Quantity_Color(Quantity_NOC_ORANGE);
    case T::TorsionMx: return Quantity_Color(Quantity_NOC_GOLD);
    case T::StressSigma: return Quantity_Color(Quantity_NOC_PURPLE);
    case T::AxialForceN: return Quantity_Color(Quantity_NOC_DODGERBLUE1);
    case T::ShearForceVy: return Quantity_Color(Quantity_NOC_GREEN1);
    case T::ShearForceVz: return Quantity_Color(Quantity_NOC_TURQUOISE);
    case T::DeflectionUx:
    case T::DeflectionUy:
    case T::DeflectionUz:
    case T::DeflectionUres: return Quantity_Color(Quantity_NOC_MAGENTA1);
    default: return Quantity_Color(Quantity_NOC_YELLOW);
    }
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
    clearExtremumMarker();   // marqueur d'un autre calcul : jamais conservé
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

void ResultsVisualManager::setDeformedRepresentation(DeformedRepresentation representation)
{
    if (m_deformedRepresentation == representation) return;
    m_deformedRepresentation = representation;
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
    setDiagramTypes(type == TSA::Geometry::DiagramType::None ? std::vector<TSA::Geometry::DiagramType>{}
                                                             : std::vector<TSA::Geometry::DiagramType>{ type });
}

std::vector<TSA::Geometry::DiagramType> ResultsVisualManager::diagramTypes() const
{
    std::vector<TSA::Geometry::DiagramType> types;
    for (const auto& layer : m_layers) types.push_back(layer.type);
    return types;
}

bool ResultsVisualManager::isDiagramVisible(TSA::Geometry::DiagramType type) const
{
    return std::any_of(m_layers.begin(), m_layers.end(), [type](const DiagramLayer& layer) { return layer.type == type; });
}

void ResultsVisualManager::setDiagramTypes(const std::vector<TSA::Geometry::DiagramType>& types)
{
    std::vector<TSA::Geometry::DiagramType> wanted;
    for (const auto type : types)
        if (type != TSA::Geometry::DiagramType::None && std::find(wanted.begin(), wanted.end(), type) == wanted.end()) wanted.push_back(type);
    if (wanted == diagramTypes()) return;
    m_layers.clear();
    for (const auto type : wanted)
    {
        DiagramLayer layer;
        layer.type = type;
        m_layers.push_back(layer);
    }
    autoComputeDiagramScale();
    refreshDiagrams();
}

void ResultsVisualManager::setDiagramVisible(TSA::Geometry::DiagramType type, bool visible)
{
    auto types = diagramTypes();
    const auto it = std::find(types.begin(), types.end(), type);
    if (visible == (it != types.end())) return;
    if (visible) types.push_back(type);
    else types.erase(it);
    setDiagramTypes(types);
}

void ResultsVisualManager::syncActiveDiagram()
{
    for (auto& layer : m_layers) layer.scale = layer.autoScale * m_diagramMultiplier;
    if (m_layers.empty())
    {
        m_diagramType = TSA::Geometry::DiagramType::None;
        return;
    }
    const DiagramLayer& active = m_layers.back();
    m_diagramType = active.type;
    m_diagramAutoScale = active.autoScale;
    m_diagramScale = active.scale;
    m_diagramMaxValue = active.maxValue;
    m_diagramAllZero = active.allZero;
}

void ResultsVisualManager::refreshDiagrams()
{
    updateDiagramShapes();
    updateLegend();
    redrawView();
    emit visualStateChanged();
}

void ResultsVisualManager::setDiagramScaleMultiplier(double multiplier)
{
    if (!(multiplier > 0.0) || !std::isfinite(multiplier)) multiplier = 1.0;
    m_diagramMultiplier = multiplier;
    syncActiveDiagram();
    refreshDiagrams();
}

void ResultsVisualManager::setDiagramScale(double scale)
{
    // Échelle absolue (m par unité) : enregistrée comme multiplicateur de l'échelle automatique
    if (!(scale > 0.0) || std::abs(m_diagramScale - scale) <= 1e-12 * std::max(1.0, std::abs(scale))) return;
    m_diagramScale = scale;
    m_diagramMultiplier = m_diagramAutoScale > 0.0 ? scale / m_diagramAutoScale : 1.0;
    syncActiveDiagram();
    refreshDiagrams();
}

void ResultsVisualManager::setDiagramScalePreset(ScalePreset preset, double customVal)
{
    // Diagrammes : multiplicateur de l'échelle automatique (une échelle « ×100 » absolue n'a pas de sens pour une
    // grandeur en kN·m ; elle était auparavant partagée avec l'amplification de la déformée).
    m_diagramPreset = preset;
    autoComputeDiagramScale();
    switch (preset)
    {
    case ScalePreset::Auto:   setDiagramScaleMultiplier(1.0); break;
    case ScalePreset::X1:     setDiagramScaleMultiplier(1.0); break;
    case ScalePreset::X10:    setDiagramScaleMultiplier(10.0); break;
    case ScalePreset::X100:   setDiagramScaleMultiplier(100.0); break;
    case ScalePreset::X1000:  setDiagramScaleMultiplier(1000.0); break;
    case ScalePreset::X10000: setDiagramScaleMultiplier(10000.0); break;
    case ScalePreset::Custom: setDiagramScaleMultiplier(customVal > 0.0 ? customVal : 1.0); break;
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
    {
        // Calcul affiché : jamais un résultat d'un autre calcul présenté comme courant
        const auto& meta = m_results->executionMetadata();
        QString loading = QString::fromStdString(meta.loadCombinationType);
        if (!m_results->caseOrComboName().empty()) loading += QStringLiteral(" « %1 »").arg(QString::fromStdString(m_results->caseOrComboName()));
        text += tr("CALCUL : %1 — %2 — %3\n").arg(QString::fromStdString(meta.solverEngine), loading, QString::fromStdString(m_results->timestamp()));
        const int step = m_results->activeStep(), steps = m_results->stepCount();
        if (m_results->analysisType() == TSA::Analysis::AnalysisType::Modal && !m_results->modal().empty())
        {
            const int k = std::clamp(step < 0 ? 0 : step, 0, int(m_results->modal().modes.size()) - 1);
            text += tr("MODE %1 / %2 : f = %3 Hz (forme propre normalisée)\n").arg(k + 1).arg(m_results->modal().modes.size()).arg(m_results->modal().modes[std::size_t(k)].frequency, 0, 'f', 3);
        }
        else if (steps > 1)
            text += tr("PAS %1 / %2\n").arg(step < 0 ? steps : step + 1).arg(steps);
    }
    if (m_deformedVisible)
    {
        const auto& sum = m_results->summary();
        const bool trueScale = std::abs(m_deformationScale - 1.0) < 1e-9;
        text += tr("DÉFORMÉE : δ_max = %1 mm (Nœud #%2) | Échelle ×%3 %4\n")
            .arg(sum.maxDisplacement * 1000.0, 0, 'f', 2)
            .arg(sum.maxDisplacementNodeId)
            .arg(m_deformationScale, 0, 'g', 4)
            .arg(trueScale ? tr("(échelle réelle)") : tr("(amplifiée pour l'affichage, résultats inchangés)"));
        if (m_animationPhase < 1.0 - 1e-9)
            text += tr("ANIMATION : %1 % du facteur ×%2 — interpolation visuelle de la déformée statique (pas une analyse dynamique)\n")
                        .arg(m_animationPhase * 100.0, 0, 'f', 0)
                        .arg(m_deformationScale, 0, 'g', 4);
        if (m_axesFromStations + m_axesFromHermite > 0)
            text += tr("Courbe des barres : %1 depuis les stations du solveur, %2 par interpolation cubique nodale\n")
                        .arg(m_axesFromStations).arg(m_axesFromHermite);
        if (m_axesInvalid > 0)
            text += tr("⚠ %1 barre(s) sans déformée (%2)\n")
                        .arg(m_axesInvalid).arg(QString::fromStdString(m_firstAxisProblem));
        if (m_maxTwist > 1e-12)
        {
            text += tr("TORSION : θx max = %1 mrad (%2) | %3\n")
                        .arg(m_maxTwist * 1000.0, 0, 'g', 4)
                        .arg(QString::fromStdString(m_maxTwistElement))
                        .arg(m_sectionRotationVisible ? tr("sections tournées ×%1").arg(m_deformationScale, 0, 'g', 4)
                                                      : tr("rotation des sections masquée"));
        }
    }
    for (const DiagramLayer& layer : m_layers)
    {
        const QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(layer.type, !m_results || m_results->units().force != "N");
        if (layer.allZero)
            text += tr("DIAGRAMME : %1 — grandeur nulle sur toute la structure (aucun diagramme)\n")
                        .arg(TSA::Geometry::DiagramGeometry::diagramTypeName(layer.type));
        else
            text += tr("DIAGRAMME : %1 (%2) | max |%3| %2 | 1 m de diagramme = %4 %2%5\n")
                        .arg(TSA::Geometry::DiagramGeometry::diagramTypeName(layer.type))
                        .arg(unit)
                        .arg(layer.maxValue, 0, 'g', 4)
                        .arg(layer.scale > 0.0 ? 1.0 / layer.scale : 0.0, 0, 'g', 4)
                        .arg(std::abs(m_diagramMultiplier - 1.0) < 1e-12 ? tr(" (échelle automatique)") : tr(" (automatique ×%1)").arg(m_diagramMultiplier, 0, 'g', 3));
    }
    if (!m_layers.empty() && m_hiddenDiagramLabels > 0)
        text += tr("%1 étiquette(s) superposée(s) non affichée(s) (valeur égale ou inférieure à une étiquette voisine)\n").arg(m_hiddenDiagramLabels);
    return text;
}

void ResultsVisualManager::autoComputeDiagramScale()
{
    if (!m_results || !m_model)
    {
        syncActiveDiagram();
        return;
    }
    for (auto& layer : m_layers) computeLayerScale(layer);
    syncActiveDiagram();
}

void ResultsVisualManager::computeLayerScale(DiagramLayer& layer) const
{
    double maxVal = 0.0;
    const bool kN = m_results->units().force == "kN";
    for (const auto& [id, el] : m_results->allElementResults())
    {
        auto stations = displayStations(m_model, id, el, layer.type, kN);
        for (const auto& st : stations)
        {
            double v = std::abs(TSA::Geometry::DiagramGeometry::getStationValue(st, layer.type));
            if (v > maxVal) maxVal = v;
        }
    }

    layer.maxValue = maxVal;
    layer.allZero = !(maxVal > 0.0);
    if (layer.allZero)
    {
        // Grandeur nulle partout : aucun diagramme arbitraire (message dans la légende)
        layer.autoScale = 0.0;
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

    // Hauteur crête cible : 8 % de l'envergure, sans dépasser 60 % de la longueur médiane des barres (diagrammes qui
    // ne recouvrent pas la structure). Aucun plafond sur l'échelle elle-même : une rotation de
    // 1e-3 rad donne un diagramme aussi lisible qu'un moment de 100 kN·m (l'ancien plafond ×10 la réduisait à quelques millimètres).
    std::vector<double> lengths;
    for (const auto& [id, el] : m_results->allElementResults())
        if (el.length > 0.0) lengths.push_back(el.length);
    double target = 0.08 * span;
    if (!lengths.empty())
    {
        std::nth_element(lengths.begin(), lengths.begin() + lengths.size() / 2, lengths.end());
        target = std::min(target, 0.6 * lengths[lengths.size() / 2]);
    }
    layer.autoScale = target / maxVal;
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

    // Légende fixe à l'écran (coin inférieur droit, zone libre : cube de navigation en haut à droite, légende des charges
    // en haut à gauche) : ne recouvre plus la structure et ne suit plus la caméra ; taille constante quel que soit le mode
    m_legendLabel = new AIS_TextLabel();
    m_legendLabel->SetPosition(gp_Pnt(0, 0, 0));
    m_legendLabel->SetText(TCollection_ExtendedString(txt.toUtf8().constData(), true));
    m_legendLabel->SetColor(Quantity_NOC_YELLOW);
    TSA::Viewer::AnnotationStyle::instance().apply(m_legendLabel, TSA::Viewer::AnnotationRole::Legend);
    m_legendLabel->SetZoomable(false);
    m_legendLabel->SetHJustification(Graphic3d_HTA_RIGHT);
    m_legendLabel->SetVJustification(Graphic3d_VTA_BOTTOM);
    m_legendLabel->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_2d, Aspect_TOTP_RIGHT_LOWER, NCollection_Vec2<int>(16, 24)));
    m_legendLabel->SetZLayer(Graphic3d_ZLayerId_TopOSD);
    m_legendLabel->SetFont("Arial");
    ctx->Display(m_legendLabel, false);
    ctx->Deactivate(m_legendLabel);   // non sélectionnable
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
        const bool asWire = m_deformedRepresentation == DeformedRepresentation::Wire;
        if (!asWire && (key.kind == StructuralElementKind::Beam || key.kind == StructuralElementKind::Column))
        {
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
                    m_twistMarkerShapes.push_back({ key, m });
                }
            }
        }
        else
        {
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
        replaceShape(m_deformedElementShapes, key, ais);
    }
    applyIsolation();
}

template <typename Map, typename Key>
void ResultsVisualManager::replaceShape(Map& map, const Key& key, const Handle(AIS_Shape)& shape)
{
    // Garde : un objet déjà affiché sous la même clé est retiré de la scène avant d'être remplacé (jamais d'objet
    // orphelin resté à l'écran)
    auto it = map.find(key);
    if (it != map.end() && !it->second.IsNull() && it->second != shape)
        if (auto ctx = context()) ctx->Remove(it->second, false);
    map[key] = shape;
}

ResultsVisualManager::Diagnostics ResultsVisualManager::diagnostics() const
{
    Diagnostics d;
    // Objets réellement à l'écran : un objet masqué (plan de travail, chevauchement des textes) reste connu de ce gestionnaire
    auto ctx = context();
    auto shown = [&](const auto& object) { return int(!object.IsNull() && (ctx.IsNull() || ctx->IsDisplayed(object))); };
    for (const auto& [k, v] : m_deformedElementShapes) d.deformed += shown(v);
    for (const auto& [k, v] : m_deformedNodeShapes) d.deformed += shown(v);
    for (const auto& [k, v] : m_twistMarkerShapes) d.twistMarkers += shown(v);
    for (const auto& [k, v] : m_diagramShapes) d.diagrams += shown(v);
    for (const auto& [k, v] : m_diagramLabels)
        for (const auto& label : v) d.diagramLabels += shown(label);
    for (const auto& [k, v] : m_reactionShapes) d.reactions += shown(v);
    for (const auto& [k, v] : m_reactionLabels) d.reactionLabels += shown(v);
    for (const auto& [k, v] : m_leaderShapes) d.leaders += shown(v);
    d.extremum = int(!m_extremumMarkerShape.IsNull()) + int(!m_extremumLabel.IsNull());
    d.legend = int(!m_legendLabel.IsNull());
    return d;
}

void ResultsVisualManager::applyIsolation()
{
    auto ctx = context();
    if (ctx.IsNull() || !m_occView) return;
    auto setShown = [&](const Handle(AIS_InteractiveObject)& object, bool visible) {
        if (object.IsNull() || visible == ctx->IsDisplayed(object)) return;
        if (visible) ctx->Display(object, false);
        else ctx->Erase(object, false);
    };
    for (const auto& [key, shape] : m_deformedElementShapes) setShown(shape, m_occView->isResultElementVisible(key));
    for (const auto& [key, shape] : m_twistMarkerShapes) setShown(shape, m_occView->isResultElementVisible(key));
    for (const auto& [key, shape] : m_diagramShapes) setShown(shape, m_occView->isResultElementVisible(key.second));
    for (const auto& [nodeId, shape] : m_reactionShapes) setShown(shape, m_occView->isResultNodeVisible(nodeId));
    layoutLabels();
}

void ResultsVisualManager::layoutLabels()
{
    m_overlappedLabels = 0;
    auto ctx = context();
    if (ctx.IsNull() || !m_occView || m_occView->view().IsNull() || m_occView->view()->Window().IsNull()) return;
    int width = 0, height = 0;
    m_occView->view()->Window()->Size(width, height);
    if (width <= 0 || height <= 0) return;
    const Handle(Graphic3d_Camera)& camera = m_occView->view()->Camera();
    const gp_Vec up(camera->Up());
    const auto& textStyle = TSA::Viewer::LoadDisplayStyle::instance();
    const double referenceLength = m_occView->loadReferenceLength();
    const double dpr = TSA::Viewer::AnnotationStyle::instance().settings().devicePixelRatio;
    auto toPixel = [&](const gp_Pnt& point, double& x, double& y) {
        const gp_Pnt n = camera->Project(point);
        x = (n.X() + 1.0) * 0.5 * width;
        y = (1.0 - n.Y()) * 0.5 * height;
    };

    struct Item
    {
        Handle(AIS_TextLabel) label;
        double priority = 0.0, x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    };
    std::vector<Item> items;
    auto collect = [&](const Handle(AIS_TextLabel)& label, bool kept) {
        if (label.IsNull()) return;
        if (!kept)
        {
            if (ctx->IsDisplayed(label)) ctx->Erase(label, false);
            return;
        }
        Item it;
        it.label = label;
        const auto pr = m_labelPriority.find(label.get());
        it.priority = pr != m_labelPriority.end() ? pr->second : 0.0;
        double x = 0.0, y = 0.0, xu = 0.0, yu = 0.0;
        toPixel(label->Position(), x, y);
        toPixel(label->Position().Translated(up), xu, yu);
        // Même échelle que les valeurs des charges (Style des charges) : le texte suit le zoom entre deux bornes, jamais
        // démesuré en vue rapprochée ni illisible en vue d'ensemble
        const double pixelsPerMeter = std::hypot(xu - x, yu - y);
        const double em = std::round(textStyle.labelPixels(pixelsPerMeter, referenceLength, dpr));
        bool changed = std::abs(label->Attributes()->TextAspect()->Height() - em) > 0.5;
        if (changed) label->SetHeight(em);
        if (const auto anchor = m_labelAnchors.find(label.get()); anchor != m_labelAnchors.end() && pixelsPerMeter > 0.0)
        {
            const LabelAnchor& a = anchor->second;
            gp_Pnt position = a.position;
            double bx = 0.0, by = 0.0, px = 0.0, py = 0.0;
            toPixel(a.base, bx, by);
            toPixel(a.position.Translated(-a.along), px, py);   // décalage perpendiculaire seul : le test ne dépend pas de l'écart le long de l'élément
            gp_Vec side = gp_Vec(camera->Direction()).Crossed(gp_Vec(a.axis));
            // Décalage vu de bout : à l'écran, le texte tomberait sur l'élément (Mz vu de face, My vu en plan)
            if (std::hypot(px - bx, py - by) < 2.0 * em && side.Magnitude() > 1e-6)
                position = a.base.Translated(a.along + side.Normalized() * (a.sign * (a.clearance + 1.5 * em / pixelsPerMeter)));
            if (position.Distance(label->Position()) > 1e-6)
            {
                label->SetPosition(position);
                changed = true;
                toPixel(position, x, y);
            }
        }
        if (changed && ctx->IsDisplayed(label)) ctx->Redisplay(label, false);
        const double w = 0.6 * em * (label->Text().Length() + 1), h = 1.5 * em;   // encombrement estimé, ancrage centré en bas
        it.x0 = x - 0.5 * w;
        it.x1 = x + 0.5 * w;
        it.y0 = y - h;
        it.y1 = y;
        items.push_back(it);
    };
    for (const auto& [key, labels] : m_diagramLabels)
        for (const auto& label : labels) collect(label, m_occView->isResultElementVisible(key.second));
    for (const auto& [nodeId, label] : m_reactionLabels) collect(label, m_occView->isResultNodeVisible(nodeId));

    // Le plus fort d'abord ; un texte qui en chevauche un autre déjà gardé n'est pas affiché (grille pour rester rapide)
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.priority > b.priority; });
    constexpr double cell = 96.0;
    std::map<std::pair<int, int>, std::vector<std::size_t>> grid;
    std::vector<Handle(AIS_TextLabel)> shownLabels;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const Item& it = items[i];
        bool overlaps = false;
        const int cx0 = int(std::floor(it.x0 / cell)), cx1 = int(std::floor(it.x1 / cell));
        const int cy0 = int(std::floor(it.y0 / cell)), cy1 = int(std::floor(it.y1 / cell));
        for (int cx = cx0; cx <= cx1 && !overlaps; ++cx)
            for (int cy = cy0; cy <= cy1 && !overlaps; ++cy)
            {
                const auto found = grid.find({ cx, cy });
                if (found == grid.end()) continue;
                for (const std::size_t j : found->second)
                {
                    const Item& o = items[j];
                    if (it.x0 < o.x1 && o.x0 < it.x1 && it.y0 < o.y1 && o.y0 < it.y1)
                    {
                        overlaps = true;
                        break;
                    }
                }
            }
        if (overlaps)
        {
            ++m_overlappedLabels;
            if (ctx->IsDisplayed(it.label)) ctx->Erase(it.label, false);
            continue;
        }
        for (int cx = cx0; cx <= cx1; ++cx)
            for (int cy = cy0; cy <= cy1; ++cy) grid[{ cx, cy }].push_back(i);
        if (!ctx->IsDisplayed(it.label)) ctx->Display(it.label, false);
        shownLabels.push_back(it.label);
    }
    updateLeaders(shownLabels);
}

void ResultsVisualManager::updateLeaders(const std::vector<Handle(AIS_TextLabel)>& shownLabels)
{
    auto ctx = context();
    if (ctx.IsNull()) return;
    // Une ligne de rappel relie chaque valeur visible à son point de la courbe : avec plusieurs diagrammes, elle dit à quelle
    // courbe la valeur appartient. Refaites avec les textes (zoom, chevauchements, isolation) ; aucune ligne sans son texte.
    m_leaderSegments.clear();
    std::map<TSA::Geometry::DiagramType, TopoDS_Compound> lines;
    BRep_Builder builder;
    for (const auto& label : shownLabels)
    {
        const auto anchor = m_labelAnchors.find(label.get());
        if (anchor == m_labelAnchors.end()) continue;
        const gp_Pnt& from = anchor->second.curve;
        const gp_Pnt to = label->Position();
        if (from.Distance(to) < 1e-4) continue;
        auto line = lines.find(anchor->second.type);
        if (line == lines.end())
        {
            line = lines.emplace(anchor->second.type, TopoDS_Compound()).first;
            builder.MakeCompound(line->second);
        }
        builder.Add(line->second, BRepBuilderAPI_MakeEdge(from, to).Edge());
        builder.Add(line->second, BRepBuilderAPI_MakeVertex(from).Vertex());   // point marqué sur la courbe
        m_leaderSegments.push_back({ from, to });
    }
    for (auto it = m_leaderShapes.begin(); it != m_leaderShapes.end();)
    {
        if (lines.count(it->first))
        {
            ++it;
            continue;
        }
        ctx->Remove(it->second, false);
        it = m_leaderShapes.erase(it);
    }
    for (const auto& [type, compound] : lines)
    {
        auto& shape = m_leaderShapes[type];
        if (shape.IsNull())
        {
            shape = new AIS_Shape(compound);
            shape->SetColor(diagramColor(type));
            shape->SetWidth(1.6);
            shape->Attributes()->SetPointAspect(new Prs3d_PointAspect(Aspect_TOM_BALL, diagramColor(type), 2.5));
            shape->SetZLayer(Graphic3d_ZLayerId_Topmost);   // visible devant la structure, comme son texte
            ctx->Display(shape, AIS_WireFrame, -1, false);   // trait d'annotation : jamais sélectionnable
        }
        else
        {
            shape->SetShape(compound);
            ctx->Redisplay(shape, false);
        }
    }
}

void ResultsVisualManager::styleResultLabel(const Handle(AIS_TextLabel)& label, const Quantity_Color& color) const
{
    if (label.IsNull()) return;
    // Au-dessus de la structure (calque à profondeur effacée, comme les valeurs des charges) et contour contrasté : un texte
    // n'est jamais caché dans une section, et reste lisible devant elle
    label->SetColor(color);
    label->SetZLayer(Graphic3d_ZLayerId_Topmost);
    label->SetDisplayType(Aspect_TODT_DEKALE);
    const bool dark = !m_occView || m_occView->isDarkMode();
    label->SetColorSubTitle(dark ? Quantity_Color(0.05, 0.05, 0.05, Quantity_TOC_RGB) : Quantity_Color(1.0, 1.0, 1.0, Quantity_TOC_RGB));
}

double ResultsVisualManager::sectionClearance(const TSA::Analysis::ElementKey& key) const
{
    double rotation = 0.0;
    const TSA::Model::Section* section = memberSection(key, &rotation);
    return (section ? 0.5 * std::max(section->width, section->height) : 0.0) + 0.08;
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
        for (auto& [key, shape] : m_twistMarkerShapes) ctx->Remove(shape, false);
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

    if (m_layers.empty() || !m_results || !m_results->isValid() || !m_model)
    {
        applyIsolation();
        return;
    }

    const bool kN = m_results->units().force == "kN";
    struct PendingLabel
    {
        DiagramKey key;
        gp_Pnt position;
        double value;
        double priority;
        QString text;
        LabelAnchor anchor;
    };
    std::vector<PendingLabel> pending;
    for (const DiagramLayer& layer : m_layers)
    {
    if (layer.allZero) continue;   // grandeur nulle partout : rien de dessiné, la légende l'indique
    const TSA::Geometry::DiagramType diagramType = layer.type;
    for (const auto& [elKey, elemRes] : m_results->allElementResults())
    {
        const int elId = elKey.id;
        auto stations = displayStations(m_model, elKey, elemRes, diagramType, kN);
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

        const DiagramKey shapeKey { diagramType, elKey };
        TopoDS_Shape dShape = TSA::Geometry::DiagramGeometry::createDiagramShape(
            p1, p2, rot, stations, diagramType, layer.scale, true
        );

        if (!dShape.IsNull())
        {
            Handle(AIS_Shape) ais = new AIS_Shape(dShape);
            ais->SetColor(diagramColor(diagramType));
            ais->SetTransparency(0.25);
            ctx->Display(ais, false);
            replaceShape(m_diagramShapes, shapeKey, ais);
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
                double v = TSA::Geometry::DiagramGeometry::getStationValue(stations[s], diagramType);
                if (v > maxVal) { maxVal = v; maxIdx = s; }
                if (v < minVal) { minVal = v; minIdx = s; }
            }

            gp_Vec offsetDir = TSA::Geometry::DiagramGeometry::getDiagramOffsetDirection(p1, p2, rot, diagramType);
            if (offsetDir.Magnitude() < 1e-6) continue;
            offsetDir.Normalize();
            gp_Vec vAB(p1, p2);
            double length = vAB.Magnitude();
            QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(diagramType, m_results->units().force != "N");   // BUG-117
            const double clearance = sectionClearance(elKey);

            const auto layerIndex = static_cast<double>(&layer - m_layers.data());
            gp_Dir axis(gp_Vec(0, 0, 1));
            if (length > 1e-6) axis = gp_Dir(vAB);
            auto addLabel = [&](size_t idx, double val, double inward)
            {
                if (std::abs(val) <= 1e-9 * layer.maxValue) return;
                double pos = (length > 1e-6) ? (stations[idx].position / length) : 0.0;
                gp_Pnt base = p1.Translated(vAB * pos);
                // Texte placé au-delà de la section, du côté du diagramme : jamais dans la matière de l'élément. Aux extrémités,
                // il rentre vers le milieu de l'élément (davantage pour chaque diagramme) : la ligne de rappel dit à quelle
                // courbe il appartient, et deux textes d'une même extrémité ne se recouvrent pas.
                const gp_Vec along = gp_Vec(axis) * (inward * std::min(0.3 * length, 0.35 + 0.3 * layerIndex));
                const double reach = std::abs(val * layer.scale) + clearance + kLeaderLength;
                gp_Pnt posLabel = base.Translated(along + offsetDir * std::copysign(reach, val));

                // 4 chiffres significatifs : une rotation de 0,0059 rad s'affichait « 0.0 rad » (format à 1 décimale)
                const QString name = m_layers.size() > 1 ? TSA::Geometry::DiagramGeometry::diagramShortName(diagramType) + QStringLiteral(" = ") : QString();
                LabelAnchor anchor;
                anchor.base = base;
                anchor.position = posLabel;
                anchor.curve = base.Translated(offsetDir * (val * layer.scale));   // même point que DiagramGeometry::createDiagramShape
                anchor.axis = axis;
                anchor.along = along;
                anchor.clearance = clearance;
                anchor.sign = val < 0.0 ? -1.0 : 1.0;
                anchor.type = diagramType;
                pending.push_back({ shapeKey, posLabel, val, std::abs(val) / layer.maxValue,
                                    QString("%1%2 %3").arg(name).arg(QString::number(val, 'g', 4)).arg(unit), anchor });
            };

            // Valeurs aux deux extrémités, puis le maximum et le minimum s'ils sont ailleurs. Valeur constante le long de
            // l'élément (treillis, effort normal) : une seule fois.
            const size_t last = stations.size() - 1;
            const auto valueAt = [&](size_t idx) { return TSA::Geometry::DiagramGeometry::getStationValue(stations[idx], diagramType); };
            const double same = 1e-6 * layer.maxValue;
            if (maxVal - minVal <= same) addLabel(last / 2, valueAt(last / 2), 0.0);
            else
            {
                addLabel(0, valueAt(0), 1.0);
                addLabel(last, valueAt(last), -1.0);
                const auto interior = [&](size_t idx) {
                    return idx != 0 && idx != last && std::abs(valueAt(idx) - valueAt(0)) > same && std::abs(valueAt(idx) - valueAt(last)) > same;
                };
                if (interior(maxIdx)) addLabel(maxIdx, maxVal, 0.0);
                if (minIdx != maxIdx && interior(minIdx)) addLabel(minIdx, minVal, 0.0);
            }
        }
    }
    }
    // Étiquettes : à un nœud partagé, la poutre et le poteau donnent la même valeur au même endroit. Par grandeur, une
    // étiquette dont la position est à moins de 4 % de l'envergure d'une étiquette de plus grande valeur absolue n'est pas
    // affichée (valeur toujours lisible par l'autre ; rien n'est perdu dans les tableaux et la note).
    double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
    for (const auto& [id, node] : m_model->nodes())
    {
        const double c[3] = { node.x(), node.y(), node.z() };
        for (int k = 0; k < 3; ++k) lo[k] = std::min(lo[k], c[k]), hi[k] = std::max(hi[k], c[k]);
    }
    const double minGap = 0.04 * std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1.0 });
    std::stable_sort(pending.begin(), pending.end(), [](const PendingLabel& a, const PendingLabel& b) { return a.priority > b.priority; });
    std::map<TSA::Geometry::DiagramType, std::vector<gp_Pnt>> keptByType;
    m_hiddenDiagramLabels = 0;
    for (const auto& pl : pending)
    {
        auto& kept = keptByType[pl.key.first];
        const bool crowded = std::any_of(kept.begin(), kept.end(), [&](const gp_Pnt& p) { return p.Distance(pl.position) < minGap; });
        if (crowded)
        {
            ++m_hiddenDiagramLabels;
            continue;
        }
        kept.push_back(pl.position);
        Handle(AIS_TextLabel) lbl = new AIS_TextLabel();
        lbl->SetPosition(pl.position);
        lbl->SetText(TCollection_ExtendedString(pl.text.toUtf8().constData(), true));
        styleResultLabel(lbl, diagramColor(pl.key.first));
        TSA::Viewer::AnnotationStyle::instance().apply(lbl, TSA::Viewer::AnnotationRole::DiagramValue);
        lbl->SetFont("Arial");
        ctx->Display(lbl, false);
        m_labelPriority[lbl.get()] = pl.priority;
        m_labelAnchors[lbl.get()] = pl.anchor;
        m_diagramLabels[pl.key].push_back(lbl);
    }
    applyIsolation();
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
    if (ctx)
        for (auto& [type, shape] : m_leaderShapes) ctx->Remove(shape, false);
    m_leaderShapes.clear();
    m_leaderSegments.clear();
    m_labelPriority.clear();
    m_labelAnchors.clear();
    m_hiddenDiagramLabels = 0;
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

    const double toKN = m_results->units().forceToKiloNewtons();   // flèches et textes en kN (calcul en kN ou N), BUG-117
    for (const auto& [nodeId, raw] : m_results->allReactions())
    {
        auto react = raw;
        for (double* v : { &react.rx, &react.ry, &react.rz, &react.mx, &react.my, &react.mz }) *v *= toKN;
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
        styleResultLabel(lbl, Quantity_Color(Quantity_NOC_LIMEGREEN));
        TSA::Viewer::AnnotationStyle::instance().apply(lbl, TSA::Viewer::AnnotationRole::ReactionValue);
        ctx->Display(lbl, false);
        m_reactionLabels[nodeId] = lbl;
    }
    applyIsolation();
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
    TSA::Viewer::AnnotationStyle::instance().apply(m_extremumLabel, TSA::Viewer::AnnotationRole::Extremum);

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
