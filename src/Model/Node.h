#pragma once
#include "../Coordinate/Point3D.h"
#include <string>

namespace TSA::Model
{

class Node
{
public:
    Node() = default;
    Node(int id, double x, double y, double z, const std::string& levelId = "");

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    double x() const { return m_x; }
    void setX(double x) { m_x = x; }

    double y() const { return m_y; }
    void setY(double y) { m_y = y; }

    double z() const { return m_z; }
    void setZ(double z) { m_z = z; }

    void setCoordinates(double x, double y, double z);

    TSA::Coordinate::Point3D point3D() const { return TSA::Coordinate::Point3D(m_x, m_y, m_z); }

    const std::string& levelId() const { return m_levelId; }
    void setLevelId(const std::string& lvlId) { m_levelId = lvlId; }

private:
    int m_id = 0;
    double m_x = 0.0;
    double m_y = 0.0;
    double m_z = 0.0;
    std::string m_levelId;
};

} // namespace TSA::Model
