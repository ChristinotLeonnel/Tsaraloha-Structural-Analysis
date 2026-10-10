// Suite « deployment » : distribution autonome (tests 286-288, docs/DEPLOYMENT.md). Résolution des chemins
// indépendante du dossier de travail, données utilisateur créées et inscriptibles, sauvegarde avant
// migration ; shortcut.txt : création, migration de format avec sauvegarde et réglages conservés, format
// futur non réécrit, fichier illisible ou corrompu sans plantage.

#include "test_common.h"

#include "Core/AppPaths.h"
#include "UI/Shortcuts/ShortcutConfig.h"
#include "UI/Shortcuts/ShortcutManager.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

using TSA::Core::AppPaths;
using TSA::UI::Shortcuts::ShortcutManager;

namespace
{
bool writeAll(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(bytes) == bytes.size();
}
QString readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
} // namespace

bool runSuite_Deployment(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 286 : chemins indépendants du dossier de travail
    // -------------------------------------------------------------------------
    {
        const QString before[] = { AppPaths::applicationDir(), AppPaths::shippedModulesDir(), AppPaths::occtResourcesDir(), AppPaths::openSeesDir(),
                                   AppPaths::userConfigDir(), AppPaths::logsDir() };
        const QString cwd = QDir::currentPath();
        QTemporaryDir elsewhere;
        QDir::setCurrent(elsewhere.path());   // lancement depuis un autre dossier (raccourci, ligne de commande)
        const QString after[] = { AppPaths::applicationDir(), AppPaths::shippedModulesDir(), AppPaths::occtResourcesDir(), AppPaths::openSeesDir(),
                                  AppPaths::userConfigDir(), AppPaths::logsDir() };
        QDir::setCurrent(cwd);
        bool same = true, absolute = true;
        for (int i = 0; i < 6; ++i)
        {
            same &= before[i] == after[i];
            absolute &= before[i].isEmpty() || QDir::isAbsolutePath(before[i]);
        }
        TEST_CHECK(same && absolute, "Test 286: chemins identiques et absolus quel que soit le dossier de travail");
        TEST_CHECK(AppPaths::applicationDir() == QDir::cleanPath(QCoreApplication::applicationDirPath()), "Test 286: dossier de l'application = dossier de l'exécutable");
        TEST_CHECK(!AppPaths::shippedModulesDir().isEmpty() && QFileInfo(AppPaths::occtResourcesDir() + "/Shaders").isDir(),
                   "Test 286: modules livrés et ressources OCCT trouvés (" << AppPaths::occtResourcesDir().toStdString() << ")");
        TEST_CHECK(QFileInfo(AppPaths::openSeesDir()).isDir(), "Test 286: moteur OpenSees trouvé (" << AppPaths::openSeesDir().toStdString() << ")");
        std::cout << "[PASS] Test 286: Chemins indépendants du dossier de travail" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 287 : données utilisateur et sauvegarde avant migration
    // -------------------------------------------------------------------------
    {
        for (const QString& d : { AppPaths::userConfigDir(), AppPaths::userDataDir(), AppPaths::userTemplatesDir(), AppPaths::userModulesDir(),
                                  AppPaths::userExtensionsDir(), AppPaths::logsDir() })
            TEST_CHECK(QFileInfo(d).isDir() && AppPaths::isWritableDir(d), "Test 287: dossier utilisateur créé et inscriptible (" << d.toStdString() << ")");
        TEST_CHECK(!AppPaths::userDataDir().startsWith(AppPaths::applicationDir() + "/"), "Test 287: données utilisateur hors du dossier de l'application");
        QTemporaryDir tmp;
        const QString f = tmp.filePath("config.txt");
        writeAll(f, "v0");
        QString err;
        const QString b1 = AppPaths::backupFile(f, "v0", &err), b2 = AppPaths::backupFile(f, "v0", &err);
        TEST_CHECK(b1.endsWith("config.txt.bak-v0") && b2.endsWith("config.txt.bak-v0-2") && readAll(b1) == "v0" && readAll(b2) == "v0" && readAll(f) == "v0",
                   "Test 287: sauvegardes numérotées, jamais écrasées, original intact");
        TEST_CHECK(AppPaths::backupFile(tmp.filePath("absent.txt"), "x", &err).isEmpty() && !err.isEmpty(), "Test 287: sauvegarde d'un fichier absent signalée");
        std::cout << "[PASS] Test 287: Données utilisateur et sauvegardes" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 288 : shortcut.txt — migration de format, format futur, fichier corrompu
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath("shortcut.txt");
        ShortcutManager mgr;
        QAction save(QStringLiteral("Enregistrer"));
        TEST_CHECK(mgr.bind("cmd.file.save", &save), "Test 288: action reliée");
        mgr.setConfigPath(path);
        // Fichier d'un format plus ancien, avec une personnalisation.
        QString old = mgr.defaultConfigText();
        old.replace(QStringLiteral("# format: tsa-shortcuts %1").arg(TSA::UI::Shortcuts::shortcutFormatVersion()), QStringLiteral("# format: tsa-shortcuts 0"));
        old.replace(QRegularExpression("(cmd\\.file\\.save\\s+\\|)\\s*Ctrl\\+S\\s*\\|"), "\\1 Ctrl+Alt+S |");
        TEST_CHECK(old.contains("tsa-shortcuts 0") && writeAll(path, old.toUtf8()), "Test 288: fichier au format 0 préparé");
        TEST_CHECK(mgr.loadConfig(), "Test 288: fichier ancien chargé (" << mgr.lastErrors().join(" | ").toStdString() << ")");
        const QString migrated = readAll(path);
        TEST_CHECK(QFile::exists(path + ".bak-v0") && readAll(path + ".bak-v0") == old, "Test 288: original sauvegardé avant migration");
        TEST_CHECK(migrated.contains(QStringLiteral("tsa-shortcuts %1").arg(TSA::UI::Shortcuts::shortcutFormatVersion())) &&
                       QRegularExpression("cmd\\.file\\.save\\s+\\|\\s*Ctrl\\+Alt\\+S").match(migrated).hasMatch() && save.shortcuts() == QList<QKeySequence> { QKeySequence("Ctrl+Alt+S") },
                   "Test 288: fichier réécrit au format actuel, personnalisation conservée et appliquée");
        TEST_CHECK(mgr.lastWarnings().join(" ").contains("sauvegardée"), "Test 288: migration signalée");

        // Format plus récent : jamais réécrit.
        QString future = migrated;
        future.replace(QStringLiteral("# format: tsa-shortcuts %1").arg(TSA::UI::Shortcuts::shortcutFormatVersion()), QStringLiteral("# format: tsa-shortcuts 99"));
        TEST_CHECK(writeAll(path, future.toUtf8()) && mgr.reload() && readAll(path) == future && !QFile::exists(path + ".bak-v99") &&
                       mgr.lastWarnings().join(" ").contains("plus récent"),
                   "Test 288: format futur lu, non réécrit, avertissement");

        // Fichier corrompu (octets aléatoires) puis vide : aucun plantage, dernière configuration valide conservée.
        QByteArray garbage;
        for (int i = 0; i < 4096; ++i) garbage.append(char((i * 7919) % 251));
        bool survived = writeAll(path, garbage);
        mgr.reload();
        survived &= save.shortcuts() == QList<QKeySequence> { QKeySequence("Ctrl+Alt+S") } || !save.shortcuts().isEmpty();
        survived &= writeAll(path, QByteArray());
        mgr.reload();
        TEST_CHECK(survived && !save.shortcuts().isEmpty(), "Test 288: fichier corrompu ou vide sans plantage, raccourcis toujours actifs");
        // Fichier absent dans un dossier inexistant : recréé.
        ShortcutManager fresh;
        fresh.setConfigPath(tmp.filePath("nouveau/dossier/shortcut.txt"));
        TEST_CHECK(fresh.loadConfig() && QFile::exists(tmp.filePath("nouveau/dossier/shortcut.txt")), "Test 288: fichier absent recréé à la première exécution");
        std::cout << "[PASS] Test 288: shortcut.txt robuste et migré" << std::endl;
        ++passed;
    }
    return true;
}
