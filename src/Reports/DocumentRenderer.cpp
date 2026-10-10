#include "DocumentRenderer.h"

#include <QDir>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextDocument>

namespace TSA::Reports
{

Templates::RenderResult DocumentRenderer::render(const Templates::TemplatePackage& package, const QString& reportId, const QJsonObject& data,
                                                 const QJsonObject& optionValues)
{
    return package.render(reportId, data, optionValues);
}

bool DocumentRenderer::writeHtml(const QString& html, const QString& path, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(html.toUtf8()) < 0 || !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1 : %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool DocumentRenderer::writePdf(const QString& html, const QString& path, const PageSetup& page, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(page.size.compare(QLatin1String("A3"), Qt::CaseInsensitive) == 0 ? QPageSize::A3 : QPageSize::A4));
    writer.setPageOrientation(page.landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
    writer.setPageMargins(QMarginsF(page.marginMm, page.marginMm, page.marginMm, page.marginMm), QPageLayout::Millimeter);
    writer.setResolution(300);
    writer.setCreator(QStringLiteral("Tsaraloha"));
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);
    if (!QFileInfo(path).exists() || QFileInfo(path).size() == 0)
    {
        if (error) *error = QStringLiteral("PDF non produit : %1").arg(path);
        return false;
    }
    return true;
}

QStringList DocumentRenderer::externalReferences(const QString& html)
{
    // Ressources chargées (src=, url(), <link href>) hors URI data: et ancres internes.
    static const QRegularExpression re(QStringLiteral("(?:\\bsrc\\s*=\\s*[\"']?|url\\(\\s*[\"']?|<link[^>]*href\\s*=\\s*[\"']?)\\s*((?:file|https?|ftp):[^\"'\\s)>]*|//[^\"'\\s)>]*)"),
                                       QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    auto it = re.globalMatch(html);
    while (it.hasNext())
    {
        const QString ref = it.next().captured(1);
        if (!out.contains(ref)) out << ref;
    }
    return out;
}

} // namespace TSA::Reports
