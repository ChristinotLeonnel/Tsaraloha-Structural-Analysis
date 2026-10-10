#include "MemberLoadGlyph.h"

#include "../Analysis/LoadResolver.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>

namespace TSA::Geometry
{

int distributedStationCount(double span, const MemberLoadGlyphSettings& settings)
{
    const double spacing = settings.spacing > 1e-9 ? settings.spacing : 1.0;
    const int maxArrows = std::max(2, settings.maxArrows);
    if (!(span > 0.05 * spacing)) return 1;          // intervalle quasi nul : une flèche au milieu
    if (span < spacing) return 2;                     // court : une flèche à chaque extrémité
    const double wanted = std::round(span / spacing) + 1.0;
    const int n = static_cast<int>(std::min(wanted, static_cast<double>(maxArrows)));
    return std::clamp(n, std::max(2, settings.minArrows), maxArrows);
}

MemberLoadGlyph buildMemberLoadGlyph(const TSA::Model::MemberLoad& load,
                                     const gp_Pnt& p1, const gp_Pnt& p2, double betaAngleDeg,
                                     const MemberLoadGlyphSettings& settings)
{
    using TSA::Analysis::LoadResolver;
    using TSA::Model::LoadType;

    MemberLoadGlyph g;
    const gp_Vec axis(p1, p2);
    const double L = axis.Magnitude();
    if (!(L > 1e-9)) return g;
    const gp_Vec unitAxis = axis / L;
    const double arrowLength = settings.arrowLength > 0.0 ? settings.arrowLength : 0.6;

    const auto [a, b] = load.appliedRange(L);
    g.start = a;
    g.end = b;
    auto pointAt = [&](double s) { return p1.Translated(unitAxis * s); };
    auto vectorAt = [&](double s) {
        return LoadResolver::memberLoadVector(load, load.intensityAt(s, L), p1, p2, betaAngleDeg);
    };

    if (load.type() == LoadType::MemberMoment)
    {
        g.kind = MemberLoadGlyphKind::DistributedMoment;
        g.labelAnchor = pointAt(0.5 * (a + b));
        return g;
    }

    if (load.type() == LoadType::MemberPoint)
    {
        g.kind = MemberLoadGlyphKind::Point;
        const gp_Vec v = vectorAt(a);
        g.maxIntensity = std::abs(load.q1());
        g.labelAnchor = pointAt(a);
        if (v.Magnitude() > 1e-12)
        {
            LoadArrow arrow;
            arrow.position = a;
            arrow.intensity = load.q1();
            arrow.tip = pointAt(a);
            arrow.direction = gp_Dir(v);
            arrow.length = arrowLength;
            arrow.tail = arrow.tip.Translated(-gp_Vec(arrow.direction) * arrowLength);
            g.labelAnchor = arrow.tail;
            g.arrows.push_back(arrow);
        }
        return g;
    }

    g.kind = MemberLoadGlyphKind::Distributed;
    const int n = distributedStationCount(b - a, settings);
    std::vector<double> stations;
    stations.reserve(static_cast<std::size_t>(n) + 1);
    if (n == 1)
        stations.push_back(0.5 * (a + b));
    else
        for (int i = 0; i < n; ++i) stations.push_back(a + (b - a) * static_cast<double>(i) / (n - 1));

    // Enveloppe exacte quand l'intensité change de signe : point d'intensité nulle ajouté (pour une
    // charge en module seul, |q| y forme un angle que l'interpolation entre stations lisserait).
    if (load.type() == LoadType::MemberLinear && load.q1() * load.q2() < 0.0 && b - a > 1e-12)
    {
        const double s0 = a + (b - a) * load.q1() / (load.q1() - load.q2());
        stations.insert(std::upper_bound(stations.begin(), stations.end(), s0), s0);
    }

    std::vector<gp_Vec> vectors;
    vectors.reserve(stations.size());
    double vMax = 0.0;
    for (double s : stations)
    {
        vectors.push_back(vectorAt(s));
        vMax = std::max(vMax, vectors.back().Magnitude());
    }
    // q maximal sur l'intervalle (extrémités pour une charge linéaire, q1 sinon).
    g.maxIntensity = load.type() == LoadType::MemberLinear ? std::max(std::abs(load.q1()), std::abs(load.q2()))
                                                         : std::abs(load.q1());

    for (std::size_t i = 0; i < stations.size(); ++i)
    {
        const gp_Pnt tip = pointAt(stations[i]);
        const double magnitude = vectors[i].Magnitude();
        const double length = vMax > 1e-12 ? arrowLength * magnitude / vMax : 0.0;
        const gp_Pnt tail = magnitude > 1e-12 ? tip.Translated(-vectors[i] / magnitude * length) : tip;
        g.envelope.push_back(tail);
        if (magnitude <= 1e-9 * std::max(vMax, 1e-12)) continue;   // q = 0 : pas de flèche, enveloppe sur la barre

        LoadArrow arrow;
        arrow.position = stations[i];
        arrow.intensity = load.intensityAt(stations[i], L);
        arrow.tip = tip;
        arrow.tail = tail;
        arrow.direction = gp_Dir(vectors[i]);
        arrow.length = length;
        g.arrows.push_back(arrow);
    }

    // Étiquette : au-delà de l'enveloppe, au milieu de l'intervalle chargé.
    const gp_Pnt mid = pointAt(0.5 * (a + b));
    const gp_Vec vMid = vectorAt(0.5 * (a + b));
    const gp_Vec away = vMid.Magnitude() > 1e-12 ? -vMid.Normalized() : gp_Vec(0.0, 0.0, 1.0);
    g.labelAnchor = mid.Translated(away * (arrowLength * 1.9));
    return g;
}

} // namespace TSA::Geometry
