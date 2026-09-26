#include "Beam.h"
#include "Model.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

Beam::Beam(int id, int startNodeId, int endNodeId, double width, double height, const std::string& name, BarRole role)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_role(role)
    , m_section(Section::rectangular(width, height))
    , m_material(Material::concreteC25_30())
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        const char* prefix = (m_role == BarRole::Column) ? "C" : ((m_role == BarRole::Brace) ? "D" : "B");
        ss << prefix << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

Beam::Beam(int id, int startNodeId, int endNodeId, const Section& section, const Material& material, BarRole role, double rotation, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_role(role)
    , m_section(section)
    , m_material(material)
    , m_rotation(rotation)
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        const char* prefix = (m_role == BarRole::Column) ? "C" : ((m_role == BarRole::Brace) ? "D" : "B");
        ss << prefix << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

BarProperties Beam::properties() const
{
    BarProperties p;
    p.id = m_id;
    p.name = m_name;
    p.role = m_role;
    p.section = m_section;
    p.material = m_material;
    p.rotation = m_rotation;
    p.eccentricity = m_eccentricity;
    p.startRelease = m_startRelease;
    p.endRelease = m_endRelease;
    p.color = m_color;
    return p;
}

void Beam::setProperties(const BarProperties& props)
{
    m_name = props.name;
    m_role = props.role;
    m_section = props.section;
    m_material = props.material;
    m_rotation = props.rotation;
    m_eccentricity = props.eccentricity;
    m_startRelease = props.startRelease;
    m_endRelease = props.endRelease;
    m_color = props.color;
}

std::string Beam::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    const char* prefix = (m_role == BarRole::Column) ? "C" : ((m_role == BarRole::Brace) ? "D" : "B");
    ss << prefix << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

void Beam::setDimensions(double width, double height)
{
    m_section.width = width;
    m_section.height = height;
}

double Beam::length(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    double dx = end->x() - start->x();
    double dy = end->y() - start->y();
    double dz = end->z() - start->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void Beam::setMaterialId(int id)
{
    m_material = Material::findById(id);
}

} // namespace TSA::Model
