#pragma once

#include "GridDefinition.h"
#include <gp_Pnt.hxx>
#include <gp_Ax2.hxx>
#include <vector>

namespace TSA::Grid
{

struct CylindricalCircle
{
    gp_Pnt center;
    double radius = 1.0;
    double zLevel = 0.0;
    std::string label;
    int index = -1;
};

struct CylindricalRadialLine
{
    gp_Pnt start;
    gp_Pnt end;
    double angleDeg = 0.0;
    double zLevel = 0.0;
    std::string label;
    int index = -1;
};

struct CylindricalIntersection
{
    gp_Pnt point;
    int radiusIndex = -1;
    int angleIndex = -1;
    int zIndex = -1;
    double radius = 0.0;
    double angleDeg = 0.0;
    std::string labelRadius;
    std::string labelAngle;
    std::string labelZ;
};

class CylindricalGrid
{
public:
    explicit CylindricalGrid(const GridDefinition& definition);
    ~CylindricalGrid() = default;

    const GridDefinition& definition() const { return m_definition; }
    void updateDefinition(const GridDefinition& definition);

    const std::vector<CylindricalCircle>& circles() const { return m_circles; }
    const std::vector<CylindricalRadialLine>& radialLines() const { return m_radialLines; }
    const std::vector<CylindricalIntersection>& intersections() const { return m_intersections; }

    struct LabelAnchor
    {
        gp_Pnt position;
        std::string text;
        bool isAngle = false;
    };
    const std::vector<LabelAnchor>& labelAnchors() const { return m_labelAnchors; }

    double maxRadius() const { return m_maxRadius; }
    double extension() const { return m_extension; }
    void setExtension(double ext);

    // Conversions mathématiques
    gp_Pnt polarToWorld(double r, double angleDeg, double z) const;
    void worldToPolar(const gp_Pnt& worldPt, double& r, double& angleDeg, double& z) const;

    // Snapping intelligent
    GridSnapResult findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const;

private:
    void computeGeometry();

private:
    GridDefinition m_definition;
    double m_extension = 1.2; // Débord pour bulles
    double m_maxRadius = 0.0;

    std::vector<CylindricalCircle> m_circles;
    std::vector<CylindricalRadialLine> m_radialLines;
    std::vector<CylindricalIntersection> m_intersections;
    std::vector<LabelAnchor> m_labelAnchors;
};

} // namespace TSA::Grid
