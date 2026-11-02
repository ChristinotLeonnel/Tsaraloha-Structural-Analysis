// Coupe 2D du plan de travail (mode de vue par défaut du plan) : comme un plan AutoCAD, seuls les éléments contenus dans le plan
// sont dessinés, et ceux qui le traversent apparaissent par leur coupe (poteau en plan, dalle en élévation). Les résultats suivent
// les éléments du plan : aucun résultat d'un élément devant ou derrière ne se superpose. L'ancien mode (isolation par proximité,
// éléments qui traversent dessinés en entier) reste disponible.
#include "OccView.h"

#include "../Analysis/ResultsModel.h"
#include "../Model/Model.h"
#include "ResultsVisualManager.h"

#include <AIS_Shape.hxx>
#include <BOPAlgo_Tools.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <Bnd_Box.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <QSettings>

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto kPlaneViewKey = "Display/planeViewMode";
constexpr double kInset = 0.005;           ///< m : plan décalé dans l'élément quand il ne fait que toucher sa face d'extrémité
constexpr double kBarMarkerRadius = 0.06;  ///< m : repère d'une barre filaire (treillis, câble) coupée par le plan
} // namespace

OccView::PlaneViewMode OccView::storedPlaneViewMode()
{
    return QSettings().value(QString::fromLatin1(kPlaneViewKey)).toString() == QLatin1String("proximity") ? PlaneViewMode::Proximity
                                                                                                         : PlaneViewMode::Section;
}

void OccView::setPlaneViewMode(PlaneViewMode mode)
{
    if (m_planeViewMode == mode) return;
    m_planeViewMode = mode;
    QSettings().setValue(QString::fromLatin1(kPlaneViewKey), mode == PlaneViewMode::Section ? QStringLiteral("section") : QStringLiteral("proximity"));
    updateElementIsolation();
    if (!m_view.IsNull()) m_view->Redraw();
    emit planeViewModeChanged();
}

bool OccView::linearKeptOnPlane(int startNodeId, int endNodeId, double tol) const
{
    if (m_planeViewMode == PlaneViewMode::Section) return isNodeOnActiveWorkPlane(startNodeId, tol) && isNodeOnActiveWorkPlane(endNodeId, tol);
    return isLinearElementOnActiveWorkPlane(startNodeId, endNodeId, tol);
}

bool OccView::wallKeptOnPlane(int startNodeId, int endNodeId, double tol) const
{
    // Un voile s'élève à la verticale de ses deux nœuds : il n'est dans le plan que si le plan est vertical. Sur un plan
    // horizontal à sa base, il est coupé, comme à mi-hauteur.
    if (m_planeViewMode == PlaneViewMode::Section && std::abs(m_workPlane.normal().Z()) > 1e-6) return false;
    return linearKeptOnPlane(startNodeId, endNodeId, tol);
}

bool OccView::surfaceKeptOnPlane(const std::vector<int>& nodeIds, double tol) const
{
    if (m_planeViewMode != PlaneViewMode::Section) return isSurfaceElementOnActiveWorkPlane(nodeIds, tol);
    return !nodeIds.empty() && std::all_of(nodeIds.begin(), nodeIds.end(), [&](int id) { return isNodeOnActiveWorkPlane(id, tol); });
}

bool OccView::isResultElementVisible(const TSA::Analysis::ElementKey& key) const
{
    if (!m_model) return false;
    using Kind = TSA::Analysis::StructuralElementKind;
    const TSA::Model::ElementSet noIsolation;
    const TSA::Model::ElementSet& iso = m_isolatedElements ? *m_isolatedElements : noIsolation;
    auto kept = [&](ElementCategory category, const std::set<int>& isolated, const std::set<int>& hidden, int a, int b) {
        if (hidden.count(key.id) || (m_isolatedElements && !isolated.count(key.id))) return false;
        return isElementCategoryVisible(category) && keepLinearUnderIsolation(a, b);
    };
    switch (key.kind)
    {
    case Kind::Beam:
        if (const auto* e = m_model->getBeam(key.id)) return kept(ElementCategory::Beams, iso.beams, m_hiddenElements.beams, e->startNodeId(), e->endNodeId());
        return false;
    case Kind::Column:
        if (const auto* e = m_model->getColumn(key.id)) return kept(ElementCategory::Columns, iso.columns, m_hiddenElements.columns, e->startNodeId(), e->endNodeId());
        return false;
    case Kind::Truss:
        if (const auto* e = m_model->getTrussMember(key.id)) return kept(ElementCategory::Trusses, iso.trussMembers, m_hiddenElements.trussMembers, e->startNodeId(), e->endNodeId());
        return false;
    case Kind::Cable:
        if (const auto* e = m_model->getCable(key.id)) return kept(ElementCategory::Cables, iso.cables, m_hiddenElements.cables, e->startNodeId(), e->endNodeId());
        return false;
    default:
        return true;
    }
}

bool OccView::isResultNodeVisible(int nodeId) const
{
    if (m_hiddenElements.nodes.count(nodeId) || !keepNodeUnderIsolation(nodeId)) return false;
    if (!m_isolatedElements || !m_model) return true;
    // Isolation par éléments : une réaction reste visible si son nœud appartient à un élément isolé
    const auto& iso = *m_isolatedElements;
    if (iso.nodes.count(nodeId)) return true;
    auto ends = [&](const std::set<int>& ids, auto getter) {
        return std::any_of(ids.begin(), ids.end(), [&](int id) {
            const auto* e = getter(id);
            return e && (e->startNodeId() == nodeId || e->endNodeId() == nodeId);
        });
    };
    return ends(iso.beams, [&](int id) { return m_model->getBeam(id); }) || ends(iso.columns, [&](int id) { return m_model->getColumn(id); })
           || ends(iso.trussMembers, [&](int id) { return m_model->getTrussMember(id); }) || ends(iso.cables, [&](int id) { return m_model->getCable(id); });
}

void OccView::clearSectionCut()
{
    if (!m_context.IsNull())
        for (const auto& shape : m_sectionCutShapes) m_context->Remove(shape, false);
    m_sectionCutShapes.clear();
    m_sectionCutStamp.clear();
}

void OccView::updateSectionCut()
{
    if (m_context.IsNull()) return;
    if (!m_mode2DActive || m_planeViewMode != PlaneViewMode::Section || !m_model)
    {
        clearSectionCut();
        return;
    }

    // Calcul refait seulement si le plan, le modèle ou la visibilité ont changé (cette passe suit chaque mise à jour de la vue)
    const gp_Pnt origin = m_workPlane.origin();
    const gp_Dir normal = m_workPlane.normal();
    const std::size_t shapeCount = m_beamShapes.size() + m_columnShapes.size() + m_slabShapes.size() + m_wallShapes.size()
                                   + m_foundationShapes.size() + m_trussShapes.size() + m_cableShapes.size();
    std::vector<double> stamp { origin.X(), origin.Y(), origin.Z(), normal.X(), normal.Y(), normal.Z(), double(m_model->revision()),
                                double(m_hiddenElementCategories), double(m_hiddenElements.size()), m_isolatedElements ? double(m_isolatedElements->size()) : -1.0,
                                double(shapeCount) };
    const bool intact = std::all_of(m_sectionCutShapes.begin(), m_sectionCutShapes.end(), [&](const Handle(AIS_Shape)& s) { return m_context->IsDisplayed(s); });
    if (stamp == m_sectionCutStamp && intact) return;
    clearSectionCut();
    m_sectionCutStamp = std::move(stamp);

    const double tol = isolationTolerance();

    auto addCut = [&](const TopoDS_Shape& cut, const Quantity_Color& color, bool filled) {
        Handle(AIS_Shape) ais = new AIS_Shape(cut);
        ais->SetColor(color);
        if (filled)
        {
            ais->Attributes()->SetFaceBoundaryDraw(true);
            ais->Attributes()->SetFaceBoundaryAspect(new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.5));
        }
        else
            ais->SetWidth(2.5);
        // Dessin de coupe : ni sélectionnable ni surlignable, il ne remplace pas l'élément du modèle
        m_context->Display(ais, filled ? AIS_Shaded : AIS_WireFrame, -1, false);
        m_sectionCutShapes.push_back(ais);
    };

    auto cutShape = [&](const Handle(AIS_Shape)& element) -> bool {
        if (element.IsNull() || element->Shape().IsNull()) return false;
        const TopoDS_Shape& shape = element->Shape();
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        if (box.IsVoid()) return false;
        double x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
        box.Get(x0, y0, z0, x1, y1, z1);
        double wMin = 1e300, wMax = -1e300;
        for (const double x : { x0, x1 })
            for (const double y : { y0, y1 })
                for (const double z : { z0, z1 })
                {
                    double u = 0, v = 0, w = 0;
                    m_workPlane.toLocal(gp_Pnt(x, y, z), u, v, w);
                    wMin = std::min(wMin, w);
                    wMax = std::max(wMax, w);
                }
        if (wMax < -tol || wMin > tol) return false;
        // Élément posé sur le plan (poteau dont l'extrémité est au niveau) : coupe prise juste à l'intérieur, ramenée au plan
        const double inset = std::min(kInset, 0.25 * std::max(wMax - wMin, 1e-6));
        const double level = std::clamp(0.0, wMin + inset, std::max(wMin + inset, wMax - inset));
        const gp_Pln plane(origin.Translated(gp_Vec(normal) * level), normal);
        BRepAlgoAPI_Section section(shape, plane, false);
        section.Approximation(false);
        section.Build();
        if (!section.IsDone() || section.Shape().IsNull()) return false;
        TopExp_Explorer edge(section.Shape(), TopAbs_EDGE);
        if (!edge.More()) return false;

        gp_Trsf back;
        back.SetTranslation(gp_Vec(normal) * -level);
        const TopLoc_Location location(back);
        Quantity_Color color;
        element->Color(color);
        TopoDS_Shape wires, faces;
        const bool closed = BOPAlgo_Tools::EdgesToWires(section.Shape(), wires) == 0 && BOPAlgo_Tools::WiresToFaces(wires, faces)
                            && !faces.IsNull() && TopExp_Explorer(faces, TopAbs_FACE).More();
        addCut((closed ? faces : section.Shape()).Moved(location), color, closed);
        return true;
    };

    // Barre filaire coupée (treillis, câble sans volume) : repère à l'intersection
    auto markBar = [&](int a, int b, const Handle(AIS_Shape)& element) {
        const auto* n1 = m_model->getNode(a);
        const auto* n2 = m_model->getNode(b);
        if (!n1 || !n2) return;
        const gp_Pnt p1(n1->x(), n1->y(), n1->z()), p2(n2->x(), n2->y(), n2->z());
        double u = 0, v = 0, w1 = 0, w2 = 0;
        m_workPlane.toLocal(p1, u, v, w1);
        m_workPlane.toLocal(p2, u, v, w2);
        if (w1 * w2 > 0.0 || std::abs(w1 - w2) < 1e-12) return;
        const gp_Pnt at = p1.Translated(gp_Vec(p1, p2) * (w1 / (w1 - w2)));
        const TopoDS_Edge circle = BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(at, normal), kBarMarkerRadius));
        const TopoDS_Face disc = BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakeWire(circle).Wire());
        Quantity_Color color(Quantity_NOC_ORANGE);
        if (!element.IsNull()) element->Color(color);
        addCut(disc, color, true);
    };

    auto visible = [&](const std::set<int>& isolated, const std::set<int>& hidden, int id) {
        return !hidden.count(id) && (!m_isolatedElements || isolated.count(id));
    };
    const TSA::Model::ElementSet none;
    const TSA::Model::ElementSet& iso = m_isolatedElements ? *m_isolatedElements : none;

    if (isElementCategoryVisible(ElementCategory::Beams))
        for (const auto& [id, shape] : m_beamShapes)
            if (const auto* e = m_model->getBeam(id); e && visible(iso.beams, m_hiddenElements.beams, id) && !linearKeptOnPlane(e->startNodeId(), e->endNodeId(), tol))
                cutShape(shape);
    if (isElementCategoryVisible(ElementCategory::Columns))
        for (const auto& [id, shape] : m_columnShapes)
            if (const auto* e = m_model->getColumn(id); e && visible(iso.columns, m_hiddenElements.columns, id) && !linearKeptOnPlane(e->startNodeId(), e->endNodeId(), tol))
                cutShape(shape);
    if (isElementCategoryVisible(ElementCategory::Walls))
        for (const auto& [id, shape] : m_wallShapes)
            if (const auto* e = m_model->getWall(id); e && visible(iso.walls, m_hiddenElements.walls, id) && !wallKeptOnPlane(e->startNodeId(), e->endNodeId(), tol))
                cutShape(shape);
    if (isElementCategoryVisible(ElementCategory::Slabs))
        for (const auto& [id, shape] : m_slabShapes)
            if (const auto* e = m_model->getSlab(id); e && visible(iso.slabs, m_hiddenElements.slabs, id) && !surfaceKeptOnPlane(e->nodeIds(), tol))
                cutShape(shape);
    if (isElementCategoryVisible(ElementCategory::Foundations))
        for (const auto& [id, shape] : m_foundationShapes)
            if (const auto* e = m_model->getFoundation(id); e && visible(iso.foundations, m_hiddenElements.foundations, id) && !keepNodeUnderIsolation(e->nodeId()))
                cutShape(shape);
    if (isElementCategoryVisible(ElementCategory::Trusses))
        for (const auto& [id, shape] : m_trussShapes)
            if (const auto* e = m_model->getTrussMember(id); e && visible(iso.trussMembers, m_hiddenElements.trussMembers, id) && !linearKeptOnPlane(e->startNodeId(), e->endNodeId(), tol))
                if (!cutShape(shape)) markBar(e->startNodeId(), e->endNodeId(), shape);
    if (isElementCategoryVisible(ElementCategory::Cables))
        for (const auto& [id, shape] : m_cableShapes)
            if (const auto* e = m_model->getCable(id); e && visible(iso.cables, m_hiddenElements.cables, id) && !linearKeptOnPlane(e->startNodeId(), e->endNodeId(), tol))
                if (!cutShape(shape)) markBar(e->startNodeId(), e->endNodeId(), shape);
}
