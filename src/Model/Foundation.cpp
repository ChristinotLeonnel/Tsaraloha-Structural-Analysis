#include "Foundation.h"
#include "Model.h"
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Foundation::Foundation(int id, int nodeId, double widthA, double lengthB, double heightH, const std::string& name, FoundationType type)
    : m_id(id)
    , m_name(name)
    , m_nodeId(nodeId)
    , m_type(type)
    , m_widthA(widthA)
    , m_lengthB(lengthB)
    , m_heightH(heightH)
    , m_material(Material::concreteC25_30())
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "F" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string Foundation::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "F" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

} // namespace TSA::Model
