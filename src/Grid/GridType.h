#pragma once

#include <gp_Pnt.hxx>
#include <string>

namespace TSA::Grid
{

enum class GridType
{
    Cartesian,
    Cylindrical,
    Arbitrary
};

struct ArbitraryLine
{
    std::string label;
    gp_Pnt p1 = gp_Pnt(0.0, 0.0, 0.0);
    gp_Pnt p2 = gp_Pnt(0.0, 0.0, 0.0);
    std::string type = "droite"; // "droite" (ligne infinie / étendue) ou "segment"
    bool isBold = false;
};

struct GridDisplaySettings
{
    std::string lineColor;         // Hex code e.g. "#7A8494", empty = theme default
    std::string lineStyle = "dashed"; // "solid", "dashed", "dotted"
    double lineWidth = 1.0;
    double extension = 1.50;       // Débordement au-delà des limites en mètres
    double bubbleRadius = 0.40;    // Rayon de la bulle d'axe en mètres
    bool showBubbles = true;
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
