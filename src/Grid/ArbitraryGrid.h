#pragma once

#include "GridDefinition.h"
#include "GridType.h"
#include <gp_Pnt.hxx>
#include <vector>
#include <string>

namespace TSA::Grid
{

struct ArbitraryRenderLine
{
    gp_Pnt start;
    gp_Pnt end;
    std::string label;
    std::string type; // "droite" ou "segment"
    bool isBold = false;
};

struct ArbitraryLabelAnchor
{
    gp_Pnt position;
    std::string text;
    bool isBold = false;
};

class ArbitraryGrid
{
public:
    explicit ArbitraryGrid(const GridDefinition& definition);
    ~ArbitraryGrid() = default;

    const GridDefinition& definition() const { return m_definition; }

    const std::vector<ArbitraryRenderLine>& renderLines() const { return m_renderLines; }
    const std::vector<gp_Pnt>& intersections() const { return m_intersections; }
    const std::vector<ArbitraryLabelAnchor>& labelAnchors() const { return m_labelAnchors; }

    GridSnapResult findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const;

    void rebuild();

private:
    void computeGeometry();
    static bool computeLineIntersection(const gp_Pnt& p1, const gp_Pnt& p2,
                                        const gp_Pnt& p3, const gp_Pnt& p4,
                                        gp_Pnt& outIntersection);

private:
    GridDefinition m_definition;
    std::vector<ArbitraryRenderLine> m_renderLines;
    std::vector<gp_Pnt> m_intersections;
    std::vector<ArbitraryLabelAnchor> m_labelAnchors;
};

} // namespace TSA::Grid
