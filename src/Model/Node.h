#pragma once

namespace TSA::Model
{

class Node
{
public:
    Node() = default;
    Node(int id, double x, double y, double z);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    double x() const { return m_x; }
    void setX(double x) { m_x = x; }

    double y() const { return m_y; }
    void setY(double y) { m_y = y; }

    double z() const { return m_z; }
    void setZ(double z) { m_z = z; }

    void setCoordinates(double x, double y, double z);

private:
    int m_id = 0;
    double m_x = 0.0;
    double m_y = 0.0;
    double m_z = 0.0;
};

} // namespace TSA::Model
