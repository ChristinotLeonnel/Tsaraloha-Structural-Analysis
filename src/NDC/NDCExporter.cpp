#include "NDCExporter.h"
#include <QFile>
#include <QTextStream>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>

namespace TSA::NDC
{

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
    out << doc.toHtml();
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
        textDoc.setHtml(doc.toHtml());
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
