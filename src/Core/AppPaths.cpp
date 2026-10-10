#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace TSA::Core
{

namespace
{
QString ensured(const QString& dir)
{
    if (!dir.isEmpty()) QDir().mkpath(dir);
    return dir;
}

QString writable(QStandardPaths::StandardLocation loc)
{
    QString dir = QStandardPaths::writableLocation(loc);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.tsa");
    return QDir::cleanPath(dir);
}
} // namespace

QString AppPaths::applicationDir()
{
    const QString dir = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();
    return dir.isEmpty() ? QString() : QDir::cleanPath(dir);
}

QString AppPaths::developmentSourceDir()
{
#ifdef TSA_SOURCE_DIR
    // Un paquet (MANIFEST.json à côté de l'exécutable, cmake/PackageTSA.cmake) n'utilise jamais les sources,
    // même si le chemin de compilation existe sur ce poste : il est autonome ou il échoue visiblement.
    if (QFileInfo::exists(applicationDir() + QStringLiteral("/MANIFEST.json"))) return {};
    const QString dir = QString::fromUtf8(TSA_SOURCE_DIR);
    return QFileInfo(dir + QStringLiteral("/CMakeLists.txt")).exists() ? QDir::cleanPath(dir) : QString();
#else
    return {};
#endif
}

QString AppPaths::shippedDir(const QString& relative, const QString& devRelative)
{
    const QString app = applicationDir();
    if (!app.isEmpty())
    {
        const QString shipped = QDir(app).filePath(relative);
        if (QFileInfo(shipped).isDir()) return QDir::cleanPath(shipped);
    }
    const QString src = developmentSourceDir();
    if (!src.isEmpty())
    {
        const QString dev = QDir(src).filePath(devRelative.isEmpty() ? relative : devRelative);
        if (QFileInfo(dev).isDir()) return QDir::cleanPath(dev);
    }
    return app.isEmpty() ? QString() : QDir::cleanPath(QDir(app).filePath(relative));
}

QString AppPaths::occtResourcesDir() { return shippedDir(QStringLiteral("resources/occt"), QStringLiteral("opencascade-8.0.1-vc14-64/src")); }
QString AppPaths::enginesDir() { return shippedDir(QStringLiteral("engines")); }

QString AppPaths::openSeesDir()
{
    // Paquet : engines/OpenSees ; anciennes installations : thirdparty/OpenSees ; développement : sources.
    const QString app = applicationDir();
    for (const QString& rel : { QStringLiteral("engines/OpenSees"), QStringLiteral("thirdparty/OpenSees") })
        if (!app.isEmpty() && QFileInfo(QDir(app).filePath(rel)).isDir()) return QDir::cleanPath(QDir(app).filePath(rel));
    return shippedDir(QStringLiteral("engines/OpenSees"), QStringLiteral("thirdparty/OpenSees"));
}

QString AppPaths::shippedModulesDir() { return shippedDir(QStringLiteral("modules"), QStringLiteral("sdk/modules")); }
QString AppPaths::shippedExtensionsDir() { return shippedDir(QStringLiteral("Extensions")); }

QString AppPaths::userConfigDir() { return ensured(writable(QStandardPaths::AppConfigLocation)); }
QString AppPaths::userDataDir() { return ensured(writable(QStandardPaths::AppLocalDataLocation)); }
QString AppPaths::userTemplatesDir() { return ensured(userDataDir() + QStringLiteral("/templates")); }
QString AppPaths::userModulesDir() { return ensured(userDataDir() + QStringLiteral("/modules")); }
QString AppPaths::userExtensionsDir() { return ensured(userDataDir() + QStringLiteral("/Extensions")); }

QString AppPaths::logsDir()
{
    const QString app = applicationDir();
    if (!app.isEmpty())
    {
        // Poste de développement (jamais pour un paquet) : dossier logs/ existant à côté du dossier de compilation.
        const bool dev = !developmentSourceDir().isEmpty();
        for (const QString& rel : { QStringLiteral("../../logs"), QStringLiteral("../logs") })
        {
            if (!dev) break;
            const QString devLogs = QDir::cleanPath(QDir(app).filePath(rel));
            if (QFileInfo(devLogs).isDir() && isWritableDir(devLogs)) return devLogs;
        }
        const QString local = QDir(app).filePath(QStringLiteral("logs"));
        if (QFileInfo(local).isDir() && isWritableDir(local)) return QDir::cleanPath(local);
        // Installation portable (dossier inscriptible) : logs/ créé à côté de l'exécutable ; sinon
        // (Program Files) : données utilisateur.
        if (dev && isWritableDir(app)) return ensured(QDir::cleanPath(local));
    }
    return ensured(userDataDir() + QStringLiteral("/logs"));
}

bool AppPaths::isWritableDir(const QString& dir)
{
    if (dir.isEmpty() || !QDir().mkpath(dir)) return false;
    QTemporaryFile probe(QDir(dir).filePath(QStringLiteral(".tsa_write_probe_XXXXXX")));
    return probe.open();
}

QString AppPaths::backupFile(const QString& file, const QString& label, QString* error)
{
    if (!QFileInfo::exists(file))
    {
        if (error) *error = QStringLiteral("fichier à sauvegarder absent : %1").arg(QDir::toNativeSeparators(file));
        return {};
    }
    QString target = file + QStringLiteral(".bak-") + label;
    for (int n = 2; QFileInfo::exists(target); ++n) target = file + QStringLiteral(".bak-%1-%2").arg(label).arg(n);
    if (!QFile::copy(file, target))
    {
        if (error) *error = QStringLiteral("sauvegarde impossible de %1").arg(QDir::toNativeSeparators(file));
        return {};
    }
    return target;
}

} // namespace TSA::Core
