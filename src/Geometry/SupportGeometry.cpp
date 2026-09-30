#include "SupportGeometry.h"
#include "../Model/Node.h"
#include "../Model/SupportDefinition.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
inline gp_Pnt operator+(const gp_Pnt& p, const gp_Vec& v)
{
    return p.Translated(v);
}

inline gp_Pnt operator-(const gp_Pnt& p, const gp_Vec& v)
{
    return p.Translated(-v);
}
} // namespace

namespace TSA::Geometry
{

void SupportGeometry::buildOrthonormalBasis(
    const gp_Dir& normal,
    gp_Vec& outU,
    gp_Vec& outV)
{
    gp_Vec n(normal);
    gp_Vec ref = (std::abs(normal.Z()) < 0.85) ? gp_Vec(0, 0, 1) : gp_Vec(0, 1, 0);
    outU = n.Crossed(ref);
    if (outU.Magnitude() < 1e-6)
    {
        ref = gp_Vec(1, 0, 0);
        outU = n.Crossed(ref);
    }
    outU.Normalize();
    outV = n.Crossed(outU).Normalized();
}

TopoDS_Shape SupportGeometry::createSupportShape(
    const TSA::Model::Node& node,
    double scale,
    const gp_Dir& preferredNormal)
{
    const auto& supp = node.support();
    if (supp.isFree())
    {
        return TopoDS_Shape();
    }

    gp_Pnt pt(node.x(), node.y(), node.z());
    gp_Dir normal = preferredNormal;

    if (supp.orientationType() == TSA::Model::SupportOrientationType::CustomVector)
    {
        gp_Vec cv(supp.customDirX(), supp.customDirY(), supp.customDirZ());
        if (cv.Magnitude() > 1e-4)
        {
            normal = gp_Dir(cv);
        }
    }

    return createSupportShape(supp, pt, scale, normal);
}

TopoDS_Shape SupportGeometry::createSupportShape(
    const TSA::Model::SupportDefinition& supp,
    const gp_Pnt& pt,
    double scale,
    const gp_Dir& preferredNormal)
{
    if (supp.isFree())
    {
        return TopoDS_Shape();
    }

    if (supp.isFixed())
    {
        return createFixedSupportShape(pt, preferredNormal, scale);
    }

    if (supp.isPinned())
    {
        return createPinnedSupportShape(pt, preferredNormal, scale);
    }

    if (supp.isRoller())
    {
        gp_Vec u, v;
        buildOrthonormalBasis(preferredNormal, u, v);
        return createRollerSupportShape(pt, preferredNormal, gp_Dir(u), scale);
    }

    // Gestion des ressorts ou des configurations mixtes/personnalisées
    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    bool hasParts = false;

    // 1. Ressorts de translation
    if (supp.tx() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createSpringShape(pt, gp_Dir(1, 0, 0), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }
    if (supp.ty() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createSpringShape(pt, gp_Dir(0, 1, 0), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }
    if (supp.tz() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createSpringShape(pt, gp_Dir(0, 0, 1), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }

    // 2. Ressorts de rotation
    if (supp.rx() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createRotationalSpringShape(pt, gp_Dir(1, 0, 0), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }
    if (supp.ry() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createRotationalSpringShape(pt, gp_Dir(0, 1, 0), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }
    if (supp.rz() == TSA::Model::DOFState::Spring)
    {
        TopoDS_Shape s = createRotationalSpringShape(pt, gp_Dir(0, 0, 1), scale);
        if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
    }

    // 3. Traductions fixes
    int fixedTranslCount = (supp.tx() == TSA::Model::DOFState::Fixed ? 1 : 0) +
                           (supp.ty() == TSA::Model::DOFState::Fixed ? 1 : 0) +
                           (supp.tz() == TSA::Model::DOFState::Fixed ? 1 : 0);

    if (fixedTranslCount == 3)
    {
        int fixedRotCount = (supp.rx() == TSA::Model::DOFState::Fixed ? 1 : 0) +
                            (supp.ry() == TSA::Model::DOFState::Fixed ? 1 : 0) +
                            (supp.rz() == TSA::Model::DOFState::Fixed ? 1 : 0);
        if (fixedRotCount >= 2)
        {
            TopoDS_Shape s = createFixedSupportShape(pt, preferredNormal, scale);
            if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
        }
        else
        {
            TopoDS_Shape s = createPinnedSupportShape(pt, preferredNormal, scale);
            if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
        }
    }
    else if (fixedTranslCount > 0)
    {
        // 1 ou 2 translations fixes (ex: appui glissant ou rouleau)
        if (supp.tz() == TSA::Model::DOFState::Fixed && supp.tx() == TSA::Model::DOFState::Free)
        {
            gp_Vec u, v;
            buildOrthonormalBasis(preferredNormal, u, v);
            TopoDS_Shape s = createRollerSupportShape(pt, preferredNormal, gp_Dir(u), scale);
            if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
        }
        else
        {
            TopoDS_Shape s = createPinnedSupportShape(pt, preferredNormal, scale * 0.8);
            if (!s.IsNull()) { bb.Add(comp, s); hasParts = true; }
        }
    }

    if (hasParts)
    {
        return comp;
    }

    // Par défaut si non vide mais non identifié
    return createPinnedSupportShape(pt, preferredNormal, scale);
}

TopoDS_Shape SupportGeometry::createFixedSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& normal,
    double scale)
{
    gp_Vec u, v;
    buildOrthonormalBasis(normal, u, v);
    gp_Vec n(normal);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double plateW = scale * 0.9;
    double plateD = scale * 0.9;
    double plateH = scale * 0.14;

    // Plaque de base sous le nœud
    gp_Pnt p0 = pt + u * (-plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt p1 = pt + u * ( plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt p2 = pt + u * ( plateW * 0.5) + v * ( plateD * 0.5);
    gp_Pnt p3 = pt + u * (-plateW * 0.5) + v * ( plateD * 0.5);

    BRepBuilderAPI_MakePolygon poly(p0, p1, p2, p3, true);
    if (poly.IsDone())
    {
        BRepBuilderAPI_MakeFace makeFace(poly.Wire());
        if (makeFace.IsDone())
        {
            BRepPrimAPI_MakePrism prism(makeFace.Face(), -n * plateH);
            if (prism.IsDone())
            {
                bb.Add(comp, prism.Shape());
            }
        }
    }

    // Hachures de fondation / ancrage au sol (symbolique d'encastrement)
    gp_Pnt plateBottom = pt - n * plateH;
    int numHatches = 5;
    double step = plateW / (numHatches + 1);
    double hatchLen = scale * 0.28;
    gp_Vec hatchDir = (-n * 0.7 + u * 0.4 + v * 0.4).Normalized();

    for (int i = 1; i <= numHatches; ++i)
    {
        for (int j = 1; j <= numHatches; ++j)
        {
            if (i == 1 || i == numHatches || j == 1 || j == numHatches || (i + j) % 2 == 0)
            {
                gp_Pnt hStart = plateBottom + u * (-plateW * 0.5 + i * step) + v * (-plateD * 0.5 + j * step);
                gp_Pnt hEnd = hStart + hatchDir * hatchLen;
                BRepBuilderAPI_MakeEdge edge(hStart, hEnd);
                if (edge.IsDone())
                {
                    bb.Add(comp, edge.Edge());
                }
            }
        }
    }

    return comp;
}

TopoDS_Shape SupportGeometry::createPinnedSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& normal,
    double scale)
{
    gp_Vec u, v;
    buildOrthonormalBasis(normal, u, v);
    gp_Vec n(normal);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double pyramidH = scale * 0.65;
    double baseW = scale * 0.55;
    double baseD = scale * 0.55;

    gp_Pnt baseCenter = pt - n * pyramidH;
    gp_Pnt b0 = baseCenter + u * (-baseW * 0.5) + v * (-baseD * 0.5);
    gp_Pnt b1 = baseCenter + u * ( baseW * 0.5) + v * (-baseD * 0.5);
    gp_Pnt b2 = baseCenter + u * ( baseW * 0.5) + v * ( baseD * 0.5);
    gp_Pnt b3 = baseCenter + u * (-baseW * 0.5) + v * ( baseD * 0.5);

    // 4 faces latérales de la pyramide (pointe sur le nœud 'pt')
    auto addTri = [&](const gp_Pnt& pA, const gp_Pnt& pB, const gp_Pnt& pC) {
        BRepBuilderAPI_MakePolygon tri(pA, pB, pC, true);
        if (tri.IsDone())
        {
            BRepBuilderAPI_MakeFace f(tri.Wire());
            if (f.IsDone()) bb.Add(comp, f.Face());
        }
    };

    addTri(pt, b0, b1);
    addTri(pt, b1, b2);
    addTri(pt, b2, b3);
    addTri(pt, b3, b0);

    // Face de base de la pyramide
    BRepBuilderAPI_MakePolygon basePoly(b0, b1, b2, b3, true);
    if (basePoly.IsDone())
    {
        BRepBuilderAPI_MakeFace f(basePoly.Wire());
        if (f.IsDone()) bb.Add(comp, f.Face());
    }

    // Plaque d'appui sous la pyramide
    double plateW = scale * 0.75;
    double plateD = scale * 0.75;
    double plateH = scale * 0.10;

    gp_Pnt pb0 = baseCenter + u * (-plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt pb1 = baseCenter + u * ( plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt pb2 = baseCenter + u * ( plateW * 0.5) + v * ( plateD * 0.5);
    gp_Pnt pb3 = baseCenter + u * (-plateW * 0.5) + v * ( plateD * 0.5);

    BRepBuilderAPI_MakePolygon platePoly(pb0, pb1, pb2, pb3, true);
    if (platePoly.IsDone())
    {
        BRepBuilderAPI_MakeFace pf(platePoly.Wire());
        if (pf.IsDone())
        {
            BRepPrimAPI_MakePrism prism(pf.Face(), -n * plateH);
            if (prism.IsDone()) bb.Add(comp, prism.Shape());
        }
    }

    // Hachures inclinées de sol
    gp_Pnt ground = baseCenter - n * plateH;
    int numLines = 6;
    double step = plateW / (numLines + 1);
    double hLen = scale * 0.22;
    gp_Vec hVec = (-n * 0.8 + u * 0.5).Normalized();

    for (int i = 1; i <= numLines; ++i)
    {
        gp_Pnt s1 = ground + u * (-plateW * 0.5 + i * step) + v * (-plateD * 0.4);
        gp_Pnt e1 = s1 + hVec * hLen;
        BRepBuilderAPI_MakeEdge ed1(s1, e1);
        if (ed1.IsDone()) bb.Add(comp, ed1.Edge());

        gp_Pnt s2 = ground + u * (-plateW * 0.5 + i * step) + v * (plateD * 0.4);
        gp_Pnt e2 = s2 + hVec * hLen;
        BRepBuilderAPI_MakeEdge ed2(s2, e2);
        if (ed2.IsDone()) bb.Add(comp, ed2.Edge());
    }

    return comp;
}

TopoDS_Shape SupportGeometry::createRollerSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& normal,
    const gp_Dir& rollerAxis,
    double scale)
{
    gp_Vec n(normal);
    gp_Vec u(rollerAxis);
    gp_Vec v = n.Crossed(u).Normalized();

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double pyramidH = scale * 0.50;
    double baseW = scale * 0.50;
    double baseD = scale * 0.50;

    gp_Pnt baseCenter = pt - n * pyramidH;
    gp_Pnt b0 = baseCenter + u * (-baseW * 0.5) + v * (-baseD * 0.5);
    gp_Pnt b1 = baseCenter + u * ( baseW * 0.5) + v * (-baseD * 0.5);
    gp_Pnt b2 = baseCenter + u * ( baseW * 0.5) + v * ( baseD * 0.5);
    gp_Pnt b3 = baseCenter + u * (-baseW * 0.5) + v * ( baseD * 0.5);

    auto addTri = [&](const gp_Pnt& pA, const gp_Pnt& pB, const gp_Pnt& pC) {
        BRepBuilderAPI_MakePolygon tri(pA, pB, pC, true);
        if (tri.IsDone())
        {
            BRepBuilderAPI_MakeFace f(tri.Wire());
            if (f.IsDone()) bb.Add(comp, f.Face());
        }
    };

    addTri(pt, b0, b1);
    addTri(pt, b1, b2);
    addTri(pt, b2, b3);
    addTri(pt, b3, b0);

    BRepBuilderAPI_MakePolygon basePoly(b0, b1, b2, b3, true);
    if (basePoly.IsDone())
    {
        BRepBuilderAPI_MakeFace f(basePoly.Wire());
        if (f.IsDone()) bb.Add(comp, f.Face());
    }

    // Cylindres de roulement (2 rouleaux sous la pyramide)
    double rollerRadius = scale * 0.06;
    double rollerLength = baseW * 0.9;
    double gapBetweenRollers = baseD * 0.35;

    for (int s : {-1, 1})
    {
        gp_Pnt rCenter = baseCenter - n * rollerRadius + v * (s * gapBetweenRollers * 0.5) - u * (rollerLength * 0.5);
        gp_Ax2 cylAx(rCenter, gp_Dir(u));
        BRepPrimAPI_MakeCylinder cyl(cylAx, rollerRadius, rollerLength);
        if (cyl.IsDone())
        {
            bb.Add(comp, cyl.Shape());
        }
    }

    // Plaque de base sous les rouleaux
    double plateH = scale * 0.08;
    double plateW = scale * 0.70;
    double plateD = scale * 0.70;
    gp_Pnt plateTopCenter = baseCenter - n * (rollerRadius * 2.0);

    gp_Pnt pb0 = plateTopCenter + u * (-plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt pb1 = plateTopCenter + u * ( plateW * 0.5) + v * (-plateD * 0.5);
    gp_Pnt pb2 = plateTopCenter + u * ( plateW * 0.5) + v * ( plateD * 0.5);
    gp_Pnt pb3 = plateTopCenter + u * (-plateW * 0.5) + v * ( plateD * 0.5);

    BRepBuilderAPI_MakePolygon platePoly(pb0, pb1, pb2, pb3, true);
    if (platePoly.IsDone())
    {
        BRepBuilderAPI_MakeFace pf(platePoly.Wire());
        if (pf.IsDone())
        {
            BRepPrimAPI_MakePrism prism(pf.Face(), -n * plateH);
            if (prism.IsDone()) bb.Add(comp, prism.Shape());
        }
    }

    // Hachures de sol
    gp_Pnt ground = plateTopCenter - n * plateH;
    int numLines = 5;
    double step = plateW / (numLines + 1);
    double hLen = scale * 0.20;
    gp_Vec hVec = (-n * 0.8 + v * 0.5).Normalized();

    for (int i = 1; i <= numLines; ++i)
    {
        gp_Pnt s1 = ground + u * (-plateW * 0.5 + i * step);
        gp_Pnt e1 = s1 + hVec * hLen;
        BRepBuilderAPI_MakeEdge ed1(s1, e1);
        if (ed1.IsDone()) bb.Add(comp, ed1.Edge());
    }

    return comp;
}

TopoDS_Shape SupportGeometry::createSlidingSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& normal,
    const gp_Dir& slideDir,
    double scale)
{
    gp_Vec n(normal);
    gp_Vec s(slideDir);
    gp_Vec t = n.Crossed(s).Normalized();

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double blockL = scale * 0.8;
    double blockW = scale * 0.4;
    double blockH = scale * 0.2;

    gp_Pnt p0 = pt + s * (-blockL * 0.5) + t * (-blockW * 0.5);
    gp_Pnt p1 = pt + s * ( blockL * 0.5) + t * (-blockW * 0.5);
    gp_Pnt p2 = pt + s * ( blockL * 0.5) + t * ( blockW * 0.5);
    gp_Pnt p3 = pt + s * (-blockL * 0.5) + t * ( blockW * 0.5);

    BRepBuilderAPI_MakePolygon poly(p0, p1, p2, p3, true);
    if (poly.IsDone())
    {
        BRepBuilderAPI_MakeFace f(poly.Wire());
        if (f.IsDone())
        {
            BRepPrimAPI_MakePrism prism(f.Face(), -n * blockH);
            if (prism.IsDone()) bb.Add(comp, prism.Shape());
        }
    }

    // Double flèche indiquant la direction de translation libre
    for (int dirSign : {-1, 1})
    {
        gp_Pnt tip = pt + s * (dirSign * blockL * 0.6);
        gp_Pnt base = pt + s * (dirSign * blockL * 0.4);
        BRepBuilderAPI_MakeEdge edge(base, tip);
        if (edge.IsDone()) bb.Add(comp, edge.Edge());

        gp_Pnt arrowW1 = tip - s * (dirSign * scale * 0.08) + t * (scale * 0.06);
        gp_Pnt arrowW2 = tip - s * (dirSign * scale * 0.08) - t * (scale * 0.06);
        BRepBuilderAPI_MakeEdge eA(tip, arrowW1);
        BRepBuilderAPI_MakeEdge eB(tip, arrowW2);
        if (eA.IsDone()) bb.Add(comp, eA.Edge());
        if (eB.IsDone()) bb.Add(comp, eB.Edge());
    }

    return comp;
}

TopoDS_Shape SupportGeometry::createLinearSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& guideAxis,
    double scale)
{
    gp_Vec g(guideAxis);
    gp_Vec u, v;
    buildOrthonormalBasis(guideAxis, u, v);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double sleeveR = scale * 0.15;
    double sleeveL = scale * 0.7;

    gp_Pnt startPnt = pt - g * (sleeveL * 0.5);
    gp_Ax2 cylAx(startPnt, guideAxis);
    BRepPrimAPI_MakeCylinder cyl(cylAx, sleeveR, sleeveL);
    if (cyl.IsDone()) bb.Add(comp, cyl.Shape());

    return comp;
}

TopoDS_Shape SupportGeometry::createPlanarSupportShape(
    const gp_Pnt& pt,
    const gp_Dir& planeNormal,
    double scale)
{
    gp_Vec u, v;
    buildOrthonormalBasis(planeNormal, u, v);
    gp_Vec n(planeNormal);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    double plateR = scale * 0.5;
    gp_Ax2 circAx(pt - n * (scale * 0.05), planeNormal);
    BRepPrimAPI_MakeCylinder plate(circAx, plateR, scale * 0.1);
    if (plate.IsDone()) bb.Add(comp, plate.Shape());

    return comp;
}

TopoDS_Shape SupportGeometry::createSpringShape(
    const gp_Pnt& pt,
    const gp_Dir& dir,
    double scale,
    int coils)
{
    gp_Vec d(dir);
    gp_Vec u, v;
    buildOrthonormalBasis(dir, u, v);

    double totalLength = scale * 0.8;
    double radius = scale * 0.18;
    int numPoints = 12 * coils;

    if (numPoints < 12) numPoints = 12;

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    // Discrétisation de l'hélice 3D
    std::vector<gp_Pnt> pts;
    pts.reserve(numPoints + 1);

    for (int i = 0; i <= numPoints; ++i)
    {
        double frac = static_cast<double>(i) / numPoints;
        double angle = frac * (2.0 * M_PI * coils);
        double dist = frac * totalLength;

        // Le ressort part du nœud et s'étend dans la direction opposée à 'dir'
        gp_Pnt p = pt - d * dist + u * (radius * std::cos(angle)) + v * (radius * std::sin(angle));
        pts.push_back(p);
    }

    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        BRepBuilderAPI_MakeEdge edge(pts[i], pts[i + 1]);
        if (edge.IsDone())
        {
            bb.Add(comp, edge.Edge());
        }
    }

    // Petite plaque de butée à l'extrémité du ressort
    gp_Pnt endPnt = pt - d * totalLength;
    double padSize = radius * 1.4;
    gp_Pnt q0 = endPnt + u * (-padSize * 0.5) + v * (-padSize * 0.5);
    gp_Pnt q1 = endPnt + u * ( padSize * 0.5) + v * (-padSize * 0.5);
    gp_Pnt q2 = endPnt + u * ( padSize * 0.5) + v * ( padSize * 0.5);
    gp_Pnt q3 = endPnt + u * (-padSize * 0.5) + v * ( padSize * 0.5);

    BRepBuilderAPI_MakePolygon poly(q0, q1, q2, q3, true);
    if (poly.IsDone())
    {
        BRepBuilderAPI_MakeFace f(poly.Wire());
        if (f.IsDone()) bb.Add(comp, f.Face());
    }

    return comp;
}

TopoDS_Shape SupportGeometry::createRotationalSpringShape(
    const gp_Pnt& pt,
    const gp_Dir& axis,
    double scale)
{
    gp_Vec a(axis);
    gp_Vec u, v;
    buildOrthonormalBasis(axis, u, v);

    double radius = scale * 0.35;
    double sweepAngle = 1.6 * M_PI; // ~290 degrés
    int numPoints = 24;

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    std::vector<gp_Pnt> pts;
    pts.reserve(numPoints + 1);

    for (int i = 0; i <= numPoints; ++i)
    {
        double frac = static_cast<double>(i) / numPoints;
        double angle = frac * sweepAngle;
        double r = radius * (0.6 + 0.4 * frac); // Légère spirale d'Archimède

        gp_Pnt p = pt + u * (r * std::cos(angle)) + v * (r * std::sin(angle));
        pts.push_back(p);
    }

    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        BRepBuilderAPI_MakeEdge edge(pts[i], pts[i + 1]);
        if (edge.IsDone())
        {
            bb.Add(comp, edge.Edge());
        }
    }

    // Flèche à l'extrémité de l'arc
    if (!pts.empty())
    {
        const gp_Pnt& tip = pts.back();
        double angleTip = sweepAngle;
        gp_Vec tangent = (-u * std::sin(angleTip) + v * std::cos(angleTip)).Normalized();
        gp_Vec normalRadial = (u * std::cos(angleTip) + v * std::sin(angleTip)).Normalized();

        gp_Pnt a1 = tip - tangent * (scale * 0.1) + normalRadial * (scale * 0.06);
        gp_Pnt a2 = tip - tangent * (scale * 0.1) - normalRadial * (scale * 0.06);

        BRepBuilderAPI_MakeEdge e1(tip, a1);
        BRepBuilderAPI_MakeEdge e2(tip, a2);
        if (e1.IsDone()) bb.Add(comp, e1.Edge());
        if (e2.IsDone()) bb.Add(comp, e2.Edge());
    }

    return comp;
}

} // namespace TSA::Geometry
