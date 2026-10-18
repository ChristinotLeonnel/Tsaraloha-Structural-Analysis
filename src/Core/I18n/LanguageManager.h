#pragma once

// Langue de l'interface (docs/I18N.md) : français (langue source des textes), anglais, malgache.
//
//  - Les textes sont écrits en français dans le code (tr(), QT_TRANSLATE_NOOP) ; les traductions anglaise et malgache
//    sont des fichiers Qt Linguist (translations/tsa_en.ts, tsa_mg.ts) compilés en .qm et intégrés aux ressources
//    (:/i18n/tsa_<code>.qm).
//  - La langue est choisie au démarrage, avant toute fenêtre : la plupart des textes sont posés une fois à la
//    construction des widgets. Changer de langue demande donc un redémarrage (proposé, projets rouverts).
//  - Repli : un texte sans traduction s'affiche dans la langue source (français), y compris en malgache
//    (le français est langue officielle à Madagascar) ; les textes standard de Qt (boutons OK, Annuler…) suivent
//    la même règle. Les lacunes sont mesurées par tools/i18n/check_translations.py, jamais masquées.
//  - La langue ne change ni les nombres, ni les unités, ni les résultats : le format des nombres reste celui des
//    paramètres régionaux de Windows (QLocale par défaut inchangée).
//
// Préférence : clé « ui/language » des paramètres utilisateur (QSettings), séparée des projets .tsa.

#include <QList>
#include <QString>

#include <optional>

class QCoreApplication;
class QSettings;

namespace TSA::Core::I18n
{

enum class Language
{
    French,
    English,
    Malagasy
};

struct LanguageInfo
{
    Language language;
    QString code;          ///< « fr », « en », « mg » (ISO 639-1)
    QString nativeName;    ///< nom affiché dans le sélecteur, dans la langue elle-même
};

class LanguageManager
{
public:
    static const QList<LanguageInfo>& languages();
    static QString code(Language language);
    static QString nativeName(Language language);
    static std::optional<Language> fromCode(const QString& code);
    static Language defaultLanguage() { return Language::French; }

    static QString settingsKey();
    static Language storedLanguage();
    static Language storedLanguage(const QSettings& settings);
    static void storeLanguage(Language language);
    static void storeLanguage(QSettings& settings, Language language);

    /// Langue demandée par la ligne de commande (« --lang=en »), prioritaire sur la préférence, jamais enregistrée.
    static std::optional<Language> languageFromArguments(const QStringList& arguments);

    /// Installe les traductions de l'application et de Qt pour cette langue (remplace les précédentes).
    /// Rend faux si la traduction de l'application est introuvable (textes en français).
    static bool install(QCoreApplication& app, Language language);
    static Language currentLanguage();

    /// Ressource .qm de l'application (vide pour le français, langue source).
    static QString translationResource(Language language);
    static bool hasTranslation(Language language);

    /// Texte traduit dans une langue donnée sans toucher à la langue installée (synonymes de la recherche, message
    /// de redémarrage dans la langue choisie). Sans traduction : le texte source.
    static QString translate(Language language, const char* context, const char* sourceText,
                             const char* disambiguation = nullptr, int n = -1);
    /// Une traduction existe-t-elle (même identique au texte source) ? Le français, langue source, en a toujours une.
    static bool hasTranslationFor(Language language, const char* context, const char* sourceText,
                                  const char* disambiguation = nullptr);
};

} // namespace TSA::Core::I18n
