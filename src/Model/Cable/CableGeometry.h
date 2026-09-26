#pragma once

#include "CableTypes.h"
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <vector>

namespace TSA::Model
{

/**
 * @brief Calculateur et gestionnaire géométrique pour les tracés de câbles (droit, parabolique, caténaire, multi-points).
 */
class CableGeometry
{
public:
    CableGeometry();
    CableGeometry(const gp_Pnt& pStart, const gp_Pnt& pEnd, CableGeometryMode mode = CableGeometryMode::Straight);

    CableGeometryMode mode() const { return m_mode; }
    void setMode(CableGeometryMode mode) { m_mode = mode; }

    const gp_Pnt& startPoint() const { return m_startPoint; }
    void setStartPoint(const gp_Pnt& p) { m_startPoint = p; }

    const gp_Pnt& endPoint() const { return m_endPoint; }
    void setEndPoint(const gp_Pnt& p) { m_endPoint = p; }

    // Flèche géométrique au milieu de portée (sag) en mètres
    double sag() const { return m_sag; }
    void setSag(double s) { m_sag = std::max(0.0, s); }

    // Paramètres caténaire mécanique
    double horizontalTensionH() const { return m_horizontalTensionH; }
    void setHorizontalTensionH(double H) { m_horizontalTensionH = std::max(1.0, H); }
    void setCatenaryHorizontalTension(double H) { setHorizontalTensionH(H); }

    double linearWeightW() const { return m_linearWeightW; }
    void setLinearWeightW(double w) { m_linearWeightW = std::max(1e-4, w); }
    void setCatenaryLinearWeight(double w) { setLinearWeightW(w); }

    double catenaryParameter() const { return horizontalTensionH() / (linearWeightW() > 1e-9 ? linearWeightW() : 1.0); }

    // Points de passage intermédiaires (ThroughPoints / Polyline / Spline)
    const std::vector<gp_Pnt>& intermediatePoints() const { return m_intermediatePoints; }
    void setIntermediatePoints(const std::vector<gp_Pnt>& pts) { m_intermediatePoints = pts; }
    void addIntermediatePoint(const gp_Pnt& p) { m_intermediatePoints.push_back(p); }
    void clearIntermediatePoints() { m_intermediatePoints.clear(); }

    // Grandeurs géométriques calculées
    double chordLength() const;
    double horizontalSpan() const;
    double verticalElevationDifference() const;
    double curveLength(int numSamples = 40) const;

    // Discrétisation spatiale 3D pour le rendu B-Rep OCCT et le maillage
    std::vector<gp_Pnt> samplePoints(int numSamples = 30) const;

    // Évaluation ponctuelle pour un paramètre sans dimension t dans [0, 1]
    gp_Pnt evaluatePoint(double t) const;
    gp_Vec evaluateTangent(double t) const;

    // Relations physiques caténaire / parabole
    static double calculateCatenarySag(double span, double H, double w);
    static double calculateCatenaryTensionFromSag(double span, double sag, double w);
    static double calculateCatenaryLength(double span, double sag);

private:
    CableGeometryMode m_mode = CableGeometryMode::Straight;
    gp_Pnt m_startPoint{0.0, 0.0, 0.0};
    gp_Pnt m_endPoint{10.0, 0.0, 0.0};

    double m_sag = 0.50;                // Flèche au milieu (m)
    double m_horizontalTensionH = 1e5;  // Effort horizontal H (N)
    double m_linearWeightW = 20.0;      // Poids propre w (N/m)

    std::vector<gp_Pnt> m_intermediatePoints;
};

} // namespace TSA::Model
