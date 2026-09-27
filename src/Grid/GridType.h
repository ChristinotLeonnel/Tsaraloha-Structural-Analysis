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

struct AngularPattern
{
    double startAngle = 0.0; // Angle de départ en degrés
    int repeatCount = 0;     // Nombre de répétitions après l'angle initial
    double angleStep = 0.0;  // Pas angulaire / espacement en degrés
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
    Node,
    Endpoint,
    Midpoint,
    Center,
    Perpendicular,
    Nearest,
    Element
};

enum class SnapMode : uint32_t
{
    None          = 0,
    Grid          = 1 << 0,
    Node          = 1 << 1,
    Endpoint      = 1 << 2,
    Midpoint      = 1 << 3,
    Intersection  = 1 << 4,
    Center        = 1 << 5,
    Perpendicular = 1 << 6,
    Nearest       = 1 << 7,
    Axis          = 1 << 8,
    Level         = 1 << 9,
    All           = 0xFFFFFFFF
};

inline SnapMode operator|(SnapMode a, SnapMode b) {
    return static_cast<SnapMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline SnapMode operator&(SnapMode a, SnapMode b) {
    return static_cast<SnapMode>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline SnapMode operator~(SnapMode a) {
    return static_cast<SnapMode>(~static_cast<uint32_t>(a));
}
inline bool hasSnapMode(SnapMode flags, SnapMode test) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

struct GridSnapResult
{
    bool snapped = false;
    gp_Pnt point = gp_Pnt(0.0, 0.0, 0.0);
    GridSnapType type = GridSnapType::None;
    double distance = 1e9;
    std::string description;
    int targetEntityId = -1;
};

} // namespace TSA::Grid
