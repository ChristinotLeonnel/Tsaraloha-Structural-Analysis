#include "Wall.h"
#include <algorithm>
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

    // Longueur en plan : le voile est extrudé verticalement depuis sa base ; avec une base inclinée (pieds à des cotes
    // différentes), son élévation est un parallélogramme d'aire longueur en plan × hauteur.
    const double dx = end->x() - start->x();
    const double dy = end->y() - start->y();
    return std::sqrt(dx * dx + dy * dy);
}

double Wall::baseRise(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    return (start && end) ? end->z() - start->z() : 0.0;
}

double Wall::grossArea(const Model& model) const
{
    return length(model) * m_height;
}

double Wall::area(const Model& model) const
{
    double a = grossArea(model);
    for (const auto& o : m_openings) a -= std::abs(polygonArea(o.polygon));
    return std::max(0.0, a);
}

void Wall::setMaterialId(int id)
{
    m_material = Material::findById(id);
}

} // namespace TSA::Model
