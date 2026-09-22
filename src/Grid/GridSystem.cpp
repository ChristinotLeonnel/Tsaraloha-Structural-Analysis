#include "GridSystem.h"

namespace TSA::Grid
{

GridSystem::GridSystem(const GridDefinition& definition)
    : m_definition(definition)
{
    rebuildCalculators();
}

void GridSystem::updateDefinition(const GridDefinition& definition)
{
    m_definition = definition;
    rebuildCalculators();
}

void GridSystem::rebuildCalculators()
{
    if (m_definition.type() == GridType::Cartesian)
    {
        m_cartesian = std::make_unique<CartesianGrid>(m_definition);
        m_cylindrical.reset();
    }
    else
    {
        m_cylindrical = std::make_unique<CylindricalGrid>(m_definition);
        m_cartesian.reset();
    }
}

GridSnapResult GridSystem::findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const
{
    if (!m_isActive || !m_isVisible)
    {
        return GridSnapResult{};
    }

    if (m_definition.type() == GridType::Cartesian && m_cartesian)
    {
        return m_cartesian->findClosestSnap(worldPoint, snapToleranceWorld);
    }
    else if (m_definition.type() == GridType::Cylindrical && m_cylindrical)
    {
        return m_cylindrical->findClosestSnap(worldPoint, snapToleranceWorld);
    }

    return GridSnapResult{};
}

} // namespace TSA::Grid
