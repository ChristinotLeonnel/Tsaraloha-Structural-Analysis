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
    double sf = (std::isnan(scaleFactor) || std::isinf(scaleFactor)) ? 0.0 : scaleFactor;
    double ux = (std::isnan(disp.ux) || std::isinf(disp.ux)) ? 0.0 : disp.ux;
    double uy = (std::isnan(disp.uy) || std::isinf(disp.uy)) ? 0.0 : disp.uy;
    double uz = (std::isnan(disp.uz) || std::isinf(disp.uz)) ? 0.0 : disp.uz;
    gp_Vec d(ux * sf, uy * sf, uz * sf);
    return orig.Translated(d);
}

TopoDS_Shape DeformedGeometry::createDeformedNodeSphere(
    const gp_Pnt& orig,
    const TSA::Analysis::NodeDisplacement& disp,
    double scaleFactor,
    double radius)
{
    if (std::isnan(radius) || std::isinf(radius) || radius <= 1e-6)
    {
        radius = 0.08;
    }
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
    if (std::isnan(scaleFactor) || std::isinf(scaleFactor))
    {
        scaleFactor = 0.0;
    }
    if (numSegments < 2) numSegments = 2;
    if (numSegments > 100) numSegments = 100;

    gp_Pnt p1Def = computeDeformedPoint(p1, d1, scaleFactor);
    gp_Pnt p2Def = computeDeformedPoint(p2, d2, scaleFactor);

    gp_Vec vDef(p1Def, p2Def);
    double LDef = vDef.Magnitude();
    if (LDef < 1e-6) return TopoDS_Shape();

    // Tangentes initiales sur la corde déformée
    gp_Vec t1 = vDef;
    gp_Vec t2 = vDef;

    // Prise en compte des rotations nodales amplifiées sur les tangentes
    double rx1 = (std::isnan(d1.rx) || std::isinf(d1.rx)) ? 0.0 : d1.rx * scaleFactor;
    double ry1 = (std::isnan(d1.ry) || std::isinf(d1.ry)) ? 0.0 : d1.ry * scaleFactor;
    double rz1 = (std::isnan(d1.rz) || std::isinf(d1.rz)) ? 0.0 : d1.rz * scaleFactor;

    double rx2 = (std::isnan(d2.rx) || std::isinf(d2.rx)) ? 0.0 : d2.rx * scaleFactor;
    double ry2 = (std::isnan(d2.ry) || std::isinf(d2.ry)) ? 0.0 : d2.ry * scaleFactor;
    double rz2 = (std::isnan(d2.rz) || std::isinf(d2.rz)) ? 0.0 : d2.rz * scaleFactor;

    gp_Vec rot1(rx1, ry1, rz1);
    gp_Vec rot2(rx2, ry2, rz2);

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
    try
    {
        BRepBuilderAPI_MakeEdge edge(p1Def, p2Def);
        return edge.Shape();
    }
    catch (...)
    {
        return TopoDS_Shape();
    }
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
    if (std::isnan(scaleFactor) || std::isinf(scaleFactor))
    {
        scaleFactor = 0.0;
    }
    gp_Pnt p1Def = computeDeformedPoint(p1, d1, scaleFactor);
    gp_Pnt p2Def = computeDeformedPoint(p2, d2, scaleFactor);

    if (p1Def.Distance(p2Def) < 1e-4)
    {
        return TopoDS_Shape();
    }

    gp_Vec vDef(p1Def, p2Def);
    double LDef = vDef.Magnitude();
    if (LDef < 1e-6) return TopoDS_Shape();

    double rx1 = (std::isnan(d1.rx) || std::isinf(d1.rx)) ? 0.0 : d1.rx;
    double ry1 = (std::isnan(d1.ry) || std::isinf(d1.ry)) ? 0.0 : d1.ry;
    double rz1 = (std::isnan(d1.rz) || std::isinf(d1.rz)) ? 0.0 : d1.rz;

    double rx2 = (std::isnan(d2.rx) || std::isinf(d2.rx)) ? 0.0 : d2.rx;
    double ry2 = (std::isnan(d2.ry) || std::isinf(d2.ry)) ? 0.0 : d2.ry;
    double rz2 = (std::isnan(d2.rz) || std::isinf(d2.rz)) ? 0.0 : d2.rz;

    // Vérifie si des rotations notables sont présentes
    double rotMag1 = std::sqrt(rx1 * rx1 + ry1 * ry1 + rz1 * rz1) * scaleFactor;
    double rotMag2 = std::sqrt(rx2 * rx2 + ry2 * ry2 + rz2 * rz2) * scaleFactor;

    if (rotMag1 < 1e-5 && rotMag2 < 1e-5)
    {
        TSA::Model::Node nA(1, p1Def.X(), p1Def.Y(), p1Def.Z());
        TSA::Model::Node nB(2, p2Def.X(), p2Def.Y(), p2Def.Z());
        return BeamGeometry::createBeamShape(nA, nB, section, rotationDeg);
    }

    // Discrétisation cubique d'Hermite pour afficher la flèche/courbure
    const int numSegments = 6;
    gp_Vec t1 = vDef;
    gp_Vec t2 = vDef;
    gp_Vec rot1(rx1 * scaleFactor, ry1 * scaleFactor, rz1 * scaleFactor);
    gp_Vec rot2(rx2 * scaleFactor, ry2 * scaleFactor, rz2 * scaleFactor);
    t1 += rot1.Crossed(t1);
    t2 += rot2.Crossed(t2);

    auto evalHermite = [&](double s) -> gp_Pnt {
        double s2 = s * s;
        double s3 = s2 * s;
        double h1 = 2.0 * s3 - 3.0 * s2 + 1.0;
        double h2 = -2.0 * s3 + 3.0 * s2;
        double h3 = s3 - 2.0 * s2 + s;
        double h4 = s3 - s2;
        gp_XYZ ptCoord = p1Def.XYZ() * h1 + p2Def.XYZ() * h2 +
                         t1.XYZ() * h3 + t2.XYZ() * h4;
        return gp_Pnt(ptCoord);
    };

    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);
    int addedCount = 0;

    gp_Pnt prevPnt = p1Def;
    for (int i = 1; i <= numSegments; ++i)
    {
        double s = static_cast<double>(i) / numSegments;
        gp_Pnt curPnt = evalHermite(s);
        if (prevPnt.Distance(curPnt) > 1e-5)
        {
            TSA::Model::Node nA(i * 2 - 1, prevPnt.X(), prevPnt.Y(), prevPnt.Z());
            TSA::Model::Node nB(i * 2, curPnt.X(), curPnt.Y(), curPnt.Z());
            try
            {
                TopoDS_Shape segShape = BeamGeometry::createBeamShape(nA, nB, section, rotationDeg);
                if (!segShape.IsNull())
                {
                    builder.Add(comp, segShape);
                    addedCount++;
                }
            }
            catch (...) {}
        }
        prevPnt = curPnt;
    }

    if (addedCount > 0)
    {
        return comp;
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
    if (std::isnan(modalScale) || std::isinf(modalScale)) modalScale = 0.0;
    if (std::isnan(phaseRad) || std::isinf(phaseRad)) phaseRad = 0.0;

    double c = std::cos(phaseRad);

    auto cleanVal = [](double v) {
        return (std::isnan(v) || std::isinf(v)) ? 0.0 : v;
    };

    TSA::Analysis::NodeDisplacement d1;
    d1.ux = cleanVal(phi1.ux) * c;
    d1.uy = cleanVal(phi1.uy) * c;
    d1.uz = cleanVal(phi1.uz) * c;
    d1.rx = cleanVal(phi1.rx) * c;
    d1.ry = cleanVal(phi1.ry) * c;
    d1.rz = cleanVal(phi1.rz) * c;

    TSA::Analysis::NodeDisplacement d2;
    d2.ux = cleanVal(phi2.ux) * c;
    d2.uy = cleanVal(phi2.uy) * c;
    d2.uz = cleanVal(phi2.uz) * c;
    d2.rx = cleanVal(phi2.rx) * c;
    d2.ry = cleanVal(phi2.ry) * c;
    d2.rz = cleanVal(phi2.rz) * c;

    return createDeformedBeamShape(p1, p2, d1, d2, section, modalScale, rotationDeg);
}

} // namespace TSA::Geometry
