#pragma once

#include "CalculationSnapshot.h"
#include "ResultsModel.h"
#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis
{

enum class NonlinearAlgorithm
{
    Newton,                 ///< Newton-Raphson standard
    NewtonLineSearch,       ///< Newton avec recherche linéaire (Line Search)
    ModifiedNewton,         ///< Newton modifié (matrice tangente recalculée)
    KrylovNewton,           ///< Accélération sous-espace de Krylov
    BFGS,                   ///< Quasi-Newton BFGS
    Broyden,                ///< Quasi-Newton Broyden
    SecantNewton            ///< Secant Newton
};

enum class IntegratorType
{
    LoadControl,            ///< Contrôle de chargement incrémental
    DisplacementControl,    ///< Contrôle du déplacement à un nœud cible
    ArcLength,              ///< Longueur d'arc (Arc-Length)
    MinUnbalDispNorm        ///< Norme de déplacement déséquilibré minimum
};

enum class SystemSolver
{
    BandGeneral,            ///< Solveur bande générale LAPACK
    ProfileSPD,             ///< Solveur profil symétrique défini positif
    SuperLU,                ///< Solveur creux direct SuperLU (Sparse)
    UmfPack,                ///< Solveur creux UmfPack
    BandSPD,                ///< Solveur bande symétrique défini positif
    SparseGEN               ///< Solveur creux général
};

enum class ConstraintHandler
{
    Transformation,         ///< Méthode de transformation (recommandée)
    Plain,                  ///< Contraintes directes / simples
    Penalty,                ///< Méthode des pénalités
    Lagrange                ///< Multiplicateurs de Lagrange
};

enum class TrussFormulation
{
    Truss,                  ///< Formulation linéaire standard
    CorotTruss,             ///< Formulation corotationnelle (grands déplacements)
    TrussSection,           ///< Formulation avec intégration de section
    CorotTrussSection       ///< Formulation corotationnelle avec section
};

enum class GeomTransfType
{
    Linear,                 ///< Transformation géométrique linéaire
    PDelta,                 ///< Effets du second ordre P-Delta
    Corotational            ///< Formulation corotationnelle complète
};

struct AnalysisParameters
{
    AnalysisType type = AnalysisType::LinearStatic;
    int targetLoadCaseId = 0;       ///< 0 = tous les cas actifs
    int targetCombinationId = 0;    ///< 0 = cas individuels
    bool useKiloNewtons = true;     ///< true: kN, m, kPa, kNm | false: N, m, Pa, Nm
    bool includeSelfWeight = true;

    // Méthodes et algorithmes de résolution
    NonlinearAlgorithm algorithmType = NonlinearAlgorithm::Newton;
    NonlinearAlgorithm algorithm = NonlinearAlgorithm::Newton;
    IntegratorType integratorType = IntegratorType::LoadControl;
    IntegratorType integrator = IntegratorType::LoadControl;
    SystemSolver systemSolver = SystemSolver::BandGeneral;
    ConstraintHandler constraintHandler = ConstraintHandler::Transformation;
    TrussFormulation trussFormulation = TrussFormulation::Truss;
    GeomTransfType geomTransf = GeomTransfType::Linear;

    // Contrôle et convergence
    int maxIterations = 50;
    double tolerance = 1e-6;
    int numSteps = 10;
    double stepSize = 0.05;         ///< Facteur de pas (LoadControl ou ArcLength)

    // Contrôle de déplacement
    int controlNodeId = 1;
    int controlDof = 3;             ///< 1=UX, 2=UY, 3=UZ
    double dispIncrement = -0.001;  ///< Incrément de déplacement par pas (m)

    // Gestion des résultats
    bool saveAllSteps = true;       ///< Enregistrer tous les incréments

    // Niveau d'extraction : Light (défaut) ou Advanced (matrices, mapping DDL, forces brutes)
    ExtractionLevel extractionLevel = ExtractionLevel::Light;
    /// K_global n'est extraite (system FullGeneral, matrice dense n×n dans OpenSees) que si le
    /// nombre de DDL libres estimé est inférieur ou égal à ce plafond.
    int maxGlobalStiffnessDofs = 1500;

    // Fichiers recorders (analyse principale)
    std::string workingDir = ".";
    std::string dispOutputFile = "node_disp.out";
    std::string reactOutputFile = "node_react.out";
    std::string forceOutputFile = "ele_forces.out";          ///< poutres/poteaux : localForce (12)
    std::string axialOutputFile = "ele_axial.out";           ///< treillis/câbles : basicForce (1)
    std::string globalForceOutputFile = "ele_global.out";    ///< Advanced : globalForce (12)
    std::string basicForceOutputFile = "ele_basic.out";      ///< Advanced : poutres basicForce (6)

    // Passage « matrices » (Advanced, script séparé, état de référence non déformé)
    std::string matrixScriptFile = "matrices.tcl";
    std::string dofMapOutputFile = "dof_map.out";
    std::string globalStiffnessOutputFile = "k_global.out";
    std::string beamBasicStiffnessOutputFile = "kb_beam.out";
    std::string trussBasicStiffnessOutputFile = "kb_truss.out";
};

/// Chiffres significatifs demandés aux recorders OpenSees (-precision).
constexpr int kRecorderPrecision = 16;

inline const char* algorithmToTcl(NonlinearAlgorithm algo)
{
    switch (algo)
    {
    case NonlinearAlgorithm::Newton: return "Newton";
    case NonlinearAlgorithm::NewtonLineSearch: return "NewtonLineSearch";
    case NonlinearAlgorithm::ModifiedNewton: return "ModifiedNewton";
    case NonlinearAlgorithm::KrylovNewton: return "KrylovNewton";
    case NonlinearAlgorithm::BFGS: return "BFGS";
    case NonlinearAlgorithm::Broyden: return "Broyden";
    case NonlinearAlgorithm::SecantNewton: return "SecantNewton";
    }
    return "Newton";
}

inline const char* integratorToTcl(IntegratorType integ)
{
    switch (integ)
    {
    case IntegratorType::LoadControl: return "LoadControl";
    case IntegratorType::DisplacementControl: return "DisplacementControl";
    case IntegratorType::ArcLength: return "ArcLength";
    case IntegratorType::MinUnbalDispNorm: return "MinUnbalDispNorm";
    }
    return "LoadControl";
}

inline const char* toTclString(IntegratorType integ)
{
    return integratorToTcl(integ);
}

inline const char* toTclString(NonlinearAlgorithm algo)
{
    switch (algo)
    {
    case NonlinearAlgorithm::Newton: return "Newton";
    case NonlinearAlgorithm::NewtonLineSearch: return "NewtonLineSearch 0.8";
    case NonlinearAlgorithm::ModifiedNewton: return "ModifiedNewton";
    case NonlinearAlgorithm::KrylovNewton: return "KrylovNewton";
    case NonlinearAlgorithm::BFGS: return "BFGS";
    case NonlinearAlgorithm::Broyden: return "Broyden";
    case NonlinearAlgorithm::SecantNewton: return "SecantNewton";
    }
    return "Newton";
}

inline const char* toTclString(SystemSolver sys)
{
    switch (sys)
    {
    case SystemSolver::BandGeneral: return "BandGeneral";
    case SystemSolver::ProfileSPD: return "ProfileSPD";
    case SystemSolver::SuperLU: return "SuperLU";
    case SystemSolver::UmfPack: return "UmfPack";
    case SystemSolver::BandSPD: return "BandSPD";
    case SystemSolver::SparseGEN: return "SparseGEN";
    }
    return "BandGeneral";
}

inline const char* systemSolverToTcl(SystemSolver sys)
{
    return toTclString(sys);
}

inline const char* toTclString(ConstraintHandler ch)
{
    switch (ch)
    {
    case ConstraintHandler::Transformation: return "Transformation";
    case ConstraintHandler::Plain: return "Plain";
    case ConstraintHandler::Penalty: return "Penalty 1.0e12 1.0e12";
    case ConstraintHandler::Lagrange: return "Lagrange";
    }
    return "Transformation";
}

inline const char* toTclString(GeomTransfType gt)
{
    switch (gt)
    {
    case GeomTransfType::Linear: return "Linear";
    case GeomTransfType::PDelta: return "PDelta";
    case GeomTransfType::Corotational: return "Corotational";
    }
    return "Linear";
}

/**
 * @brief Constructeur de scripts OpenSees modulaire et multi-analyses.
 * Traduit le snapshot calculatoire figé en code Tcl vérifiable avec recorders et diagnostics.
 */
class OpenSeesModelMap;

/// Motif de chargement OpenSees (pattern Plain) : un par cas d'une combinaison, ou un seul motif.
struct LoadPatternSpec
{
    int id = 1;
    std::string name;
    int caseId = 0;                  ///< 0 = tous les cas
    double factor = 1.0;
    bool includeSelfWeight = false;
};

/// Charge appliquée à une poutre / un poteau, en repère local, en unités du script et facteur du
/// motif inclus. Une seule source pour le script ET la reconstruction des efforts le long des barres
/// (OpenSeesResultsReader) : ce qui est calculé est exactement ce qui est affiché.
///  - Uniform : eleLoad -beamUniform (toute la longueur) ;
///  - Point   : eleLoad -beamPoint ;
///  - Linear  : charge linéaire variable (trapézoïdale) sur [a, b] — non gérée par eleLoad en 3D dans
///    OpenSees 3.8.0 : appliquée aux nœuds par ses forces d'encastrement parfait (fixedEndForces),
///    que le lecteur ajoute aux efforts d'extrémité calculés par OpenSees.
struct BeamElementLoad
{
    enum class Kind { Uniform, Point, Linear };
    Kind kind = Kind::Uniform;
    double wx = 0.0, wy = 0.0, wz = 0.0;     ///< intensité (Uniform), force (Point), intensité en a (Linear)
    double wxB = 0.0, wyB = 0.0, wzB = 0.0;  ///< intensité en b (Linear)
    double relativePosition = 0.0;           ///< Point : x / L ; Linear : a / L
    double relativeEnd = 1.0;                ///< Linear : b / L
    std::string comment;

    bool point() const { return kind == Kind::Point; }
    /// Forces d'encastrement parfait (barre bi-encastrée) exercées SUR la barre en i et en j,
    /// repère local [Fx Fy Fz Mx My Mz].
    void fixedEndForces(double length, double atI[6], double atJ[6]) const;
};

/// Résultante d'une charge sur barre en repère global (unités de la charge, sans facteur) et position
/// de son centre depuis le nœud i. Intensités q1 et q2 résolues séparément, comme pour le calcul.
struct MemberLoadResultant
{
    gp_Vec force;
    double centroid = 0.0;
};
MemberLoadResultant memberLoadResultant(const TSA::Model::MemberLoad& load, const CalculationSnapshot& snapshot);

class OpenSeesAnalysisBuilder
{
public:
    static std::vector<LoadPatternSpec> loadPatterns(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    /// Charges eleLoad d'un motif, par tag d'élément poutre/poteau (treillis et câbles : charges aux nœuds).
    static std::map<int, std::vector<BeamElementLoad>> beamElementLoads(const CalculationSnapshot& snapshot,
                                                                        const AnalysisParameters& params,
                                                                        const LoadPatternSpec& pattern);
    /// Charges eleLoad de tous les motifs (calcul linéaire : superposition), par tag d'élément.
    static std::map<int, std::vector<BeamElementLoad>> beamElementLoads(const CalculationSnapshot& snapshot,
                                                                        const AnalysisParameters& params);

    static std::string buildScript(const CalculationSnapshot& snapshot,
                                   const AnalysisParameters& params);
    static std::string buildScript(const CalculationSnapshot& snapshot,
                                   const OpenSeesModelMap& map,
                                   const AnalysisParameters& params);

    /// Script du passage « matrices » (mode Advanced) : même modèle (nœuds, appuis, ressorts,
    /// éléments, handler, numberer), sans charge ni résolution : `initialize` numérote les DDL
    /// à l'état de référence, puis export du mapping (nodeDOFs), des rigidités basiques
    /// (recorder basicStiffness) et, si withGlobalStiffness, de la matrice du système
    /// (system FullGeneral + printA -ret, seul système exposant sa matrice dans OpenSees 3.8.0).
    static std::string buildMatrixScript(const CalculationSnapshot& snapshot,
                                         const OpenSeesModelMap& map,
                                         const AnalysisParameters& params,
                                         bool withGlobalStiffness);

    static std::string buildNodes(const CalculationSnapshot& snapshot);
    static std::string buildBoundaryConditions(const CalculationSnapshot& snapshot, const OpenSeesModelMap& map);
    static std::string buildBoundaryConditions(const CalculationSnapshot& snapshot);
    static std::string buildElements(const CalculationSnapshot& snapshot, const OpenSeesModelMap& map, const AnalysisParameters& params);
    static std::string buildRecorders(const OpenSeesModelMap& map, const AnalysisParameters& params);
    static std::string buildLoads(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildAnalysisCommands(const CalculationSnapshot& snapshot, const AnalysisParameters& params);

    /// Liste Tcl d'entiers "1 2 3".
    static std::string joinTags(const std::vector<int>& tags);
};

} // namespace TSA::Analysis
