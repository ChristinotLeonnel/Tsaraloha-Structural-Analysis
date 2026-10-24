// Suite « i18n » : langues de l'interface, recherche globale, icônes et ruban (docs/I18N.md, docs/UI_SEARCH.md,
// docs/ICONS.md).
//   450  langues : codes, noms affichés dans la langue elle-même, préférence enregistrée et relue (fichier de réglages
//        isolé, comme après un redémarrage), « --lang= » prioritaire et jamais enregistré, code inconnu → français ;
//   451  traductions intégrées : :/i18n/tsa_en.qm et tsa_mg.qm chargés ; textes de référence traduits en anglais et en
//        malgache ; texte absent → texte source (repli documenté) ; traduction absente détectable ;
//   452  installation : QCoreApplication::translate suit la langue installée (anglais, malgache, retour au français) ;
//        textes standard de Qt en français pour le malgache (repli), d'origine pour l'anglais ;
//   453  caractères : accents français et lettres malgaches conservés par les traductions et affichables (police) ;
//   454  catalogue des commandes : libellés et descriptions présents en anglais et en malgache (aucune lacune) ;
//        placeholders %1… conservés ;
//   455  recherche, comparaison : normalisation (casse, accents, ligatures, apostrophes), distance avec transposition ;
//   456  recherche, pertinence : nom affiché, nom traduit, synonyme, sans accent, partiel, faute de frappe ; libellé exact
//        avant fragment ; mots courts facultatifs ; aucun doublon ;
//   457  recherche, rapidité : 6 000 entrées, requêtes sous 50 ms ;
//   458  champ de recherche : liste, flèches, Entrée (commande émise), Échap (fermer puis effacer), « aucun résultat »
//        jamais exécutable, touches réservées au champ (ShortcutOverride) ;
//   459  boîte « Langue » : explications dans la langue sélectionnée, boutons de redémarrage seulement si la langue change ;
//   460  icônes du catalogue présentes et dessinées dans les deux thèmes ;
//   461  contraste avec le fond du ruban : forme >= 3:1 (thèmes sombre et clair), état désactivé >= 1,8:1 ;
//   462  deux commandes différentes n'ont jamais la même image (exceptions justifiées listées) ;
//   463  moteur d'icônes : palette par thème, état désactivé, ressource absente détectable ;
// Suite « ribbon » (même fichier) :
//   464  dix onglets dans l'ordre du travail ; chaque commande de la fenêtre à une seule place ; aucun bouton fictif ;
//   465  commande absente de la fenêtre : ni groupe vide ni bouton de remplacement ;
//   466  fenêtre de 1920 à 600 px : chaque commande reste atteignable (groupes réduits, repliés, menu « Plus ») ;
//   467  ruban en anglais et en malgache : titres traduits, rangée d'onglets sans débordement.

#include "test_common.h"

#include "Commands/CommandCatalog.h"
#include "Commands/CommandSearchIndex.h"
#include "Commands/CommandSynonyms.h"
#include "Core/I18n/LanguageManager.h"
#include "UI/Dialogs/LanguageDialog.h"
#include "UI/Search/GlobalSearchWidget.h"
#include "UI/Theme/ThemedIcon.h"
#include "UI/Ribbon/RibbonBar.h"
#include "UI/Ribbon/RibbonBuilder.h"
#include "UI/Ribbon/RibbonPanel.h"
#include "UI/Ribbon/RibbonTab.h"

#include <QAction>
#include <QMenu>
#include <QTabBar>
#include <set>

#include <QElapsedTimer>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>
#include <clocale>
#include <cstdlib>

using TSA::Commands::CommandCatalog;
using TSA::Commands::CommandSearchIndex;
using TSA::Commands::SearchEntry;
using TSA::Commands::SearchKind;
using TSA::Core::I18n::Language;
using TSA::Core::I18n::LanguageManager;

namespace
{
QSet<QString> placeholders(const QString& s)
{
    QSet<QString> out;
    static const QRegularExpression re(QStringLiteral("%L?\\d+|%n"));
    for (auto it = re.globalMatch(s); it.hasNext();) out.insert(it.next().captured(0));
    return out;
}

SearchEntry entry(const QString& id, const QString& title, const QStringList& keywords = {}, const QString& description = {})
{
    SearchEntry e;
    e.id = id;
    e.title = title;
    e.keywords = keywords;
    e.description = description;
    return e;
}

/// Réinstalle le français à la sortie du bloc (les suites suivantes ne doivent jamais hériter d'une autre langue).
struct FrenchOnExit
{
    ~FrenchOnExit() { LanguageManager::install(*QCoreApplication::instance(), Language::French); }
};

QString topId(const CommandSearchIndex& index, const QString& query)
{
    const auto hits = index.search(query, 5);
    return hits.isEmpty() ? QString() : index.entries()[hits.first().entry].id;
}
} // namespace

bool runSuite_I18n(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 490 : locale numérique « C » malgré la locale du système (src/Core/NumericLocale.cpp)
    // Sous Unix, QCoreApplication applique la locale du système : avec fr_FR, strtod lisait « 3.5 » → 3.
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(std::localeconv()->decimal_point[0] == '.', "Test 490: séparateur décimal « . » pour la bibliothèque C (LC_NUMERIC="
                                                                    << std::setlocale(LC_NUMERIC, nullptr) << ")");
        TEST_CHECK(std::strtod("3.5", nullptr) == 3.5 && std::stod("-0.01282209874953716") == -0.01282209874953716,
                   "Test 490: strtod / std::stod lisent le point décimal");
        TEST_CHECK(std::to_string(0.5) == "0.500000", "Test 490: std::to_string écrit le point décimal");
        std::cout << "[PASS] Test 490: Locale numérique C (LANG=" << (std::getenv("LANG") ? std::getenv("LANG") : "") << ")" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 450 : langues et préférence
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(LanguageManager::languages().size() == 3, "Test 450: trois langues proposées");
        TEST_CHECK(LanguageManager::nativeName(Language::English) == "English" && LanguageManager::nativeName(Language::French) == QString::fromUtf8("Français")
                       && LanguageManager::nativeName(Language::Malagasy) == "Malagasy",
                   "Test 450: noms des langues dans la langue elle-même");
        TEST_CHECK(LanguageManager::fromCode("EN") == Language::English && LanguageManager::fromCode("mg-MG") == Language::Malagasy
                       && LanguageManager::fromCode("fr") == Language::French && !LanguageManager::fromCode("de").has_value(),
                   "Test 450: codes ISO reconnus, code inconnu refusé");
        QTemporaryDir dir;
        const QString ini = dir.filePath("tsa_settings.ini");
        {
            QSettings s(ini, QSettings::IniFormat);
            TEST_CHECK(LanguageManager::storedLanguage(s) == Language::French, "Test 450: sans préférence, français (langue initiale)");
            LanguageManager::storeLanguage(s, Language::Malagasy);
        }
        {
            QSettings relu(ini, QSettings::IniFormat);   // nouvel objet : comme après un redémarrage
            TEST_CHECK(LanguageManager::storedLanguage(relu) == Language::Malagasy, "Test 450: préférence relue (malgache)");
            TEST_CHECK(relu.allKeys() == QStringList{ LanguageManager::settingsKey() } && relu.value(LanguageManager::settingsKey()).toString() == "mg",
                       "Test 450: une seule clé « ui/language » dans les réglages (rien dans les projets)");
            relu.setValue(LanguageManager::settingsKey(), "xx");
            TEST_CHECK(LanguageManager::storedLanguage(relu) == Language::French, "Test 450: valeur invalide → français");
        }
        TEST_CHECK(LanguageManager::languageFromArguments({ "TSA.exe", "--lang=en", "projet.tsa" }) == Language::English
                       && !LanguageManager::languageFromArguments({ "TSA.exe", "projet.tsa" }).has_value(),
                   "Test 450: « --lang=en » reconnu sur la ligne de commande");
        std::cout << "[PASS] Test 450: Langues, préférence enregistrée et relue, ligne de commande" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 451 : traductions intégrées et repli
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(LanguageManager::hasTranslation(Language::English) && LanguageManager::hasTranslation(Language::Malagasy),
                   "Test 451: :/i18n/tsa_en.qm et :/i18n/tsa_mg.qm intégrés (Qt LinguistTools présent à la compilation)");
        QTranslator en, mg;
        TEST_CHECK(en.load(LanguageManager::translationResource(Language::English)) && mg.load(LanguageManager::translationResource(Language::Malagasy)),
                   "Test 451: fichiers .qm lisibles");
        const struct { const char* ctx; const char* fr; const char* en; const char* mg; } refs[] = {
            { "CommandCatalog", "Nouveau projet", "New project", "Tetikasa vaovao" },
            { "CommandCatalog", "Enregistrer", "Save", "Tehirizo" },
            { "CommandCatalog", "Ouvrir un projet", "Open project", "Sokafy tetikasa" },
            { "CommandCategory", "Charges", "Loads", "Enta" },
            { "CommandCatalog", "Langue de l'interface", "Interface language", "Fitenin'ny fifandraisana" },
        };
        for (const auto& r : refs)
        {
            const QString gotEn = LanguageManager::translate(Language::English, r.ctx, r.fr);
            const QString gotMg = LanguageManager::translate(Language::Malagasy, r.ctx, r.fr);
            TEST_CHECK(gotEn == QString::fromUtf8(r.en) && gotMg == QString::fromUtf8(r.mg),
                       "Test 451: « " << r.fr << " » → « " << gotEn.toStdString() << " » / « " << gotMg.toStdString() << " »");
        }
        TEST_CHECK(LanguageManager::translate(Language::French, "CommandCatalog", "Nouveau projet") == "Nouveau projet",
                   "Test 451: français = texte source");
        const char* unknown = "Texte absent de toute traduction 451";
        TEST_CHECK(LanguageManager::translate(Language::Malagasy, "CommandCatalog", unknown) == QString::fromUtf8(unknown)
                       && !LanguageManager::hasTranslationFor(Language::Malagasy, "CommandCatalog", unknown)
                       && LanguageManager::hasTranslationFor(Language::Malagasy, "CommandCatalog", "Nouveau projet"),
                   "Test 451: texte non traduit → texte source (repli), lacune détectable");
        std::cout << "[PASS] Test 451: Traductions anglaise et malgache intégrées, repli sur le français détectable" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 452 : installation dans l'application
    // -------------------------------------------------------------------------
    {
        auto& app = *QCoreApplication::instance();
        FrenchOnExit restoreFrench;
        TEST_CHECK(LanguageManager::install(app, Language::English), "Test 452: anglais installé");
        const QString en = QCoreApplication::translate("CommandCatalog", "Nouveau projet");
        const QString qtCancelEn = QCoreApplication::translate("QPlatformTheme", "Cancel");
        TEST_CHECK(LanguageManager::install(app, Language::Malagasy), "Test 452: malgache installé");
        const QString mg = QCoreApplication::translate("CommandCatalog", "Nouveau projet");
        const QString qtCancelMg = QCoreApplication::translate("QPlatformTheme", "Cancel");
        const Language during = LanguageManager::currentLanguage();
        TEST_CHECK(LanguageManager::install(app, Language::French), "Test 452: retour au français");
        const QString fr = QCoreApplication::translate("CommandCatalog", "Nouveau projet");
        TEST_CHECK(en == "New project" && mg == "Tetikasa vaovao" && fr == "Nouveau projet" && during == Language::Malagasy,
                   "Test 452: textes de l'application selon la langue installée (« " << en.toStdString() << " », « " << mg.toStdString() << " »)");
        TEST_CHECK(qtCancelEn == "Cancel" && qtCancelMg == "Annuler",
                   "Test 452: boutons standard de Qt : « " << qtCancelEn.toStdString() << " » (anglais), « " << qtCancelMg.toStdString() << " » (malgache → français)");
        std::cout << "[PASS] Test 452: Langue installée appliquée aux textes de l'application et de Qt" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 453 : caractères accentués et malgaches
    // -------------------------------------------------------------------------
    {
        const QString fr = QString::fromUtf8("Éléments filaires, nœuds, flèche, à, ç, ô");
        const QString mg = LanguageManager::translate(Language::Malagasy, "CommandCatalog", "Fermer le projet");
        TEST_CHECK(mg != "Fermer le projet" && !mg.contains(QChar::ReplacementCharacter), "Test 453: traduction malgache sans caractère de remplacement");
        TEST_CHECK(fr.toUtf8() == QByteArray("\xC3\x89l\xC3\xA9ments filaires, n\xC5\x93uds, fl\xC3\xA8" "che, \xC3\xA0, \xC3\xA7, \xC3\xB4"),
                   "Test 453: source UTF-8 conservée");
        if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        {
            // Plateforme « offscreen » : aucune police réelle (même « É » y est absent) ; le contrôle des glyphes n'a de
            // sens qu'avec les polices de Windows
            std::cout << "[SKIP] Test 453: polices indisponibles sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
        }
        else
        {
            QLabel label(fr + QStringLiteral(" — ") + mg + QString::fromUtf8(" — Fanohanana raikitra, Fitaovana, ô ñ"));
            const QFontMetrics fm(label.font());
            for (const QChar c : label.text())
                TEST_CHECK(c.isSpace() || fm.inFontUcs4(c.unicode()), "Test 453: caractère « " << QString(c).toStdString() << " » affichable par la police de l'interface");
            std::cout << "[PASS] Test 453: Accents français et textes malgaches conservés et affichables" << std::endl;
            ++passed;
        }
    }

    // -------------------------------------------------------------------------
    // TEST 454 : catalogue des commandes traduit (libellés, descriptions)
    // -------------------------------------------------------------------------
    {
        QStringList missing;
        int checked = 0;
        for (const auto& [id, d] : CommandCatalog::instance().allCommands())
        {
            for (const std::string* text : { &d.name, &d.description })
            {
                if (text->empty()) continue;
                ++checked;
                for (Language l : { Language::English, Language::Malagasy })
                {
                    if (!LanguageManager::hasTranslationFor(l, "CommandCatalog", text->c_str()))
                        missing << QStringLiteral("%1 [%2] %3").arg(LanguageManager::code(l), QString::fromStdString(id), QString::fromStdString(*text));
                    else if (placeholders(LanguageManager::translate(l, "CommandCatalog", text->c_str())) != placeholders(QString::fromStdString(*text)))
                        missing << QStringLiteral("%1 [%2] paramètres %n/%1 différents").arg(LanguageManager::code(l), QString::fromStdString(id));
                }
            }
        }
        for (int i = 0; i < std::min<int>(10, missing.size()); ++i) std::cerr << "  manquant : " << missing[i].toStdString() << std::endl;
        TEST_CHECK(missing.isEmpty() && checked > 500,
                   "Test 454: " << missing.size() << " lacune(s) sur " << checked << " textes du catalogue × 2 langues");
        std::cout << "[PASS] Test 454: Catalogue des commandes traduit en anglais et en malgache (" << checked << " textes)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 455 : normalisation et distance
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(CommandSearchIndex::normalize(QString::fromUtf8("  Modèle  ŒUVRE l’été, Valin'ny kajy ")) == "modele oeuvre l ete valin ny kajy",
                   "Test 455: casse, accents, ligature, apostrophes → « " << CommandSearchIndex::normalize(QString::fromUtf8("  Modèle  ŒUVRE l’été, Valin'ny kajy ")).toStdString() << " »");
        TEST_CHECK(CommandSearchIndex::boundedDistance("poutre", "poutre", 2) == 0 && CommandSearchIndex::boundedDistance("poutre", "poutte", 2) == 1
                       && CommandSearchIndex::boundedDistance("beam", "baem", 1) == 1 && CommandSearchIndex::boundedDistance("abc", "xyz", 1) == 2,
                   "Test 455: distance (substitution, transposition, borne dépassée)");
        TEST_CHECK(CommandSearchIndex::isStopWord("de") && CommandSearchIndex::isStopWord("the") && CommandSearchIndex::isStopWord("ny")
                       && !CommandSearchIndex::isStopWord("poutre"),
                   "Test 455: mots courts facultatifs (fr, en, mg)");
        std::cout << "[PASS] Test 455: Comparaison sans casse ni accents, distance de frappe" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 456 : pertinence de la recherche
    // -------------------------------------------------------------------------
    {
        CommandSearchIndex index;
        auto beam = entry("cmd.create.beam", QString::fromUtf8("Poutre"), { "Beam", "Andry mitsivalana" }, QString::fromUtf8("Dessiner une poutre entre deux nœuds"));
        beam.keywords << TSA::Commands::commandSynonyms("cmd.create.beam");
        index.add(beam);
        index.add(entry("cmd.create.column", QString::fromUtf8("Poteau"), { "Column", "Andry" }, QString::fromUtf8("Dessiner un poteau vertical")));
        index.add(entry("cmd.coord.workplane_xy", QString::fromUtf8("Plan de travail XY"), { "Work plane XY", "Planina fiasana XY" }));
        index.add(entry("cmd.view.top", QString::fromUtf8("Vue de dessus"), { "Top view", "Fijery avy ambony" }, QString::fromUtf8("Plan XY vu du dessus")));
        index.add(entry("cmd.results.diagram_mz", QString::fromUtf8("Diagramme Mz"), { "Mz diagram" }, QString::fromUtf8("Moment fléchissant autour de l'axe fort")));
        index.add(entry("cmd.loads.distributed", QString::fromUtf8("Charge répartie"), { "Distributed load", "Enta miparitaka" }));
        index.add(entry("cmd.file.save", QString::fromUtf8("Enregistrer"), { "Save", "Tehirizo" }));
        index.add(entry("cmd.file.save_as", QString::fromUtf8("Enregistrer sous"), { "Save as", "Tehirizo amin'ny anarana hafa" }));
        index.add(entry("cmd.create.beam", QString::fromUtf8("Poutre"), beam.keywords, beam.description));   // même identifiant : remplacé
        TEST_CHECK(index.size() == 8, "Test 456: identifiant en double remplacé, jamais ajouté");

        const struct { const char* query; const char* expected; const char* what; } cases[] = {
            { "Poutre", "cmd.create.beam", "nom affiché exact" },
            { "beam", "cmd.create.beam", "nom anglais" },
            { "mitsivalana", "cmd.create.beam", "nom malgache" },
            { "IPE", "cmd.create.beam", "synonyme (profilé)" },
            { "POUTRÉ", "cmd.create.beam", "casse et accent ignorés" },
            { "pout", "cmd.create.beam", "début de mot" },
            { "poutte", "cmd.create.beam", "faute de frappe" },
            { "colum", "cmd.create.column", "anglais incomplet" },
            { "plan de travail", "cmd.coord.workplane_xy", "plusieurs mots, mot court facultatif" },
            { "planina fiasana", "cmd.coord.workplane_xy", "malgache, plusieurs mots" },
            { "enregistrer", "cmd.file.save", "libellé exact avant « Enregistrer sous »" },
            { "save as", "cmd.file.save_as", "anglais, deux mots" },
            { "repartie", "cmd.loads.distributed", "sans accent" },
            { "flechissant", "cmd.results.diagram_mz", "mot de la description" },
        };
        for (const auto& c : cases)
        {
            const QString got = topId(index, QString::fromUtf8(c.query));
            TEST_CHECK(got == c.expected, "Test 456: « " << c.query << " » (" << c.what << ") → " << got.toStdString() << ", attendu " << c.expected);
        }
        TEST_CHECK(index.search("zzzz qqqq").isEmpty() && index.search("").isEmpty(), "Test 456: requête sans rapport ou vide → aucun résultat");
        const auto hits = index.search("plan", 12);
        QSet<int> seen;
        for (const auto& h : hits) seen.insert(h.entry);
        TEST_CHECK(seen.size() == hits.size() && hits.size() >= 2, "Test 456: résultats sans doublon (« plan » : " << hits.size() << ")");
        std::cout << "[PASS] Test 456: Recherche par nom, traduction, synonyme, accent, fragment et faute de frappe" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 457 : rapidité sur un grand index
    // -------------------------------------------------------------------------
    {
        CommandSearchIndex index;
        const QStringList words = { "poutre", "poteau", "charge", "maillage", "résultat", "plan", "grille", "nœud", "dalle", "voile",
                                    "beam", "column", "load", "mesh", "result", "andry", "enta", "harato", "takelaka", "rindrina" };
        for (int i = 0; i < 6000; ++i)
        {
            const QString a = words[i % words.size()], b = words[(i / 7) % words.size()], c = words[(i / 13) % words.size()];
            index.add(entry(QStringLiteral("ext.cmd.%1").arg(i), QStringLiteral("%1 %2 %3").arg(a, b).arg(i), { c, b + QStringLiteral(" ") + a },
                            QStringLiteral("Commande d'extension %1 : %2 et %3").arg(i).arg(a, c)));
        }
        QElapsedTimer timer;
        const QStringList queries = { "pou", "charge mail", "harato", "resultat", "poutte", "beam column", "x", "grille plan noeud" };
        qint64 worst = 0;
        for (const QString& q : queries)
        {
            timer.start();
            const auto r = index.search(q, 12);
            worst = std::max(worst, timer.elapsed());
            TEST_CHECK(r.size() <= 12, "Test 457: au plus 12 résultats");
        }
        TEST_CHECK(worst < 50, "Test 457: requête la plus lente " << worst << " ms sur 6 000 entrées (< 50 ms)");
        std::cout << "[PASS] Test 457: Recherche rapide sur 6 000 entrées (pire requête " << worst << " ms)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 458 : champ de recherche au clavier
    // -------------------------------------------------------------------------
    {
        TSA::UI::GlobalSearchWidget w;
        w.resize(300, 30);
        w.show();   // liste déroulante affichée sous le champ, comme dans la barre de titre
        w.setEntriesProvider([] {
            return QList<SearchEntry>{ entry("cmd.create.beam", "Poutre", { "Beam" }), entry("cmd.create.column", "Poteau", { "Column" }),
                                       entry("cmd.create.slab", "Dalle", { "Slab" }) };
        });
        QStringList emitted;
        QObject::connect(&w, &TSA::UI::GlobalSearchWidget::activated, [&emitted](const QString& id) { emitted << id; });
        QLineEdit* edit = w.lineEdit();
        auto key = [edit](int k) {
            QKeyEvent override(QEvent::ShortcutOverride, k, Qt::NoModifier);
            QCoreApplication::sendEvent(edit, &override);
            const bool reserved = override.isAccepted();
            QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
            QCoreApplication::sendEvent(edit, &press);
            return reserved;
        };
        edit->setText("po");
        w.runSearchNow();
        TEST_CHECK(w.resultIds() == QStringList({ "cmd.create.beam", "cmd.create.column" }) && w.resultsList()->currentRow() == 0,
                   "Test 458: « po » → Poutre, Poteau ; premier résultat présélectionné");
        TEST_CHECK(key(Qt::Key_Down) && w.resultsList()->currentRow() == 1, "Test 458: flèche bas → second résultat (touche réservée au champ)");
        TEST_CHECK(key(Qt::Key_Return) && emitted == QStringList{ "cmd.create.column" },
                   "Test 458: Entrée → commande « cmd.create.column » émise");
        edit->setText("zzzz");
        w.runSearchNow();
        TEST_CHECK(w.resultIds().isEmpty() && w.resultsList()->count() == 1, "Test 458: « aucun résultat » affiché");
        key(Qt::Key_Return);
        TEST_CHECK(emitted.size() == 1, "Test 458: la ligne « aucun résultat » n'exécute rien");
        edit->setText("dal");
        w.runSearchNow();
        TEST_CHECK(w.isPopupVisible(), "Test 458: liste ouverte");
        TEST_CHECK(key(Qt::Key_Escape) && !w.isPopupVisible() && edit->text() == "dal", "Test 458: Échap ferme la liste (texte conservé ; « Mode sélection » non déclenché)");
        key(Qt::Key_Escape);
        TEST_CHECK(edit->text().isEmpty(), "Test 458: second Échap efface la recherche");
        TEST_CHECK(!edit->accessibleName().isEmpty() && !w.resultsList()->accessibleName().isEmpty() && !edit->placeholderText().isEmpty(),
                   "Test 458: noms accessibles et texte indicatif");
        std::cout << "[PASS] Test 458: Champ de recherche : liste, clavier, exécution émise, fermeture, accessibilité" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 459 : boîte « Langue de l'interface »
    // -------------------------------------------------------------------------
    {
        TSA::UI::LanguageDialog dialog(Language::French);
        auto* list = dialog.findChild<QListWidget*>("TSALanguageList");
        auto* restart = dialog.findChild<QPushButton*>("TSALanguageRestart");
        auto* note = dialog.findChild<QLabel*>("TSALanguageNote");
        TEST_CHECK(list && restart && note && list->count() == 3, "Test 459: trois langues listées");
        TEST_CHECK(dialog.selectedLanguage() == Language::French && restart->isHidden(), "Test 459: langue actuelle : pas de redémarrage proposé");
        for (int i = 0; i < list->count(); ++i)
            if (static_cast<Language>(list->item(i)->data(Qt::UserRole).toInt()) == Language::Malagasy) list->setCurrentRow(i);
        TEST_CHECK(dialog.selectedLanguage() == Language::Malagasy && !restart->isHidden(), "Test 459: malgache choisi : redémarrage proposé");
        const QString expected = LanguageManager::translate(Language::Malagasy, "TSA::UI::LanguageDialog", "Redémarrer maintenant");
        TEST_CHECK(restart->text() == expected && expected != QString::fromUtf8("Redémarrer maintenant"),
                   "Test 459: bouton dans la langue choisie (« " << restart->text().toStdString() << " »)");
        std::cout << "[PASS] Test 459: Choix de la langue expliqué dans la langue sélectionnée" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 460 : ressources des icônes présentes et dessinées
    // -------------------------------------------------------------------------
    QStringList catalogIcons;
    for (const auto& [id, d] : CommandCatalog::instance().allCommands())
        if (!d.iconPath.empty()) catalogIcons << QString::fromStdString(d.iconPath);
    catalogIcons.removeDuplicates();
    {
        QStringList broken;
        for (const QString& path : catalogIcons)
        {
            if (!QFile::exists(path)) { broken << path + QStringLiteral(" (absente)"); continue; }
            for (bool dark : { true, false })
            {
                const QImage img = TSA::UI::renderThemedIcon(path, 24, dark);
                int opaque = 0;
                for (int y = 0; y < img.height(); ++y)
                    for (int x = 0; x < img.width(); ++x) opaque += qAlpha(img.pixel(x, y)) > 128;
                if (opaque < 12) broken << path + (dark ? QStringLiteral(" (vide, sombre)") : QStringLiteral(" (vide, clair)"));
            }
        }
        for (const QString& b : broken) std::cerr << "  icône : " << b.toStdString() << std::endl;
        TEST_CHECK(broken.isEmpty() && catalogIcons.size() > 120, "Test 460: " << broken.size() << " icône(s) absente(s) ou vide(s) sur " << catalogIcons.size());
        std::cout << "[PASS] Test 460: " << catalogIcons.size() << " icônes du catalogue présentes et dessinées (thèmes sombre et clair)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 461 : contraste avec le fond du ruban (thèmes sombre et clair, état désactivé)
    // -------------------------------------------------------------------------
    {
        auto lum = [](QRgb c) {
            auto ch = [](int v) { const double s = v / 255.0; return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4); };
            return 0.2126 * ch(qRed(c)) + 0.7152 * ch(qGreen(c)) + 0.0722 * ch(qBlue(c));
        };
        auto ratio = [&](QRgb a, QRgb b) { double la = lum(a), lb = lum(b); if (la < lb) std::swap(la, lb); return (la + 0.05) / (lb + 0.05); };
        // Pixels lisibles : composés sur le fond, contraste au moins égal au seuil
        auto legible = [&](const QImage& img, QRgb bg, double threshold, int minAlpha) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                {
                    const QRgb p = img.pixel(x, y);
                    const int a = qAlpha(p);
                    if (a < minAlpha) continue;
                    // ARGB32 non prémultiplié : couleur × alpha + fond × (1 − alpha)
                    const QRgb onBg = qRgb((qRed(p) * a + qRed(bg) * (255 - a)) / 255, (qGreen(p) * a + qGreen(bg) * (255 - a)) / 255,
                                           (qBlue(p) * a + qBlue(bg) * (255 - a)) / 255);
                    n += ratio(onBg, bg) >= threshold;
                }
            return n;
        };
        const QRgb darkBg = qRgb(0x21, 0x28, 0x30), lightBg = qRgb(0xFF, 0xFF, 0xFF);
        QStringList weak;
        for (const QString& path : catalogIcons)
            for (bool dark : { true, false })
            {
                const QRgb bg = dark ? darkBg : lightBg;
                const QImage normal = TSA::UI::renderThemedIcon(path, 24, dark).convertToFormat(QImage::Format_ARGB32);
                const QImage disabled = TSA::UI::renderThemedIcon(path, 24, dark, QIcon::Disabled).convertToFormat(QImage::Format_ARGB32);
                if (legible(normal, bg, 3.0, 128) < 12) weak << path + (dark ? " (sombre)" : " (clair)");
                // État désactivé : opacité réduite de moitié, traits fins anticrénelés → pixels dès 64/255
                if (legible(disabled, bg, 1.8, 64) < 12) weak << path + (dark ? " (désactivée, sombre)" : " (désactivée, clair)");
            }
        for (int i = 0; i < std::min<int>(15, weak.size()); ++i) std::cerr << "  contraste insuffisant : " << weak[i].toStdString() << std::endl;
        TEST_CHECK(weak.isEmpty(), "Test 461: " << weak.size() << " rendu(s) au contraste insuffisant (forme sous 3:1, désactivée sous 1,8:1)");
        std::cout << "[PASS] Test 461: Icônes lisibles sur les deux thèmes (forme >= 3:1, état désactivé >= 1,8:1)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 462 : deux commandes différentes n'ont pas la même image
    // -------------------------------------------------------------------------
    {
        // Paires où une image identique serait voulue (même fonction à deux endroits) : aucune à ce jour
        const QSet<QString> allowed;
        QHash<QByteArray, QStringList> byContent;
        for (const auto& [id, d] : CommandCatalog::instance().allCommands())
        {
            if (d.iconPath.empty()) continue;
            QFile f(QString::fromStdString(d.iconPath));
            if (f.open(QIODevice::ReadOnly)) byContent[f.readAll()] << QString::fromStdString(id);
        }
        QStringList clashes;
        for (auto it = byContent.begin(); it != byContent.end(); ++it)
        {
            QStringList ids = it.value();
            ids.sort();
            for (int i = 0; i < ids.size(); ++i)
                for (int j = i + 1; j < ids.size(); ++j)
                    if (!allowed.contains(ids[i] + "|" + ids[j])) clashes << ids[i] + " = " + ids[j];
        }
        for (const QString& c : clashes) std::cerr << "  même image : " << c.toStdString() << std::endl;
        TEST_CHECK(clashes.isEmpty(), "Test 462: " << clashes.size() << " paire(s) de commandes différentes avec la même image");
        std::cout << "[PASS] Test 462: Chaque commande du catalogue a une image qui lui est propre" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 463 : moteur d'icônes thématique
    // -------------------------------------------------------------------------
    {
        const QString yellow = ":/icons/dimensions/dim_linear.svg";   // traits #FFD54F, illisibles sur fond blanc sans adaptation
        TEST_CHECK(!TSA::UI::iconPalette(false).isEmpty() && TSA::UI::iconPalette(false).contains("#FFD54F"),
                   "Test 463: palette du thème clair chargée (:/icons/palette.json)");
        TEST_CHECK(TSA::UI::themedSvg(yellow, false).contains(TSA::UI::iconPalette(false).value("#FFD54F").toUtf8())
                       && TSA::UI::themedSvg(yellow, true).contains("#FFD54F"),
                   "Test 463: couleur adaptée au thème clair, inchangée en thème sombre");
        const QIcon icon = TSA::UI::themedIcon(yellow);
        TEST_CHECK(!icon.isNull() && !icon.pixmap(24, 24).isNull(), "Test 463: QIcon thématique utilisable");
        const QImage normal = TSA::UI::renderThemedIcon(yellow, 24, true), disabled = TSA::UI::renderThemedIcon(yellow, 24, true, QIcon::Disabled);
        TEST_CHECK(normal != disabled, "Test 463: état désactivé rendu différemment (gris atténué)");
        TEST_CHECK(TSA::UI::themedIcon(":/icons/inexistante.svg").isNull(), "Test 463: ressource absente → icône nulle (détectable)");
        std::cout << "[PASS] Test 463: Icônes adaptées au thème et à l'état désactivé" << std::endl;
        ++passed;
    }

    return true;
}

namespace
{
/// Ruban complet : une action distincte par champ de RibbonActions (aucun nul), trois par liste, trois menus.
struct FullRibbonActions
{
    QObject owner;
    TSA::UI::RibbonActions acts;
    QMenu meshMenu, annotationMenu, loadMenu, meshSubMenu;
    QAction* subMenuAction = nullptr;   ///< commande d'un sous-menu (« Grandeur affichée sur le maillage »)

    FullRibbonActions()
    {
        auto make = [this](const QString& name) { auto* a = new QAction(name, &owner); a->setObjectName(name); return a; };
#define F(x) acts.x = make(QStringLiteral(#x))
    F(actionNew); F(actionCloseProject); F(actionOpen); F(actionSave);
    F(actionSaveAs); F(actionExit); F(actionUndo); F(actionRedo);
    F(actionCopyClipboard); F(actionPasteClipboard); F(actionCutClipboard); F(actionPasteOriginal);
    F(actionImportIfc); F(actionExportIfc); F(actionImportTsa3d); F(actionExportTsa3d);
    F(actionImportViaModule); F(actionBimGeometryVisible); F(actionTopology); F(actionStructurePresets);
    F(actionLanguage); F(actionToggleTheme); F(actionShortcutEditor); F(actionSelectMode);
    F(actionSelectAll); F(actionDrawNode); F(actionNewNode); F(actionDrawBar);
    F(actionDrawBeam); F(actionDrawColumn); F(actionDrawCable); F(actionDrawSlab);
    F(actionDrawWall); F(actionTruss); F(actionFooting); F(actionSlabOpenings);
    F(actionAddCube); F(actionSecI); F(actionSecRect); F(actionSecCirc);
    F(actionConcrete); F(actionSteel); F(actionLibrary); F(actionFixed);
    F(actionPinned); F(actionRoller); F(actionSurfaceSupports); F(actionMove3D);
    F(actionMove); F(actionCopy3D); F(actionCopy); F(actionRotate3D);
    F(actionMirror); F(actionSplitBars); F(actionMergeNodes); F(actionCleanModel);
    F(actionToolInputMode); F(actionDelete); F(actionNewGrid); F(actionGridManager);
    F(actionManageLevels); F(actionWorkPlaneXY); F(actionWorkPlaneXZ); F(actionWorkPlaneYZ);
    F(actionWorkPlaneLevel); F(actionWorkPlaneCustom); F(actionWorkPlaneVisible); F(actionViewNormalToPlane);
    F(actionWorkPlaneNextRef); F(actionWorkPlanePrevRef); F(actionWorkPlaneBack); F(actionWorkPlaneNextInter);
    F(actionWorkPlanePrevInter); F(actionWorkPlaneAlignInter); F(actionGridSnap); F(actionObjectSnap);
    F(actionGridVisible); F(actionLevelsVisible); F(actionGridLabels); F(actionRulersVisible);
    F(actionCoordSystem); F(actionMoveOrigin); F(actionSectionCut); F(actionPointLoad);
    F(actionDistLoad); F(actionTrapLoad); F(actionBarPointLoad); F(actionSurfaceLoad);
    F(actionSelfWeight); F(actionLoadCases); F(actionLoadsVisible); F(actionForcesVisible);
    F(actionMomentsVisible); F(actionLoadValuesVisible); F(actionSurfaceLoadsVisible); F(actionMeshGen);
    F(actionMeshRegenerate); F(actionMeshClear); F(actionMeshSettings); F(actionMeshLocal);
    F(actionMeshZone); F(actionMeshPreview); F(actionMeshQuality); F(actionMeshShow);
    F(actionMeshExport); F(actionMeshImport); F(actionRunSolve); F(actionAnalysisConfig);
    F(actionAICheck); F(actionAIAnalyze); F(actionResultsDock); F(actionDeformedToggle);
    F(actionResultsDisp); F(actionResultsForces); F(actionResultsStress); F(actionDiagramMz);
    F(actionDiagramMy); F(actionDiagramMx); F(actionDiagramVz); F(actionDiagramVy);
    F(actionDiagramN); F(actionDiagramDeflection); F(actionDiagramNone); F(actionReactionsToggle);
    F(actionFitModel); F(actionFitResults); F(actionFitDeformed); F(actionOpenNDC);
    F(actionReportTemplates); F(actionAIExplain); F(actionView3D); F(actionViewXY);
    F(actionViewXZ); F(actionViewYZ); F(actionViewTop); F(actionViewBottom);
    F(actionViewFront); F(actionViewBack); F(actionViewLeft); F(actionViewRight);
    F(actionViewIsometric); F(actionViewHome); F(actionNamedViews); F(actionFitAll);
    F(actionFitSelection); F(actionResetView); F(actionZoomIn); F(actionZoomOut);
    F(actionZoomWindow); F(actionPreviousView); F(actionNextView); F(actionRotateLeft);
    F(actionRotateRight); F(renderEdges); F(renderLightweight); F(actionNodesVisible);
    F(actionNodeLabelsVisible); F(actionSupportsVisible); F(actionSupportLabelsVisible); F(actionFullScreen);
    F(actionToggleModelTree); F(actionToggleProperties); F(actionToggleVisibility); F(actionToggleConsole);
    F(actionMeasure); F(actionAIAssistant); F(actionAIConfig); F(actionExtensionManager);
    F(actionModules); F(actionHelp); F(actionOnlineDocs); F(actionShortcuts);
    F(actionAbout); F(actionReportProblem); F(actionExportDiagnostic);
#undef F
        for (auto* list : { &acts.drawTools, &acts.advancedModifyTools, &acts.dimensionCommands })
            for (int i = 0; i < 3; ++i) list->push_back(make(QStringLiteral("liste%1_%2").arg(reinterpret_cast<quintptr>(list)).arg(i)));
        for (int i = 0; i < 4; ++i) acts.modelDisplayModes.push_back(make(QStringLiteral("display%1").arg(i)));
        for (int i = 0; i < 3; ++i) acts.renderModes.push_back(make(QStringLiteral("render%1").arg(i)));
        for (const char* id : { "dim_aligned", "dim_linear", "dim_x", "dim_y", "dim_z", "dim_horizontal", "dim_angular", "dim_level",
                                "dim_chain", "dim_cumulative", "dim_futur" })
            acts.dimensionTools[id] = make(QString::fromLatin1(id));
        for (QMenu* m : { &meshMenu, &annotationMenu, &loadMenu })
            for (int i = 0; i < 2; ++i) m->addAction(make(QStringLiteral("menu%1_%2").arg(reinterpret_cast<quintptr>(m)).arg(i)));
        subMenuAction = make(QStringLiteral("submenu_action"));
        meshSubMenu.addAction(subMenuAction);
        meshMenu.addMenu(&meshSubMenu);
        acts.meshViewMenu = &meshMenu;
        acts.annotationScaleMenu = &annotationMenu;
        acts.loadScaleMenu = &loadMenu;
    }
};
} // namespace

bool runSuite_Ribbon(int& passed)
{
    using TSA::UI::RibbonBar;
    using TSA::UI::RibbonBuilder;
    const bool offscreen = QGuiApplication::platformName() == QLatin1String("offscreen");

    // -------------------------------------------------------------------------
    // TEST 464 : organisation : dix onglets, chaque commande à une place, aucune commande fictive
    // -------------------------------------------------------------------------
    {
        FullRibbonActions full;
        RibbonBar bar;
        RibbonBuilder::buildAllTabs(&bar, full.acts, nullptr);
        QStringList titles;
        for (int i = 0; i < bar.tabCount(); ++i) titles << bar.tabTitle(i);
        TEST_CHECK(titles == QStringList({ QString::fromUtf8("Accueil"), QString::fromUtf8("Modélisation"), "Modifier", "Grille et plans", "Charges",
                                           "Maillage", "Analyse", QString::fromUtf8("Résultats"), "Affichage", "Outils" }),
                   "Test 464: dix onglets dans l'ordre du travail (" << titles.join(", ").toStdString() << ")");
        const auto placed = RibbonBuilder::placedActions(full.acts);
        const auto locations = bar.actionLocations();
        QStringList missing, extra, duplicated;
        QHash<const QAction*, int> count;
        for (int i = 0; i < bar.tabCount(); ++i)
            for (auto* panel : bar.tab(i)->panels())
            {
                TEST_CHECK(!panel->title().isEmpty() && !panel->isEmptyPanel(), "Test 464: groupe sans titre ou vide dans « " << bar.tabTitle(i).toStdString() << " »");
                for (QAction* a : panel->actions()) ++count[a];
            }
        std::set<const QAction*> placedSet(placed.begin(), placed.end());
        // Commandes des menus déroulants (affichage du maillage, échelles) : vraies actions, suivies par leur groupe
        for (QMenu* m : { &full.meshMenu, &full.annotationMenu, &full.loadMenu })
            for (QAction* a : m->actions()) placedSet.insert(a);
        for (QAction* a : placed)
            if (!locations.contains(a)) missing << a->objectName();
        for (auto it = count.begin(); it != count.end(); ++it)
        {
            if (!placedSet.count(it.key())) extra << it.key()->text();
            if (it.value() > 1) duplicated << it.key()->objectName();
        }
        TEST_CHECK(missing.isEmpty(), "Test 464: commandes absentes du ruban : " << missing.join(", ").toStdString());
        TEST_CHECK(extra.isEmpty(), "Test 464: boutons sans action réelle de la fenêtre : " << extra.join(", ").toStdString());
        TEST_CHECK(duplicated.isEmpty(), "Test 464: commandes placées deux fois : " << duplicated.join(", ").toStdString());
        TEST_CHECK(placed.size() > 200 && !locations.contains(full.acts.actionExit), "Test 464: " << placed.size() << " commandes placées (« Quitter » au menu de l'application)");
        std::cout << "[PASS] Test 464: Ruban en dix onglets : " << placed.size() << " commandes, chacune à une place, aucune fictive" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 465 : un ruban incomplet (fenêtre sans certaines commandes) n'affiche ni groupe vide ni bouton de remplacement
    // -------------------------------------------------------------------------
    {
        QObject owner;
        TSA::UI::RibbonActions acts;
        acts.actionNew = new QAction("Nouveau", &owner);
        acts.actionDrawBeam = new QAction("Poutre", &owner);
        acts.actionRunSolve = new QAction("Calcul", &owner);
        RibbonBar bar;
        RibbonBuilder::buildAllTabs(&bar, acts, nullptr);
        int panels = 0, buttons = 0;
        for (int i = 0; i < bar.tabCount(); ++i)
            for (auto* panel : bar.tab(i)->panels())
            {
                ++panels;
                buttons += static_cast<int>(panel->actions().size());
            }
        TEST_CHECK(panels == 3 && buttons == 3, "Test 465: " << panels << " groupe(s), " << buttons << " bouton(s) pour 3 commandes disponibles");
        std::cout << "[PASS] Test 465: Aucun groupe vide ni bouton de remplacement quand une commande manque" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 466 : fenêtre réduite : chaque commande reste atteignable (groupes réduits, repliés, menu « Plus »)
    // -------------------------------------------------------------------------
    if (offscreen)
        std::cout << "[SKIP] Test 466: mesures du ruban sans polices sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
    else
    {
        FullRibbonActions full;
        RibbonBar bar;
        RibbonBuilder::buildAllTabs(&bar, full.acts, nullptr);
        bar.show();
        QStringList lost;
        int overflowSeen = 0, collapsedSeen = 0;
        for (int width : { 1920, 1366, 1024, 800, 600, 420 })
        {
            bar.resize(width, bar.sizeHint().height());
            for (int i = 0; i < bar.tabCount(); ++i)
            {
                bar.setCurrentTabIndex(i);
                QCoreApplication::processEvents();
                auto* tab = bar.tab(i);
                const auto reach = tab->reachableActions();
                std::set<QAction*> reachable(reach.begin(), reach.end());
                for (auto* panel : tab->panels())
                    for (QAction* a : panel->actions())
                        if (!reachable.count(a)) lost << QStringLiteral("%1 px, %2 : %3").arg(width).arg(bar.tabTitle(i), a->objectName());
                // Sous-menu d'un menu déroulant (onglet Maillage) : atteignable aussi quand le groupe est replié
                if (bar.tabTitle(i) == QStringLiteral("Maillage") && !reachable.count(full.subMenuAction))
                    lost << QStringLiteral("%1 px, Maillage : commande du sous-menu").arg(width);
                overflowSeen += tab->hasOverflowMenu();
                for (auto* panel : tab->panels()) collapsedSeen += panel->isVisibleTo(tab) && panel->mode() == TSA::UI::RibbonPanelMode::Collapsed;
            }
        }
        for (int i = 0; i < std::min<int>(10, lost.size()); ++i) std::cerr << "  perdue : " << lost[i].toStdString() << std::endl;
        TEST_CHECK(lost.isEmpty(), "Test 466: " << lost.size() << " commande(s) inaccessible(s) après réduction de la fenêtre");
        TEST_CHECK(collapsedSeen > 0 && overflowSeen > 0, "Test 466: groupes repliés (" << collapsedSeen << ") et menu « Plus » (" << overflowSeen << ") aux petites largeurs");
        std::cout << "[PASS] Test 466: Toutes les commandes restent atteignables de 1920 à 420 px (groupes repliés : " << collapsedSeen << ", menu « Plus » : " << overflowSeen << ")" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 467 : ruban dans les trois langues : titres traduits, rangée d'onglets sans débordement à 1366 px
    // -------------------------------------------------------------------------
    {
        auto& app = *QCoreApplication::instance();
        FrenchOnExit restoreFrench;
        // Textes source (français) des onglets et des groupes
        QStringList sources;
        {
            FullRibbonActions full;
            RibbonBar bar;
            RibbonBuilder::buildAllTabs(&bar, full.acts, nullptr);
            for (int i = 0; i < bar.tabCount(); ++i)
            {
                sources << bar.tabTitle(i);
                for (auto* panel : bar.tab(i)->panels()) sources << panel->title();
            }
        }
        sources.removeDuplicates();
        QStringList untranslated;
        int widest = 0;
        QString widestLang;
        for (Language lang : { Language::French, Language::English, Language::Malagasy })
        {
            if (lang != Language::French)
                for (const QString& s : sources)
                    if (!LanguageManager::hasTranslationFor(lang, "TSA::UI::RibbonBuilder", s.toUtf8().constData()))
                        untranslated << LanguageManager::code(lang) + QStringLiteral(" : ") + s;
            if (offscreen) continue;
            TEST_CHECK(LanguageManager::install(app, lang), "Test 467: langue installée");
            FullRibbonActions full;
            RibbonBar bar;
            RibbonBuilder::buildAllTabs(&bar, full.acts, nullptr);
            const int w = bar.tabBar()->sizeHint().width();
            if (w > widest) { widest = w; widestLang = LanguageManager::nativeName(lang); }
        }
        TEST_CHECK(LanguageManager::install(app, Language::French), "Test 467: retour au français");
        for (int i = 0; i < std::min<int>(10, untranslated.size()); ++i) std::cerr << "  non traduit : " << untranslated[i].toStdString() << std::endl;
        TEST_CHECK(untranslated.isEmpty(), "Test 467: " << untranslated.size() << " titre(s) d'onglet ou de groupe non traduit(s) sur " << sources.size());
        if (!offscreen)
            TEST_CHECK(widest <= 1100, "Test 467: rangée d'onglets la plus large : " << widest << " px (" << widestLang.toStdString() << ") pour 1366 px de fenêtre");
        std::cout << "[PASS] Test 467: Ruban traduit (" << sources.size() << " titres) ; rangée d'onglets la plus large " << widest << " px ("
                  << widestLang.toStdString() << ")" << std::endl;
        ++passed;
    }
    return true;
}
