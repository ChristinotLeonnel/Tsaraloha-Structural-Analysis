#pragma once

#include <gp_Pnt.hxx>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

namespace TSA::Coordinate
{

class Point3D
{
public:
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Point3D() = default;
    constexpr Point3D(double inX, double inY, double inZ)
        : x(inX), y(inY), z(inZ) {}

    // Constructeur et conversion implicite avec OpenCASCADE gp_Pnt
    Point3D(const gp_Pnt& p)
        : x(p.X()), y(p.Y()), z(p.Z()) {}

    operator gp_Pnt() const
    {
        return gp_Pnt(x, y, z);
    }

    gp_Pnt toGpPnt() const
    {
        return gp_Pnt(x, y, z);
    }

    double distance(const Point3D& other) const
    {
        double dx = x - other.x;
        double dy = y - other.y;
        double dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double distanceXY(const Point3D& other) const
    {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool isAlmostEqual(const Point3D& other, double tol = 1e-4) const
    {
        return std::abs(x - other.x) <= tol &&
               std::abs(y - other.y) <= tol &&
               std::abs(z - other.z) <= tol;
    }

    Point3D operator+(const Point3D& other) const
    {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }

    Point3D operator-(const Point3D& other) const
    {
        return Point3D(x - other.x, y - other.y, z - other.z);
    }

    Point3D operator*(double scalar) const
    {
        return Point3D(x * scalar, y * scalar, z * scalar);
    }

    Point3D& operator+=(const Point3D& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Point3D& operator-=(const Point3D& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    bool operator==(const Point3D& other) const
    {
        return isAlmostEqual(other);
    }

    bool operator!=(const Point3D& other) const
    {
        return !(*this == other);
    }

    std::string toString(int precision = 3) const
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(precision)
           << "(" << x << ", " << y << ", " << z << ")";
        return ss.str();
    }
};

} // namespace TSA::Coordinate

// Alias global de confort pour les calculs de structure
using Point3D = TSA::Coordinate::Point3D;
