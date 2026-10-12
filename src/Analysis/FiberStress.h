#pragma once

// Contrainte normale de fibre extrême d'une station de barre (formule de RDM pure, couche modèle). Utilisée par la
// note de calcul (bibliothèque du modèle) et par les diagrammes de la vue 3D (Geometry/DiagramGeometry, qui délègue).

#include "ResultsModel.h"

#include <cmath>

namespace TSA::Analysis
{

/// σ = N/A ± (|My|/Wy + |Mz|/Wz) en MPa, traction > 0, signe de l'extrême le plus grand en valeur absolue. Efforts
/// en kN et kN·m (kiloNewtons) ou N et N·m. Sans module de flexion (câble, treillis), seul N/A est retenu. Ne tient
/// compte ni du cisaillement ni de la torsion (ce n'est pas une contrainte de Von Mises).
inline double fiberNormalStressMPa(const StationForces& st, double area, double wy, double wz, bool kiloNewtons = true)
{
    if (area <= 0.0) return 0.0;
    const double k = kiloNewtons ? 1e-3 : 1e-6; // kN/m² → MPa, ou N/m² → MPa
    const double axial = st.N / area;
    const double bending = (wy > 0.0 ? std::abs(st.My) / wy : 0.0) + (wz > 0.0 ? std::abs(st.Mz) / wz : 0.0);
    const double top = axial + bending, bottom = axial - bending;
    return (std::abs(top) >= std::abs(bottom) ? top : bottom) * k;
}

} // namespace TSA::Analysis
