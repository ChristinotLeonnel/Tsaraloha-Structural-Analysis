// Outils de modification : déplacer, copier, rotation, symétrie, échelle, réseaux linéaire et
// polaire, diviser (N tronçons / au point), intersecter, prolonger, ajuster, décaler, fusionner.
// Chaque outil traduit la saisie 3D en paramètres ; apply() n'utilise que les paramètres et les
// cibles (barres cliquées, sinon sélection).

#include "ModifyTools.h"

#include "../../Model/Model.h"
#include "../../Model/ModelCleanup.h"

#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace TSA::Interaction
{

using TSA::Model::ElementKind;
using TSA::Model::ElementSet;
using TSA::Model::Model;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1e-6;

std::string fmt(double v, int decimals = 3)
{
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(decimals);
    o << v;
    return o.str();
}

ToolParameter length(const std::string& key, const std::string& label, double v, double min = -1e6)
{
    return { key, label, ParamType::Length, v, {}, min, 1e6 };
}

ToolParameter makePointParam(const std::string& key, const std::string& label, const gp_Pnt& p = gp_Pnt())
{
    ToolParameter t { key, label, ParamType::Point, 0.0, p };
    return t;
}

/// Direction du curseur depuis `from` (saisie d'une distance au clavier).
std::optional<gp_Dir> cursorDirection(const gp_Pnt& from, const ToolContext& ctx)
{
    if (!ctx.cursor || ctx.cursor->Distance(from) < 1e-9) return std::nullopt;
    return gp_Dir(gp_Vec(from, *ctx.cursor));
}

/// Angle signé de (a - c) vers (b - c) autour de n.
double signedAngle(const gp_Pnt& c, const gp_Pnt& a, const gp_Pnt& b, const gp_Dir& n)
{
    gp_Vec va(c, a), vb(c, b);
    const gp_Vec nv(n);
    va -= nv * va.Dot(nv);
    vb -= nv * vb.Dot(nv);
    if (va.Magnitude() < 1e-12 || vb.Magnitude() < 1e-12) return 0.0;
    return va.AngleWithRef(vb, nv);
}

bool isBar(ElementKind k)
{
    return k == ElementKind::Beam || k == ElementKind::Column || k == ElementKind::TrussMember;
}

/// Barres d'une sélection (poutres, poteaux, treillis).
std::vector<BarRef> barsOf(const ElementSet& s)
{
    std::vector<BarRef> out;
    for (int id : s.beams) out.push_back({ ElementKind::Beam, id });
    for (int id : s.columns) out.push_back({ ElementKind::Column, id });
    for (int id : s.trussMembers) out.push_back({ ElementKind::TrussMember, id });
    return out;
}

void addTarget(ElementSet& s, const BarRef& b)
{
    switch (b.kind)
    {
    case ElementKind::Beam: s.beams.insert(b.id); break;
    case ElementKind::Column: s.columns.insert(b.id); break;
    case ElementKind::TrussMember: s.trussMembers.insert(b.id); break;
    case ElementKind::Cable: s.cables.insert(b.id); break;
    default: break;
    }
}

/// Supprime un nœud devenu inutile (aucun élément, appui ni charge nodale).
void removeIfOrphan(Model& m, int nodeId)
{
    const auto* n = m.getNode(nodeId);
    if (!n || !m.isNodeFree(nodeId) || n->support().isSupported()) return;
    for (const auto& [id, nl] : m.loadManager().nodalLoads())
        if (nl.nodeId() == nodeId) return;
    m.removeNode(nodeId);
}

/// Reporte l'extrémité (début ou fin) d'une barre sur un autre nœud.
bool reconnectEnd(Model& m, const BarRef& bar, bool start, int nodeId)
{
    auto apply = [&](auto* e, auto notify) {
        if (!e) return false;
        if (start) e->setStartNodeId(nodeId); else e->setEndNodeId(nodeId);
        (m.*notify)(bar.id);
        return true;
    };
    switch (bar.kind)
    {
    case ElementKind::Beam: return apply(m.getBeam(bar.id), &Model::notifyBeamModified);
    case ElementKind::Column: return apply(m.getColumn(bar.id), &Model::notifyColumnModified);
    case ElementKind::TrussMember: return apply(m.getTrussMember(bar.id), &Model::notifyTrussMemberModified);
    default: return false;
    }
}

bool removeBar(Model& m, const BarRef& bar)
{
    switch (bar.kind)
    {
    case ElementKind::Beam: return m.removeBeam(bar.id);
    case ElementKind::Column: return m.removeColumn(bar.id);
    case ElementKind::TrussMember: return m.removeTrussMember(bar.id);
    default: return false;
    }
}

/// Nœud au paramètre t d'une barre : extrémité si t ≈ 0 / 1, sinon division de la barre.
/// Retourne 0 si la division est refusée (charges non redistribuables).
int nodeOnBar(Model& m, const BarRef& bar, double t, int preferredNode = 0)
{
    int s = 0, e = 0;
    gp_Pnt a, b;
    if (!ToolGeometry::barEnds(m, bar, a, b, &s, &e)) return 0;
    const double tolT = 1e-3 / std::max(a.Distance(b), 1e-9);
    if (t <= tolT) return s;
    if (t >= 1.0 - tolT) return e;
    return m.splitBarAt(bar.kind, bar.id, t, preferredNode);
}

// =============================================================================================
// Transformations de la sélection
// =============================================================================================

class TranslateToolBase : public ModelingTool
{
public:
    TranslateToolBase()
    {
        addParam(length("dx", "Translation X (m)", 0.0));
        addParam(length("dy", "Translation Y (m)", 0.0));
        addParam(length("dz", "Translation Z (m)", 0.0));
    }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool needsSelection() const override { return true; }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_picks.size() >= 2 || m_typed; }
    void reset() override { ModelingTool::reset(); m_typed = false; }

    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        m_picks.push_back(pick);
        if (m_picks.size() == 2) setVector(gp_Vec(m_picks[0].point, m_picks[1].point));
    }
    bool acceptValue(double v, const ToolContext& ctx) override
    {
        if (m_picks.size() != 1) return false;
        const auto dir = cursorDirection(m_picks[0].point, ctx);
        if (!dir) return false;
        setVector(gp_Vec(*dir) * v);
        m_typed = true;
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1) return p;
        p.lines.push_back({ m_picks[0].point, cursor });
        gp_Trsf t;
        t.SetTranslation(gp_Vec(m_picks[0].point, cursor));
        p.selectionTransform = t;
        return p;
    }

protected:
    void setVector(const gp_Vec& v) { setParam("dx", v.X()); setParam("dy", v.Y()); setParam("dz", v.Z()); }
    gp_Vec vector() const { return gp_Vec(param("dx"), param("dy"), param("dz")); }
    std::string basePrompt(const std::string& verb) const
    {
        return m_picks.empty() ? verb + " : cliquez le point de base"
                               : verb + " : cliquez la destination (ou tapez une distance dans la direction du curseur + Entrée)";
    }
    bool m_typed = false;
};

class MoveTool final : public TranslateToolBase
{
public:
    std::string id() const override { return "move"; }
    std::string name() const override { return "Déplacer"; }
    std::string description() const override { return "Déplacer la sélection d'un point de base vers une destination"; }
    std::string prompt() const override { return basePrompt("Déplacer"); }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const auto nodes = ToolGeometry::nodeClosure(m, targets(ctx));
        const gp_Vec v = vector();
        if (nodes.empty() || v.Magnitude() < kTol) return { false, "Déplacer : sélection vide ou vecteur nul." };
        m.moveNodes(nodes, v.X(), v.Y(), v.Z());
        return { true, "Déplacement de " + std::to_string(nodes.size()) + " nœud(s) de (" + fmt(v.X()) + ", " + fmt(v.Y()) + ", " + fmt(v.Z()) + ") m." };
    }
};

class CopyTool final : public TranslateToolBase
{
public:
    CopyTool() { addParam({ "count", "Nombre de copies", ParamType::Count, 1, {}, 1, 1000 }); }
    std::string id() const override { return "copy"; }
    std::string name() const override { return "Copier"; }
    std::string description() const override { return "Copier la sélection par translation (copies répétées possibles)"; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override { return basePrompt("Copier"); }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const ElementSet s = targets(ctx);
        const gp_Vec v = vector();
        const int n = static_cast<int>(param("count"));
        if (s.empty() || v.Magnitude() < kTol) return { false, "Copier : sélection vide ou vecteur nul." };
        const auto ids = m.copyElements(s.nodes, s.beams, s.columns, s.slabs, v.X(), v.Y(), v.Z(), n, s.cables, s.trussMembers);
        return { !ids.empty(), std::to_string(n) + " copie(s) : " + std::to_string(ids.size()) + " objet(s) créé(s)." };
    }
};

class LinearArrayTool final : public TranslateToolBase
{
public:
    LinearArrayTool() { addParam({ "count", "Nombre de copies", ParamType::Count, 3, {}, 1, 1000 }); }
    std::string id() const override { return "array_linear"; }
    std::string name() const override { return "Réseau linéaire"; }
    std::string description() const override { return "Répéter la sélection N fois selon un pas (vecteur base → point)"; }
    std::string prompt() const override
    {
        const std::string n = std::to_string(static_cast<int>(param("count")));
        return m_picks.empty() ? "Réseau linéaire (" + n + " copies, tapez un nombre pour le changer) : cliquez le point de base"
                               : "Réseau linéaire (" + n + " copies) : cliquez le point définissant le pas (ou tapez le pas)";
    }
    bool acceptValue(double v, const ToolContext& ctx) override
    {
        if (m_picks.empty()) { setParam("count", std::round(v)); return true; }
        return TranslateToolBase::acceptValue(v, ctx);
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext& ctx) const override
    {
        ToolPreview p = TranslateToolBase::preview(cursor, ctx);
        if (m_picks.size() == 1)
        {
            const gp_Vec step(m_picks[0].point, cursor);
            for (int k = 2; k <= static_cast<int>(param("count")); ++k)
                p.lines.push_back({ m_picks[0].point.Translated(step * (k - 1)), m_picks[0].point.Translated(step * k) });
        }
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const ElementSet s = targets(ctx);
        const gp_Vec v = vector();
        const int n = static_cast<int>(param("count"));
        if (s.empty() || v.Magnitude() < kTol) return { false, "Réseau : sélection vide ou pas nul." };
        const auto ids = m.copyElements(s.nodes, s.beams, s.columns, s.slabs, v.X(), v.Y(), v.Z(), n, s.cables, s.trussMembers);
        return { !ids.empty(), "Réseau linéaire : " + std::to_string(n) + " copie(s), " + std::to_string(ids.size()) + " objet(s)." };
    }
};

class RotateTool final : public ModelingTool
{
public:
    RotateTool()
    {
        addParam(makePointParam("center", "Centre (m)"));
        addParam(makePointParam("axis", "Axe (direction)", gp_Pnt(0, 0, 1)));
        addParam({ "angle", "Angle (°)", ParamType::Angle, 90.0, {}, -360, 360 });
        addParam({ "copy", "Conserver l'original (copie)", ParamType::Bool, 0.0, {}, 0, 1 });
        addParam({ "count", "Nombre de copies", ParamType::Count, 1, {}, 1, 1000 });
    }
    std::string id() const override { return "rotate"; }
    std::string name() const override { return "Rotation"; }
    std::string description() const override { return "Faire tourner (ou copier en tournant) la sélection autour de la normale du plan de travail"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool needsSelection() const override { return true; }
    void prepare(const ToolContext& ctx) override
    {
        setPointParam("axis", gp_Pnt(ctx.planeNormal.X(), ctx.planeNormal.Y(), ctx.planeNormal.Z()));
    }
    void reset() override { ModelingTool::reset(); m_typed = false; }
    std::string prompt() const override
    {
        switch (m_picks.size())
        {
        case 0: return "Rotation : cliquez le centre";
        case 1: return "Rotation : cliquez la direction de référence (ou tapez l'angle en degrés + Entrée)";
        default: return "Rotation : cliquez la nouvelle direction (Ctrl + clic : copie)";
        }
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_picks.size() >= 3 || m_typed; }
    void addPick(const ToolPick& pick, const ToolContext& ctx) override
    {
        m_picks.push_back(pick);
        if (m_picks.size() == 1) setPointParam("center", pick.point);
        if (m_picks.size() == 3)
        {
            setParam("angle", signedAngle(m_picks[0].point, m_picks[1].point, m_picks[2].point, axis(ctx)) * 180.0 / kPi);
            setParam("copy", pick.modifier ? 1.0 : 0.0);
            setParam("count", 1);
        }
    }
    bool acceptValue(double v, const ToolContext&) override
    {
        if (m_picks.empty()) return false;
        setParam("angle", v);
        setParam("count", 1);
        m_typed = true;
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext& ctx) const override
    {
        ToolPreview p;
        if (m_picks.empty()) return p;
        p.lines.push_back({ m_picks[0].point, m_picks.size() >= 2 ? m_picks[1].point : cursor });
        if (m_picks.size() == 2)
        {
            p.lines.push_back({ m_picks[0].point, cursor });
            gp_Trsf t;
            t.SetRotation(gp_Ax1(m_picks[0].point, axis(ctx)), signedAngle(m_picks[0].point, m_picks[1].point, cursor, axis(ctx)));
            p.selectionTransform = t;
        }
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const ElementSet s = targets(ctx);
        const gp_Pnt c = pointParam("center");
        const gp_Pnt a = pointParam("axis");
        const gp_Vec av(a.X(), a.Y(), a.Z());
        const double rad = param("angle") * kPi / 180.0;
        if (s.empty() || av.Magnitude() < kTol || std::abs(rad) < 1e-9) return { false, "Rotation : sélection vide, axe nul ou angle nul." };
        const gp_Dir axisDir(av);
        if (param("copy") > 0.5)
        {
            const auto ids = m.copyAndRotateElements(s.nodes, s.beams, s.columns, s.slabs, c, axisDir, rad,
                                                     static_cast<int>(param("count")), s.cables, s.trussMembers);
            return { !ids.empty(), "Copie par rotation de " + fmt(param("angle"), 1) + "° : " + std::to_string(ids.size()) + " objet(s)." };
        }
        gp_Trsf t;
        t.SetRotation(gp_Ax1(c, axisDir), rad);
        const auto nodes = ToolGeometry::nodeClosure(m, s);
        m.transformNodes(nodes, t);
        return { true, "Rotation de " + fmt(param("angle"), 1) + "° : " + std::to_string(nodes.size()) + " nœud(s)." };
    }

private:
    gp_Dir axis(const ToolContext& ctx) const { (void)ctx; const gp_Pnt a = pointParam("axis"); return gp_Dir(a.X(), a.Y(), a.Z()); }
    bool m_typed = false;
};

class PolarArrayTool final : public ModelingTool
{
public:
    PolarArrayTool()
    {
        addParam(makePointParam("center", "Centre (m)"));
        addParam(makePointParam("axis", "Axe (direction)", gp_Pnt(0, 0, 1)));
        addParam({ "count", "Nombre total d'exemplaires", ParamType::Count, 4, {}, 2, 1000 });
        addParam({ "total", "Angle total (°)", ParamType::Angle, 360.0, {}, -360, 360 });
    }
    std::string id() const override { return "array_polar"; }
    std::string name() const override { return "Réseau polaire"; }
    std::string description() const override { return "Répartir N exemplaires de la sélection autour d'un centre"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool needsSelection() const override { return true; }
    void prepare(const ToolContext& ctx) override
    {
        setPointParam("axis", gp_Pnt(ctx.planeNormal.X(), ctx.planeNormal.Y(), ctx.planeNormal.Z()));
    }
    std::string prompt() const override
    {
        return "Réseau polaire (" + std::to_string(static_cast<int>(param("count"))) + " exemplaires sur " + fmt(param("total"), 0)
               + "°, tapez un nombre pour le changer) : cliquez le centre";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return !m_picks.empty(); }
    void addPick(const ToolPick& pick, const ToolContext&) override { m_picks.push_back(pick); setPointParam("center", pick.point); }
    bool acceptValue(double v, const ToolContext&) override { setParam("count", std::round(v)); return true; }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const ElementSet s = targets(ctx);
        const int n = static_cast<int>(param("count"));
        const double total = param("total");
        const gp_Pnt a = pointParam("axis");
        if (s.empty() || n < 2 || gp_Vec(a.X(), a.Y(), a.Z()).Magnitude() < kTol) return { false, "Réseau polaire : sélection vide." };
        const bool fullCircle = std::abs(std::abs(total) - 360.0) < 1e-9;
        const double step = (fullCircle ? total / n : total / (n - 1)) * kPi / 180.0;
        const auto ids = m.copyAndRotateElements(s.nodes, s.beams, s.columns, s.slabs, pointParam("center"),
                                                 gp_Dir(a.X(), a.Y(), a.Z()), step, n - 1, s.cables, s.trussMembers);
        return { !ids.empty(), "Réseau polaire : " + std::to_string(n) + " exemplaires, " + std::to_string(ids.size()) + " objet(s) créé(s)." };
    }
};

class MirrorTool final : public ModelingTool
{
public:
    MirrorTool()
    {
        addParam(makePointParam("p1", "Point 1 de l'axe (m)"));
        addParam(makePointParam("p2", "Point 2 de l'axe (m)", gp_Pnt(1, 0, 0)));
        addParam(makePointParam("normal", "Normale du plan de travail", gp_Pnt(0, 0, 1)));
        addParam({ "keep", "Conserver l'original (copie miroir)", ParamType::Bool, 1.0, {}, 0, 1 });
    }
    std::string id() const override { return "mirror"; }
    std::string name() const override { return "Symétrie"; }
    std::string description() const override
    {
        return "Symétrie par rapport au plan contenant l'axe P1-P2 et perpendiculaire au plan de travail";
    }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool needsSelection() const override { return true; }
    void prepare(const ToolContext& ctx) override
    {
        setPointParam("normal", gp_Pnt(ctx.planeNormal.X(), ctx.planeNormal.Y(), ctx.planeNormal.Z()));
    }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Symétrie : cliquez le 1er point de l'axe de symétrie"
                               : "Symétrie : cliquez le 2e point de l'axe (Ctrl + clic : retourner sans copier)";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        m_picks.push_back(pick);
        setPointParam(m_picks.size() == 1 ? "p1" : "p2", pick.point);
        if (m_picks.size() == 2) setParam("keep", pick.modifier ? 0.0 : 1.0);
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1) return p;
        p.lines.push_back({ m_picks[0].point, cursor });
        gp_Dir n;
        if (planeNormal(m_picks[0].point, cursor, n))
        {
            gp_Trsf t;
            t.SetMirror(gp_Ax2(m_picks[0].point, n));
            p.selectionTransform = t;
        }
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const ElementSet s = targets(ctx);
        gp_Dir n;
        if (s.empty() || !planeNormal(pointParam("p1"), pointParam("p2"), n))
            return { false, "Symétrie : sélection vide ou axe parallèle à la normale du plan de travail." };
        const bool keep = param("keep") > 0.5;
        const auto ids = m.mirrorElements(s.nodes, s.beams, s.columns, s.slabs, pointParam("p1"), n, keep, s.cables);
        std::string msg = keep ? "Copie miroir : " + std::to_string(ids.size()) + " objet(s) créé(s)."
                               : "Symétrie : " + std::to_string(ids.size()) + " nœud(s) déplacé(s).";
        if (!s.trussMembers.empty()) msg += " (Les treillis ne sont pas pris en charge par la symétrie.)";
        return { !ids.empty(), msg };
    }

private:
    bool planeNormal(const gp_Pnt& a, const gp_Pnt& b, gp_Dir& out) const
    {
        const gp_Pnt np = pointParam("normal");
        const gp_Vec axisV(a, b), wn(np.X(), np.Y(), np.Z());
        const gp_Vec n = axisV.Crossed(wn);
        if (n.Magnitude() < 1e-9) return false;
        out = gp_Dir(n);
        return true;
    }
};

class ScaleTool final : public ModelingTool
{
public:
    ScaleTool()
    {
        addParam(makePointParam("base", "Point de base (m)"));
        addParam({ "factor", "Facteur d'échelle", ParamType::Factor, 1.0, {}, 1e-6, 1e6 });
    }
    std::string id() const override { return "scale"; }
    std::string name() const override { return "Échelle"; }
    std::string description() const override { return "Agrandir / réduire la géométrie de la sélection depuis un point de base"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool needsSelection() const override { return true; }
    void reset() override { ModelingTool::reset(); m_typed = false; }
    std::string prompt() const override
    {
        switch (m_picks.size())
        {
        case 0: return "Échelle : cliquez le point de base";
        case 1: return "Échelle : tapez le facteur + Entrée, ou cliquez un point de référence";
        default: return "Échelle : cliquez la nouvelle position du point de référence";
        }
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }
    bool ready() const override { return m_picks.size() >= 3 || m_typed; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        m_picks.push_back(pick);
        if (m_picks.size() == 1) setPointParam("base", pick.point);
        if (m_picks.size() == 3)
        {
            const double r = m_picks[0].point.Distance(m_picks[1].point);
            if (r > kTol) setParam("factor", m_picks[0].point.Distance(m_picks[2].point) / r);
        }
    }
    bool acceptValue(double v, const ToolContext&) override
    {
        if (m_picks.empty() || v <= 0) return false;
        setParam("factor", v);
        m_typed = true;
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview p;
        if (m_picks.empty()) return p;
        p.lines.push_back({ m_picks[0].point, cursor });
        if (m_picks.size() == 2)
        {
            const double r = m_picks[0].point.Distance(m_picks[1].point);
            if (r > kTol)
            {
                gp_Trsf t;
                t.SetScale(m_picks[0].point, m_picks[0].point.Distance(cursor) / r);
                p.selectionTransform = t;
            }
        }
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const double f = param("factor");
        const auto nodes = ToolGeometry::nodeClosure(m, targets(ctx));
        if (nodes.empty() || f <= 0 || std::abs(f - 1.0) < 1e-12) return { false, "Échelle : sélection vide ou facteur 1." };
        gp_Trsf t;
        t.SetScale(pointParam("base"), f);
        m.transformNodes(nodes, t);
        return { true, "Échelle " + fmt(f, 4) + " : " + std::to_string(nodes.size()) + " nœud(s)." };
    }

private:
    bool m_typed = false;
};

// =============================================================================================
// Topologie des barres
// =============================================================================================

class SplitTool final : public ModelingTool
{
public:
    SplitTool() { addParam({ "segments", "Nombre de tronçons", ParamType::Count, 2, {}, 2, 1000 }); }
    std::string id() const override { return "split"; }
    std::string name() const override { return "Diviser en N"; }
    std::string description() const override { return "Diviser des barres en N tronçons égaux"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override
    {
        return "Diviser en " + std::to_string(static_cast<int>(param("segments")))
               + " (tapez un nombre pour le changer) : cliquez une barre";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Bar; }
    bool ready() const override { return !m_picks.empty(); }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (!pick.bar) return;
        m_picks.push_back(pick);
        addTarget(m_targets, *pick.bar);
    }
    bool acceptValue(double v, const ToolContext&) override { setParam("segments", std::round(v)); return true; }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const int n = static_cast<int>(param("segments"));
        int done = 0, refused = 0;
        for (const auto& bar : barsOf(targets(ctx)))
        {
            std::vector<int> parts;
            if (bar.kind == ElementKind::Beam) parts = m.splitBeam(bar.id, n);
            else if (bar.kind == ElementKind::Column) parts = m.splitColumn(bar.id, n);
            else
            {
                // Treillis : divisions successives au point (même règle de charges).
                int current = bar.id;
                bool ok = true;
                for (int k = n; k >= 2 && ok; --k)
                {
                    int created = 0;
                    ok = m.splitBarAt(ElementKind::TrussMember, current, 1.0 / k, 0, &created) != 0;
                    current = created;
                }
                if (ok) parts.assign(static_cast<std::size_t>(n), 0);
            }
            parts.empty() ? ++refused : ++done;
        }
        if (done == 0) return { false, "Diviser : aucune barre divisée (charges ponctuelles ou partielles, ou aucune barre)." };
        std::string msg = std::to_string(done) + " barre(s) divisée(s) en " + std::to_string(n) + ".";
        if (refused) msg += " " + std::to_string(refused) + " refusée(s) (charges non redistribuables).";
        return { true, msg };
    }
};

class SplitAtPointTool final : public ModelingTool
{
public:
    SplitAtPointTool() { addParam({ "ratio", "Position relative (0–1)", ParamType::Factor, 0.5, {}, 0.0, 1.0 }); }
    std::string id() const override { return "split_at"; }
    std::string name() const override { return "Diviser au point"; }
    std::string description() const override { return "Diviser une barre au point cliqué (accroché aux nœuds, milieux, grilles)"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override { return "Diviser au point : cliquez la barre à l'endroit de la division"; }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Bar; }
    bool ready() const override { return !m_picks.empty(); }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (!pick.bar) return;
        m_picks.push_back(pick);
        addTarget(m_targets, *pick.bar);
        setParam("ratio", pick.barParameter);
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const double t = param("ratio");
        int done = 0;
        const int existing = (!m_picks.empty() && m_picks[0].nodeId > 0) ? m_picks[0].nodeId : 0;
        for (const auto& bar : barsOf(targets(ctx)))
            if (m.splitBarAt(bar.kind, bar.id, t, existing)) ++done;
        return { done > 0, done > 0 ? std::to_string(done) + " barre(s) divisée(s) à " + fmt(t * 100.0, 1) + " %."
                                    : "Diviser au point : division impossible (extrémité, charges non redistribuables)." };
    }
};

class IntersectTool final : public ModelingTool
{
public:
    IntersectTool() { addParam(length("tol", "Tolérance de croisement (m)", 0.001, 0.0)); }
    std::string id() const override { return "intersect"; }
    std::string name() const override { return "Intersecter"; }
    std::string description() const override
    {
        return "Créer un nœud commun aux croisements de barres (division des barres qui se coupent)";
    }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Intersecter : cliquez la 1re barre" : "Intersecter : cliquez la 2e barre";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Bar; }
    bool ready() const override { return m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (!pick.bar) return;
        m_picks.push_back(pick);
        addTarget(m_targets, *pick.bar);
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        std::vector<BarRef> bars = barsOf(targets(ctx));
        const double tol = param("tol");
        int joints = 0;
        // Chaque paire au plus une fois ; les barres créées par division sont traitées au tour suivant.
        for (std::size_t i = 0; i < bars.size(); ++i)
            for (std::size_t j = i + 1; j < bars.size(); ++j)
                joints += intersectPair(m, bars[i], bars[j], tol, bars) ? 1 : 0;
        return { joints > 0, joints > 0 ? std::to_string(joints) + " nœud(s) d'intersection créé(s)." : "Intersecter : aucune barre ne se croise." };
    }

private:
    static bool intersectPair(Model& m, const BarRef& a, const BarRef& b, double tol, std::vector<BarRef>& bars)
    {
        // Logique commune au nettoyage du modèle (ModelCleanup::connectCrossingBars).
        std::vector<std::pair<ElementKind, int>> created;
        if (!TSA::Model::ModelCleanup::connectCrossingBars(m, a.kind, a.id, b.kind, b.id, tol, &created)) return false;
        for (const auto& [k, id] : created) bars.push_back({ k, id });
        return true;
    }
};

class ExtendTool final : public ModelingTool
{
public:
    std::string id() const override { return "extend"; }
    std::string name() const override { return "Prolonger"; }
    std::string description() const override { return "Prolonger une barre jusqu'à une barre limite (nœud commun créé sur la limite)"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool supportsDialog() const override { return false; }
    bool continuesAfterApply() const override { return true; }
    void reset() override { ModelingTool::reset(); }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Prolonger : cliquez la barre limite"
                               : "Prolonger : cliquez la barre à prolonger, près de l'extrémité à allonger";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Bar; }
    bool ready() const override { return m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (pick.bar && isBar(pick.bar->kind)) m_picks.push_back(pick);
    }
    ToolResult apply(Model& m, const ToolContext&) override
    {
        if (m_picks.size() < 2) return { false, "Prolonger : deux barres requises." };
        const BarRef boundary = *m_picks[0].bar, target = *m_picks[1].bar;
        gp_Pnt b1, b2, t1, t2;
        int ts = 0, te = 0;
        if (!ToolGeometry::barEnds(m, boundary, b1, b2) || !ToolGeometry::barEnds(m, target, t1, t2, &ts, &te))
            return { false, "Prolonger : barre introuvable." };
        double tt = 0, tb = 0;
        if (!ToolGeometry::closestParameters(t1, t2, b1, b2, tt, tb)) return { false, "Prolonger : barres parallèles." };
        const gp_Pnt pt = t1.Translated(gp_Vec(t1, t2) * tt), pb = b1.Translated(gp_Vec(b1, b2) * tb);
        if (pt.Distance(pb) > 1e-3) return { false, "Prolonger : la barre ne rencontre pas la limite (droites non sécantes)." };
        if (tb < -1e-6 || tb > 1 + 1e-6) return { false, "Prolonger : le point de rencontre est hors de la barre limite." };
        const bool atStart = m_picks[1].barParameter < 0.5;
        if ((atStart && tt > 1e-9) || (!atStart && tt < 1 - 1e-9))
            return { false, "Prolonger : la limite n'est pas du côté de l'extrémité cliquée (utilisez Ajuster pour raccourcir)." };

        const int node = nodeOnBar(m, boundary, tb);
        if (!node) return { false, "Prolonger : division de la barre limite impossible (charges non redistribuables)." };
        const int oldNode = atStart ? ts : te;
        reconnectEnd(m, target, atStart, node);
        removeIfOrphan(m, oldNode);
        return { true, "Barre prolongée jusqu'au nœud N" + std::to_string(node) + "." };
    }
};

class TrimTool final : public ModelingTool
{
public:
    std::string id() const override { return "trim"; }
    std::string name() const override { return "Ajuster"; }
    std::string description() const override { return "Couper une barre sur une barre de coupe et supprimer la partie cliquée"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool supportsDialog() const override { return false; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override
    {
        return m_picks.empty() ? "Ajuster : cliquez la barre de coupe" : "Ajuster : cliquez la partie de barre à supprimer";
    }
    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Bar; }
    bool ready() const override { return m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (pick.bar && isBar(pick.bar->kind)) m_picks.push_back(pick);
    }
    ToolResult apply(Model& m, const ToolContext&) override
    {
        if (m_picks.size() < 2) return { false, "Ajuster : deux barres requises." };
        const BarRef cutter = *m_picks[0].bar, target = *m_picks[1].bar;
        gp_Pnt c1, c2, t1, t2;
        int ts = 0, te = 0;
        if (!ToolGeometry::barEnds(m, cutter, c1, c2) || !ToolGeometry::barEnds(m, target, t1, t2, &ts, &te))
            return { false, "Ajuster : barre introuvable." };
        double tt = 0, tc = 0;
        if (!ToolGeometry::closestParameters(t1, t2, c1, c2, tt, tc)) return { false, "Ajuster : barres parallèles." };
        const gp_Pnt pt = t1.Translated(gp_Vec(t1, t2) * tt), pc = c1.Translated(gp_Vec(c1, c2) * tc);
        const double eps = 1e-3 / std::max(t1.Distance(t2), 1e-9);
        if (pt.Distance(pc) > 1e-3 || tt <= eps || tt >= 1 - eps || tc < -1e-6 || tc > 1 + 1e-6)
            return { false, "Ajuster : les barres ne se coupent pas à l'intérieur de la barre à ajuster." };

        const int node = nodeOnBar(m, cutter, tc);
        if (!node) return { false, "Ajuster : division de la barre de coupe impossible." };
        int endPart = 0;
        if (!m.splitBarAt(target.kind, target.id, tt, node, &endPart))
            return { false, "Ajuster : division impossible (charges non redistribuables)." };
        const bool removeStart = m_picks[1].barParameter < tt;
        const BarRef removed = removeStart ? target : BarRef { target.kind, endPart };
        const int freeEnd = removeStart ? ts : te;
        removeBar(m, removed);
        removeIfOrphan(m, freeEnd);
        return { true, "Barre ajustée au nœud N" + std::to_string(node) + "." };
    }
};

class OffsetTool final : public ModelingTool
{
public:
    OffsetTool()
    {
        addParam(length("distance", "Distance (m)", 1.0, 0.0));
        addParam({ "opposite", "Côté opposé", ParamType::Bool, 0.0, {}, 0, 1 });
    }
    std::string id() const override { return "offset"; }
    std::string name() const override { return "Décaler"; }
    std::string description() const override { return "Copier une barre parallèlement à elle-même, à une distance donnée"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    bool continuesAfterApply() const override { return true; }
    std::string prompt() const override
    {
        const std::string d = fmt(param("distance"));
        return m_picks.empty() ? "Décaler de " + d + " m (tapez une distance pour la changer) : cliquez la barre"
                               : "Décaler de " + d + " m : cliquez le côté du décalage";
    }
    PickKind nextPick() const override
    {
        if (ready()) return PickKind::None;
        return m_picks.empty() ? PickKind::Bar : PickKind::Point;
    }
    bool ready() const override { return m_picks.size() >= 2; }
    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (m_picks.empty())
        {
            if (!pick.bar || !isBar(pick.bar->kind)) return;
            addTarget(m_targets, *pick.bar);
        }
        m_picks.push_back(pick);
    }
    bool acceptValue(double v, const ToolContext&) override
    {
        if (v <= 0) return false;
        setParam("distance", v);
        return true;
    }
    ToolPreview preview(const gp_Pnt& cursor, const ToolContext& ctx) const override
    {
        ToolPreview p;
        if (m_picks.size() != 1 || !m_picks[0].bar) return p;
        p.lines.push_back({ m_picks[0].point, cursor });
        (void)ctx;
        return p;
    }
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        int created = 0;
        for (const auto& bar : barsOf(targets(ctx)))
        {
            gp_Pnt a, b;
            if (!ToolGeometry::barEnds(m, bar, a, b)) continue;
            const gp_Vec axis(a, b);
            if (axis.Magnitude() < kTol) continue;
            gp_Vec dir;
            if (m_picks.size() >= 2)
            {
                // Côté : composante, perpendiculaire à la barre, du vecteur barre → clic.
                const gp_Vec ax = axis.Normalized();
                gp_Vec side(a, m_picks[1].point);
                side -= ax * side.Dot(ax);
                if (side.Magnitude() < kTol) continue;
                dir = side.Normalized();
            }
            else
            {
                gp_Vec n = gp_Vec(ctx.planeNormal).Crossed(axis);
                if (n.Magnitude() < kTol) n = gp_Vec(ctx.planeX).Crossed(axis);
                if (n.Magnitude() < kTol) continue;
                dir = n.Normalized() * (param("opposite") > 0.5 ? -1.0 : 1.0);
            }
            const gp_Vec v = dir * param("distance");
            ElementSet one;
            addTarget(one, bar);
            created += m.copyElements(one.nodes, one.beams, one.columns, one.slabs, v.X(), v.Y(), v.Z(), 1,
                                      one.cables, one.trussMembers).empty() ? 0 : 1;
        }
        return { created > 0, created > 0 ? std::to_string(created) + " barre(s) décalée(s) de " + fmt(param("distance")) + " m."
                                           : "Décaler : aucune barre (ou côté indéterminé)." };
    }
};

class MergeNodesTool final : public ModelingTool
{
public:
    MergeNodesTool() { addParam(length("tol", "Tolérance (m)", 0.001, 1e-6)); }
    std::string id() const override { return "merge_nodes"; }
    std::string name() const override { return "Fusionner les nœuds"; }
    std::string description() const override { return "Fusionner les nœuds confondus (éléments, appuis et charges reportés)"; }
    ToolCategory category() const override { return ToolCategory::Modify; }
    std::string prompt() const override { return "Fusion des nœuds confondus à " + fmt(param("tol") * 1000.0, 1) + " mm"; }
    PickKind nextPick() const override { return PickKind::None; }
    bool ready() const override { return true; }
    void addPick(const ToolPick&, const ToolContext&) override {}
    ToolResult apply(Model& m, const ToolContext&) override
    {
        const int n = m.mergeCoincidentNodes(param("tol"));
        return { n > 0, n > 0 ? std::to_string(n) + " nœud(s) fusionné(s)." : "Aucun nœud confondu." };
    }
};

} // namespace

void registerModifyTools(ModelingToolRegistry& r)
{
    r.registerTool([] { return std::make_unique<MoveTool>(); });
    r.registerTool([] { return std::make_unique<CopyTool>(); });
    r.registerTool([] { return std::make_unique<RotateTool>(); });
    r.registerTool([] { return std::make_unique<MirrorTool>(); });
    r.registerTool([] { return std::make_unique<ScaleTool>(); });
    r.registerTool([] { return std::make_unique<LinearArrayTool>(); });
    r.registerTool([] { return std::make_unique<PolarArrayTool>(); });
    r.registerTool([] { return std::make_unique<OffsetTool>(); });
    r.registerTool([] { return std::make_unique<SplitTool>(); });
    r.registerTool([] { return std::make_unique<SplitAtPointTool>(); });
    r.registerTool([] { return std::make_unique<IntersectTool>(); });
    r.registerTool([] { return std::make_unique<ExtendTool>(); });
    r.registerTool([] { return std::make_unique<TrimTool>(); });
    r.registerTool([] { return std::make_unique<MergeNodesTool>(); });
}

} // namespace TSA::Interaction
