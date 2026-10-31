#pragma once

// Repères locaux des éléments sélectionnés, tels que le calcul les définit (aucun graphisme ici).
//   - Poutres et poteaux : CoordinateTransformationService::computeElementLocalFrame, la fonction qui fournit à OpenSees son
//     vecteur vecxz (OpenSeesModelMap) : x du nœud i vers le nœud j, angle de rotation de la section inclus.
//   - Treillis et câbles : axe x seul. Leur formulation est purement axiale : y et z n'y ont aucun sens mécanique.
//   - Dalles et voiles : repère de la surface (MeshGeometry : ouvertures, zones de charge, normale). x = U, y = V, z = N.
//     Les efforts de coque, eux, sont exprimés dans le repère de chaque élément du maillage (docs/SURFACES.md).
//   - Fondations et nœuds : aucun repère local défini, rien n'est renvoyé.

#include "../Model/SelectionQuery.h"

#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>

#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Viewer
{

/// Au-delà de ce nombre d'éléments sélectionnés, aucun repère local n'est affiché : la vue serait illisible.
constexpr std::size_t kMaxElementAxisFrames = 32;

struct ElementAxisFrame
{
    enum class Kind
    {
        Beam,
        Column,
        Truss,
        Cable,
        Slab,
        Wall
    };
    Kind kind = Kind::Beam;
    int id = 0;
    gp_Pnt origin;                  ///< point d'affichage : centre de l'élément
    gp_Dir x, y, z;                 ///< repère orthonormé direct
    bool transverseAxes = true;     ///< faux : seul x est défini (treillis, câbles)
    gp_Pnt start, end;              ///< barre : extrémités de l'axe ; surface : confondus avec le centre
    bool surface() const { return kind == Kind::Slab || kind == Kind::Wall; }
};

/// Repère d'un élément filaire entre deux points (rotation de section en degrés) ; false si les points sont confondus.
bool barAxisFrame(const gp_Pnt& start, const gp_Pnt& end, double rotationDeg, ElementAxisFrame* out);

/// Repères des éléments de la sélection (poutres, poteaux, treillis, câbles, dalles, voiles, dans cet ordre, par numéro).
/// Plus de kMaxElementAxisFrames éléments concernés : liste vide et *tooMany vrai.
std::vector<ElementAxisFrame> elementAxisFrames(const TSA::Model::Model& model, const TSA::Model::ElementSet& selection,
                                                bool* tooMany = nullptr);

/// Point de l'élément désigné par un rayon de visée : point de l'axe le plus proche du rayon (barre), intersection avec le
/// plan (surface). Centre de l'élément si le rayon est parallèle à la barre ou au plan.
gp_Pnt axisAnchorOnElement(const ElementAxisFrame& frame, const gp_Lin& ray);

} // namespace TSA::Viewer
