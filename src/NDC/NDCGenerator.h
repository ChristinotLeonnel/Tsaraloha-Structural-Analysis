#pragma once

#include "NDCDocumentModel.h"
#include "ReportConfiguration.h"
#include <memory>
#include <functional>
#include <QImage>

namespace TSA::Model
{
class Model;
}

namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::NDC
{

/**
 * @brief Générateur automatique de la Note de Calcul (NDC) de structure.
 * Extrait les données géométriques, matériaux, sections et charges du modèle structural TSA,
 * ainsi que les résultats éléments finis (OpenSees) pour composer un rapport technique complet,
 * rigoureux et conforme aux exigences IEEE Std 1063 et Eurocodes.
 */
class NDCGenerator
{
public:
    using SnapshotProvider = std::function<QImage(const QString& viewType, int width, int height)>;

    static NDCDocument generate(
        const TSA::Model::Model& model,
        const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
        const QString& projectName = QString(),
        const QString& engineerName = QString()
    );

    static NDCDocument generate(
        const TSA::Model::Model& model,
        const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
        const ReportConfiguration& config,
        const SnapshotProvider& snapshotProvider = nullptr
    );
};

} // namespace TSA::NDC
