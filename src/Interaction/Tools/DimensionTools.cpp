#include "DimensionTools.h"

#include "../../Annotation/DimensionGeometry.h"
#include "../../Annotation/DimensionService.h"
#include "../../Model/Model.h"

#include <gp_Vec.hxx>

namespace TSA::Interaction
{

namespace
{
using TSA::Annotation::Dimension;
using TSA::Annotation::DimensionKind;
using TSA::Annotation::MeasureAxis;

class DimensionTool final : public ModelingTool
{
public:
    DimensionTool(std::string id, std::string name, std::string description, DimensionKind kind, MeasureAxis axis, bool autoAxis = false)
        : m_id(std::move(id)), m_name(std::move(name)), m_description(std::move(description)), m_kind(kind), m_axis(axis), m_autoAxis(autoAxis)
    {
    }

    std::string id() const override { return m_id; }
    std::string name() const override { return m_name; }
    std::string description() const override { return m_description; }
    ToolCategory category() const override { return ToolCategory::Annotate; }
    bool supportsDialog() const override { return false; }
    bool continuesAfterApply() const override { return true; }

    void reset() override
    {
        ModelingTool::reset();
        m_collecting = true;
        m_frozen = 0;
    }

    std::string prompt() const override
    {
        const size_t n = anchorCount();
        switch (m_kind)
        {
        case DimensionKind::Angular:
            if (n == 0) return "Cotation angulaire : cliquez le sommet de l'angle";
            if (n == 1) return "Cliquez un point du premier bras (nœud, extrémité de barre…)";
            if (n == 2) return "Cliquez un point du second bras";
            return "Placez l'arc de cote (clic), ou tapez son rayon + Entrée";
        case DimensionKind::Level:
            if (n == 0) return "Cotation de niveau : cliquez le point dont la cote est affichée";
            return "Placez le texte de la cote de niveau (clic)";
        case DimensionKind::Chain:
        case DimensionKind::Cumulative:
            if (m_collecting)
                return n < 2 ? "Cliquez les points successifs (au moins 2)"
                             : "Cliquez le point suivant, ou Entrée pour placer la ligne de cote";
            return "Placez la ligne de cote (clic), ou tapez son décalage + Entrée";
        default:
            if (n == 0) return m_name + " : cliquez le premier point (nœud, extrémité, intersection…)";
            if (n == 1) return "Cliquez le second point";
            return "Placez la ligne de cote (clic), ou tapez son décalage + Entrée";
        }
    }

    PickKind nextPick() const override { return ready() ? PickKind::None : PickKind::Point; }

    void addPick(const ToolPick& pick, const ToolContext&) override
    {
        if (ready()) return;
        m_picks.push_back(pick);
        if (m_collecting && !isMultiPoint() && static_cast<int>(m_picks.size()) >= Dimension::requiredAnchors(m_kind))
        {
            m_collecting = false;
            m_frozen = m_picks.size();
        }
    }

    bool finish() override
    {
        if (isMultiPoint() && m_collecting && m_picks.size() >= 2)
        {
            m_collecting = false;
            m_frozen = m_picks.size();
            return true;
        }
        return false;
    }

    bool acceptValue(double value, const ToolContext& ctx) override
    {
        if (m_collecting || ready() || m_kind == DimensionKind::Level) return false;
        const std::vector<gp_Pnt> pts = anchorPoints();
        const gp_Pnt cursor = ctx.cursor.value_or(pts.back());
        gp_Pnt position;
        if (m_kind == DimensionKind::Angular)
        {
            gp_Vec dir(pts[0], cursor);
            if (dir.Magnitude() < 1e-9) dir = gp_Vec(pts[0], pts[1]);
            position = pts[0].Translated(dir.Normalized() * std::abs(value));
        }
        else
        {
            const gp_Pnt mid((pts.front().XYZ() + pts.back().XYZ()) * 0.5);
            gp_Vec along(pts.front(), pts.back());
            gp_Vec off(mid, cursor);
            if (along.Magnitude() > 1e-9) off -= along.Normalized() * off.Dot(along.Normalized());
            if (off.Magnitude() < 1e-9)
                off = along.Magnitude() > 1e-9 ? gp_Vec(ctx.planeNormal).Crossed(along) : gp_Vec(ctx.planeX);
            if (off.Magnitude() < 1e-9) off = gp_Vec(0, 0, 1);
            position = mid.Translated(off.Normalized() * value);
        }
        ToolPick p;
        p.point = position;
        m_picks.push_back(p);
        return true;
    }

    bool ready() const override { return !m_collecting && m_picks.size() == anchorCount() + 1 && anchorCount() > 0; }

    ToolPreview preview(const gp_Pnt& cursor, const ToolContext&) const override
    {
        ToolPreview out;
        const std::vector<gp_Pnt> pts = anchorPoints();
        if (m_collecting)
        {
            // Points déjà cliqués reliés au curseur (construction).
            for (size_t i = 0; i + 1 < pts.size(); ++i) out.lines.emplace_back(pts[i], pts[i + 1]);
            if (!pts.empty()) out.lines.emplace_back(pts.back(), cursor);
            return out;
        }
        Dimension d = makeDimension(cursor);
        const auto layout = TSA::Annotation::computeLayout(d, pts, TSA::Annotation::DimensionStyle());
        out.lines = layout.segments;
        return out;
    }

    ToolResult apply(TSA::Model::Model& model, const ToolContext&) override
    {
        ToolResult r;
        if (!ready())
        {
            r.message = "saisie incomplète";
            return r;
        }
        const Dimension d = makeDimension(m_picks.back().point);
        std::string error;
        const int id = TSA::Annotation::addDimension(model, d, &error);
        if (!id)
        {
            r.message = "cotation refusée : " + error;
            reset();
            return r;
        }
        const auto layout = TSA::Annotation::layoutFor(model, model.dimensions().items.at(id), model.dimensions().style);
        std::string text;
        for (const auto& t : layout.texts)
            if (t.text != "0") text += (text.empty() ? "" : ", ") + t.text;
        r.success = true;
        r.message = "cotation #" + std::to_string(id) + " créée (" + text + ")";
        reset(); // prêt pour la cotation suivante
        return r;
    }

private:
    bool isMultiPoint() const { return m_kind == DimensionKind::Chain || m_kind == DimensionKind::Cumulative; }

    /// Ancrages : clics de la saisie des points (figés à la fin de cette saisie) ; le clic suivant est la position.
    size_t anchorCount() const { return m_collecting ? m_picks.size() : m_frozen; }

    std::vector<gp_Pnt> anchorPoints() const
    {
        std::vector<gp_Pnt> pts;
        for (size_t i = 0; i < anchorCount() && i < m_picks.size(); ++i) pts.push_back(m_picks[i].point);
        return pts;
    }

    Dimension makeDimension(const gp_Pnt& position) const
    {
        Dimension d;
        d.kind = m_kind;
        d.axis = m_axis;
        const size_t n = std::min(anchorCount(), m_picks.size());
        for (size_t i = 0; i < n; ++i)
        {
            TSA::Annotation::DimensionAnchor a;
            a.nodeId = m_picks[i].nodeId > 0 ? m_picks[i].nodeId : -1;
            a.point = { m_picks[i].point.X(), m_picks[i].point.Y(), m_picks[i].point.Z() };
            d.anchors.push_back(a);
        }
        d.position = { position.X(), position.Y(), position.Z() };
        if (m_autoAxis && d.anchors.size() == 2)
            d.axis = TSA::Annotation::autoLinearAxis(m_picks[0].point, m_picks[1].point, position);
        return d;
    }

    std::string m_id, m_name, m_description;
    DimensionKind m_kind;
    MeasureAxis m_axis;
    bool m_autoAxis = false;
    bool m_collecting = true;
    size_t m_frozen = 0;
};

template <typename... Args>
ModelingToolRegistry::Factory factory(Args... args)
{
    return [=] { return std::make_unique<DimensionTool>(args...); };
}
} // namespace

void registerDimensionTools(ModelingToolRegistry& registry)
{
    registry.registerTool(factory(std::string("dim_aligned"), std::string("Cotation alignée"),
                                  std::string("Distance réelle entre deux points, même inclinée (vraie grandeur)."),
                                  DimensionKind::Linear, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_linear"), std::string("Cotation linéaire"),
                                  std::string("Distance projetée sur l'axe X, Y ou Z choisi selon la position du curseur "
                                              "(comme une cotation horizontale / verticale)."),
                                  DimensionKind::Linear, MeasureAxis::X, true));
    registry.registerTool(factory(std::string("dim_horizontal"), std::string("Cotation horizontale"),
                                  std::string("Distance en plan (projection sur le plan horizontal)."),
                                  DimensionKind::Linear, MeasureAxis::Horizontal, false));
    registry.registerTool(factory(std::string("dim_x"), std::string("Cotation suivant X"),
                                  std::string("Écart des coordonnées X entre deux points."), DimensionKind::Linear, MeasureAxis::X, false));
    registry.registerTool(factory(std::string("dim_y"), std::string("Cotation suivant Y"),
                                  std::string("Écart des coordonnées Y entre deux points."), DimensionKind::Linear, MeasureAxis::Y, false));
    registry.registerTool(factory(std::string("dim_z"), std::string("Cotation verticale (Z)"),
                                  std::string("Différence de niveau (écart des coordonnées Z) entre deux points."),
                                  DimensionKind::Linear, MeasureAxis::Z, false));
    registry.registerTool(factory(std::string("dim_angular"), std::string("Cotation angulaire"),
                                  std::string("Angle au sommet entre deux directions (barres, lignes) : sommet, puis un point "
                                              "de chaque bras."),
                                  DimensionKind::Angular, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_level"), std::string("Cotation de niveau"),
                                  std::string("Altitude d'un point par rapport à la référence de niveau du style."),
                                  DimensionKind::Level, MeasureAxis::Z, false));
    registry.registerTool(factory(std::string("dim_chain"), std::string("Cotation en chaîne"),
                                  std::string("Distances successives entre plusieurs points, sur une même ligne de cote "
                                              "(direction premier → dernier point)."),
                                  DimensionKind::Chain, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_cumulative"), std::string("Cotation cumulée"),
                                  std::string("Distances mesurées depuis le premier point (origine commune)."),
                                  DimensionKind::Cumulative, MeasureAxis::Aligned, false));
}

} // namespace TSA::Interaction
