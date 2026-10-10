#pragma once

// Configuration des raccourcis clavier (fichier shortcut.txt) : lecture, écriture, validation des
// combinaisons, raccourcis effectifs et conflits. Sans widget ni action : testé indépendamment de
// ShortcutManager, qui applique le résultat aux QAction.
//
// Format d'une ligne de commande (les autres lignes sont des commentaires « # » ou des titres « [ ] ») :
//   identifiant | raccourci(s) | état | par défaut | description
// Seuls les trois premiers champs sont lus ; « par défaut » et « description » sont informatifs et
// régénérés. Plusieurs raccourcis (alias) sont séparés par « ; ». Une suite de touches (accord) s'écrit
// « D, A ». État : on / off. Raccourci vide ou « aucun » : pas de raccourci (« - » est la touche moins).

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

namespace TSA::UI::Shortcuts
{

/// Portée d'un raccourci : deux commandes ne sont en conflit que si leurs portées se recouvrent.
/// « window » (fenêtre principale, vue 3D comprise) recouvre « viewport » ; les autres portées
/// (« startcenter », « dialog:<nom> ») sont disjointes.
bool scopesOverlap(const QString& a, const QString& b);

struct CommandDefinition
{
    QString id;                     ///< identifiant stable (ex. cmd.file.save)
    QString name;                   ///< libellé (traduisible, jamais utilisé comme clé)
    QString description;
    QString category;               ///< libellé de catégorie (titre de section du fichier)
    int categoryRank = 0;           ///< ordre des sections
    QList<QKeySequence> defaults;   ///< raccourcis par défaut
    QString scope = QStringLiteral("window");
};

struct ShortcutSetting
{
    QList<QKeySequence> sequences;
    bool enabled = true;
    bool operator==(const ShortcutSetting& o) const { return sequences == o.sequences && enabled == o.enabled; }
};
using ShortcutSettings = QMap<QString, ShortcutSetting>;   ///< identifiant → réglage du fichier

struct ParseResult
{
    bool ok = false;                ///< aucune erreur bloquante (la configuration peut être appliquée)
    ShortcutSettings settings;      ///< commandes connues présentes dans le fichier
    QStringList errors;             ///< syntaxe, combinaison invalide, état invalide, identifiant en double
    QStringList warnings;           ///< identifiant inconnu, version de format plus récente
    int formatVersion = 0;          ///< version déclarée par l'en-tête « # format: tsa-shortcuts N » (0 : absente)
};

/// Version actuelle du format de shortcut.txt.
int shortcutFormatVersion();

struct Conflict
{
    QString idA, idB;
    QKeySequence sequenceA, sequenceB;
    bool prefix = false;            ///< sequenceA est le début de sequenceB (Qt attendrait la suite)
    QString message() const;
};

/// Combinaison utilisable : au moins une touche, aucune touche inconnue, pas de modificateur seul.
bool isValidSequence(const QKeySequence& sequence);
/// Analyse « Ctrl+S ; F7 » (noms français acceptés : Maj, Suppr, Échap, Inser, Gauche…).
bool parseSequenceList(const QString& text, QList<QKeySequence>* out, QString* error = nullptr);
/// Texte portable « Ctrl+S ; F7 » (celui du fichier).
QString sequenceListText(const QList<QKeySequence>& sequences);
/// Texte natif « Ctrl+S / F7 » (menus, infobulles).
QString sequenceListNativeText(const QList<QKeySequence>& sequences);

ParseResult parseConfig(const QString& text, const QSet<QString>& knownIds);
/// Fichier complet : en-tête commenté, une section par catégorie, une ligne par commande.
QString formatConfig(const QList<CommandDefinition>& definitions, const ShortcutSettings& settings);

/// Raccourcis effectifs : réglage du fichier s'il existe (aucun si désactivé), sinon valeurs par défaut.
QList<QKeySequence> effectiveSequences(const CommandDefinition& definition, const ShortcutSettings& settings);
/// Conflits exacts ou de préfixe entre commandes dont les portées se recouvrent.
QList<Conflict> findConflicts(const QList<CommandDefinition>& definitions, const ShortcutSettings& settings);

/// Retire d'une infobulle un suffixe « (Ctrl+S) », « (G / F7) », « (Maj+F) »… entièrement composé de
/// raccourcis ; « (Fx, Fy, Fz) » ou « (LCS) » sont conservés.
QString stripShortcutHint(const QString& toolTip);

} // namespace TSA::UI::Shortcuts
