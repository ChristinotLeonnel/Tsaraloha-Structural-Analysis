#include "Node.h"
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Node::Node(int id, double x, double y, double z, const std::string& levelId, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_x(x)
    , m_y(y)
    , m_z(z)
    , m_levelId(levelId)
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "N" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string Node::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "N" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

void Node::setCoordinates(double x, double y, double z)
{
    m_x = x;
    m_y = y;
    m_z = z;
}

} // namespace TSA::Model
