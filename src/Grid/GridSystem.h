#pragma once

#include "GridDefinition.h"
#include "CartesianGrid.h"
#include "CylindricalGrid.h"
#include <memory>

namespace TSA::Grid
{

class GridSystem
{
public:
    explicit GridSystem(const GridDefinition& definition);
    ~GridSystem() = default;

    const GridDefinition& definition() const { return m_definition; }
    void updateDefinition(const GridDefinition& definition);

    const std::string& id() const { return m_definition.id(); }
    const std::string& name() const { return m_definition.name(); }
    GridType type() const { return m_definition.type(); }

    // États de visibilité et d'activité
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; m_definition.setVisible(visible); }

    bool isActive() const { return m_isActive; }
    void setActive(bool active) { m_isActive = active; m_definition.setActive(active); }

    bool showLabels() const { return m_showLabels; }
    void setShowLabels(bool show) { m_showLabels = show; m_definition.setShowLabels(show); }

    bool showIntersections() const { return m_showIntersections; }
    void setShowIntersections(bool show) { m_showIntersections = show; m_definition.setShowIntersections(show); }

    bool showLevels() const { return m_showLevels; }
    void setShowLevels(bool show) { m_showLevels = show; }

    // Accès aux calculateurs mathématiques
    const CartesianGrid* cartesian() const { return m_cartesian.get(); }
    const CylindricalGrid* cylindrical() const { return m_cylindrical.get(); }

    // Snapping unifié
    GridSnapResult findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const;

private:
    void rebuildCalculators();

private:
    GridDefinition m_definition;
    bool m_isVisible = true;
    bool m_isActive = true;
    bool m_showLabels = true;
    bool m_showIntersections = true;
    bool m_showLevels = true;

    std::unique_ptr<CartesianGrid> m_cartesian;
    std::unique_ptr<CylindricalGrid> m_cylindrical;
};

} // namespace TSA::Grid
