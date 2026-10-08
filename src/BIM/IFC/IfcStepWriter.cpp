#include "IfcStepWriter.h"
#include "App/ProductInfo.h"

#include <QDateTime>
#include <QString>

#include <cstdio>

namespace TSA::BIM::Ifc
{

int IfcStepWriter::add(std::string_view entity, const std::string& args)
{
    const int id = static_cast<int>(m_lines.size()) + 1;
    m_lines.push_back("#" + std::to_string(id) + "=" + std::string(entity) + "(" + args + ");");
    return id;
}

std::string IfcStepWriter::str(const std::string& utf8)
{
    // ISO 10303-21 : caractères imprimables ASCII ; ' doublé ; \ doublé ; le reste en \X2\HHHH\X0\ (UTF-16)
    const QString q = QString::fromStdString(utf8);
    std::string out = "'";
    bool inX2 = false;
    for (const QChar c : q)
    {
        const char16_t u = c.unicode();
        if (u >= 0x20 && u < 0x7F)
        {
            if (inX2)
            {
                out += "\\X0\\";
                inX2 = false;
            }
            if (u == '\'') out += "''";
            else if (u == '\\') out += "\\\\";
            else out += static_cast<char>(u);
        }
        else
        {
            if (!inX2)
            {
                out += "\\X2\\";
                inX2 = true;
            }
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned>(u));
            out += buf;
        }
    }
    if (inX2) out += "\\X0\\";
    return out + "'";
}

std::string IfcStepWriter::optStr(const std::string& utf8)
{
    return utf8.empty() ? "$" : str(utf8);
}

std::string IfcStepWriter::real(double v)
{
    if (v == 0.0) return "0.";
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.15G", v);
    std::string s = buf;
    const auto e = s.find('E');
    const std::string mant = e == std::string::npos ? s : s.substr(0, e);
    const std::string exp = e == std::string::npos ? "" : s.substr(e);
    // STEP impose un point décimal dans la mantisse
    return (mant.find('.') == std::string::npos ? mant + "." : mant) + exp;
}

std::string IfcStepWriter::refs(const std::vector<int>& ids)
{
    std::string out = "(";
    for (std::size_t i = 0; i < ids.size(); ++i) out += (i ? ",#" : "#") + std::to_string(ids[i]);
    return out + ")";
}

std::string IfcStepWriter::reals(const std::vector<double>& v)
{
    std::string out = "(";
    for (std::size_t i = 0; i < v.size(); ++i) out += (i ? "," : "") + real(v[i]);
    return out + ")";
}

std::string IfcStepWriter::document(const StepHeader& h) const
{
    const std::string stamp = h.timeStamp.empty()
        ? QDateTime::currentDateTime().toString(Qt::ISODate).toStdString()
        : h.timeStamp;
    std::string out;
    out.reserve(m_lines.size() * 80 + 1024);
    out += "ISO-10303-21;\nHEADER;\n";
    out += "FILE_DESCRIPTION(('ViewDefinition [" + h.viewDefinition + "]'),'2;1');\n";
    out += "FILE_NAME(" + str(h.fileName) + "," + str(stamp) + ",(" + str(h.author) + "),(" + str(h.organization)
        + "),'" + std::string(TSA::Product::kName) + " STEP writer','" + TSA::Product::kIfcOriginatingSystem + "','');\n";
    out += "FILE_SCHEMA(('" + h.schema + "'));\nENDSEC;\nDATA;\n";
    for (const auto& l : m_lines)
    {
        out += l;
        out += '\n';
    }
    out += "ENDSEC;\nEND-ISO-10303-21;\n";
    return out;
}

} // namespace TSA::BIM::Ifc
