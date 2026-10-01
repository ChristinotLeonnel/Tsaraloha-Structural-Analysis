#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>

namespace TSA::Analysis
{

enum class AnalysisType
{
    LinearStatic,
    NonLinearStatic,
    Modal,
    DynamicTimeHistory,
    Pushover
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
    int elementId = 0;
    double length = 0.0;
    StationForces startForces; // Station i (x = 0)
    StationForces endForces;   // Station j (x = L)
    std::vector<StationForces> intermediateStations; // Profil discrétisé

    double maxNormalForce() const;
    double minNormalForce() const;
    double maxBendingMoment() const;
    double maxShearForce() const;
};

/**
 * @brief Caractéristiques d'un mode propre de vibration.
 */
struct ModalMode
{
    int modeNumber = 1;
    double eigenvalue = 0.0;       ///< lambda = omega^2
    double omega = 0.0;            ///< Pulsation propre (rad/s)
    double frequency = 0.0;        ///< Fréquence propre (Hz)
    double period = 0.0;           ///< Période propre (s)
    std::map<int, NodeDisplacement> shape; ///< Forme modale normalisée par nœud
};

/**
 * @brief Point de la courbe de capacité Pushover.
 */
struct PushoverStep
{
    int stepNumber = 0;
    double topDisplacement = 0.0;  ///< Déplacement au sommet (m)
    double baseShear = 0.0;        ///< Effort tranchant à la base (kN)
    double loadFactor = 0.0;
};

/**
 * @brief Pas de temps pour analyse dynamique temporelle.
 */
struct TimeHistoryStep
{
    double time = 0.0;
    std::map<int, NodeDisplacement> displacements;
};

/**
 * @brief Résultats d'un incrément / pas de calcul (analyse non-linéaire ou temporelle).
 */
struct StepResults
{
    int stepNumber = 0;
    double factorOrTime = 0.0;
    std::map<int, NodeDisplacement> displacements;
    std::map<int, NodeReaction> reactions;
    std::map<int, ElementResults> elementResults;
};

/**
 * @brief Contrôle d'équilibre statique global.
 */
struct GlobalEquilibrium
{
    double appliedFx = 0.0;
    double appliedFy = 0.0;
    double appliedFz = 0.0;
    double reactionFx = 0.0;
    double reactionFy = 0.0;
    double reactionFz = 0.0;

    double errorFx() const { return appliedFx + reactionFx; }
    double errorFy() const { return appliedFy + reactionFy; }
    double errorFz() const { return appliedFz + reactionFz; }
    bool isBalanced(double tol = 1e-3) const;
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

    double maxCompression = 0.0;
    int maxCompressionElementId = 0;

    double maxBendingMoment = 0.0;
    int maxBendingMomentElementId = 0;

    double fundamentalPeriod = 0.0;
    double fundamentalFrequency = 0.0;
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

    // Résultats des éléments
    void setElementResults(int elemId, const ElementResults& res);
    const ElementResults* getElementResults(int elemId) const;
    const std::map<int, ElementResults>& allElementResults() const { return m_elementResults; }

    // Modes propres
    void addModalMode(const ModalMode& mode);
    const std::vector<ModalMode>& modalModes() const { return m_modalModes; }
    const ModalMode* getModalMode(int modeNumber) const;

    // Pushover
    void addPushoverStep(const PushoverStep& step);
    const std::vector<PushoverStep>& pushoverSteps() const { return m_pushoverSteps; }

    // Time History
    void addTimeHistoryStep(const TimeHistoryStep& step);
    const std::vector<TimeHistoryStep>& timeHistorySteps() const { return m_timeHistorySteps; }

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

    std::map<int, NodeDisplacement> m_displacements;
    std::map<int, NodeReaction> m_reactions;
    std::map<int, ElementResults> m_elementResults;
    std::vector<ModalMode> m_modalModes;
    std::vector<PushoverStep> m_pushoverSteps;
    std::vector<TimeHistoryStep> m_timeHistorySteps;
    std::vector<StepResults> m_stepResults;

    int m_activeStep = -1;
    std::map<int, NodeDisplacement> m_finalDisplacements;
    std::map<int, NodeReaction> m_finalReactions;
    std::map<int, ElementResults> m_finalElementResults;

    GlobalEquilibrium m_equilibrium;
    ResultsSummary m_summary;
};

} // namespace TSA::Analysis
