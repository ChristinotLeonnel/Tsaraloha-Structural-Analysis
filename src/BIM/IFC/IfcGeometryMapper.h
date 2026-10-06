#pragma once

// Géométrie IFC des produits physiques : solides extrudés (SweptSolid) et axe (Curve3D).
// Les repères des barres reproduisent ceux du viewport (src/Geometry/BeamGeometry.cpp) pour que
// l'objet IFC coïncide avec l'objet affiché. Coordonnées modèle (m), jamais des coordonnées écran.

#include "IfcExportContext.h"
#include "IfcMapper.h"
#include "../Core/BimModel.h"

#include <array>
#include <map>
#include <vector>

namespace TSA::BIM::Ifc
{

using Vec3 = std::array<double, 3>;

class IfcGeometryMapper
{
public:
    IfcGeometryMapper(IfcExportContext& ctx, IfcMapper& mapper) : m_ctx(ctx), m_mapper(mapper) {}

    int point(const Vec3& p);
    int point2(double x, double y);
    int direction(const Vec3& d);
    int placement3(const Vec3& origin, const Vec3& axis, const Vec3& refDir);
    int localPlacement(int relativeTo, int axisPlacement);

    /// IfcProductDefinitionShape du produit (un solide par élément analytique) ; 0 si aucun solide.
    int productShape(const PhysicalElement& e);

    /// Repère local d'une barre (identique au viewport) : x = largeur, y = hauteur, z = axe.
    static void barFrame(const Vec3& a, const Vec3& b, double rotationDeg, Vec3& x, Vec3& y, Vec3& z);

private:
    int barSolid(const TSA::Model::Section& s, const Vec3& a, const Vec3& b, double rotationDeg, int eccentricity);
    int slabSolid(const std::vector<int>& nodeIds, double thickness);
    int wallSolid(const Vec3& a, const Vec3& b, double height, double thickness, double offset);
    int boxSolid(const Vec3& topCenter, double a, double b, double h);
    int cylinderSolid(const Vec3& topCenter, double d, double h);
    int extrude(int profile, int placement, const Vec3& dir, double depth);

    IfcExportContext& m_ctx;
    IfcMapper& m_mapper;
    std::map<std::string, int> m_dirs;
};

} // namespace TSA::BIM::Ifc
