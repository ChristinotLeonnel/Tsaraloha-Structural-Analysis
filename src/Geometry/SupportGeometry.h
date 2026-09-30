#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <string>

namespace TSA::Model
{
class Node;
class SupportDefinition;
}

namespace TSA::Geometry
{

/**
 * @brief Générateur de représentations géométriques 3D B-Rep pour les appuis structuraux.
 * Conforme aux conventions visuelles du génie civil (Encastrements, Articulations,
 * Rouleaux, Glissières, Ressorts de translation et de rotation, Appuis plans).
 */
class SupportGeometry
{
public:
    /**
     * @brief Crée la forme 3D complète de l'appui d'un nœud selon sa SupportDefinition.
     * @param node Nœud portant l'appui.
     * @param scale Échelle visuelle (en mètres).
     * @param preferredNormal Normale préférée pour l'orientation (ex: verticale Z ou locale).
     */
    static TopoDS_Shape createSupportShape(
        const TSA::Model::Node& node,
        double scale = 0.35,
        const gp_Dir& preferredNormal = gp_Dir(0, 0, 1));

    /**
     * @brief Crée la forme 3D à partir d'une définition d'appui et d'un point 3D.
     */
    static TopoDS_Shape createSupportShape(
        const TSA::Model::SupportDefinition& support,
        const gp_Pnt& location,
        double scale = 0.35,
        const gp_Dir& preferredNormal = gp_Dir(0, 0, 1));

    // --- Formes spécialisées ---

    /// Encastrement rigide (Plaque d'assise ancrée + hachures de sol)
    static TopoDS_Shape createFixedSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& normal = gp_Dir(0, 0, 1),
        double scale = 0.35);

    /// Articulation / Rotule (Pyramide pivotante + plaque de base + sol)
    static TopoDS_Shape createPinnedSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& normal = gp_Dir(0, 0, 1),
        double scale = 0.35);

    /// Appui simple / Rouleau (Pyramide + cylindres de roulement + plaque de base)
    static TopoDS_Shape createRollerSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& normal = gp_Dir(0, 0, 1),
        const gp_Dir& rollerAxis = gp_Dir(1, 0, 0),
        double scale = 0.35);

    /// Appui glissant / Coulisseau
    static TopoDS_Shape createSlidingSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& normal = gp_Dir(0, 0, 1),
        const gp_Dir& slideDir = gp_Dir(1, 0, 0),
        double scale = 0.35);

    /// Appui linéaire guidé
    static TopoDS_Shape createLinearSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& guideAxis = gp_Dir(1, 0, 0),
        double scale = 0.35);

    /// Appui plan (glissement multi-directionnel dans un plan)
    static TopoDS_Shape createPlanarSupportShape(
        const gp_Pnt& pt,
        const gp_Dir& planeNormal = gp_Dir(0, 0, 1),
        double scale = 0.35);

    /// Ressort hélicoïdal 3D pour DDL élastique en translation
    static TopoDS_Shape createSpringShape(
        const gp_Pnt& pt,
        const gp_Dir& dir,
        double scale = 0.35,
        int coils = 4);

    /// Ressort spiral / arc de cercle pour DDL élastique en rotation
    static TopoDS_Shape createRotationalSpringShape(
        const gp_Pnt& pt,
        const gp_Dir& axis,
        double scale = 0.35);

private:
    static void buildOrthonormalBasis(
        const gp_Dir& normal,
        gp_Vec& outU,
        gp_Vec& outV);
};

} // namespace TSA::Geometry
