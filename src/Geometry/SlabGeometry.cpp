#include "SlabGeometry.h"
#include "../Model/Node.h"

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <cmath>

namespace TSA::Geometry
{

TopoDS_Shape SlabGeometry::createSlabShape(
    const std::vector<const TSA::Model::Node*>& nodes,
    double thickness
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

    // 4. Extruder la face pour former le solide 3D de la dalle
    BRepPrimAPI_MakePrism makePrism(makeFace.Face(), extrusionVec);
    if (!makePrism.IsDone())
    {
        return makeFace.Face(); // En repli, renvoyer la face surfacique
    }

    return makePrism.Shape();
}

} // namespace TSA::Geometry
