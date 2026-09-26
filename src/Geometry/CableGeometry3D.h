#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <vector>

namespace TSA::Model
{
    class Cable;
    class Model;
}

namespace TSA::Geometry
{

/**
 * @brief Générateur de solides 3D OpenCASCADE (B-Rep) haute précision pour câbles et ancrages.
 * Construit les géométries réelles (tubes, cylindres, balayages sur courbes B-Spline)
 * à l'échelle métrique exacte (ex: Ø15.7 mm = 0.0157 m).
 */
class CableGeometry3D
{
public:
    /**
     * @brief Construit le solide TopoDS_Shape d'un câble à partir de son modèle et de ses nœuds réels.
     * @param cable Référence vers le câble
     * @param model Modèle structural contenant les coordonnées des nœuds
     * @param includeAnchors Si true, intègre les corps d'ancrage aux extrémités
     */
    static TopoDS_Shape createCableShape(
        const TSA::Model::Cable& cable,
        const TSA::Model::Model& model,
        bool includeAnchors = true
    );

    /**
     * @brief Construit le solide d'un câble droit rectiligne entre deux points 3D.
     */
    static TopoDS_Shape createStraightCable(
        const gp_Pnt& pA,
        const gp_Pnt& pB,
        double diameterMeters
    );

    /**
     * @brief Construit le solide d'un câble courbe (parabole, caténaire, multi-points) par balayage d'une section circulaire.
     */
    static TopoDS_Shape createCurvedCable(
        const std::vector<gp_Pnt>& points,
        double diameterMeters
    );

    /**
     * @brief Construit la forme 3D d'une tête d'ancrage / trompette aux nœuds.
     */
    static TopoDS_Shape createAnchorSocket(
        const gp_Pnt& anchorPos,
        const gp_Pnt& towardPoint,
        double socketDiameter,
        double socketLength
    );
};

} // namespace TSA::Geometry
