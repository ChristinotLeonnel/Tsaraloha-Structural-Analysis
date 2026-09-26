#include "Column.h"
#include "Model.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Column::Column(int id, int startNodeId, int endNodeId, double width, double height, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_section(Section::rectangular(width, height))
    , m_material(Material::concreteC25_30())
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "C" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

Column::Column(int id, int startNodeId, int endNodeId, const Section& section, const Material& material, double rotation, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_section(section)
    , m_material(material)
    , m_rotation(rotation)
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "C" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string Column::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "C" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

void Column::setDimensions(double width, double height)
{
    m_section.width = width;
    m_section.height = height;
}

double Column::length(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    double dx = end->x() - start->x();
    double dy = end->y() - start->y();
    double dz = end->z() - start->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool Column::isVertical(const Model& model, double tol) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return false;

    double dx = std::abs(end->x() - start->x());
    double dy = std::abs(end->y() - start->y());
    return (dx < tol && dy < tol);
}

double Column::bottomElevation(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    return std::min(start->z(), end->z());
}

double Column::topElevation(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    return std::max(start->z(), end->z());
}

std::string Column::direction(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return "Indéfini";

    if (isVertical(model))
    {
        return (end->z() >= start->z()) ? "Vertical (+Z)" : "Vertical (-Z)";
    }
    return "Incliné";
}

BarProperties Column::properties() const
{
    BarProperties p;
    p.id = m_id;
    p.name = m_name;
    p.role = BarRole::Column;
    p.section = m_section;
    p.material = m_material;
    p.rotation = m_rotation;
    p.color = m_color;
    return p;
}

} // namespace TSA::Model
