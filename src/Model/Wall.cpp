#include "Wall.h"
#include "Model.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Wall::Wall(int id, int startNodeId, int endNodeId, double height, double thickness, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_height(height)
    , m_thickness(thickness)
    , m_material(Material::concreteC25_30())
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "W" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string Wall::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "W" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

double Wall::length(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    double dx = end->x() - start->x();
    double dy = end->y() - start->y();
    double dz = end->z() - start->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Wall::area(const Model& model) const
{
    return length(model) * m_height;
}

void Wall::setMaterialId(int id)
{
    m_material = Material::findById(id);
}

} // namespace TSA::Model
