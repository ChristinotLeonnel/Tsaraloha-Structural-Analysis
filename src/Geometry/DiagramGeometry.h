#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <vector>
#include <QString>

#include "../Analysis/ResultsModel.h"

namespace TSA::Geometry
{

enum class DiagramType
{
    None,
    AxialForceN,    ///< N (Effort Normal)
    ShearForceVy,   ///< Vy (Effort Tranchant selon Y local)
    ShearForceVz,   ///< Vz (Effort Tranchant selon Z local)
    TorsionMx,      ///< Mx (Moment de Torsion)
    BendingMy,      ///< My (Moment Fléchissant autour de Y local)
    BendingMz       ///< Mz (Moment Fléchissant autour de Z local)
};

/**
 * @brief Constructeur géométrique 3D pour les diagrammes d'efforts internes le long des éléments structuraux.
 * Construit des surfaces polygonales décalées et hachurées représentant N, V, M directement dans la vue 3D.
 */
class DiagramGeometry
{
public:
    /**
     * @brief Retourne le nom lisible du diagramme (ex. "Moment Fléchissant Mz").
     */
    static QString diagramTypeName(DiagramType type);

    /**
     * @brief Retourne l'unité physique associée (ex. "kNm", "kN", "N").
     */
    static QString diagramUnit(DiagramType type, bool useKiloNewtons = true);

    /**
     * @brief Extrait la valeur scalaire correspondante d'une station d'effort.
     */
    static double getStationValue(const TSA::Analysis::StationForces& st, DiagramType type);

    /**
     * @brief Détermine la direction locale de projection du diagramme.
     */
    static gp_Vec getDiagramOffsetDirection(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        double rotationDeg,
        DiagramType type
    );

    /**
     * @brief Construit la forme 3D (surfaces et hachures) d'un diagramme le long d'un élément.
     */
    static TopoDS_Shape createDiagramShape(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        double rotationDeg,
        const std::vector<TSA::Analysis::StationForces>& stations,
        DiagramType type,
        double scaleFactor,
        bool showHatching = true
    );
};

} // namespace TSA::Geometry
