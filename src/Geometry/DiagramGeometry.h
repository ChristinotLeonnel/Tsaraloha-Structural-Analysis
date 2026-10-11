#pragma once

#include <cmath>

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
    AxialForceN,    ///< N / Nx (Effort Normal)
    ShearForceVy,   ///< Vy / Qy (Effort Tranchant selon Y local)
    ShearForceVz,   ///< Vz / Qz (Effort Tranchant selon Z local)
    TorsionMx,      ///< Mx (Moment de Torsion)
    BendingMy,      ///< My (Moment Fléchissant autour de Y local)
    BendingMz,      ///< Mz (Moment Fléchissant autour de Z local)
    DeflectionUx,   ///< UX (Déplacement axial le long de l'élément)
    DeflectionUy,   ///< UY (Flèche transversale selon Y local)
    DeflectionUz,   ///< UZ (Flèche verticale selon Z local)
    DeflectionUres, ///< U résultant (Norme du déplacement)
    RotationRx,     ///< RX (Rotation de torsion rad)
    RotationRy,     ///< RY (Rotation de flexion Y rad)
    RotationRz,     ///< RZ (Rotation de flexion Z rad)
    StressSigma     ///< σ de fibre extrême (MPa) = N/A ± |My|/Wy ± |Mz|/Wz — théorie des poutres, traction > 0
};

/**
 * @brief Constructeur géométrique 3D pour les diagrammes d'efforts internes le long des éléments structuraux.
 * Construit des surfaces polygonales décalées et hachurées représentant N, V, M directement dans la vue 3D.
 */
class DiagramGeometry
{
public:
    /// Contrainte normale de fibre extrême (MPa, traction > 0) d'une station : σ = N/A ± (|My|/Wy + |Mz|/Wz),
    /// signe de l'extrême le plus grand en valeur absolue. Efforts en kN et kN·m (kiloNewtons) ou N et N·m.
    /// Sans module de flexion (câble, treillis), seul N/A est retenu. Ne tient compte ni du cisaillement ni
    /// de la torsion (ce n'est pas une contrainte de Von Mises).
    static double normalStressMPa(const TSA::Analysis::StationForces& st, double area, double wy, double wz, bool kiloNewtons = true)
    {
        // En ligne : utilisée aussi par la note de calcul (bibliothèque du modèle, sans la partie graphique)
        if (area <= 0.0) return 0.0;
        const double k = kiloNewtons ? 1e-3 : 1e-6; // kN/m² → MPa, ou N/m² → MPa
        const double axial = st.N / area;
        const double bending = (wy > 0.0 ? std::abs(st.My) / wy : 0.0) + (wz > 0.0 ? std::abs(st.Mz) / wz : 0.0);
        const double top = axial + bending, bottom = axial - bending;
        return (std::abs(top) >= std::abs(bottom) ? top : bottom) * k;
    }

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
