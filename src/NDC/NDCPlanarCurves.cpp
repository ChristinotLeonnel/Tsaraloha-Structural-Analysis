#include "NDCPlanarCurves.h"

#include "../Analysis/ResultsModel.h"

#include <QBuffer>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <functional>

namespace TSA::NDC
{

using TSA::Analysis::PlanarCurvePoint;
using TSA::Analysis::PlanarMemberCurves;
using TSA::Analysis::StructuralElementKind;

namespace
{
QString num(double v, int d = 2) { return QString::number(std::abs(v) < 0.5 * std::pow(10.0, -d) ? 0.0 : v, 'f', d); }

QString kindName(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Beam: return QStringLiteral("Poutre");
    case StructuralElementKind::Column: return QStringLiteral("Poteau");
    case StructuralElementKind::Truss: return QStringLiteral("Treillis");
    case StructuralElementKind::Cable: return QStringLiteral("Câble");
    }
    return QStringLiteral("Barre");
}

QString toDataUri(const QImage& img)
{
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(ba.toBase64());
}

/// L/f (flèche relative) ; « — » si flèche négligeable.
QString spanRatio(double L, double f)
{
    if (std::abs(f) < 1e-9 * std::max(L, 1.0)) return QStringLiteral("—");
    return QStringLiteral("L/%1").arg(static_cast<long long>(std::llround(L / std::abs(f))));
}

/// Un diagramme : valeurs le long de x, remplissage, valeurs aux extrémités et à l'extremum.
void drawPanel(QPainter& p, const QRectF& box, const QString& title, const QString& unit,
               const std::vector<PlanarCurvePoint>& pts, double L,
               const std::function<double(const PlanarCurvePoint&)>& value, bool positiveDown,
               const QColor& color, double scaleForLabel = 1.0)
{
    p.setPen(QColor("#334155"));
    QFont f = p.font();
    f.setPointSizeF(9);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(box.left(), box.top(), box.width(), 16), Qt::AlignLeft, QStringLiteral("%1 (%2)").arg(title, unit));
    f.setBold(false);
    p.setFont(f);

    const QRectF plot(box.left() + 60, box.top() + 22, box.width() - 120, box.height() - 34);
    double vmax = 0.0;
    for (const auto& pt : pts) vmax = std::max(vmax, std::abs(value(pt)));
    const double s = vmax > 0 ? (plot.height() / 2 - 4) / vmax : 0.0;
    const double y0 = plot.center().y();
    auto X = [&](double x) { return plot.left() + plot.width() * x / std::max(L, 1e-12); };
    auto Y = [&](double v) { return y0 + (positiveDown ? 1.0 : -1.0) * v * s; };

    QPainterPath area;
    area.moveTo(X(0), y0);
    for (const auto& pt : pts) area.lineTo(X(pt.x), Y(value(pt)));
    area.lineTo(X(L), y0);
    area.closeSubpath();
    QColor fill = color;
    fill.setAlpha(60);
    p.fillPath(area, fill);
    QPainterPath line;
    for (std::size_t k = 0; k < pts.size(); ++k)
        k == 0 ? line.moveTo(X(pts[k].x), Y(value(pts[k]))) : line.lineTo(X(pts[k].x), Y(value(pts[k])));
    p.setPen(QPen(color, 1.8));
    p.drawPath(line);
    p.setPen(QPen(QColor("#0f172a"), 1.4));
    p.drawLine(QPointF(X(0), y0), QPointF(X(L), y0));   // barre

    // Valeurs : extrémités et extremum absolu
    if (pts.empty()) return;
    auto label = [&](const PlanarCurvePoint& pt, Qt::Alignment align) {
        const double v = value(pt) * scaleForLabel;
        const QPointF at(X(pt.x), Y(value(pt)));
        p.setPen(QColor("#0f172a"));
        double ty = at.y() + ((Y(value(pt)) >= y0) ? 2 : -16);
        ty = std::clamp(ty, box.top() + 18, box.bottom() - 14);   // jamais sur le titre ni hors du panneau
        const double tx = std::clamp(at.x() - 55, box.left(), box.right() - 110);
        const QRectF r(tx, ty, 110, 14);
        p.drawText(r, align, num(v, 2));
    };
    label(pts.front(), Qt::AlignLeft);
    label(pts.back(), Qt::AlignRight);
    const auto ext = std::max_element(pts.begin(), pts.end(), [&](const auto& a, const auto& b) {
        return std::abs(value(a)) < std::abs(value(b));
    });
    if (ext != pts.begin() && ext != std::prev(pts.end())) label(*ext, Qt::AlignHCenter);
}
} // namespace

QImage renderPlanarMemberCurves(const PlanarMemberCurves& c, const QString& title)
{
    const int w = 900, panelH = 150, headerH = 30;
    QImage img(w, headerH + 4 * panelH, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = p.font();
    f.setPointSizeF(10.5);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor("#1e293b"));
    p.drawText(QRectF(10, 6, w - 20, 20), Qt::AlignLeft,
               QStringLiteral("%1 — L = %2 m").arg(title, num(c.length, 3)));

    const double L = c.length;
    // Déformée : écart à la corde, en mm, tracé à l'échelle de la fenêtre.
    const double vi = c.points.empty() ? 0.0 : c.points.front().v;
    const double vj = c.points.empty() ? 0.0 : c.points.back().v;
    auto fleche = [&](const PlanarCurvePoint& pt) { return (pt.v - (vi + (vj - vi) * pt.x / std::max(L, 1e-12))) * 1000.0; };

    drawPanel(p, QRectF(10, headerH + 0 * panelH, w - 20, panelH), QStringLiteral("Effort normal N(x)"), QStringLiteral("kN"),
              c.points, L, [](const PlanarCurvePoint& pt) { return pt.N; }, false, QColor("#7c3aed"));
    drawPanel(p, QRectF(10, headerH + 1 * panelH, w - 20, panelH), QStringLiteral("Effort tranchant V(x) = dM/dx"), QStringLiteral("kN"),
              c.points, L, [](const PlanarCurvePoint& pt) { return pt.V; }, false, QColor("#059669"));
    drawPanel(p, QRectF(10, headerH + 2 * panelH, w - 20, panelH),
              QStringLiteral("Moment fléchissant M(x) — tracé du côté de la fibre tendue"), QStringLiteral("kN·m"),
              c.points, L, [](const PlanarCurvePoint& pt) { return pt.M; }, true, QColor("#2563eb"));
    drawPanel(p, QRectF(10, headerH + 3 * panelH, w - 20, panelH),
              QStringLiteral("Déformée v(x) − corde (EI v'' = M) — échelle amplifiée"), QStringLiteral("mm"),
              c.points, L, fleche, false, QColor("#dc2626"));
    return img;
}

void appendPlanarCurvesChapter(NDCDocument& doc, const TSA::Analysis::ResultsModel& results,
                               int& chapterNumber, int& tableNumber, int& figureNumber)
{
    const auto& all = results.planarCurves();
    if (all.empty()) return;
    const auto& meta = results.executionMetadata();

    NDCChapter ch;
    ch.number = chapterNumber++;
    ch.title = QStringLiteral("Courbes RDM par Barre — Calcul Plan");

    NDCSection method;
    method.title = QStringLiteral("Méthode, Formules et Conventions");
    method.keyValues.push_back({ QStringLiteral("Moteur"), QString::fromStdString(meta.solverEngine + (meta.solverVersion.empty() ? "" : " v" + meta.solverVersion)) });
    if (!meta.calculationMethod.empty())
        method.keyValues.push_back({ QStringLiteral("Méthode"), QString::fromStdString(meta.calculationMethod) });
    method.keyValues.push_back({ QStringLiteral("Portée calculée"), QString::fromStdString(meta.analysisScope.empty() ? "Modèle complet" : meta.analysisScope) });
    if (!results.caseOrComboName().empty())
        method.keyValues.push_back({ QStringLiteral("Chargement"), QString::fromStdString(results.caseOrComboName()) });
    if (meta.maxResidualForce > 0.0 || meta.isEquilibriumVerified)
        method.keyValues.push_back({ QStringLiteral("Résidu d'équilibre global"), QStringLiteral("%1 kN").arg(meta.maxResidualForce, 0, 'g', 3) });
    method.paragraphs.push_back(QStringLiteral(
        "Inconnues : déplacements des nœuds (u, v, θ) dans le plan de la portée. Raideur de flexion d'une barre "
        "K = 4EI/L, report K/2 = 2EI/L à l'extrémité opposée (3EI/L côté encastré si l'autre extrémité est une rotule), "
        "moments d'encastrement parfait (qL²/12 sous charge uniforme), raideur axiale EA/L."));
    method.paragraphs.push_back(QStringLiteral(
        "Le long de chaque barre (x depuis le nœud i, coupure en x) : <b>M(x) = μ(x) + M<sub>i</sub>·(1 − x/L) + M<sub>j</sub>·x/L</b>, "
        "où μ(x) est le moment isostatique de la barre sur deux appuis sous ses charges ; <b>V(x) = dM/dx</b> ; "
        "<b>N(x)</b> > 0 en traction. M > 0 tend la fibre inférieure (y' < 0) d'une barre parcourue de i vers j ; "
        "les diagrammes de M sont tracés du côté de la fibre tendue."));
    method.paragraphs.push_back(QStringLiteral(
        "Déformée selon la RDM (Navier-Bernoulli) : intégration de la ligne élastique <b>EI·v''(x) = M(x)</b> avec "
        "v(0) = v<sub>i</sub> et v(L) = v<sub>j</sub> (déplacements calculés des nœuds), et <b>EA·u'(x) = N(x)</b>. "
        "La flèche est l'écart maximal de la déformée à la corde ; elle est comparée, à titre indicatif, aux limites "
        "usuelles L/250 (aspect, EN 1990 / EN 1992-1-1 §7.4.1) et L/500 (après mise en œuvre des éléments fragiles)."));
    ch.sections.push_back(method);

    NDCSection summary;
    summary.title = QStringLiteral("Valeurs Caractéristiques des Courbes");
    NDCTable t;
    t.number = tableNumber++;
    t.caption = QStringLiteral("Lecture des courbes par barre (kN, kN·m, m, mm)");
    t.headers = { QStringLiteral("Barre"), QStringLiteral("L"), QStringLiteral("M_i"), QStringLiteral("M_j"),
                  QStringLiteral("M travée (x)"), QStringLiteral("V_i"), QStringLiteral("V_j"), QStringLiteral("N min / max"),
                  QStringLiteral("Flèche (x)"), QStringLiteral("L/f"), QStringLiteral("L/250") };
    t.columnAlignments = { "left", "right", "right", "right", "right", "right", "right", "right", "right", "center", "center" };
    for (const auto& [key, c] : all)
    {
        const QString span = c.hasSpanExtremum ? QStringLiteral("%1 (%2)").arg(num(c.MSpanExtremum), num(c.xSpanExtremum, 3)) : QStringLiteral("—");
        const double fmm = c.deflectionMax * 1000.0;
        const bool flexural = key.kind == StructuralElementKind::Beam || key.kind == StructuralElementKind::Column;
        const QString check = !flexural || std::abs(c.deflectionMax) < 1e-12 ? QStringLiteral("—")
                            : (std::abs(c.deflectionMax) <= c.length / 250.0 ? QStringLiteral("✓") : QStringLiteral("✗"));
        t.rows.push_back({ QString::fromStdString(key.label()), num(c.length, 3), num(c.Mi), num(c.Mj), span, num(c.Vi), num(c.Vj),
                           QStringLiteral("%1 / %2").arg(num(c.Nmin), num(c.Nmax)),
                           QStringLiteral("%1 (%2)").arg(num(fmm, 3), num(c.xDeflectionMax, 3)), spanRatio(c.length, c.deflectionMax), check });
    }
    summary.tables.push_back(t);
    ch.sections.push_back(summary);

    for (const auto& [key, c] : all)
    {
        NDCSection s;
        const QString name = QStringLiteral("%1 %2").arg(kindName(key.kind), QString::fromStdString(key.label()));
        s.title = name;
        s.keyValues.push_back({ QStringLiteral("Longueur"), QStringLiteral("%1 m").arg(num(c.length, 3)) });
        s.keyValues.push_back({ QStringLiteral("Moments d'extrémité M_i / M_j"), QStringLiteral("%1 / %2 kN·m").arg(num(c.Mi), num(c.Mj)) });
        if (c.hasSpanExtremum)
            s.keyValues.push_back({ QStringLiteral("Moment extrême en travée (V = 0)"),
                                    QStringLiteral("%1 kN·m en x = %2 m").arg(num(c.MSpanExtremum), num(c.xSpanExtremum, 3)) });
        s.keyValues.push_back({ QStringLiteral("M max / M min"),
                                QStringLiteral("%1 kN·m (x = %2 m) / %3 kN·m (x = %4 m)").arg(num(c.Mmax), num(c.xMmax, 3), num(c.Mmin), num(c.xMmin, 3)) });
        if (!c.momentZeros.empty())
        {
            QStringList z;
            for (double x : c.momentZeros) z << num(x, 3);
            s.keyValues.push_back({ QStringLiteral("Moment nul en x ="), z.join(QStringLiteral(" ; ")) + QStringLiteral(" m") });
        }
        s.keyValues.push_back({ QStringLiteral("Efforts tranchants V_i / V_j"), QStringLiteral("%1 / %2 kN").arg(num(c.Vi), num(c.Vj)) });
        s.keyValues.push_back({ QStringLiteral("Effort normal min / max"), QStringLiteral("%1 / %2 kN").arg(num(c.Nmin), num(c.Nmax)) });
        s.keyValues.push_back({ QStringLiteral("Flèche (écart à la corde)"),
                                QStringLiteral("%1 mm en x = %2 m (%3)").arg(num(c.deflectionMax * 1000.0, 3), num(c.xDeflectionMax, 3), spanRatio(c.length, c.deflectionMax)) });
        s.keyValues.push_back({ QStringLiteral("Rotations des sections θ_i / θ_j"),
                                QStringLiteral("%1 / %2 mrad").arg(num(c.rotationI * 1000.0, 3), num(c.rotationJ * 1000.0, 3)) });

        NDCFigure fig;
        fig.number = figureNumber++;
        fig.caption = QStringLiteral("Courbes N, V, M et déformée — %1").arg(name);
        fig.elementRef = name;
        fig.loadCaseOrCombo = QString::fromStdString(results.caseOrComboName());
        fig.imageBase64 = toDataUri(renderPlanarMemberCurves(c, name));
        fig.widthPercent = 95;
        s.figures.push_back(fig);
        ch.sections.push_back(s);
    }
    doc.addChapter(ch);
}

} // namespace TSA::NDC
