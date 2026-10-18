// Recherche globale du shell (docs/UI_SEARCH.md) : entrées tirées du catalogue des commandes (libellés dans les trois
// langues, synonymes), des panneaux du document et du registre d'automatisation (commandes de la console et des
// extensions) ; exécution par les actions existantes, jamais par une seconde implémentation.

#include "AppShell.h"
#include "UI/Theme/ThemedIcon.h"

#include "TitleBar.h"
#include "../MainWindow.h"
#include "../Search/GlobalSearchWidget.h"
#include "../Shortcuts/ShortcutManager.h"
#include "../../Automation/CommandRegistry.h"
#include "../../Commands/CommandCatalog.h"
#include "../../Commands/CommandSynonyms.h"
#include "../../Core/I18n/LanguageManager.h"

#include <QAction>
#include <QDockWidget>

namespace TSA::UI
{

using TSA::Commands::SearchEntry;
using TSA::Commands::SearchKind;
using TSA::Core::I18n::Language;
using TSA::Core::I18n::LanguageManager;
using TSA::UI::Shortcuts::ShortcutManager;

namespace
{
bool isOwnedBy(const QObject* object, const QObject* owner)
{
    for (const QObject* o = object; o; o = o->parent())
        if (o == owner) return true;
    return false;
}

QString cleanActionText(QString text)
{
    text.remove(QLatin1Char('&'));
    if (text.endsWith(QStringLiteral("..."))) text.chop(3);
    if (text.endsWith(QChar(0x2026))) text.chop(1);
    return text.trimmed();
}

SearchKind kindFor(const QString& id)
{
    if (id.startsWith(QStringLiteral("cmd.settings.")) || id.endsWith(QStringLiteral(".config")) || id == QStringLiteral("cmd.mesh.settings")
        || id == QStringLiteral("cmd.help.shortcut_editor"))
        return SearchKind::Setting;
    if (id.startsWith(QStringLiteral("cmd.window.")) || id == QStringLiteral("cmd.properties.panel")) return SearchKind::Panel;
    return SearchKind::Command;
}
} // namespace

void AppShell::setupSearch()
{
    m_search = new GlobalSearchWidget(this);
    m_search->setFixedWidth(280);
    m_search->setEntriesProvider([this] { return searchEntries(); });
    m_search->setIconProvider([this](const SearchEntry& e) -> QIcon {
        if (e.kind == SearchKind::Panel && e.id.startsWith(QStringLiteral("panel:")) && m_workspace)
            if (auto* dock = m_workspace->findChild<QDockWidget*>(e.id.mid(6))) return dock->toggleViewAction()->icon();
        if (e.kind == SearchKind::ConsoleCommand) return TSA::UI::themedIcon(QStringLiteral(":/icons/common/console.svg"));
        if (QAction* a = actionForCommand(e.id); a && !a->icon().isNull()) return a->icon();
        return QIcon();
    });
    connect(m_search, &GlobalSearchWidget::activated, this, &AppShell::executeSearchResult);
    m_titleBar->addTrailingWidget(m_search);

    m_actSearch = new QAction(TSA::UI::themedIcon(QStringLiteral(":/icons/common/search.svg")), tr("Rechercher une commande"), this);
    m_actSearch->setToolTip(tr("Retrouver une commande, un panneau ou un paramètre par son nom"));
    connect(m_actSearch, &QAction::triggered, this, [this] {
        // Plein écran : la barre de titre est masquée ; elle s'affiche le temps de la recherche
        if (!m_titleBar->isVisible())
        {
            m_titleBar->show();
            m_searchShowedTitleBar = true;
        }
        m_search->focusSearch();
    });
    connect(m_search, &GlobalSearchWidget::searchClosed, this, [this] {
        if (m_searchShowedTitleBar && isFullScreen()) m_titleBar->hide();
        m_searchShowedTitleBar = false;
    });
    addAction(m_actSearch);   // raccourci actif dans toute la fenêtre (Start Center et documents)
    auto& mgr = ShortcutManager::instance();
    mgr.bind(QStringLiteral("cmd.tools.search"), m_actSearch);
    auto syncHint = [this] {
        m_search->setShortcutText(TSA::UI::Shortcuts::sequenceListNativeText(ShortcutManager::instance().effective(QStringLiteral("cmd.tools.search"))));
    };
    syncHint();
    connect(&mgr, &ShortcutManager::shortcutsChanged, this, syncHint);
}

QAction* AppShell::actionForCommand(const QString& id) const
{
    const QList<QAction*> actions = ShortcutManager::instance().actions(id);
    // 1. Action du document au premier plan ; 2. action de l'application (shell, gestionnaire), jamais celle d'un
    //    autre document (onglet caché)
    if (m_workspace)
        for (QAction* a : actions)
            if (isOwnedBy(a, m_workspace)) return a;
    for (QAction* a : actions)
    {
        bool inDocument = false;
        for (const MainWindow* d : m_documents)
            if (isOwnedBy(a, d)) inDocument = true;
        if (!inDocument) return a;
    }
    return nullptr;
}

QList<SearchEntry> AppShell::searchEntries() const
{
    QList<SearchEntry> out;
    auto& mgr = ShortcutManager::instance();
    const QHash<const QAction*, QString> locations = m_workspace ? m_workspace->ribbonActionLocations() : QHash<const QAction*, QString>();
    const Language current = LanguageManager::currentLanguage();
    int order = 0;

    // 1. Commandes du catalogue : libellé et description dans la langue affichée, noms dans les autres langues
    for (const auto& def : ShortcutManager::catalogDefinitions())
    {
        QAction* action = actionForCommand(def.id);
        // Dans un document, une commande que la fenêtre ne propose pas n'est pas listée ; au Start Center, les
        // commandes de projet le sont (leur sélection explique qu'il faut ouvrir un projet).
        if (!action && isProjectOpen()) continue;
        if (action && !action->isVisible()) continue;
        SearchEntry e;
        e.id = def.id;
        e.kind = kindFor(def.id);
        e.title = def.name;
        e.description = def.description;
        e.shortcut = TSA::UI::Shortcuts::sequenceListNativeText(mgr.effective(def.id));
        e.location = action && locations.contains(action) ? locations.value(action) : def.category;
        e.order = order++;
        if (const auto* desc = TSA::Commands::CommandCatalog::instance().findCommand(def.id.toStdString()))
        {
            e.iconPath = QString::fromStdString(desc->iconPath);
            for (const auto& info : LanguageManager::languages())
                if (info.language != current)
                    e.keywords << LanguageManager::translate(info.language, "CommandCatalog", desc->name.c_str());
        }
        if (action)
        {
            const QString text = cleanActionText(action->text());
            if (!text.isEmpty() && text.compare(e.title, Qt::CaseInsensitive) != 0) e.keywords << text;
        }
        e.keywords << TSA::Commands::commandSynonyms(def.id);
        e.keywords.removeDuplicates();
        out << e;
    }

    // 2. Panneaux du document (ceux qui n'ont pas déjà une commande du catalogue)
    if (m_workspace)
    {
        for (QDockWidget* dock : m_workspace->findChildren<QDockWidget*>())
        {
            if (dock->objectName().isEmpty() || dock->windowTitle().isEmpty()) continue;
            if (!mgr.idOf(dock->toggleViewAction()).isEmpty()) continue;
            SearchEntry e;
            e.id = QStringLiteral("panel:") + dock->objectName();
            e.kind = SearchKind::Panel;
            e.title = dock->windowTitle();
            e.description = tr("Afficher le panneau « %1 »").arg(dock->windowTitle());
            e.location = tr("Fenêtres et panneaux");
            e.order = order++;
            out << e;
        }
    }

    // 3. Commandes du registre (console, scripts, extensions) : paramètres saisis dans la console
    for (const auto* spec : TSA::Automation::CommandRegistry::builtIn().commands())
    {
        SearchEntry e;
        e.id = QStringLiteral("console:") + QString::fromStdString(spec->id);
        e.kind = SearchKind::ConsoleCommand;
        e.title = QCoreApplication::translate("TSA::Automation::Commands", spec->title.c_str());
        e.description = QCoreApplication::translate("TSA::Automation::Commands", spec->description.c_str());
        e.location = tr("Console › %1").arg(QString::fromStdString(spec->id));
        e.keywords << QString::fromStdString(spec->id);
        for (const auto& info : LanguageManager::languages())
            if (info.language != current)
                e.keywords << LanguageManager::translate(info.language, "TSA::Automation::Commands", spec->title.c_str());
        e.keywords.removeDuplicates();
        e.order = order++;
        out << e;
    }
    return out;
}

void AppShell::executeSearchResult(const QString& id)
{
    if (id.startsWith(QStringLiteral("panel:")))
    {
        QDockWidget* dock = m_workspace ? m_workspace->findChild<QDockWidget*>(id.mid(6)) : nullptr;
        if (!dock)
        {
            m_search->showFeedback(tr("Ce panneau n'existe que dans un projet ouvert : créez ou ouvrez un projet."));
            return;
        }
        dock->show();
        dock->raise();
        m_search->showFeedback(tr("Panneau « %1 » affiché").arg(dock->windowTitle()));
        return;
    }
    if (id.startsWith(QStringLiteral("console:")))
    {
        if (!m_workspace)
        {
            m_search->showFeedback(tr("Les commandes de la console s'exécutent dans un projet : créez ou ouvrez un projet."));
            return;
        }
        const QString command = id.mid(8);
        m_workspace->prefillConsoleCommand(command + QLatin1Char(' '));
        m_search->showFeedback(tr("Complétez les paramètres de « %1 » dans la console, puis validez par Entrée.").arg(command));
        return;
    }

    const auto* def = ShortcutManager::instance().definition(id);
    QString name = def ? def->name : id;
    if (!def)
        for (const auto& d : ShortcutManager::catalogDefinitions())
            if (d.id == id) name = d.name;
    QAction* action = actionForCommand(id);
    if (!action)
    {
        m_search->showFeedback(isProjectOpen()
                                   ? tr("« %1 » n'est pas disponible dans cette fenêtre : rien n'a été exécuté.").arg(name)
                                   : tr("« %1 » demande un projet ouvert : créez ou ouvrez un projet. Rien n'a été exécuté.").arg(name));
        return;
    }
    if (!action->isEnabled())
    {
        m_search->showFeedback(tr("« %1 » est désactivée dans l'état actuel du projet (elle demande par exemple une sélection, "
                                  "un maillage, des résultats ou une action à annuler) : rien n'a été exécuté.")
                                   .arg(name));
        return;
    }
    action->trigger();
    if (action->isCheckable())
        m_search->showFeedback(tr("« %1 » : %2").arg(name, action->isChecked() ? tr("activé") : tr("désactivé")));
}

} // namespace TSA::UI
