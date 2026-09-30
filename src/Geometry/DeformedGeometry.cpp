#include "DeformedGeometry.h"
#include "BeamGeometry.h"
#include "../Model/Node.h"

#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>

namespace TSA::Geometry
{

gp_Pnt DeformedGeometry::computeDeformedPoint(
    const gp_Pnt& orig,
    const TSA::Analysis::NodeDisplacement& disp,
    double scaleFactor)
{
    gp_Vec d(disp.ux * scaleFactor, disp.uy * scaleFactor, disp.uz * scaleFactor);
    return orig.Translated(d);
}

TopoDS_Shape DeformedGeometry::createDeformedNodeSphere(
    const gp_Pnt& orig,
    const TSA::Analysis::NodeDisplacement& disp,
    double scaleFactor,
    double radius)
{
    gp_Pnt p = computeDeformedPoint(orig, disp, scaleFactor);
    try
    {
        return BRepPrimAPI_MakeSphere(p, radius).Shape();
    }
    catch (...)
    {
        return TopoDS_Shape();
    }
}

TopoDS_Shape DeformedGeometry::createDeformedCenterline(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    const TSA::Analysis::NodeDisplacement& d1,
    const TSA::Analysis::NodeDisplacement& d2,
    double scaleFactor,
    int numSegments)
{
    if (numSegments < 2) numSegments = 2;

    gp_Pnt p1Def = computeDeformedPoint(p1, d1, scaleFactor);
    gp_Pnt p2Def = computeDeformedPoint(p2, d2, scaleFactor);

    gp_Vec vOrig(p1, p2);
    double L = vOrig.Magnitude();
    if (L < 1e-6) return TopoDS_Shape();

    // Tangentes initiales
    gp_Vec t1 = vOrig;
    gp_Vec t2 = vOrig;

    // Prise en compte des rotations nodales amplifiées sur les tangentes
    // theta x/y/z créent une rotation de la direction
    gp_Vec rot1(d1.rx * scaleFactor, d1.ry * scaleFactor, d1.rz * scaleFactor);
    gp_Vec rot2(d2.rx * scaleFactor, d2.ry * scaleFactor, d2.rz * scaleFactor);

    // Variation des tangentes via produit vectoriel d'angle infinitésimal rot ^ t
    t1 += rot1.Crossed(t1);
    t2 += rot2.Crossed(t2);

    BRepBuilderAPI_MakePolygon poly;

    for (int i = 0; i <= numSegments; ++i)
    {
        double s = static_cast<double>(i) / numSegments;
        double s2 = s * s;
        double s3 = s2 * s;

        // Polynômes d'Hermite cubique
        double h1 = 2.0 * s3 - 3.0 * s2 + 1.0;
        double h2 = -2.0 * s3 + 3.0 * s2;
        double h3 = s3 - 2.0 * s2 + s;
        double h4 = s3 - s2;

        gp_XYZ ptCoord = p1Def.XYZ() * h1 + p2Def.XYZ() * h2 +
                         t1.XYZ() * h3 + t2.XYZ() * h4;

        poly.Add(gp_Pnt(ptCoord));
    }

    try
    {
        if (poly.IsDone())
        {
            return poly.Wire();
        }
    }
    catch (...)
    {
    }

    // Fallback : segment linéaire simple
    BRepBuilderAPI_MakeEdge edge(p1Def, p2Def);
    return edge.Shape();
}

TopoDS_Shape DeformedGeometry::createDeformedBeamShape(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    const TSA::Analysis::NodeDisplacement& d1,
    const TSA::Analysis::NodeDisplacement& d2,
    const TSA::Model::Section& section,
    double scaleFactor,
    double rotationDeg)
{
    gp_Pnt p1Def = computeDeformedPoint(p1, d1, scaleFactor);
    gp_Pnt p2Def = computeDeformedPoint(p2, d2, scaleFactor);

    if (p1Def.Distance(p2Def) < 1e-4)
    {
        return TopoDS_Shape();
    }

    TSA::Model::Node nA(1, p1Def.X(), p1Def.Y(), p1Def.Z());
    TSA::Model::Node nB(2, p2Def.X(), p2Def.Y(), p2Def.Z());

    return BeamGeometry::createBeamShape(nA, nB, section, rotationDeg);
}

TopoDS_Shape DeformedGeometry::createModalDeformedBeamShape(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    const TSA::Analysis::NodeDisplacement& phi1,
    const TSA::Analysis::NodeDisplacement& phi2,
    const TSA::Model::Section& section,
    double modalScale,
    double phaseRad,
    double rotationDeg)
{
    double c = std::cos(phaseRad);

    TSA::Analysis::NodeDisplacement d1;
    d1.ux = phi1.ux * c;
    d1.uy = phi1.uy * c;
    d1.uz = phi1.uz * c;
    d1.rx = phi1.rx * c;
    d1.ry = phi1.ry * c;
    d1.rz = phi1.rz * c;

    TSA::Analysis::NodeDisplacement d2;
    d2.ux = phi2.ux * c;
    d2.uy = phi2.uy * c;
    d2.uz = phi2.uz * c;
    d2.rx = phi2.rx * c;
    d2.ry = phi2.ry * c;
    d2.rz = phi2.rz * c;

    return createDeformedBeamShape(p1, p2, d1, d2, section, modalScale, rotationDeg);
}

} // namespace TSA::Geometry
