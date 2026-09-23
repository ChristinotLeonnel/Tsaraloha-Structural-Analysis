#include "BeamGeometry.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Section.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
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
    double rotationDegrees)
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

    // 3. Construction selon la forme réelle de la section
    switch (section.shape)
    {
    case TSA::Model::SectionShape::Circular:
    {
        double radius = std::max(0.01, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);
        gp_Ax2 axes(pA, gp_Dir(dirZ), gp_Dir(dirX));
        BRepPrimAPI_MakeCylinder cyl(axes, radius, length);
        if (cyl.IsDone())
        {
            return cyl.Shape();
        }
        break;
    }

    case TSA::Model::SectionShape::Pipe:
    {
        double ro = std::max(0.015, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);
        double tw = (section.tw > 0.0 && section.tw < ro) ? section.tw : 0.01;
        double ri = ro - tw;

        // Discrétisation polygonale à double contour (contour extérieur + intérieur inversé)
        const int nSeg = 24;
        BRepBuilderAPI_MakePolygon polyExt;
        for (int i = 0; i < nSeg; ++i)
        {
            double angle = 2.0 * M_PI * i / nSeg;
            polyExt.Add(pA.Translated(dirX * (ro * std::cos(angle)) + dirY * (ro * std::sin(angle))));
        }
        polyExt.Close();

        if (polyExt.IsDone())
        {
            if (ri > 0.005)
            {
                BRepBuilderAPI_MakePolygon polyInt;
                for (int i = nSeg - 1; i >= 0; --i) // Inversion pour trou
                {
                    double angle = 2.0 * M_PI * i / nSeg;
                    polyInt.Add(pA.Translated(dirX * (ri * std::cos(angle)) + dirY * (ri * std::sin(angle))));
                }
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

            // Fallback plein si l'intérieur échoue
            BRepBuilderAPI_MakeFace faceMaker(polyExt.Wire());
            if (faceMaker.IsDone())
            {
                BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
                if (prism.IsDone()) return prism.Shape();
            }
        }
        break;
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

    case TSA::Model::SectionShape::Rectangular:
    default:
        break;
    }

    // Forme Rectangulaire standard (par défaut ou si la primitive spéciale a échoué)
    double halfW = std::max(0.01, section.width) / 2.0;
    double halfH = std::max(0.01, section.height) / 2.0;

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

    BRepPrimAPI_MakePrism prism(faceMaker.Face(), vAB);
    if (!prism.IsDone())
    {
        return TopoDS_Shape();
    }

    return prism.Shape();
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
