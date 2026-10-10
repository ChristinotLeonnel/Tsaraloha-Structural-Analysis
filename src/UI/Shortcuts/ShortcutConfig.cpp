#include "ShortcutConfig.h"

#include <QRegularExpression>

#include <algorithm>

namespace TSA::UI::Shortcuts
{

namespace
{
const QString kFormatTag = QStringLiteral("tsa-shortcuts");
constexpr int kFormatVersion = 1;

/// Noms français ou usuels → noms Qt portables (par élément entre « + »).
QString normalizeToken(const QString& token)
{
    static const QMap<QString, QString> names = {
        { "maj", "Shift" },       { "shift", "Shift" },     { "ctrl", "Ctrl" },     { "alt", "Alt" },
        { "suppr", "Del" },       { "delete", "Del" },      { "échap", "Esc" },     { "echap", "Esc" },
        { "escape", "Esc" },      { "inser", "Ins" },       { "insert", "Ins" },    { "gauche", "Left" },
        { "droite", "Right" },    { "haut", "Up" },         { "bas", "Down" },      { "entrée", "Return" },
        { "entree", "Return" },   { "enter", "Return" },    { "espace", "Space" },  { "origine", "Home" },
        { "fin", "End" },         { "pageprec", "PgUp" },   { "pagesuiv", "PgDown" }, { "num", "Num" },
    };
    const QString t = token.trimmed();
    auto it = names.find(t.toLower());
    return it != names.end() ? it.value() : t;
}

/// « Ctrl+Maj+F, Échap » → « Ctrl+Shift+F, Esc » ; « Num7 » → « Num+7 ».
QString normalizeSequenceText(QString text)
{
    text = text.trimmed();
    static const QRegularExpression numDigit(QStringLiteral("(?i)\\bnum\\s*([0-9])\\b"));
    text.replace(numDigit, QStringLiteral("Num+\\1"));
    QStringList chords;
    for (const QString& chord : text.split(QLatin1Char(',')))
    {
        // « + » seul est une touche : « Ctrl++ » → éléments « Ctrl » et « + ».
        QString c = chord.trimmed();
        QStringList parts;
        QString current;
        for (int i = 0; i < c.size(); ++i)
        {
            const QChar ch = c.at(i);
            if (ch == QLatin1Char('+') && !current.trimmed().isEmpty())
            {
                parts << normalizeToken(current);
                current.clear();
            }
            else
                current += ch;
        }
        if (!current.isEmpty()) parts << normalizeToken(current);
        chords << parts.join(QLatin1Char('+'));
    }
    return chords.join(QStringLiteral(", "));
}

bool isModifierKey(int key)
{
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta ||
           key == Qt::Key_AltGr;
}

/// Sépare les alias sur « ; » sauf quand « ; » est la touche (« Ctrl+; »).
QStringList splitAliases(const QString& text)
{
    QStringList out;
    QString current;
    for (const QChar ch : text)
    {
        if (ch == QLatin1Char(';') && !current.trimmed().endsWith(QLatin1Char('+')) && !current.trimmed().isEmpty())
        {
            out << current.trimmed();
            current.clear();
        }
        else
            current += ch;
    }
    if (!current.trimmed().isEmpty()) out << current.trimmed();
    return out;
}

bool startsWith(const QKeySequence& longer, const QKeySequence& shorter)
{
    if (shorter.count() >= longer.count()) return false;
    for (int i = 0; i < shorter.count(); ++i)
        if (longer[i] != shorter[i]) return false;
    return true;
}

QString padRight(const QString& s, int width)
{
    return s.size() >= width ? s : s + QString(width - s.size(), QLatin1Char(' '));
}
} // namespace

int shortcutFormatVersion()
{
    return kFormatVersion;
}

bool scopesOverlap(const QString& a, const QString& b)
{
    if (a == b) return true;
    const auto windowLike = [](const QString& s) { return s == QLatin1String("window") || s == QLatin1String("viewport"); };
    return windowLike(a) && windowLike(b);
}

QString Conflict::message() const
{
    if (prefix)
        return QStringLiteral("« %1 » (%2) est le début de « %3 » (%4) : Qt attendrait la touche suivante, "
                              "« %1 » ne se déclencherait jamais")
            .arg(sequenceA.toString(QKeySequence::PortableText), idA, sequenceB.toString(QKeySequence::PortableText), idB);
    return QStringLiteral("« %1 » est attribué à %2 et à %3 : aucune des deux commandes ne se déclencherait")
        .arg(sequenceA.toString(QKeySequence::PortableText), idA, idB);
}

bool isValidSequence(const QKeySequence& sequence)
{
    if (sequence.isEmpty()) return false;
    for (int i = 0; i < sequence.count(); ++i)
    {
        const int key = sequence[i].key();
        if (key == 0 || key == Qt::Key_unknown || isModifierKey(key)) return false;
    }
    return true;
}

bool parseSequenceList(const QString& text, QList<QKeySequence>* out, QString* error)
{
    QList<QKeySequence> result;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.compare(QLatin1String("aucun"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
    {
        if (out) *out = result;
        return true;
    }
    for (const QString& alias : splitAliases(trimmed))
    {
        if (alias.contains(QLatin1Char('|')))
        {
            if (error) *error = QStringLiteral("« | » sert de séparateur de colonnes et ne peut pas être un raccourci");
            return false;
        }
        const QString normalized = normalizeSequenceText(alias);
        const QKeySequence seq = QKeySequence::fromString(normalized, QKeySequence::PortableText);
        if (!isValidSequence(seq))
        {
            if (error) *error = QStringLiteral("combinaison invalide « %1 »").arg(alias);
            return false;
        }
        if (!result.contains(seq)) result << seq;
    }
    if (out) *out = result;
    return true;
}

QString sequenceListText(const QList<QKeySequence>& sequences)
{
    QStringList parts;
    for (const auto& s : sequences) parts << s.toString(QKeySequence::PortableText);
    return parts.join(QStringLiteral(" ; "));
}

QString sequenceListNativeText(const QList<QKeySequence>& sequences)
{
    QStringList parts;
    for (const auto& s : sequences) parts << s.toString(QKeySequence::NativeText);
    return parts.join(QStringLiteral(" / "));
}

ParseResult parseConfig(const QString& text, const QSet<QString>& knownIds)
{
    ParseResult r;
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression formatRe(QStringLiteral("^#\\s*format\\s*:\\s*(\\S+)\\s+(\\d+)"));
    for (int i = 0; i < lines.size(); ++i)
    {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        if (i == 0 && line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
        const QString trimmed = line.trimmed();
        const QString where = QStringLiteral("ligne %1").arg(i + 1);
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith(QLatin1Char('#')))
        {
            const auto m = formatRe.match(trimmed);
            if (m.hasMatch() && m.captured(1) == kFormatTag) r.formatVersion = m.captured(2).toInt();
            if (m.hasMatch() && m.captured(1) == kFormatTag && m.captured(2).toInt() > kFormatVersion)
                r.warnings << QStringLiteral("%1 : format %2 plus récent que celui de cette version (%3)")
                                  .arg(where, m.captured(2)).arg(kFormatVersion);
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('[')))
        {
            if (!trimmed.endsWith(QLatin1Char(']'))) r.errors << QStringLiteral("%1 : titre de section non fermé « %2 »").arg(where, trimmed);
            continue;
        }
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.size() < 2)
        {
            r.errors << QStringLiteral("%1 : « identifiant | raccourci » attendu, lu « %2 »").arg(where, trimmed);
            continue;
        }
        const QString id = fields.at(0).trimmed();
        if (id.isEmpty() || id.contains(QLatin1Char(' ')))
        {
            r.errors << QStringLiteral("%1 : identifiant de commande invalide « %2 »").arg(where, id);
            continue;
        }
        ShortcutSetting setting;
        QString err;
        if (!parseSequenceList(fields.at(1), &setting.sequences, &err))
        {
            r.errors << QStringLiteral("%1 (%2) : %3").arg(where, id, err);
            continue;
        }
        if (fields.size() >= 3)
        {
            const QString state = fields.at(2).trimmed().toLower();
            if (state.isEmpty() || state == "on" || state == "oui" || state == "actif" || state == "1") setting.enabled = true;
            else if (state == "off" || state == "non" || state == "inactif" || state == "0") setting.enabled = false;
            else
            {
                r.errors << QStringLiteral("%1 (%2) : état « %3 » invalide (on ou off)").arg(where, id, fields.at(2).trimmed());
                continue;
            }
        }
        if (!knownIds.contains(id))
        {
            r.warnings << QStringLiteral("%1 : commande inconnue « %2 » ignorée (renommée, supprimée ou d'une autre version)").arg(where, id);
            continue;
        }
        if (r.settings.contains(id))
        {
            r.errors << QStringLiteral("%1 : commande « %2 » déjà définie plus haut").arg(where, id);
            continue;
        }
        r.settings.insert(id, setting);
    }
    r.ok = r.errors.isEmpty();
    return r;
}

QString formatConfig(const QList<CommandDefinition>& definitions, const ShortcutSettings& settings)
{
    QList<CommandDefinition> defs = definitions;
    std::stable_sort(defs.begin(), defs.end(), [](const CommandDefinition& a, const CommandDefinition& b) { return a.categoryRank < b.categoryRank; });

    QString out;
    out += QStringLiteral(
        "# ==============================================================================================\n"
        "#  TSA — Tsaraloha Structural Analysis : raccourcis clavier (shortcut.txt)\n"
        "# ==============================================================================================\n"
        "#  Modifiez ce fichier puis enregistrez-le : TSA le relit aussitôt, sans redémarrage ni recompilation.\n"
        "#  Une configuration invalide n'est jamais appliquée : la précédente reste active et l'erreur est\n"
        "#  signalée dans la console de TSA. Éditeur intégré : Aide > Personnaliser les raccourcis (Ctrl+F1).\n"
        "#\n"
        "#  Colonnes : identifiant | raccourci(s) | état | par défaut | description\n"
        "#   - identifiant : stable, ne pas modifier (les libellés de l'interface peuvent changer) ;\n"
        "#   - raccourci(s) : « Ctrl+S », « Shift+F », « Num+7 », « F5 » ; plusieurs alias séparés par « ; » ;\n"
        "#     suite de touches façon AutoCAD : « D, A » (D puis A) ; vide ou « aucun » : pas de raccourci ;\n"
        "#     noms français acceptés : Maj, Suppr, Échap, Inser, Gauche, Droite, Haut, Bas, Entrée, Espace ;\n"
        "#   - état : on (actif) ou off (désactivé, le raccourci est conservé pour plus tard) ;\n"
        "#   - par défaut, description : pour information, ignorés à la lecture et régénérés.\n"
        "#  Une commande absente du fichier garde son raccourci par défaut ; une commande inconnue est ignorée.\n"
        "#  Deux commandes ne peuvent pas partager un raccourci, ni l'une commencer l'autre (« D » et « D, A ») :\n"
        "#  la configuration serait refusée. Alt+F4 (quitter) reste géré par Windows.\n"
        "#  Les champs de saisie gardent leurs touches (lettres, Ctrl+C, Ctrl+V, Ctrl+Z, Suppr…).\n"
        "# ==============================================================================================\n");
    out += QStringLiteral("# format: %1 %2\n").arg(kFormatTag).arg(kFormatVersion);

    QString currentCategory;
    for (const auto& d : defs)
    {
        if (d.category != currentCategory)
        {
            currentCategory = d.category;
            out += QStringLiteral("\n[%1]\n").arg(currentCategory);
            out += QStringLiteral("# %1| %2| %3| %4| %5\n")
                       .arg(padRight(QStringLiteral("identifiant"), 34), padRight(QStringLiteral("raccourci(s)"), 22),
                            padRight(QStringLiteral("état"), 5), padRight(QStringLiteral("par défaut"), 22), QStringLiteral("description"));
        }
        const auto it = settings.find(d.id);
        const QList<QKeySequence> active = it != settings.end() ? it->sequences : d.defaults;
        const bool enabled = it != settings.end() ? it->enabled : true;
        out += QStringLiteral("%1| %2| %3| %4| %5\n")
                   .arg(padRight(d.id, 36), padRight(sequenceListText(active), 22), padRight(enabled ? "on" : "off", 5),
                        padRight(sequenceListText(d.defaults), 22), d.name + (d.description.isEmpty() ? QString() : " — " + d.description));
    }
    return out;
}

QList<QKeySequence> effectiveSequences(const CommandDefinition& definition, const ShortcutSettings& settings)
{
    const auto it = settings.find(definition.id);
    if (it == settings.end()) return definition.defaults;
    return it->enabled ? it->sequences : QList<QKeySequence>();
}

QList<Conflict> findConflicts(const QList<CommandDefinition>& definitions, const ShortcutSettings& settings)
{
    struct Entry
    {
        QString id, scope;
        QKeySequence seq;
    };
    QList<Entry> entries;
    for (const auto& d : definitions)
        for (const auto& s : effectiveSequences(d, settings)) entries.push_back({ d.id, d.scope, s });

    QList<Conflict> out;
    for (int i = 0; i < entries.size(); ++i)
        for (int j = i + 1; j < entries.size(); ++j)
        {
            const Entry& a = entries[i];
            const Entry& b = entries[j];
            if (a.id == b.id || !scopesOverlap(a.scope, b.scope)) continue;
            if (a.seq == b.seq) out.push_back({ a.id, b.id, a.seq, b.seq, false });
            else if (startsWith(b.seq, a.seq)) out.push_back({ a.id, b.id, a.seq, b.seq, true });
            else if (startsWith(a.seq, b.seq)) out.push_back({ b.id, a.id, b.seq, a.seq, true });
        }
    return out;
}

QString stripShortcutHint(const QString& toolTip)
{
    static const QRegularExpression tail(QStringLiteral("\\s*\\(([^()]*)\\)\\s*$"));
    QString text = toolTip;
    const auto m = tail.match(text);
    if (!m.hasMatch()) return text;
    QString inner = m.captured(1).trimmed();
    if (inner.isEmpty()) return text;
    inner.replace(QStringLiteral(" ou "), QStringLiteral(" / "));
    for (const QString& part : inner.split(QLatin1Char('/')))
    {
        const QString p = part.trimmed();
        // Une description ne contient pas de minuscule isolée suivie d'autres mots : « Fit All » est
        // refusé par QKeySequence (touche inconnue), « LCS » aussi.
        const QKeySequence seq = QKeySequence::fromString(normalizeSequenceText(p), QKeySequence::PortableText);
        if (p.isEmpty() || !isValidSequence(seq)) return text;
    }
    return text.left(m.capturedStart()).trimmed();
}

} // namespace TSA::UI::Shortcuts
