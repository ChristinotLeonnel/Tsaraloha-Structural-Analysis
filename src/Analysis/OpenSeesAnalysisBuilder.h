#pragma once

#include "CalculationSnapshot.h"
#include "ResultsModel.h"
#include <string>

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

    // Modal
    int numEigenmodes = 3;

    // Gestion des résultats
    bool saveAllSteps = true;       ///< Enregistrer tous les incréments

    // Fichiers recorders
    std::string workingDir = ".";
    std::string dispOutputFile = "node_disp.out";
    std::string reactOutputFile = "node_react.out";
    std::string forceOutputFile = "ele_forces.out";
};

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
class OpenSeesAnalysisBuilder
{
public:
    static std::string buildScript(const CalculationSnapshot& snapshot,
                                   const AnalysisParameters& params);

    static std::string buildNodes(const CalculationSnapshot& snapshot);
    static std::string buildBoundaryConditions(const CalculationSnapshot& snapshot);
    static std::string buildElements(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildElements(const CalculationSnapshot& snapshot, bool useKiloNewtons);
    static std::string buildRecorders(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildLoads(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
    static std::string buildAnalysisCommands(const CalculationSnapshot& snapshot, const AnalysisParameters& params);
};

} // namespace TSA::Analysis
