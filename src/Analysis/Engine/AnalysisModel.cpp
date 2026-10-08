#include "AnalysisModel.h"

#include "../../Coordinate/GeometryTolerance.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Coordinate/WorkPlane.h"
#include "../../Coordinate/WorkPlaneManager.h"
#include "../../Grid/GridManager.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Analysis
{

// ---------------------------------------------------------------------------------------------
// AnalysisPlane / AnalysisMapping
// ---------------------------------------------------------------------------------------------

std::pair<double, double> AnalysisPlane::toPlane(double x, double y, double z) const
{
    const gp_Vec d(origin, gp_Pnt(x, y, z));
    return { d.Dot(gp_Vec(u)), d.Dot(gp_Vec(v)) };
}

double AnalysisPlane::distance(double x, double y, double z) const
{
    return gp_Vec(origin, gp_Pnt(x, y, z)).Dot(gp_Vec(n));
}

void AnalysisMapping::addNode(int tsaNodeId)
{
    if (m_nodeIndex.count(tsaNodeId)) return;
    m_nodesByIndex.push_back(tsaNodeId);
    m_nodeIndex[tsaNodeId] = static_cast<int>(m_nodesByIndex.size());
}

void AnalysisMapping::addElement(const ElementKey& key, int analysisIndex)
{
    m_elementIndex[key] = analysisIndex;
    m_elementsByIndex[analysisIndex] = key;
}

int AnalysisMapping::analysisNode(int tsaNodeId) const
{
    auto it = m_nodeIndex.find(tsaNodeId);
    return it != m_nodeIndex.end() ? it->second : 0;
}

int AnalysisMapping::tsaNode(int analysisIndex) const
{
    if (analysisIndex < 1 || analysisIndex > static_cast<int>(m_nodesByIndex.size())) return 0;
    return m_nodesByIndex[static_cast<std::size_t>(analysisIndex - 1)];
}

int AnalysisMapping::analysisElement(const ElementKey& key) const
{
    auto it = m_elementIndex.find(key);
    return it != m_elementIndex.end() ? it->second : 0;
}

std::optional<ElementKey> AnalysisMapping::tsaElement(int analysisIndex) const
{
    auto it = m_elementsByIndex.find(analysisIndex);
    if (it == m_elementsByIndex.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------------------------------
// AnalysisScopeResolver
// ---------------------------------------------------------------------------------------------

namespace
{
constexpr double kTol = TSA::Coordinate::GeometryTolerance::planeMembership;

std::string formatElevation(double z)
{
    std::ostringstream o;
    o << std::showpos << std::fixed << std::setprecision(2) << z << " m";
    return o.str();
}

TSA::Model::ElementSet intersect(const TSA::Model::ElementSet& a, const TSA::Model::ElementSet& b)
{
    auto inter = [](const std::set<int>& x, const std::set<int>& y) {
        std::set<int> r;
        std::set_intersection(x.begin(), x.end(), y.begin(), y.end(), std::inserter(r, r.begin()));
        return r;
    };
    TSA::Model::ElementSet r;
    r.nodes = inter(a.nodes, b.nodes);
    r.beams = inter(a.beams, b.beams);
    r.columns = inter(a.columns, b.columns);
    r.slabs = inter(a.slabs, b.slabs);
    r.walls = inter(a.walls, b.walls);
    r.foundations = inter(a.foundations, b.foundations);
    r.trussMembers = inter(a.trussMembers, b.trussMembers);
    r.cables = inter(a.cables, b.cables);
    return r;
}

/// Plan vertical d'un axe de grille cartésienne, dans le repère de la grille (origine + rotation Z),
/// avec la même transformation que CartesianGrid::rebuild.
std::optional<AnalysisPlane> gridAxisPlane(const TSA::Grid::GridDefinition& def, GridAxisFamily family,
                                           const std::string& label, std::string* error)
{
    const auto& positions = family == GridAxisFamily::X ? def.xPositions() : def.yPositions();
    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        const std::string l = family == GridAxisFamily::X ? def.getXLabel(i) : def.getYLabel(i);
        if (l != label) continue;

        const double r = def.rotationDeg() * M_PI / 180.0;
        const double c = std::cos(r), s = std::sin(r);
        const gp_Pnt& o = def.origin();
        AnalysisPlane p;
        p.v = gp_Dir(0, 0, 1);
        if (family == GridAxisFamily::Y)
        {
            // Ligne y = yPositions[i], parallèle à l'axe X local de la grille.
            const double ly = positions[i];
            p.origin = gp_Pnt(o.X() - ly * s, o.Y() + ly * c, o.Z());
            p.u = gp_Dir(c, s, 0);
        }
        else
        {
            // Ligne x = xPositions[i], parallèle à l'axe Y local de la grille.
            const double lx = positions[i];
            p.origin = gp_Pnt(o.X() + lx * c, o.Y() + lx * s, o.Z());
            p.u = gp_Dir(-s, c, 0);
        }
        p.n = p.u.Crossed(p.v);
        return p;
    }
    if (error) *error = "L'axe « " + label + " » n'existe pas dans la grille « " + def.name() + " ».";
    return std::nullopt;
}

TSA::Model::ElementSet onPlane(const TSA::Model::Model& model, const AnalysisPlane& p)
{
    const auto wp = TSA::Coordinate::WorkPlane::fromOriginAndAxes(p.origin, p.u, p.v, "Portée d'analyse");
    return TSA::Model::SelectionQuery::onWorkPlane(model, wp, kTol);
}

/// Plan contenant tous les nœuds du modèle. Vertical d'abord (nœuds alignés en plan : contient la pesanteur,
/// cas d'une poutre, d'un portique, d'un treillis), sinon horizontal (même cote).
/// Vertical : u = direction horizontale du plan (composante dominante positive), v = +Z, n = u × v.
std::optional<AnalysisPlane> modelPlane(const TSA::Model::Model& model, std::string* error, std::string* label)
{
    const auto& nodes = model.nodes();
    if (nodes.size() < 2)
    {
        if (error) *error = "Le modèle doit contenir au moins deux nœuds pour définir un plan.";
        return std::nullopt;
    }
    const auto& first = nodes.begin()->second;
    const double x0 = first.x(), y0 = first.y(), z0 = first.z();

    bool horizontal = true;
    double far = 0.0, dx = 0.0, dy = 0.0;
    for (const auto& [id, n] : nodes)
    {
        horizontal &= std::abs(n.z() - z0) <= kTol;
        const double ex = n.x() - x0, ey = n.y() - y0, d = std::hypot(ex, ey);
        if (d > far) { far = d; dx = ex; dy = ey; }
    }
    double ux = 1.0, uy = 0.0;
    if (far > kTol)
    {
        ux = dx / far;
        uy = dy / far;
        if (std::abs(ux) >= std::abs(uy) ? ux < 0.0 : uy < 0.0) { ux = -ux; uy = -uy; }
    }
    bool vertical = true;
    for (const auto& [id, n] : nodes)
        vertical &= std::abs(ux * (n.y() - y0) - uy * (n.x() - x0)) <= kTol;

    AnalysisPlane p;
    if (vertical)
    {
        p.origin = gp_Pnt(x0, y0, 0);
        p.u = gp_Dir(ux, uy, 0);
        p.v = gp_Dir(0, 0, 1);
        p.n = p.u.Crossed(p.v);
        if (label)
            *label = std::abs(uy) <= 1e-9 ? "Plan du modèle (vertical XZ)"
                   : std::abs(ux) <= 1e-9 ? "Plan du modèle (vertical YZ)" : "Plan du modèle (vertical)";
        return p;
    }
    if (horizontal)
    {
        p.origin = gp_Pnt(0, 0, z0);
        p.u = gp_Dir(1, 0, 0);
        p.v = gp_Dir(0, 1, 0);
        p.n = gp_Dir(0, 0, 1);
        if (label) *label = "Plan du modèle (horizontal, z = " + formatElevation(z0) + ")";
        return p;
    }
    if (error) *error = "Le modèle n'est pas plan : choisir un axe de grille, un niveau ou un plan de travail.";
    return std::nullopt;
}

template <typename Map>
std::set<int> existing(const std::set<int>& ids, const Map& items)
{
    std::set<int> r;
    for (int id : ids)
        if (items.count(id)) r.insert(id);
    return r;
}
} // namespace

ResolvedScope AnalysisScopeResolver::resolve(const TSA::Model::Model& model,
                                             const TSA::Grid::GridManager* grids,
                                             const AnalysisScope& scope)
{
    ResolvedScope r;
    switch (scope.type)
    {
    case ScopeType::EntireModel:
        r.entireModel = true;
        r.label = "Modèle complet";
        r.elements = TSA::Model::SelectionQuery::all(model);
        break;

    case ScopeType::SelectedElements:
        r.label = "Sélection";
        r.elements.nodes = existing(scope.nodes, model.nodes());
        r.elements.beams = existing(scope.beams, model.beams());
        r.elements.columns = existing(scope.columns, model.columns());
        r.elements.trussMembers = existing(scope.trussMembers, model.trussMembers());
        r.elements.cables = existing(scope.cables, model.cables());
        r.elements.slabs = existing(scope.slabs, model.slabs());
        r.elements.walls = existing(scope.walls, model.walls());
        r.elements.foundations = existing(scope.foundations, model.foundations());
        if (r.elements.empty())
        {
            r.error = "La sélection ne contient aucun objet du modèle.";
            return r;
        }
        break;

    case ScopeType::GridAxis:
    {
        const auto* grid = grids ? grids->getGrid(scope.gridId) : nullptr;
        if (!grid)
        {
            r.error = "Grille introuvable (« " + scope.gridId + " »).";
            return r;
        }
        if (grid->type() != TSA::Grid::GridType::Cartesian)
        {
            r.error = "Seuls les axes d'une grille cartésienne définissent un plan d'analyse.";
            return r;
        }
        auto plane = gridAxisPlane(grid->definition(), scope.axisFamily, scope.axisLabel, &r.error);
        if (!plane) return r;
        r.plane = plane;
        r.label = "Axe " + scope.axisLabel + " (" + grid->name() + ")";
        r.elements = onPlane(model, *plane);
        break;
    }

    case ScopeType::Level:
    {
        const auto* lm = model.levelManager();
        const auto* level = lm ? lm->getLevel(scope.levelId) : nullptr;
        if (!level)
        {
            r.error = "Niveau introuvable (« " + scope.levelId + " »).";
            return r;
        }
        AnalysisPlane p;
        p.origin = gp_Pnt(0, 0, level->elevation);
        p.u = gp_Dir(1, 0, 0);
        p.v = gp_Dir(0, 1, 0);
        p.n = gp_Dir(0, 0, 1);
        r.plane = p;
        r.label = "Niveau " + level->name + " (" + formatElevation(level->elevation) + ")";
        r.elements = TSA::Model::SelectionQuery::atElevation(model, level->elevation, kTol);
        break;
    }

    case ScopeType::WorkPlane:
    {
        const auto* wpm = model.workPlaneManager();
        const auto* wp = wpm ? wpm->getWorkPlane(scope.workPlaneId) : nullptr;
        if (!wp)
        {
            r.error = "Plan de travail introuvable (#" + std::to_string(scope.workPlaneId) + ").";
            return r;
        }
        AnalysisPlane p;
        p.origin = wp->origin();
        p.u = wp->axisX();
        p.v = wp->axisY();
        p.n = wp->normal();
        r.plane = p;
        r.label = "Plan de travail " + wp->name();
        r.elements = TSA::Model::SelectionQuery::onWorkPlane(model, *wp, kTol);
        break;
    }

    case ScopeType::ModelPlane:
    {
        auto plane = modelPlane(model, &r.error, &r.label);
        if (!plane) return r;
        r.plane = plane;
        r.elements = TSA::Model::SelectionQuery::all(model);
        break;
    }
    }

    // Restriction par niveau d'une portée (ex. axe B ∩ niveau 2) : intersection avec la cote.
    if (scope.hasLevelRestriction() && !r.entireModel)
    {
        const auto* lm = model.levelManager();
        const auto* level = lm ? lm->getLevel(scope.levelId) : nullptr;
        if (!level)
        {
            r.error = "Niveau introuvable (« " + scope.levelId + " »).";
            return r;
        }
        r.elements = intersect(r.elements, TSA::Model::SelectionQuery::atElevation(model, level->elevation, kTol));
        r.label += " ∩ niveau " + level->name;
    }

    r.ok = true;
    return r;
}

std::vector<AnalysisScopeResolver::ScopeOption> AnalysisScopeResolver::availableScopes(
    const TSA::Model::Model& model, const TSA::Grid::GridManager* grids)
{
    std::vector<ScopeOption> out;
    out.push_back({ AnalysisScope{}, "Modèle complet" });
    // Modèle plan (portique, poutre, treillis dessinés sans grille) : calculable par un moteur 2D tel quel.
    std::string planeLabel;
    if (modelPlane(model, nullptr, &planeLabel))
    {
        AnalysisScope s;
        s.type = ScopeType::ModelPlane;
        out.push_back({ s, planeLabel });
    }

    if (grids)
    {
        for (const auto& g : grids->grids())
        {
            if (!g || g->type() != TSA::Grid::GridType::Cartesian) continue;
            const auto& def = g->definition();
            auto addAxes = [&](GridAxisFamily family, std::size_t count) {
                for (std::size_t i = 0; i < count; ++i)
                {
                    AnalysisScope s;
                    s.type = ScopeType::GridAxis;
                    s.gridId = def.id();
                    s.axisFamily = family;
                    s.axisLabel = family == GridAxisFamily::X ? def.getXLabel(i) : def.getYLabel(i);
                    out.push_back({ s, "Axe " + s.axisLabel + " — " + def.name() });
                }
            };
            addAxes(GridAxisFamily::Y, def.yPositions().size());   // A, B, C…
            addAxes(GridAxisFamily::X, def.xPositions().size());   // 1, 2, 3…
        }
    }

    if (const auto* lm = model.levelManager())
    {
        for (const auto& level : lm->levels())
        {
            AnalysisScope s;
            s.type = ScopeType::Level;
            s.levelId = level.id;
            out.push_back({ s, "Niveau " + level.name + " (" + formatElevation(level.elevation) + ")" });
        }
    }

    if (const auto* wpm = model.workPlaneManager())
    {
        for (const auto& [id, wp] : wpm->workPlanes())
        {
            AnalysisScope s;
            s.type = ScopeType::WorkPlane;
            s.workPlaneId = id;
            out.push_back({ s, "Plan de travail " + wp.name() });
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// AnalysisModelExtractor
// ---------------------------------------------------------------------------------------------

AnalysisModel AnalysisModelExtractor::extract(const TSA::Model::Model& model,
                                              const ResolvedScope& scope,
                                              AnalysisDimension dimension)
{
    AnalysisModel am;
    am.dimension = dimension;
    am.scopeLabel = scope.label;
    am.entireModel = scope.entireModel;
    am.sourceRevision = model.revision();
    am.plane = scope.plane;

    // Une seule capture, restreinte à la portée (aucune géométrie OCCT n'est construite).
    am.snapshot = CalculationSnapshot::capture(model, scope.entireModel ? nullptr : &scope.elements);

    for (const auto& [id, node] : am.snapshot.nodes()) am.mapping.addNode(id);
    for (const auto& [tag, el] : am.snapshot.elements()) am.mapping.addElement(el.key(), tag);

    if (dimension == AnalysisDimension::Plane2D && am.plane)
    {
        for (const auto& [id, n] : am.snapshot.nodes())
        {
            am.planarCoordinates[id] = am.plane->toPlane(n.x, n.y, n.z);
            if (std::abs(am.plane->distance(n.x, n.y, n.z)) > kTol) am.outOfPlaneNodes.push_back(id);
        }
    }

    am.slabs.assign(scope.elements.slabs.begin(), scope.elements.slabs.end());
    am.walls.assign(scope.elements.walls.begin(), scope.elements.walls.end());
    am.foundations.assign(scope.elements.foundations.begin(), scope.elements.foundations.end());

    if (!scope.entireModel)
    {
        // Barres hors portée reliées à un nœud de la portée : leur effet est perdu (à signaler).
        auto scan = [&](const auto& items, StructuralElementKind kind) {
            for (const auto& [id, e] : items)
            {
                if (am.snapshot.hasElement(kind, id)) continue;
                if (am.snapshot.hasNode(e.startNodeId()) || am.snapshot.hasNode(e.endNodeId()))
                    am.crossingElements.push_back({ kind, id });
            }
        };
        scan(model.beams(), StructuralElementKind::Beam);
        scan(model.columns(), StructuralElementKind::Column);
        scan(model.trussMembers(), StructuralElementKind::Truss);
        scan(model.cables(), StructuralElementKind::Cable);

        const auto& lm = model.loadManager();
        am.droppedNodalLoads = lm.nodalLoads().size() - am.snapshot.nodalLoads().size();
        am.droppedMemberLoads = lm.memberLoads().size() - am.snapshot.memberLoads().size();
    }
    return am;
}

} // namespace TSA::Analysis
