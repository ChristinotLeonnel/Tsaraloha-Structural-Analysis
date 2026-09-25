#include "FoundationGeometry.h"
#include "../Model/Node.h"
#include "../Model/Foundation.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

namespace TSA::Geometry
{

TopoDS_Shape FoundationGeometry::createFoundationShape(
    const TSA::Model::Node& node,
    double widthA,
    double lengthB,
    double heightH)
{
    // Semelle centrée sous le nœud : s'étend de z - heightH jusqu'à z
    double minX = node.x() - widthA / 2.0;
    double minY = node.y() - lengthB / 2.0;
    double minZ = node.z() - heightH;

    gp_Pnt pMin(minX, minY, minZ);
    gp_Pnt pMax(minX + widthA, minY + lengthB, node.z());

    BRepPrimAPI_MakeBox box(pMin, pMax);
    if (!box.IsDone())
        return TopoDS_Shape();

    return box.Shape();
}

} // namespace TSA::Geometry
