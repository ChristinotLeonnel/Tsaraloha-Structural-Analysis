#include "Node.h"

namespace TSA::Model
{

Node::Node(int id, double x, double y, double z)
    : m_id(id)
    , m_x(x)
    , m_y(y)
    , m_z(z)
{
}

void Node::setCoordinates(double x, double y, double z)
{
    m_x = x;
    m_y = y;
    m_z = z;
}

} // namespace TSA::Model
