#pragma once

// Ce qu'une transformation rigide change sur une copie ou un élément transformé, selon le comportement mesuré dans Autodesk Robot
// Structural Analysis 2024 (docs/audit/2026-10-31/COORDINATE_SYSTEM_AND_SECTION_ROTATION_REPORT.md).
//   - Section : rotation, le repère local suit l'image rigide (y' = image de y) ; symétrie, x' et z' sont les images de x et z, et
//     y' est opposé à l'image de y (le repère reste direct). L'angle de section est recalculé en conséquence.
//   - Charges : enregistrements recopiés à l'identique. Une charge globale garde sa direction globale ; une charge locale garde ses
//     valeurs dans le repère local de la copie. Seule la position en (u, v) d'une dalle miroir change, avec son repère.

#include "../Mesh/MeshGeometry.h"
#include "Load/MemberLoad.h"
#include "Load/SurfaceLoad.h"

#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <utility>
#include <vector>

namespace TSA::Model
{

bool isTranslation(const gp_Trsf& trsf);
/// Vrai pour une symétrie : le trièdre direct devient indirect.
bool reflects(const gp_Trsf& trsf);
/// Vrai si l'axe Z global reste l'axe Z global, dans le même sens (une verticale reste une verticale).
bool keepsVertical(const gp_Trsf& trsf);

/// Angle de section (degrés) de la barre [copyStart, copyEnd], image de [start, end] d'angle rotationDeg.
double barRotationImage(const gp_Trsf& trsf, const gp_Pnt& start, const gp_Pnt& end, double rotationDeg, const gp_Pnt& copyStart, const gp_Pnt& copyEnd);

/// Même action physique exprimée dans un autre repère local de barre (from → to) : une charge par composante non nulle.
/// Une charge globale, ou un repère inchangé, est rendue telle quelle.
std::vector<MemberLoad> memberLoadInFrame(const MemberLoad& load, const gp_Ax3& from, const gp_Ax3& to);

/// Passage de (u, v) dans le repère de la surface source à (u', v') dans celui de sa copie.
struct SurfacePointMap
{
    double a = 0.0, b = 0.0;    ///< translation
    double uu = 1.0, uv = 0.0, vu = 0.0, vv = 1.0;   ///< u' = a + uu·u + uv·v ; v' = b + vu·u + vv·v

    std::pair<double, double> operator()(double u, double v) const { return { a + uu * u + uv * v, b + vu * u + vv * v }; }
    bool operator==(const SurfacePointMap&) const = default;
};
SurfacePointMap surfacePointMap(const TSA::Mesh::PlaneFrame& source, const TSA::Mesh::PlaneFrame& copy, const gp_Trsf& trsf);

/// Charge surfacique sur la copie miroir d'une dalle : position, zone et gradient convertis, direction et valeur inchangées.
SurfaceLoad surfaceLoadAt(const SurfaceLoad& load, const SurfacePointMap& map);

/// Contour d'une dalle copiée en miroir : parcours inversé (la normale reste l'image de la normale), en commençant par
/// l'image du 2e sommet pour que le repère local de la copie reste aligné sur l'image de celui de la source.
std::vector<int> mirroredContour(const std::vector<int>& nodeIds);
/// Bord de ce contour correspondant au bord `edge` (i : du sommet i au sommet i + 1 ; -1 : tout le contour).
int mirroredEdge(int edge, int nodeCount);

} // namespace TSA::Model
