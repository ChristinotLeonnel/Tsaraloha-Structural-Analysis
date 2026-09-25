#include "BeamGeometry.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Section.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TSA::Geometry
{

TopoDS_Shape BeamGeometry::createBeamShape(
    const TSA::Model::Node& startNode,
    const TSA::Model::Node& endNode,
    const TSA::Model::Section& section,
    double rotationDegrees,
    TSA::Model::BarEccentricity eccentricity)
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

    // 1. Repère local de référence (dirX0, dirY0)
    gp_Vec globalZ(0.0, 0.0, 1.0);
    gp_Vec dirX0;
    gp_Vec dirY0;

    if (std::abs(dirZ.Dot(globalZ)) > 0.999)
    {
        // Quasi-verticale (poteau vertical selon Z)
        gp_Vec refX(1.0, 0.0, 0.0);
        dirY0 = dirZ.Crossed(refX);
        dirY0.Normalize();
        dirX0 = dirY0.Crossed(dirZ);
        dirX0.Normalize();
    }
    else
    {
        // Barre horizontale ou inclinée
        dirX0 = dirZ.Crossed(globalZ);
        dirX0.Normalize();
        dirY0 = dirZ.Crossed(dirX0);
        dirY0.Normalize();
    }

    // 2. Application de l'angle de rotation bêta dans le plan de la section
    double rad = rotationDegrees * (M_PI / 180.0);
    double cosB = std::cos(rad);
    double sinB = std::sin(rad);

    gp_Vec dirX = dirX0 * cosB + dirY0 * sinB;
    gp_Vec dirY = -dirX0 * sinB + dirY0 * cosB;

    // 2.5 Décalage d'excentrement dans le plan de la section (dirX, dirY)
    if (eccentricity == TSA::Model::BarEccentricity::TopFlange)
    {
        pA = pA.Translated(-dirY * (section.height / 2.0));
    }
    else if (eccentricity == TSA::Model::BarEccentricity::BottomFlange)
    {
        pA = pA.Translated(dirY * (section.height / 2.0));
    }
    else if (eccentricity == TSA::Model::BarEccentricity::LeftFlange)
    {
        pA = pA.Translated(dirX * (section.width / 2.0));
    }
    else if (eccentricity == TSA::Model::BarEccentricity::RightFlange)
    {
        pA = pA.Translated(-dirX * (section.width / 2.0));
    }

    // 3. Construction selon la forme réelle de la section
    switch (section.shape)
    {
    case TSA::Model::SectionShape::Circular:
    {
        double radius = std::max(0.001, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);

        // 1. Ré-orthogonalisation stricte pour garantir la validité mathématique de gp_Ax2
        gp_Vec nZ = dirZ;
        nZ.Normalize();
        gp_Vec nX = dirX - nZ * (dirX.Dot(nZ));
        if (nX.Magnitude() < 1e-4)
        {
            gp_Vec ref(1.0, 0.0, 0.0);
            if (std::abs(nZ.Dot(ref)) > 0.99)
                ref = gp_Vec(0.0, 1.0, 0.0);
            nX = nZ.Crossed(ref);
        }
        nX.Normalize();

        // 2. Primitive cylindrique exacte et directe OpenCASCADE
        try
        {
            gp_Ax2 axes(pA, gp_Dir(nZ), gp_Dir(nX));
            BRepPrimAPI_MakeCylinder cyl(axes, radius, length);
            if (cyl.IsDone() && !cyl.Shape().IsNull())
            {
                return cyl.Shape();
            }
        }
        catch (...)
        {
        }

        // 3. Alternative : Prisme d'un cercle analytique dans le plan normal
        try
        {
            gp_Ax2 axes(pA, gp_Dir(nZ), gp_Dir(nX));
            gp_Circ circ(axes, radius);
            BRepBuilderAPI_MakeEdge makeEdge(circ);
            if (makeEdge.IsDone())
            {
                BRepBuilderAPI_MakeWire makeWire(makeEdge.Edge());
                if (makeWire.IsDone())
                {
                    gp_Pln pln(pA, gp_Dir(nZ));
                    BRepBuilderAPI_MakeFace makeFace(pln, makeWire.Wire());
                    if (makeFace.IsDone())
                    {
                        BRepPrimAPI_MakePrism prism(makeFace.Face(), vAB);
                        if (prism.IsDone() && !prism.Shape().IsNull())
                        {
                            return prism.Shape();
                        }
                    }
                }
            }
        }
        catch (...)
        {
        }

        // 4. Alternative : Discrétisation polygonale très fine (72 segments) avec support planaire explicite
        try
        {
            const int nSeg = 72;
            BRepBuilderAPI_MakePolygon poly;
            gp_Vec nY = nZ.Crossed(nX);
            nY.Normalize();
            for (int i = 0; i < nSeg; ++i)
            {
                double angle = 2.0 * M_PI * i / nSeg;
                poly.Add(pA.Translated(nX * (radius * std::cos(angle)) + nY * (radius * std::sin(angle))));
            }
            poly.Close();
            if (poly.IsDone())
            {
                gp_Pln pln(pA, gp_Dir(nZ));
                BRepBuilderAPI_MakeFace makeFace(pln, poly.Wire());
                if (makeFace.IsDone())
                {
                    BRepPrimAPI_MakePrism prism(makeFace.Face(), vAB);
                    if (prism.IsDone() && !prism.Shape().IsNull())
                    {
                        return prism.Shape();
                    }
                }
            }
        }
        catch (...)
        {
        }

        // En cas d'échec absolu, ne JAMAIS retourner un rectangle pour une section circulaire
        return TopoDS_Shape();
    }

    case TSA::Model::SectionShape::Pipe:
    {
        double ro = std::max(0.005, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);
        double tw = (section.tw > 0.0 && section.tw < ro) ? section.tw : std::min(0.01, ro * 0.2);
        double ri = ro - tw;

        gp_Vec nZ = dirZ;
        nZ.Normalize();
        gp_Vec nX = dirX - nZ * (dirX.Dot(nZ));
        if (nX.Magnitude() < 1e-4)
        {
            gp_Vec ref(1.0, 0.0, 0.0);
            if (std::abs(nZ.Dot(ref)) > 0.99)
                ref = gp_Vec(0.0, 1.0, 0.0);
            nX = nZ.Crossed(ref);
        }
        nX.Normalize();

        if (ri > 0.001)
        {
            // 1. Extrusion de face plane avec deux fils circulaires concentriques
            try
            {
                gp_Ax2 axesExt(pA, gp_Dir(nZ), gp_Dir(nX));
                gp_Circ circExt(axesExt, ro);
                BRepBuilderAPI_MakeEdge edgeExt(circExt);
                BRepBuilderAPI_MakeWire wireExt(edgeExt.Edge());

                gp_Ax2 axesInt(pA, gp_Dir(-nZ), gp_Dir(nX)); // Orientation inversée pour former le vide intérieur
                gp_Circ circInt(axesInt, ri);
                BRepBuilderAPI_MakeEdge edgeInt(circInt);
                BRepBuilderAPI_MakeWire wireInt(edgeInt.Edge());

                if (wireExt.IsDone() && wireInt.IsDone())
                {
                    gp_Pln pln(pA, gp_Dir(nZ));
                    BRepBuilderAPI_MakeFace makeFace(pln, wireExt.Wire());
                    makeFace.Add(wireInt.Wire());
                    if (makeFace.IsDone())
                    {
                        BRepPrimAPI_MakePrism prism(makeFace.Face(), vAB);
                        if (prism.IsDone() && !prism.Shape().IsNull())
                        {
                            return prism.Shape();
                        }
                    }
                }
            }
            catch (...)
            {
            }

            // 2. Discrétisation polygonale fine double wire (48 facettes)
            try
            {
                const int nSeg = 48;
                BRepBuilderAPI_MakePolygon polyExt;
                gp_Vec nY = nZ.Crossed(nX);
                nY.Normalize();
                for (int i = 0; i < nSeg; ++i)
                {
                    double angle = 2.0 * M_PI * i / nSeg;
                    polyExt.Add(pA.Translated(nX * (ro * std::cos(angle)) + nY * (ro * std::sin(angle))));
                }
                polyExt.Close();

                BRepBuilderAPI_MakePolygon polyInt;
                for (int i = nSeg - 1; i >= 0; --i)
                {
                    double angle = 2.0 * M_PI * i / nSeg;
                    polyInt.Add(pA.Translated(nX * (ri * std::cos(angle)) + nY * (ri * std::sin(angle))));
                }
                polyInt.Close();

                if (polyExt.IsDone() && polyInt.IsDone())
                {
                    gp_Pln pln(pA, gp_Dir(nZ));
                    BRepBuilderAPI_MakeFace makeFace(pln, polyExt.Wire());
                    makeFace.Add(polyInt.Wire());
                    if (makeFace.IsDone())
                    {
                        BRepPrimAPI_MakePrism prism(makeFace.Face(), vAB);
                        if (prism.IsDone() && !prism.Shape().IsNull())
                        {
                            return prism.Shape();
                        }
                    }
                }
            }
            catch (...)
            {
            }
        }

        // 3. Fallback : cylindre plein au rayon ro (mais JAMAIS un rectangle pour un tube rond)
        try
        {
            gp_Ax2 axes(pA, gp_Dir(nZ), gp_Dir(nX));
            BRepPrimAPI_MakeCylinder cyl(axes, ro, length);
            if (cyl.IsDone() && !cyl.Shape().IsNull())
            {
                return cyl.Shape();
            }
        }
        catch (...)
        {
        }

        return TopoDS_Shape();
    }

    case TSA::Model::SectionShape::IShape:
    {
        double b = std::max(0.03, section.width);
        double h = std::max(0.05, section.height);
        double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.08);
        double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : std::max(0.008, h * 0.10);

        double b2 = b / 2.0;
        double h2 = h / 2.0;
        double w2 = tw / 2.0;
        double hin = h2 - tf;

        // 12 sommets du profil en I dans le plan local (dirX=largeur, dirY=hauteur)
        std::vector<std::pair<double, double>> pts2D = {
            {  b2,  h2 },
            { -b2,  h2 },
            { -b2,  hin },
            { -w2,  hin },
            { -w2, -hin },
            { -b2, -hin },
            { -b2, -h2 },
            {  b2, -h2 },
            {  b2, -hin },
            {  w2, -hin },
            {  w2,  hin },
            {  b2,  hin }
        };

        BRepBuilderAPI_MakePolygon poly;
        for (const auto& pt : pts2D)
        {
            gp_Pnt p3d = pA.Translated(dirX * pt.first + dirY * pt.second);
            poly.Add(p3d);
        }
        poly.Close();

        if (poly.IsDone())
        {
            BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
            if (faceMaker.IsDone())
            {
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                if (prism.IsDone())
                {
                    return prism.Shape();
                }
            }
        }
        break;
    }

    case TSA::Model::SectionShape::BoxHollow:
    {
        double b = std::max(0.03, section.width);
        double h = std::max(0.03, section.height);
        double tw = (section.tw > 0.0 && 2.0 * section.tw < b) ? section.tw : 0.01;
        double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : 0.01;

        double b2 = b / 2.0;
        double h2 = h / 2.0;
        double ib2 = b2 - tw;
        double ih2 = h2 - tf;

        BRepBuilderAPI_MakePolygon polyExt;
        polyExt.Add(pA.Translated(-dirX * b2 - dirY * h2));
        polyExt.Add(pA.Translated( dirX * b2 - dirY * h2));
        polyExt.Add(pA.Translated( dirX * b2 + dirY * h2));
        polyExt.Add(pA.Translated(-dirX * b2 + dirY * h2));
        polyExt.Close();

        if (polyExt.IsDone() && ib2 > 0.005 && ih2 > 0.005)
        {
            BRepBuilderAPI_MakePolygon polyInt;
            polyInt.Add(pA.Translated(-dirX * ib2 + dirY * ih2));
            polyInt.Add(pA.Translated( dirX * ib2 + dirY * ih2));
            polyInt.Add(pA.Translated( dirX * ib2 - dirY * ih2));
            polyInt.Add(pA.Translated(-dirX * ib2 - dirY * ih2));
            polyInt.Close();

            if (polyInt.IsDone())
            {
                BRepBuilderAPI_MakeFace faceMaker(polyExt.Wire());
                faceMaker.Add(polyInt.Wire());
                if (faceMaker.IsDone())
                {
                    BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                    if (prism.IsDone()) return prism.Shape();
                }
            }
        }
        break;
    }

    case TSA::Model::SectionShape::UPN:
    {
        double b = std::max(0.02, section.width);
        double h = std::max(0.04, section.height);
        double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.10);
        double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : std::max(0.007, h * 0.10);

        double b2 = b / 2.0;
        double h2 = h / 2.0;

        std::vector<std::pair<double, double>> pts2D = {
            { -b2, -h2 },
            {  b2, -h2 },
            {  b2, -h2 + tf },
            { -b2 + tw, -h2 + tf },
            { -b2 + tw,  h2 - tf },
            {  b2,  h2 - tf },
            {  b2,  h2 },
            { -b2,  h2 }
        };

        BRepBuilderAPI_MakePolygon poly;
        for (const auto& pt : pts2D)
        {
            poly.Add(pA.Translated(dirX * pt.first + dirY * pt.second));
        }
        poly.Close();

        if (poly.IsDone())
        {
            BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
            if (faceMaker.IsDone())
            {
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                if (prism.IsDone()) return prism.Shape();
            }
        }
        break;
    }

    case TSA::Model::SectionShape::Angle:
    {
        double b = std::max(0.02, section.width);
        double h = std::max(0.02, section.height);
        double t = (section.tw > 0.0 && section.tw < std::min(b, h)) ? section.tw : std::max(0.005, std::min(b, h) * 0.12);

        double b2 = b / 2.0;
        double h2 = h / 2.0;

        std::vector<std::pair<double, double>> pts2D = {
            { -b2, -h2 },
            {  b2, -h2 },
            {  b2, -h2 + t },
            { -b2 + t, -h2 + t },
            { -b2 + t,  h2 },
            { -b2,  h2 }
        };

        BRepBuilderAPI_MakePolygon poly;
        for (const auto& pt : pts2D)
        {
            poly.Add(pA.Translated(dirX * pt.first + dirY * pt.second));
        }
        poly.Close();

        if (poly.IsDone())
        {
            BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
            if (faceMaker.IsDone())
            {
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                if (prism.IsDone()) return prism.Shape();
            }
        }
        break;
    }

    case TSA::Model::SectionShape::TSection:
    {
        double b = std::max(0.02, section.width);
        double h = std::max(0.02, section.height);
        double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.10);
        double tf = (section.tf > 0.0 && section.tf < h) ? section.tf : std::max(0.007, h * 0.12);

        double b2 = b / 2.0;
        double h2 = h / 2.0;
        double tw2 = tw / 2.0;

        std::vector<std::pair<double, double>> pts2D = {
            { -tw2, -h2 },
            {  tw2, -h2 },
            {  tw2,  h2 - tf },
            {   b2,  h2 - tf },
            {   b2,  h2 },
            {  -b2,  h2 },
            {  -b2,  h2 - tf },
            { -tw2,  h2 - tf }
        };

        BRepBuilderAPI_MakePolygon poly;
        for (const auto& pt : pts2D)
        {
            poly.Add(pA.Translated(dirX * pt.first + dirY * pt.second));
        }
        poly.Close();

        if (poly.IsDone())
        {
            BRepBuilderAPI_MakeFace faceMaker(poly.Wire());
            if (faceMaker.IsDone())
            {
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                if (prism.IsDone()) return prism.Shape();
            }
        }
        break;
    }

    case TSA::Model::SectionShape::Rectangular:
    default:
    {
        double halfW = std::max(0.005, section.width) / 2.0;
        double halfH = std::max(0.005, section.height) / 2.0;

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

        gp_Pln pln(pA, gp_Dir(dirZ));
        BRepBuilderAPI_MakeFace faceMaker(pln, poly.Wire());
        if (!faceMaker.IsDone())
        {
            faceMaker = BRepBuilderAPI_MakeFace(poly.Wire());
        }
        if (!faceMaker.IsDone())
        {
            return TopoDS_Shape();
        }

        BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
        if (!prism.IsDone())
        {
            return TopoDS_Shape();
        }

        return prism.Shape();
    }
    }

    return TopoDS_Shape();
}

TopoDS_Shape BeamGeometry::createBeamShape(
    const TSA::Model::Node& startNode,
    const TSA::Model::Node& endNode,
    double width,
    double height,
    double rotationDegrees)
{
    return createBeamShape(startNode, endNode, TSA::Model::Section::rectangular(width, height), rotationDegrees);
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
