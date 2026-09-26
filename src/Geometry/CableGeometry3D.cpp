#include "CableGeometry3D.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Model.h"
#include "../Model/Node.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>

#include <cmath>
#include <algorithm>

namespace TSA::Geometry
{

TopoDS_Shape CableGeometry3D::createStraightCable(
    const gp_Pnt& pA,
    const gp_Pnt& pB,
    double diameterMeters)
{
    gp_Vec vAB(pA, pB);
    double length = vAB.Magnitude();
    if (length < 1e-4)
    {
        return TopoDS_Shape();
    }

    double radius = std::max(0.0005, diameterMeters / 2.0);

    gp_Vec dirZ = vAB / length;
    gp_Vec globalZ(0.0, 0.0, 1.0);
    gp_Vec dirX;

    if (std::abs(dirZ.Dot(globalZ)) > 0.999)
    {
        gp_Vec refX(1.0, 0.0, 0.0);
        dirX = dirZ.Crossed(refX);
    }
    else
    {
        dirX = dirZ.Crossed(globalZ);
    }
    dirX.Normalize();

    try
    {
        gp_Ax2 axes(pA, gp_Dir(dirZ), gp_Dir(dirX));
        BRepPrimAPI_MakeCylinder cyl(axes, radius, length);
        cyl.Build();
        if (cyl.IsDone() && !cyl.Shape().IsNull())
        {
            return cyl.Shape();
        }
    }
    catch (...)
    {
    }

    try
    {
        gp_Ax2 axes(pA, gp_Dir(dirZ), gp_Dir(dirX));
        gp_Circ circ(axes, radius);
        BRepBuilderAPI_MakeEdge makeEdge(circ);
        BRepBuilderAPI_MakeWire makeWire(makeEdge.Edge());
        gp_Pln pln(pA, gp_Dir(dirZ));
        BRepBuilderAPI_MakeFace makeFace(pln, makeWire.Wire());
        BRepPrimAPI_MakePrism prism(makeFace.Face(), vAB);
        if (prism.IsDone() && !prism.Shape().IsNull())
        {
            return prism.Shape();
        }
    }
    catch (...)
    {
    }

    return TopoDS_Shape();
}

TopoDS_Shape CableGeometry3D::createAnchorSocket(
    const gp_Pnt& anchorPos,
    const gp_Pnt& towardPoint,
    double socketDiameter,
    double socketLength)
{
    gp_Vec v(anchorPos, towardPoint);
    if (v.Magnitude() < 1e-4)
    {
        return TopoDS_Shape();
    }
    v.Normalize();

    double radius = std::max(0.002, socketDiameter / 2.0);
    double length = std::max(0.01, socketLength);

    gp_Vec globalZ(0.0, 0.0, 1.0);
    gp_Vec dirX = (std::abs(v.Dot(globalZ)) > 0.999) ? v.Crossed(gp_Vec(1.0, 0.0, 0.0)) : v.Crossed(globalZ);
    dirX.Normalize();

    try
    {
        gp_Ax2 axes(anchorPos, gp_Dir(v), gp_Dir(dirX));
        BRepPrimAPI_MakeCylinder cyl(axes, radius, length);
        cyl.Build();
        if (cyl.IsDone() && !cyl.Shape().IsNull())
        {
            return cyl.Shape();
        }
    }
    catch (...)
    {
    }

    try
    {
        gp_Ax2 axes(anchorPos, gp_Dir(v), gp_Dir(dirX));
        gp_Circ circ(axes, radius);
        BRepBuilderAPI_MakeEdge makeEdge(circ);
        BRepBuilderAPI_MakeWire makeWire(makeEdge.Edge());
        gp_Pln pln(anchorPos, gp_Dir(v));
        BRepBuilderAPI_MakeFace makeFace(pln, makeWire.Wire());
        BRepPrimAPI_MakePrism prism(makeFace.Face(), v * length);
        if (prism.IsDone() && !prism.Shape().IsNull())
        {
            return prism.Shape();
        }
    }
    catch (...)
    {
    }

    return TopoDS_Shape();
}

TopoDS_Shape CableGeometry3D::createCurvedCable(
    const std::vector<gp_Pnt>& points,
    double diameterMeters)
{
    if (points.size() < 2)
    {
        return TopoDS_Shape();
    }

    if (points.size() == 2)
    {
        return createStraightCable(points[0], points[1], diameterMeters);
    }

    double radius = std::max(0.0005, diameterMeters / 2.0);

    // 1. Tentative avec l'algorithme de courbe lisse continue B-Spline et Sweep Pipe
    try
    {
        const int nPts = static_cast<int>(points.size());
        Handle(TColgp_HArray1OfPnt) hPoints = new TColgp_HArray1OfPnt(1, nPts);
        for (int i = 0; i < nPts; ++i)
        {
            hPoints->SetValue(i + 1, points[i]);
        }

        GeomAPI_Interpolate interpolator(hPoints, Standard_False, 1e-5);
        interpolator.Perform();
        if (interpolator.IsDone())
        {
            Handle(Geom_BSplineCurve) curve = interpolator.Curve();
            if (!curve.IsNull())
            {
                BRepBuilderAPI_MakeEdge mkEdge(curve);
                if (mkEdge.IsDone())
                {
                    BRepBuilderAPI_MakeWire mkWire(mkEdge.Edge());
                    if (mkWire.IsDone())
                    {
                        TopoDS_Wire spine = mkWire.Wire();

                        // Construction du cercle normal au point initial
                        gp_Pnt p0 = points.front();
                        gp_Vec tangent = points[1].XYZ() - points[0].XYZ();
                        if (tangent.Magnitude() < 1e-6) tangent = gp_Vec(1.0, 0.0, 0.0);
                        tangent.Normalize();

                        gp_Vec globalZ(0.0, 0.0, 1.0);
                        gp_Vec normalX = (std::abs(tangent.Dot(globalZ)) > 0.999) 
                                         ? tangent.Crossed(gp_Vec(1.0, 0.0, 0.0)) 
                                         : tangent.Crossed(globalZ);
                        normalX.Normalize();

                        gp_Ax2 diskAxes(p0, gp_Dir(tangent), gp_Dir(normalX));
                        gp_Circ profileCirc(diskAxes, radius);
                        BRepBuilderAPI_MakeEdge mkCircEdge(profileCirc);
                        BRepBuilderAPI_MakeWire mkCircWire(mkCircEdge.Edge());
                        BRepBuilderAPI_MakeFace mkCircFace(mkCircWire.Wire());

                        if (mkCircFace.IsDone())
                        {
                            BRepOffsetAPI_MakePipe mkPipe(spine, mkCircFace.Face());
                            if (mkPipe.IsDone() && !mkPipe.Shape().IsNull())
                            {
                                return mkPipe.Shape();
                            }
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
    }

    // 2. Alternative robuste : Assemblage de segments cylindriques raccordés
    try
    {
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        bool hasAny = false;

        for (size_t i = 1; i < points.size(); ++i)
        {
            TopoDS_Shape seg = createStraightCable(points[i - 1], points[i], diameterMeters);
            if (!seg.IsNull())
            {
                builder.Add(compound, seg);
                hasAny = true;
            }
        }

        if (hasAny)
        {
            return compound;
        }
    }
    catch (...)
    {
    }

    return createStraightCable(points.front(), points.back(), diameterMeters);
}

TopoDS_Shape CableGeometry3D::createCableShape(
    const TSA::Model::Cable& cable,
    const TSA::Model::Model& model,
    bool includeAnchors)
{
    const auto* nA = model.getNode(cable.startNodeId());
    const auto* nB = model.getNode(cable.endNodeId());
    if (!nA || !nB)
    {
        return TopoDS_Shape();
    }

    gp_Pnt pA(nA->x(), nA->y(), nA->z());
    gp_Pnt pB(nB->x(), nB->y(), nB->z());

    if (pA.Distance(pB) < 1e-4)
    {
        return TopoDS_Shape();
    }

    double diam = cable.diameter();
    TopoDS_Shape bodyShape;

    if (cable.geometryMode() == TSA::Model::CableGeometryMode::Straight)
    {
        bodyShape = createStraightCable(pA, pB, diam);
    }
    else
    {
        std::vector<gp_Pnt> pts = cable.sampleWorldPoints(model, 25);
        if (pts.size() >= 2)
        {
            bodyShape = createCurvedCable(pts, diam);
        }
        else
        {
            bodyShape = createStraightCable(pA, pB, diam);
        }
    }

    if (!includeAnchors || bodyShape.IsNull())
    {
        return bodyShape;
    }

    // Ajout visuel des ancrages aux extrémités (têtes d'ancrage Ø et L adaptées)
    double socketDiam = std::max(diam * 2.2, 0.03);
    double socketLen = std::max(diam * 4.0, 0.08);

    TopoDS_Shape socketA = createAnchorSocket(pA, pB, socketDiam, socketLen);
    TopoDS_Shape socketB = createAnchorSocket(pB, pA, socketDiam, socketLen);

    if (socketA.IsNull() && socketB.IsNull())
    {
        return bodyShape;
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, bodyShape);
    if (!socketA.IsNull()) builder.Add(compound, socketA);
    if (!socketB.IsNull()) builder.Add(compound, socketB);

    return compound;
}

} // namespace TSA::Geometry
