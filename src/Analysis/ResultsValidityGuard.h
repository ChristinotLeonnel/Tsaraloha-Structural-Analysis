#pragma once

#include "../Model/Model.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace TSA::Analysis
{

class ResultsModel;

/**
 * @brief Garantit que des résultats de calcul ne sont jamais présentés comme valides pour un
 * modèle qui a changé depuis l'analyse.
 *
 * Observe le modèle (IModelObserver::onModelEdited) et compare sa révision à celle qui a été
 * analysée. À la première divergence, les résultats sont invalidés (ResultsModel::invalidate)
 * et le rappel « résultats devenus obsolètes » est appelé une seule fois, pour que l'interface
 * retire la déformée, les diagrammes et mette à jour la note de calcul.
 *
 * Avant ce garde-fou, ResultsModel::invalidate() n'était appelé nulle part : après toute
 * modification du modèle, les résultats précédents (y compris les vérifications Eurocode de la
 * note de calcul) restaient affichés comme valides.
 */
class ResultsValidityGuard : public TSA::Model::IModelObserver
{
public:
    using StaleCallback = std::function<void()>;

    explicit ResultsValidityGuard(TSA::Model::Model* model = nullptr);
    ~ResultsValidityGuard() override;

    ResultsValidityGuard(const ResultsValidityGuard&) = delete;
    ResultsValidityGuard& operator=(const ResultsValidityGuard&) = delete;

    void setModel(TSA::Model::Model* model);

    /// À appeler juste après une analyse réussie : les résultats correspondent à la révision
    /// actuelle du modèle.
    void trackResults(const std::shared_ptr<ResultsModel>& results);
    /// Résultats calculés sur la révision analyzedRevision (capturée au lancement d'un calcul en
    /// tâche de fond) : marqués obsolètes tout de suite si le modèle a changé depuis.
    void trackResults(const std::shared_ptr<ResultsModel>& results, std::uint64_t analyzedRevision);
    void clearResults();

    void setStaleCallback(StaleCallback callback) { m_onStale = std::move(callback); }

    /// Vrai si des résultats suivis existent et correspondent encore au modèle.
    bool resultsUpToDate() const;

    void onModelEdited() override;
    void onModelDestroyed() override { m_model = nullptr; }

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<ResultsModel> m_results;
    std::uint64_t m_analyzedRevision = 0;
    StaleCallback m_onStale;
};

} // namespace TSA::Analysis
