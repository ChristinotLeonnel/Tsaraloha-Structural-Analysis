#include "EngineeringKnowledgeBase.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace TSA::AI
{

namespace
{
constexpr int kChunkChars = 900;

QString stripAccents(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s.normalized(QString::NormalizationForm_D))
        if (c.category() != QChar::Mark_NonSpacing) out.append(c);
    return out;
}

const QSet<QString>& stopWords()
{
    static const QSet<QString> words = {
        "le", "la", "les", "un", "une", "des", "de", "du", "et", "ou", "en", "dans", "pour", "par", "sur", "au", "aux",
        "est", "sont", "que", "qui", "quoi", "ce", "cet", "cette", "ces", "il", "elle", "on", "ne", "pas", "plus", "avec",
        "the", "a", "an", "of", "to", "in", "is", "are", "and", "or", "for", "on", "with", "what", "how", "why", "pourquoi", "comment" };
    return words;
}
} // namespace

QStringList EngineeringKnowledgeBase::tokenize(const QString& text)
{
    static const QRegularExpression split(QStringLiteral("[^a-z0-9_]+"));
    QStringList tokens;
    for (const QString& t : stripAccents(text.toLower()).split(split, Qt::SkipEmptyParts))
        if (t.size() >= 2 && !stopWords().contains(t)) tokens << t;
    return tokens;
}

void EngineeringKnowledgeBase::clear()
{
    m_chunks.clear();
    m_termFreq.clear();
    m_lengths.clear();
    m_docFreq.clear();
    m_avgLength = 0.0;
}

void EngineeringKnowledgeBase::addDocument(const QString& source, const QString& content)
{
    // Découpage par titres Markdown, puis par paragraphes jusqu'à ~900 caractères.
    QString heading = QFileInfo(source).completeBaseName();
    QString buffer;
    auto flush = [&] {
        const QString t = buffer.trimmed();
        if (t.size() > 40) m_chunks.push_back({ source, heading, t });
        buffer.clear();
    };
    for (const QString& line : content.split('\n'))
    {
        if (line.startsWith('#'))
        {
            flush();
            heading = line.mid(line.indexOf(' ') + 1).trimmed();
            continue;
        }
        if (buffer.size() + line.size() > kChunkChars && line.trimmed().isEmpty()) flush();
        if (buffer.size() > kChunkChars * 2) flush();
        buffer += line + '\n';
    }
    flush();
    rebuildStatistics();
}

int EngineeringKnowledgeBase::indexDirectories(const QStringList& directories)
{
    for (const QString& dir : directories)
    {
        if (!QFileInfo(dir).isDir()) continue;
        const QDir base(dir);
        QDirIterator it(dir, { "*.md", "*.txt" }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString path = it.next();
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text) || f.size() > 2 * 1024 * 1024) continue;
            addDocument(QFileInfo(dir).fileName() + "/" + base.relativeFilePath(path), QString::fromUtf8(f.readAll()));
        }
    }
    return chunkCount();
}

void EngineeringKnowledgeBase::rebuildStatistics()
{
    m_termFreq.clear();
    m_lengths.clear();
    m_docFreq.clear();
    double total = 0.0;
    for (const auto& c : m_chunks)
    {
        std::unordered_map<QString, int> tf;
        const QStringList tokens = tokenize(c.heading + " " + c.text);
        for (const QString& t : tokens) ++tf[t];
        for (const auto& [term, n] : tf) ++m_docFreq[term];
        m_lengths.push_back(static_cast<int>(tokens.size()));
        total += tokens.size();
        m_termFreq.push_back(std::move(tf));
    }
    m_avgLength = m_chunks.empty() ? 0.0 : total / m_chunks.size();
}

std::vector<KnowledgeHit> EngineeringKnowledgeBase::search(const QString& query, int k, double minScore) const
{
    constexpr double k1 = 1.4, b = 0.75;
    const QStringList terms = tokenize(query);
    std::vector<KnowledgeHit> hits;
    const double n = static_cast<double>(m_chunks.size());
    for (size_t i = 0; i < m_chunks.size(); ++i)
    {
        double score = 0.0;
        for (const QString& t : terms)
        {
            const auto tfIt = m_termFreq[i].find(t);
            if (tfIt == m_termFreq[i].end()) continue;
            const double df = m_docFreq.at(t);
            const double idf = std::log(1.0 + (n - df + 0.5) / (df + 0.5));
            const double tf = tfIt->second;
            score += idf * tf * (k1 + 1.0) / (tf + k1 * (1.0 - b + b * m_lengths[i] / std::max(1.0, m_avgLength)));
        }
        if (score >= minScore) hits.push_back({ &m_chunks[i], score });
    }
    std::sort(hits.begin(), hits.end(), [](const KnowledgeHit& a, const KnowledgeHit& c) { return a.score > c.score; });
    if (static_cast<int>(hits.size()) > k) hits.resize(k);
    return hits;
}

QStringList EngineeringKnowledgeBase::sources() const
{
    QStringList s;
    for (const auto& c : m_chunks)
        if (!s.contains(c.source)) s << c.source;
    return s;
}

QStringList EngineeringKnowledgeBase::defaultDirectories()
{
    QStringList dirs;
    const QString app = QCoreApplication::applicationDirPath();
    for (const QString& candidate : { app + "/docs", app + "/../docs" })
    {
        const QString clean = QDir::cleanPath(candidate);
        if (QFileInfo(clean).isDir() && !dirs.contains(clean)) dirs << clean;
    }
    dirs << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ai/knowledge";
    return dirs;
}

} // namespace TSA::AI
