#pragma once

#include "../../Analysis/AnalysisTypes.h"

#include <QGroupBox>

#include <memory>
#include <optional>

class QLabel;

namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::UI
{

/// Résultats de l'élément filaire sélectionné, quel que soit le moteur qui les a produits
/// (moteur, portée, efforts aux extrémités et extremums). Masqué si le calcul ne fournit pas
/// d'efforts (ResultAvailability) ou si l'élément n'a pas été calculé.
class ElementResultsPanel : public QGroupBox
{
public:
    explicit ElementResultsPanel(QWidget* parent = nullptr);

    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);
    void showElement(const std::optional<TSA::Analysis::ElementKey>& key);
    void refresh();

private:
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;
    std::optional<TSA::Analysis::ElementKey> m_key;
    QLabel* m_text = nullptr;
};

} // namespace TSA::UI
