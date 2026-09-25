#include "TrussMember.h"
#include "Model.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::Model
{

TrussMember::TrussMember(int id, int startNodeId, int endNodeId, double diameterOrWidth, const std::string& name, TrussMemberRole role)
    : m_id(id)
    , m_name(name)
    , m_startNodeId(startNodeId)
    , m_endNodeId(endNodeId)
    , m_role(role)
    , m_section(Section::circular(diameterOrWidth))
    , m_material(Material::steelS235())
{
    if (m_name.empty() && m_id > 0)
    {
        std::ostringstream ss;
        ss << "TR" << std::setw(3) << std::setfill('0') << m_id;
        m_name = ss.str();
    }
}

std::string TrussMember::formattedName() const
{
    if (!m_name.empty())
        return m_name;
    std::ostringstream ss;
    ss << "TR" << std::setw(3) << std::setfill('0') << m_id;
    return ss.str();
}

double TrussMember::length(const Model& model) const
{
    const auto* start = model.getNode(m_startNodeId);
    const auto* end   = model.getNode(m_endNodeId);
    if (!start || !end) return 0.0;

    double dx = end->x() - start->x();
    double dy = end->y() - start->y();
    double dz = end->z() - start->z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace TSA::Model
