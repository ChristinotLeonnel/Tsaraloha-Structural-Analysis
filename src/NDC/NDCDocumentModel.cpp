#include "NDCDocumentModel.h"
#include <QDate>
#include <QFile>
#include <QTextStream>
#include <cmath>

namespace TSA::NDC
{

NDCDocument::NDCDocument()
{
    if (config.emissionDate.isEmpty())
    {
        config.emissionDate = QDate::currentDate().toString("dd/MM/yyyy");
    }
}

std::vector<NDCFigure> NDCDocument::allFigures() const
{
    std::vector<NDCFigure> res;
    for (const auto& ch : chapters)
    {
        for (const auto& sec : ch.sections)
        {
            for (const auto& fig : sec.figures)
            {
                res.push_back(fig);
            }
        }
    }
    return res;
}

std::vector<NDCTable> NDCDocument::allTables() const
{
    std::vector<NDCTable> res;
    for (const auto& ch : chapters)
    {
        for (const auto& sec : ch.sections)
        {
            for (const auto& tbl : sec.tables)
            {
                res.push_back(tbl);
            }
        }
    }
    return res;
}

QString NDCDocument::getTsaLogoSvg()
{
    QFile file(":/icons/TSA.svg");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString::fromUtf8(file.readAll());
    }
    return QStringLiteral("<svg width=\"80\" height=\"80\" viewBox=\"0 0 100 100\"><circle cx=\"50\" cy=\"50\" r=\"45\" fill=\"#1a56db\"/><text x=\"50\" y=\"58\" font-family=\"Arial\" font-size=\"24\" font-weight=\"bold\" fill=\"white\" text-anchor=\"middle\">TSA</text></svg>");
}

QString NDCDocument::toHtml() const
{
    QString html;
    QTextStream ts(&html);

    QString pageOrientationStr = (config.pageOrientation == PageOrientation::Landscape) ? "landscape" : "portrait";
    QString pageSizeStr = (config.pageFormat == PageFormat::A3) ? "A3" : "A4";

    ts << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n";
    ts << "<title>" << config.projectTitle << " — Note de Calcul</title>\n";
    ts << "<style>\n";
    ts << "  @page {\n";
    ts << "    size: " << pageSizeStr << " " << pageOrientationStr << ";\n";
    ts << "    margin: " << config.marginMmTop << "mm " << config.marginMmRight << "mm "
       << config.marginMmBottom << "mm " << config.marginMmLeft << "mm;\n";
    ts << "  }\n";
    ts << "  body {\n";
    ts << "    font-family: " << config.fontFamily << ";\n";
    ts << "    font-size: " << config.baseFontSizePt << "pt;\n";
    ts << "    line-height: 1.5;\n";
    ts << "    color: #1e293b;\n";
    ts << "    background-color: #ffffff;\n";
    ts << "    margin: 0 auto;\n";
    ts << "    padding: 0;\n";
    ts << "  }\n";
    ts << "  .cover-container {\n";
    ts << "    min-height: 92vh;\n";
    ts << "    display: flex;\n";
    ts << "    flex-direction: column;\n";
    ts << "    justify-content: space-between;\n";
    ts << "    border: 3px solid " << config.primaryColor << ";\n";
    ts << "    padding: 35px;\n";
    ts << "    box-sizing: border-box;\n";
    ts << "    page-break-after: always;\n";
    ts << "  }\n";
    ts << "  .cover-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid #e2e8f0; padding-bottom: 20px; }\n";
    ts << "  .cover-brand { font-size: 20pt; font-weight: 800; letter-spacing: 1.5px; color: " << config.primaryColor << "; }\n";
    ts << "  .cover-logo { width: 75px; height: 75px; }\n";
    ts << "  .cover-title-box { text-align: center; margin: 40px 0; }\n";
    ts << "  .cover-main-title { font-size: 26pt; font-weight: 800; color: #0f172a; text-transform: uppercase; margin-bottom: 12px; letter-spacing: 0.5px; }\n";
    ts << "  .cover-sub-title { font-size: 16pt; color: #475569; font-weight: 600; margin-bottom: 20px; }\n";
    ts << "  .cover-status-badge { display: inline-block; padding: 6px 18px; border-radius: 20px; background-color: #dbeafe; color: #1e40af; font-weight: 700; font-size: 12pt; border: 1px solid #bfdbfe; }\n";
    ts << "  .cover-meta-grid {\n";
    ts << "    display: grid;\n";
    ts << "    grid-template-columns: 1fr 1fr;\n";
    ts << "    gap: 15px;\n";
    ts << "    background: #f8fafc;\n";
    ts << "    padding: 22px;\n";
    ts << "    border-radius: 8px;\n";
    ts << "    border: 1px solid #cbd5e1;\n";
    ts << "    font-size: 10.5pt;\n";
    ts << "  }\n";
    ts << "  .cover-meta-item strong { color: #334155; display: inline-block; width: 180px; }\n";
    ts << "  .cover-footer { border-top: 1px solid #e2e8f0; padding-top: 15px; font-size: 9.5pt; color: #64748b; display: flex; justify-content: space-between; }\n";
    ts << "  .toc-box { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 25px; margin: 30px 0; page-break-after: always; }\n";
    ts << "  .toc-title { font-size: 18pt; font-weight: 700; color: #0f172a; border-bottom: 2px solid " << config.primaryColor << "; padding-bottom: 8px; margin-bottom: 18px; }\n";
    ts << "  .toc-item { margin: 6px 0; font-size: 11pt; }\n";
    ts << "  .toc-item a { text-decoration: none; color: #1e293b; display: flex; justify-content: space-between; }\n";
    ts << "  .toc-item a:hover { color: " << config.primaryColor << "; }\n";
    ts << "  .toc-sec { margin-left: 20px; font-size: 10pt; color: #475569; }\n";
    ts << "  h1, h2 { color: #0f172a; border-bottom: 2px solid " << config.primaryColor << "; padding-bottom: 6px; margin-top: 35px; page-break-after: avoid; }\n";
    ts << "  h1 { font-size: 18pt; }\n";
    ts << "  h2 { font-size: 15pt; border-bottom: 1px solid #cbd5e1; }\n";
    ts << "  h3 { font-size: 12.5pt; color: #334155; margin-top: 20px; margin-bottom: 8px; page-break-after: avoid; }\n";
    ts << "  p { margin: 8px 0; text-align: justify; }\n";
    ts << "  .kv-list { margin: 10px 0; padding-left: 0; list-style: none; font-size: 10.5pt; }\n";
    ts << "  .kv-list li { margin: 5px 0; padding: 3px 0; border-bottom: 1px dotted #e2e8f0; }\n";
    ts << "  .kv-key { font-weight: 600; color: #475569; display: inline-block; width: 280px; }\n";
    ts << "  .kv-val { color: #0f172a; font-family: 'Consolas', 'Courier New', monospace; font-weight: 600; }\n";
    ts << "  table { width: 100%; border-collapse: collapse; margin: 15px 0 25px 0; font-size: 10pt; page-break-inside: avoid; }\n";
    ts << "  th { background-color: #f1f5f9; color: #1e293b; font-weight: 700; text-align: left; padding: 8px 10px; border: 1px solid #cbd5e1; }\n";
    ts << "  td { padding: 7px 10px; border: 1px solid #e2e8f0; }\n";
    ts << "  tr:nth-child(even) td { background-color: #f8fafc; }\n";
    ts << "  .caption { font-size: 9.5pt; font-style: italic; color: #64748b; margin-top: 6px; margin-bottom: 15px; text-align: center; }\n";
    ts << "  .figure-box { text-align: center; margin: 25px 0; page-break-inside: avoid; }\n";
    ts << "  .figure-img { max-width: 100%; border-radius: 6px; border: 1px solid #cbd5e1; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }\n";
    ts << "  .badge { display: inline-block; padding: 2px 8px; border-radius: 4px; font-weight: 700; font-size: 9pt; }\n";
    ts << "  .badge-success { background: #dcfce7; color: #15803d; border: 1px solid #86efac; }\n";
    ts << "  .badge-warning { background: #fef9c3; color: #a16207; border: 1px solid #fde047; }\n";
    ts << "  .badge-danger { background: #fee2e2; color: #b91c1c; border: 1px solid #fca5a5; }\n";
    ts << "  .badge-info { background: #e0f2fe; color: #0369a1; border: 1px solid #bae6fd; }\n";
    ts << "  @media print {\n";
    ts << "    body { max-width: 100%; padding: 0; }\n";
    ts << "    .cover-container { min-height: 100vh; }\n";
    ts << "    h1 { page-break-before: always; }\n";
    ts << "    table, .figure-box { page-break-inside: avoid; }\n";
    ts << "  }\n";
    ts << "</style>\n</head>\n<body>\n";

    // =========================================================================
    // 1. PAGE DE COUVERTURE OFFICIELLE (SI ACTIVÉE)
    // =========================================================================
    if (config.includeCoverPage)
    {
        ts << "<div class=\"cover-container\">\n";
        ts << "  <div class=\"cover-header\">\n";
        ts << "    <div>\n";
        ts << "      <div class=\"cover-brand\">TSARALOHA STRUCTURAL ANALYSIS</div>\n";
        ts << "      <div style=\"font-size: 10pt; color: #64748b;\">Logiciel d'Ingénierie Structurale & Calcul aux Éléments Finis</div>\n";
        ts << "    </div>\n";
        if (config.showTsaLogo)
        {
            ts << "    <div class=\"cover-logo\">" << getTsaLogoSvg() << "</div>\n";
        }
        ts << "  </div>\n";

        ts << "  <div class=\"cover-title-box\">\n";
        ts << "    <div class=\"cover-main-title\">NOTE DE CALCUL DE STRUCTURE</div>\n";
        ts << "    <div class=\"cover-sub-title\">" << config.projectTitle << "</div>\n";
        if (!config.projectDescription.isEmpty())
        {
            ts << "    <p style=\"font-size: 11pt; color: #64748b; max-width: 700px; margin: 0 auto 15px auto;\">"
               << config.projectDescription << "</p>\n";
        }
        ts << "    <div class=\"cover-status-badge\">" << config.documentStatus << "</div>\n";
        ts << "  </div>\n";

        ts << "  <div class=\"cover-meta-grid\">\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Projet N° :</strong> " << config.projectNumber << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Document N° :</strong> " << config.documentNumber << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Indice de Révision :</strong> " << config.revision << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Date d'Émission :</strong> " << config.emissionDate << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Auteur / Ingénieur :</strong> " << config.engineerName << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Bureau d'Études :</strong> " << config.organization << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Maître d'Ouvrage :</strong> " << config.clientName << "</div>\n";
        ts << "    <div class=\"cover-meta-item\"><strong>Normes de Référence :</strong> " << standardReference << "</div>\n";
        ts << "    <div style=\"grid-column: span 2; margin-top: 6px; padding-top: 8px; border-top: 1px dashed #cbd5e1;\">\n";
        ts << "      <strong>Plateforme de Calcul :</strong> " << softwareVersion << "\n";
        ts << "    </div>\n";
        ts << "  </div>\n";

        ts << "  <div class=\"cover-footer\">\n";
        ts << "    <div>TSA v1.0.0 — Conforme aux exigences IEEE Std 1063 & Eurocodes</div>\n";
        ts << "    <div>Document certifié d'analyse structurale</div>\n";
        ts << "  </div>\n";
        ts << "</div>\n";
    }

    // =========================================================================
    // 2. SOMMAIRE & TABLES DES MATIÈRES (SI ACTIVÉES)
    // =========================================================================
    if (config.includeToc && !chapters.empty())
    {
        ts << "<div class=\"toc-box\">\n";
        ts << "  <div class=\"toc-title\">TABLE DES MATIÈRES</div>\n";
        for (const auto& ch : chapters)
        {
            ts << "  <div class=\"toc-item\"><strong><a href=\"#chap_" << ch.number << "\">"
               << ch.number << ". " << ch.title << "</a></strong></div>\n";
            for (size_t s = 0; s < ch.sections.size(); ++s)
            {
                const auto& sec = ch.sections[s];
                if (sec.title.isEmpty()) continue;
                ts << "  <div class=\"toc-item toc-sec\"><a href=\"#sec_" << ch.number << "_" << (s + 1) << "\">"
                   << ch.number << "." << (s + 1) << " " << sec.title << "</a></div>\n";
            }
        }

        // Liste des figures si demandé
        auto figures = allFigures();
        if (config.includeLof && !figures.empty())
        {
            ts << "  <div class=\"toc-title\" style=\"margin-top: 30px;\">LISTE DES FIGURES</div>\n";
            for (const auto& fig : figures)
            {
                ts << "  <div class=\"toc-item\"><a href=\"#fig_" << fig.number << "\">Figure "
                   << fig.number << " — " << fig.caption << "</a></div>\n";
            }
        }

        // Liste des tableaux si demandé
        auto tables = allTables();
        if (config.includeLot && !tables.empty())
        {
            ts << "  <div class=\"toc-title\" style=\"margin-top: 30px;\">LISTE DES TABLEAUX</div>\n";
            for (const auto& tbl : tables)
            {
                ts << "  <div class=\"toc-item\"><a href=\"#tbl_" << tbl.number << "\">Tableau "
                   << tbl.number << " — " << tbl.caption << "</a></div>\n";
            }
        }
        ts << "</div>\n";
    }

    // =========================================================================
    // 3. CORPS TECHNIQUE DE LA NOTE DE CALCUL
    // =========================================================================
    for (const auto& ch : chapters)
    {
        ts << "<h1 id=\"chap_" << ch.number << "\">" << ch.number << ". " << ch.title << "</h1>\n";

        for (size_t s = 0; s < ch.sections.size(); ++s)
        {
            const auto& sec = ch.sections[s];
            if (!sec.title.isEmpty())
            {
                ts << "<h2 id=\"sec_" << ch.number << "_" << (s + 1) << "\">"
                   << ch.number << "." << (s + 1) << " " << sec.title << "</h2>\n";
            }

            for (const auto& p : sec.paragraphs)
            {
                ts << "<p>" << p << "</p>\n";
            }

            if (!sec.keyValues.empty())
            {
                ts << "<ul class=\"kv-list\">\n";
                for (const auto& [k, v] : sec.keyValues)
                {
                    ts << "  <li><span class=\"kv-key\">" << k << " :</span> <span class=\"kv-val\">" << v << "</span></li>\n";
                }
                ts << "</ul>\n";
            }

            // Tableaux
            for (const auto& tbl : sec.tables)
            {
                ts << "<table id=\"tbl_" << tbl.number << "\">\n<thead>\n<tr>\n";
                for (size_t hIdx = 0; hIdx < static_cast<size_t>(tbl.headers.size()); ++hIdx)
                {
                    QString align = "left";
                    if (hIdx < tbl.columnAlignments.size()) align = tbl.columnAlignments[hIdx];
                    ts << "  <th style=\"text-align: " << align << ";\">" << tbl.headers[hIdx] << "</th>\n";
                }
                ts << "</tr>\n</thead>\n<tbody>\n";

                for (const auto& row : tbl.rows)
                {
                    ts << "<tr>\n";
                    for (size_t cIdx = 0; cIdx < static_cast<size_t>(row.size()); ++cIdx)
                    {
                        QString align = "left";
                        if (cIdx < tbl.columnAlignments.size()) align = tbl.columnAlignments[cIdx];
                        ts << "  <td style=\"text-align: " << align << ";\">" << row[cIdx] << "</td>\n";
                    }
                    ts << "</tr>\n";
                }
                ts << "</tbody>\n</table>\n";
                if (!tbl.caption.isEmpty())
                {
                    ts << "<div class=\"caption\">Tableau " << tbl.number << " — " << tbl.caption << "</div>\n";
                }
            }

            // Figures
            for (const auto& fig : sec.figures)
            {
                ts << "<div class=\"figure-box\" id=\"fig_" << fig.number << "\">\n";
                if (!fig.imageBase64.isEmpty())
                {
                    ts << "  <img class=\"figure-img\" style=\"width: " << fig.widthPercent
                       << "%;\" src=\"" << fig.imageBase64 << "\" alt=\"" << fig.caption << "\">\n";
                }
                else if (!fig.localFilePath.isEmpty())
                {
                    ts << "  <img class=\"figure-img\" style=\"width: " << fig.widthPercent
                       << "%;\" src=\"file:///" << fig.localFilePath << "\" alt=\"" << fig.caption << "\">\n";
                }
                ts << "  <div class=\"caption\"><strong>Figure " << fig.number << "</strong> — " << fig.caption;
                if (!fig.elementRef.isEmpty()) ts << " (Élément : " << fig.elementRef << ")";
                if (!fig.loadCaseOrCombo.isEmpty()) ts << " [" << fig.loadCaseOrCombo << "]";
                ts << "</div>\n";
                ts << "</div>\n";
            }
        }
    }

    ts << "</body>\n</html>\n";
    return html;
}

QString NDCDocument::toPlainText() const
{
    QString txt;
    QTextStream ts(&txt);

    ts << "================================================================================\n";
    ts << "                    NOTE DE CALCUL DE STRUCTURE — " << config.projectTitle << "\n";
    ts << "================================================================================\n";
    ts << "Projet N°    : " << config.projectNumber << " | Doc N° : " << config.documentNumber << " (" << config.revision << ")\n";
    ts << "Statut       : " << config.documentStatus << "\n";
    ts << "Auteur       : " << config.engineerName << " (" << config.organization << ")\n";
    ts << "Client       : " << config.clientName << "\n";
    ts << "Date         : " << config.emissionDate << "\n";
    ts << "Normes       : " << standardReference << "\n";
    ts << "Solveur      : " << softwareVersion << "\n";
    ts << "--------------------------------------------------------------------------------\n\n";

    for (const auto& ch : chapters)
    {
        ts << ch.number << ". " << ch.title.toUpper() << "\n";
        ts << "--------------------------------------------------------------------------------\n";

        for (const auto& sec : ch.sections)
        {
            if (!sec.title.isEmpty())
            {
                ts << "  " << sec.title << "\n\n";
            }
            for (const auto& p : sec.paragraphs)
            {
                ts << "    " << p << "\n";
            }
            for (const auto& [k, v] : sec.keyValues)
            {
                ts << "    • " << k << " : " << v << "\n";
            }
            ts << "\n";

            for (const auto& tbl : sec.tables)
            {
                ts << "    [Tableau " << tbl.number << " : " << tbl.caption << "]\n";
            }
            for (const auto& fig : sec.figures)
            {
                ts << "    [Figure " << fig.number << " : " << fig.caption << "]\n";
            }
        }
        ts << "\n";
    }

    return txt;
}

} // namespace TSA::NDC
