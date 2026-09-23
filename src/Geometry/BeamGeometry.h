#pragma once

#include <TopoDS_Shape.hxx>
#include "../Model/Section.h"

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
    // Construit un solide 3D TopoDS_Shape selon la géométrie exacte de la section et l'angle de rotation bêta
    static TopoDS_Shape createBeamShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        const TSA::Model::Section& section,
        double rotationDegrees = 0.0
    );

    // Surcharge de compatibilité pour sections rectangulaires simples
    static TopoDS_Shape createBeamShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        double width,
        double height,
        double rotationDegrees = 0.0
    );

    // Construit une sphère 3D représentant un nœud structural
    static TopoDS_Shape createNodeShape(
        const TSA::Model::Node& node,
        double radius = 0.08
    );
};

} // namespace TSA::Geometry
