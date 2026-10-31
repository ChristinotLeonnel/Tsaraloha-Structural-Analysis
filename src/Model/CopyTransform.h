#pragma once

// Image des données d'un élément par une transformation rigide (copie par rotation, copie miroir) : une charge copiée
// suit la structure, sa direction est celle de l'image de la charge d'origine. Une translation ne change rien : ces
// fonctions ne servent pas pour elle. Fonctions pures, sans modèle ; utilisées par StructuralClipboard.

#include "../Mesh/MeshGeometry.h"
#include "Load/MemberLoad.h"
#include "Load/NodalLoad.h"
#include "Load/SurfaceLoad.h"
#include "SurfaceSupport.h"

#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
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

NodalLoad nodalLoadImage(const NodalLoad& load, const gp_Trsf& trsf);

/// Image d'une charge sur barre : une charge par composante non nulle dans le repère de la charge (global, ou repère local
/// de la barre copiée). Un axe qui reste aligné sur un axe du repère donne une seule charge. La gravité reste la gravité.
/// source et copy : repères locaux des barres (computeElementLocalFrame).
std::vector<MemberLoad> memberLoadImage(const MemberLoad& load, const gp_Trsf& trsf, const gp_Ax3& source, const gp_Ax3& copy);

/// Passage de (u, v) dans le repère de la surface source à (u', v') dans celui de sa copie.
struct SurfacePointMap
{
    double a = 0.0, b = 0.0;    ///< translation
    double uu = 1.0, uv = 0.0, vu = 0.0, vv = 1.0;   ///< u' = a + uu·u + uv·v ; v' = b + vu·u + vv·v

    std::pair<double, double> operator()(double u, double v) const { return { a + uu * u + uv * v, b + vu * u + vv * v }; }
    bool operator==(const SurfacePointMap&) const = default;
};
SurfacePointMap surfacePointMap(const TSA::Mesh::PlaneFrame& source, const TSA::Mesh::PlaneFrame& copy, const gp_Trsf& trsf);

/// Image d'une charge surfacique : position, zone et gradient convertis par map ; une direction globale qui n'est plus alignée
/// sur un axe donne une charge par composante. normalReversed : la normale de la copie est opposée à l'image de la normale.
std::vector<SurfaceLoad> surfaceLoadImage(const SurfaceLoad& load, const gp_Trsf& trsf, const SurfacePointMap& map, const gp_Dir& sourceNormal,
                                          bool normalReversed);

/// Contour d'une dalle copiée en miroir : parcours inversé (la normale reste l'image de la normale), en commençant par
/// l'image du 2e sommet pour que le repère local de la copie reste aligné sur l'image de celui de la source.
std::vector<int> mirroredContour(const std::vector<int>& nodeIds);
/// Bord de ce contour correspondant au bord `edge` (i : du sommet i au sommet i + 1 ; -1 : tout le contour).
int mirroredEdge(int edge, int nodeCount);

} // namespace TSA::Model
