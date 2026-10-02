#include "ReportManager.h"
#include "NDCGenerator.h"
#include "NDCExporter.h"
#include "ResultAnalyzer.h"
#include "ReportTemplate.h"
#include "../Model/Model.h"
#include "../Analysis/ResultsModel.h"

namespace TSA::NDC
{

ReportManager::ReportManager()
{
    m_config = ReportTemplate::createTemplate(ReportTemplateType::BureauEtudes);
}

void ReportManager::applyTemplate(ReportTemplateType type)
{
    m_config = ReportTemplate::createTemplate(type);
}

NDCDocument ReportManager::generateReport()
{
    if (!m_model)
    {
        return NDCDocument();
    }
    return NDCGenerator::generate(*m_model, m_results, m_config, m_snapshotProvider);
}

NDCDocument ReportManager::generateReport(TSA::Model::Model& model, const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_model = &model;
    m_results = results;
    return generateReport();
}

bool ReportManager::exportToPdf(const QString& filePath, QString* error)
{
    NDCDocument doc = generateReport();
    return NDCExporter::exportToPdf(doc, filePath, error);
}

bool ReportManager::exportToHtml(const QString& filePath, QString* error)
{
    NDCDocument doc = generateReport();
    return NDCExporter::exportToHtml(doc, filePath, error);
}

MostStressedSummary ReportManager::computeExtrema() const
{
    if (!m_model || !m_results)
    {
        return MostStressedSummary();
    }
    return ResultAnalyzer::analyzeExtrema(*m_model, m_results);
}

} // namespace TSA::NDC
