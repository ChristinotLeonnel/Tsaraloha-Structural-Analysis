#include "CylindricalCoordinates.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TSA::Coordinate
{

void CylindricalCoordinates::setRadii(const std::vector<double>& rList)
{
    m_radii = rList;
    std::sort(m_radii.begin(), m_radii.end());
    m_radii.erase(std::unique(m_radii.begin(), m_radii.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-6;
    }), m_radii.end());
}

void CylindricalCoordinates::setAnglesDeg(const std::vector<double>& aList)
{
    m_anglesDeg = aList;
    std::sort(m_anglesDeg.begin(), m_anglesDeg.end());
    m_anglesDeg.erase(std::unique(m_anglesDeg.begin(), m_anglesDeg.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-6;
    }), m_anglesDeg.end());
}

Point3D CylindricalCoordinates::toCartesian(double r, double thetaDeg, double z) const
{
    double rad = thetaDeg * M_PI / 180.0;
    return Point3D(r * std::cos(rad), r * std::sin(rad), z);
}

Point3D CylindricalCoordinates::gridPoint(int ir, int itheta, int iz) const
{
    double r = (ir >= 0 && static_cast<size_t>(ir) < m_radii.size()) ? m_radii[ir] : 0.0;
    double th = (itheta >= 0 && static_cast<size_t>(itheta) < m_anglesDeg.size()) ? m_anglesDeg[itheta] : 0.0;
    double z = (iz >= 0 && static_cast<size_t>(iz) < m_zLevels.size()) ? m_zLevels[iz] : 0.0;
    return toCartesian(r, th, z);
}

bool CylindricalCoordinates::findClosestPolar(const Point3D& worldPnt, double tol, Point3D& outPnt) const
{
    double closestDist = tol + 1.0;
    bool found = false;

    for (double r : m_radii)
    {
        for (double th : m_anglesDeg)
        {
            for (double z : m_zLevels)
            {
                Point3D cand = toCartesian(r, th, z);
                double d = cand.distance(worldPnt);
                if (d <= tol && d < closestDist)
                {
                    closestDist = d;
                    outPnt = cand;
                    found = true;
                }
            }
        }
    }
    return found;
}

} // namespace TSA::Coordinate
