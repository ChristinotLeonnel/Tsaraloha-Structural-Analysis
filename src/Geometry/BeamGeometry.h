#pragma once

#include <TopoDS_Shape.hxx>

namespace TSA::Model
{
    class Node;
    class Beam;
}

namespace TSA::Geometry
{

class BeamGeometry
{
public:
    // Construit un solide 3D TopoDS_Shape à section rectangulaire reliant les deux nœuds
    static TopoDS_Shape createBeamShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        double width,
        double height
    );

    // Construit une sphère 3D représentant un nœud structural
    static TopoDS_Shape createNodeShape(
        const TSA::Model::Node& node,
        double radius = 0.08
    );
};

} // namespace TSA::Geometry
