#include "BeamGeometry.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <cmath>

namespace TSA::Geometry
{

TopoDS_Shape BeamGeometry::createBeamShape(
    const TSA::Model::Node& startNode,
    const TSA::Model::Node& endNode,
    double width,
    double height)
{
    gp_Pnt pA(startNode.x(), startNode.y(), startNode.z());
    gp_Pnt pB(endNode.x(), endNode.y(), endNode.z());

    gp_Vec vAB(pA, pB);
    double length = vAB.Magnitude();
    if (length < 1e-4)
    {
        return TopoDS_Shape();
    }

    gp_Vec dirZ = vAB / length; // Direction longitudinale

    // Détermination du repère local de la section (dirX, dirY)
    gp_Vec globalZ(0.0, 0.0, 1.0);
    gp_Vec dirX;
    gp_Vec dirY;

    // Si la barre est quasi-verticale (poteau vertical selon Z global)
    if (std::abs(dirZ.Dot(globalZ)) > 0.999)
    {
        dirX = gp_Vec(1.0, 0.0, 0.0);
        dirY = dirZ.Crossed(dirX);
        dirY.Normalize();
        dirX = dirY.Crossed(dirZ);
        dirX.Normalize();
    }
    else
    {
        // Pour une poutre horizontale ou inclinée
        dirX = dirZ.Crossed(globalZ);
        dirX.Normalize();
        dirY = dirZ.Crossed(dirX);
        dirY.Normalize();
    }

    double halfW = width / 2.0;
    double halfH = height / 2.0;

    // 4 sommets du contour rectangulaire centré sur le nœud de départ
    gp_Pnt c1 = pA.Translated(-dirX * halfW - dirY * halfH);
    gp_Pnt c2 = pA.Translated( dirX * halfW - dirY * halfH);
    gp_Pnt c3 = pA.Translated( dirX * halfW + dirY * halfH);
    gp_Pnt c4 = pA.Translated(-dirX * halfW + dirY * halfH);

    BRepBuilderAPI_MakePolygon poly;
    poly.Add(c1);
    poly.Add(c2);
    poly.Add(c3);
    poly.Add(c4);
    poly.Close();

    if (!poly.IsDone())
    {
        return TopoDS_Shape();
    }

    BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
    if (!faceMaker.IsDone())
    {
        return TopoDS_Shape();
    }

    // Extrusion du profil le long de l'axe de la barre
    BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
    if (!prism.IsDone())
    {
        return TopoDS_Shape();
    }

    return prism.Shape();
}

TopoDS_Shape BeamGeometry::createNodeShape(
    const TSA::Model::Node& node,
    double radius)
{
    gp_Pnt pt(node.x(), node.y(), node.z());
    BRepPrimAPI_MakeSphere sphere(pt, radius);
    if (!sphere.IsDone())
    {
        return TopoDS_Shape();
    }
    return sphere.Shape();
}

} // namespace TSA::Geometry
