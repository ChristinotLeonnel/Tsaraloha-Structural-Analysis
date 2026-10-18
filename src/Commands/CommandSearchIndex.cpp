#include "CommandSearchIndex.h"

#include <QSet>

#include <algorithm>
#include <vector>

namespace TSA::Commands
{

namespace
{
constexpr double kTitleWeight = 1.0;
constexpr double kKeywordWeight = 0.9;
constexpr double kIdWeight = 0.6;
constexpr double kDescriptionWeight = 0.4;

/// Tolérance aux fautes de frappe selon la longueur du mot saisi.
int allowedTypos(int length)
{
    return length >= 7 ? 2 : (length >= 4 ? 1 : 0);
}
} // namespace

QString CommandSearchIndex::normalize(const QString& text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed)
    {
        if (c.category() == QChar::Mark_NonSpacing) continue;   // accents (é → e, ô → o, ñ → n)
        switch (c.unicode())
        {
        case 0x0153: case 0x0152: out += QStringLiteral("oe"); continue;   // œ Œ
        case 0x00E6: case 0x00C6: out += QStringLiteral("ae"); continue;   // æ Æ
        case 0x00DF: out += QStringLiteral("ss"); continue;                // ß
        default: break;
        }
        out += c.isLetterOrNumber() ? c.toCaseFolded() : QChar(QLatin1Char(' '));
    }
    return out.simplified();
}

bool CommandSearchIndex::isStopWord(const QString& w)
{
    static const QSet<QString> words = {
        // français
        "de", "du", "des", "la", "le", "les", "l", "d", "un", "une", "et", "a", "au", "aux", "en", "pour", "sur", "par", "avec",
        // anglais
        "the", "an", "of", "to", "and", "in", "on", "for", "with",
        // malagasy
        "ny", "ho", "sy", "amin", "ary", "ao", "eo", "ilay", "izay", "na", "fa",
    };
    return words.contains(w);
}

int CommandSearchIndex::boundedDistance(const QString& a, const QString& b, int maxDistance)
{
    const int n = static_cast<int>(a.size()), m = static_cast<int>(b.size());
    if (std::abs(n - m) > maxDistance) return maxDistance + 1;
    if (n == 0) return m;
    if (m == 0) return n;
    // Distance optimale d'alignement (Damerau restreinte), trois lignes ; arrêt dès que la ligne dépasse la borne
    std::vector<int> prev2(m + 1), prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i)
    {
        cur[0] = i;
        int rowMin = cur[0];
        for (int j = 1; j <= m; ++j)
        {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int v = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) v = std::min(v, prev2[j - 2] + 1);
            cur[j] = v;
            rowMin = std::min(rowMin, v);
        }
        if (rowMin > maxDistance) return maxDistance + 1;
        std::swap(prev2, prev);
        std::swap(prev, cur);
    }
    return std::min(prev[m], maxDistance + 1);
}

CommandSearchIndex::Field CommandSearchIndex::makeField(const QString& raw, double weight)
{
    Field f;
    f.text = normalize(raw);
    f.words = f.text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    f.weight = weight;
    return f;
}

void CommandSearchIndex::add(const SearchEntry& entry)
{
    Prepared p;
    p.fields << makeField(entry.title, kTitleWeight);
    for (const QString& k : entry.keywords)
        if (!k.trimmed().isEmpty()) p.fields << makeField(k, kKeywordWeight);
    QString id = entry.id;
    id.replace(QLatin1Char('.'), QLatin1Char(' ')).replace(QLatin1Char('_'), QLatin1Char(' ')).replace(QLatin1Char(':'), QLatin1Char(' '));
    p.fields << makeField(id, kIdWeight);
    p.fields << makeField(entry.description, kDescriptionWeight);

    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].id == entry.id)
        {
            m_entries[i] = entry;
            m_prepared[i] = p;
            return;
        }
    m_entries << entry;
    m_prepared << p;
}

void CommandSearchIndex::clear()
{
    m_entries.clear();
    m_prepared.clear();
}

double CommandSearchIndex::score(const Prepared& p, const QString& query, const QStringList& tokens) const
{
    // 1. La requête entière : libellé exact, début, début d'un mot, sous-chaîne
    double phrase = 0.0;
    for (const Field& f : p.fields)
    {
        double s = 0.0;
        if (f.text == query) s = 1000.0;
        else if (f.text.startsWith(query)) s = 700.0;
        else if (f.text.contains(QLatin1Char(' ') + query)) s = 550.0;
        else if (query.size() >= 3 && f.weight >= kKeywordWeight && f.text.contains(query)) s = 350.0;
        phrase = std::max(phrase, s * f.weight);
    }

    // 2. Chaque mot de la requête : présent (exact, début, fragment) ou à une faute près, dans un champ quelconque ;
    //    les mots facultatifs n'éliminent pas l'entrée.
    bool allStop = std::all_of(tokens.begin(), tokens.end(), [](const QString& t) { return isStopWord(t); });
    double sum = 0.0;
    for (const QString& t : tokens)
    {
        const bool required = allStop || !isStopWord(t);
        const int typos = allowedTypos(static_cast<int>(t.size()));
        double best = 0.0;
        for (const Field& f : p.fields)
        {
            const bool fuzzyField = f.weight >= kKeywordWeight;   // pas de fautes ni de fragments dans les descriptions
            for (const QString& w : f.words)
            {
                double s = 0.0;
                if (w == t) s = 100.0;
                else if (w.startsWith(t)) s = 85.0 - std::min<double>(15.0, w.size() - t.size());
                else if (fuzzyField && t.size() >= 3 && w.contains(t)) s = 50.0;
                else if (fuzzyField && typos > 0)
                {
                    const int d = boundedDistance(t, w, typos);
                    if (d <= typos) s = 40.0 - 8.0 * d;
                    else if (w.size() > t.size())
                    {
                        const int dp = boundedDistance(t, w.left(t.size()), typos);   // mot en cours de frappe
                        if (dp <= typos) s = 30.0 - 8.0 * dp;
                    }
                }
                best = std::max(best, s * f.weight);
            }
        }
        if (best <= 0.0 && required) return 0.0;
        sum += best;
    }
    return phrase + 4.0 * sum / std::max<qsizetype>(1, tokens.size());
}

QList<SearchHit> CommandSearchIndex::search(const QString& rawQuery, int limit) const
{
    QList<SearchHit> hits;
    const QString query = normalize(rawQuery);
    if (query.isEmpty() || limit <= 0) return hits;
    const QStringList tokens = query.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = 0; i < m_prepared.size(); ++i)
    {
        const double s = score(m_prepared[i], query, tokens);
        if (s > 0.0) hits << SearchHit{ i, s };
    }
    std::stable_sort(hits.begin(), hits.end(), [this](const SearchHit& a, const SearchHit& b) {
        if (a.score != b.score) return a.score > b.score;
        const auto& ea = m_entries[a.entry];
        const auto& eb = m_entries[b.entry];
        if (ea.title.size() != eb.title.size()) return ea.title.size() < eb.title.size();
        return ea.order < eb.order;
    });
    if (hits.size() > limit) hits.resize(limit);
    return hits;
}

} // namespace TSA::Commands
