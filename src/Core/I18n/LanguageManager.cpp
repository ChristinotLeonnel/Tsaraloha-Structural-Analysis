#include "LanguageManager.h"

#include <QCoreApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

#include <map>
#include <memory>

namespace TSA::Core::I18n
{

namespace
{
struct InstalledState
{
    Language language = Language::French;
    std::unique_ptr<QTranslator> application;
    std::unique_ptr<QTranslator> qtBase;
};

InstalledState& installed()
{
    static InstalledState state;
    return state;
}

/// Traducteurs chargés à la demande pour translate() (jamais installés dans l'application).
QTranslator* lookupTranslator(Language language)
{
    static std::map<Language, std::unique_ptr<QTranslator>> cache;
    auto it = cache.find(language);
    if (it == cache.end())
    {
        std::unique_ptr<QTranslator> t;
        const QString resource = LanguageManager::translationResource(language);
        if (!resource.isEmpty())
        {
            t = std::make_unique<QTranslator>();
            if (!t->load(resource)) t.reset();
        }
        it = cache.emplace(language, std::move(t)).first;
    }
    return it->second.get();
}

/// Textes standard de Qt : anglais = textes d'origine de Qt ; français et malgache = traduction française de Qt
/// (repli documenté du malgache, Qt ne fournit pas de traduction malgache).
bool loadQtBase(QTranslator& translator, Language language)
{
    if (language == Language::English) return false;
    const QLocale locale(QLocale::French);
    const QString deployed = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    return translator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), deployed)
        || translator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
}
} // namespace

const QList<LanguageInfo>& LanguageManager::languages()
{
    // Noms dans la langue elle-même : chacun retrouve la sienne quelle que soit la langue affichée.
    static const QList<LanguageInfo> list = {
        { Language::English, QStringLiteral("en"), QStringLiteral("English") },
        { Language::French, QStringLiteral("fr"), QStringLiteral("Français") },
        { Language::Malagasy, QStringLiteral("mg"), QStringLiteral("Malagasy") },
    };
    return list;
}

QString LanguageManager::code(Language language)
{
    for (const auto& info : languages())
        if (info.language == language) return info.code;
    return QStringLiteral("fr");
}

QString LanguageManager::nativeName(Language language)
{
    for (const auto& info : languages())
        if (info.language == language) return info.nativeName;
    return QString();
}

std::optional<Language> LanguageManager::fromCode(const QString& code)
{
    const QString c = code.trimmed().toLower().left(2);
    for (const auto& info : languages())
        if (info.code == c) return info.language;
    return std::nullopt;
}

QString LanguageManager::settingsKey()
{
    return QStringLiteral("ui/language");
}

Language LanguageManager::storedLanguage()
{
    QSettings settings;
    return storedLanguage(settings);
}

Language LanguageManager::storedLanguage(const QSettings& settings)
{
    return fromCode(settings.value(settingsKey()).toString()).value_or(defaultLanguage());
}

void LanguageManager::storeLanguage(Language language)
{
    QSettings settings;
    storeLanguage(settings, language);
}

void LanguageManager::storeLanguage(QSettings& settings, Language language)
{
    settings.setValue(settingsKey(), code(language));
    settings.sync();
}

std::optional<Language> LanguageManager::languageFromArguments(const QStringList& arguments)
{
    for (const QString& arg : arguments)
        if (arg.startsWith(QStringLiteral("--lang="))) return fromCode(arg.mid(7));
    return std::nullopt;
}

QString LanguageManager::translationResource(Language language)
{
    if (language == Language::French) return QString();
    return QStringLiteral(":/i18n/tsa_%1.qm").arg(code(language));
}

bool LanguageManager::hasTranslation(Language language)
{
    return language == Language::French || QFile::exists(translationResource(language));
}

bool LanguageManager::install(QCoreApplication& app, Language language)
{
    InstalledState& state = installed();
    if (state.application) app.removeTranslator(state.application.get());
    if (state.qtBase) app.removeTranslator(state.qtBase.get());
    state.application.reset();
    state.qtBase.reset();
    state.language = language;

    auto qt = std::make_unique<QTranslator>();
    if (loadQtBase(*qt, language))
    {
        app.installTranslator(qt.get());
        state.qtBase = std::move(qt);
    }

    if (language == Language::French) return true;
    auto tsa = std::make_unique<QTranslator>();
    if (!tsa->load(translationResource(language))) return false;
    app.installTranslator(tsa.get());   // installé en dernier : consulté avant celui de Qt
    state.application = std::move(tsa);
    return true;
}

Language LanguageManager::currentLanguage()
{
    return installed().language;
}

QString LanguageManager::translate(Language language, const char* context, const char* sourceText,
                                   const char* disambiguation, int n)
{
    const QString source = QString::fromUtf8(sourceText);
    if (language == Language::French) return source;
    if (QTranslator* t = lookupTranslator(language))
    {
        const QString out = t->translate(context, sourceText, disambiguation, n);
        if (!out.isEmpty()) return out;
    }
    return source;
}

bool LanguageManager::hasTranslationFor(Language language, const char* context, const char* sourceText,
                                        const char* disambiguation)
{
    if (language == Language::French) return true;
    QTranslator* t = lookupTranslator(language);
    return t && !t->translate(context, sourceText, disambiguation).isEmpty();
}

} // namespace TSA::Core::I18n
