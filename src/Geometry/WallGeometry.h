#pragma once

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <vector>

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

    /// Voile percé : contour médian (encoches des portes compris) et trous (fenêtres) dans le plan médian, extrudés de
    /// thickness selon la normale, centrés sur le plan médian. Contours issus de TSA::Mesh::wallSurface (géométrie du calcul).
    static TopoDS_Shape createWallShape(const std::vector<gp_Pnt>& outer, const std::vector<std::vector<gp_Pnt>>& holes, const gp_Vec& normal,
                                        double thickness);
};

} // namespace TSA::Geometry
