#pragma once

#include "Point3D.h"
#include "LevelManager.h"
#include <vector>
#include <string>
#include <memory>

namespace TSA::Coordinate
{

class CylindricalCoordinates
{
public:
    CylindricalCoordinates() = default;

    const std::vector<double>& radii() const { return m_radii; }
    void setRadii(const std::vector<double>& rList);

    const std::vector<double>& anglesDeg() const { return m_anglesDeg; }
    void setAnglesDeg(const std::vector<double>& aList);

    // Niveaux Z (partagés ou synchronisés)
    const std::vector<double>& zLevels() const { return m_zLevels; }
    void setZLevels(const std::vector<double>& zList) { m_zLevels = zList; }

    Point3D toCartesian(double r, double thetaDeg, double z) const;
    Point3D gridPoint(int ir, int itheta, int iz) const;

    bool findClosestPolar(const Point3D& worldPnt, double tol, Point3D& outPnt) const;

private:
    std::vector<double> m_radii;
    std::vector<double> m_anglesDeg;
    std::vector<double> m_zLevels;
};

} // namespace TSA::Coordinate
