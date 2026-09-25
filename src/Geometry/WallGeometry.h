#pragma once

#include <TopoDS_Shape.hxx>

namespace TSA::Model
{
    class Node;
    class Wall;
}

namespace TSA::Geometry
{

class WallGeometry
{
public:
    static TopoDS_Shape createWallShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        double height,
        double thickness,
        double offset = 0.0
    );
};

} // namespace TSA::Geometry
