#pragma once

#include "NDCDocumentModel.h"
#include <memory>

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
    static NDCDocument generate(
        const TSA::Model::Model& model,
        const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
        const QString& projectName = QString(),
        const QString& engineerName = QString()
    );
};

} // namespace TSA::NDC
