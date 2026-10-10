// Suite « shortcuts » : raccourcis clavier (tests 257-266). Catalogue (valeurs par défaut sans conflit),
// fichier shortcut.txt (lecture, écriture, erreurs), conflits et portées, ShortcutManager (création du
// fichier, dernière configuration valide, rechargement à chaud réel par QFileSystemWatcher), frappes
// clavier simulées (nouveau / ancien raccourci, suites de touches, champs de saisie), éditeur, répétition.

#include "test_common.h"

#include "Commands/CommandCatalog.h"
#include "UI/Dialogs/ShortcutEditorDialog.h"
#include "UI/Shortcuts/ShortcutConfig.h"
#include "UI/Shortcuts/ShortcutManager.h"

#include <QAction>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifdef TSA_HAVE_QTTEST
#include <QTest>
#endif

using namespace TSA::UI::Shortcuts;

namespace
{
QList<QKeySequence> seqs(const QString& text)
{
    QList<QKeySequence> out;
    parseSequenceList(text, &out);
    return out;
}

bool writeText(const QString& path, const QString& text)
{
    QSaveFile f(path); // remplacement atomique, comme la plupart des éditeurs
    return f.open(QIODevice::WriteOnly | QIODevice::Text) && f.write(text.toUtf8()) >= 0 && f.commit();
}

/// Attend (boucle d'événements) que cond() devienne vrai ; false après timeoutMs.
template <typename F>
bool waitFor(F cond, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (!cond())
    {
        if (t.elapsed() > timeoutMs) return false;
        QEventLoop loop;
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return true;
}

CommandDefinition def(const QString& id, const QString& defaults, const QString& scope = QStringLiteral("window"))
{
    CommandDefinition d;
    d.id = id;
    d.name = id;
    d.category = QStringLiteral("Test");
    d.defaults = seqs(defaults);
    d.scope = scope;
    return d;
}
} // namespace

bool runSuite_Shortcuts(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 257 : catalogue — identifiants stables, valeurs par défaut valides, aucun conflit
    // -------------------------------------------------------------------------
    {
        const auto defs = ShortcutManager::catalogDefinitions();
        TEST_CHECK(defs.size() >= 200, "Test 257: catalogue complet (" << defs.size() << " commandes)");
        static const QRegularExpression idRe(QStringLiteral("^cmd(\\.[a-z0-9_]+){2,3}$"));
        bool idsOk = true, defaultsOk = true;
        for (const auto& d : defs)
        {
            idsOk &= idRe.match(d.id).hasMatch() && !d.name.isEmpty() && !d.category.isEmpty();
            const std::string raw = TSA::Commands::CommandCatalog::instance().findCommand(d.id.toStdString())->shortcut;
            QList<QKeySequence> parsed;
            defaultsOk &= parseSequenceList(QString::fromStdString(raw), &parsed) && parsed == d.defaults;
        }
        TEST_CHECK(idsOk, "Test 257: identifiants cmd.<domaine>.<nom>, libellés et catégories renseignés");
        TEST_CHECK(defaultsOk, "Test 257: tous les raccourcis par défaut du catalogue sont des combinaisons valides");
        const auto conflicts = findConflicts(defs, {});
        TEST_CHECK(conflicts.isEmpty(), "Test 257: aucun conflit par défaut (" << (conflicts.isEmpty() ? "" : conflicts.first().message().toStdString()) << ")");
        auto defaultsOf = [&](const char* id) {
            for (const auto& d : defs)
                if (d.id == QLatin1String(id)) return d.defaults;
            return QList<QKeySequence>();
        };
        // Raccourcis historiques conservés (aucun remplacement silencieux).
        TEST_CHECK(defaultsOf("cmd.file.new") == seqs("Ctrl+N") && defaultsOf("cmd.file.save") == seqs("Ctrl+S") &&
                       defaultsOf("cmd.edit.redo") == seqs("Ctrl+Y ; Ctrl+Shift+Z") && defaultsOf("cmd.display.grid") == seqs("G ; F7") &&
                       defaultsOf("cmd.analysis.solve") == seqs("F5") && defaultsOf("cmd.view.top") == seqs("Num+7") &&
                       defaultsOf("cmd.create.beam") == seqs("B") && defaultsOf("cmd.isolate.show_all") == seqs("Alt+H") &&
                       defaultsOf("cmd.coord.axis_z") == seqs("Alt+Z") && defaultsOf("cmd.window.console") == seqs("F2") &&
                       defaultsOf("cmd.view.zoom_in") == QList<QKeySequence>{ QKeySequence(Qt::Key_Plus) } &&
                       defaultsOf("cmd.view.zoom_out") == QList<QKeySequence>{ QKeySequence(Qt::Key_Minus) },
                   "Test 257: raccourcis existants conservés");
        int withShortcut = 0;
        for (const auto& d : defs) withShortcut += d.defaults.isEmpty() ? 0 : 1;
        std::cout << "  " << defs.size() << " commandes, " << withShortcut << " avec raccourci par défaut" << std::endl;
        std::cout << "[PASS] Test 257: Catalogue des commandes et raccourcis par défaut" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 258 : lecture de shortcut.txt — syntaxe, alias, accords, noms français, erreurs
    // -------------------------------------------------------------------------
    {
        const QSet<QString> known = { "cmd.a", "cmd.b", "cmd.c", "cmd.d", "cmd.e" };
        const QString good = QString(QChar(0xFEFF)) + QStringLiteral(
            "# format: tsa-shortcuts 1\r\n[Section]\r\n"
            "cmd.a | Ctrl+Maj+S ; F7 | on | x | desc\r\n"
            "cmd.b | D, A | off\r\n"
            "cmd.c | aucun |\r\n"
            "cmd.d | Suppr\r\n"
            "cmd.inconnue | Ctrl+K\r\n");
        auto r = parseConfig(good, known);
        TEST_CHECK(r.ok && r.errors.isEmpty(), "Test 258: fichier valide (" << r.errors.join(";").toStdString() << ")");
        TEST_CHECK(r.settings["cmd.a"].sequences == seqs("Ctrl+Shift+S ; F7") && r.settings["cmd.a"].enabled, "Test 258: alias et « Maj »");
        TEST_CHECK(r.settings["cmd.b"].sequences == QList<QKeySequence>{ QKeySequence(QStringLiteral("D, A")) } && !r.settings["cmd.b"].enabled,
                   "Test 258: suite de touches « D, A », désactivée");
        TEST_CHECK(r.settings["cmd.c"].sequences.isEmpty() && r.settings["cmd.d"].sequences == seqs("Del"), "Test 258: « aucun » et « Suppr »");
        TEST_CHECK(r.warnings.size() == 1 && r.warnings.first().contains("cmd.inconnue") && !r.settings.contains("cmd.inconnue"),
                   "Test 258: commande inconnue ignorée avec avertissement");
        const char* bad[] = { "cmd.a Ctrl+S", "cmd.a | Ctrl+Truc", "cmd.a | Ctrl", "cmd.a | F5 | peut-être", "cmd.a | Ctrl+|",
                              "[Section", "cmd.a | F5\ncmd.a | F6" };
        bool allRejected = true;
        for (const char* b : bad)
        {
            const auto rb = parseConfig(QString::fromUtf8(b), known);
            allRejected &= !rb.ok && !rb.errors.isEmpty() && rb.errors.first().contains(QStringLiteral("ligne"));
        }
        TEST_CHECK(allRejected, "Test 258: syntaxe, combinaison inconnue, modificateur seul, état, « | », section, doublon : refusés avec numéro de ligne");
        TEST_CHECK(!parseConfig("# format: tsa-shortcuts 9\n", known).warnings.isEmpty(), "Test 258: format plus récent signalé");
        TEST_CHECK(parseConfig("", known).ok, "Test 258: fichier vide : valeurs par défaut");
        std::cout << "[PASS] Test 258: Lecture de shortcut.txt" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 259 : écriture — fichier complet, commenté, relu à l'identique
    // -------------------------------------------------------------------------
    {
        const auto defs = ShortcutManager::catalogDefinitions();
        ShortcutSettings settings;
        settings["cmd.file.save"] = { seqs("F12"), true };
        settings["cmd.view.reset"] = { seqs("R"), false };
        const QString text = formatConfig(defs, settings);
        QSet<QString> ids;
        for (const auto& d : defs) ids.insert(d.id);
        const auto r = parseConfig(text, ids);
        bool same = r.ok && r.settings.size() == defs.size();
        for (const auto& d : defs) same &= effectiveSequences(d, r.settings) == effectiveSequences(d, settings);
        TEST_CHECK(same, "Test 259: aller-retour écriture / lecture identique (" << r.errors.join(";").toStdString() << ")");
        bool sections = true;
        for (const char* c : { "[Gestion des projets]", "[Édition]", "[Modélisation]", "[Sélection et isolation]", "[Navigation 2D et 3D]",
                               "[Grille, accrochage et plans de travail]", "[Matériaux et sections]", "[Maillage]", "[Charges]",
                               "[Analyse structurelle]", "[Résultats]", "[Rapports]", "[BIM et échanges de données]",
                               "[Fenêtres et panneaux]", "[Outils généraux]" })
            sections &= text.contains(QString::fromUtf8(c));
        TEST_CHECK(sections && text.contains("# format: tsa-shortcuts 1") && text.contains("cmd.view.reset") &&
                       text.contains(QRegularExpression("cmd\\.view\\.reset\\s+\\|\\s+R\\s+\\|\\s+off")),
                   "Test 259: catégories demandées, en-tête, commande désactivée conservée");
        std::cout << "[PASS] Test 259: Écriture de shortcut.txt" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 260 : conflits — exact, préfixe, portées distinctes
    // -------------------------------------------------------------------------
    {
        QList<CommandDefinition> defs = { def("cmd.x", "Ctrl+K"), def("cmd.y", "Ctrl+K"), def("cmd.z", "D, A"), def("cmd.w", "D") };
        const auto c = findConflicts(defs, {});
        TEST_CHECK(c.size() == 2 && !c[0].prefix && c[1].prefix && c[1].idA == "cmd.w", "Test 260: conflit exact et conflit de préfixe « D » / « D, A »");
        ShortcutSettings fix;
        fix["cmd.y"] = { seqs("Ctrl+K"), false };   // désactivé : plus de conflit
        fix["cmd.w"] = { seqs("D, W"), true };
        TEST_CHECK(findConflicts(defs, fix).isEmpty(), "Test 260: désactivation et remplacement résolvent les conflits");
        QList<CommandDefinition> scoped = { def("cmd.main", "Ctrl+N"), def("cmd.start", "Ctrl+N", "startcenter"), def("cmd.view", "Ctrl+N", "viewport") };
        const auto cs = findConflicts(scoped, {});
        TEST_CHECK(cs.size() == 1 && cs[0].idA == "cmd.main" && cs[0].idB == "cmd.view",
                   "Test 260: même combinaison dans des portées disjointes acceptée ; fenêtre / vue 3D en conflit");
        std::cout << "[PASS] Test 260: Détection des conflits" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 261 : gestionnaire — création du fichier, application, dernière configuration valide
    // -------------------------------------------------------------------------
    {
        QTemporaryDir dir;
        ShortcutManager mgr;
        mgr.setReloadDelay(50);
        QAction save(QStringLiteral("Enregistrer"));
        save.setToolTip(QStringLiteral("Enregistrer le projet (Ctrl+S)"));
        QAction grid(QStringLiteral("Grille"));
        QAction fit(QStringLiteral("Zoom étendu"));
        TEST_CHECK(mgr.bind("cmd.file.save", &save) && mgr.bind("cmd.display.grid", &grid) && mgr.bind("cmd.view.fit_all", &fit),
                   "Test 261: actions reliées au catalogue");
        QAction other(QStringLiteral("Hors catalogue"));
        TEST_CHECK(!mgr.bind("cmd.inexistante", &other) && other.shortcuts().isEmpty(), "Test 261: identifiant hors catalogue signalé, sans raccourci");
        const QString path = dir.filePath("sub/shortcut.txt");
        mgr.setConfigPath(path);
        TEST_CHECK(mgr.loadConfig() && QFile::exists(path), "Test 261: shortcut.txt créé (dossier compris) s'il manque");
        TEST_CHECK(save.shortcut() == QKeySequence("Ctrl+S") && grid.shortcuts() == seqs("G ; F7") && fit.shortcuts() == seqs("F ; Z, E"),
                   "Test 261: valeurs par défaut appliquées (alias, suite de touches)");
        TEST_CHECK(save.toolTip() == QStringLiteral("Enregistrer le projet (Ctrl+S)"), "Test 261: infobulle sans doublon « (Ctrl+S) (Ctrl+S) »");

        // Modification du fichier puis relecture.
        QString text = mgr.defaultConfigText();
        text.replace(QRegularExpression("(cmd\\.file\\.save\\s+\\|)\\s*Ctrl\\+S\\s*\\|"), "\\1 Ctrl+Alt+S |");
        TEST_CHECK(writeText(path, text) && mgr.reload(), "Test 261: fichier modifié relu");
        TEST_CHECK(save.shortcuts() == seqs("Ctrl+Alt+S") && save.toolTip() == QStringLiteral("Enregistrer le projet (Ctrl+Alt+S)"),
                   "Test 261: nouveau raccourci et infobulle mis à jour, ancien retiré");

        // Configuration invalide : refusée, la précédente reste.
        QStringList rejected;
        QObject::connect(&mgr, &ShortcutManager::configRejected, [&](const QStringList& e) { rejected = e; });
        TEST_CHECK(writeText(path, text + "cmd.view.fit_all | Ctrl+Truc\n") && !mgr.reload() && !rejected.isEmpty() &&
                       save.shortcuts() == seqs("Ctrl+Alt+S"),
                   "Test 261: syntaxe invalide refusée, dernière configuration valide conservée");
        QString conflict = text;
        conflict.replace(QRegularExpression("(cmd\\.view\\.fit_all\\s+\\|)\\s*F ; Z, E\\s*\\|"), "\\1 G |");
        TEST_CHECK(writeText(path, conflict) && !mgr.reload() && mgr.lastErrors().join(" ").contains("conflit") && fit.shortcuts() == seqs("F ; Z, E"),
                   "Test 261: conflit refusé (aucune résolution arbitraire)");
        // Désactivation d'un raccourci.
        QString off = text;
        off.replace(QRegularExpression("(cmd\\.display\\.grid\\s+\\|\\s*G ; F7\\s*\\|)\\s*on"), "\\1 off");
        TEST_CHECK(writeText(path, off) && mgr.reload() && grid.shortcuts().isEmpty() && !mgr.isEnabled("cmd.display.grid"),
                   "Test 261: raccourci désactivé");
        // Plusieurs relectures : un seul déclenchement par action.
        int fired = 0;
        QObject::connect(&save, &QAction::triggered, [&] { ++fired; });
        for (int i = 0; i < 5; ++i) mgr.reload();
        save.trigger();
        TEST_CHECK(fired == 1, "Test 261: relectures successives sans connexion en double");
        // Action détruite : aucun plantage à la relecture.
        {
            auto* temp = new QAction(QStringLiteral("temporaire"));
            mgr.bind("cmd.view.reset", temp);
            delete temp;
        }
        TEST_CHECK(mgr.reload(), "Test 261: commande dont l'action a disparu : aucun plantage");
        std::cout << "[PASS] Test 261: ShortcutManager (fichier, application, refus)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 262 : rechargement à chaud réel (QFileSystemWatcher), fichier remplacé deux fois
    // -------------------------------------------------------------------------
    {
        QTemporaryDir dir;
        ShortcutManager mgr;
        mgr.setReloadDelay(50);
        QAction solve(QStringLiteral("Calcul"));
        mgr.bind("cmd.analysis.solve", &solve);
        const QString path = dir.filePath("shortcut.txt");
        mgr.setConfigPath(path);
        mgr.loadConfig();
        int applied = 0;
        QObject::connect(&mgr, &ShortcutManager::configApplied, [&] { ++applied; });
        QString text = mgr.defaultConfigText();
        QString v1 = text;
        v1.replace(QRegularExpression("(cmd\\.analysis\\.solve\\s+\\|)\\s*F5\\s*\\|"), "\\1 Ctrl+F12 |");
        writeText(path, v1);
        const bool first = waitFor([&] { return solve.shortcut() == QKeySequence("Ctrl+F12"); }, 4000);
        TEST_CHECK(first, "Test 262: modification détectée et appliquée sans redémarrage");
        QString v2 = text;
        v2.replace(QRegularExpression("(cmd\\.analysis\\.solve\\s+\\|)\\s*F5\\s*\\|"), "\\1 Shift+F12 ; F5 |");
        writeText(path, v2);
        const bool second = waitFor([&] { return solve.shortcuts() == seqs("Shift+F12 ; F5"); }, 4000);
        TEST_CHECK(second, "Test 262: fichier remplacé une deuxième fois : toujours surveillé");
        const int before = applied;
        writeText(path, v2); // contenu identique
        waitFor([] { return false; }, 400);
        TEST_CHECK(applied == before, "Test 262: contenu inchangé : pas de réapplication");
        writeText(path, v2 + "garbage sans séparateur\n");
        waitFor([] { return false; }, 400);
        TEST_CHECK(solve.shortcuts() == seqs("Shift+F12 ; F5"), "Test 262: enregistrement invalide : configuration conservée");
        // Écriture par TSA (éditeur) : appliquée une fois, la notification du système de fichiers qui suit
        // est reconnue (fins de ligne CRLF comprises) et ne provoque pas de relecture en écho.
        writeText(path, v2);
        waitFor([&] { return mgr.lastErrors().isEmpty(); }, 1000);
        int echo = 0;
        QObject::connect(&mgr, &ShortcutManager::configApplied, [&] { ++echo; });
        ShortcutSettings own = mgr.settings();
        own["cmd.analysis.solve"] = { seqs("Ctrl+Shift+F11"), true };
        const bool saved = mgr.saveSettings(own);
        waitFor([] { return false; }, 600);
        TEST_CHECK(saved && echo == 1 && solve.shortcut() == QKeySequence("Ctrl+Shift+F11"),
                   "Test 262: écriture par TSA appliquée une seule fois, sans relecture en écho (" << echo << ")");
        std::cout << "[PASS] Test 262: Rechargement à chaud de shortcut.txt" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 263 : frappes clavier simulées — nouveau / ancien raccourci, accord, champs de saisie
    // -------------------------------------------------------------------------
    {
#ifdef TSA_HAVE_QTTEST
        QTemporaryDir dir;
        ShortcutManager mgr;
        QWidget window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        auto* lay = new QVBoxLayout(&window);
        auto* edit = new QLineEdit(&window);
        auto* focusSink = new QWidget(&window);
        focusSink->setFocusPolicy(Qt::StrongFocus);
        lay->addWidget(edit);
        lay->addWidget(focusSink);
        QAction node(QStringLiteral("Nœud")), dim(QStringLiteral("Cotation")), copy(QStringLiteral("Copier"));
        for (QAction* a : { &node, &dim, &copy }) window.addAction(a);
        int nNode = 0, nDim = 0, nCopy = 0;
        QObject::connect(&node, &QAction::triggered, [&] { ++nNode; });
        QObject::connect(&dim, &QAction::triggered, [&] { ++nDim; });
        QObject::connect(&copy, &QAction::triggered, [&] { ++nCopy; });
        mgr.bind("cmd.create.node", &node);
        mgr.bind("cmd.tool.dim_aligned", &dim);
        mgr.bind("cmd.edit.copy", &copy);
        mgr.setConfigPath(dir.filePath("shortcut.txt"));
        mgr.loadConfig();
        auto guard = std::make_unique<ShortcutInputGuard>();
        qApp->installEventFilter(guard.get());
        window.show();
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QApplication::setActiveWindow(&window);
        QT_WARNING_POP
        // Fenêtre jamais affichée à l'écran : le système ne l'active pas ; le moteur de raccourcis de Qt
        // ne consulte que la fenêtre active de l'application, que setActiveWindow fixe.
        waitFor([&] { return QApplication::activeWindow() == &window; }, 2000);
        TEST_CHECK(QApplication::activeWindow() == &window, "Test 263: fenêtre de test active pour l'application (frappes simulées)");
        focusSink->setFocus();
        QTest::keyClick(focusSink, Qt::Key_N);
        TEST_CHECK(nNode == 1, "Test 263: « N » déclenche la commande (" << nNode << ")");
        QTest::keyClick(focusSink, Qt::Key_D);
        QTest::keyClick(focusSink, Qt::Key_A);
        TEST_CHECK(nDim == 1 && nNode == 1, "Test 263: suite de touches « D, A »");
        // Remplacement : N → Ctrl+Alt+K ; l'ancienne touche ne déclenche plus rien.
        ShortcutSettings s = mgr.settings();
        s["cmd.create.node"] = { seqs("Ctrl+Alt+K"), true };
        TEST_CHECK(mgr.saveSettings(s), "Test 263: nouveau raccourci enregistré");
        QTest::keyClick(focusSink, Qt::Key_N);
        QTest::keyClick(focusSink, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        TEST_CHECK(nNode == 2, "Test 263: nouveau raccourci actif, ancien inactif (" << nNode << ")");
        // Champ de saisie : les lettres et Ctrl+C restent au champ.
        edit->setFocus();
        QTest::keyClicks(edit, QStringLiteral("dan"));
        QTest::keyClick(edit, Qt::Key_C, Qt::ControlModifier);
        TEST_CHECK(edit->text() == QStringLiteral("dan") && nDim == 1 && nCopy == 0,
                   "Test 263: saisie dans un champ : aucune commande déclenchée (texte « " << edit->text().toStdString() << " »)");
        // Désactivation : plus de déclenchement.
        s["cmd.create.node"].enabled = false;
        mgr.saveSettings(s);
        focusSink->setFocus();
        QTest::keyClick(focusSink, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        TEST_CHECK(nNode == 2, "Test 263: raccourci désactivé sans effet");
        // Action indisponible : aucun déclenchement, aucun plantage.
        s["cmd.create.node"].enabled = true;
        mgr.saveSettings(s);
        node.setEnabled(false);
        QTest::keyClick(focusSink, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        TEST_CHECK(nNode == 2, "Test 263: commande indisponible : raccourci sans effet");
        qApp->removeEventFilter(guard.get());
        std::cout << "[PASS] Test 263: Frappes clavier simulées" << std::endl;
#else
        std::cout << "[PASS] Test 263: Frappes clavier simulées — NON EXÉCUTÉ (Qt Test absent)" << std::endl;
#endif
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 264 : éditeur des raccourcis — recherche, conflit immédiat, application, restauration
    // -------------------------------------------------------------------------
    {
        QTemporaryDir dir;
        ShortcutManager mgr;
        QAction save(QStringLiteral("Enregistrer")), open(QStringLiteral("Ouvrir")), grid(QStringLiteral("Grille"));
        mgr.bind("cmd.file.save", &save);
        mgr.bind("cmd.file.open", &open);
        mgr.bind("cmd.display.grid", &grid);
        mgr.setConfigPath(dir.filePath("shortcut.txt"));
        mgr.loadConfig();
        TSA::UI::ShortcutEditorDialog dlg(mgr);
        dlg.setSearchText(QStringLiteral("Ctrl+S"));
        TEST_CHECK(dlg.visibleRowCount() == 1, "Test 264: recherche par raccourci");
        dlg.setSearchText(QStringLiteral("grille"));
        TEST_CHECK(dlg.visibleRowCount() == 1, "Test 264: recherche par nom / description");
        dlg.setSearchText(QString());
        TEST_CHECK(dlg.selectCommand("cmd.file.save") && dlg.setSelectedShortcutText("Ctrl+O") && !dlg.conflicts().isEmpty(),
                   "Test 264: conflit détecté immédiatement");
        QStringList errors;
        TEST_CHECK(!dlg.apply(&errors) && save.shortcut() == QKeySequence("Ctrl+S"), "Test 264: application refusée tant que le conflit existe");
        TEST_CHECK(!dlg.setSelectedShortcutText("Ctrl+Truc"), "Test 264: combinaison invalide refusée");
        TEST_CHECK(dlg.setSelectedShortcutText("Ctrl+Shift+F12 ; F12") && dlg.conflicts().isEmpty() && dlg.apply(&errors) &&
                       save.shortcuts() == seqs("Ctrl+Shift+F12 ; F12"),
                   "Test 264: alias appliqués");
        QString content;
        {
            QFile f(mgr.configPath()); // refermé aussitôt : Windows refuse de remplacer un fichier ouvert
            if (f.open(QIODevice::ReadOnly)) content = QString::fromUtf8(f.readAll());
        }
        TEST_CHECK(content.contains(QRegularExpression("cmd\\.file\\.save\\s+\\|\\s+Ctrl\\+Shift\\+F12 ; F12")), "Test 264: shortcut.txt réécrit");
        dlg.selectCommand("cmd.display.grid");
        dlg.setSelectedEnabled(false);
        errors.clear();
        const bool appliedOff = dlg.apply(&errors);
        TEST_CHECK(appliedOff && grid.shortcuts().isEmpty(),
                   "Test 264: désactivation depuis l'éditeur (" << appliedOff << ", " << dlg.workingSettings().value("cmd.display.grid").enabled << ", "
                   << errors.join(";").toStdString() << ")");
        dlg.selectCommand("cmd.file.save");
        dlg.resetSelected();
        TEST_CHECK(dlg.apply() && save.shortcut() == QKeySequence("Ctrl+S"), "Test 264: restauration individuelle");
        dlg.resetAll();
        TEST_CHECK(dlg.apply() && grid.shortcuts() == seqs("G ; F7"), "Test 264: tout rétablir");
        std::cout << "[PASS] Test 264: Éditeur des raccourcis" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 265 : répéter la dernière commande
    // -------------------------------------------------------------------------
    {
        ShortcutManager mgr;
        QAction beam(QStringLiteral("Poutre")), fit(QStringLiteral("Zoom"));
        int nBeam = 0;
        QObject::connect(&beam, &QAction::triggered, [&] { ++nBeam; });
        mgr.bind("cmd.create.beam", &beam);
        mgr.bind("cmd.view.fit_all", &fit);
        QAction* repeat = mgr.repeatAction();
        TEST_CHECK(!repeat->isEnabled() && repeat->shortcut() == QKeySequence("Ctrl+Return"), "Test 265: rien à répéter au départ");
        beam.trigger();
        fit.trigger(); // vue : ne remplace pas la dernière commande de travail
        repeat->trigger();
        TEST_CHECK(nBeam == 2 && repeat->isEnabled() && repeat->text().contains("Poutre"), "Test 265: la dernière commande de modélisation est relancée");
        std::cout << "[PASS] Test 265: Répéter la dernière commande" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 266 : infobulles ; fichier de référence du dépôt synchronisé avec le catalogue
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(stripShortcutHint("Zoom étendu (Fit All)") == "Zoom étendu (Fit All)" && stripShortcutHint("Afficher (G / F7)") == "Afficher" &&
                       stripShortcutHint("Cadrer (Maj+F)") == "Cadrer" && stripShortcutHint("Vue (Num7)") == "Vue" &&
                       stripShortcutHint("Forces (Fx, Fy, Fz)") == "Forces (Fx, Fy, Fz)" && stripShortcutHint("Repère (LCS)") == "Repère (LCS)" &&
                       stripShortcutHint("Thème (Ctrl+T / F10)") == "Thème" && stripShortcutHint("Supprimer (Suppr)") == "Supprimer",
                   "Test 266: suffixes de raccourci retirés, parenthèses descriptives conservées");
        const QString reference = QStringLiteral(TSA_SOURCE_DIR) + QStringLiteral("/docs/shortcut.txt");
        const QString expected = formatConfig(ShortcutManager::catalogDefinitions(), {});
        if (qEnvironmentVariableIsSet("TSA_UPDATE_SHORTCUT_REFERENCE")) writeText(reference, expected);
        QFile ref(reference);
        const bool readable = ref.open(QIODevice::ReadOnly);
        const QString actual = readable ? QString::fromUtf8(ref.readAll()).replace("\r\n", "\n") : QString();
        TEST_CHECK(readable && actual == expected,
                   "Test 266: docs/shortcut.txt à jour (régénérer : TSA_UPDATE_SHORTCUT_REFERENCE=1 TSA_TestSuite --suite=shortcuts)");
        std::cout << "[PASS] Test 266: Infobulles et fichier de référence" << std::endl;
        ++passed;
    }
    return true;
}
