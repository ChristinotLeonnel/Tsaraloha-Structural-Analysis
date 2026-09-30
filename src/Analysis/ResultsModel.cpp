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

bool GlobalEquilibrium::isBalanced(double tol) const
{
    double totalF = std::sqrt(appliedFx * appliedFx + appliedFy * appliedFy + appliedFz * appliedFz);
    if (totalF < 1e-6) return true;

    double err = std::sqrt(errorFx() * errorFx() + errorFy() * errorFy() + errorFz() * errorFz());
    return (err / totalF) <= tol;
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
    m_modalModes.clear();
    m_pushoverSteps.clear();
    m_timeHistorySteps.clear();
    m_equilibrium = GlobalEquilibrium{};
    m_summary = ResultsSummary{};
    m_journalLog.clear();
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

void ResultsModel::setElementResults(int elemId, const ElementResults& res)
{
    m_elementResults[elemId] = res;
}

const ElementResults* ResultsModel::getElementResults(int elemId) const
{
    auto it = m_elementResults.find(elemId);
    return it != m_elementResults.end() ? &it->second : nullptr;
}

void ResultsModel::addModalMode(const ModalMode& mode)
{
    m_modalModes.push_back(mode);
}

const ModalMode* ResultsModel::getModalMode(int modeNumber) const
{
    for (const auto& m : m_modalModes)
    {
        if (m.modeNumber == modeNumber) return &m;
    }
    return nullptr;
}

void ResultsModel::addPushoverStep(const PushoverStep& step)
{
    m_pushoverSteps.push_back(step);
}

void ResultsModel::addTimeHistoryStep(const TimeHistoryStep& step)
{
    m_timeHistorySteps.push_back(step);
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
    for (const auto& [elemId, res] : m_elementResults)
    {
        double maxN = res.maxNormalForce();
        if (maxN > m_summary.maxTension)
        {
            m_summary.maxTension = maxN;
            m_summary.maxTensionElementId = elemId;
        }

        double minN = res.minNormalForce();
        if (minN < m_summary.maxCompression)
        {
            m_summary.maxCompression = minN;
            m_summary.maxCompressionElementId = elemId;
        }

        double maxM = res.maxBendingMoment();
        if (maxM > m_summary.maxBendingMoment)
        {
            m_summary.maxBendingMoment = maxM;
            m_summary.maxBendingMomentElementId = elemId;
        }
    }

    // 4. Modal
    if (!m_modalModes.empty())
    {
        m_summary.fundamentalPeriod = m_modalModes.front().period;
        m_summary.fundamentalFrequency = m_modalModes.front().frequency;
    }
}

} // namespace TSA::Analysis
