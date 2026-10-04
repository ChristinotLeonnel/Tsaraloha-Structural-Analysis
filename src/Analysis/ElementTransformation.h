#pragma once

// Transformations d'éléments filaires 3D (6 DDL par nœud) reproduisant exactement
// OpenSees 3.8.0 LinearCrdTransf3d (sans excentricités nodales) :
//   - axes locaux : x = (j - i)/L ; y = (vecxz × x)/|vecxz × x| ; z = x × y
//   - u_local = R · u_global par triplet (R : lignes = x, y, z exprimés dans le repère global)
//   - repère « basique » de ElasticBeam3d : [N, Mz_i, Mz_j, My_i, My_j, T]
//       ub0 = ul6 - ul0
//       ub1 = ul5  + (ul1 - ul7)/L      ub2 = ul11 + (ul1 - ul7)/L
//       ub3 = ul4  + (ul8 - ul2)/L      ub4 = ul10 + (ul8 - ul2)/L
//       ub5 = ul9 - ul3
//   - k_local = T_blᵀ · k_basic · T_bl ; K_global = Rᵀ · k_local · R (R bloc-diagonale 12×12)
// Ces relations ne valent que pour la transformation Linear (ou PDelta/Corotational à
// l'état initial non chargé) : l'appelant porte la responsabilité de l'étiquetage.
// Ordre des 12 DDL d'un élément : [UX UY UZ RX RY RZ]_i puis [UX UY UZ RX RY RZ]_j.

#include "AnalysisTypes.h"

#include <array>

namespace TSA::Analysis
{

using Vec3 = std::array<double, 3>;
using Vec12 = std::array<double, 12>;

struct LocalAxes
{
    Vec3 x { 1.0, 0.0, 0.0 };
    Vec3 y { 0.0, 1.0, 0.0 };
    Vec3 z { 0.0, 0.0, 1.0 };
    double length = 0.0;
    bool valid = false;
};

namespace ElementTransformation
{
/// Axes locaux tels qu'OpenSees les calcule à partir de geomTransf … $vecxz.
LocalAxes openSeesAxes(const Vec3& nodeI, const Vec3& nodeJ, const Vec3& vecxz);

/// u_local = T · u_global (12 composantes).
Vec12 globalToLocal(const LocalAxes& axes, const Vec12& global);
/// u_global = Tᵀ · u_local (T orthogonale).
Vec12 localToGlobal(const LocalAxes& axes, const Vec12& local);

/// Déformations basiques ElasticBeam3d depuis les déplacements locaux.
std::array<double, 6> beamLocalToBasic(const Vec12& local, double length);

/// k_local (12×12) = T_blᵀ · k_basic (6×6) · T_bl  (ElasticBeam3d).
DenseMatrix beamBasicToLocalStiffness(const DenseMatrix& kBasic, double length);
/// k_local (12×12) d'une barre de treillis : k_basic (1×1) sur les DDL axiaux ux_i, ux_j.
DenseMatrix trussBasicToLocalStiffness(double kBasic);
/// K_global = Tᵀ · k_local · T.
DenseMatrix localToGlobalStiffness(const LocalAxes& axes, const DenseMatrix& kLocal);
/// Matrice de transformation 12×12 T (u_local = T · u_global).
DenseMatrix transformationMatrix(const LocalAxes& axes);
} // namespace ElementTransformation

} // namespace TSA::Analysis
