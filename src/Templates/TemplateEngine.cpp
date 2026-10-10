#include "TemplateEngine.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace TSA::Templates
{

namespace
{

struct Node
{
    enum Kind
    {
        Text,
        Variable,
        Section,
        Inverted,
        Partial
    } kind = Text;
    QString text;      ///< texte littéral ou nom
    bool escape = true;
    int line = 1;
    std::vector<Node> children;
};

struct Parsed
{
    std::vector<Node> nodes;
    QStringList errors;
};

bool blank(QStringView s)
{
    for (QChar c : s)
        if (c != QLatin1Char(' ') && c != QLatin1Char('\t') && c != QLatin1Char('\r')) return false;
    return true;
}

Parsed parse(const QString& t)
{
    Parsed out;
    struct Frame
    {
        Node node;
        std::vector<Node>* target;
    };
    std::vector<Node> root;
    std::vector<std::unique_ptr<Frame>> stack;
    auto current = [&]() -> std::vector<Node>& { return stack.empty() ? root : stack.back()->node.children; };
    auto lineOf = [&](qsizetype pos) { return int(QStringView(t).left(pos).count(QLatin1Char('\n'))) + 1; };

    qsizetype pos = 0;
    while (pos < t.size())
    {
        const qsizetype open = t.indexOf(QStringLiteral("{{"), pos);
        if (open < 0)
        {
            current().push_back({ Node::Text, t.mid(pos) });
            break;
        }
        const bool triple = t.mid(open, 3) == QLatin1String("{{{");
        const qsizetype close = t.indexOf(triple ? QStringLiteral("}}}") : QStringLiteral("}}"), open + (triple ? 3 : 2));
        if (close < 0)
        {
            out.errors << QStringLiteral("ligne %1 : balise « {{ » non fermée").arg(lineOf(open));
            current().push_back({ Node::Text, t.mid(pos) });
            break;
        }
        const qsizetype tagEnd = close + (triple ? 3 : 2);
        QString inner = t.mid(open + (triple ? 3 : 2), close - open - (triple ? 3 : 2)).trimmed();
        QChar sigil = triple ? QLatin1Char('{') : (inner.isEmpty() ? QChar() : inner.at(0));
        if (!triple && (sigil == '#' || sigil == '^' || sigil == '/' || sigil == '!' || sigil == '>' || sigil == '&' || sigil == '='))
            inner = inner.mid(1).trimmed();
        else if (!triple)
            sigil = QChar();
        const int line = lineOf(open);

        // Règle « standalone » : balise de structure seule sur sa ligne → ligne entière supprimée.
        qsizetype textEnd = open, next = tagEnd;
        if (sigil == '#' || sigil == '^' || sigil == '/' || sigil == '!' || sigil == '>')
        {
            const qsizetype lineStart = open == 0 ? 0 : t.lastIndexOf(QLatin1Char('\n'), open - 1) + 1;   // (from = -1 : recherche depuis la fin)
            qsizetype lineEnd = t.indexOf(QLatin1Char('\n'), tagEnd);
            if (lineEnd < 0) lineEnd = t.size();
            if (lineStart >= pos && blank(QStringView(t).mid(lineStart, open - lineStart)) && blank(QStringView(t).mid(tagEnd, lineEnd - tagEnd)))
            {
                textEnd = lineStart;
                next = lineEnd < t.size() ? lineEnd + 1 : lineEnd;
            }
        }
        if (textEnd > pos) current().push_back({ Node::Text, t.mid(pos, textEnd - pos) });
        pos = next;

        if (sigil == '!') continue;
        if (sigil == '=')
        {
            out.errors << QStringLiteral("ligne %1 : changement de délimiteurs non pris en charge").arg(line);
            continue;
        }
        if (inner.isEmpty())
        {
            out.errors << QStringLiteral("ligne %1 : balise vide").arg(line);
            continue;
        }
        if (sigil == '#' || sigil == '^')
        {
            auto f = std::make_unique<Frame>();
            f->node = { sigil == '#' ? Node::Section : Node::Inverted, inner, true, line };
            stack.push_back(std::move(f));
        }
        else if (sigil == '/')
        {
            if (stack.empty() || stack.back()->node.text != inner)
            {
                out.errors << QStringLiteral("ligne %1 : fermeture « %2 » sans section ouverte correspondante%3")
                                  .arg(line)
                                  .arg(inner, stack.empty() ? QString() : QStringLiteral(" (attendu « %1 »)").arg(stack.back()->node.text));
                continue;
            }
            Node done = std::move(stack.back()->node);
            stack.pop_back();
            current().push_back(std::move(done));
        }
        else if (sigil == '>')
            current().push_back({ Node::Partial, inner, true, line });
        else
            current().push_back({ Node::Variable, inner, sigil.isNull(), line });
    }
    while (!stack.empty())
    {
        out.errors << QStringLiteral("ligne %1 : section « %2 » non fermée").arg(stack.back()->node.line).arg(stack.back()->node.text);
        Node done = std::move(stack.back()->node);
        stack.pop_back();
        current().push_back(std::move(done));
    }
    out.nodes = std::move(root);
    return out;
}

void collect(const std::vector<Node>& nodes, QStringList* names, QStringList* partials)
{
    for (const auto& n : nodes)
    {
        if ((n.kind == Node::Variable || n.kind == Node::Section || n.kind == Node::Inverted) && names && !names->contains(n.text)) names->append(n.text);
        if (n.kind == Node::Partial && partials && !partials->contains(n.text)) partials->append(n.text);
        collect(n.children, names, partials);
    }
}

bool truthy(const QJsonValue& v)
{
    switch (v.type())
    {
    case QJsonValue::Null:
    case QJsonValue::Undefined: return false;
    case QJsonValue::Bool: return v.toBool();
    case QJsonValue::Double: return v.toDouble() != 0.0;
    case QJsonValue::String: return !v.toString().isEmpty();
    case QJsonValue::Array: return !v.toArray().isEmpty();
    case QJsonValue::Object: return true;
    }
    return false;
}

QString scalarText(const QJsonValue& v, bool* ok)
{
    *ok = true;
    switch (v.type())
    {
    case QJsonValue::String: return v.toString();
    case QJsonValue::Bool: return v.toBool() ? QStringLiteral("oui") : QStringLiteral("non");
    case QJsonValue::Double:
    {
        const double d = v.toDouble();
        if (std::isfinite(d) && std::floor(d) == d && std::abs(d) < 1e15) return QString::number(qint64(d));
        return QString::number(d, 'g', 12);
    }
    case QJsonValue::Null:
    case QJsonValue::Undefined: return {};
    default: *ok = false; return {};
    }
}

class Renderer
{
public:
    Renderer(const TemplateEngine::PartialResolver& partials, const RenderOptions& options, RenderResult& result)
        : m_partials(partials)
        , m_options(options)
        , m_result(result)
    {
    }

    void run(const std::vector<Node>& nodes, std::vector<QJsonValue>& ctx, int depth)
    {
        for (const auto& n : nodes)
        {
            if (m_result.output.size() > m_options.maxOutputChars)
            {
                if (!m_overflow) m_result.errors << QStringLiteral("sortie trop volumineuse : rendu interrompu");
                m_overflow = true;
                return;
            }
            switch (n.kind)
            {
            case Node::Text: m_result.output += n.text; break;
            case Node::Variable:
            {
                bool found = false;
                const QJsonValue v = lookup(n.text, ctx, &found);
                if (!found)
                {
                    if (!m_result.missingVariables.contains(n.text)) m_result.missingVariables << n.text;
                    break;
                }
                bool scalar = true;
                const QString s = scalarText(v, &scalar);
                if (!scalar)
                {
                    m_result.errors << QStringLiteral("ligne %1 : « %2 » n'est pas une valeur simple (liste ou objet : utiliser une section)").arg(n.line).arg(n.text);
                    break;
                }
                m_result.output += n.escape ? TemplateEngine::escapeHtml(s) : s;
                break;
            }
            case Node::Section:
            case Node::Inverted:
            {
                bool found = false;
                const QJsonValue v = lookup(n.text, ctx, &found);
                const bool on = found && truthy(v);
                if (n.kind == Node::Inverted)
                {
                    if (!on) run(n.children, ctx, depth);
                    break;
                }
                if (!on) break;
                if (v.isArray())
                {
                    for (const auto& item : v.toArray())
                    {
                        ctx.push_back(item);
                        run(n.children, ctx, depth);
                        ctx.pop_back();
                    }
                }
                else if (v.isObject())
                {
                    ctx.push_back(v);
                    run(n.children, ctx, depth);
                    ctx.pop_back();
                }
                else
                    run(n.children, ctx, depth);
                break;
            }
            case Node::Partial:
            {
                if (depth >= m_options.maxPartialDepth)
                {
                    m_result.errors << QStringLiteral("ligne %1 : inclusions imbriquées au-delà de %2 niveaux (« %3 »)").arg(n.line).arg(m_options.maxPartialDepth).arg(n.text);
                    break;
                }
                const Parsed* p = partial(n.text, n.line);
                if (p) run(p->nodes, ctx, depth + 1);
                break;
            }
            }
        }
    }

private:
    QJsonValue lookup(const QString& name, const std::vector<QJsonValue>& ctx, bool* found) const
    {
        *found = false;
        if (name == QLatin1String("."))
        {
            *found = !ctx.empty();
            return ctx.empty() ? QJsonValue() : ctx.back();
        }
        const QStringList parts = name.split(QLatin1Char('.'));
        QJsonValue v;
        bool first = false;
        for (auto it = ctx.rbegin(); it != ctx.rend(); ++it)
            if (it->isObject() && it->toObject().contains(parts.front()))
            {
                v = it->toObject().value(parts.front());
                first = true;
                break;
            }
        if (!first) return {};
        for (int i = 1; i < parts.size(); ++i)
        {
            if (!v.isObject() || !v.toObject().contains(parts[i])) return {};
            v = v.toObject().value(parts[i]);
        }
        *found = true;
        return v;
    }

    const Parsed* partial(const QString& name, int line)
    {
        auto it = m_cache.find(name);
        if (it != m_cache.end()) return it->second.get();
        const std::optional<QString> text = m_partials ? m_partials(name) : std::nullopt;
        if (!text)
        {
            m_result.errors << QStringLiteral("ligne %1 : partiel « %2 » introuvable").arg(line).arg(name);
            m_cache[name] = nullptr;
            return nullptr;
        }
        auto parsed = std::make_unique<Parsed>(parse(*text));
        for (const auto& e : parsed->errors) m_result.errors << QStringLiteral("partiel « %1 », %2").arg(name, e);
        return (m_cache[name] = std::move(parsed)).get();
    }

    const TemplateEngine::PartialResolver& m_partials;
    const RenderOptions& m_options;
    RenderResult& m_result;
    std::map<QString, std::unique_ptr<Parsed>> m_cache;
    bool m_overflow = false;
};

} // namespace

QStringList TemplateEngine::check(const QString& templateText)
{
    return parse(templateText).errors;
}

QStringList TemplateEngine::referencedNames(const QString& templateText)
{
    QStringList names;
    collect(parse(templateText).nodes, &names, nullptr);
    return names;
}

QStringList TemplateEngine::referencedPartials(const QString& templateText)
{
    QStringList partials;
    collect(parse(templateText).nodes, nullptr, &partials);
    return partials;
}

RenderResult TemplateEngine::render(const QString& templateText, const QJsonValue& data, const PartialResolver& partials, const RenderOptions& options)
{
    RenderResult result;
    const Parsed parsed = parse(templateText);
    result.errors = parsed.errors;
    std::vector<QJsonValue> ctx { data };
    Renderer(partials, options, result).run(parsed.nodes, ctx, 0);
    return result;
}

QString TemplateEngine::escapeHtml(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (QChar c : text)
    {
        switch (c.unicode())
        {
        case '&': out += QLatin1String("&amp;"); break;
        case '<': out += QLatin1String("&lt;"); break;
        case '>': out += QLatin1String("&gt;"); break;
        case '"': out += QLatin1String("&quot;"); break;
        case '\'': out += QLatin1String("&#39;"); break;
        default: out += c;
        }
    }
    return out;
}

} // namespace TSA::Templates
