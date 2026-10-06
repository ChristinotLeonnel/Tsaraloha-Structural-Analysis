#include "IfcStepReader.h"

#include <QFile>
#include <QString>

#include <cctype>
#include <cstdlib>

namespace TSA::BIM::Ifc
{

double StepValue::asNumber(double fallback) const
{
    if (kind == Kind::Integer || kind == Kind::Real) return number;
    if (kind == Kind::Typed && !list.empty()) return list.front().asNumber(fallback);
    return fallback;
}

std::string StepValue::asText() const
{
    if (kind == Kind::String || kind == Kind::Enum) return text;
    if (kind == Kind::Typed && !list.empty()) return list.front().asText();
    return {};
}

bool StepValue::asBool(bool fallback) const
{
    const std::string t = kind == Kind::Typed && !list.empty() ? list.front().text : text;
    if (t == "T") return true;
    if (t == "F") return false;
    return fallback;
}

std::vector<int> StepValue::refs() const
{
    std::vector<int> out;
    if (kind == Kind::Ref) out.push_back(ref);
    for (const auto& v : list)
        if (v.kind == Kind::Ref) out.push_back(v.ref);
    return out;
}

const StepValue& StepEntity::arg(std::size_t i) const
{
    static const StepValue null;
    return i < args.size() ? args[i] : null;
}

namespace
{
class Parser
{
public:
    Parser(const std::string& s, std::size_t pos) : m_s(s), m_p(pos) {}

    std::size_t pos() const { return m_p; }

    int integer()
    {
        const int v = static_cast<int>(std::strtol(m_s.c_str() + m_p, nullptr, 10));
        while (m_p < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_p]))) ++m_p;
        return v;
    }

    void skipSpace()
    {
        for (;;)
        {
            while (m_p < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_p]))) ++m_p;
            if (m_p + 1 < m_s.size() && m_s[m_p] == '/' && m_s[m_p + 1] == '*')
            {
                const auto end = m_s.find("*/", m_p + 2);
                m_p = end == std::string::npos ? m_s.size() : end + 2;
                continue;
            }
            return;
        }
    }

    bool peek(char c)
    {
        skipSpace();
        return m_p < m_s.size() && m_s[m_p] == c;
    }

    bool eat(char c)
    {
        if (!peek(c)) return false;
        ++m_p;
        return true;
    }

    std::string keyword()
    {
        skipSpace();
        const std::size_t b = m_p;
        while (m_p < m_s.size() && (std::isalnum(static_cast<unsigned char>(m_s[m_p])) || m_s[m_p] == '_')) ++m_p;
        std::string k = m_s.substr(b, m_p - b);
        for (auto& c : k) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return k;
    }

    bool list(std::vector<StepValue>& out)
    {
        if (!eat('(')) return false;
        if (eat(')')) return true;
        for (;;)
        {
            StepValue v;
            if (!value(v)) return false;
            out.push_back(std::move(v));
            if (eat(',')) continue;
            return eat(')');
        }
    }

    bool value(StepValue& v)
    {
        skipSpace();
        if (m_p >= m_s.size()) return false;
        const char c = m_s[m_p];
        if (c == '$') { ++m_p; v.kind = StepValue::Kind::Null; return true; }
        if (c == '*') { ++m_p; v.kind = StepValue::Kind::Derived; return true; }
        if (c == '#')
        {
            ++m_p;
            v.kind = StepValue::Kind::Ref;
            v.ref = static_cast<int>(std::strtol(m_s.c_str() + m_p, nullptr, 10));
            while (m_p < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_p]))) ++m_p;
            return true;
        }
        if (c == '\'') return string(v);
        if (c == '.')
        {
            const auto end = m_s.find('.', m_p + 1);
            if (end == std::string::npos) return false;
            v.kind = StepValue::Kind::Enum;
            v.text = m_s.substr(m_p + 1, end - m_p - 1);
            m_p = end + 1;
            return true;
        }
        if (c == '(')
        {
            v.kind = StepValue::Kind::List;
            return list(v.list);
        }
        if (c == '"')   // binaire : conservé en texte
        {
            const auto end = m_s.find('"', m_p + 1);
            if (end == std::string::npos) return false;
            v.kind = StepValue::Kind::String;
            v.text = m_s.substr(m_p + 1, end - m_p - 1);
            m_p = end + 1;
            return true;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+')
        {
            const std::size_t b = m_p;
            ++m_p;
            bool real = false;
            while (m_p < m_s.size())
            {
                const char d = m_s[m_p];
                if (std::isdigit(static_cast<unsigned char>(d))) ++m_p;
                else if (d == '.' || d == 'E' || d == 'e' || ((d == '-' || d == '+') && (m_s[m_p - 1] == 'E' || m_s[m_p - 1] == 'e')))
                {
                    real = true;
                    ++m_p;
                }
                else break;
            }
            v.kind = real ? StepValue::Kind::Real : StepValue::Kind::Integer;
            v.number = std::strtod(m_s.substr(b, m_p - b).c_str(), nullptr);
            return true;
        }
        if (std::isalpha(static_cast<unsigned char>(c)))
        {
            v.kind = StepValue::Kind::Typed;
            v.text = keyword();
            return list(v.list);
        }
        return false;
    }

private:
    static void appendUtf16(std::string& out, const std::u16string& u)
    {
        out += QString::fromUtf16(u.data(), static_cast<qsizetype>(u.size())).toStdString();
    }

    bool string(StepValue& v)
    {
        ++m_p;   // '
        std::string out;
        while (m_p < m_s.size())
        {
            const char c = m_s[m_p];
            if (c == '\'')
            {
                if (m_p + 1 < m_s.size() && m_s[m_p + 1] == '\'') { out += '\''; m_p += 2; continue; }
                ++m_p;
                v.kind = StepValue::Kind::String;
                v.text = std::move(out);
                return true;
            }
            if (c == '\\' && m_p + 1 < m_s.size())
            {
                if (m_s.compare(m_p, 4, "\\X2\\") == 0 || m_s.compare(m_p, 4, "\\X4\\") == 0)
                {
                    const bool x4 = m_s[m_p + 2] == '4';
                    m_p += 4;
                    const auto end = m_s.find("\\X0\\", m_p);
                    if (end == std::string::npos) return false;
                    std::u16string u;
                    const int width = x4 ? 8 : 4;
                    for (std::size_t i = m_p; i + width <= end; i += width)
                    {
                        const unsigned long cp = std::strtoul(m_s.substr(i, width).c_str(), nullptr, 16);
                        if (cp > 0xFFFF) { u += char16_t(0xD800 + ((cp - 0x10000) >> 10)); u += char16_t(0xDC00 + ((cp - 0x10000) & 0x3FF)); }
                        else u += char16_t(cp);
                    }
                    appendUtf16(out, u);
                    m_p = end + 4;
                    continue;
                }
                if (m_s.compare(m_p, 3, "\\X\\") == 0 && m_p + 5 <= m_s.size())
                {
                    appendUtf16(out, std::u16string(1, char16_t(std::strtoul(m_s.substr(m_p + 3, 2).c_str(), nullptr, 16))));
                    m_p += 5;
                    continue;
                }
                if (m_s.compare(m_p, 3, "\\S\\") == 0 && m_p + 3 < m_s.size())
                {
                    appendUtf16(out, std::u16string(1, char16_t(static_cast<unsigned char>(m_s[m_p + 3]) + 128)));
                    m_p += 4;
                    continue;
                }
                if (m_s[m_p + 1] == '\\') { out += '\\'; m_p += 2; continue; }
            }
            out += c;
            ++m_p;
        }
        return false;
    }

    const std::string& m_s;
    std::size_t m_p;
};
} // namespace

bool IfcStepReader::parse(const std::string& s, std::string* error)
{
    m_entities.clear();
    m_byType.clear();
    m_schema.clear();
    auto fail = [error](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    if (s.find("ISO-10303-21") == std::string::npos) return fail("fichier non STEP (en-tête ISO-10303-21 absent)");

    if (const auto fs = s.find("FILE_SCHEMA"); fs != std::string::npos)
    {
        const auto q1 = s.find('\'', fs);
        const auto q2 = q1 == std::string::npos ? q1 : s.find('\'', q1 + 1);
        if (q2 != std::string::npos) m_schema = s.substr(q1 + 1, q2 - q1 - 1);
    }
    const auto data = s.find("DATA;");
    if (data == std::string::npos) return fail("section DATA absente");

    Parser p(s, data + 5);
    for (;;)
    {
        p.skipSpace();
        if (p.peek('E')) break;   // ENDSEC
        if (!p.eat('#')) return fail("entité attendue près de l'octet " + std::to_string(p.pos()));
        const int id = p.integer();
        if (!p.eat('=')) return fail("« = » attendu après #" + std::to_string(id));
        StepEntity e;
        e.id = id;
        if (p.peek('('))
        {
            // Instance complexe (sous-types multiples) : conservée sous le premier type
            std::vector<StepValue> parts;
            if (!p.list(parts)) return fail("instance complexe illisible #" + std::to_string(id));
            if (!parts.empty() && parts.front().kind == StepValue::Kind::Typed)
            {
                e.type = parts.front().text;
                e.args = parts.front().list;
            }
        }
        else
        {
            e.type = p.keyword();
            if (e.type.empty() || !p.list(e.args)) return fail("entité #" + std::to_string(id) + " illisible");
        }
        if (!p.eat(';')) return fail("« ; » attendu après #" + std::to_string(id));
        m_byType[e.type].push_back(id);
        m_entities[id] = std::move(e);
    }
    return true;
}

bool IfcStepReader::parseFile(const std::string& path, std::string* error)
{
    QFile f(QString::fromStdString(path));
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = "impossible d'ouvrir « " + path + " »";
        return false;
    }
    const QByteArray bytes = f.readAll();
    return parse(std::string(bytes.constData(), static_cast<std::size_t>(bytes.size())), error);
}

const StepEntity* IfcStepReader::get(int id) const
{
    auto it = m_entities.find(id);
    return it == m_entities.end() ? nullptr : &it->second;
}

std::vector<const StepEntity*> IfcStepReader::byType(const std::string& type) const
{
    std::vector<const StepEntity*> out;
    if (auto it = m_byType.find(type); it != m_byType.end())
        for (int id : it->second) out.push_back(&m_entities.at(id));
    return out;
}

} // namespace TSA::BIM::Ifc
