#pragma once

#include <TopoDS_Shape.hxx>
#include <vector>

namespace TSA::Model
{
    class Node;
    class Slab;
}

namespace TSA::Geometry
{

class SlabGeometry
{
public:
    // Construit un solide 3D TopoDS_Shape représentant une dalle surfacique avec épaisseur
    static TopoDS_Shape createSlabShape(
        const std::vector<const TSA::Model::Node*>& nodes,
        double thickness = 0.20
    );
};

} // namespace TSA::Geometry
