#pragma once

#include <string>

namespace TSA::Coordinate
{

struct Level
{
    std::string id;
    std::string name;
    double elevation = 0.0;
    bool visible = true;

    Level() = default;
    Level(const std::string& inId, const std::string& inName, double inElev, bool inVisible = true)
        : id(inId), name(inName), elevation(inElev), visible(inVisible) {}
};

} // namespace TSA::Coordinate
