#include "ResultsModel.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace TSA::Analysis
{

double ElementResults::maxNormalForce() const
{
    double val = std::max(startForces.N, endForces.N);
    for (const auto& s : intermediateStations)
    {
        val = std::max(val, s.N);
    }
    return val;
}

double ElementResults::minNormalForce() const
{
    double val = std::min(startForces.N, endForces.N);
    for (const auto& s : intermediateStations)
    {
        val = std::min(val, s.N);
    }
    return val;
}

double ElementResults::maxBendingMoment() const
{
    auto momMag = [](const StationForces& s) {
        return std::sqrt(s.My * s.My + s.Mz * s.Mz);
    };

    double val = std::max(momMag(startForces), momMag(endForces));
    for (const auto& s : intermediateStations)
    {
        val = std::max(val, momMag(s));
    }
    return val;
}

double ElementResults::maxShearForce() const
{
    auto shearMag = [](const StationForces& s) {
        return std::sqrt(s.Vy * s.Vy + s.Vz * s.Vz);
    };

    double val = std::max(shearMag(startForces), shearMag(endForces));
    for (const auto& s : intermediateStations)
    {
        val = std::max(val, shearMag(s));
    }
    return val;
}

double GlobalEquilibrium::relativeMomentResidual() const
{
    const double err = std::sqrt(errorMx() * errorMx() + errorMy() * errorMy() + errorMz() * errorMz());
    return momentScale > 1e-12 ? err / momentScale : 0.0;
}

bool GlobalEquilibrium::isBalanced(double tol) const
{
    double totalF = std::sqrt(appliedFx * appliedFx + appliedFy * appliedFy + appliedFz * appliedFz);
    const double err = std::sqrt(errorFx() * errorFx() + errorFy() * errorFy() + errorFz() * errorFz());
    const bool forcesOk = totalF < 1e-6 || (err / totalF) <= tol;
    return forcesOk && relativeMomentResidual() <= tol;
}

ResultsModel::ResultsModel()
{
    updateTimestamp();
}

void ResultsModel::clear()
{
    m_isValid = false;
    m_displacements.clear();
    m_reactions.clear();
    m_elementResults.clear();
    m_stepResults.clear();
    m_finalDisplacements.clear();
    m_finalReactions.clear();
    m_finalElementResults.clear();
    m_advanced = AdvancedResults{};
    m_availability = ResultAvailability{};
    m_engineTables.clear();
    m_planarCurves.clear();
    m_solverMesh = SolverMesh{};
    m_units = UnitSystem{};
    m_activeStep = -1;
    m_equilibrium = GlobalEquilibrium{};
    m_summary = ResultsSummary{};
    m_executionMetadata = AnalysisExecutionMetadata{};
    m_journalLog.clear();
}

ResultAvailability ResultsModel::availabilityFromData() const
{
    ResultAvailability a;
    a.displacements = !m_displacements.empty();
    a.reactions = !m_reactions.empty();
    a.elementForces = !m_elementResults.empty();
    a.dofMapping = m_advanced.available && !m_advanced.dofMap.empty();
    a.globalStiffness = m_advanced.available && m_advanced.hasGlobalStiffness;
    a.elementStiffness = m_advanced.available && !m_advanced.elementMatrices.empty();
    return a;
}

void ResultsModel::invalidate()
{
    m_isValid = false;
    appendLog("[TSA] Le modèle a été modifié : résultats invalidés.");
}

void ResultsModel::updateTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    m_timestamp = ss.str();
}

void ResultsModel::setNodeDisplacement(int nodeId, const NodeDisplacement& disp)
{
    m_displacements[nodeId] = disp;
}

const NodeDisplacement* ResultsModel::getNodeDisplacement(int nodeId) const
{
    auto it = m_displacements.find(nodeId);
    return it != m_displacements.end() ? &it->second : nullptr;
}

void ResultsModel::setNodeReaction(int nodeId, const NodeReaction& react)
{
    m_reactions[nodeId] = react;
}

const NodeReaction* ResultsModel::getNodeReaction(int nodeId) const
{
    auto it = m_reactions.find(nodeId);
    return it != m_reactions.end() ? &it->second : nullptr;
}

void ResultsModel::setElementResults(const ElementResults& res)
{
    m_elementResults[res.key()] = res;
}

const ElementResults* ResultsModel::getElementResults(StructuralElementKind kind, int elemId) const
{
    return getElementResults(ElementKey{ kind, elemId });
}

const ElementResults* ResultsModel::getElementResults(const ElementKey& key) const
{
    auto it = m_elementResults.find(key);
    return it != m_elementResults.end() ? &it->second : nullptr;
}

std::size_t AdvancedResults::memoryBytes() const
{
    std::size_t bytes = kGlobal.memoryBytes();
    bytes += dofMap.equations.size() * sizeof(DofEquation);
    bytes += (loadVector.size() + displacementVector.size()) * sizeof(double);
    for (const auto& [k, m] : elementMatrices)
        bytes += (m.kBasic.data.size() + m.kLocal.data.size() + m.kGlobal.data.size()) * sizeof(double);
    for (const auto& [k, f] : elementForces)
        bytes += (f.local.size() + f.global.size() + f.basic.size()) * sizeof(double);
    return bytes;
}

std::vector<double> AdvancedResults::globalDisplacementVector(const std::map<int, NodeDisplacement>& displacements) const
{
    std::vector<double> u(static_cast<std::size_t>(dofMap.equationCount()), 0.0);
    for (const auto& eq : dofMap.equations)
    {
        auto it = displacements.find(eq.nodeId);
        if (it == displacements.end()) continue;
        const NodeDisplacement& d = it->second;
        const double comp[6] = { d.ux, d.uy, d.uz, d.rx, d.ry, d.rz };
        if (eq.dof >= 0 && eq.dof < 6)
            u[eq.equation] = comp[eq.dof];
    }
    return u;
}

void ResultsModel::addStepResults(const StepResults& step)
{
    m_stepResults.push_back(step);
}

const StepResults* ResultsModel::getStepResults(int stepNumber) const
{
    for (const auto& s : m_stepResults)
    {
        if (s.stepNumber == stepNumber) return &s;
    }
    return nullptr;
}

void ResultsModel::setActiveStep(int step)
{
    if (step < 0 || step >= static_cast<int>(m_stepResults.size()))
    {
        m_activeStep = -1;
        if (!m_finalDisplacements.empty()) m_displacements = m_finalDisplacements;
        if (!m_finalReactions.empty()) m_reactions = m_finalReactions;
        if (!m_finalElementResults.empty()) m_elementResults = m_finalElementResults;
        computeSummary();
        return;
    }

    if (m_finalDisplacements.empty() && !m_displacements.empty())
    {
        m_finalDisplacements = m_displacements;
        m_finalReactions = m_reactions;
        m_finalElementResults = m_elementResults;
    }

    m_activeStep = step;
    const auto& s = m_stepResults[step];
    m_displacements = s.displacements;
    m_reactions = s.reactions;
    m_elementResults = s.elementResults;
    computeSummary();
}

void ResultsModel::computeSummary()
{
    m_summary = ResultsSummary{};

    // 1. Déplacement maximal
    for (const auto& [nodeId, disp] : m_displacements)
    {
        double mag = disp.translationMagnitude();
        if (mag > m_summary.maxDisplacement)
        {
            m_summary.maxDisplacement = mag;
            m_summary.maxDisplacementNodeId = nodeId;
        }
    }

    // 2. Réaction maximale
    for (const auto& [nodeId, r] : m_reactions)
    {
        double mag = r.forceMagnitude();
        if (mag > m_summary.maxReactionForce)
        {
            m_summary.maxReactionForce = mag;
            m_summary.maxReactionNodeId = nodeId;
        }
    }

    // 3. Efforts maximaux dans les éléments
    for (const auto& [key, res] : m_elementResults)
    {
        double maxN = res.maxNormalForce();
        if (maxN > m_summary.maxTension)
        {
            m_summary.maxTension = maxN;
            m_summary.maxTensionElementId = key.id;
            m_summary.maxTensionElementKind = key.kind;
        }

        double minN = res.minNormalForce();
        if (minN < m_summary.maxCompression)
        {
            m_summary.maxCompression = minN;
            m_summary.maxCompressionElementId = key.id;
            m_summary.maxCompressionElementKind = key.kind;
        }

        double maxM = res.maxBendingMoment();
        if (maxM > m_summary.maxBendingMoment)
        {
            m_summary.maxBendingMoment = maxM;
            m_summary.maxBendingMomentElementId = key.id;
            m_summary.maxBendingMomentElementKind = key.kind;
        }
    }

}

} // namespace TSA::Analysis

namespace TSA::Analysis
{

namespace
{
void mergeElementMap(std::map<ElementKey, ElementResults>& results, const std::map<ElementKey, std::vector<std::pair<ElementKey, double>>>& parts)
{
    for (const auto& [parent, list] : parts)
    {
        std::vector<const ElementResults*> pieces;
        for (const auto& [key, x0] : list)
        {
            auto it = results.find(key);
            if (it == results.end()) break;
            pieces.push_back(&it->second);
        }
        if (pieces.size() == list.size() && !pieces.empty())
        {
            ElementResults merged;
            merged.kind = parent.kind;
            merged.elementId = parent.id;
            merged.opsTag = pieces.front()->opsTag;
            double L = 0;
            for (const auto* p : pieces) L += p->length;
            merged.length = L;
            merged.startForces = pieces.front()->startForces;
            merged.startForces.position = 0.0;
            for (std::size_t k = 0; k < pieces.size(); ++k)
            {
                const double x0 = list[k].second;
                if (k > 0)
                {
                    StationForces s = pieces[k]->startForces;
                    s.position = x0;
                    merged.intermediateStations.push_back(s);
                }
                for (StationForces s : pieces[k]->intermediateStations)
                {
                    s.position += x0;
                    merged.intermediateStations.push_back(s);
                }
                if (k + 1 < pieces.size())
                {
                    StationForces s = pieces[k]->endForces;
                    s.position = x0 + pieces[k]->length;
                    merged.intermediateStations.push_back(s);
                }
            }
            merged.endForces = pieces.back()->endForces;
            merged.endForces.position = L;
            results[parent] = merged;
        }
        for (const auto& [key, x0] : list) results.erase(key);
    }
}
} // namespace

void ResultsModel::mergeSubdivisions(const std::map<ElementKey, std::vector<std::pair<ElementKey, double>>>& parts, const std::set<int>& internalNodes)
{
    if (parts.empty()) return;
    mergeElementMap(m_elementResults, parts);
    mergeElementMap(m_finalElementResults, parts);
    for (auto& step : m_stepResults)
    {
        mergeElementMap(step.elementResults, parts);
        for (int n : internalNodes) step.displacements.erase(n), step.reactions.erase(n);
    }
    for (int n : internalNodes)
    {
        m_displacements.erase(n);
        m_reactions.erase(n);
        m_finalDisplacements.erase(n);
        m_finalReactions.erase(n);
    }
    // Matrices et efforts bruts par sous-élément : propres au système résolu, non rattachables à une barre.
    std::size_t dropped = 0;
    for (const auto& [parent, list] : parts)
        for (const auto& [key, x0] : list)
            dropped += m_advanced.elementMatrices.erase(key) + m_advanced.elementForces.erase(key);
    if (dropped)
        m_advanced.warnings.push_back("barres subdivisées selon le maillage : matrices et efforts bruts des sous-éléments non présentés par barre "
                                      "(la matrice globale inclut les nœuds intermédiaires)");
    // Maillage du solveur : nœuds intermédiaires et sous-éléments rattachés au modèle.
    std::map<ElementKey, ElementKey> parentOf;
    for (const auto& [parent, list] : parts)
        for (const auto& [key, x0] : list) parentOf[key] = parent;
    for (auto& [tag, node] : m_solverMesh.nodes)
        if (internalNodes.count(node.tsaNodeId) || internalNodes.count(tag))
        {
            node.tsaNodeId = 0;
            node.role = "subdivision (maillage TSA)";
        }
    for (auto& cell : m_solverMesh.cells)
        if (cell.source)
        {
            auto it = parentOf.find(*cell.source);
            if (it != parentOf.end()) cell.source = it->second;
        }
    if (!m_solverMesh.empty()) m_solverMesh.description += " ; barres subdivisées selon le maillage TSA";
    computeSummary();
}

} // namespace TSA::Analysis
