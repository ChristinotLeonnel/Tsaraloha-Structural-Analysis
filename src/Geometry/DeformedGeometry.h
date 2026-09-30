#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <vector>

#include "../Model/Section.h"
#include "../Analysis/ResultsModel.h"

namespace TSA::Geometry
{

/**
 * @brief Constructeur géométrique pour la visualisation 3D de la déformée structurelle (OpenSees).
 * Génère des solides ou des lignes représentant la géométrie amplifiée après calcul ou en mode propre.
 */
class DeformedGeometry
{
public:
    /**
     * @brief Calcule la position déformée d'un nœud selon son déplacement et le facteur d'amplification.
     */
    static gp_Pnt computeDeformedPoint(
        const gp_Pnt& orig,
        const TSA::Analysis::NodeDisplacement& disp,
        double scaleFactor
    );

    /**
     * @brief Construit une sphère 3D représentant un nœud à sa position déformée.
     */
    static TopoDS_Shape createDeformedNodeSphere(
        const gp_Pnt& orig,
        const TSA::Analysis::NodeDisplacement& disp,
        double scaleFactor,
        double radius = 0.08
    );

    /**
     * @brief Construit un fil (wire) ou tube déformé représentant la fibre neutre d'une barre.
     * Utilise une interpolation cubique d'Hermite prenant en compte les déplacements et rotations aux nœuds.
     */
    static TopoDS_Shape createDeformedCenterline(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        const TSA::Analysis::NodeDisplacement& d1,
        const TSA::Analysis::NodeDisplacement& d2,
        double scaleFactor,
        int numSegments = 12
    );

    /**
     * @brief Construit un solide 3D d'une barre avec sa section réelle à sa position déformée.
     */
    static TopoDS_Shape createDeformedBeamShape(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        const TSA::Analysis::NodeDisplacement& d1,
        const TSA::Analysis::NodeDisplacement& d2,
        const TSA::Model::Section& section,
        double scaleFactor,
        double rotationDeg = 0.0
    );

    /**
     * @brief Construit une barre déformée pour un mode propre dynamique à un instant de phase donné.
     * Déplacement modal : phi * cos(phase).
     */
    static TopoDS_Shape createModalDeformedBeamShape(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        const TSA::Analysis::NodeDisplacement& phi1,
        const TSA::Analysis::NodeDisplacement& phi2,
        const TSA::Model::Section& section,
        double modalScale,
        double phaseRad,
        double rotationDeg = 0.0
    );
};

} // namespace TSA::Geometry
