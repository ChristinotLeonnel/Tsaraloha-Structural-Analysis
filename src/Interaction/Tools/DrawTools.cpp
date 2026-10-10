// Outils de dessin : chaîne de poutres, rectangle de poutres, portique, contreventement en X,
// arc de poutres, poteaux aux intersections de grille. Les éléments reprennent les préréglages
// de création (sections, matériaux, angle β) ; les nœuds existants sont réutilisés (1 mm).

#include "DimensionTools.h"
#include "ModifyTools.h"

#include "../../Grid/GridManager.h"
#include "../../Model/Model.h"

#include <ElCLib.hxx>
#include <gce_MakeCirc.hxx>
#include <gp_Vec.hxx>

#include <array>
#include <cmath>
#include <set>
#include <sstream>

namespace TSA::Interaction
{

using TSA::Model::ElementSet;
using TSA::Model::Model;

namespace
{
constexpr double kPi = 3.14159265358979323846;

std::string fmt(double v)
{
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(2);
    o << v;
    return o.str();
}

ToolParameter pointP(const std::string& key, const std::string& label, const gp_Pnt& p = gp_Pnt())
{
    return { key, label, ParamType::Point, 0.0, p };
}

int beam(Model& m, const ToolContext& ctx, int a, int b, ElementSet& out)
{
    if (a == b) return 0;
    const auto& p = ctx.presets.beam;
    const int id = m.addBar(a, b, p.section, p.material, TSA::Model::BarRole::Beam, p.betaAngle);
    if (id > 0) out.beams.insert(id);
    return id;
}

int column(Model& m, const ToolContext& ctx, int a, int b, ElementSet& out)
{
    if (a == b) return 0;
    const auto& p = ctx.presets.column;
    const int id = m.addColumn(a, b, p.section, p.material, p.betaAngle);
    if (id > 0) out.columns.insert(id);
    return id;
}

/// Axes du plan de travail (X, Y = N × X).
void planeAxes(const ToolContext& ctx, gp_Vec& x, gp_Vec& y)
{
    x = gp_Vec(ctx.planeX);
    y = gp_Vec(ctx.planeNormal).Crossed(x);
}

/// Base commune : un nombre fixe de points, mémorisés dans des paramètres Point.
class FixedPointsTool : public ModelingTool
{
public:
    ToolCategory category() const override { return ToolCategory::Draw; }
    bool continuesAfterApply() const override { return true; }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_picks.size() >= m_pointKeys.size(); }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (ready()) return;
        setPointParam(m_pointKeys[m_picks.size()], pick.point);
        m_picks.push_back(pick);
    }

protected:
    std::vector<std::string> m_pointKeys;
};

class BeamChainTool final : public ModelingTool
{
public:
    std::string id() const override { return "draw_beam_chain"; }
    std::string name() const override { return "Chaîne de poutres"; }
    std::string description() const override { return "Dessiner des poutres consécutives (Entrée pour terminer)"; }
    ToolCategory category() const override { return ToolCategory::Draw; }
    bool supportsDialog() const override { return false; }
    bool continuesAfterApply() const override { return true; }
    void reset() override { ModelingTool::reset(); m_finished = false; }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Chaîne de poutres : cliquez le premier point"
                               : "Chaîne de poutres : cliquez le point suivant (Entrée : terminer, " + std::to_string(m_picks.size() - 1) + " poutre(s))";
    }
    PickKind nextPick() const override { return m_finished ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_finished && m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (!m_picks.empty() && m_picks.back().point.Distance(pick.point) < 1e-6) return;
        m_picks.push_back(pick);
    }
    bool finish() override
    {
        m_finished = m_picks.size() >= 2;
        return m_finished;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        for (std::size_t i = 1; i < m_picks.size(); ++i) p.lines.push_back({ m_picks[i - 1].point, m_picks[i].point });
        if (!m_picks.empty()) p.lines.push_back({ m_picks.back().point, cursor });
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        ToolResult r;
        int prev = 0;
        for (const auto& pk : m_picks)
        {
            const int n = pk.nodeId > 0 && m.getNode(pk.nodeId) ? pk.nodeId : ToolGeometry::nodeAt(m, pk.point);
            if (prev) beam(m, ctx, prev, n, r.created);
            prev = n;
        }
        r.success = !r.created.beams.empty();
        r.message = std::to_string(r.created.beams.size()) + " poutre(s) créée(s).";
        return r;
    }

private:
    bool m_finished = false;
};

class BeamRectangleTool final : public FixedPointsTool
{
public:
    BeamRectangleTool()
    {
        addParam(pointP("c1", "Coin 1 (m)"));
        addParam(pointP("c2", "Coin opposé (m)", gp_Pnt(6, 5, 0)));
        m_pointKeys = { "c1", "c2" };
    }
    std::string id() const override { return "draw_beam_rectangle"; }
    std::string name() const override { return "Rectangle de poutres"; }
    std::string description() const override { return "Quatre poutres formant un rectangle dans le plan de travail"; }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Rectangle de poutres : cliquez un coin" : "Rectangle de poutres : cliquez le coin opposé";
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext& ctx) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1) return p;
        const auto c = corners(m_picks[0].point, cursor, ctx);
        for (int i = 0; i < 4; ++i) p.lines.push_back({ c[i], c[(i + 1) % 4] });
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const auto c = corners(pointParam("c1"), pointParam("c2"), ctx);
        if (c[0].Distance(c[1]) < 1e-6 || c[1].Distance(c[2]) < 1e-6)
            return { false, "Rectangle : les deux coins doivent différer selon les deux axes du plan de travail." };
        ToolResult r;
        int n[4];
        for (int i = 0; i < 4; ++i) n[i] = ToolGeometry::nodeAt(m, c[i]);
        for (int i = 0; i < 4; ++i) beam(m, ctx, n[i], n[(i + 1) % 4], r.created);
        r.success = true;
        r.message = "Rectangle de 4 poutres créé.";
        return r;
    }

private:
    static std::array<gp_Pnt, 4> corners(const gp_Pnt& a, const gp_Pnt& b, const ToolContext& ctx)
    {
        gp_Vec x, y;
        planeAxes(ctx, x, y);
        const gp_Vec d(a, b);
        const gp_Vec dx = x * d.Dot(x), dy = y * d.Dot(y);
        return { a, a.Translated(dx), a.Translated(dx + dy), a.Translated(dy) };
    }
};

class PortalTool final : public FixedPointsTool
{
public:
    PortalTool()
    {
        addParam(pointP("p1", "Pied gauche (m)"));
        addParam(pointP("p2", "Pied droit (m)", gp_Pnt(6, 0, 0)));
        addParam({ "height", "Hauteur (m)", ParamType::Length, 3.0, {}, 0.01, 1000 });
        m_pointKeys = { "p1", "p2" };
    }
    std::string id() const override { return "draw_portal"; }
    std::string name() const override { return "Portique"; }
    std::string description() const override { return "Deux poteaux et une traverse à partir des deux pieds"; }
    std::string prompt() const override
    {
        const std::string h = fmt(param("height"));
        return m_picks.empty() ? "Portique (hauteur " + h + " m, tapez une hauteur pour la changer) : cliquez le pied gauche"
                               : "Portique (hauteur " + h + " m) : cliquez le pied droit";
    }
    bool acceptValue(double v, const ToolContext&) override
    {
        if (v <= 0) return false;
        setParam("height", v);
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1) return p;
        const gp_Vec up(0, 0, param("height"));
        const gp_Pnt a = m_picks[0].point, b = cursor;
        p.lines = { { a, a.Translated(up) }, { a.Translated(up), b.Translated(up) }, { b, b.Translated(up) } };
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const gp_Pnt a = pointParam("p1"), b = pointParam("p2");
        if (a.Distance(b) < 1e-6) return { false, "Portique : pieds confondus." };
        const gp_Vec up(0, 0, param("height"));
        ToolResult r;
        const int na = ToolGeometry::nodeAt(m, a), nb = ToolGeometry::nodeAt(m, b);
        const int ta = ToolGeometry::nodeAt(m, a.Translated(up)), tb = ToolGeometry::nodeAt(m, b.Translated(up));
        column(m, ctx, na, ta, r.created);
        column(m, ctx, nb, tb, r.created);
        beam(m, ctx, ta, tb, r.created);
        r.success = true;
        r.message = "Portique créé (2 poteaux, 1 traverse).";
        return r;
    }
};

class XBracingTool final : public FixedPointsTool
{
public:
    XBracingTool()
    {
        addParam(pointP("a", "Coin 1 (m)"));
        addParam(pointP("b", "Coin 2 (m)", gp_Pnt(6, 0, 0)));
        addParam(pointP("c", "Coin 3 (m)", gp_Pnt(6, 0, 3)));
        addParam(pointP("d", "Coin 4 (m)", gp_Pnt(0, 0, 3)));
        m_pointKeys = { "a", "b", "c", "d" };
    }
    std::string id() const override { return "draw_x_bracing"; }
    std::string name() const override { return "Contreventement en X"; }
    std::string description() const override { return "Deux diagonales de treillis dans un panneau défini par ses 4 coins (dans l'ordre)"; }
    std::string prompt() const override
    {
        return "Contreventement en X : cliquez le coin " + std::to_string(m_picks.size() + 1) + " sur 4 (dans l'ordre du contour)";
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        std::vector<gp_Pnt> pts;
        for (const auto& pk : m_picks) pts.push_back(pk.point);
        pts.push_back(cursor);
        for (std::size_t i = 1; i < pts.size(); ++i) p.lines.push_back({ pts[i - 1], pts[i] });
        if (pts.size() >= 3) p.lines.push_back({ pts[0], pts[2] });
        if (pts.size() == 4) p.lines.push_back({ pts[1], pts[3] });
        return p;
    }
    ToolResult apply(Model& m, const ToolContext&) override
    {
        int n[4];
        const char* keys[] = { "a", "b", "c", "d" };
        for (int i = 0; i < 4; ++i) n[i] = ToolGeometry::nodeAt(m, pointParam(keys[i]));
        if (n[0] == n[2] || n[1] == n[3]) return { false, "Contreventement : coins confondus." };
        ToolResult r;
        r.created.trussMembers.insert(m.addTrussMember(n[0], n[2], 0.10, "", TSA::Model::TrussMemberRole::Diagonal));
        r.created.trussMembers.insert(m.addTrussMember(n[1], n[3], 0.10, "", TSA::Model::TrussMemberRole::Diagonal));
        r.success = true;
        r.message = "Contreventement en X : 2 diagonales créées (non reliées entre elles).";
        return r;
    }
};

class BeamArcTool final : public FixedPointsTool
{
public:
    BeamArcTool()
    {
        addParam(pointP("start", "Début (m)"));
        addParam(pointP("mid", "Point de passage (m)", gp_Pnt(5, 0, 2)));
        addParam(pointP("end", "Fin (m)", gp_Pnt(10, 0, 0)));
        addParam({ "segments", "Nombre de poutres", ParamType::Count, 8, {}, 2, 1000 });
        m_pointKeys = { "start", "mid", "end" };
    }
    std::string id() const override { return "draw_beam_arc"; }
    std::string name() const override { return "Arc de poutres"; }
    std::string description() const override { return "Arc par 3 points discrétisé en N poutres droites"; }
    std::string prompt() const override
    {
        const char* step[] = { "le début", "un point de passage", "la fin" };
        return "Arc de " + std::to_string(static_cast<int>(param("segments"))) + " poutres (tapez un nombre pour le changer) : cliquez "
               + step[std::min<std::size_t>(m_picks.size(), 2)];
    }
    bool acceptValue(double v, const ToolContext&) override { setParam("segments", std::round(v)); return true; }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.size() == 1) p.lines.push_back({ m_picks[0].point, cursor });
        if (m_picks.size() == 2)
        {
            std::vector<gp_Pnt> pts;
            if (arcPoints(m_picks[0].point, m_picks[1].point, cursor, static_cast<int>(param("segments")), pts))
                for (std::size_t i = 1; i < pts.size(); ++i) p.lines.push_back({ pts[i - 1], pts[i] });
        }
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        std::vector<gp_Pnt> pts;
        if (!arcPoints(pointParam("start"), pointParam("mid"), pointParam("end"), static_cast<int>(param("segments")), pts))
            return { false, "Arc : les trois points sont alignés ou confondus." };
        ToolResult r;
        int prev = 0;
        for (const auto& p : pts)
        {
            const int n = ToolGeometry::nodeAt(m, p);
            if (prev) beam(m, ctx, prev, n, r.created);
            prev = n;
        }
        r.success = !r.created.beams.empty();
        r.message = "Arc de " + std::to_string(r.created.beams.size()) + " poutre(s) créé.";
        return r;
    }

    static bool arcPoints(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& c, int n, std::vector<gp_Pnt>& out)
    {
        if (n < 1) return false;
        gce_MakeCirc mk(a, b, c);
        if (!mk.IsDone()) return false;
        const gp_Circ circ = mk.Value();
        const double u1 = ElCLib::Parameter(circ, a);
        auto rel = [&](const gp_Pnt& p) {
            double d = ElCLib::Parameter(circ, p) - u1;
            while (d < 0) d += 2 * kPi;
            while (d >= 2 * kPi) d -= 2 * kPi;
            return d;
        };
        const double a2 = rel(b), a3 = rel(c);
        const double sweep = a2 < a3 ? a3 : a3 - 2 * kPi;   // sens qui passe par b
        out.clear();
        for (int k = 0; k <= n; ++k) out.push_back(ElCLib::Value(u1 + sweep * k / n, circ));
        out.front() = a;
        out.back() = c;
        return true;
    }
};

class GridColumnsTool final : public FixedPointsTool
{
public:
    GridColumnsTool()
    {
        addParam(pointP("c1", "Coin 1 de la zone (m)"));
        addParam(pointP("c2", "Coin opposé (m)", gp_Pnt(20, 20, 0)));
        addParam({ "height", "Hauteur (m)", ParamType::Length, 3.0, {}, 0.01, 1000 });
        m_pointKeys = { "c1", "c2" };
    }
    std::string id() const override { return "draw_grid_columns"; }
    std::string name() const override { return "Poteaux sur grille"; }
    std::string description() const override
    {
        return "Un poteau à chaque intersection de la grille active comprise dans une zone (pied à la cote du 1er coin)";
    }
    bool continuesAfterApply() const override { return false; }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Poteaux sur grille : cliquez un coin de la zone (tapez une hauteur pour la changer)"
                               : "Poteaux sur grille : cliquez le coin opposé";
    }
    bool acceptValue(double v, const ToolContext&) override
    {
        if (v <= 0) return false;
        setParam("height", v);
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1) return p;
        const gp_Pnt a = m_picks[0].point;
        const gp_Pnt b(cursor.X(), a.Y(), a.Z()), c(cursor.X(), cursor.Y(), a.Z()), d(a.X(), cursor.Y(), a.Z());
        p.lines = { { a, b }, { b, c }, { c, d }, { d, a } };
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const auto* grid = ctx.grids ? ctx.grids->activeGrid() : nullptr;
        if (!grid || !grid->cartesian()) return { false, "Poteaux sur grille : aucune grille cartésienne active." };
        const gp_Pnt a = pointParam("c1"), b = pointParam("c2");
        const double x0 = std::min(a.X(), b.X()) - 1e-6, x1 = std::max(a.X(), b.X()) + 1e-6;
        const double y0 = std::min(a.Y(), b.Y()) - 1e-6, y1 = std::max(a.Y(), b.Y()) + 1e-6;
        std::set<std::pair<long long, long long>> seen;
        ToolResult r;
        for (const auto& gi : grid->cartesian()->intersections())
        {
            const double x = gi.point.X(), y = gi.point.Y();
            if (x < x0 || x > x1 || y < y0 || y > y1) continue;
            if (!seen.insert({ std::llround(x * 1e4), std::llround(y * 1e4) }).second) continue;
            const int base = ToolGeometry::nodeAt(m, gp_Pnt(x, y, a.Z()));
            const int top = ToolGeometry::nodeAt(m, gp_Pnt(x, y, a.Z() + param("height")));
            column(m, ctx, base, top, r.created);
        }
        r.success = !r.created.columns.empty();
        r.message = r.success ? std::to_string(r.created.columns.size()) + " poteau(x) créé(s)."
                              : "Poteaux sur grille : aucune intersection dans la zone.";
        return r;
    }
};

} // namespace

void registerDrawTools(ModelingToolRegistry& r)
{
    r.registerTool([] { return std::make_unique<BeamChainTool>(); });
    r.registerTool([] { return std::make_unique<BeamRectangleTool>(); });
    r.registerTool([] { return std::make_unique<PortalTool>(); });
    r.registerTool([] { return std::make_unique<XBracingTool>(); });
    r.registerTool([] { return std::make_unique<BeamArcTool>(); });
    r.registerTool([] { return std::make_unique<GridColumnsTool>(); });
}

void registerBuiltInModelingTools(ModelingToolRegistry& registry)
{
    registerModifyTools(registry);
    registerDrawTools(registry);
    registerDimensionTools(registry);
}

} // namespace TSA::Interaction
