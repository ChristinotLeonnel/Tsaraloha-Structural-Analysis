#include "CableAnchor.h"
#include <cmath>

namespace TSA::Model
{

CableAnchor::CableAnchor()
    : m_id(0)
    , m_name("Anchor_1")
    , m_type(AnchorType::Fixed)
{
}

CableAnchor::CableAnchor(int id, const std::string& name, AnchorType type, const TSA::Coordinate::Point3D& pos)
    : m_id(id)
    , m_name(name)
    , m_type(type)
    , m_position(pos)
{
}

void CableAnchor::setDirection(double dx, double dy, double dz)
{
    double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-9)
    {
        m_dirX = dx / len;
        m_dirY = dy / len;
        m_dirZ = dz / len;
    }
    else
    {
        m_dirX = 0.0;
        m_dirY = 0.0;
        m_dirZ = 1.0;
    }
}

gp_Dir CableAnchor::toGpDir() const
{
    double len = std::sqrt(m_dirX * m_dirX + m_dirY * m_dirY + m_dirZ * m_dirZ);
    if (len > 1e-9)
    {
        return gp_Dir(m_dirX, m_dirY, m_dirZ);
    }
    return gp_Dir(0.0, 0.0, 1.0);
}

} // namespace TSA::Model
