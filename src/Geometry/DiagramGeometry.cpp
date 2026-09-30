#include "DiagramGeometry.h"
#include "../Coordinate/CoordinateTransformationService.h"

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <cmath>

namespace TSA::Geometry
{

QString DiagramGeometry::diagramTypeName(DiagramType type)
{
    switch (type)
    {
    case DiagramType::AxialForceN:  return QStringLiteral("Effort Normal (N)");
    case DiagramType::ShearForceVy: return QStringLiteral("Effort Tranchant (Vy)");
    case DiagramType::ShearForceVz: return QStringLiteral("Effort Tranchant (Vz)");
    case DiagramType::TorsionMx:    return QStringLiteral("Moment de Torsion (Mx)");
    case DiagramType::BendingMy:    return QStringLiteral("Moment Fléchissant (My)");
    case DiagramType::BendingMz:    return QStringLiteral("Moment Fléchissant (Mz)");
    case DiagramType::None:
    default:                        return QStringLiteral("Aucun");
    }
}

QString DiagramGeometry::diagramUnit(DiagramType type, bool useKiloNewtons)
{
    switch (type)
    {
    case DiagramType::AxialForceN:
    case DiagramType::ShearForceVy:
    case DiagramType::ShearForceVz:
        return useKiloNewtons ? QStringLiteral("kN") : QStringLiteral("N");
    case DiagramType::TorsionMx:
    case DiagramType::BendingMy:
    case DiagramType::BendingMz:
        return useKiloNewtons ? QStringLiteral("kNm") : QStringLiteral("Nm");
    case DiagramType::None:
    default:
        return QString();
    }
}

double DiagramGeometry::getStationValue(const TSA::Analysis::StationForces& st, DiagramType type)
{
    switch (type)
    {
    case DiagramType::AxialForceN:  return st.N;
    case DiagramType::ShearForceVy: return st.Vy;
    case DiagramType::ShearForceVz: return st.Vz;
    case DiagramType::TorsionMx:    return st.Mx;
    case DiagramType::BendingMy:    return st.My;
    case DiagramType::BendingMz:    return st.Mz;
    case DiagramType::None:
    default:                        return 0.0;
    }
}

gp_Vec DiagramGeometry::getDiagramOffsetDirection(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    double rotationDeg,
    DiagramType type)
{
    gp_Ax3 frame = TSA::Coordinate::CoordinateTransformationService::computeElementLocalFrame(p1, p2, rotationDeg);

    // frame.Direction() = Z local
    // frame.YDirection() = Y local
    switch (type)
    {
    case DiagramType::BendingMz:
    case DiagramType::ShearForceVy:
        // Diagramme tracé dans la direction transversale Y local
        return gp_Vec(frame.YDirection());

    case DiagramType::BendingMy:
    case DiagramType::ShearForceVz:
    case DiagramType::AxialForceN:
    case DiagramType::TorsionMx:
    default:
        // Diagramme tracé dans la direction normale Z local
        return gp_Vec(frame.Direction());
    }
}

TopoDS_Shape DiagramGeometry::createDiagramShape(
    const gp_Pnt& p1,
    const gp_Pnt& p2,
    double rotationDeg,
    const std::vector<TSA::Analysis::StationForces>& stations,
    DiagramType type,
    double scaleFactor,
    bool showHatching)
{
    if (type == DiagramType::None || stations.size() < 2)
    {
        return TopoDS_Shape();
    }

    gp_Vec vAB(p1, p2);
    double length = vAB.Magnitude();
    if (length < 1e-5) return TopoDS_Shape();

    gp_Vec offsetDir = getDiagramOffsetDirection(p1, p2, rotationDeg, type);
    if (offsetDir.Magnitude() < 1e-6) return TopoDS_Shape();
    offsetDir.Normalize();

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    size_t nStations = stations.size();
    std::vector<gp_Pnt> basePoints(nStations);
    std::vector<gp_Pnt> diagPoints(nStations);
    std::vector<double> vals(nStations);

    for (size_t i = 0; i < nStations; ++i)
    {
        double pos = 0.0;
        if (length > 1e-6)
        {
            pos = stations[i].position / length;
        }
        if (pos < 0.0) pos = 0.0;
        if (pos > 1.0) pos = 1.0;

        gp_Pnt base = p1.Translated(vAB * pos);
        basePoints[i] = base;

        double v = getStationValue(stations[i], type);
        vals[i] = v;

        gp_Pnt dPt = base.Translated(offsetDir * (v * scaleFactor));
        diagPoints[i] = dPt;

        // Ligne de hachure à chaque station
        if (showHatching && std::abs(v * scaleFactor) > 1e-4)
        {
            try
            {
                BRepBuilderAPI_MakeEdge hatchEdge(base, dPt);
                if (hatchEdge.IsDone())
                {
                    bb.Add(comp, hatchEdge.Shape());
                }
            }
            catch (...) {}
        }
    }

    // Construction des faces de surface le long du diagramme
    for (size_t i = 0; i < nStations - 1; ++i)
    {
        const gp_Pnt& b1 = basePoints[i];
        const gp_Pnt& b2 = basePoints[i + 1];
        const gp_Pnt& d1 = diagPoints[i];
        const gp_Pnt& d2 = diagPoints[i + 1];
        double v1 = vals[i];
        double v2 = vals[i + 1];

        // Ligne extérieure de contour
        try
        {
            BRepBuilderAPI_MakeEdge contourEdge(d1, d2);
            if (contourEdge.IsDone())
            {
                bb.Add(comp, contourEdge.Shape());
            }
        }
        catch (...) {}

        // Détection de croisement par zéro
        if ((v1 > 1e-5 && v2 < -1e-5) || (v1 < -1e-5 && v2 > 1e-5))
        {
            double r = -v1 / (v2 - v1);
            gp_Pnt zPt = b1.Translated((b2.XYZ() - b1.XYZ()) * r);

            // Triangle 1 : b1 -> d1 -> zPt
            try
            {
                BRepBuilderAPI_MakePolygon poly1(b1, d1, zPt, true);
                if (poly1.IsDone())
                {
                    BRepBuilderAPI_MakeFace face1(poly1.Wire());
                    if (face1.IsDone()) bb.Add(comp, face1.Shape());
                }
            }
            catch (...) {}

            // Triangle 2 : zPt -> d2 -> b2
            try
            {
                BRepBuilderAPI_MakePolygon poly2(zPt, d2, b2, true);
                if (poly2.IsDone())
                {
                    BRepBuilderAPI_MakeFace face2(poly2.Wire());
                    if (face2.IsDone()) bb.Add(comp, face2.Shape());
                }
            }
            catch (...) {}
        }
        else
        {
            // Quadrilatère : b1 -> d1 -> d2 -> b2
            try
            {
                BRepBuilderAPI_MakePolygon poly(b1, d1, d2, b2, true);
                if (poly.IsDone())
                {
                    BRepBuilderAPI_MakeFace face(poly.Wire());
                    if (face.IsDone()) bb.Add(comp, face.Shape());
                }
            }
            catch (...) {}
        }
    }

    return comp;
}

} // namespace TSA::Geometry
