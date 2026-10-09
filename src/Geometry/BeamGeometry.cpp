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

namespace
{
double circularRadius(const TSA::Model::Section& section)
{
    return std::max(0.001, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);
}

void pipeRadii(const TSA::Model::Section& section, double& ro, double& ri)
{
    ro = std::max(0.005, (section.diameter > 0.0 ? section.diameter : section.width) / 2.0);
    const double tw = (section.tw > 0.0 && section.tw < ro) ? section.tw : std::min(0.01, ro * 0.2);
    ri = ro - tw;
}

std::vector<gp_XY> circlePoints(double radius, int n)
{
    std::vector<gp_XY> pts;
    for (int i = 0; i < n; ++i)
    {
        const double a = 2.0 * M_PI * i / n;
        pts.emplace_back(radius * std::cos(a), radius * std::sin(a));
    }
    return pts;
}

/// Prisme d'un contour plan (outer + vide éventuel) dans le repère (x, y) de la section en pA.
TopoDS_Shape prismFromOutline(const BeamGeometry::SectionOutline& outline, const gp_Pnt& pA, const gp_Vec& x,
                              const gp_Vec& y, const gp_Vec& vAB)
{
    if (outline.outer.size() < 3) return TopoDS_Shape();
    try
    {
        BRepBuilderAPI_MakePolygon outer;
        for (const auto& p : outline.outer) outer.Add(pA.Translated(x * p.X() + y * p.Y()));
        outer.Close();
        if (!outer.IsDone()) return TopoDS_Shape();
        BRepBuilderAPI_MakeFace face(outer.Wire());
        if (face.IsDone() && outline.inner.size() >= 3)
        {
            BRepBuilderAPI_MakePolygon inner; // parcouru en sens inverse : vide de la section
            for (auto it = outline.inner.rbegin(); it != outline.inner.rend(); ++it)
                inner.Add(pA.Translated(x * it->X() + y * it->Y()));
            inner.Close();
            if (inner.IsDone()) face.Add(inner.Wire());
        }
        if (!face.IsDone()) return TopoDS_Shape();
        BRepPrimAPI_MakePrism prism(face.Face(), vAB);
        if (prism.IsDone()) return prism.Shape();
    }
    catch (...)
    {
    }
    return TopoDS_Shape();
}
} // namespace

BeamGeometry::SectionFrame BeamGeometry::sectionFrame(const gp_Pnt& a, const gp_Pnt& b, double rotationDegrees)
{
    SectionFrame f;
    const gp_Vec vAB(a, b);
    const double length = vAB.Magnitude();
    if (length < 1e-4) return f;
    const gp_Vec dirZ = vAB / length; // Direction longitudinale

    // Repère local de référence (dirX0, dirY0)
    const gp_Vec globalZ(0.0, 0.0, 1.0);
    gp_Vec dirX0;
    gp_Vec dirY0;
    if (std::abs(dirZ.Dot(globalZ)) > 0.999)
    {
        // Quasi-verticale (poteau vertical selon Z)
        const gp_Vec refX(1.0, 0.0, 0.0);
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

    // Angle de rotation bêta dans le plan de la section
    const double rad = rotationDegrees * (M_PI / 180.0);
    f.x = dirX0 * std::cos(rad) + dirY0 * std::sin(rad);
    f.y = -dirX0 * std::sin(rad) + dirY0 * std::cos(rad);
    f.z = dirZ;
    f.valid = true;
    return f;
}

BeamGeometry::SectionOutline BeamGeometry::sectionOutline(const TSA::Model::Section& section)
{
    SectionOutline o;
    auto set = [&o](std::initializer_list<std::pair<double, double>> pts) {
        for (const auto& [u, v] : pts) o.outer.emplace_back(u, v);
    };

    switch (section.shape)
    {
    case TSA::Model::SectionShape::Circular:
        o.outer = circlePoints(circularRadius(section), 48);
        o.smooth = true;
        break;

    case TSA::Model::SectionShape::Pipe:
    {
        double ro = 0.0, ri = 0.0;
        pipeRadii(section, ro, ri);
        o.outer = circlePoints(ro, 48);
        if (ri > 0.001) o.inner = circlePoints(ri, 48);
        o.smooth = true;
        break;
    }

    case TSA::Model::SectionShape::IShape:
    {
        const double b = std::max(0.03, section.width);
        const double h = std::max(0.05, section.height);
        const double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.08);
        const double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : std::max(0.008, h * 0.10);
        const double b2 = b / 2.0, h2 = h / 2.0, w2 = tw / 2.0, hin = h2 - tf;
        // 12 sommets du profil en I (x = largeur, y = hauteur)
        set({ { b2, h2 }, { -b2, h2 }, { -b2, hin }, { -w2, hin }, { -w2, -hin }, { -b2, -hin },
              { -b2, -h2 }, { b2, -h2 }, { b2, -hin }, { w2, -hin }, { w2, hin }, { b2, hin } });
        break;
    }

    case TSA::Model::SectionShape::BoxHollow:
    {
        const double b = std::max(0.03, section.width);
        const double h = std::max(0.03, section.height);
        const double tw = (section.tw > 0.0 && 2.0 * section.tw < b) ? section.tw : 0.01;
        const double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : 0.01;
        const double b2 = b / 2.0, h2 = h / 2.0, ib2 = b2 - tw, ih2 = h2 - tf;
        set({ { -b2, -h2 }, { b2, -h2 }, { b2, h2 }, { -b2, h2 } });
        if (ib2 > 0.005 && ih2 > 0.005)
            o.inner = { gp_XY(-ib2, -ih2), gp_XY(ib2, -ih2), gp_XY(ib2, ih2), gp_XY(-ib2, ih2) };
        break;
    }

    case TSA::Model::SectionShape::UPN:
    {
        const double b = std::max(0.02, section.width);
        const double h = std::max(0.04, section.height);
        const double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.10);
        const double tf = (section.tf > 0.0 && 2.0 * section.tf < h) ? section.tf : std::max(0.007, h * 0.10);
        const double b2 = b / 2.0, h2 = h / 2.0;
        set({ { -b2, -h2 }, { b2, -h2 }, { b2, -h2 + tf }, { -b2 + tw, -h2 + tf },
              { -b2 + tw, h2 - tf }, { b2, h2 - tf }, { b2, h2 }, { -b2, h2 } });
        break;
    }

    case TSA::Model::SectionShape::Angle:
    {
        const double b = std::max(0.02, section.width);
        const double h = std::max(0.02, section.height);
        const double t = (section.tw > 0.0 && section.tw < std::min(b, h)) ? section.tw : std::max(0.005, std::min(b, h) * 0.12);
        const double b2 = b / 2.0, h2 = h / 2.0;
        set({ { -b2, -h2 }, { b2, -h2 }, { b2, -h2 + t }, { -b2 + t, -h2 + t }, { -b2 + t, h2 }, { -b2, h2 } });
        break;
    }

    case TSA::Model::SectionShape::TSection:
    {
        const double b = std::max(0.02, section.width);
        const double h = std::max(0.02, section.height);
        const double tw = (section.tw > 0.0 && section.tw < b) ? section.tw : std::max(0.005, b * 0.10);
        const double tf = (section.tf > 0.0 && section.tf < h) ? section.tf : std::max(0.007, h * 0.12);
        const double b2 = b / 2.0, h2 = h / 2.0, tw2 = tw / 2.0;
        set({ { -tw2, -h2 }, { tw2, -h2 }, { tw2, h2 - tf }, { b2, h2 - tf },
              { b2, h2 }, { -b2, h2 }, { -b2, h2 - tf }, { -tw2, h2 - tf } });
        break;
    }

    case TSA::Model::SectionShape::Rectangular:
    default:
    {
        const double halfW = std::max(0.005, section.width) / 2.0;
        const double halfH = std::max(0.005, section.height) / 2.0;
        set({ { -halfW, -halfH }, { halfW, -halfH }, { halfW, halfH }, { -halfW, halfH } });
        break;
    }
    }

    // Sens trigonométrique (aire signée > 0) ; les deux contours sont inversés ensemble (correspondance).
    double area = 0.0;
    for (std::size_t i = 0; i < o.outer.size(); ++i)
    {
        const gp_XY& p = o.outer[i];
        const gp_XY& q = o.outer[(i + 1) % o.outer.size()];
        area += p.X() * q.Y() - q.X() * p.Y();
    }
    if (area < 0.0)
    {
        std::reverse(o.outer.begin(), o.outer.end());
        std::reverse(o.inner.begin(), o.inner.end());
    }
    return o;
}

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
    // 1-2. Repère de la section (largeur x, hauteur y, axe z) avec la rotation bêta
    const SectionFrame frame = sectionFrame(pA, pB, rotationDegrees);
    if (!frame.valid)
    {
        return TopoDS_Shape();
    }
    const gp_Vec dirZ = frame.z;
    const gp_Vec dirX = frame.x;
    const gp_Vec dirY = frame.y;

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
        double radius = circularRadius(section);

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
            cyl.Build(); // construction paresseuse en OCCT : IsDone() est faux avant Build()
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
        double ro = 0.0, ri = 0.0;
        pipeRadii(section, ro, ri);

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
            cyl.Build(); // construction paresseuse en OCCT : IsDone() est faux avant Build()
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

    default:
        // Profils polygonaux (I, caisson, U, cornière, T, rectangle) : contour partagé avec la déformée.
        return prismFromOutline(sectionOutline(section), pA, dirX, dirY, vAB);
    }
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
    // La construction des primitives OCCT est paresseuse (Build() est appelé par Shape()) :
    // tester IsDone() sans Build() préalable renvoyait toujours faux -> forme nulle, et aucun
    // nœud n'était jamais affiché dans le viewport.
    sphere.Build();
    if (!sphere.IsDone())
    {
        return TopoDS_Shape();
    }
    return sphere.Shape();
}

} // namespace TSA::Geometry
