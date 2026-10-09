#include "ResultsValidityGuard.h"
#include "ResultsModel.h"
#include "../Diagnostics/Logger.h"

namespace TSA::Analysis
{

ResultsValidityGuard::ResultsValidityGuard(TSA::Model::Model* model)
{
    setModel(model);
}

ResultsValidityGuard::~ResultsValidityGuard()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void ResultsValidityGuard::setModel(TSA::Model::Model* model)
{
    if (m_model == model)
        return;
    if (m_model)
    {
        m_model->removeObserver(this);
    }
    m_model = model;
    if (m_model)
    {
        m_model->addObserver(this);
    }
}

void ResultsValidityGuard::trackResults(const std::shared_ptr<ResultsModel>& results)
{
    trackResults(results, m_model ? m_model->revision() : 0);
}

void ResultsValidityGuard::trackResults(const std::shared_ptr<ResultsModel>& results, std::uint64_t analyzedRevision)
{
    m_results = results;
    m_analyzedRevision = analyzedRevision;
    onModelEdited();   // modèle modifié pendant le calcul : invalidation immédiate
}

void ResultsValidityGuard::clearResults()
{
    m_results.reset();
}

bool ResultsValidityGuard::resultsUpToDate() const
{
    return m_results && m_results->isValid() && m_model && m_model->revision() == m_analyzedRevision;
}

void ResultsValidityGuard::onModelEdited()
{
    // Appelé à chaque notification : ne travailler qu'au premier écart de révision.
    if (!m_results || !m_results->isValid() || !m_model || m_model->revision() == m_analyzedRevision)
        return;

    m_results->invalidate();
    TSA_LOG_INFO("Analysis", "ResultsInvalidated",
                 "Modèle modifié depuis l'analyse : résultats marqués obsolètes (recalcul nécessaire).");
    if (m_onStale)
    {
        m_onStale();
    }
}

} // namespace TSA::Analysis
