#pragma once

#include <gp_Pnt.hxx>
#include <string>

namespace TSA::Grid
{

enum class GridType
{
    Cartesian,
    Cylindrical
};

enum class GridSnapType
{
    None,
    Intersection,
    AxisLine,
    Circle,
    RadialLine,
    LevelPlane,
    Origin,
    Node
};

struct GridSnapResult
{
    bool snapped = false;
    gp_Pnt point = gp_Pnt(0.0, 0.0, 0.0);
    GridSnapType type = GridSnapType::None;
    double distance = 1e9;
    std::string description;
};

} // namespace TSA::Grid
