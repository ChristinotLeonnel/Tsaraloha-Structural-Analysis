#include "NDCDocumentModel.h"
#include <QDate>
#include <QTextStream>

namespace TSA::NDC
{

NDCDocument::NDCDocument()
{
    date = QDate::currentDate().toString("dd/MM/yyyy");
}

QString NDCDocument::toHtml() const
{
    QString html;
    QTextStream ts(&html);

    ts << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n";
    ts << "<title>" << projectTitle << " — Note de Calcul</title>\n";
    ts << "<style>\n";
    ts << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; "
          "line-height: 1.5; color: #222; max-width: 900px; margin: 0 auto; padding: 25px 30px; background-color: #fff; }\n";
    ts << "  .header-box { border-bottom: 3px solid #1a56db; padding-bottom: 20px; margin-bottom: 30px; }\n";
    ts << "  .header-title { font-size: 26px; font-weight: bold; color: #1a56db; margin: 0 0 8px 0; }\n";
    ts << "  .meta-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; font-size: 13px; color: #555; background: #f8fafc; padding: 12px; border-radius: 6px; border: 1px solid #e2e8f0; }\n";
    ts << "  h2 { font-size: 19px; color: #0f172a; border-bottom: 1px solid #cbd5e1; padding-bottom: 6px; margin-top: 35px; }\n";
    ts << "  h3 { font-size: 15px; color: #334155; margin-top: 20px; margin-bottom: 8px; }\n";
    ts << "  p { font-size: 13.5px; margin: 6px 0; text-align: justify; }\n";
    ts << "  .kv-list { margin: 10px 0; padding-left: 0; list-style: none; font-size: 13px; }\n";
    ts << "  .kv-list li { margin: 4px 0; }\n";
    ts << "  .kv-key { font-weight: 600; color: #475569; display: inline-block; width: 260px; }\n";
    ts << "  .kv-val { color: #0f172a; font-family: Consolas, monospace; }\n";
    ts << "  table { width: 100%; border-collapse: collapse; margin: 12px 0 20px 0; font-size: 12.5px; }\n";
    ts << "  th { background-color: #f1f5f9; color: #334155; font-weight: 600; text-align: left; padding: 7px 10px; border: 1px solid #cbd5e1; }\n";
    ts << "  td { padding: 6px 10px; border: 1px solid #e2e8f0; }\n";
    ts << "  tr:nth-child(even) td { background-color: #f8fafc; }\n";
    ts << "  .caption { font-size: 11.5px; font-style: italic; color: #64748b; margin-top: 4px; }\n";
    ts << "  .badge { display: inline-block; padding: 2px 8px; border-radius: 4px; font-weight: 600; font-size: 11px; }\n";
    ts << "  .badge-success { background: #dcfce7; color: #15803d; border: 1px solid #86efac; }\n";
    ts << "  .badge-warning { background: #fef9c3; color: #a16207; border: 1px solid #fde047; }\n";
    ts << "  @media print {\n";
    ts << "    body { max-width: 100%; padding: 0; font-size: 11pt; }\n";
    ts << "    h2 { page-break-before: always; margin-top: 20px; }\n";
    ts << "    .header-box { border-bottom-width: 2pt; }\n";
    ts << "    table { page-break-inside: avoid; }\n";
    ts << "  }\n";
    ts << "</style>\n</head>\n<body>\n";

    // En-tête officiel
    ts << "<div class=\"header-box\">\n";
    ts << "  <div class=\"header-title\">NOTE DE CALCUL DE STRUCTURE</div>\n";
    ts << "  <div style=\"font-size: 16px; color: #334155; margin-bottom: 12px;\">" << projectTitle << "</div>\n";
    ts << "  <div class=\"meta-grid\">\n";
    ts << "    <div><strong>Auteur :</strong> " << author << "</div>\n";
    ts << "    <div><strong>Organisme :</strong> " << organization << "</div>\n";
    ts << "    <div><strong>Date d'émission :</strong> " << date << "</div>\n";
    ts << "    <div><strong>Normes de référence :</strong> " << standardReference << "</div>\n";
    ts << "    <div style=\"grid-column: span 2;\"><strong>Système d'analyse :</strong> " << softwareVersion << "</div>\n";
    ts << "  </div>\n";
    ts << "</div>\n";

    // Chapitres
    for (const auto& ch : chapters)
    {
        ts << "<h2 id=\"chap_" << ch.number << "\">" << ch.number << ". " << ch.title << "</h2>\n";

        for (size_t s = 0; s < ch.sections.size(); ++s)
        {
            const auto& sec = ch.sections[s];
            if (!sec.title.isEmpty())
            {
                ts << "<h3 id=\"sec_" << ch.number << "_" << (s + 1) << "\">"
                   << ch.number << "." << (s + 1) << " " << sec.title << "</h3>\n";
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

            for (const auto& tbl : sec.tables)
            {
                ts << "<table>\n<thead>\n<tr>\n";
                for (const auto& h : tbl.headers)
                {
                    ts << "  <th>" << h << "</th>\n";
                }
                ts << "</tr>\n</thead>\n<tbody>\n";

                for (const auto& row : tbl.rows)
                {
                    ts << "<tr>\n";
                    for (const auto& cell : row)
                    {
                        ts << "  <td>" << cell << "</td>\n";
                    }
                    ts << "</tr>\n";
                }
                ts << "</tbody>\n</table>\n";
                if (!tbl.caption.isEmpty())
                {
                    ts << "<div class=\"caption\">" << tbl.caption << "</div>\n";
                }
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
    ts << "                    NOTE DE CALCUL DE STRUCTURE — " << projectTitle << "\n";
    ts << "================================================================================\n";
    ts << "Auteur       : " << author << "\n";
    ts << "Date         : " << date << "\n";
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
        }
        ts << "\n";
    }

    return txt;
}

} // namespace TSA::NDC
