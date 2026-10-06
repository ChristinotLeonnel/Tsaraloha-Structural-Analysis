#pragma once

// Moteur d'accrochage 3D (OSNAP) : choisit LE point d'accrochage sous le curseur, en espace écran.
//
//   curseur (pixels) + rayon de visée
//        ↓ offer*() : candidats (nœuds, extrémités, milieux, centres, intersections, perpendiculaire,
//                     proche, face, grille)
//        ↓ projection écran → distance au curseur (pixels, indépendante du zoom)
//        ↓ classement : points discrets > suivis (axe, face) ; distance écran + biais de priorité ;
//                       égalité → le plus proche de l'observateur
//   GridSnapResult (position EXACTE utilisée par l'aperçu ET par le clic)
//
// Indépendant de la vue OCCT : la projection est fournie par l'appelant (OccView) ou par un test.
// Aucune allocation par candidat ; une passe linéaire sur le modèle par mouvement de souris.

#include "GridType.h"

#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <functional>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}

namespace TSA::Grid
{

class GridSystem;

struct SnapQuery
{
    double cursorX = 0.0;               ///< position du curseur (pixels de la fenêtre de rendu)
    double cursorY = 0.0;
    gp_Pnt rayOrigin;                   ///< rayon de visée passant par le curseur
    gp_Dir rayDir { 0.0, 0.0, -1.0 };
    double radiusPx = 14.0;             ///< ouverture d'accrochage (pixels)
    SnapMode modes = SnapMode::All;     ///< types autorisés (réglages utilisateur)
    bool objects = true;                ///< accrochage aux éléments du modèle (F3)
    bool grids = true;                  ///< accrochage aux grilles (S)
    bool hasReference = false;          ///< point de référence (perpendiculaire), ex. point précédent
    gp_Pnt reference;

    /// Projection monde → écran ; false si le point est derrière l'observateur.
    std::function<bool(const gp_Pnt&, double& sx, double& sy)> project;
    /// Filtre de visibilité d'un élément (ElementKind, id) ; vide = tout est accrochable.
    std::function<bool(int kind, int id)> acceptElement;
    /// Filtre d'un point (ex. hors du plan en mode 2D) ; vide = tout point accepté.
    std::function<bool(const gp_Pnt&)> acceptPoint;
};

class SnapEngine
{
public:
    explicit SnapEngine(const SnapQuery& query);

    /// Candidats du modèle : nœuds, extrémités / milieux / proche des barres et voiles, centres
    /// de dalles, faces, intersections entre éléments, perpendiculaire au point de référence.
    void collectModel(const TSA::Model::Model& model);
    /// Candidats des grilles visibles : intersections, origines, axes, rayons, arcs.
    void collectGrids(const std::vector<const GridSystem*>& grids);

    /// Point discret candidat (évalué immédiatement, seul le meilleur est conservé).
    void offerPoint(GridSnapType type, SnapSource source, const gp_Pnt& p, int kind, int id, const std::string& desc);
    /// Suivi d'un segment 3D : point du segment le plus proche du rayon de visée.
    void offerSegment(GridSnapType type, SnapSource source, const gp_Pnt& a, const gp_Pnt& b, int kind, int id,
                      const std::string& desc);

    /// Meilleur accrochage (snapped = false si aucun candidat dans l'ouverture).
    GridSnapResult result() const;

    /// Biais de priorité (pixels) : plus faible = plus prioritaire à distance écran égale.
    static double priorityBias(GridSnapType type, SnapSource source);
    /// Libellé court du type (« Nœud », « Milieu »…), affiché à côté du marqueur.
    static const char* typeLabel(GridSnapType type, SnapSource source);
    /// Libellé du marqueur : type + élément, sans coordonnées (« Milieu · Poutre B3 », « Nœud N7 »).
    static std::string displayLabel(const GridSnapResult& snap);
    /// Point d'accrochage discret (true) ou suivi continu le long d'une courbe / face (false).
    static bool isDiscrete(GridSnapType type);

private:
    struct Segment
    {
        gp_Pnt a, b;
        int kind, id;
    };

    bool screenDistance(const gp_Pnt& p, double& dist) const;
    void consider(GridSnapResult& best, double& bestScore, GridSnapType type, SnapSource source, const gp_Pnt& p,
                  double dist, int kind, int id, const std::string& desc) const;
    void collectIntersections();
    bool closestOnRay(const gp_Pnt& a, const gp_Pnt& b, gp_Pnt& out) const;

    const SnapQuery& m_q;
    double m_radius;
    GridSnapResult m_bestPoint;
    GridSnapResult m_bestTrack;
    double m_bestPointScore = 1e18;
    double m_bestTrackScore = 1e18;
    std::vector<Segment> m_nearSegments;   ///< segments du modèle proches du curseur (intersections)
};

} // namespace TSA::Grid
