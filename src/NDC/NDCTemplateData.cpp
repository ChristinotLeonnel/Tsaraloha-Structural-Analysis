#include "NDCTemplateData.h"

#include "App/ProductInfo.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace TSA::NDC
{

namespace
{
/// Nombre écrit comme QTextStream (notation courte, 6 chiffres significatifs) : même texte que toHtml.
QString num(double v) { return QString::number(v, 'g', 6); }

QString logoSvg()
{
    QFile file(QString::fromLatin1(TSA::Product::kIconSvg));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString::fromUtf8(file.readAll());
    return QStringLiteral("<svg width=\"80\" height=\"80\" viewBox=\"0 0 100 100\"><circle cx=\"50\" cy=\"50\" r=\"45\" fill=\"#1a56db\"/><text x=\"50\" y=\"58\" font-family=\"Arial\" font-size=\"24\" font-weight=\"bold\" fill=\"white\" text-anchor=\"middle\">TSA</text></svg>");
}

QString imageSource(const NDCFigure& fig, bool inlineLocal)
{
    if (!fig.imageBase64.isEmpty()) return fig.imageBase64;
    if (fig.localFilePath.isEmpty()) return {};
    if (inlineLocal)
    {
        QFile f(fig.localFilePath);
        const QString ext = QFileInfo(fig.localFilePath).suffix().toLower();
        const QString mime = ext == "svg" ? QStringLiteral("image/svg+xml") : ext == "jpg" || ext == "jpeg" ? QStringLiteral("image/jpeg") : QStringLiteral("image/png");
        if (f.open(QIODevice::ReadOnly)) return QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(f.readAll().toBase64()));
    }
    return QStringLiteral("file:///") + fig.localFilePath;
}

QJsonObject tableJson(const NDCTable& t)
{
    auto align = [&](qsizetype i) { return size_t(i) < t.columnAlignments.size() ? t.columnAlignments[size_t(i)] : QStringLiteral("left"); };
    QJsonArray headers, rows;
    for (qsizetype h = 0; h < t.headers.size(); ++h) headers.append(QJsonObject { { "text", t.headers[h] }, { "align", align(h) } });
    for (const auto& row : t.rows)
    {
        QJsonArray cells;
        for (qsizetype c = 0; c < row.size(); ++c) cells.append(QJsonObject { { "text", row[c] }, { "align", align(c) } });
        rows.append(QJsonObject { { "cells", cells } });
    }
    return { { "number", t.number }, { "caption", t.caption }, { "headers", headers }, { "rows", rows } };
}

QJsonObject figureJson(const NDCFigure& f, bool inlineLocal)
{
    return { { "number", f.number },
             { "caption", f.caption },
             { "src", imageSource(f, inlineLocal) },
             { "widthPercent", f.widthPercent },
             { "viewName", f.viewName },
             { "elementRef", f.elementRef },
             { "loadCaseOrCombo", f.loadCaseOrCombo } };
}
} // namespace

QJsonObject ndcTemplateData(const NDCDocument& doc, const NDCTemplateDataOptions& options)
{
    const auto& c = doc.config;
    QJsonArray chapters;
    for (const auto& ch : doc.chapters)
    {
        QJsonArray sections, toc;
        for (size_t s = 0; s < ch.sections.size(); ++s)
        {
            const auto& sec = ch.sections[s];
            QJsonArray paragraphs, kv, tables, figures;
            for (const auto& p : sec.paragraphs) paragraphs.append(p);
            for (const auto& [k, v] : sec.keyValues) kv.append(QJsonObject { { "key", k }, { "value", v } });
            for (const auto& t : sec.tables) tables.append(tableJson(t));
            for (const auto& f : sec.figures) figures.append(figureJson(f, options.inlineLocalImages));
            // Toutes les clés sont présentes (même vides) : une section ne doit jamais hériter d'une liste parente.
            const QJsonObject o { { "chapter", ch.number }, { "index", int(s + 1) }, { "title", sec.title }, { "paragraphs", paragraphs },
                                  { "hasKeyValues", !kv.isEmpty() }, { "keyValues", kv }, { "tables", tables }, { "figures", figures } };
            sections.append(o);
            if (!sec.title.isEmpty()) toc.append(QJsonObject { { "chapter", ch.number }, { "index", int(s + 1) }, { "title", sec.title } });
        }
        chapters.append(QJsonObject { { "number", ch.number }, { "title", ch.title }, { "sections", sections }, { "tocSections", toc } });
    }
    QJsonArray allFigures, allTables;
    for (const auto& f : doc.allFigures()) allFigures.append(figureJson(f, options.inlineLocalImages));
    for (const auto& t : doc.allTables()) allTables.append(tableJson(t));

    return {
        { "schema", "tsa-ndc/1" },
        { "product", QJsonObject { { "name", TSA::Product::name() }, { "reportVersion", QString::fromLatin1(TSA::Product::kReportVersion) } } },
        { "project",
          QJsonObject { { "title", c.projectTitle },
                        { "description", c.projectDescription },
                        { "number", c.projectNumber },
                        { "documentNumber", c.documentNumber },
                        { "revision", c.revision },
                        { "status", c.documentStatus },
                        { "engineer", c.engineerName },
                        { "organization", c.organization },
                        { "client", c.clientName },
                        { "address", c.organizationAddress },
                        { "email", c.contactEmail },
                        { "phone", c.contactPhone },
                        { "date", c.emissionDate },
                        { "standards", doc.standardReference },
                        { "software", doc.softwareVersion } } },
        { "page",
          QJsonObject { { "size", c.pageFormat == PageFormat::A3 ? "A3" : "A4" },
                        { "orientation", c.pageOrientation == PageOrientation::Landscape ? "landscape" : "portrait" },
                        { "marginTop", num(c.marginMmTop) },
                        { "marginRight", num(c.marginMmRight) },
                        { "marginBottom", num(c.marginMmBottom) },
                        { "marginLeft", num(c.marginMmLeft) } } },
        { "style",
          QJsonObject { { "primaryColor", c.primaryColor }, { "fontFamily", c.fontFamily }, { "baseFontSizePt", c.baseFontSizePt },
                        { "headerText", c.customHeaderText } } },
        { "show",
          QJsonObject { { "coverPage", c.includeCoverPage },
                        { "logo", c.showTsaLogo },
                        { "toc", c.includeToc && !doc.chapters.empty() },
                        { "lof", c.includeLof && !allFigures.isEmpty() },
                        { "lot", c.includeLot && !allTables.isEmpty() },
                        { "header", c.enableHeader },
                        { "footer", c.enableFooter },
                        { "pagination", c.enablePagination } } },
        { "logoSvg", c.showTsaLogo ? logoSvg() : QString() },
        { "chapters", chapters },
        { "allFigures", allFigures },
        { "allTables", allTables },
    };
}

} // namespace TSA::NDC
