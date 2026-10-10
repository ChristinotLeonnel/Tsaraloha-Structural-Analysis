#include "NDCExporter.h"
#include "NDCTemplateData.h"
#include "../Reports/DocumentRenderer.h"
#include "../Templates/TemplateRepository.h"
#include <QFile>
#include <QTextStream>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>

namespace TSA::NDC
{

QString NDCExporter::renderHtml(const NDCDocument& doc, QStringList* diagnostics, QString* templateUsed)
{
    const auto& repo = TSA::Templates::TemplateRepository::instance();
    const QString id = doc.config.templateId.isEmpty() ? QStringLiteral("tsa.ndc.standard") : doc.config.templateId;
    const auto* entry = repo.effective(id);
    QStringList diag;
    if (!entry)
        diag << QStringLiteral("template « %1 » introuvable : générateur historique utilisé").arg(id);
    else if (entry->package.manifest().dataSchema != QLatin1String("tsa-ndc/1") || entry->package.manifest().reports.empty())
        diag << QStringLiteral("template « %1 » : schéma « tsa-ndc/1 » attendu : générateur historique utilisé").arg(id);
    else
    {
        const auto& m = entry->package.manifest();
        const auto r = TSA::Reports::DocumentRenderer::render(entry->package, m.reports.front().id, ndcTemplateData(doc));
        for (const auto& v : r.missingVariables) diag << QStringLiteral("variable absente des données : %1").arg(v);
        diag << r.warnings;
        if (r.ok())
        {
            if (diagnostics) *diagnostics = diag;
            if (templateUsed)
                *templateUsed = QStringLiteral("%1 %2 (%3%4)").arg(m.id, m.version, TSA::Templates::templateOriginName(entry->origin),
                                                                   entry->modified ? QStringLiteral(", modifié") : QString());
            return r.output;
        }
        for (const auto& e : r.errors) diag << QStringLiteral("template « %1 » : %2").arg(id, e);
        diag << QStringLiteral("générateur historique utilisé");
    }
    if (diagnostics) *diagnostics = diag;
    if (templateUsed) *templateUsed = QStringLiteral("générateur historique");
    return doc.toHtml();
}

bool NDCExporter::exportToHtml(const NDCDocument& doc, const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (error) *error = QString("Impossible d'ouvrir le fichier pour l'écriture : %1").arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << renderHtml(doc);
    file.close();
    return true;
}

bool NDCExporter::exportToPdf(const NDCDocument& doc, const QString& filePath, QString* error)
{
    try
    {
        QPdfWriter pdfWriter(filePath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setPageOrientation(QPageLayout::Portrait);
        pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
        pdfWriter.setResolution(300);

        QTextDocument textDoc;
        textDoc.setHtml(renderHtml(doc));
        textDoc.print(&pdfWriter);
        return true;
    }
    catch (const std::exception& e)
    {
        if (error) *error = QString::fromUtf8(e.what());
        return false;
    }
    catch (...)
    {
        if (error) *error = "Erreur inconnue lors de la génération du PDF.";
        return false;
    }
}

} // namespace TSA::NDC
