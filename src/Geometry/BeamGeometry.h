#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_XY.hxx>
#include <vector>
#include "../Model/Section.h"
#include "../Model/Beam.h"

namespace TSA::Model
{
    class Node;
    class Beam;
}

namespace TSA::Geometry
{

class BeamGeometry
{
public:
    // Construit un solide 3D TopoDS_Shape selon la géométrie exacte de la section, la rotation bêta et l'excentrement
    static TopoDS_Shape createBeamShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        const TSA::Model::Section& section,
        double rotationDegrees = 0.0,
        TSA::Model::BarEccentricity eccentricity = TSA::Model::BarEccentricity::None
    );

    // Surcharge de compatibilité pour sections rectangulaires simples
    static TopoDS_Shape createBeamShape(
        const TSA::Model::Node& startNode,
        const TSA::Model::Node& endNode,
        double width,
        double height,
        double rotationDegrees = 0.0
    );

    /// Repère d'affichage d'une barre : x = largeur de section, y = hauteur, z = axe (A → B), rotation β
    /// incluse. valid = faux si la barre est de longueur nulle.
    struct SectionFrame
    {
        bool valid = false;
        gp_Vec x, y, z;
    };
    static SectionFrame sectionFrame(const gp_Pnt& a, const gp_Pnt& b, double rotationDegrees);

    /// Contour de la section dans le plan (x = largeur, y = hauteur), centré, sens trigonométrique.
    /// inner : contour du vide (tubes, caissons), même nombre de points que outer et en correspondance
    /// (point i de l'un face au point i de l'autre). smooth : section ronde (normales radiales).
    struct SectionOutline
    {
        std::vector<gp_XY> outer;
        std::vector<gp_XY> inner;
        bool smooth = false;
    };
    static SectionOutline sectionOutline(const TSA::Model::Section& section);

    // Construit une sphère 3D représentant un nœud structural
    static TopoDS_Shape createNodeShape(
        const TSA::Model::Node& node,
        double radius = 0.08
    );
};

} // namespace TSA::Geometry
