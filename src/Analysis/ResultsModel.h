#pragma once

#include "AnalysisTypes.h"
#include "SolverMesh.h"

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>

namespace TSA::Analysis
{

/// TSA se limite au calcul statique (linéaire, ou non linéaire par incréments de charge).
enum class AnalysisType
{
    LinearStatic,
    NonLinearStatic
};

/**
 * @brief Déplacement et rotation nodale (6 DDL).
 */
struct NodeDisplacement
{
    double ux = 0.0;
    double uy = 0.0;
    double uz = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;

    double translationMagnitude() const
    {
        return std::sqrt(ux * ux + uy * uy + uz * uz);
    }
};

/**
 * @brief Réaction d'appui nodale (6 DDL).
 */
struct NodeReaction
{
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    double mx = 0.0;
    double my = 0.0;
    double mz = 0.0;

    double forceMagnitude() const
    {
        return std::sqrt(rx * rx + ry * ry + rz * rz);
    }
};

/**
 * @brief Efforts intérieurs en une station le long d'une barre (repère local).
 */
/// Efforts et déplacements en une station d'une barre, repère local (x de i vers j).
/// Convention RDM commune à tous les moteurs : N > 0 en traction ; My, Mz > 0 quand la fibre située du
/// côté négatif de l'axe local (z < 0, resp. y < 0) est tendue (moment de travée positif sous une charge
/// dirigée vers −z / −y) ; Vy = dMz/dx, Vz = dMy/dx ; Mx : torsion, face positive.
struct StationForces
{
    double position = 0.0;  ///< Position le long de l'élément (0.0 <= x <= L)
    double N = 0.0;         ///< Effort normal (kN ou N)
    double Vy = 0.0;        ///< Effort tranchant local Y (kN ou N)
    double Vz = 0.0;        ///< Effort tranchant local Z (kN ou N)
    double Mx = 0.0;        ///< Moment de torsion local (kNm ou Nm)
    double My = 0.0;        ///< Moment fléchissant local Y (kNm ou Nm)
    double Mz = 0.0;        ///< Moment fléchissant local Z (kNm ou Nm)

    double ux = 0.0;        ///< Déplacement local u(x)
    double uy = 0.0;        ///< Déplacement local v(x)
    double uz = 0.0;        ///< Déplacement local w(x)
    double rx = 0.0;        ///< Rotation locale rx(x)
    double ry = 0.0;        ///< Rotation locale ry(x)
    double rz = 0.0;        ///< Rotation locale rz(x)
};

/**
 * @brief Résultats d'une barre structurelle (extrémités et stations intermédiaires).
 */
struct ElementResults
{
    int elementId = 0;                  ///< identifiant TSA (unique dans sa famille seulement)
    StructuralElementKind kind = StructuralElementKind::Beam;
    int opsTag = 0;                     ///< tag de l'élément dans le modèle OpenSees
    double length = 0.0;

    ElementKey key() const { return { kind, elementId }; }
    StationForces startForces; // Station i (x = 0)
    StationForces endForces;   // Station j (x = L)
    std::vector<StationForces> intermediateStations; // Profil discrétisé

    double maxNormalForce() const;
    double minNormalForce() const;
    double maxBendingMoment() const;
    double maxShearForce() const;
};

/**
 * @brief Résultats d'un incrément de charge (analyse statique non linéaire).
 */
struct StepResults
{
    int stepNumber = 0;
    double factorOrTime = 0.0;
    std::map<int, NodeDisplacement> displacements;
    std::map<int, NodeReaction> reactions;
    std::map<ElementKey, ElementResults> elementResults;
};

/**
 * @brief Contrôle d'équilibre statique global.
 */
struct GlobalEquilibrium
{
    /// Résultantes des charges appliquées et des réactions, dans les unités du calcul
    /// (UnitSystem du ResultsModel). Convention : Σ F_ext + Σ R ≈ 0.
    double appliedFx = 0.0;
    double appliedFy = 0.0;
    double appliedFz = 0.0;
    double reactionFx = 0.0;
    double reactionFy = 0.0;
    double reactionFz = 0.0;
    /// Moments autour de l'origine globale (charges : r × F + M nodaux ; réactions : r × R + M_R).
    double appliedMx = 0.0, appliedMy = 0.0, appliedMz = 0.0;
    double reactionMx = 0.0, reactionMy = 0.0, reactionMz = 0.0;
    /// Échelle de référence des moments (Σ |r × F| + Σ |M| des charges) pour le contrôle relatif.
    double momentScale = 0.0;

    double errorFx() const { return appliedFx + reactionFx; }
    double errorFy() const { return appliedFy + reactionFy; }
    double errorFz() const { return appliedFz + reactionFz; }
    double errorMx() const { return appliedMx + reactionMx; }
    double errorMy() const { return appliedMy + reactionMy; }
    double errorMz() const { return appliedMz + reactionMz; }
    double maxError() const {
        return std::max({ std::abs(errorFx()), std::abs(errorFy()), std::abs(errorFz()) });
    }
    /// |Σ M + Σ M_R| / échelle des moments (0 si aucune charge).
    double relativeMomentResidual() const;
    /// Équilibre des forces ET des moments, en relatif.
    bool isBalanced(double tol = 1e-3) const;
};

/**
 * @brief Métadonnées d'exécution et traçabilité normative des résultats d'analyse.
 * [NORM: ISO/IEC 25010 §4.2.5, EN 1990 §6, IEEE Std 1063]
 */
struct AnalysisExecutionMetadata
{
    std::string solverEngine = "OpenSees";
    std::string solverVersion = "3.8.0";
    std::string engineId;               ///< identifiant du moteur (registre), ex. "opensees", "custom2d"
    std::string analysisScope;          ///< portée calculée, ex. « Axe B (Grille 1) » ; vide = modèle complet
    std::string analysisDimension;      ///< "2d" ou "3d"
    std::string calculationMethod;      ///< méthode du moteur (texte de la note de calcul)
    std::string nationalAnnex = "France NF (NF EN 1990/NA)";
    std::string normativeFramework = "EN 1990:2002+A1:2005 / ISO/IEC 25010";
    std::string loadCombinationType = "Statique Linéaire";
    double globalEquilibriumTolerance = 1e-3;
    double maxResidualForce = 0.0;
    bool isEquilibriumVerified = false;
    std::string executionTimestamp;
    int totalNodes = 0;
    int totalElements = 0;
    double executionDurationMs = 0.0;

    // Configuration réelle du système OpenSees (traçabilité des matrices)
    std::string modelBuilder = "BasicBuilder -ndm 3 -ndf 6";
    std::string systemSolver;
    std::string constraintHandler;
    std::string numberer = "RCM";
    std::string geomTransf;
    double relativeEquilibriumResidual = 0.0;   ///< |Σ F + Σ R| / |Σ F|
    double relativeMomentResidual = 0.0;        ///< |Σ M + Σ M_R| / échelle des moments (autour de l'origine)
    ExtractionLevel extractionLevel = ExtractionLevel::Light;
};

/// Efforts élémentaires bruts fournis par OpenSees (mode ADVANCED), sans post-traitement.
/// Ordre des 12 composantes : [Fx Fy Fz Mx My Mz]_i puis _j.
///  - local  : forces exercées sur l'élément, repère local (ElasticBeam3d « localForce » ;
///             treillis/câbles : T · global, transformation exacte)
///  - global : forces exercées sur l'élément, repère global (« globalForce », tous éléments)
///  - basic  : ElasticBeam3d [N, Mz_i, Mz_j, My_i, My_j, T] ; treillis/câbles [N] (traction > 0)
struct ElementForceSet
{
    ElementKey key;
    int opsTag = 0;
    std::vector<double> local;
    std::vector<double> global;
    std::vector<double> basic;
    std::vector<std::string> basicLabels;
    std::string localSource;
};

/// Matrices de rigidité d'un élément, avec leurs métadonnées de traçabilité.
struct ElementMatrices
{
    ElementKey key;
    int opsTag = 0;
    std::string opsClass;             ///< "elasticBeamColumn", "truss", "corotTruss"
    int nodeI = 0;
    int nodeJ = 0;
    double length = 0.0;
    std::array<double, 9> rotation {}; ///< lignes : axes locaux x, y, z dans le repère global
    bool available = false;
    std::string unavailableReason;
    DenseMatrix kBasic;  MatrixMetadata kBasicMeta;
    DenseMatrix kLocal;  MatrixMetadata kLocalMeta;
    DenseMatrix kGlobal; MatrixMetadata kGlobalMeta;
};

/// Appui élastique modélisé par un élément zeroLength (nœud auxiliaire fixe).
struct SpringSupportInfo
{
    int nodeId = 0;           ///< nœud TSA
    int auxNodeTag = 0;       ///< nœud OpenSees auxiliaire (entièrement fixé)
    int elementTag = 0;       ///< élément zeroLength
    std::vector<int> dofs;    ///< 0..5
    std::vector<double> stiffness;
};

/// Catégories de résultats réellement fournies par le moteur pour CE calcul (capacité déclarée ET
/// données présentes). L'UI des résultats n'affiche que ces catégories.
struct ResultAvailability
{
    bool displacements = false;
    bool reactions = false;
    bool elementForces = false;
    bool elementStiffness = false;
    bool globalStiffness = false;
    bool dofMapping = false;
};

/// Table de résultats propre à un moteur (résultats qui n'ont pas d'équivalent commun).
/// Les lignes portent déjà les identifiants TSA (remappés par l'adaptateur).
struct EngineResultTable
{
    std::string engineId;
    std::string title;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

/// Courbes d'une barre calculée dans un plan (moteur 2D), convention RDM :
/// N > 0 en traction, M > 0 tend la fibre y' < 0 (fibre inférieure d'une poutre parcourue de
/// gauche à droite), V = dM/dx ; u, v : déplacements locaux axial / transversal (m).
struct PlanarCurvePoint
{
    double x = 0.0, N = 0.0, V = 0.0, M = 0.0, u = 0.0, v = 0.0;
};

struct PlanarMemberCurves
{
    ElementKey key;
    double length = 0.0;
    std::vector<PlanarCurvePoint> points;
    // Valeurs caractéristiques (fournies par le moteur)
    double Mi = 0.0, Mj = 0.0;
    bool hasSpanExtremum = false;
    double xSpanExtremum = 0.0, MSpanExtremum = 0.0;
    double Mmax = 0.0, xMmax = 0.0, Mmin = 0.0, xMmin = 0.0;
    std::vector<double> momentZeros;
    double Vi = 0.0, Vj = 0.0, Nmin = 0.0, Nmax = 0.0;
    double deflectionMax = 0.0, xDeflectionMax = 0.0;   ///< écart signé à la corde
    double rotationI = 0.0, rotationJ = 0.0;
};

/// Résultats avancés (optionnels) : présents seulement si ExtractionLevel::Advanced.
struct AdvancedResults
{
    bool available = false;
    DofMap dofMap;
    bool hasGlobalStiffness = false;
    SparseMatrix kGlobal;
    MatrixMetadata kGlobalMeta;
    std::string kGlobalUnavailableReason;
    /// Second membre F et solution U du système K·U = F, dans l'ordre des équations (moteurs qui
    /// exportent leur système résolu, ex. Custom2D) ; vides sinon.
    std::vector<double> loadVector;
    std::vector<double> displacementVector;
    std::map<ElementKey, ElementMatrices> elementMatrices;
    std::map<ElementKey, ElementForceSet> elementForces;
    std::vector<SpringSupportInfo> springs;
    std::vector<std::string> warnings;
    double matrixRunDurationMs = 0.0;

    std::size_t memoryBytes() const;
    /// Vecteur U_global dans l'ordre des équations (DDL libres uniquement).
    std::vector<double> globalDisplacementVector(const std::map<int, NodeDisplacement>& displacements) const;
};

/**
 * @brief Synthèse des valeurs extrêmes.
 */
struct ResultsSummary
{
    double maxDisplacement = 0.0;
    int maxDisplacementNodeId = 0;

    double maxReactionForce = 0.0;
    int maxReactionNodeId = 0;

    double maxTension = 0.0;
    int maxTensionElementId = 0;
    StructuralElementKind maxTensionElementKind = StructuralElementKind::Beam;

    double maxCompression = 0.0;
    int maxCompressionElementId = 0;
    StructuralElementKind maxCompressionElementKind = StructuralElementKind::Beam;

    double maxBendingMoment = 0.0;
    int maxBendingMomentElementId = 0;
    StructuralElementKind maxBendingMomentElementKind = StructuralElementKind::Beam;

};

/**
 * @brief Modèle de résultats complet et indépendant du solveur pour TSA.
 */
class ResultsModel
{
public:
    ResultsModel();

    void clear();
    void invalidate();
    bool isValid() const { return m_isValid; }
    void setValid(bool valid) { m_isValid = valid; }

    AnalysisType analysisType() const { return m_analysisType; }
    void setAnalysisType(AnalysisType type) { m_analysisType = type; }

    const std::string& caseOrComboName() const { return m_caseOrComboName; }
    void setCaseOrComboName(const std::string& name) { m_caseOrComboName = name; }

    const std::string& timestamp() const { return m_timestamp; }
    void updateTimestamp();

    // Déplacements nodaux
    void setNodeDisplacement(int nodeId, const NodeDisplacement& disp);
    const NodeDisplacement* getNodeDisplacement(int nodeId) const;
    bool hasNodeDisplacement(int nodeId) const { return getNodeDisplacement(nodeId) != nullptr; }
    NodeDisplacement nodeDisplacement(int nodeId) const {
        const auto* d = getNodeDisplacement(nodeId);
        return d ? *d : NodeDisplacement();
    }
    const std::map<int, NodeDisplacement>& allDisplacements() const { return m_displacements; }

    // Réactions nodales
    void setNodeReaction(int nodeId, const NodeReaction& react);
    const NodeReaction* getNodeReaction(int nodeId) const;
    bool hasNodeReaction(int nodeId) const { return getNodeReaction(nodeId) != nullptr; }
    NodeReaction nodeReaction(int nodeId) const {
        const auto* r = getNodeReaction(nodeId);
        return r ? *r : NodeReaction();
    }
    const std::map<int, NodeReaction>& allReactions() const { return m_reactions; }

    bool hasResults() const { return m_isValid && (!m_displacements.empty() || !m_elementResults.empty()); }

    // Résultats des éléments : toujours désignés par (famille, id TSA)
    void setElementResults(const ElementResults& res);
    const ElementResults* getElementResults(StructuralElementKind kind, int elemId) const;
    const ElementResults* getElementResults(const ElementKey& key) const;
    const std::map<ElementKey, ElementResults>& allElementResults() const { return m_elementResults; }

    // Unités des valeurs stockées (celles du script OpenSees, aucune conversion)
    const UnitSystem& units() const { return m_units; }
    void setUnits(const UnitSystem& u) { m_units = u; }

    // Résultats avancés (matrices, mapping DDL, forces brutes) — vides en mode Light
    const AdvancedResults& advanced() const { return m_advanced; }
    AdvancedResults& advanced() { return m_advanced; }

    // Pas et Incréments non-linéaires
    void addStepResults(const StepResults& step);
    const std::vector<StepResults>& allStepResults() const { return m_stepResults; }
    std::vector<StepResults>& allStepResults() { return m_stepResults; }
    const StepResults* getStepResults(int stepNumber) const;
    int stepCount() const { return static_cast<int>(m_stepResults.size()); }
    int activeStep() const { return m_activeStep; }
    void setActiveStep(int step);

    // Équilibre global & Synthèse
    GlobalEquilibrium equilibrium() const { return m_equilibrium; }
    void setEquilibrium(const GlobalEquilibrium& eq) { m_equilibrium = eq; }

    ResultsSummary summary() const { return m_summary; }
    void computeSummary();

    // Métadonnées d'exécution et traçabilité normative
    const AnalysisExecutionMetadata& executionMetadata() const noexcept { return m_executionMetadata; }
    AnalysisExecutionMetadata& executionMetadata() noexcept { return m_executionMetadata; }
    void setExecutionMetadata(const AnalysisExecutionMetadata& meta) { m_executionMetadata = meta; }

    // Disponibilité des catégories de résultats (renseignée par AnalysisManager)
    const ResultAvailability& availability() const { return m_availability; }
    void setAvailability(const ResultAvailability& a) { m_availability = a; }
    /// Disponibilité constatée sur les données présentes (aucune catégorie vide n'est annoncée).
    ResultAvailability availabilityFromData() const;

    // Courbes N, V, M et déformée des barres d'un calcul plan (vide pour un calcul 3D)
    const std::map<ElementKey, PlanarMemberCurves>& planarCurves() const { return m_planarCurves; }
    void setPlanarCurves(const PlanarMemberCurves& c) { m_planarCurves[c.key] = c; }

    // Résultats propres au moteur
    const std::vector<EngineResultTable>& engineTables() const { return m_engineTables; }
    void addEngineTable(const EngineResultTable& t) { m_engineTables.push_back(t); }

    // Maillage numérique réellement transmis au moteur (vide si le moteur ne le fournit pas)
    const SolverMesh& solverMesh() const { return m_solverMesh; }
    void setSolverMesh(SolverMesh mesh) { m_solverMesh = std::move(mesh); }

    // Journal d'analyse
    void appendLog(const std::string& line) { m_journalLog += line + "\n"; }
    const std::string& journalLog() const { return m_journalLog; }
    void clearLog() { m_journalLog.clear(); }

private:
    bool m_isValid = false;
    AnalysisType m_analysisType = AnalysisType::LinearStatic;
    std::string m_caseOrComboName;
    std::string m_timestamp;
    std::string m_journalLog;
    AnalysisExecutionMetadata m_executionMetadata;

    std::map<int, NodeDisplacement> m_displacements;
    std::map<int, NodeReaction> m_reactions;
    std::map<ElementKey, ElementResults> m_elementResults;
    UnitSystem m_units;
    AdvancedResults m_advanced;
    ResultAvailability m_availability;
    std::vector<EngineResultTable> m_engineTables;
    std::map<ElementKey, PlanarMemberCurves> m_planarCurves;
    SolverMesh m_solverMesh;
    std::vector<StepResults> m_stepResults;

    int m_activeStep = -1;
    std::map<int, NodeDisplacement> m_finalDisplacements;
    std::map<int, NodeReaction> m_finalReactions;
    std::map<ElementKey, ElementResults> m_finalElementResults;

    GlobalEquilibrium m_equilibrium;
    ResultsSummary m_summary;
};

} // namespace TSA::Analysis
