#include "SlabGeometry.h"
#include "../Model/Node.h"

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <TopoDS_Wire.hxx>
#include <cmath>

namespace TSA::Geometry
{

TopoDS_Shape SlabGeometry::createSlabShape(
    const std::vector<const TSA::Model::Node*>& nodes,
    double thickness,
    const std::vector<std::vector<gp_Pnt>>& holes
)
{
    if (nodes.size() < 3)
    {
        return TopoDS_Shape();
    }

    // 1. Construire le polygone 3D
    BRepBuilderAPI_MakePolygon makePolygon;
    for (const auto* n : nodes)
    {
        if (!n)
            return TopoDS_Shape();
        makePolygon.Add(gp_Pnt(n->x(), n->y(), n->z()));
    }
    makePolygon.Close();

    if (!makePolygon.IsDone())
    {
        return TopoDS_Shape();
    }

    // 2. Calculer le vecteur normal du polygone (méthode de Newell)
    double nx = 0.0, ny = 0.0, nz = 0.0;
    size_t count = nodes.size();
    for (size_t i = 0; i < count; ++i)
    {
        const auto* cur = nodes[i];
        const auto* next = nodes[(i + 1) % count];

        nx += (cur->y() - next->y()) * (cur->z() + next->z());
        ny += (cur->z() - next->z()) * (cur->x() + next->x());
        nz += (cur->x() - next->x()) * (cur->y() + next->y());
    }

    double norm = std::sqrt(nx * nx + ny * ny + nz * nz);
    gp_Vec extrusionVec(0.0, 0.0, -thickness);
    if (norm > 1e-7)
    {
        nx /= norm;
        ny /= norm;
        nz /= norm;
        // Extrusion vers le bas relative au vecteur normal
        extrusionVec = gp_Vec(-nx * thickness, -ny * thickness, -nz * thickness);
    }

    // 3. Créer la face plane à partir du contour polygonal
    BRepBuilderAPI_MakeFace makeFace(makePolygon.Wire(), true);
    if (!makeFace.IsDone())
    {
        return TopoDS_Shape();
    }

    // 3 bis. Ouvertures (trémies) : contours intérieurs de la face, orientés en sens inverse
    for (const auto& hole : holes)
    {
        if (hole.size() < 3) continue;
        BRepBuilderAPI_MakePolygon hp;
        for (const auto& p : hole) hp.Add(p);
        hp.Close();
        if (!hp.IsDone()) continue;
        TopoDS_Wire w = hp.Wire();
        BRepBuilderAPI_MakeFace probe(w, true);
        if (probe.IsDone())
        {
            // Sens opposé à celui du contour extérieur
            gp_Vec nh(0, 0, 0);
            for (std::size_t i = 0; i < hole.size(); ++i)
            {
                const gp_Pnt& a = hole[i];
                const gp_Pnt& b = hole[(i + 1) % hole.size()];
                nh += gp_Vec((a.Y() - b.Y()) * (a.Z() + b.Z()), (a.Z() - b.Z()) * (a.X() + b.X()), (a.X() - b.X()) * (a.Y() + b.Y()));
            }
            if (nh.X() * nx + nh.Y() * ny + nh.Z() * nz > 0) w.Reverse();
        }
        makeFace.Add(w);
    }

    // 4. Extruder la face pour former le solide 3D de la dalle
    BRepPrimAPI_MakePrism makePrism(makeFace.Face(), extrusionVec);
    if (!makePrism.IsDone())
    {
        return makeFace.Face(); // En repli, renvoyer la face surfacique
    }

    return makePrism.Shape();
}

} // namespace TSA::Geometry
