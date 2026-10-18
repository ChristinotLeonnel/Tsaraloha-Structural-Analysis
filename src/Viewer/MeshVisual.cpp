#include "MeshVisual.h"

#include "MeshVSDataSource.h"

#include <AIS_ColorScale.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ArrayOfPoints.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_AspectMarker3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <MeshVS_Drawer.hxx>
#include <MeshVS_DrawerAttribute.hxx>
#include <MeshVS_Mesh.hxx>
#include <MeshVS_MeshEntityOwner.hxx>
#include <MeshVS_MeshPrsBuilder.hxx>
#include <MeshVS_SelectionModeFlags.hxx>
#include <MeshVS_TextPrsBuilder.hxx>
#include <NCollection_Vec2.hxx>
#include <Prs3d_Presentation.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <SelectMgr_Selection.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>

#include <QCoreApplication>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Viewer
{

using TSA::Mesh::ElementTopology;
using TSA::Mesh::elementTopology;
using Style = MeshViewOptions::Style;

namespace
{
/// Segments, triangles et marqueurs colorés par sommet, sans sélection (calques visuels du maillage).
class OverlayObject : public AIS_InteractiveObject
{
public:
    std::vector<gp_Pnt> segPoints;
    std::vector<Quantity_Color> segColors;
    double width = 1.5;
    std::vector<gp_Pnt> triPoints;
    std::vector<Quantity_Color> triColors;
    std::vector<gp_Pnt> markers;
    Quantity_Color markerColor { 0.95, 0.95, 0.95, Quantity_TOC_sRGB };
    double markerScale = 2.0;

    void addSegment(const gp_Pnt& a, const gp_Pnt& b, const Quantity_Color& ca, const Quantity_Color& cb)
    {
        segPoints.push_back(a);
        segPoints.push_back(b);
        segColors.push_back(ca);
        segColors.push_back(cb);
    }

    void Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs, const int mode) override
    {
        if (mode != 0) return;
        if (!triPoints.empty())
        {
            Handle(Graphic3d_ArrayOfTriangles) arr = new Graphic3d_ArrayOfTriangles(int(triPoints.size()), 0, Graphic3d_ArrayFlags_VertexColor);
            for (std::size_t i = 0; i < triPoints.size(); ++i) arr->AddVertex(triPoints[i], triColors[i]);
            Handle(Graphic3d_AspectFillArea3d) aspect = new Graphic3d_AspectFillArea3d();
            aspect->SetInteriorStyle(Aspect_IS_SOLID);
            aspect->SetShadingModel(Graphic3d_TypeOfShadingModel_Unlit); // couleurs exactes de la légende
            aspect->SetInteriorColor(Quantity_NOC_WHITE);
            aspect->SetFaceCulling(Graphic3d_TypeOfBackfacingModel_DoubleSided);
            Handle(Graphic3d_Group) g = prs->NewGroup();
            g->SetGroupPrimitivesAspect(aspect);
            g->AddPrimitiveArray(arr);
        }
        if (!segPoints.empty())
        {
            Handle(Graphic3d_ArrayOfSegments) arr = new Graphic3d_ArrayOfSegments(int(segPoints.size()), 0, Graphic3d_ArrayFlags_VertexColor);
            for (std::size_t i = 0; i < segPoints.size(); ++i) arr->AddVertex(segPoints[i], segColors[i]);
            Handle(Graphic3d_Group) g = prs->NewGroup();
            g->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(Quantity_NOC_WHITE, Aspect_TOL_SOLID, width));
            g->AddPrimitiveArray(arr);
        }
        if (!markers.empty())
        {
            Handle(Graphic3d_ArrayOfPoints) pts = new Graphic3d_ArrayOfPoints(int(markers.size()));
            for (const auto& p : markers) pts->AddVertex(p);
            Handle(Graphic3d_Group) g = prs->NewGroup();
            g->SetGroupPrimitivesAspect(new Graphic3d_AspectMarker3d(Aspect_TOM_O_POINT, markerColor, markerScale));
            g->AddPrimitiveArray(pts);
        }
    }
    void ComputeSelection(const Handle(SelectMgr_Selection)&, const int) override {}
};

const Quantity_Color kGenerated(0.30, 0.90, 0.55, Quantity_TOC_sRGB);
const Quantity_Color kPreview(1.0, 0.62, 0.15, Quantity_TOC_sRGB);
const Quantity_Color kFace(0.70, 0.76, 0.84, Quantity_TOC_sRGB);
const Quantity_Color kPoor(1.0, 0.15, 0.15, Quantity_TOC_sRGB);
const Quantity_Color kHighlight(1.0, 0.9, 0.1, Quantity_TOC_sRGB);
const Quantity_Color kNoValue(0.55, 0.55, 0.55, Quantity_TOC_sRGB);
const Quantity_Color kGhost(0.50, 0.50, 0.50, Quantity_TOC_sRGB);

std::string fmt(double v, int digits = 4)
{
    std::ostringstream os;
    os << std::setprecision(digits) << v;
    return os.str();
}

std::string join(const std::vector<std::string>& v)
{
    std::string out;
    for (const auto& s : v) out += (out.empty() ? "" : ", ") + s;
    return out;
}

QString qs(const std::string& s)
{
    return QString::fromStdString(s);
}

/// Nom d'un champ de résultats : clé de MeshResultSet (texte source marqué dans MeshResultField), traduit à l'affichage seulement.
QString fieldName(const std::string& name)
{
    return QCoreApplication::translate("TSA::Mesh::MeshResultField", name.c_str());
}
} // namespace

MeshVisual::MeshVisual(const Handle(AIS_InteractiveContext)& context, const Handle(V3d_View)& view)
    : m_contextFn([context]() { return context; })
    , m_viewFn([view]() { return view; })
{
}

MeshVisual::MeshVisual(std::function<Handle(AIS_InteractiveContext)()> context, std::function<Handle(V3d_View)()> view)
    : m_contextFn(std::move(context))
    , m_viewFn(std::move(view))
{
}

MeshVisual::~MeshVisual()
{
    removeObjects();
}

gp_Pnt MeshVisual::position(int nodeId) const
{
    if (!m_source.IsNull()) return m_source->position(nodeId);
    const auto* n = m_model.node(nodeId);
    return n ? gp_Pnt(n->x, n->y, n->z) : gp_Pnt();
}

void MeshVisual::removeObjects()
{
    if (!context().IsNull())
        for (Handle(AIS_InteractiveObject) o : { Handle(AIS_InteractiveObject)(m_meshVS), m_edges, m_poor, m_results, m_highlight,
                                                 Handle(AIS_InteractiveObject)(m_legend) })
            if (!o.IsNull()) context()->Remove(o, false);
    m_meshVS.Nullify();
    m_edges.Nullify();
    m_poor.Nullify();
    m_results.Nullify();
    m_highlight.Nullify();
    m_legend.Nullify();
    m_source.Nullify();
    m_edgeSegments = m_labelCount = 0;
    m_labelsTruncated = false;
}

void MeshVisual::clear()
{
    removeObjects();
    m_mesh.reset();
    m_model = TSA::Mesh::MeshDisplayModel();
    m_quality.clear();
    m_colorField.reset();
    m_displacement.reset();
    m_scale = 0.0;
    m_highlightElement = m_highlightNode = 0;
    if (!view().IsNull()) view()->Redraw();
}

void MeshVisual::show(const TSA::Mesh::FeMesh& mesh, Mode mode, const std::vector<double>* quality, double threshold,
                      const TSA::Mesh::MeshResultField* colorField, const TSA::Mesh::MeshResultField* displacement, double scale)
{
    // Nouveau maillage : résultats et mise en évidence portaient sur l'ancien ; l'appelant réapplique les
    // résultats s'ils correspondent toujours à ce maillage (jamais d'association implicite).
    m_mesh = std::make_shared<const TSA::Mesh::FeMesh>(mesh);
    m_mode = mode;
    m_quality = quality ? *quality : std::vector<double>();
    m_threshold = threshold;
    m_colorField = colorField ? std::optional<TSA::Mesh::MeshResultField>(*colorField) : std::nullopt;
    m_displacement = displacement && displacement->components == 3 ? std::optional<TSA::Mesh::MeshResultField>(*displacement) : std::nullopt;
    m_scale = m_displacement ? scale : 0.0;
    m_highlightElement = m_highlightNode = 0;
    rebuild();
}

void MeshVisual::setOptions(const MeshViewOptions& options)
{
    m_options = options;
    if (m_mesh) rebuild();
}

void MeshVisual::setResults(const TSA::Mesh::MeshResultField* colorField, const TSA::Mesh::MeshResultField* displacement, double scale)
{
    m_colorField = colorField ? std::optional<TSA::Mesh::MeshResultField>(*colorField) : std::nullopt;
    m_displacement = displacement && displacement->components == 3 ? std::optional<TSA::Mesh::MeshResultField>(*displacement) : std::nullopt;
    m_scale = m_displacement ? scale : 0.0;
    if (m_mesh) rebuild();
}

void MeshVisual::setDeformationScale(double scale)
{
    if (!m_displacement) return;
    m_scale = scale;
    if (m_mesh) rebuild();
}

void MeshVisual::rebuild()
{
    const auto t0 = std::chrono::steady_clock::now();
    removeObjects();
    if (context().IsNull() || !m_mesh) return;
    m_model.build(*m_mesh, m_options.filter);
    m_source = new MeshVSDataSource(m_model);
    m_source->setDisplacements(m_displacement ? &m_displacement->values : nullptr, m_scale);

    const bool preview = m_mode == Mode::Preview;
    const Style style = preview ? Style::Wireframe : m_options.style;
    const bool colored = m_colorField.has_value() && !preview;
    const Quantity_Color line = preview ? kPreview : kGenerated;
    const double width = preview ? 1.2 : m_options.lineWidth;

    // 1. MeshVS : faces, volumes (faces affichées), liens 1D, nœuds, étiquettes, sélection.
    if (!m_source->GetAllElements().IsEmpty() || !m_source->GetAllNodes().IsEmpty())
    {
        m_meshVS = new MeshVS_Mesh();
        m_meshVS->SetDataSource(m_source);
        m_meshVS->AddBuilder(new MeshVS_MeshPrsBuilder(m_meshVS), true);
        Handle(MeshVS_Drawer) d = m_meshVS->GetDrawer();
        d->SetInteger(MeshVS_DA_MaxFaceNodes, 32); // Hex20 : 20 nœuds (10 par défaut)
        d->SetBoolean(MeshVS_DA_DisplayNodes, m_options.showNodes && !preview);
        d->SetInteger(MeshVS_DA_MarkerType, Aspect_TOM_O_POINT);
        d->SetColor(MeshVS_DA_MarkerColor, Quantity_Color(0.95, 0.95, 0.95, Quantity_TOC_sRGB));
        d->SetDouble(MeshVS_DA_MarkerScale, std::max(0.5, m_options.nodeSize));
        // Arêtes des faces par MeshVS : seulement en mode réduit (chaque élément réduit a ses propres arêtes, fines
        // pour ne pas masquer les petites faces) ; sinon les arêtes uniques sont tracées une seule fois par le
        // calque des arêtes.
        d->SetBoolean(MeshVS_DA_ShowEdges, style == Style::Shrink);
        d->SetColor(MeshVS_DA_EdgeColor, Quantity_Color(0.12, 0.12, 0.12, Quantity_TOC_sRGB));
        d->SetDouble(MeshVS_DA_EdgeWidth, 1.0);
        const bool noFill = style == Style::Wireframe || colored;
        d->SetInteger(MeshVS_DA_InteriorStyle, noFill ? Aspect_IS_EMPTY : Aspect_IS_SOLID);
        d->SetColor(MeshVS_DA_InteriorColor, kFace);
        d->SetColor(MeshVS_DA_BackInteriorColor, kFace);
        d->SetColor(MeshVS_DA_BeamColor, line);
        d->SetDouble(MeshVS_DA_BeamWidth, width);
        d->SetDouble(MeshVS_DA_ShrinkCoeff, 0.75);
        d->SetBoolean(MeshVS_DA_SmoothShading, false);
        d->SetBoolean(MeshVS_DA_Reflection, true);
        d->SetBoolean(MeshVS_DA_SupressBackFaces, false);
        int displayMode = style == Style::Shrink ? MeshVS_DMF_Shrink : MeshVS_DMF_Shading;
        if (!preview && (m_options.nodeLabels || m_options.elementLabels))
        {
            Handle(MeshVS_TextPrsBuilder) text = new MeshVS_TextPrsBuilder(m_meshVS, 12.0, Quantity_Color(1.0, 0.95, 0.4, Quantity_TOC_sRGB));
            std::vector<int> ids;
            if (m_options.nodeLabels)
            {
                m_labelsTruncated |= TSA::Mesh::MeshDisplayModel::labelSubset(m_model.visibleNodes(), m_options.maxLabels, &ids);
                NCollection_DataMap<int, TCollection_AsciiString> map;
                for (int id : ids) map.Bind(id, TCollection_AsciiString("N") + id);
                text->SetTexts(false, map);
                m_labelCount += ids.size();
            }
            if (m_options.elementLabels)
            {
                std::vector<int> drawn;
                for (int id : m_model.visibleElements())
                    if (m_source->GetAllElements().Contains(id)) drawn.push_back(id);
                m_labelsTruncated |= TSA::Mesh::MeshDisplayModel::labelSubset(drawn, m_options.maxLabels, &ids);
                NCollection_DataMap<int, TCollection_AsciiString> map;
                for (int id : ids) map.Bind(id, TCollection_AsciiString("E") + id);
                text->SetTexts(true, map);
                m_labelCount += ids.size();
            }
            m_meshVS->AddBuilder(text, false);
            displayMode |= MeshVS_DMF_TextDataPrs;
        }
        m_meshVS->SetDisplayMode(displayMode);
        context()->Display(m_meshVS, displayMode, -1, false);
        if (m_options.transparency > 0.0 && !noFill) context()->SetTransparency(m_meshVS, std::min(0.95, m_options.transparency), false);
        // Nœuds sélectionnables : ceux des éléments affichés (sinon MeshVS ne rend sélectionnables que les nœuds masqués).
        m_meshVS->UpdateSelectableNodes();
        if (m_pickMode) activatePick();
    }

    // 2. Arêtes uniques (chacune une seule fois) ; forme initiale grise sous une forme déformée.
    if (style == Style::Wireframe || style == Style::FacesEdges || (m_displacement && m_scale != 0.0 && m_options.showUndeformed))
    {
        Handle(OverlayObject) edges = new OverlayObject();
        edges->width = width;
        if (style == Style::Wireframe || style == Style::FacesEdges)
            for (const auto& e : m_model.edges())
            {
                if (e.mid)
                {
                    edges->addSegment(position(e.a), position(e.mid), line, line);
                    edges->addSegment(position(e.mid), position(e.b), line, line);
                }
                else
                    edges->addSegment(position(e.a), position(e.b), line, line);
            }
        m_edgeSegments = edges->segPoints.size() / 2;
        if (m_displacement && m_scale != 0.0 && m_options.showUndeformed)
        {
            auto initial = [&](int id) {
                const auto* n = m_model.node(id);
                return gp_Pnt(n->x, n->y, n->z);
            };
            for (const auto& e : m_model.edges()) edges->addSegment(initial(e.a), initial(e.b), kGhost, kGhost);
            for (int id : m_model.visibleElements())
                if (const auto* fe = m_model.element(id); TSA::Mesh::feTypeDimension(fe->type) == 1)
                    edges->addSegment(initial(fe->nodes[0]), initial(fe->nodes[1]), kGhost, kGhost);
        }
        if (!edges->segPoints.empty())
        {
            context()->Display(edges, 0, -1, false);
            m_edges = edges;
        }
    }

    // 3. Champ de résultats : couleurs nodales (ou par élément) et légende ; gris = aucune valeur.
    if (colored)
    {
        displayLegend();
        const auto& field = *m_colorField;
        auto colorOf = [&](int elementId, int nodeId) {
            double v = 0.0;
            Quantity_Color c = kNoValue;
            const int id = field.location == TSA::Mesh::FieldLocation::Node ? nodeId : elementId;
            if (field.scalar(id, &v)) m_legend->FindColor(v, c);
            return c;
        };
        Handle(OverlayObject) res = new OverlayObject();
        res->width = width + 1.5;
        auto fan = [&](int elementId, const std::vector<int>& ids) {
            for (std::size_t k = 1; k + 1 < ids.size(); ++k)
                for (int n : { ids[0], ids[k], ids[k + 1] })
                {
                    res->triPoints.push_back(position(n));
                    res->triColors.push_back(colorOf(elementId, n));
                }
        };
        std::vector<int> ids;
        for (int id : m_model.visibleElements())
        {
            const auto* e = m_model.element(id);
            const ElementTopology* topo = elementTopology(e->type);
            if (topo->dimension == 1)
            {
                ids = { e->nodes[0] };
                if (topo->edgeMid[0] >= 0) ids.push_back(e->nodes[std::size_t(topo->edgeMid[0])]);
                ids.push_back(e->nodes[1]);
                for (std::size_t k = 0; k + 1 < ids.size(); ++k)
                    res->addSegment(position(ids[k]), position(ids[k + 1]), colorOf(id, ids[k]), colorOf(id, ids[k + 1]));
            }
            else if (topo->dimension == 2)
            {
                ids.clear();
                for (int local : TSA::Mesh::faceOutline(*topo, topo->faces.front())) ids.push_back(e->nodes[std::size_t(local)]);
                fan(id, ids);
            }
            else
                for (const auto& face : m_model.volumeFaces(id))
                {
                    ids.clear();
                    for (int local : face) ids.push_back(e->nodes[std::size_t(local)]);
                    fan(id, ids);
                }
        }
        context()->Display(res, 0, -1, false);
        context()->SetDisplayPriority(res, Graphic3d_DisplayPriority_Above); // tracé après les liens MeshVS confondus
        m_results = res;
    }

    // 4. Éléments de qualité insuffisante : arêtes en rouge au premier plan.
    if (!preview && !m_quality.empty())
    {
        Handle(OverlayObject) poor = new OverlayObject();
        poor->width = width + 1.0;
        for (int id : m_model.visibleElements())
        {
            const int index = m_model.elementIndex(id);
            if (index < 0 || std::size_t(index) >= m_quality.size() || !(m_quality[std::size_t(index)] < m_threshold)) continue;
            const auto* e = m_model.element(id);
            const ElementTopology* topo = elementTopology(e->type);
            for (std::size_t k = 0; k < topo->edges.size(); ++k)
            {
                const int a = e->nodes[std::size_t(topo->edges[k][0])], b = e->nodes[std::size_t(topo->edges[k][1])];
                poor->addSegment(position(a), position(b), kPoor, kPoor);
            }
        }
        if (!poor->segPoints.empty())
        {
            context()->Display(poor, 0, -1, false);
            context()->SetZLayer(poor, Graphic3d_ZLayerId_Topmost);
            m_poor = poor;
        }
    }

    if (m_highlightElement || m_highlightNode) highlight(m_highlightElement, m_highlightNode, false);
    m_lastBuildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!view().IsNull()) view()->Redraw();
}

void MeshVisual::displayLegend()
{
    const auto& field = *m_colorField;
    auto [lo, hi] = field.range();
    if (!(hi > lo)) hi = lo + std::max(1e-12, std::abs(lo) * 1e-6);
    m_legend = new AIS_ColorScale();
    m_legend->SetRange(lo, hi);
    m_legend->SetNumberOfIntervals(10);
    m_legend->SetColorType(Aspect_TOCSD_AUTO);
    m_legend->SetLabelType(Aspect_TOCSD_AUTO);
    m_legend->SetFormat("%.3g");
    std::string title = field.name + (field.unit.empty() ? "" : " [" + field.unit + "]");
    if (field.components == 3) title += " (norme)";
    if (!field.source.empty()) title += "\n" + field.source;
    m_legend->SetTitle(TCollection_ExtendedString(title.c_str(), true));
    m_legend->SetSize(24, 260);
    m_legend->SetTextHeight(13);
    m_legend->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_2d, Aspect_TOTP_LEFT_LOWER, NCollection_Vec2<int>(20, 40)));
    m_legend->SetZLayer(Graphic3d_ZLayerId_TopOSD);
    context()->Display(m_legend, 0, -1, false);
}

void MeshVisual::setPickMode(bool on)
{
    m_pickMode = on;
    if (context().IsNull() || m_meshVS.IsNull()) return;
    if (on) activatePick();
    else context()->Deactivate(m_meshVS);
}

void MeshVisual::activatePick()
{
    // Affiché sans sélection (mode −1) : l'objet doit être enregistré auprès du gestionnaire de sélection
    // avant toute activation (sinon l'activation est sans effet sur la détection).
    context()->Load(m_meshVS, -1);
    // Un mode par type d'entité : MeshVS ne crée les entités sensibles des éléments que pour un type à la fois.
    for (int mode : { int(MeshVS_SMF_Node), int(MeshVS_SMF_Link), int(MeshVS_SMF_Face), int(MeshVS_SMF_Volume) })
        context()->Activate(m_meshVS, mode);
}

bool MeshVisual::handlePick(const Handle(SelectMgr_EntityOwner)& owner, Pick* out)
{
    Handle(MeshVS_MeshEntityOwner) entity = Handle(MeshVS_MeshEntityOwner)::DownCast(owner);
    if (entity.IsNull() || m_meshVS.IsNull() || entity->Selectable().get() != m_meshVS.get()) return false;
    const bool element = entity->Type() != MeshVS_ET_Node;
    const int id = entity->ID();
    highlight(element ? id : 0, element ? 0 : id, false);
    if (out)
    {
        out->element = element;
        out->id = id;
        out->text = element ? describeElement(id) : describeNode(id);
    }
    return true;
}

std::string MeshVisual::hoverText(const Handle(SelectMgr_EntityOwner)& owner) const
{
    Handle(MeshVS_MeshEntityOwner) entity = Handle(MeshVS_MeshEntityOwner)::DownCast(owner);
    if (entity.IsNull() || m_meshVS.IsNull() || entity->Selectable().get() != m_meshVS.get()) return {};
    const int id = entity->ID();
    if (entity->Type() == MeshVS_ET_Node)
    {
        const gp_Pnt p = position(id);
        return QCoreApplication::translate("TSA::Viewer::MeshVisual", "Maillage : nœud %1 (%2 ; %3 ; %4) m")
            .arg(id)
            .arg(qs(fmt(p.X())), qs(fmt(p.Y())), qs(fmt(p.Z())))
            .toStdString();
    }
    const auto* e = m_model.element(id);
    return e ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "Maillage : élément %1 (%2, %3)")
                   .arg(id)
                   .arg(qs(TSA::Mesh::feTypeLabel(e->type).substr(0, TSA::Mesh::feTypeLabel(e->type).find(' '))), qs(e->source))
                   .toStdString()
             : std::string();
}

std::string MeshVisual::describeElement(int elementId) const
{
    const auto* e = m_model.element(elementId);
    if (!e) return QCoreApplication::translate("TSA::Viewer::MeshVisual", "Élément %1 : inexistant").arg(elementId).toStdString();
    QString s = (TSA::Mesh::feTypeIsSecondOrder(e->type)
                     ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "Élément %1 — %2 (%3D, second ordre)")
                     : QCoreApplication::translate("TSA::Viewer::MeshVisual", "Élément %1 — %2 (%3D)"))
                    .arg(e->id)
                    .arg(qs(TSA::Mesh::feTypeLabel(e->type)))
                    .arg(TSA::Mesh::feTypeDimension(e->type));
    std::vector<std::string> nodes;
    for (int n : e->nodes) nodes.push_back(std::to_string(n));
    s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Nœuds : %1").arg(qs(join(nodes)));
    if (e->gmshTag)
        s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Gmsh : élément %1, entité %2:%3").arg(qulonglong(e->gmshTag)).arg(e->gmshEntityDim).arg(e->gmshEntityTag);
    else
        s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Gmsh : étiquette inconnue (maillage antérieur à la conservation des étiquettes)");
    const QString source = e->source.empty() ? QStringLiteral("—") : qs(e->source);
    s += "\n" + (e->boundary ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "Source : %1 (élément de frontière)").arg(source)
                             : QCoreApplication::translate("TSA::Viewer::MeshVisual", "Source : %1").arg(source));
    const auto groups = m_model.groupsOfElement(e->id);
    s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Groupes : %1")
                    .arg(groups.empty() ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "aucun") : qs(join(groups)));
    const int index = m_model.elementIndex(e->id);
    if (index >= 0 && std::size_t(index) < m_quality.size() && m_quality[std::size_t(index)] <= 1.0)
        s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Qualité (minSICN) : %1").arg(qs(fmt(m_quality[std::size_t(index)], 3)));
    else if (e->quality <= 1.0)
        s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Qualité (minSICN) : %1").arg(qs(fmt(e->quality, 3)));
    if (m_colorField && m_colorField->location == TSA::Mesh::FieldLocation::Element)
    {
        double v = 0.0;
        s += "\n" + fieldName(m_colorField->name) + " : " +
             (m_colorField->scalar(e->id, &v) ? qs(fmt(v) + " " + m_colorField->unit) : QCoreApplication::translate("TSA::Viewer::MeshVisual", "non calculé"));
    }
    return s.toStdString();
}

std::string MeshVisual::describeNode(int nodeId) const
{
    const auto* n = m_model.node(nodeId);
    if (!n) return QCoreApplication::translate("TSA::Viewer::MeshVisual", "Nœud %1 : inexistant").arg(nodeId).toStdString();
    QString s = QCoreApplication::translate("TSA::Viewer::MeshVisual", "Nœud %1 — (%2 ; %3 ; %4) m")
                    .arg(n->id)
                    .arg(qs(fmt(n->x, 6)), qs(fmt(n->y, 6)), qs(fmt(n->z, 6)));
    s += "\n" + (n->tsaNode > 0 ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "Nœud TSA confondu : %1").arg(n->tsaNode)
                                : QCoreApplication::translate("TSA::Viewer::MeshVisual", "Nœud du maillage (aucun nœud TSA)"));
    s += "\n" + (n->gmshTag ? QCoreApplication::translate("TSA::Viewer::MeshVisual", "Gmsh : nœud %1").arg(qulonglong(n->gmshTag))
                            : QCoreApplication::translate("TSA::Viewer::MeshVisual", "Gmsh : étiquette inconnue"));
    if (!n->source.empty()) s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Source : %1").arg(qs(n->source));
    const auto groups = m_model.groupsOfNode(n->id);
    if (!groups.empty()) s += "\n" + QCoreApplication::translate("TSA::Viewer::MeshVisual", "Groupes : %1").arg(qs(join(groups)));
    if (m_displacement)
    {
        const auto it = m_displacement->values.find(n->id);
        if (it == m_displacement->values.end())
            s += "\n" + fieldName(m_displacement->name) + " : " + QCoreApplication::translate("TSA::Viewer::MeshVisual", "non calculé");
        else
        {
            const auto& u = it->second;
            s += "\n" + fieldName(m_displacement->name) + " : " +
                 QCoreApplication::translate("TSA::Viewer::MeshVisual", "(%1 ; %2 ; %3) %4, norme %5 %4")
                     .arg(qs(fmt(u[0])), qs(fmt(u[1])), qs(fmt(u[2])), qs(m_displacement->unit),
                          qs(fmt(std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]))));
        }
    }
    if (m_colorField && m_colorField->location == TSA::Mesh::FieldLocation::Node && (!m_displacement || m_colorField->name != m_displacement->name))
    {
        double v = 0.0;
        s += "\n" + fieldName(m_colorField->name) + " : " +
             (m_colorField->scalar(n->id, &v) ? qs(fmt(v) + " " + m_colorField->unit) : QCoreApplication::translate("TSA::Viewer::MeshVisual", "non calculé"));
    }
    return s.toStdString();
}

void MeshVisual::clearHighlight()
{
    if (!context().IsNull() && !m_highlight.IsNull()) context()->Remove(m_highlight, false);
    m_highlight.Nullify();
    m_highlightElement = m_highlightNode = 0;
}

void MeshVisual::highlight(int elementId, int nodeId, bool zoom)
{
    if (!context().IsNull() && !m_highlight.IsNull()) context()->Remove(m_highlight, false);
    m_highlight.Nullify();
    m_highlightElement = elementId;
    m_highlightNode = nodeId;
    if (context().IsNull() || !m_mesh) return;
    Handle(OverlayObject) obj = new OverlayObject();
    obj->width = 4.0;
    obj->markerColor = kHighlight;
    obj->markerScale = 4.0;
    Bnd_Box box;
    if (const auto* e = elementId ? m_model.element(elementId) : nullptr)
        if (const ElementTopology* topo = elementTopology(e->type); topo && int(e->nodes.size()) == topo->nodeCount)
            for (std::size_t k = 0; k < topo->edges.size(); ++k)
            {
                std::vector<int> chain { e->nodes[std::size_t(topo->edges[k][0])] };
                if (topo->edgeMid[k] >= 0) chain.push_back(e->nodes[std::size_t(topo->edgeMid[k])]);
                chain.push_back(e->nodes[std::size_t(topo->edges[k][1])]);
                for (std::size_t j = 0; j + 1 < chain.size(); ++j)
                {
                    obj->addSegment(position(chain[j]), position(chain[j + 1]), kHighlight, kHighlight);
                    box.Add(position(chain[j]));
                    box.Add(position(chain[j + 1]));
                }
            }
    if (nodeId && m_model.node(nodeId))
    {
        obj->markers.push_back(position(nodeId));
        box.Add(position(nodeId));
    }
    if (box.IsVoid()) return;
    context()->Display(obj, 0, -1, false);
    context()->SetZLayer(obj, Graphic3d_ZLayerId_Topmost);
    m_highlight = obj;
    if (zoom && !view().IsNull())
    {
        box.Enlarge(std::max(0.5, std::sqrt(box.SquareExtent()) * 2.0));
        view()->FitAll(box, 0.1);
    }
    if (!view().IsNull()) view()->Redraw();
}

Bnd_Box MeshVisual::bounds() const
{
    Bnd_Box box;
    if (m_mesh)
        for (int id : m_model.visibleNodes()) box.Add(position(id));
    return box;
}

void MeshVisual::fit()
{
    const Handle(V3d_View) v = view();
    Bnd_Box box = bounds();
    if (v.IsNull() || box.IsVoid()) return;
    box.Enlarge(std::max(1e-3, std::sqrt(box.SquareExtent()) * 0.02));
    v->FitAll(box, 0.05);
    v->ZFitAll();
    v->Redraw();
}

} // namespace TSA::Viewer
