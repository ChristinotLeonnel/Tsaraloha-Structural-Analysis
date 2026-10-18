#include "DimensionTools.h"

#include "../../Annotation/DimensionGeometry.h"
#include "../../Annotation/DimensionService.h"
#include "../../Model/Model.h"

#include <gp_Vec.hxx>

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

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
    /// name et description : textes source français marqués QT_TRANSLATE_NOOP, traduits à l'affichage.
    DimensionTool(std::string id, std::string name, std::string description, DimensionKind kind, MeasureAxis axis, bool autoAxis = false)
        : m_id(std::move(id)), m_name(std::move(name)), m_description(std::move(description)), m_kind(kind), m_axis(axis), m_autoAxis(autoAxis)
    {
    }

    std::string id() const override { return m_id; }
    std::string name() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DimensionTools", m_name.c_str()).toStdString();
    }
    std::string description() const override
    {
        return QCoreApplication::translate("TSA::Interaction::DimensionTools", m_description.c_str()).toStdString();
    }
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
        QString text;
        switch (m_kind)
        {
        case DimensionKind::Angular:
            if (n == 0) text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cotation angulaire : cliquez le sommet de l'angle");
            else if (n == 1)
                text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cliquez un point du premier bras (nœud, extrémité de barre…)");
            else if (n == 2) text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cliquez un point du second bras");
            else text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Placez l'arc de cote (clic), ou tapez son rayon + Entrée");
            break;
        case DimensionKind::Level:
            if (n == 0)
                text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cotation de niveau : cliquez le point dont la cote est affichée");
            else text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Placez le texte de la cote de niveau (clic)");
            break;
        case DimensionKind::Chain:
        case DimensionKind::Cumulative:
            if (m_collecting)
                text = n < 2 ? QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cliquez les points successifs (au moins 2)")
                             : QCoreApplication::translate("TSA::Interaction::DimensionTools",
                                                           "Cliquez le point suivant, ou Entrée pour placer la ligne de cote");
            else
                text = QCoreApplication::translate("TSA::Interaction::DimensionTools",
                                                   "Placez la ligne de cote (clic), ou tapez son décalage + Entrée");
            break;
        default:
            if (n == 0)
                text = QCoreApplication::translate("TSA::Interaction::DimensionTools",
                                                   "%1 : cliquez le premier point (nœud, extrémité, intersection…)")
                           .arg(QString::fromStdString(name()));
            else if (n == 1) text = QCoreApplication::translate("TSA::Interaction::DimensionTools", "Cliquez le second point");
            else
                text = QCoreApplication::translate("TSA::Interaction::DimensionTools",
                                                   "Placez la ligne de cote (clic), ou tapez son décalage + Entrée");
            break;
        }
        return text.toStdString();
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
            r.message = QCoreApplication::translate("TSA::Interaction::DimensionTools", "saisie incomplète").toStdString();
            return r;
        }
        const Dimension d = makeDimension(m_picks.back().point);
        std::string error;
        const int id = TSA::Annotation::addDimension(model, d, &error);
        if (!id)
        {
            r.message = QCoreApplication::translate("TSA::Interaction::DimensionTools", "cotation refusée : %1")
                            .arg(QString::fromStdString(error)).toStdString();
            reset();
            return r;
        }
        const auto layout = TSA::Annotation::layoutFor(model, model.dimensions().items.at(id), model.dimensions().style);
        std::string text;
        for (const auto& t : layout.texts)
            if (t.text != "0") text += (text.empty() ? "" : ", ") + t.text;
        r.success = true;
        r.message = QCoreApplication::translate("TSA::Interaction::DimensionTools", "cotation #%1 créée (%2)")
                        .arg(id).arg(QString::fromStdString(text)).toStdString();
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
    registry.registerTool(factory(std::string("dim_aligned"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation alignée")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Distance réelle entre deux points, même inclinée (vraie grandeur).")),
                                  DimensionKind::Linear, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_linear"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation linéaire")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Distance projetée sur l'axe X, Y ou Z choisi selon la position du curseur "
                                                                "(comme une cotation horizontale / verticale).")),
                                  DimensionKind::Linear, MeasureAxis::X, true));
    registry.registerTool(factory(std::string("dim_horizontal"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation horizontale")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Distance en plan (projection sur le plan horizontal).")),
                                  DimensionKind::Linear, MeasureAxis::Horizontal, false));
    registry.registerTool(factory(std::string("dim_x"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation suivant X")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Écart des coordonnées X entre deux points.")),
                                  DimensionKind::Linear, MeasureAxis::X, false));
    registry.registerTool(factory(std::string("dim_y"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation suivant Y")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Écart des coordonnées Y entre deux points.")),
                                  DimensionKind::Linear, MeasureAxis::Y, false));
    registry.registerTool(factory(std::string("dim_z"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation verticale (Z)")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Différence de niveau (écart des coordonnées Z) entre deux points.")),
                                  DimensionKind::Linear, MeasureAxis::Z, false));
    registry.registerTool(factory(std::string("dim_angular"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation angulaire")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Angle au sommet entre deux directions (barres, lignes) : sommet, puis un point "
                                                                "de chaque bras.")),
                                  DimensionKind::Angular, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_level"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation de niveau")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Altitude d'un point par rapport à la référence de niveau du style.")),
                                  DimensionKind::Level, MeasureAxis::Z, false));
    registry.registerTool(factory(std::string("dim_chain"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation en chaîne")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Distances successives entre plusieurs points, sur une même ligne de cote "
                                                                "(direction premier → dernier point).")),
                                  DimensionKind::Chain, MeasureAxis::Aligned, false));
    registry.registerTool(factory(std::string("dim_cumulative"), std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools", "Cotation cumulée")),
                                  std::string(QT_TRANSLATE_NOOP("TSA::Interaction::DimensionTools",
                                                                "Distances mesurées depuis le premier point (origine commune).")),
                                  DimensionKind::Cumulative, MeasureAxis::Aligned, false));
}

} // namespace TSA::Interaction
