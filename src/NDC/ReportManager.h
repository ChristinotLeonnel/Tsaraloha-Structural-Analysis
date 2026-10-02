#pragma once

#include "NDCDocumentModel.h"
#include "ReportConfiguration.h"
#include "ReportTemplate.h"
#include "ResultAnalyzer.h"
#include "NormativeReferenceDetector.h"

#include <memory>
#include <functional>
#include <QString>
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
 * @brief Gestionnaire central de la Note de Calcul (NDC).
 * Coordonne la configuration, la détection normative, l'extraction analytique des extrema,
 * la composition du document et l'export multiformat (PDF / HTML).
 */
class ReportManager
{
public:
    ReportManager();
    ~ReportManager() = default;

    // Attachement du modèle et des résultats
    void setModel(TSA::Model::Model* model) { m_model = model; }
    TSA::Model::Model* model() const { return m_model; }

    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results) { m_results = results; }
    std::shared_ptr<TSA::Analysis::ResultsModel> resultsModel() const { return m_results; }

    // Configuration et template
    const ReportConfiguration& configuration() const { return m_config; }
    ReportConfiguration& configuration() { return m_config; }
    void setConfiguration(const ReportConfiguration& config) { m_config = config; }
    void applyTemplate(ReportTemplateType type);

    // Support des captures d'écran 3D (Dependency Injection pour rester découplé de l'UI/OCCT)
    using SnapshotProvider = std::function<QImage(const QString& viewType, int width, int height)>;
    void setSnapshotProvider(SnapshotProvider provider) { m_snapshotProvider = std::move(provider); }
    const SnapshotProvider& snapshotProvider() const { return m_snapshotProvider; }

    // Génération
    NDCDocument generateReport();
    NDCDocument generateReport(TSA::Model::Model& model, const std::shared_ptr<TSA::Analysis::ResultsModel>& results = nullptr);

    // Export
    bool exportToPdf(const QString& filePath, QString* error = nullptr);
    bool exportToHtml(const QString& filePath, QString* error = nullptr);
    bool exportPdf(const QString& filePath, QString* error = nullptr) { return exportToPdf(filePath, error); }
    bool exportHtml(const QString& filePath, QString* error = nullptr) { return exportToHtml(filePath, error); }

    // Analyse directe
    MostStressedSummary computeExtrema() const;

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;
    ReportConfiguration m_config;
    SnapshotProvider m_snapshotProvider = nullptr;
};

} // namespace TSA::NDC
