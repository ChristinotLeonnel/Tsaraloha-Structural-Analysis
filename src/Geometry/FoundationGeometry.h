#pragma once

#include <TopoDS_Shape.hxx>

namespace TSA::Model
{
    class Node;
    class Foundation;
}

namespace TSA::Geometry
{

class FoundationGeometry
{
public:
    static TopoDS_Shape createFoundationShape(
        const TSA::Model::Node& node,
        double widthA,
        double lengthB,
        double heightH
    );
};

} // namespace TSA::Geometry
