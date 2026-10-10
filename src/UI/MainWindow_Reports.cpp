// Documents par templates (docs/TEMPLATES.md) : l'interface fournit les données (modèle, résultats, note de
// calcul structurée) au dialogue ; la mise en page appartient aux templates.

#include "MainWindow.h"

#include "../Analysis/ResultsModel.h"
#include "../Model/Model.h"
#include "../NDC/NDCGenerator.h"
#include "../NDC/NDCTemplateData.h"
#include "../NDC/NDCViewerWidget.h"
#include "../Reports/ReportDataBuilder.h"
#include "Dialogs/ReportTemplatesDialog.h"

void MainWindow::onActionReportTemplates()
{
    if (!m_model) return;
    const TSA::NDC::ReportConfiguration config = m_ndcWidget ? m_ndcWidget->configuration() : TSA::NDC::ReportConfiguration {};
    auto provider = [this, config](const QString& schema) -> QJsonObject {
        if (schema == QLatin1String(TSA::Reports::kReportDataSchema))
        {
            TSA::Reports::ReportProjectInfo p { config.projectTitle, config.projectDescription, config.projectNumber, config.documentNumber, config.revision,
                                                config.documentStatus, config.engineerName,     config.organization,  config.clientName,     config.emissionDate };
            return TSA::Reports::ReportDataBuilder::build(*m_model, m_resultsModel.get(), p);
        }
        if (schema == QLatin1String("tsa-ndc/1")) return TSA::NDC::ndcTemplateData(TSA::NDC::NDCGenerator::generate(*m_model, m_resultsModel, config));
        return {};
    };
    TSA::UI::ReportTemplatesDialog dlg(provider, this);
    dlg.exec();
}
