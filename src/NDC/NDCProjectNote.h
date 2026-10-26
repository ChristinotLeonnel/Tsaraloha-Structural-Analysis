#pragma once

// Notes de calcul « simplifiée » et « détaillée » (ReportConfiguration::noteLayout) : chapitres construits à partir
// du modèle et des résultats réels du calcul. Toute vérification qui n'est pas implémentée est listée comme
// « non calculée » ; aucune valeur n'est inventée.

#include "NDCDocumentModel.h"
#include "NDCGenerator.h"

#include <map>
#include <memory>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class ResultsModel;
struct AnalysisExecutionMetadata;
}

namespace TSA::NDC
{

/// Contrôle topologique du modèle transmis au calcul (barres, poteaux, treillis, câbles).
struct ModelAudit
{
    std::size_t nodes = 0, members = 0, isolatedNodes = 0, zeroLength = 0, duplicateNodes = 0, duplicateMembers = 0;
    std::size_t nodesInsideMembers = 0, components = 0, supportedNodes = 0;
    double totalLength = 0.0;
};

ModelAudit auditModel(const TSA::Model::Model& model);

/// « OpenSees 3.8.0 », « OpenSees (version non lue) » : moteur et version tels qu'enregistrés avec les résultats,
/// jamais la version installée au moment de la note (BUG-111).
QString engineLabel(const TSA::Analysis::AnalysisExecutionMetadata& meta);

/// Poids propre des éléments transmis au calcul (kN) : Σ ρ · g · A · L, g = 9,81 m/s².
double selfWeightKN(const TSA::Model::Model& model);

class NDCProjectNote
{
public:
    static NDCDocument build(const TSA::Model::Model& model, const std::shared_ptr<TSA::Analysis::ResultsModel>& results,
                             const ReportConfiguration& config, const NDCGenerator::SnapshotProvider& snapshotProvider);
};

} // namespace TSA::NDC
