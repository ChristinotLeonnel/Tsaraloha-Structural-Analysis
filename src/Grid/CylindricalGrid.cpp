#include "CylindricalGrid.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

namespace TSA::Grid
{

constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
constexpr double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

CylindricalGrid::CylindricalGrid(const GridDefinition& definition)
    : m_definition(definition)
{
    computeGeometry();
}

void CylindricalGrid::updateDefinition(const GridDefinition& definition)
{
    m_definition = definition;
    computeGeometry();
}

void CylindricalGrid::setExtension(double ext)
{
    if (std::abs(m_extension - ext) > 1e-4)
    {
        m_extension = ext;
        computeGeometry();
    }
}

gp_Pnt CylindricalGrid::polarToWorld(double r, double angleDeg, double z) const
{
    double rad = angleDeg * DEG_TO_RAD;
    const gp_Pnt& orig = m_definition.origin();
    return gp_Pnt(
        orig.X() + r * std::cos(rad),
        orig.Y() + r * std::sin(rad),
        orig.Z() + z
    );
}

void CylindricalGrid::worldToPolar(const gp_Pnt& worldPt, double& r, double& angleDeg, double& z) const
{
    const gp_Pnt& orig = m_definition.origin();
    double dx = worldPt.X() - orig.X();
    double dy = worldPt.Y() - orig.Y();
    z = worldPt.Z() - orig.Z();

    r = std::sqrt(dx * dx + dy * dy);
    double rad = std::atan2(dy, dx);
    angleDeg = rad * RAD_TO_DEG;
    if (angleDeg < 0.0)
    {
        angleDeg += 360.0;
    }
}

void CylindricalGrid::computeGeometry()
{
    m_circles.clear();
    m_radialLines.clear();
    m_intersections.clear();
    m_labelAnchors.clear();

    const auto& radii = m_definition.radii();
    const auto& angles = m_definition.angles();
    const auto& zLevels = m_definition.zLevels();
    const gp_Pnt& orig = m_definition.origin();

    if (radii.empty() || angles.empty())
    {
        return;
    }

    m_maxRadius = radii.back();
    double outerR = m_maxRadius + m_extension;

    std::vector<double> levels = zLevels;
    if (levels.empty())
    {
        levels.push_back(0.0);
    }

    for (size_t k = 0; k < levels.size(); ++k)
    {
        double zVal = orig.Z() + levels[k];
        gp_Pnt centerPt(orig.X(), orig.Y(), zVal);

        // 1. Cercles concentriques
        for (size_t i = 0; i < radii.size(); ++i)
        {
            CylindricalCircle c;
            c.center = centerPt;
            c.radius = radii[i];
            c.zLevel = zVal;
            c.label = m_definition.getRadiusLabel(i);
            c.index = static_cast<int>(i);

            m_circles.push_back(c);

            // Ancrage libellé rayon (à 0° ou au début)
            gp_Pnt pos = polarToWorld(radii[i], 0.0, levels[k]);
            m_labelAnchors.push_back({ pos, c.label, false });
        }

        // 2. Lignes radiales
        for (size_t j = 0; j < angles.size(); ++j)
        {
            double ang = angles[j];
            CylindricalRadialLine line;
            line.start = centerPt;
            line.end = polarToWorld(outerR, ang, levels[k]);
            line.angleDeg = ang;
            line.zLevel = zVal;
            line.label = m_definition.getAngleLabel(j);
            line.index = static_cast<int>(j);

            m_radialLines.push_back(line);

            // Ancrage libellé d'angle en périphérie extérieure
            m_labelAnchors.push_back({ line.end, line.label, true });
        }

        // 3. Intersections (Cercles x Rayons)
        for (size_t i = 0; i < radii.size(); ++i)
        {
            for (size_t j = 0; j < angles.size(); ++j)
            {
                CylindricalIntersection inter;
                inter.point = polarToWorld(radii[i], angles[j], levels[k]);
                inter.radiusIndex = static_cast<int>(i);
                inter.angleIndex = static_cast<int>(j);
                inter.zIndex = static_cast<int>(k);
                inter.radius = radii[i];
                inter.angleDeg = angles[j];
                inter.labelRadius = m_definition.getRadiusLabel(i);
                inter.labelAngle = m_definition.getAngleLabel(j);
                inter.labelZ = m_definition.getZLabel(k);

                m_intersections.push_back(inter);
            }
        }
    }
}

GridSnapResult CylindricalGrid::findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const
{
    GridSnapResult bestResult;
    bestResult.snapped = false;
    bestResult.distance = std::numeric_limits<double>::max();

    const gp_Pnt& orig = m_definition.origin();

    // 1. Accrochage prioritaire : Centre / Origine
    double distCenter = worldPoint.Distance(orig);
    if (distCenter <= snapToleranceWorld && distCenter < bestResult.distance)
    {
        bestResult.snapped = true;
        bestResult.point = orig;
        bestResult.type = GridSnapType::Origin;
        bestResult.distance = distCenter;
        bestResult.description = "Centre Grille Polaire (" + std::to_string(orig.X()) + ", " +
                                 std::to_string(orig.Y()) + ", " + std::to_string(orig.Z()) + ")";
    }

    // 2. Accrochage : Intersections (Cercle x Rayon)
    for (const auto& inter : m_intersections)
    {
        double d = worldPoint.Distance(inter.point);
        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = inter.point;
            bestResult.type = GridSnapType::Intersection;
            bestResult.distance = d;

            std::ostringstream oss;
            oss << "Intersection Polaire " << inter.labelRadius << " / " << inter.labelAngle
                << " [" << inter.labelZ << "] ("
                << std::fixed << std::setprecision(3)
                << inter.point.X() << ", " << inter.point.Y() << ", " << inter.point.Z() << " m)";
            bestResult.description = oss.str();
        }
    }

    if (bestResult.snapped && bestResult.type == GridSnapType::Intersection)
    {
        return bestResult;
    }

    // 3. Accrochage : Rayons (lignes radiales)
    for (const auto& line : m_radialLines)
    {
        gp_Vec vLine(line.start, line.end);
        double lenSq = vLine.SquareMagnitude();
        if (lenSq < 1e-6) continue;

        gp_Vec vPt(line.start, worldPoint);
        double t = (vPt.Dot(vLine)) / lenSq;
        t = std::clamp(t, 0.0, 1.0);

        gp_Pnt projPoint = line.start.Translated(vLine * t);
        double d = worldPoint.Distance(projPoint);

        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = projPoint;
            bestResult.type = GridSnapType::RadialLine;
            bestResult.distance = d;

            std::ostringstream oss;
            oss << "Rayon Polaire " << line.label
                << " (" << std::fixed << std::setprecision(3)
                << projPoint.X() << ", " << projPoint.Y() << ", " << projPoint.Z() << " m)";
            bestResult.description = oss.str();
        }
    }

    // 4. Accrochage : Cercles concentriques
    for (const auto& circ : m_circles)
    {
        double dx = worldPoint.X() - circ.center.X();
        double dy = worldPoint.Y() - circ.center.Y();
        double curDist = std::sqrt(dx * dx + dy * dy);

        if (curDist > 1e-4)
        {
            double scale = circ.radius / curDist;
            gp_Pnt projPoint(circ.center.X() + dx * scale,
                             circ.center.Y() + dy * scale,
                             circ.zLevel);
            double d = worldPoint.Distance(projPoint);

            if (d <= snapToleranceWorld && d < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = projPoint;
                bestResult.type = GridSnapType::Circle;
                bestResult.distance = d;

                std::ostringstream oss;
                oss << "Cercle " << circ.label
                    << " (" << std::fixed << std::setprecision(3)
                    << projPoint.X() << ", " << projPoint.Y() << ", " << projPoint.Z() << " m)";
                bestResult.description = oss.str();
            }
        }
    }

    return bestResult;
}

} // namespace TSA::Grid
