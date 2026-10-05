#pragma once

// Contrat d'intégration du solveur structurel 2D personnalisé.
//
// Le solveur ne connaît RIEN de TSA : il reçoit une ossature plane déjà extraite (indices contigus,
// coordonnées dans le plan, unités kN / m / kPa) et rend ses résultats dans les mêmes indices.
// L'adaptateur (Custom2DAdapter) fait seul la conversion TSA ↔ 2D et le remappage des résultats
// vers les objets TSA. Pour brancher le solveur : implémenter ISolver et appeler
// Custom2DEngine::connectSolver (voir registerBuiltInEngines).
//
// Repères et conventions :
//  - plan (x, y) = (u, v) de la portée ; pour un axe de grille, x suit l'axe, y = +Z global ;
//  - DDL par nœud : ux, uy, θz (θz autour de la normale n = x × y, sens direct) ;
//  - repère local d'une barre : x' de i vers j, y' = x' tourné de +90° dans le plan ;
//  - efforts d'extrémité : forces EXERCÉES SUR la barre, repère local
//    [Fx_i, Fy_i, Mz_i, Fx_j, Fy_j, Mz_j] (méthode des déplacements classique) ;
//  - stations intermédiaires (facultatives) : efforts intérieurs, N > 0 en traction, V selon y',
//    M autour de z (même signe que Mz_j).

#include "../../ResultsModel.h"   // AnalysisType

#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis::Custom2D
{

struct Node
{
    int index = 0;              ///< 1..N
    double x = 0.0, y = 0.0;    ///< m
    bool fixX = false, fixY = false, fixRz = false;
    double kX = 0.0, kY = 0.0;  ///< appuis élastiques (kN/m) ; 0 = aucun
    double kRz = 0.0;           ///< kN·m/rad
};

enum class ElementType
{
    Frame,   ///< barre fléchie (E, A, I)
    Truss    ///< effort normal seul (E, A)
};

struct Element
{
    int index = 0;              ///< 1..M
    int nodeI = 0, nodeJ = 0;   ///< indices de nœuds
    ElementType type = ElementType::Frame;
    double E = 0.0;             ///< kPa (kN/m²)
    double A = 0.0;             ///< m²
    double I = 0.0;             ///< m⁴, flexion dans le plan
    double length = 0.0;        ///< m
    double weightPerLength = 0.0; ///< kN/m (ρ·g·A), pour le poids propre
    bool releaseI = false;      ///< rotule (moment nul) à l'extrémité i
    bool releaseJ = false;      ///< rotule à l'extrémité j
};

struct NodalLoad
{
    int loadCaseId = 0;
    int node = 0;
    double fx = 0.0, fy = 0.0;  ///< kN, repère du plan
    double mz = 0.0;            ///< kN·m
};

enum class MemberLoadKind
{
    Distributed,   ///< linéique trapézoïdale de a à b (uniforme : q1 = q2, a = 0, b = L)
    Point          ///< force concentrée en a
};

struct MemberLoad
{
    int loadCaseId = 0;
    int element = 0;
    MemberLoadKind kind = MemberLoadKind::Distributed;
    double a = 0.0, b = 0.0;      ///< positions absolues depuis le nœud i (m)
    double px1 = 0.0, py1 = 0.0;  ///< composantes locales (x', y') en a — kN/m ou kN
    double px2 = 0.0, py2 = 0.0;  ///< composantes locales en b (Distributed)
};

struct LoadCase
{
    int id = 0;
    std::string name;
    bool includeSelfWeight = false;   ///< poids propre rattaché au cas (utilisé par les combinaisons)
    double selfWeightFactor = 1.0;
};

struct Combination
{
    int id = 0;
    std::string name;
    std::map<int, double> factors;    ///< cas → coefficient
};

/// Chargement demandé (mêmes règles que le générateur OpenSees pour rester comparables) :
///  - combinationId > 0 : Σ facteur × cas, poids propre selon LoadCase::includeSelfWeight ;
///  - sinon cas de loadCaseIds superposés (vide = tous), poids propre si includeSelfWeight.
struct Request
{
    AnalysisType type = AnalysisType::LinearStatic;
    std::vector<int> loadCaseIds;
    int combinationId = 0;
    bool includeSelfWeight = true;
    double gravityX = 0.0, gravityY = -1.0;   ///< direction unitaire de la pesanteur dans le plan
};

/// Réglages propres au solveur (bloc « custom2d » des réglages d'analyse).
struct Options
{
    double axialStiffnessFactor = 1.0;   ///< 1 = EA réel ; grand = barres inextensibles
    int curvePoints = 41;                ///< points par courbe d'effort / déformée
};

struct Input
{
    Options options;
    std::vector<Node> nodes;
    std::vector<Element> elements;
    std::vector<NodalLoad> nodalLoads;
    std::vector<MemberLoad> memberLoads;
    std::vector<LoadCase> loadCases;
    std::vector<Combination> combinations;
    Request request;
};

struct NodeDisplacement
{
    int node = 0;
    double ux = 0.0, uy = 0.0, rz = 0.0;   ///< m, rad
};

struct NodeReaction
{
    int node = 0;
    double fx = 0.0, fy = 0.0, mz = 0.0;   ///< kN, kN·m
};

struct Station
{
    double x = 0.0;                        ///< m depuis le nœud i
    double N = 0.0, V = 0.0, M = 0.0;      ///< convention de l'en-tête (V selon y', M comme Mz_j)
    double u = 0.0, v = 0.0;               ///< déplacements locaux (x', y'), m
};

/// Valeurs caractéristiques d'une barre, convention RDM : M > 0 tend la fibre y' < 0,
/// V = dM/dx, N > 0 en traction. Flèche = écart de la déformée à la corde.
struct MemberSummary
{
    double length = 0.0;
    double Mi = 0.0, Mj = 0.0;
    bool hasSpanExtremum = false;
    double xSpanExtremum = 0.0, MSpanExtremum = 0.0;
    double Mmax = 0.0, xMmax = 0.0, Mmin = 0.0, xMmin = 0.0;
    std::vector<double> momentZeros;
    double Vi = 0.0, Vj = 0.0;
    double Nmin = 0.0, Nmax = 0.0;
    double deflectionMax = 0.0, xDeflectionMax = 0.0;
    double rotationI = 0.0, rotationJ = 0.0;
};

struct ElementForces
{
    int element = 0;
    double fxI = 0.0, fyI = 0.0, mzI = 0.0;
    double fxJ = 0.0, fyJ = 0.0, mzJ = 0.0;
    std::vector<Station> stations;
    bool hasSummary = false;
    MemberSummary summary;
};

/// Résultat propre au solveur, sans équivalent commun (ex. indicateurs internes).
/// nodeColumn / elementColumn : colonne contenant un indice 2D, remplacé par l'identifiant TSA.
struct CustomTable
{
    std::string title;
    std::vector<std::string> columns;
    std::vector<std::vector<double>> rows;
    int nodeColumn = -1;
    int elementColumn = -1;
};

struct Output
{
    bool success = false;
    std::string message;
    std::vector<NodeDisplacement> displacements;
    std::vector<NodeReaction> reactions;
    std::vector<ElementForces> elementForces;
    std::vector<CustomTable> customTables;
    std::vector<std::string> log;
    std::string method;                    ///< méthode de calcul (pour la note de calcul)
    double equilibriumResidual = -1.0;     ///< |Σ charges + Σ réactions| (kN) ; < 0 : non fourni
};

/// Fonctionnalités réellement implémentées par un solveur (pilotent les capacités du moteur).
struct Features
{
    bool truss = false;     ///< barres articulées (ElementType::Truss)
    bool springs = false;   ///< appuis élastiques
    bool releases = false;  ///< rotules d'extrémité (releaseI / releaseJ)
};

class ISolver
{
public:
    virtual ~ISolver() = default;
    virtual std::string name() const = 0;
    /// Version réelle du solveur ; vide si inconnue.
    virtual std::string version() const = 0;
    virtual Output solve(const Input& input) = 0;
    virtual void cancel() {}
    virtual Features features() const { return {}; }
};

} // namespace TSA::Analysis::Custom2D
