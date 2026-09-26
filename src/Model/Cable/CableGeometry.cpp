#include "CableGeometry.h"
#include <cmath>
#include <algorithm>

namespace TSA::Model
{

CableGeometry::CableGeometry()
    : m_mode(CableGeometryMode::Straight)
    , m_startPoint(0.0, 0.0, 0.0)
    , m_endPoint(10.0, 0.0, 0.0)
{
}

CableGeometry::CableGeometry(const gp_Pnt& pStart, const gp_Pnt& pEnd, CableGeometryMode mode)
    : m_mode(mode)
    , m_startPoint(pStart)
    , m_endPoint(pEnd)
{
}

double CableGeometry::chordLength() const
{
    return m_startPoint.Distance(m_endPoint);
}

double CableGeometry::horizontalSpan() const
{
    double dx = m_endPoint.X() - m_startPoint.X();
    double dy = m_endPoint.Y() - m_startPoint.Y();
    return std::sqrt(dx * dx + dy * dy);
}

double CableGeometry::verticalElevationDifference() const
{
    return std::abs(m_endPoint.Z() - m_startPoint.Z());
}

double CableGeometry::calculateCatenarySag(double span, double H, double w)
{
    if (H <= 1e-4 || span <= 1e-4 || w <= 1e-6)
    {
        return 0.0;
    }
    double a = H / w;
    // Formule exacte : f = a * (cosh(L / (2 * a)) - 1)
    double arg = span / (2.0 * a);
    if (arg < 1e-3)
    {
        // Développement limité d'ordre 2 pour éviter l'annulation numérique : f approx w*L^2 / (8*H)
        return (w * span * span) / (8.0 * H);
    }
    return a * (std::cosh(arg) - 1.0);
}

double CableGeometry::calculateCatenaryTensionFromSag(double span, double sag, double w)
{
    if (sag <= 1e-4 || span <= 1e-4 || w <= 1e-6)
    {
        return 1e5;
    }
    // Approximation précise d'Ernst / parabole pour le dimensionnement initial
    return (w * span * span) / (8.0 * sag);
}

double CableGeometry::calculateCatenaryLength(double span, double sag)
{
    if (span <= 1e-4) return 0.0;
    if (sag <= 1e-4) return span;
    // Longueur développée approchée d'une chaînette / parabole à faible flèche :
    // S = L * [1 + 8/3 * (f/L)^2 - 32/5 * (f/L)^4]
    double r = sag / span;
    return span * (1.0 + (8.0 / 3.0) * r * r - (32.0 / 5.0) * r * r * r * r);
}

double CableGeometry::curveLength(int numSamples) const
{
    if (m_mode == CableGeometryMode::Straight)
    {
        return chordLength();
    }

    auto pts = samplePoints(numSamples);
    if (pts.size() < 2)
    {
        return chordLength();
    }

    double total = 0.0;
    for (size_t i = 1; i < pts.size(); ++i)
    {
        total += pts[i - 1].Distance(pts[i]);
    }
    return total;
}

gp_Pnt CableGeometry::evaluatePoint(double t) const
{
    t = std::clamp(t, 0.0, 1.0);

    // Cas ThroughPoints / Polyline avec points de passage intermédiaires
    if ((m_mode == CableGeometryMode::ThroughPoints || m_mode == CableGeometryMode::Polyline) && !m_intermediatePoints.empty())
    {
        std::vector<gp_Pnt> allPts;
        allPts.reserve(m_intermediatePoints.size() + 2);
        allPts.push_back(m_startPoint);
        for (const auto& pt : m_intermediatePoints) allPts.push_back(pt);
        allPts.push_back(m_endPoint);

        size_t nSegments = allPts.size() - 1;
        double segmentT = t * static_cast<double>(nSegments);
        size_t segIdx = static_cast<size_t>(segmentT);
        if (segIdx >= nSegments) segIdx = nSegments - 1;
        double localT = segmentT - static_cast<double>(segIdx);

        const auto& pA = allPts[segIdx];
        const auto& pB = allPts[segIdx + 1];
        return gp_Pnt(
            pA.X() + localT * (pB.X() - pA.X()),
            pA.Y() + localT * (pB.Y() - pA.Y()),
            pA.Z() + localT * (pB.Z() - pA.Z())
        );
    }

    // Point de base sur la corde rectiligne
    gp_Pnt pChord(
        m_startPoint.X() + t * (m_endPoint.X() - m_startPoint.X()),
        m_startPoint.Y() + t * (m_endPoint.Y() - m_startPoint.Y()),
        m_startPoint.Z() + t * (m_endPoint.Z() - m_startPoint.Z())
    );

    if (m_mode == CableGeometryMode::Straight)
    {
        return pChord;
    }

    if (m_mode == CableGeometryMode::Parabolic)
    {
        // Déviation parabolique verticale sous charge uniforme : delta_z = -4 * f * t * (1 - t)
        double sagVal = m_sag;
        double deltaZ = -4.0 * sagVal * t * (1.0 - t);
        return gp_Pnt(pChord.X(), pChord.Y(), pChord.Z() + deltaZ);
    }

    if (m_mode == CableGeometryMode::Catenary)
    {
        // Chaînette exacte : y(xi) = a * [cosh(xi / a) - cosh(L / (2a))]
        double Lh = horizontalSpan();
        if (Lh < 1e-4)
        {
            return pChord;
        }

        double a = (m_horizontalTensionH > 0.0 && m_linearWeightW > 0.0) 
                   ? (m_horizontalTensionH / m_linearWeightW) 
                   : (Lh * Lh / (8.0 * std::max(0.01, m_sag)));

        double xi = (t - 0.5) * Lh;
        double halfL = 0.5 * Lh;
        double deltaZ = a * (std::cosh(xi / a) - std::cosh(halfL / a));
        return gp_Pnt(pChord.X(), pChord.Y(), pChord.Z() + deltaZ);
    }

    // Mode Spline par défaut
    double deltaZ = -4.0 * m_sag * t * (1.0 - t);
    return gp_Pnt(pChord.X(), pChord.Y(), pChord.Z() + deltaZ);
}

gp_Vec CableGeometry::evaluateTangent(double t) const
{
    double dt = 1e-4;
    double tA = std::max(0.0, t - dt);
    double tB = std::min(1.0, t + dt);
    gp_Pnt pA = evaluatePoint(tA);
    gp_Pnt pB = evaluatePoint(tB);
    gp_Vec v(pA, pB);
    if (v.Magnitude() > 1e-9)
    {
        v.Normalize();
        return v;
    }
    return gp_Vec(0.0, 0.0, 1.0);
}

std::vector<gp_Pnt> CableGeometry::samplePoints(int numSamples) const
{
    numSamples = std::clamp(numSamples, 2, 200);

    if (m_mode == CableGeometryMode::Straight)
    {
        return { m_startPoint, m_endPoint };
    }

    if ((m_mode == CableGeometryMode::ThroughPoints || m_mode == CableGeometryMode::Polyline) && !m_intermediatePoints.empty())
    {
        std::vector<gp_Pnt> pts;
        pts.reserve(m_intermediatePoints.size() + 2);
        pts.push_back(m_startPoint);
        for (const auto& pt : m_intermediatePoints)
        {
            pts.push_back(pt);
        }
        pts.push_back(m_endPoint);
        return pts;
    }

    std::vector<gp_Pnt> pts;
    pts.reserve(numSamples);
    int nDiv = std::max(1, numSamples - 1);
    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / static_cast<double>(nDiv);
        pts.push_back(evaluatePoint(t));
    }
    return pts;
}

} // namespace TSA::Model
