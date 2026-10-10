#include "DimensionGeometry.h"

#include "../Model/Model.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Annotation
{

namespace
{
constexpr double kTiny = 1e-9;
constexpr double kPi = 3.14159265358979323846;

gp_Pnt toPnt(const std::array<double, 3>& a)
{
    return gp_Pnt(a[0], a[1], a[2]);
}

/// Direction perpendiculaire quelconque (stable) à d.
gp_Dir anyPerpendicular(const gp_Dir& d)
{
    const gp_Vec ref = std::abs(d.Z()) < 0.9 ? gp_Vec(0, 0, 1) : gp_Vec(1, 0, 0);
    gp_Vec p = gp_Vec(d).Crossed(ref);
    return gp_Dir(p);
}

std::string fixed(double v, int decimals)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(std::max(0, decimals)) << v;
    std::string s = o.str();
    // « -0.000 » → « 0.000 »
    if (!s.empty() && s[0] == '-' && s.find_first_not_of("-0.") == std::string::npos) s.erase(0, 1);
    return s;
}

double rounded(const DimensionStyle& st, double displayValue)
{
    if (st.rounding > 0.0) return std::round(displayValue / st.rounding) * st.rounding;
    return displayValue;
}

/// Disposition des cotations linéaires, en chaîne et cumulées.
void linearLayout(const Dimension& dim, const std::vector<gp_Pnt>& P, const DimensionStyle& st, DimensionLayout& out)
{
    const gp_Pnt& first = P.front();
    const gp_Pnt& last = P.back();
    gp_Vec dv;
    switch (dim.axis)
    {
    case MeasureAxis::X: dv = gp_Vec(1, 0, 0); break;
    case MeasureAxis::Y: dv = gp_Vec(0, 1, 0); break;
    case MeasureAxis::Z: dv = gp_Vec(0, 0, 1); break;
    case MeasureAxis::Horizontal: dv = gp_Vec(last.X() - first.X(), last.Y() - first.Y(), 0); break;
    case MeasureAxis::Aligned: dv = gp_Vec(first, last); break;
    }
    if (dv.Magnitude() < kTiny)
    {
        out.error = dim.axis == MeasureAxis::Horizontal ? "distance horizontale nulle entre le premier et le dernier point"
                                                        : "premier et dernier points confondus";
        return;
    }
    const gp_Dir d(dv);
    const gp_Pnt C = toPnt(dim.position);
    auto project = [&](const gp_Pnt& p) { return C.Translated(gp_Vec(d) * gp_Vec(C, p).Dot(gp_Vec(d))); };

    std::vector<gp_Pnt> Q;
    for (const auto& p : P) Q.push_back(project(p));

    // Côté de la ligne de cote (pour l'orientation du texte dans le plan).
    gp_Vec offset(P.front(), Q.front());
    gp_Dir up = offset.Magnitude() > kTiny ? gp_Dir(offset) : anyPerpendicular(d);

    // Lignes d'attache : du point mesuré (écart) jusqu'au-delà de la ligne de cote (dépassement).
    for (size_t i = 0; i < P.size(); ++i)
    {
        gp_Vec e(P[i], Q[i]);
        const double len = e.Magnitude();
        if (len <= st.extensionGap + kTiny) continue;
        e.Normalize();
        out.segments.emplace_back(P[i].Translated(e * st.extensionGap), Q[i].Translated(e * st.extensionOvershoot));
    }
    out.segments.emplace_back(Q.front(), Q.back());

    auto arrowPair = [&](const gp_Pnt& a, const gp_Pnt& b) {
        if (a.Distance(b) < kTiny) return;
        out.arrows.push_back({ a, gp_Dir(gp_Vec(b, a)) });
        out.arrows.push_back({ b, gp_Dir(gp_Vec(a, b)) });
    };

    if (dim.kind == DimensionKind::Cumulative)
    {
        out.texts.push_back({ Q.front(), "0", d, up });
        for (size_t i = 1; i < P.size(); ++i)
        {
            const double v = std::abs(gp_Vec(P.front(), P[i]).Dot(gp_Vec(d)));
            out.values.push_back(v);
            if (Q.front().Distance(Q[i]) > kTiny) out.arrows.push_back({ Q[i], gp_Dir(gp_Vec(Q.front(), Q[i])) });
            out.texts.push_back({ Q[i], applyTextOverride(dim.textOverride, formatLength(st, v)), d, up });
        }
    }
    else
    {
        for (size_t i = 0; i + 1 < P.size(); ++i)
        {
            const double v = std::abs(gp_Vec(P[i], P[i + 1]).Dot(gp_Vec(d)));
            out.values.push_back(v);
            arrowPair(Q[i], Q[i + 1]);
            const gp_Pnt mid((Q[i].XYZ() + Q[i + 1].XYZ()) * 0.5);
            out.texts.push_back({ mid, applyTextOverride(dim.textOverride, formatLength(st, v)), d, up });
        }
    }
    out.valid = true;
}

void angularLayout(const Dimension& dim, const std::vector<gp_Pnt>& P, const DimensionStyle& st, DimensionLayout& out)
{
    const gp_Pnt& V = P[0];
    const gp_Vec va(V, P[1]), vb(V, P[2]);
    if (va.Magnitude() < kTiny || vb.Magnitude() < kTiny)
    {
        out.error = "un bras de l'angle est de longueur nulle (point confondu avec le sommet)";
        return;
    }
    const gp_Dir ua(va), ub(vb);
    const double cosA = std::clamp(gp_Vec(ua).Dot(gp_Vec(ub)), -1.0, 1.0);
    const double angle = std::acos(cosA);
    gp_Vec n = gp_Vec(ua).Crossed(gp_Vec(ub));
    if (n.Magnitude() < 1e-9)
    {
        if (angle < 1e-6)
        {
            out.error = "les deux bras sont confondus (angle nul)";
            return;
        }
        n = gp_Vec(anyPerpendicular(ua)); // angle plat : plan quelconque contenant le bras
    }
    const gp_Dir nd(n);
    const gp_Dir w(gp_Vec(nd).Crossed(gp_Vec(ua)));
    double r = V.Distance(toPnt(dim.position));
    if (r < kTiny) r = 0.5 * std::min(va.Magnitude(), vb.Magnitude());

    auto arcPoint = [&](double t) { return V.Translated(gp_Vec(ua) * (r * std::cos(t)) + gp_Vec(w) * (r * std::sin(t))); };
    const int n_seg = std::max(8, static_cast<int>(std::ceil(angle / (2 * kPi) * 72)));
    gp_Pnt prev = arcPoint(0);
    for (int i = 1; i <= n_seg; ++i)
    {
        const gp_Pnt p = arcPoint(angle * i / n_seg);
        out.segments.emplace_back(prev, p);
        prev = p;
    }
    // Bras prolongés jusqu'à l'arc s'il est plus loin que les points.
    auto armExtension = [&](const gp_Dir& u, double armLen) {
        if (r > armLen + kTiny)
            out.segments.emplace_back(V.Translated(gp_Vec(u) * (armLen + st.extensionGap)), V.Translated(gp_Vec(u) * (r + st.extensionOvershoot)));
    };
    armExtension(ua, va.Magnitude());
    armExtension(ub, vb.Magnitude());

    out.arrows.push_back({ arcPoint(0), gp_Dir(gp_Vec(w).Reversed()) });
    const gp_Vec tangentEnd = gp_Vec(ua) * (-std::sin(angle)) + gp_Vec(w) * std::cos(angle);
    out.arrows.push_back({ arcPoint(angle), gp_Dir(tangentEnd) });
    const double deg = angle * 180.0 / kPi;
    out.values.push_back(deg);
    const gp_Pnt mid = arcPoint(angle / 2);
    const gp_Dir radial(gp_Vec(V, mid));
    out.texts.push_back({ mid, applyTextOverride(dim.textOverride, formatAngle(st, deg)), gp_Dir(gp_Vec(radial).Crossed(gp_Vec(nd)).Reversed()), radial });
    out.valid = true;
}

void levelLayout(const Dimension& dim, const std::vector<gp_Pnt>& P, const DimensionStyle& st, DimensionLayout& out)
{
    const gp_Pnt& p = P[0];
    const gp_Pnt c = toPnt(dim.position);
    const double v = p.Z() - st.levelReference;
    out.values.push_back(v);
    out.levelMarkers.push_back(p);
    if (p.Distance(c) > kTiny) out.segments.emplace_back(p, c);
    out.texts.push_back({ c, applyTextOverride(dim.textOverride, formatLevel(st, v)), gp_Dir(1, 0, 0), gp_Dir(0, 0, 1) });
    out.valid = true;
}
} // namespace

std::vector<gp_Pnt> resolveAnchors(const TSA::Model::Model& model, const Dimension& dim, bool* invalidReference)
{
    std::vector<gp_Pnt> pts;
    bool invalid = false;
    for (const auto& a : dim.anchors)
    {
        const TSA::Model::Node* n = a.associative() ? model.getNode(a.nodeId) : nullptr;
        if (n) pts.emplace_back(n->x(), n->y(), n->z());
        else
        {
            pts.push_back(toPnt(a.point));
            if (a.nodeId > 0) invalid = true; // nœud associé disparu (ou marqué orphelin)
        }
    }
    if (invalidReference) *invalidReference = invalid;
    return pts;
}

DimensionLayout computeLayout(const Dimension& dim, const std::vector<gp_Pnt>& points, const DimensionStyle& style)
{
    DimensionLayout out;
    const int need = Dimension::requiredAnchors(dim.kind);
    const bool exact = dim.kind == DimensionKind::Linear || dim.kind == DimensionKind::Angular || dim.kind == DimensionKind::Level;
    if (static_cast<int>(points.size()) < need || (exact && static_cast<int>(points.size()) != need))
    {
        out.error = "nombre de points incorrect";
        return out;
    }
    switch (dim.kind)
    {
    case DimensionKind::Linear:
    case DimensionKind::Chain:
    case DimensionKind::Cumulative: linearLayout(dim, points, style, out); break;
    case DimensionKind::Angular: angularLayout(dim, points, style, out); break;
    case DimensionKind::Level: levelLayout(dim, points, style, out); break;
    }
    return out;
}

DimensionLayout layoutFor(const TSA::Model::Model& model, const Dimension& dim, const DimensionStyle& style)
{
    bool invalid = false;
    const auto pts = resolveAnchors(model, dim, &invalid);
    DimensionLayout out = computeLayout(dim, pts, style);
    out.invalidReference = invalid || dim.hasInvalidReference();
    return out;
}

std::string formatLength(const DimensionStyle& st, double meters)
{
    std::string s = fixed(rounded(st, meters * st.unitFactor()), st.decimals);
    if (st.showUnit) s += " " + st.unitSymbol();
    return s;
}

std::string formatAngle(const DimensionStyle& st, double degrees)
{
    return fixed(degrees, st.angleDecimals) + "°";
}

std::string formatLevel(const DimensionStyle& st, double meters)
{
    const double v = rounded(st, meters * st.unitFactor());
    std::string s = fixed(std::abs(v), st.decimals);
    const bool zero = s.find_first_not_of("0.") == std::string::npos;
    s = (zero ? "±" : (v > 0 ? "+" : "-")) + s;
    if (st.showUnit) s += " " + st.unitSymbol();
    return s;
}

std::string applyTextOverride(const std::string& override, const std::string& value)
{
    if (override.empty()) return value;
    std::string out = override;
    for (size_t pos = out.find("<>"); pos != std::string::npos; pos = out.find("<>", pos + value.size()))
        out.replace(pos, 2, value);
    return out;
}

MeasureAxis autoLinearAxis(const gp_Pnt& a, const gp_Pnt& b, const gp_Pnt& cursor)
{
    const gp_Pnt mid((a.XYZ() + b.XYZ()) * 0.5);
    const double off[3] = { std::abs(cursor.X() - mid.X()), std::abs(cursor.Y() - mid.Y()), std::abs(cursor.Z() - mid.Z()) };
    const double span[3] = { std::abs(b.X() - a.X()), std::abs(b.Y() - a.Y()), std::abs(b.Z() - a.Z()) };
    // Axe de décalage : celui où le curseur s'écarte le plus ; l'axe mesuré est, parmi les deux autres,
    // celui où les points sont le plus éloignés.
    int offAxis = 0;
    for (int k = 1; k < 3; ++k)
        if (off[k] > off[offAxis]) offAxis = k;
    int best = -1;
    for (int k = 0; k < 3; ++k)
        if (k != offAxis && (best < 0 || span[k] > span[best])) best = k;
    return best == 0 ? MeasureAxis::X : (best == 1 ? MeasureAxis::Y : MeasureAxis::Z);
}

} // namespace TSA::Annotation
