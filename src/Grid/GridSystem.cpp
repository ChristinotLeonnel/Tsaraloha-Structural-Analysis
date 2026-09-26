#include "GridSystem.h"

namespace TSA::Grid
{

GridSystem::GridSystem(const GridDefinition& definition)
    : m_definition(definition)
    , m_isVisible(definition.isVisible())
    , m_isActive(definition.isActive())
    , m_showLabels(definition.showLabels())
    , m_showIntersections(definition.showIntersections())
{
    rebuildCalculators();
}

void GridSystem::updateDefinition(const GridDefinition& definition)
{
    std::string currentId = m_definition.id();
    bool wasActive = m_isActive;
    m_definition = definition;
    if (!currentId.empty())
    {
        m_definition.setId(currentId);
    }
    m_isVisible = definition.isVisible();
    m_isActive = definition.isActive() || wasActive;
    m_showLabels = definition.showLabels();
    m_showIntersections = definition.showIntersections();
    rebuildCalculators();
}

void GridSystem::rebuildCalculators()
{
    if (m_definition.type() == GridType::Cartesian)
    {
        m_cartesian = std::make_unique<CartesianGrid>(m_definition);
        m_cylindrical.reset();
        m_arbitrary.reset();
    }
    else if (m_definition.type() == GridType::Cylindrical)
    {
        m_cylindrical = std::make_unique<CylindricalGrid>(m_definition);
        m_cartesian.reset();
        m_arbitrary.reset();
    }
    else if (m_definition.type() == GridType::Arbitrary)
    {
        m_arbitrary = std::make_unique<ArbitraryGrid>(m_definition);
        m_cartesian.reset();
        m_cylindrical.reset();
    }
}

GridSnapResult GridSystem::findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const
{
    if (!m_isVisible)
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
    else if (m_definition.type() == GridType::Arbitrary && m_arbitrary)
    {
        return m_arbitrary->findClosestSnap(worldPoint, snapToleranceWorld);
    }

    return GridSnapResult{};
}

} // namespace TSA::Grid
