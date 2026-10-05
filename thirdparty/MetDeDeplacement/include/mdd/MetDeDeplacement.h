#pragma once

// =============================================================================================
// MetDeDeplacement — méthode des déplacements pour ossatures planes (version 2, intégrée à TSA)
// Auteur du modèle d'origine : TSARALOHA Nomenjanahary Christinot Léonnel Calixte.
//
// La méthode des rotations d'origine (inconnues : rotations des nœuds + translations d'étage,
// K = 4EI/L, report 1/2, moments d'encastrement parfait) est ici écrite sous forme matricielle
// générale : 3 DDL par nœud (u, v, θ), barres de direction quelconque, rotules d'extrémité,
// appuis rigides ou élastiques, charges nodales, réparties trapézoïdales et ponctuelles.
// L'option `axialStiffnessFactor` (barres quasi inextensibles) retrouve l'hypothèse de la
// méthode des rotations classique.
//
// Bibliothèque autonome : C++20, sans dépendance (ni Qt, ni TSA).
//
// Conventions (repère du plan x, y ; θ et moments positifs dans le sens trigonométrique) :
//  - repère local d'une barre : x' de i vers j, y' = x' tourné de +90° ;
//  - efforts d'extrémité `MemberEndForces` : forces EXERCÉES SUR la barre par les nœuds, repère
//    local [Fx_i, Fy_i, Mz_i, Fx_j, Fy_j, Mz_j] ;
//  - courbes (convention RDM, coupure en x, tronçon gauche [0, x]) :
//      N(x) > 0 en traction ; M(x) = moment fléchissant, positif s'il tend la fibre y' < 0
//      (« fibre inférieure » d'une poutre horizontale parcourue de gauche à droite) ;
//      V(x) = dM/dx ; Vface(x) = -V(x) est la composante y' de l'effort exercé par le tronçon
//      droit sur le tronçon gauche (égale à Fy_j en x = L) ; M(L) = Mz_j, M(0) = -Mz_i ;
//      u(x), v(x) : déplacements locaux (axial, transversal) ; EI v'' = M, EA u' = N.
// =============================================================================================

#include <array>
#include <string>
#include <vector>

namespace mdd
{

inline constexpr const char* kVersion = "2.0.0";

struct Node
{
    double x = 0.0, y = 0.0;                  ///< m
    bool fixX = false, fixY = false, fixRz = false;
    double kX = 0.0, kY = 0.0;                ///< appuis élastiques, kN/m (0 = aucun)
    double kRz = 0.0;                         ///< kN·m/rad
};

struct Member
{
    int i = 0, j = 0;                         ///< indices de nœuds (0-based)
    double E = 0.0;                           ///< kPa
    double A = 0.0;                           ///< m²
    double I = 0.0;                           ///< m⁴
    bool releaseI = false;                    ///< rotule à l'extrémité i (moment nul)
    bool releaseJ = false;                    ///< rotule à l'extrémité j
};

struct NodalLoad
{
    int node = 0;
    double fx = 0.0, fy = 0.0, mz = 0.0;      ///< kN, kN·m (repère du plan)
};

/// Charge répartie trapézoïdale de a à b (positions depuis i), composantes LOCALES (kN/m).
struct DistributedLoad
{
    int member = 0;
    double a = 0.0, b = 0.0;
    double px1 = 0.0, py1 = 0.0;              ///< en a
    double px2 = 0.0, py2 = 0.0;              ///< en b
};

/// Force concentrée sur une barre, en a (depuis i), composantes LOCALES (kN).
struct PointLoad
{
    int member = 0;
    double a = 0.0;
    double px = 0.0, py = 0.0;
};

struct Options
{
    /// Multiplicateur de EA (1 = exact ; 1e4 ≈ barres inextensibles de la méthode des rotations).
    double axialStiffnessFactor = 1.0;
    /// Points par courbe (hors points singuliers ajoutés automatiquement).
    int curvePoints = 41;
};

struct Model
{
    std::vector<Node> nodes;
    std::vector<Member> members;
    std::vector<NodalLoad> nodalLoads;
    std::vector<DistributedLoad> distributedLoads;
    std::vector<PointLoad> pointLoads;
    Options options;
};

struct MemberEndForces
{
    double fxI = 0.0, fyI = 0.0, mzI = 0.0;
    double fxJ = 0.0, fyJ = 0.0, mzJ = 0.0;
};

struct CurvePoint
{
    double x = 0.0;                           ///< m depuis i
    double N = 0.0, V = 0.0, M = 0.0;         ///< convention RDM (voir en-tête)
    double u = 0.0, v = 0.0;                  ///< déplacements locaux (m)
};

/// Valeurs caractéristiques d'une barre (lecture des courbes, RDM).
struct CurveSummary
{
    double length = 0.0;
    double Mi = 0.0, Mj = 0.0;                ///< moments aux extrémités (RDM)
    bool hasSpanExtremum = false;             ///< extremum de M en travée (V = 0)
    double xSpanExtremum = 0.0, MSpanExtremum = 0.0;
    double Mmax = 0.0, xMmax = 0.0;           ///< max de M sur la barre
    double Mmin = 0.0, xMmin = 0.0;           ///< min de M sur la barre
    std::vector<double> momentZeros;          ///< abscisses où M s'annule (en travée)
    double Vi = 0.0, Vj = 0.0;                ///< V(0+), V(L-)
    double Nmin = 0.0, Nmax = 0.0;
    double deflectionMax = 0.0;               ///< flèche : écart à la corde, |valeur| max (signée)
    double xDeflectionMax = 0.0;
    double rotationI = 0.0, rotationJ = 0.0;  ///< rotations des sections d'extrémité (rad)
};

struct MemberResult
{
    MemberEndForces end;
    std::vector<CurvePoint> curve;
    CurveSummary summary;
};

struct Result
{
    bool success = false;
    std::string message;
    std::vector<std::array<double, 3>> displacements;   ///< par nœud : ux, uy (m), θz (rad)
    std::vector<std::array<double, 3>> reactions;       ///< par nœud : Rx, Ry (kN), Mz (kN·m) ; 0 hors appui
    std::vector<bool> supported;                        ///< nœud avec appui rigide ou élastique
    std::vector<MemberResult> members;
    double equilibriumResidual = 0.0;                   ///< |Σ charges + Σ réactions| (kN)
    int equations = 0;                                  ///< DDL libres
    int halfBandwidth = 0;                              ///< demi-largeur de bande après renumérotation
    std::vector<std::string> log;
};

/// Calcul statique linéaire. Ne lève pas d'exception : échec → success = false + message.
Result solve(const Model& model);

/// Moments, tranchant, effort normal et déformée d'une barre isolée à partir de ses efforts
/// d'extrémité et de ses déplacements locaux d'extrémité (u_i, v_i, u_j, v_j). Utilisé par solve().
void computeCurves(const Model& model, int memberIndex, const MemberEndForces& end,
                   const std::array<double, 4>& localEndDisplacements, MemberResult& out);

} // namespace mdd
