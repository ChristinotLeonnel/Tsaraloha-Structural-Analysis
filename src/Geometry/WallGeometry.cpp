#include "WallGeometry.h"
#include "../Model/Node.h"
#include "../Model/Wall.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <cmath>

namespace TSA::Geometry
{

TopoDS_Shape WallGeometry::createWallShape(
    const TSA::Model::Node& startNode,
    const TSA::Model::Node& endNode,
    double height,
    double thickness,
    double offset)
{
    gp_Pnt pA(startNode.x(), startNode.y(), startNode.z());
    gp_Pnt pB(endNode.x(), endNode.y(), endNode.z());

    gp_Vec vAB(pB.X() - pA.X(), pB.Y() - pA.Y(), 0.0);
    double lenXY = vAB.Magnitude();
    if (lenXY < 1e-4)
    {
        return TopoDS_Shape();
    }

    gp_Vec dirL = vAB / lenXY; // Direction longitudinale horizontale
    gp_Vec dirN(-dirL.Y(), dirL.X(), 0.0); // Normale horizontale

    double halfT = thickness / 2.0;

    // Décalage optionnel selon la normale
    gp_Vec offVec = dirN * offset;

    // 4 sommets à la base du voile
    gp_Pnt b1 = pA.Translated(offVec - dirN * halfT);
    gp_Pnt b2 = pB.Translated(offVec - dirN * halfT);
    gp_Pnt b3 = pB.Translated(offVec + dirN * halfT);
    gp_Pnt b4 = pA.Translated(offVec + dirN * halfT);

    BRepBuilderAPI_MakePolygon poly;
    poly.Add(b1);
    poly.Add(b2);
    poly.Add(b3);
    poly.Add(b4);
    poly.Close();

    if (!poly.IsDone())
        return TopoDS_Shape();

    BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
    if (!faceMaker.IsDone())
        return TopoDS_Shape();

    // Extrusion verticale le long de Z sur la hauteur height
    gp_Vec vUp(0.0, 0.0, height);
    BRepPrimAPI_MakePrism prism(faceMaker.Face(), vUp);
    if (!prism.IsDone())
        return TopoDS_Shape();

    return prism.Shape();
}

} // namespace TSA::Geometry
