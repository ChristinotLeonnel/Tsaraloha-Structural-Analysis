// Outils de dessin : chaîne de poutres, rectangle de poutres, portique, contreventement en X,
// arc de poutres, poteaux aux intersections de grille. Les éléments reprennent les préréglages
// de création (sections, matériaux, angle β, treillis) et passent par ElementCreation : nœuds
// existants réutilisés (1 mm), validations, croisements signalés, outil entier annulé au premier refus.

#include "DimensionTools.h"
#include "ModifyTools.h"

#include "../ElementCreation.h"
#include "../../Grid/GridManager.h"
#include "../../Model/Model.h"
#include "../../UndoRedo/EditTransaction.h"

#include <ElCLib.hxx>
#include <gce_MakeCirc.hxx>
#include <gp_Vec.hxx>

#include <QCoreApplication>
#include <QString>

#include <array>
#include <cmath>
#include <set>
#include <sstream>

namespace TSA::Interaction
{

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

/// Éléments d'un outil créés par ElementCreation : mêmes validations, mêmes avertissements et même
/// réutilisation des nœuds que les modes Dessin. Tout l'outil tient dans une transaction : le premier
/// refus l'annule entièrement (aucun portique à moitié créé, aucun nœud abandonné).
class ToolBuilder
{
public:
    ToolBuilder(Model& m, const ToolContext& ctx, const std::string& toolName)
        : m_creation(m), m_ctx(ctx), m_toolName(toolName), m_tx(m, toolName)
    {
    }

    bool beam(const DrawPoint& a, const DrawPoint& b)
    {
        return bar(a, b, TSA::Model::BarRole::Beam, m_ctx.presets.beam.section, m_ctx.presets.beam.material,
                   m_ctx.presets.beam.betaAngle, m_ctx.presets.beam.color, m_result.created.beams);
    }
    bool column(const DrawPoint& a, const DrawPoint& b)
    {
        return bar(a, b, TSA::Model::BarRole::Column, m_ctx.presets.column.section, m_ctx.presets.column.material,
                   m_ctx.presets.column.betaAngle, m_ctx.presets.column.color, m_result.created.columns);
    }
    bool diagonal(const DrawPoint& a, const DrawPoint& b)
    {
        auto preset = m_ctx.presets.truss;
        preset.material = TSA::Model::Material::findByName(preset.material.name);
        preset.role = TSA::Model::TrussMemberRole::Diagonal;
        return record(m_creation.createTrussMember(a, b, preset), m_result.created.trussMembers);
    }

    /// Tout est créé : une entrée Annuler, message de succès suivi des réserves.
    ToolResult done(const std::string& message)
    {
        m_tx.commit();
        m_result.success = true;
        m_result.message = message;
        for (const auto& w : m_warnings)
            m_result.message += " " + QCoreApplication::translate("TSA::Interaction::DrawTools", "Attention : %1")
                                          .arg(QString::fromStdString(w)).toStdString();
        return m_result;
    }
    /// Refus : tout ce que l'outil a créé est annulé, motif de l'élément refusé.
    ToolResult refused()
    {
        m_tx.rollback();
        return { false,
                 QCoreApplication::translate("TSA::Interaction::DrawTools", "%1 non créé : %2")
                     .arg(QString::fromStdString(m_toolName), QString::fromStdString(m_refusal)).toStdString(),
                 {} };
    }

private:
    bool bar(const DrawPoint& a, const DrawPoint& b, TSA::Model::BarRole role, const TSA::Model::Section& section,
             const TSA::Model::Material& material, double rotation, const std::string& color, std::set<int>& into)
    {
        TSA::Model::BarProperties props;
        props.id = 0;
        props.name = section.name;
        props.role = role;
        props.section = section;
        props.material = TSA::Model::Material::findByName(material.name);   // le dialogue des préréglages ne garde que le nom
        props.rotation = rotation;
        props.color = color;
        return record(m_creation.createBar(a, b, props), into);
    }
    bool record(const CreationResult& c, std::set<int>& into)
    {
        if (!c.success)
        {
            m_refusal = c.message;
            return false;
        }
        into.insert(c.elementId);
        m_warnings.insert(m_warnings.end(), c.warnings.begin(), c.warnings.end());
        return true;
    }

    ElementCreation m_creation;
    const ToolContext& m_ctx;
    std::string m_toolName;
    TSA::UndoRedo::EditTransaction m_tx;
    ToolResult m_result;
    std::vector<std::string> m_warnings;
    std::string m_refusal;
};

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
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Chaîne de poutres").toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Dessiner des poutres consécutives (Entrée pour terminer)")
            .toStdString();
    }
    ToolCategory category() const override { return ToolCategory::Draw; }
    bool supportsDialog() const override { return false; }
    bool continuesAfterApply() const override { return true; }
    void reset() override { ModelingTool::reset(); m_finished = false; }
    std::string prompt() const override
    {
        return m_picks.empty()
            ? QCoreApplication::translate("TSA::Interaction::DrawTools", "Chaîne de poutres : cliquez le premier point").toStdString()
            : QCoreApplication::translate("TSA::Interaction::DrawTools",
                                          "Chaîne de poutres : cliquez le point suivant (Entrée : terminer, %1 poutre(s))")
                  .arg(static_cast<qulonglong>(m_picks.size() - 1)).toStdString();
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
        ToolBuilder b(m, ctx, name());
        for (std::size_t i = 1; i < m_picks.size(); ++i)
            if (!b.beam({ m_picks[i - 1].point, m_picks[i - 1].nodeId }, { m_picks[i].point, m_picks[i].nodeId })) return b.refused();
        return b.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "%1 poutre(s) créée(s).")
                          .arg(static_cast<qulonglong>(m_picks.size() - 1)).toStdString());
    }

private:
    bool m_finished = false;
};

class BeamRectangleTool final : public FixedPointsTool
{
public:
    BeamRectangleTool()
    {
        addParam(pointP("c1", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 1 (m)").toStdString()));
        addParam(pointP("c2", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin opposé (m)").toStdString(),
                        gp_Pnt(6, 5, 0)));
        m_pointKeys = { "c1", "c2" };
    }
    std::string id() const override { return "draw_beam_rectangle"; }
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Rectangle de poutres").toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Quatre poutres formant un rectangle dans le plan de travail")
            .toStdString();
    }
    std::string prompt() const override
    {
        return m_picks.empty()
            ? QCoreApplication::translate("TSA::Interaction::DrawTools", "Rectangle de poutres : cliquez un coin").toStdString()
            : QCoreApplication::translate("TSA::Interaction::DrawTools", "Rectangle de poutres : cliquez le coin opposé").toStdString();
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
            return { false, QCoreApplication::translate("TSA::Interaction::DrawTools",
                                                        "Rectangle : les deux coins doivent différer selon les deux axes du plan de travail.")
                                .toStdString() };
        ToolBuilder b(m, ctx, name());
        for (int i = 0; i < 4; ++i)
            if (!b.beam(DrawPoint(c[i]), DrawPoint(c[(i + 1) % 4]))) return b.refused();
        return b.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "Rectangle de 4 poutres créé.").toStdString());
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
        addParam(pointP("p1", QCoreApplication::translate("TSA::Interaction::DrawTools", "Pied gauche (m)").toStdString()));
        addParam(pointP("p2", QCoreApplication::translate("TSA::Interaction::DrawTools", "Pied droit (m)").toStdString(),
                        gp_Pnt(6, 0, 0)));
        addParam({ "height", QCoreApplication::translate("TSA::Interaction::DrawTools", "Hauteur (m)").toStdString(),
                   ParamType::Length, 3.0, {}, 0.01, 1000 });
        m_pointKeys = { "p1", "p2" };
    }
    std::string id() const override { return "draw_portal"; }
    std::string name() const override { return QCoreApplication::translate("TSA::Interaction::DrawTools", "Portique").toStdString(); }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Deux poteaux et une traverse à partir des deux pieds")
            .toStdString();
    }
    std::string prompt() const override
    {
        const QString h = QString::fromStdString(fmt(param("height")));
        return m_picks.empty()
            ? QCoreApplication::translate("TSA::Interaction::DrawTools",
                                          "Portique (hauteur %1 m, tapez une hauteur pour la changer) : cliquez le pied gauche")
                  .arg(h).toStdString()
            : QCoreApplication::translate("TSA::Interaction::DrawTools", "Portique (hauteur %1 m) : cliquez le pied droit")
                  .arg(h).toStdString();
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
        if (a.Distance(b) < 1e-6)
            return { false, QCoreApplication::translate("TSA::Interaction::DrawTools", "Portique : pieds confondus.").toStdString() };
        const gp_Vec up(0, 0, param("height"));
        ToolBuilder tb(m, ctx, name());
        const DrawPoint ta(a.Translated(up)), tt(b.Translated(up));
        if (!tb.column(DrawPoint(a), ta) || !tb.column(DrawPoint(b), tt) || !tb.beam(ta, tt)) return tb.refused();
        return tb.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "Portique créé (2 poteaux, 1 traverse).").toStdString());
    }
};

class XBracingTool final : public FixedPointsTool
{
public:
    XBracingTool()
    {
        addParam(pointP("a", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 1 (m)").toStdString()));
        addParam(pointP("b", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 2 (m)").toStdString(), gp_Pnt(6, 0, 0)));
        addParam(pointP("c", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 3 (m)").toStdString(), gp_Pnt(6, 0, 3)));
        addParam(pointP("d", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 4 (m)").toStdString(), gp_Pnt(0, 0, 3)));
        m_pointKeys = { "a", "b", "c", "d" };
    }
    std::string id() const override { return "draw_x_bracing"; }
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Contreventement en X").toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                           "Deux diagonales de treillis dans un panneau défini par ses 4 coins (dans l'ordre)")
            .toStdString();
    }
    std::string prompt() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                           "Contreventement en X : cliquez le coin %1 sur 4 (dans l'ordre du contour)")
            .arg(static_cast<qulonglong>(m_picks.size() + 1)).toStdString();
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
    ToolResult apply(Model& m, const ToolContext& ctx) override
    {
        const DrawPoint a(pointParam("a")), b(pointParam("b")), c(pointParam("c")), d(pointParam("d"));
        ToolBuilder tb(m, ctx, name());
        if (!tb.diagonal(a, c) || !tb.diagonal(b, d)) return tb.refused();
        return tb.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "Contreventement en X : 2 diagonales créées.")
                           .toStdString());
    }
};

class BeamArcTool final : public FixedPointsTool
{
public:
    BeamArcTool()
    {
        addParam(pointP("start", QCoreApplication::translate("TSA::Interaction::DrawTools", "Début (m)").toStdString()));
        addParam(pointP("mid", QCoreApplication::translate("TSA::Interaction::DrawTools", "Point de passage (m)").toStdString(),
                        gp_Pnt(5, 0, 2)));
        addParam(pointP("end", QCoreApplication::translate("TSA::Interaction::DrawTools", "Fin (m)").toStdString(), gp_Pnt(10, 0, 0)));
        addParam({ "segments", QCoreApplication::translate("TSA::Interaction::DrawTools", "Nombre de poutres").toStdString(),
                   ParamType::Count, 8, {}, 2, 1000 });
        m_pointKeys = { "start", "mid", "end" };
    }
    std::string id() const override { return "draw_beam_arc"; }
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Arc de poutres").toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Arc par 3 points discrétisé en N poutres droites")
            .toStdString();
    }
    std::string prompt() const override
    {
        const int n = static_cast<int>(param("segments"));
        switch (std::min<std::size_t>(m_picks.size(), 2))
        {
        case 0:
            return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                               "Arc de %1 poutres (tapez un nombre pour le changer) : cliquez le début")
                .arg(n).toStdString();
        case 1:
            return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                               "Arc de %1 poutres (tapez un nombre pour le changer) : cliquez un point de passage")
                .arg(n).toStdString();
        default:
            return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                               "Arc de %1 poutres (tapez un nombre pour le changer) : cliquez la fin")
                .arg(n).toStdString();
        }
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
            return { false, QCoreApplication::translate("TSA::Interaction::DrawTools", "Arc : les trois points sont alignés ou confondus.")
                                .toStdString() };
        ToolBuilder b(m, ctx, name());
        for (std::size_t i = 1; i < pts.size(); ++i)
            if (!b.beam(DrawPoint(pts[i - 1]), DrawPoint(pts[i]))) return b.refused();
        return b.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "Arc de %1 poutre(s) créé.")
                          .arg(static_cast<qulonglong>(pts.size() - 1)).toStdString());
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
        addParam(pointP("c1", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin 1 de la zone (m)").toStdString()));
        addParam(pointP("c2", QCoreApplication::translate("TSA::Interaction::DrawTools", "Coin opposé (m)").toStdString(),
                        gp_Pnt(20, 20, 0)));
        addParam({ "height", QCoreApplication::translate("TSA::Interaction::DrawTools", "Hauteur (m)").toStdString(),
                   ParamType::Length, 3.0, {}, 0.01, 1000 });
        m_pointKeys = { "c1", "c2" };
    }
    std::string id() const override { return "draw_grid_columns"; }
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools", "Poteaux sur grille").toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DrawTools",
                                           "Un poteau à chaque intersection de la grille active comprise dans une zone "
                                           "(pied à la cote du 1er coin)")
            .toStdString();
    }
    bool continuesAfterApply() const override { return false; }
    std::string prompt() const override
    {
        return m_picks.empty()
            ? QCoreApplication::translate("TSA::Interaction::DrawTools",
                                          "Poteaux sur grille : cliquez un coin de la zone (tapez une hauteur pour la changer)")
                  .toStdString()
            : QCoreApplication::translate("TSA::Interaction::DrawTools", "Poteaux sur grille : cliquez le coin opposé").toStdString();
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
        if (!grid || !grid->cartesian())
            return { false, QCoreApplication::translate("TSA::Interaction::DrawTools", "Poteaux sur grille : aucune grille cartésienne active.")
                                .toStdString() };
        const gp_Pnt a = pointParam("c1"), b = pointParam("c2");
        const double x0 = std::min(a.X(), b.X()) - 1e-6, x1 = std::max(a.X(), b.X()) + 1e-6;
        const double y0 = std::min(a.Y(), b.Y()) - 1e-6, y1 = std::max(a.Y(), b.Y()) + 1e-6;
        std::vector<gp_Pnt> feet;
        std::set<std::pair<long long, long long>> seen;
        for (const auto& gi : grid->cartesian()->intersections())
        {
            const double x = gi.point.X(), y = gi.point.Y();
            if (x < x0 || x > x1 || y < y0 || y > y1) continue;
            if (seen.insert({ std::llround(x * 1e4), std::llround(y * 1e4) }).second) feet.emplace_back(x, y, a.Z());
        }
        if (feet.empty())
            return { false, QCoreApplication::translate("TSA::Interaction::DrawTools", "Poteaux sur grille : aucune intersection dans la zone.")
                                .toStdString() };
        ToolBuilder tb(m, ctx, name());
        const gp_Vec up(0, 0, param("height"));
        for (const auto& f : feet)
            if (!tb.column(DrawPoint(f), DrawPoint(f.Translated(up)))) return tb.refused();
        return tb.done(QCoreApplication::translate("TSA::Interaction::DrawTools", "%1 poteau(x) créé(s).")
                           .arg(static_cast<qulonglong>(feet.size())).toStdString());
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
