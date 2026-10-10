#include "ShortcutManager.h"

#include "../../Commands/CommandCatalog.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTimer>

#include <algorithm>
#include <map>

namespace TSA::UI::Shortcuts
{

using TSA::Commands::CommandCatalog;
using TSA::Commands::CommandCategory;

namespace
{
/// Contenu comparable : fins de ligne unifiées (le fichier est écrit en CRLF sous Windows).
QString normalizedText(QString text)
{
    return text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
}
} // namespace

ShortcutManager::ShortcutManager(QObject* parent)
    : QObject(parent)
{
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(250);
    connect(m_reloadTimer, &QTimer::timeout, this, [this] {
        watchPath(); // fichier remplacé (enregistrement par renommage) : surveillé de nouveau
        if (!QFileInfo::exists(m_path)) return;
        QString text, err;
        if (!readFile(&text, &err) || normalizedText(text) == m_lastText) return; // illisible pour l'instant, ou inchangé (dont notre propre écriture)
        reload();
    });
}

ShortcutManager::~ShortcutManager() = default;

ShortcutManager& ShortcutManager::instance()
{
    static QPointer<ShortcutManager> s;
    if (!s) s = new ShortcutManager(QCoreApplication::instance());
    return *s;
}

QList<CommandDefinition> ShortcutManager::catalogDefinitions()
{
    static QList<CommandDefinition> cached; // le catalogue est statique : construit une seule fois
    if (!cached.isEmpty()) return cached;
    std::size_t count = 0;
    const CommandCategory* order = TSA::Commands::categoryOrder(&count);
    std::map<CommandCategory, int> rank;
    for (std::size_t i = 0; i < count; ++i) rank[order[i]] = static_cast<int>(i);

    QList<CommandDefinition> out;
    for (const auto& [id, d] : CommandCatalog::instance().allCommands())
    {
        CommandDefinition def;
        def.id = QString::fromStdString(d.id);
        def.name = QString::fromStdString(d.name);
        def.description = QString::fromStdString(d.description);
        def.category = QString::fromStdString(TSA::Commands::categoryToString(d.category));
        def.categoryRank = rank.count(d.category) ? rank[d.category] : static_cast<int>(count);
        QString err;
        parseSequenceList(QString::fromStdString(d.shortcut), &def.defaults, &err); // validé par les tests
        out << def;
    }
    std::stable_sort(out.begin(), out.end(), [](const CommandDefinition& a, const CommandDefinition& b) { return a.categoryRank < b.categoryRank; });
    cached = out;
    return out;
}

void ShortcutManager::registerDefinition(const CommandDefinition& definition)
{
    if (!m_definitions.contains(definition.id)) m_order << definition.id;
    m_definitions.insert(definition.id, definition);
}

bool ShortcutManager::bind(const QString& id, QAction* action)
{
    if (!action) return false;
    bool known = true;
    if (!m_definitions.contains(id))
    {
        const auto* d = CommandCatalog::instance().findCommand(id.toStdString());
        if (d)
        {
            for (const auto& def : catalogDefinitions())
                if (def.id == id) registerDefinition(def);
        }
        else
        {
            known = false;
            CommandDefinition def;
            def.id = id;
            def.name = action->text().remove(QLatin1Char('&'));
            def.category = QStringLiteral("Autres");
            def.categoryRank = 1000;
            registerDefinition(def);
            m_lastWarnings << QStringLiteral("Commande « %1 » absente du catalogue : aucun raccourci par défaut").arg(id);
        }
    }
    auto& list = m_actions[id];
    for (const auto& a : list)
        if (a == action) return known;
    list << action; // action détruite : QPointer nul, ignoré partout
    if (id != QLatin1String("cmd.edit.repeat"))
        connect(action, &QAction::triggered, this, [this, action] { rememberLastCommand(action); });
    applyToAction(action, effective(id));
    return known;
}

void ShortcutManager::unbind(QAction* action)
{
    for (auto& l : m_actions) l.removeAll(action);
}

QList<CommandDefinition> ShortcutManager::definitions() const
{
    QList<CommandDefinition> out;
    for (const QString& id : m_order)
    {
        const auto it = m_actions.find(id);
        if (it == m_actions.end()) continue;
        bool live = false;
        for (const auto& a : *it) live |= !a.isNull();
        if (live) out << m_definitions.value(id);
    }
    std::stable_sort(out.begin(), out.end(), [](const CommandDefinition& a, const CommandDefinition& b) { return a.categoryRank < b.categoryRank; });
    return out;
}

const CommandDefinition* ShortcutManager::definition(const QString& id) const
{
    const auto it = m_definitions.find(id);
    return it != m_definitions.end() ? &it.value() : nullptr;
}

QList<QAction*> ShortcutManager::actions(const QString& id) const
{
    QList<QAction*> out;
    for (const auto& a : m_actions.value(id))
        if (a) out << a.data();
    return out;
}

QString ShortcutManager::idOf(const QAction* action) const
{
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it)
        for (const auto& a : it.value())
            if (a == action) return it.key();
    return {};
}

QList<QKeySequence> ShortcutManager::effective(const QString& id) const
{
    const auto it = m_definitions.find(id);
    return it != m_definitions.end() ? effectiveSequences(it.value(), m_settings) : QList<QKeySequence>();
}

QList<QKeySequence> ShortcutManager::defaults(const QString& id) const
{
    const auto it = m_definitions.find(id);
    return it != m_definitions.end() ? it->defaults : QList<QKeySequence>();
}

bool ShortcutManager::isEnabled(const QString& id) const
{
    const auto it = m_settings.find(id);
    return it == m_settings.end() || it->enabled;
}

// --- Configuration --------------------------------------------------------------------------------

QString ShortcutManager::defaultConfigPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.tsa");
    return QDir(dir).filePath(QStringLiteral("shortcut.txt"));
}

void ShortcutManager::setConfigPath(const QString& path)
{
    m_path = QDir::cleanPath(path);
    m_lastText.clear();
    if (!m_watcher)
    {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ShortcutManager::onFileSystemChanged);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &ShortcutManager::onFileSystemChanged);
    }
    if (!m_watcher->files().isEmpty()) m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty()) m_watcher->removePaths(m_watcher->directories());
    watchPath();
}

void ShortcutManager::setReloadDelay(int ms)
{
    m_reloadTimer->setInterval(ms);
}

void ShortcutManager::watchPath()
{
    if (!m_watcher || m_path.isEmpty()) return;
    const QString dir = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(dir) && !m_watcher->directories().contains(dir)) m_watcher->addPath(dir);
    if (QFileInfo::exists(m_path) && !m_watcher->files().contains(m_path)) m_watcher->addPath(m_path);
}

void ShortcutManager::onFileSystemChanged()
{
    m_reloadTimer->start(); // regroupe les notifications d'un même enregistrement
}

bool ShortcutManager::readFile(QString* text, QString* error) const
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error) *error = QStringLiteral("lecture impossible de %1 : %2").arg(QDir::toNativeSeparators(m_path), f.errorString());
        return false;
    }
    *text = QString::fromUtf8(f.readAll());
    return true;
}

bool ShortcutManager::writeFile(const QString& text, QString* error)
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path); // fichier temporaire puis remplacement : jamais de fichier à moitié écrit
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text) || f.write(text.toUtf8()) < 0 || !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1 : %2").arg(QDir::toNativeSeparators(m_path), f.errorString());
        return false;
    }
    m_lastText = normalizedText(text); // notification de notre propre écriture : ignorée
    watchPath();
    return true;
}

static QSet<QString> knownIdsOf(const QHash<QString, CommandDefinition>& defs)
{
    QSet<QString> ids;
    for (auto it = defs.begin(); it != defs.end(); ++it) ids.insert(it.key());
    return ids;
}

bool ShortcutManager::loadConfig()
{
    if (m_path.isEmpty()) setConfigPath(defaultConfigPath());
    m_lastErrors.clear();
    if (!QFileInfo::exists(m_path))
    {
        QString err;
        if (!writeFile(defaultConfigText(), &err))
        {
            m_lastErrors << err << QStringLiteral("Raccourcis par défaut utilisés (aucun fichier personnalisable).");
            applyToActions();
            emit configRejected(m_lastErrors);
            return false;
        }
        m_settings.clear();
        applyToActions();
        emit configApplied(QStringLiteral("Fichier des raccourcis créé : %1 — %2").arg(QDir::toNativeSeparators(m_path), summary()), {});
        return true;
    }
    return reload();
}

bool ShortcutManager::reload()
{
    QString text, err;
    m_lastErrors.clear();
    m_lastWarnings.clear();
    if (!readFile(&text, &err))
    {
        m_lastErrors << err << QStringLiteral("Dernière configuration valide conservée.");
        emit configRejected(m_lastErrors);
        return false;
    }
    m_lastText = normalizedText(text);
    // Les commandes du catalogue sont toutes connues (une commande pas encore reliée n'est pas « inconnue »).
    QSet<QString> known = knownIdsOf(m_definitions);
    for (const auto& d : catalogDefinitions()) known.insert(d.id);
    const ParseResult parsed = parseConfig(text, known);
    m_lastWarnings = parsed.warnings;
    if (!parsed.ok)
    {
        m_lastErrors = parsed.errors;
        m_lastErrors << QStringLiteral("Configuration refusée : la dernière configuration valide reste active.");
        emit configRejected(m_lastErrors);
        return false;
    }
    QStringList errors;
    if (!applySettings(parsed.settings, &errors))
    {
        m_lastErrors = errors;
        m_lastErrors << QStringLiteral("Configuration refusée : la dernière configuration valide reste active.");
        emit configRejected(m_lastErrors);
        return false;
    }
    return true;
}

bool ShortcutManager::applySettings(const ShortcutSettings& settings, QStringList* errors)
{
    // Conflits évalués sur toutes les commandes déclarées (reliées ou non).
    QList<CommandDefinition> all;
    for (const QString& id : m_order) all << m_definitions.value(id);
    const QList<Conflict> conflicts = findConflicts(all, settings);
    if (!conflicts.isEmpty())
    {
        if (errors)
        {
            *errors << QStringLiteral("%1 conflit(s) de raccourcis :").arg(conflicts.size());
            for (const auto& c : conflicts) *errors << QStringLiteral("  • ") + c.message();
        }
        return false;
    }
    m_settings = settings;
    applyToActions();
    emit configApplied(summary(), m_lastWarnings);
    emit shortcutsChanged();
    return true;
}

bool ShortcutManager::saveSettings(const ShortcutSettings& settings, QStringList* errors)
{
    QList<CommandDefinition> all;
    for (const QString& id : m_order) all << m_definitions.value(id);
    const QList<Conflict> conflicts = findConflicts(all, settings);
    if (!conflicts.isEmpty())
    {
        if (errors)
            for (const auto& c : conflicts) *errors << c.message();
        return false;
    }
    if (m_path.isEmpty()) setConfigPath(defaultConfigPath());
    QString err;
    QList<CommandDefinition> defs = all;
    for (const auto& d : catalogDefinitions())
        if (!m_definitions.contains(d.id)) defs << d; // commandes du catalogue non reliées : conservées dans le fichier
    if (!writeFile(formatConfig(defs, settings), &err))
    {
        if (errors) *errors << err;
        return false;
    }
    m_lastWarnings.clear();
    return applySettings(settings, errors);
}

QString ShortcutManager::defaultConfigText() const
{
    QList<CommandDefinition> defs = catalogDefinitions();
    for (const QString& id : m_order)
    {
        bool inCatalog = false;
        for (const auto& d : defs) inCatalog |= d.id == id;
        if (!inCatalog) defs << m_definitions.value(id);
    }
    return formatConfig(defs, {});
}

QString ShortcutManager::currentConfigText() const
{
    QList<CommandDefinition> defs = catalogDefinitions();
    return formatConfig(defs, m_settings);
}

QString ShortcutManager::summary() const
{
    int active = 0, disabled = 0, custom = 0;
    for (const auto& d : definitions())
    {
        const auto it = m_settings.find(d.id);
        if (it != m_settings.end() && !it->enabled) ++disabled;
        else if (!effective(d.id).isEmpty()) ++active;
        if (it != m_settings.end() && (it->sequences != d.defaults || !it->enabled)) ++custom;
    }
    return QStringLiteral("%1 commande(s) avec raccourci, %2 désactivé(s), %3 personnalisé(s)").arg(active).arg(disabled).arg(custom);
}

void ShortcutManager::applyToActions()
{
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it)
    {
        const QList<QKeySequence> seqs = effective(it.key());
        for (const auto& a : it.value())
            if (a) applyToAction(a, seqs);
    }
}

void ShortcutManager::applyToAction(QAction* action, const QList<QKeySequence>& sequences)
{
    if (action->shortcuts() != sequences) action->setShortcuts(sequences); // inchangé : rien n'est reposé
    QString tip = stripShortcutHint(action->toolTip());
    const QString text = action->text().remove(QLatin1Char('&'));
    if (tip.isEmpty()) tip = text;
    const QString wanted = sequences.isEmpty() ? tip : QStringLiteral("%1 (%2)").arg(tip, sequenceListNativeText(sequences));
    // Infobulle par défaut de Qt (= texte) : ne la fige pas si aucun raccourci.
    if (wanted != action->toolTip() && !(sequences.isEmpty() && wanted == text)) action->setToolTip(wanted);
}

// --- Aide -----------------------------------------------------------------------------------------

QString ShortcutManager::htmlReference() const
{
    QString html = QStringLiteral("<table><tr><th>Raccourci</th><th>Commande</th><th>Description</th></tr>");
    QString category;
    for (const auto& d : definitions())
    {
        const QList<QKeySequence> seqs = effective(d.id);
        if (seqs.isEmpty()) continue;
        if (d.category != category)
        {
            category = d.category;
            html += QStringLiteral("<tr><td colspan='3'><b>%1</b></td></tr>").arg(category.toHtmlEscaped());
        }
        QStringList badges;
        for (const auto& s : seqs) badges << QStringLiteral("<span class='badge'>%1</span>").arg(s.toString(QKeySequence::NativeText).toHtmlEscaped());
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                    .arg(badges.join(QStringLiteral(" ")), d.name.toHtmlEscaped(), d.description.toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
    return html;
}

// --- Répéter la dernière commande ------------------------------------------------------------------

QAction* ShortcutManager::repeatAction()
{
    if (!m_repeatAction)
    {
        m_repeatAction = new QAction(tr("Répéter la dernière commande"), this);
        m_repeatAction->setEnabled(false);
        connect(m_repeatAction, &QAction::triggered, this, [this] {
            if (m_lastCommand && m_lastCommand->isEnabled()) m_lastCommand->trigger();
        });
        bind(QStringLiteral("cmd.edit.repeat"), m_repeatAction);
    }
    return m_repeatAction;
}

void ShortcutManager::rememberLastCommand(QAction* action)
{
    const QString id = idOf(action);
    const auto* d = definition(id);
    if (!d) return;
    // Commandes de travail répétables (création, modification, cotation, charges, appuis) ; pas les
    // vues, bascules d'affichage, fichiers ou fenêtres.
    static const QStringList prefixes = { "cmd.create.", "cmd.modify.", "cmd.tool.", "cmd.loads.", "cmd.support.", "cmd.dim.edit", "cmd.tools.measure" };
    bool repeatable = false;
    for (const auto& p : prefixes) repeatable |= id.startsWith(p);
    if (!repeatable || id == QLatin1String("cmd.modify.viewport_input") || id == QLatin1String("cmd.create.presets")) return;
    m_lastCommand = action;
    if (m_repeatAction)
    {
        m_repeatAction->setEnabled(true);
        m_repeatAction->setText(tr("Répéter : %1").arg(action->text().remove(QLatin1Char('&'))));
    }
}

// --- Garde des champs de saisie -------------------------------------------------------------------

bool ShortcutInputGuard::isTextInput(const QObject* o)
{
    if (const auto* le = qobject_cast<const QLineEdit*>(o)) return !le->isReadOnly();
    if (const auto* te = qobject_cast<const QTextEdit*>(o)) return !te->isReadOnly();
    if (const auto* pe = qobject_cast<const QPlainTextEdit*>(o)) return !pe->isReadOnly();
    if (const auto* sb = qobject_cast<const QAbstractSpinBox*>(o)) return !sb->isReadOnly();
    return false;
}

bool ShortcutInputGuard::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::ShortcutOverride) return QObject::eventFilter(watched, event);
    auto* ke = static_cast<QKeyEvent*>(event);
    // L'enregistreur de raccourcis reçoit toutes les touches (y compris Ctrl+S, Échap, F5…).
    for (QObject* o = watched; o; o = o->parent())
        if (qobject_cast<QKeySequenceEdit*>(o))
        {
            event->accept();
            return false;
        }
    if (!isTextInput(watched)) return QObject::eventFilter(watched, event);

    const Qt::KeyboardModifiers mods = ke->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
    const int key = ke->key();
    static const int editingKeys[] = { Qt::Key_Delete, Qt::Key_Backspace, Qt::Key_Home, Qt::Key_End, Qt::Key_Left,
                                       Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Plus, Qt::Key_Minus };
    bool forField = mods == Qt::NoModifier && (!ke->text().isEmpty() || std::find(std::begin(editingKeys), std::end(editingKeys), key) != std::end(editingKeys));
    for (auto sk : { QKeySequence::Copy, QKeySequence::Cut, QKeySequence::Paste, QKeySequence::Undo, QKeySequence::Redo,
                     QKeySequence::SelectAll, QKeySequence::Delete })
        forField |= ke->matches(sk);
    if (forField) event->accept(); // la touche part au champ, aucun raccourci de commande
    return false;
}

} // namespace TSA::UI::Shortcuts
