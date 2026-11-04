#include "OpenSeesManager.h"
#include "../Core/ExceptionText.h"
#include "../Core/AppPaths.h"
#include "AI/Models/ModelManager.h"
#include "App/ProductInfo.h"

#include <OpenSeesCondaPackages.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QRegularExpression>
#include <QEventLoop>
#include <QVersionNumber>

#include <iostream>

namespace TSA::Analysis
{

OpenSeesManager& OpenSeesManager::instance()
{
    static OpenSeesManager s_instance;
    return s_instance;
}

OpenSeesManager::OpenSeesManager(QObject* parent)
    : QObject(parent)
{
    loadSettings();
}

QString OpenSeesManager::defaultSearchDirectory()
{
    // Paquet : <app>/engines/OpenSees ; anciennes installations : <app>/thirdparty/OpenSees ;
    // développement : sources (Core/AppPaths, docs/DEPLOYMENT.md).
    return TSA::Core::AppPaths::openSeesDir();
}

QString OpenSeesManager::condaChannelUrl()
{
    return QString::fromUtf8(Generated::kOpenSeesCondaChannel);
}

std::vector<OpenSeesManager::CondaPackage> OpenSeesManager::condaPackages()
{
    std::vector<CondaPackage> out;
    for (const char* entry : Generated::kOpenSeesCondaPackages)
    {
        const QStringList parts = QString::fromUtf8(entry).split(QLatin1Char('|'));
        if (parts.size() != 3) continue;
        out.push_back({ parts[0], parts[1].toLower(), parts[2].split(QLatin1Char(','), Qt::SkipEmptyParts) });
    }
    return out;
}

namespace
{
/// Copie récursive (fichiers remplacés) ; faux si un fichier n'a pas pu être copié.
bool copyTree(const QString& from, const QString& to)
{
    QDir src(from);
    if (!src.exists() || !QDir().mkpath(to)) return false;
    for (const QFileInfo& fi : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
    {
        const QString dest = QDir(to).filePath(fi.fileName());
        if (fi.isDir())
        {
            if (!copyTree(fi.absoluteFilePath(), dest)) return false;
        }
        else
        {
            QFile::remove(dest);
            if (!QFile::copy(fi.absoluteFilePath(), dest)) return false;
        }
    }
    return true;
}

/// Outil système de Windows par son chemin absolu (jamais résolu par le PATH, qu'un dossier quelconque peut précéder).
QString systemTool(const QString& name)
{
    const QString system32 = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32"));
    return QDir(system32).filePath(name);
}

/// Lance un outil et attend sa fin ; faux (avec la sortie d'erreur) s'il ne démarre pas, dépasse le délai, échoue ou si
/// l'arrêt est demandé (cancelled, consulté toutes les 200 ms : l'outil est alors arrêté).
bool runTool(const QString& program, const QStringList& args, int timeoutMs, QString* output,
             const std::function<bool()>& cancelled = {})
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(5000))
    {
        if (output) *output = QCoreApplication::translate("TSA::Analysis::OpenSeesManager", "%1 n'a pas pu être lancé.").arg(QFileInfo(program).fileName());
        return false;
    }
    QElapsedTimer clock;
    clock.start();
    while (!p.waitForFinished(200) && p.state() != QProcess::NotRunning)
    {
        const bool stop = cancelled && cancelled();
        if (!stop && clock.elapsed() < timeoutMs) continue;
        p.kill();
        p.waitForFinished(3000);
        if (output)
            *output = stop ? QCoreApplication::translate("TSA::Analysis::OpenSeesManager", "%1 : arrêt demandé.").arg(QFileInfo(program).fileName())
                           : QCoreApplication::translate("TSA::Analysis::OpenSeesManager", "%1 : délai dépassé.").arg(QFileInfo(program).fileName());
        return false;
    }
    if (output) *output = QString::fromLocal8Bit(p.readAll()).trimmed();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}
} // namespace

bool OpenSeesManager::installFromCondaForge(const QString& targetDir, const std::function<void(int, const QString&)>& progress,
                                            QString* errorMessage, const std::function<bool()>& cancelled)
{
    auto fail = [&](const QString& message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };
#ifndef Q_OS_WIN
    Q_UNUSED(targetDir);
    Q_UNUSED(progress);
    Q_UNUSED(cancelled);
    return fail(tr("Installation automatique d'OpenSees disponible sous Windows uniquement : python install_dependencies.py "
                   "--only opensees, ou OpenSees dans le PATH."));
#else
    const QString cancelledText = tr("Installation d'OpenSees annulée : rien n'a été modifié.");
    if (cancelled && cancelled()) return fail(cancelledText);
    const auto packages = condaPackages();
    if (packages.empty()) return fail(tr("Liste des paquets OpenSees vide (cmake/dependencies.json)."));
    // Préparation à côté de la destination ; la destination n'est modifiée que si tout a réussi
    const QString stage = QDir(targetDir).filePath(QStringLiteral(".opensees-install"));
    QDir(stage).removeRecursively();
    const QString root = QDir(stage).filePath(QStringLiteral("OpenSees"));
    if (!QDir().mkpath(QDir(root).filePath(QStringLiteral("bin"))) || !QDir().mkpath(QDir(root).filePath(QStringLiteral("lib"))))
        return fail(tr("Dossier d'installation non accessible en écriture : %1").arg(QDir::toNativeSeparators(targetDir)));
    auto cleanFail = [&](const QString& message) {
        QDir(stage).removeRecursively();
        return fail(message);
    };

    const int count = static_cast<int>(packages.size());
    for (int i = 0; i < count; ++i)
    {
        if (cancelled && cancelled()) return cleanFail(cancelledText);
        const CondaPackage& pkg = packages[static_cast<std::size_t>(i)];
        const QString url = condaChannelUrl() + QLatin1Char('/') + pkg.file + QStringLiteral(".conda");
        const QString archive = QDir(stage).filePath(pkg.file + QStringLiteral(".conda"));
        if (progress) progress(5 + 85 * i / count, tr("OpenSees : paquet %1 sur %2 (%3)…").arg(i + 1).arg(count).arg(pkg.file));
        QString out;
        // --fail : une page d'erreur HTTP n'est jamais enregistrée comme paquet
        if (!runTool(systemTool(QStringLiteral("curl.exe")), { QStringLiteral("-L"), QStringLiteral("--fail"), QStringLiteral("--silent"),
                                                               QStringLiteral("--show-error"), QStringLiteral("-o"), archive, url },
                     300000, &out, cancelled))
            return cleanFail(cancelled && cancelled() ? cancelledText : tr("Téléchargement impossible : %1 (%2).").arg(url, out));
        QString actual;
        if (!TSA::AI::ModelManager::verifySha256(archive, pkg.sha256, &actual))
            return cleanFail(tr("Paquet %1 rejeté : empreinte SHA-256 %2, attendue %3.").arg(pkg.file, actual, pkg.sha256));
        // .conda = zip contenant pkg-<nom>.tar.zst : tar.exe de Windows (bsdtar) lit les deux
        const QString x = QDir(stage).filePath(pkg.file);
        QDir().mkpath(x);
        const QString tar = systemTool(QStringLiteral("tar.exe"));
        if (!runTool(tar, { QStringLiteral("-xf"), archive, QStringLiteral("-C"), x }, 120000, &out, cancelled) ||
            !runTool(tar, { QStringLiteral("-xf"), QDir(x).filePath(QStringLiteral("pkg-%1.tar.zst").arg(pkg.file)), QStringLiteral("-C"), x },
                     120000, &out, cancelled))
            return cleanFail(cancelled && cancelled() ? cancelledText : tr("Extraction de %1 impossible (%2). Le tar de Windows doit lire le format zstd (Windows 11, ou Windows 10 "
                                "à jour) ; sinon : python install_dependencies.py --only opensees.").arg(pkg.file, out));
        for (const QString& f : pkg.files)
        {
            const QString from = QDir(x).filePath(QStringLiteral("Library/bin/") + f);
            const QString to = QDir(root).filePath(QStringLiteral("bin/") + f);
            if (!QFile::exists(from)) return cleanFail(tr("%1 absent du paquet %2.").arg(f, pkg.file));
            QFile::remove(to);
            if (!QFile::copy(from, to)) return cleanFail(tr("Copie de %1 impossible.").arg(f));
        }
        // Bibliothèque de scripts Tcl et modules (.tm : msgcat, utilisé par clock, http, platform…)
        const QString tclLib = QDir(x).filePath(QStringLiteral("Library/lib/tcl8.6"));
        if (QFile::exists(QDir(tclLib).filePath(QStringLiteral("init.tcl"))))
        {
            if (!copyTree(tclLib, QDir(root).filePath(QStringLiteral("lib/tcl8.6"))) ||
                (QDir(QDir(x).filePath(QStringLiteral("Library/lib/tcl8"))).exists() &&
                 !copyTree(QDir(x).filePath(QStringLiteral("Library/lib/tcl8")), QDir(root).filePath(QStringLiteral("lib/tcl8")))))
                return cleanFail(tr("Copie de la bibliothèque Tcl impossible."));
        }
        QDir(x).removeRecursively();
        QFile::remove(archive);
    }
    if (!QFile::exists(QDir(root).filePath(QStringLiteral("lib/tcl8.6/init.tcl"))))
        return cleanFail(tr("Bibliothèque Tcl 8.6 absente des paquets OpenSees."));

    QFile source(QDir(root).filePath(QStringLiteral("SOURCE.txt")));
    if (source.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QStringList lines { QStringLiteral("OpenSees %1 (licence UC, docs/THIRD_PARTY_LICENSES.md), compilé par conda-forge depuis "
                                           "https://github.com/OpenSees/OpenSees/archive/refs/tags/v3.8.0.tar.gz (recette conda-forge/opensees-feedstock).")
                                .arg(QString::fromUtf8(Generated::kOpenSeesVersion)),
                            QStringLiteral("Paquets (cmake/dependencies.json, opensees.windows), installés par TSA (OpenSeesManager) :") };
        for (const auto& pkg : packages) lines << QStringLiteral("  %1|%2").arg(pkg.file, pkg.sha256);
        source.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
        source.close();
    }
    // Dernier point d'arrêt : la copie dans la destination n'est jamais interrompue (installation partielle).
    if (cancelled && cancelled()) return cleanFail(cancelledText);
    if (progress) progress(92, tr("Installation d'OpenSees…"));
    if (!copyTree(root, targetDir))
        return cleanFail(tr("Copie dans %1 impossible.").arg(QDir::toNativeSeparators(targetDir)));
    QDir(stage).removeRecursively();
    return true;
#endif
}

void OpenSeesManager::loadSettings()
{
    QSettings settings(TSA::Product::kOpenSeesSettingsOrganization, TSA::Product::kOpenSeesSettingsApplication);
    m_customPath = settings.value("OpenSees/ExecutablePath", "").toString();
}

void OpenSeesManager::saveSettings()
{
    QSettings settings(TSA::Product::kOpenSeesSettingsOrganization, TSA::Product::kOpenSeesSettingsApplication);
    settings.setValue("OpenSees/ExecutablePath", m_customPath);
}

void OpenSeesManager::setCustomExecutablePath(const QString& path)
{
    m_customPath = path;
    m_cachedPath.clear();
    m_cachedVersion = {};
    m_isVerified = false;
    saveSettings();

    bool valid = isAvailable();
    emit executableChanged(executablePath(), valid);
}

void OpenSeesManager::resetToDefaultPath()
{
    setCustomExecutablePath(QString());
}

QString OpenSeesManager::findExecutableInternal() const
{
    if (!m_customPath.isEmpty() && QFile::exists(m_customPath))
    {
        return QDir::toNativeSeparators(m_customPath);
    }

    QString defaultDir = defaultSearchDirectory();
    QStringList candidateFiles = {
#ifdef Q_OS_WIN
        // Ailleurs, un .exe (exécutable Windows de thirdparty/OpenSees) n'est pas lançable.
        defaultDir + "/bin/OpenSees.exe",
        defaultDir + "/bin/gOpenSees.exe",
        defaultDir + "/OpenSees.exe",
        defaultDir + "/gOpenSees.exe",
#endif
        defaultDir + "/bin/OpenSees",
        defaultDir + "/OpenSees"
    };

    for (const auto& candidate : candidateFiles)
    {
        if (QFile::exists(candidate))
        {
            return QDir::toNativeSeparators(candidate);
        }
    }

    // Recherche dans le PATH système
    QString sysPath = QStandardPaths::findExecutable("OpenSees");
    if (sysPath.isEmpty())
    {
        sysPath = QStandardPaths::findExecutable("gOpenSees");
    }
    if (!sysPath.isEmpty())
    {
        return QDir::toNativeSeparators(sysPath);
    }

    return QString();
}

QString OpenSeesManager::executablePath() const
{
    if (m_cachedPath.isEmpty() || !QFile::exists(m_cachedPath))
    {
        m_cachedPath = findExecutableInternal();
    }
    return m_cachedPath;
}

bool OpenSeesManager::isAvailable() const
{
    if (!m_isVerified)
    {
        m_lastCheck = checkExecutable(executablePath());
        m_isVerified = m_lastCheck.ok();
    }

    return m_isVerified;
}

OpenSeesCheckResult OpenSeesManager::lastCheck() const
{
    return m_lastCheck;
}

QString OpenSeesManager::minimumSupportedVersion()
{
    return QStringLiteral("3.4.0");
}

bool OpenSeesManager::verifyExecutable(const QString& exePath, QString* errorOutput) const
{
    const OpenSeesCheckResult result = checkExecutable(exePath.isEmpty() ? executablePath() : exePath);
    if (!result.ok() && errorOutput)
    {
        *errorOutput = result.output.isEmpty() ? result.message : result.message + "\n" + result.output;
    }
    return result.ok();
}

OpenSeesCheckResult OpenSeesManager::checkExecutable(const QString& exePath, int timeoutMs) const
{
    OpenSeesCheckResult result;
    if (exePath.isEmpty())
    {
        result.message = tr("Exécutable OpenSees introuvable (ni dans %1, ni dans le PATH, ni au chemin enregistré "
                            "« OpenSees/ExecutablePath »).").arg(QDir::toNativeSeparators(defaultSearchDirectory()));
        return result;
    }
    if (!QFileInfo(exePath).isFile())
    {
        result.message = tr("Exécutable OpenSees introuvable : %1").arg(QDir::toNativeSeparators(exePath));
        return result;
    }

    QProcess process;
    process.setProgram(exePath);
    process.setWorkingDirectory(QFileInfo(exePath).absolutePath());

    process.start();
    if (!process.waitForStarted(3000))
    {
        result.status = OpenSeesCheckStatus::LaunchFailed;
        result.message = tr("Impossible de démarrer OpenSees (%1) : %2").arg(QDir::toNativeSeparators(exePath), process.errorString());
        return result;
    }

    // wipe et model n'existent que si l'interpréteur Tcl et les commandes OpenSees sont initialisés.
    const char testScript[] = "wipe; model BasicBuilder -ndm 2 -ndf 2; puts [format \"OPENSEES_CHECK_OK_%s\" [version]]; exit\n";
    process.write(testScript);
    process.closeWriteChannel();

    if (!process.waitForFinished(timeoutMs))
    {
        process.kill();
        process.waitForFinished(1000);
        result.status = OpenSeesCheckStatus::Timeout;
        result.message = tr("Délai d'attente dépassé lors de la vérification d'OpenSees.");
        return result;
    }

    // OpenSees 3.8.0 écrit sa bannière, ses erreurs et ses « puts » sur l'un ou l'autre flux.
    const QString combined = QString::fromUtf8(process.readAllStandardOutput()) + "\n"
                           + QString::fromUtf8(process.readAllStandardError());
    return interpretCheckOutput(combined, process.exitCode(), process.exitStatus() == QProcess::CrashExit);
}

OpenSeesCheckResult OpenSeesManager::interpretCheckOutput(const QString& output, int exitCode, bool crashed)
{
    OpenSeesCheckResult result;
    result.output = output.trimmed();

    // Chiffres exigés : un programme qui recopierait son entrée (« …_%s ») ne passe pas.
    static const QRegularExpression rxCheck(QStringLiteral("OPENSEES_CHECK_OK_([0-9]+(?:\\.[0-9]+)*)"));
    if (const auto check = rxCheck.match(output); check.hasMatch())
    {
        result.version = check.captured(1);
        if (QVersionNumber::fromString(result.version) < QVersionNumber::fromString(minimumSupportedVersion()))
        {
            result.status = OpenSeesCheckStatus::IncompatibleVersion;
            result.message = tr("OpenSees %1 non pris en charge : version %2 ou ultérieure requise.")
                                 .arg(result.version, minimumSupportedVersion());
            return result;
        }
        result.status = OpenSeesCheckStatus::Operational;
        result.message = tr("OpenSees %1 opérationnel.").arg(result.version);
        return result;
    }

    // Diagnostic seulement : la bannière (« Version 3.8.0 64-Bit ») s'affiche avant l'initialisation de Tcl.
    static const QRegularExpression rxBanner(QStringLiteral("Version\\s+([0-9]+(?:\\.[0-9]+)+)"));
    if (const auto banner = rxBanner.match(output); banner.hasMatch()) result.version = banner.captured(1);

    if (output.contains(QStringLiteral("init.tcl")) || output.contains(QStringLiteral("application-specific initialization failed")))
    {
        result.status = OpenSeesCheckStatus::TclInitFailed;
        result.message = tr("OpenSees démarre mais son interpréteur Tcl ne s'initialise pas (init.tcl introuvable) : "
                            "la bibliothèque Tcl doit se trouver dans <dossier de l'exécutable>/../lib/tcl8.6.");
        return result;
    }
    if (crashed || exitCode != 0)
    {
        result.status = OpenSeesCheckStatus::LaunchFailed;
        result.message = tr("OpenSees s'est arrêté anormalement (code %1) : bibliothèque manquante ou exécutable "
                            "incompatible avec ce système.").arg(exitCode);
        return result;
    }
    result.status = OpenSeesCheckStatus::UnexpectedOutput;
    result.message = tr("Le programme ne répond pas comme OpenSees à la commande de contrôle.");
    return result;
}

QString OpenSeesManager::detectVersion(const QString& exePath) const
{
    // Version rendue par la commande « version », sinon celle de la bannière ; vide si aucune n'est lisible.
    const QString path = exePath.isEmpty() ? executablePath() : exePath;
    if (path.isEmpty()) return QString();
    return checkExecutable(path).version;
}

OpenSeesVersionInfo OpenSeesManager::versionInfo() const
{
    if (m_cachedVersion.isValid)
    {
        return m_cachedVersion;
    }

    OpenSeesVersionInfo info;
    info.executablePath = executablePath();
    if (info.executablePath.isEmpty())
    {
        return info;
    }

    info.versionString = detectVersion(info.executablePath);
    if (!info.versionString.isEmpty())
    {
        QStringList parts = info.versionString.split('.');
        if (parts.size() >= 1) info.major = parts[0].toInt();
        if (parts.size() >= 2) info.minor = parts[1].toInt();
        if (parts.size() >= 3) info.patch = parts[2].toInt();
        info.isValid = true;
    }

    m_cachedVersion = info;
    return info;
}

void OpenSeesManager::runInOwnThread(const std::function<void()>& fn)
{
    if (QThread::currentThread() == thread())
        fn();
    else   // thread de travail : attente de l'exécution par la boucle d'événements du gestionnaire (thread de l'interface)
        QMetaObject::invokeMethod(this, [&fn] { fn(); }, Qt::BlockingQueuedConnection);
}

bool OpenSeesManager::downloadAndInstall(std::function<void(int, const QString&)> progressCallback,
                                        QString* errorMessage, std::function<bool()> cancelled)
{
    if (m_isDownloading.exchange(true))
    {
        if (errorMessage) *errorMessage = tr("Une installation d'OpenSees est déjà en cours.");
        return false;
    }
    struct DownloadFlag
    {
        std::atomic<bool>& flag;
        ~DownloadFlag() { flag = false; }
    } downloadFlag { m_isDownloading };

    auto report = [&](int percent, const QString& status) {
        if (progressCallback) progressCallback(percent, status);
        emit downloadProgress(percent, status);
    };

    // 1. Travail long, sans état du gestionnaire (possible dans un thread de travail) : paquets conda-forge épinglés
    //    (l'archive officielle d'opensees.berkeley.edu n'existe plus : 404, BUG-133), puis premier lancement de
    //    l'exécutable neuf, avec un délai long (analyse antivirus possible).
    const QString targetDir = defaultSearchDirectory();
    const QString installedExe = QDir(targetDir).filePath(QStringLiteral("bin/OpenSees.exe"));
    report(5, tr("Téléchargement d'OpenSees…"));
    QString error;
    const bool installed = installFromCondaForge(targetDir, report, &error, cancelled);
    OpenSeesCheckResult firstCheck;
    if (installed)
    {
        report(95, tr("Vérification d'OpenSees (premier lancement)…"));
        firstCheck = checkExecutable(installedExe, 60000);
    }

    // 2. Caches, vérification et signaux dans le thread du gestionnaire : l'interface lit cet état pendant le travail.
    bool ok = false;
    runInOwnThread([&] {
        if (!installed)
        {
            emit downloadCompleted(false, QString(), error);
            return;
        }
        m_cachedPath.clear();
        m_cachedVersion = {};
        m_isVerified = false;
        if (QDir::fromNativeSeparators(executablePath()) == QDir::fromNativeSeparators(installedExe))
        {
            m_lastCheck = firstCheck;   // vérification déjà faite à l'étape 1 : aucun lancement dans ce thread
            m_isVerified = firstCheck.ok();
            ok = m_isVerified;
        }
        else
        {
            ok = isAvailable();   // autre exécutable retenu (chemin enregistré, PATH) : vérification habituelle
        }
        if (ok)
        {
            emit downloadCompleted(true, executablePath(), tr("Installation réussie."));
            emit executableChanged(executablePath(), true);
        }
        else
        {
            emit downloadCompleted(false, QString(), tr("Vérification échouée après extraction."));
        }
    });

    if (ok)
    {
        if (progressCallback) progressCallback(100, tr("OpenSees installé et vérifié avec succès."));
        return true;
    }
    if (errorMessage)
    {
        if (!installed)
            *errorMessage = cancelled && cancelled() ? error
                                                     : error + QLatin1Char(' ') + tr("Ou indiquer l'exécutable d'OpenSees dans les préférences.");
        else
            *errorMessage = tr("OpenSees a été extrait mais n'a pas pu être validé.")
                          + (firstCheck.message.isEmpty() ? QString() : QLatin1Char(' ') + firstCheck.message);
    }
    return false;
}

void OpenSeesManager::downloadAndInstallAsync()
{
    if (m_isDownloading) return;

    QThread* thread = QThread::create([this]() {
        QString err;
        try
        {
            downloadAndInstall(nullptr, &err);   // avancement : signal downloadProgress
        }
        catch (...)   // frontière du thread (BUG-155)
        {
            emit downloadCompleted(false, QString(), tr("Installation interrompue par une erreur interne (%1).")
                                                         .arg(QString::fromStdString(TSA::Core::currentExceptionText())));
        }
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool OpenSeesManager::ensureAvailable(QString* errorMessage)
{
    if (isAvailable())
    {
        return true;
    }

    // Jamais de téléchargement implicite (au lancement d'un calcul, depuis la console, l'IA ou un Blueprint) :
    // installer un exécutable tiers demande l'accord de l'utilisateur (OpenSeesEngine::provision, proposé par la
    // fenêtre d'analyse) et sa licence n'autorise pas toute utilisation (docs/THIRD_PARTY_LICENSES.md).
    if (errorMessage) *errorMessage = m_lastCheck.message;
    return false;
}

} // namespace TSA::Analysis
