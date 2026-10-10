#pragma once

// Gestionnaire central des raccourcis clavier de TSA (source de vérité des raccourcis actifs).
//
//  - Les commandes sont décrites par TSA::Commands::CommandCatalog (identifiant stable, libellé,
//    description, raccourcis par défaut, catégorie) ; bind() relie une QAction existante à son
//    identifiant : le gestionnaire ne fait que poser ses raccourcis, la logique métier reste celle de
//    l'action.
//  - Raccourcis effectifs = fichier utilisateur shortcut.txt (QStandardPaths::AppConfigLocation) sur les
//    valeurs par défaut ; fichier créé s'il manque, jamais réécrit sans demande ; une configuration
//    invalide (syntaxe, combinaison, conflit) n'est jamais appliquée : la dernière valide reste active.
//  - Rechargement à chaud : QFileSystemWatcher (fichier + dossier, re-surveillance après remplacement),
//    regroupé par un minuteur ; contenu identique ou écrit par TSA lui-même : ignoré.
//  - Seuls les raccourcis qui changent sont reposés ; infobulles mises à jour (« … (Ctrl+S) »).
//  - « Répéter la dernière commande » (cmd.edit.repeat) et garde des champs de saisie (ShortcutInputGuard).

#include "ShortcutConfig.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class QAction;
class QFileSystemWatcher;
class QTimer;

namespace TSA::UI::Shortcuts
{

class ShortcutManager : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutManager(QObject* parent = nullptr);
    ~ShortcutManager() override;

    /// Instance de l'application (fenêtre principale, Start Center, aide).
    static ShortcutManager& instance();

    // --- Commandes ------------------------------------------------------------------------------
    /// Définitions du catalogue TSA (toutes les commandes raccourcissables).
    static QList<CommandDefinition> catalogDefinitions();
    /// Déclare une commande (tests, commandes hors catalogue). Remplace une définition existante.
    void registerDefinition(const CommandDefinition& definition);
    /// Relie une action à une commande ; la définition vient du catalogue si elle n'a pas été déclarée.
    /// Identifiant inconnu : commande déclarée sans raccourci par défaut, signalée (false).
    bool bind(const QString& id, QAction* action);
    void unbind(QAction* action);

    QList<CommandDefinition> definitions() const;          ///< commandes reliées à au moins une action
    const CommandDefinition* definition(const QString& id) const;
    QList<QAction*> actions(const QString& id) const;
    QString idOf(const QAction* action) const;
    QList<QKeySequence> effective(const QString& id) const;
    QList<QKeySequence> defaults(const QString& id) const;
    bool isEnabled(const QString& id) const;

    // --- Configuration ----------------------------------------------------------------------------
    static QString defaultConfigPath();
    void setConfigPath(const QString& path);                 ///< (re)surveille ce fichier
    QString configPath() const { return m_path; }
    /// Charge shortcut.txt (créé avec les valeurs par défaut s'il manque). false : fichier illisible ou
    /// invalide, valeurs par défaut conservées (message dans lastErrors()).
    bool loadConfig();
    /// Relit le fichier ; invalide : dernière configuration valide conservée.
    bool reload();
    /// Valide (conflits compris) puis applique ; rien n'est appliqué en cas d'erreur.
    bool applySettings(const ShortcutSettings& settings, QStringList* errors = nullptr);
    /// Valide, écrit le fichier (écriture atomique) puis applique.
    bool saveSettings(const ShortcutSettings& settings, QStringList* errors = nullptr);
    ShortcutSettings settings() const { return m_settings; }
    QString defaultConfigText() const;                      ///< fichier complet avec les valeurs par défaut
    QString currentConfigText() const;                      ///< fichier complet avec les réglages actifs
    QStringList lastErrors() const { return m_lastErrors; }
    QStringList lastWarnings() const { return m_lastWarnings; }
    /// Délai de regroupement des notifications du système de fichiers (ms).
    void setReloadDelay(int ms);

    /// Référence HTML des raccourcis actifs (aide, F1).
    QString htmlReference() const;

    /// Action « Répéter la dernière commande » (cmd.edit.repeat), à ajouter à la fenêtre principale.
    QAction* repeatAction();

signals:
    /// Configuration appliquée (summary : « 128 raccourcis actifs, 3 désactivés… »).
    void configApplied(const QString& summary, const QStringList& warnings);
    /// Configuration refusée : la précédente reste active.
    void configRejected(const QStringList& errors);
    void shortcutsChanged();

private:
    void applyToActions();
    void applyToAction(QAction* action, const QList<QKeySequence>& sequences);
    void watchPath();
    void onFileSystemChanged();
    bool readFile(QString* text, QString* error) const;
    bool writeFile(const QString& text, QString* error);
    QString summary() const;
    void rememberLastCommand(QAction* action);

    QHash<QString, CommandDefinition> m_definitions;
    QStringList m_order;                                     ///< ordre de déclaration
    QHash<QString, QList<QPointer<QAction>>> m_actions;
    ShortcutSettings m_settings;                             ///< dernière configuration valide
    QString m_path;
    QString m_lastText;                                      ///< contenu lu ou écrit en dernier
    QStringList m_lastErrors, m_lastWarnings;
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_reloadTimer = nullptr;
    QAction* m_repeatAction = nullptr;
    QPointer<QAction> m_lastCommand;
};

/// Garde des champs de saisie : dans un champ de texte, une touche de saisie (lettre, chiffre, Suppr,
/// flèches) ou d'édition standard (Ctrl+C, Ctrl+V, Ctrl+Z…) va au champ et ne déclenche aucun raccourci
/// de commande ; dans un enregistreur de raccourci (QKeySequenceEdit), toutes les touches vont au champ.
class ShortcutInputGuard : public QObject
{
    Q_OBJECT
public:
    explicit ShortcutInputGuard(QObject* parent = nullptr) : QObject(parent) {}
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// Le widget est-il un champ de saisie de texte modifiable ?
    static bool isTextInput(const QObject* widget);
};

} // namespace TSA::UI::Shortcuts
