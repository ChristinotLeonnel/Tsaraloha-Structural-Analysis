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
#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextLayout>
#include "App/ProductInfo.h"
#include <map>

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
    QString html = renderHtml(doc);
    html.remove(QRegularExpression(QStringLiteral("\\[\\[P:[A-Za-z0-9_]+\\]\\]"))); // numéros de page : PDF seulement
    out << html;
    file.close();
    return true;
}

namespace
{
const QRegularExpression& pageRefPattern()
{
    static const QRegularExpression re(QStringLiteral("\\[\\[P:([A-Za-z0-9_]+)\\]\\]"));
    return re;
}

/// Page (1 = première) de chaque ancre <a name> du document mis en page.
std::map<QString, int> anchorPages(const QTextDocument& doc, double pageHeight)
{
    std::map<QString, int> pages;
    auto* layout = doc.documentLayout();
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next())
    {
        for (auto it = b.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment f = it.fragment();
            if (!f.isValid()) continue;
            for (const QString& name : f.charFormat().anchorNames())
            {
                if (pages.count(name)) continue;
                // Position verticale de la ligne qui porte l'ancre (bloc dans une cellule de tableau compris)
                const QRectF r = layout->blockBoundingRect(b);
                const QTextLayout* tl = b.layout();
                double y = r.top();
                if (tl && tl->lineCount() > 0)
                {
                    const QTextLine line = tl->lineForTextPosition(f.position() - b.position());
                    if (line.isValid()) y += line.y();
                }
                pages[name] = int(y / pageHeight) + 1;
            }
        }
    }
    return pages;
}
} // namespace

bool NDCExporter::exportToPdf(const NDCDocument& doc, const QString& filePath, QString* error)
{
    const auto& c = doc.config;
    QPdfWriter writer(filePath);
    if (!writer.setPageSize(QPageSize(c.pageFormat == PageFormat::A3 ? QPageSize::A3 : QPageSize::A4)) ||
        !writer.setPageOrientation(c.pageOrientation == PageOrientation::Landscape ? QPageLayout::Landscape : QPageLayout::Portrait))
    {
        if (error) *error = QStringLiteral("Format de page refusé par le générateur PDF.");
        return false;
    }
    writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    writer.setResolution(300);
    writer.setTitle(QStringLiteral("%1 — Note de calcul").arg(c.projectTitle));
    writer.setCreator(QStringLiteral("%1 v%2").arg(TSA::Product::name(), QString::fromLatin1(TSA::Product::kReportVersion)));

    // Mise en page en unités logiques de 96 ppp, dessinée à 300 ppp : les largeurs du HTML (pixels) gardent leur
    // taille habituelle et les tableaux occupent toute la largeur utile.
    constexpr double kLogicalDpi = 96.0;
    const double scale = writer.resolution() / kLogicalDpi;
    const QSizeF pageMm = writer.pageLayout().fullRect(QPageLayout::Millimeter).size();
    const double bandMm = 9.0; // en-tête et pied de page
    auto px = [&](double mm) { return mm / 25.4 * kLogicalDpi; };
    const double left = px(c.marginMmLeft), top = px(c.marginMmTop + (c.enableHeader ? bandMm : 0.0));
    const double width = px(pageMm.width() - c.marginMmLeft - c.marginMmRight);
    const double height = px(pageMm.height() - c.marginMmTop - c.marginMmBottom - (c.enableHeader ? bandMm : 0.0) - (c.enableFooter ? bandMm : 0.0));

    QString html = renderHtml(doc);
    const bool hasRefs = html.contains(pageRefPattern());
    QTextDocument textDoc;
    textDoc.setDocumentMargin(0);
    textDoc.setDefaultFont(QFont(c.fontFamily.section(',', 0, 0).trimmed(), c.baseFontSizePt));
    textDoc.setPageSize(QSizeF(width, height));
    auto layoutWith = [&](const std::map<QString, int>* pages) {
        QString h = html;
        h.replace(pageRefPattern(), QStringLiteral("00"));
        if (pages)
        {
            h = html;
            QRegularExpressionMatchIterator it = pageRefPattern().globalMatch(html);
            QString out;
            qsizetype last = 0;
            while (it.hasNext())
            {
                const auto m = it.next();
                out += html.mid(last, m.capturedStart() - last);
                const auto p = pages->find(m.captured(1));
                out += p != pages->end() ? QString::number(p->second) : QStringLiteral("—");
                last = m.capturedEnd();
            }
            out += html.mid(last);
            h = out;
        }
        textDoc.setHtml(h);
        textDoc.setPageSize(QSizeF(width, height));
    };
    layoutWith(nullptr);
    if (hasRefs)
    {
        // Deux passes : numéros de page du sommaire relevés sur la première mise en page
        const auto pages = anchorPages(textDoc, height);
        layoutWith(&pages);
    }
    const int pageCount = textDoc.pageCount();

    QPainter painter;
    if (!painter.begin(&writer))
    {
        if (error) *error = QStringLiteral("Impossible d'écrire le fichier PDF : %1").arg(filePath);
        return false;
    }
    const QString headerLeft = c.customHeaderText.isEmpty() ? c.projectTitle : QStringLiteral("%1 — %2").arg(c.projectTitle, c.documentNumber);
    const QString footerLeft = doc.softwareVersion;
    QFont small(textDoc.defaultFont());
    small.setPixelSize(10); // unités logiques (96 ppp) du peintre mis à l'échelle
    for (int i = 0; i < pageCount; ++i)
    {
        if (i > 0) writer.newPage();
        painter.save();
        painter.scale(scale, scale);
        painter.save();
        painter.translate(left, top - i * height);
        textDoc.drawContents(&painter, QRectF(0, i * height, width, height));
        painter.restore();
        painter.setFont(small);
        painter.setPen(QColor(100, 116, 139));
        if (c.enableHeader && i > 0)
        {
            const double y = px(c.marginMmTop);
            painter.drawText(QRectF(left, y, width, px(bandMm) - 4), Qt::AlignLeft | Qt::AlignVCenter, headerLeft);
            painter.drawText(QRectF(left, y, width, px(bandMm) - 4), Qt::AlignRight | Qt::AlignVCenter, c.documentStatus);
            painter.drawLine(QPointF(left, y + px(bandMm) - 3), QPointF(left + width, y + px(bandMm) - 3));
        }
        if (c.enableFooter)
        {
            const double y = top + height + 3;
            painter.drawLine(QPointF(left, y), QPointF(left + width, y));
            painter.drawText(QRectF(left, y + 2, width, px(bandMm) - 4), Qt::AlignLeft | Qt::AlignVCenter, footerLeft);
            if (c.enablePagination)
                painter.drawText(QRectF(left, y + 2, width, px(bandMm) - 4), Qt::AlignRight | Qt::AlignVCenter,
                                 QStringLiteral("Page %1 / %2").arg(i + 1).arg(pageCount));
        }
        painter.restore();
    }
    painter.end();
    return true;
}

} // namespace TSA::NDC
