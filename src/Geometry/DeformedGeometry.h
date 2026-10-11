#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <string>
#include <vector>

#include "../Model/Section.h"
#include "../Analysis/ResultsModel.h"

namespace TSA::Geometry
{

/// Origine des points de la déformée d'une barre.
enum class DeformedAxisSource
{
    SolverStations,  ///< déplacements aux stations fournis par le moteur (exacts en élasticité linéaire)
    CubicHermite,    ///< fonctions de forme cubiques d'Euler-Bernoulli à partir des nœuds (u, θ)
    Linear           ///< barre articulée (treillis, câble) : interpolation linéaire des nœuds
};

/// Axe déformé d'une barre, NON amplifié : les déplacements restent ceux du calcul et le facteur
/// d'échelle n'est appliqué qu'à l'affichage (x_vis = x0 + s·u). Calculé une fois par résultat.
struct DeformedAxis
{
    bool valid = false;
    DeformedAxisSource source = DeformedAxisSource::Linear;
    std::string problem;                ///< raison de l'invalidité (aucune déformée inventée)
    std::vector<double> x;              ///< abscisse le long de la barre (m), de 0 à L
    std::vector<gp_Pnt> initial;        ///< position initiale x0
    std::vector<gp_Vec> displacement;   ///< déplacement global u (m)
    /// Rotation de torsion θx (rad) autour de l'axe local x (de i vers j), NON amplifiée, à chaque point.
    /// Poutres et poteaux seulement (DDL de rotation du solveur) ; vide pour les barres articulées.
    std::vector<double> twist;

    std::size_t size() const { return initial.size(); }
    gp_Pnt pointAt(std::size_t i, double scale) const { return initial[i].Translated(displacement[i] * scale); }
    bool hasTwist() const { return twist.size() == initial.size(); }
    double maxAbsTwist() const;
};

/// Repère de la section affichée en un point de l'axe déformé (amplifié) : origine, axes de la section
/// (x = largeur, y = hauteur du contour) et tangente.
struct DeformedSectionFrame
{
    gp_Pnt origin;
    gp_Vec x, y, t;
};

/**
 * @brief Géométrie de visualisation de la déformée (représentation distincte et temporaire : le
 * modèle structural n'est jamais modifié).
 */
class DeformedGeometry
{
public:
    /// Position déformée d'un nœud (valeurs non finies remplacées par 0 : marqueurs seulement).
    static gp_Pnt computeDeformedPoint(
        const gp_Pnt& orig,
        const TSA::Analysis::NodeDisplacement& disp,
        double scaleFactor
    );

    static TopoDS_Shape createDeformedNodeSphere(
        const gp_Pnt& orig,
        const TSA::Analysis::NodeDisplacement& disp,
        double scaleFactor,
        double radius = 0.08
    );

    /**
     * @brief Axe déformé d'une barre à partir des résultats réels.
     * flexural : poutre / poteau (stations du moteur si disponibles et cohérentes, sinon Hermite
     * cubique sur u et θ nodaux dans le repère local du solveur) ; sinon interpolation linéaire.
     * Invalide (problem renseigné) si un déplacement nodal manque ou n'est pas fini.
     */
    static DeformedAxis computeMemberAxis(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        double betaAngleDeg,
        const TSA::Analysis::NodeDisplacement* d1,
        const TSA::Analysis::NodeDisplacement* d2,
        const TSA::Analysis::ElementResults* elementResults,
        bool flexural,
        int hermiteSegments = 16
    );

    /// Polyligne de l'axe déformé amplifié (référence de contrôle, barres articulées).
    static TopoDS_Shape createAxisWire(const DeformedAxis& axis, double scale);

    /**
     * @brief Repères de la section le long de l'axe déformé amplifié.
     * Flexion : la section suit la tangente (transport minimal du repère de la barre droite). Torsion
     * (applyTwist et axe avec θx) : la section tourne en plus de s·θx autour de la tangente, s étant le
     * facteur d'affichage. Une torsion pure fait tourner les sections sans déplacer l'axe : seule cette
     * rotation la rend visible.
     */
    static std::vector<DeformedSectionFrame> sectionFrames(const DeformedAxis& axis, double rotationDeg, double scale,
                                                           bool applyTwist = true);

    /// Barre déformée continue : la section est balayée le long de l'axe déformé en un seul maillage
    /// (aucun empilement de prismes), orientée par sectionFrames (flexion et, si applyTwist, torsion).
    static TopoDS_Shape createMemberSolid(const DeformedAxis& axis, const TSA::Model::Section& section,
                                          double rotationDeg, double scale, bool applyTwist = true);

    /**
     * @brief Repères de torsion : contours de la section en `rings` points répartis le long de la barre
     * et génératrice tracée par un point fixe du contour (hélice sous torsion). Rendent la rotation
     * visible aussi pour une section ronde, dont le contour est invariant par rotation.
     */
    static TopoDS_Shape createSectionMarkers(const DeformedAxis& axis, const TSA::Model::Section& section,
                                             double rotationDeg, double scale, bool applyTwist = true, int rings = 5);

    /// Plus grande distance du centre au contour extérieur de la section (m) : déplacement d'une fibre
    /// extrême par radian de torsion.
    static double sectionRadius(const TSA::Model::Section& section);

    /// Compatibilité : axe d'Hermite nodal (sans stations) puis polyligne.
    static TopoDS_Shape createDeformedCenterline(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        const TSA::Analysis::NodeDisplacement& d1,
        const TSA::Analysis::NodeDisplacement& d2,
        double scaleFactor,
        int numSegments = 12
    );

    /// Compatibilité : axe d'Hermite nodal (sans stations) puis barre continue.
    static TopoDS_Shape createDeformedBeamShape(
        const gp_Pnt& p1,
        const gp_Pnt& p2,
        const TSA::Analysis::NodeDisplacement& d1,
        const TSA::Analysis::NodeDisplacement& d2,
        const TSA::Model::Section& section,
        double scaleFactor,
        double rotationDeg = 0.0
    );
};

} // namespace TSA::Geometry
