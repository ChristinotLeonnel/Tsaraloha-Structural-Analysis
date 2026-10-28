#include "OpenSeesManager.h"
#include "../Core/AppPaths.h"
#include "AI/Models/ModelManager.h"
#include "App/ProductInfo.h"

#include <QCoreApplication>
#include <QDir>
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

QString OpenSeesManager::officialDownloadUrl()
{
    return QStringLiteral("https://opensees.berkeley.edu/OpenSees/code/OpenSees3.8.0-x64.exe.zip");
}

QString OpenSeesManager::officialArchiveSha256()
{
    return QStringLiteral("5e37a5a95c645e31c7700ce6ec5b08c178baedc83241f7cbe10ef10cf6a4fb41");
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

OpenSeesCheckResult OpenSeesManager::checkExecutable(const QString& exePath) const
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

    if (!process.waitForFinished(5000))
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

bool OpenSeesManager::downloadAndInstall(std::function<void(int, const QString&)> progressCallback,
                                        QString* errorMessage)
{
#ifndef Q_OS_WIN
    // L'archive officielle (officialDownloadUrl) ne contient que l'exécutable Windows, installé par curl.exe et PowerShell.
    Q_UNUSED(progressCallback);
    if (errorMessage) *errorMessage = tr("Téléchargement automatique d'OpenSees disponible sous Windows uniquement : installer OpenSees et le placer dans le PATH.");
    emit downloadCompleted(false, "", tr("Téléchargement automatique d'OpenSees disponible sous Windows uniquement : installer OpenSees et le placer dans le PATH."));
    return false;
#endif
    QString targetDir = defaultSearchDirectory();
    QDir dir(targetDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    dir.mkpath("bin");
    dir.mkpath("lib");

    QString zipPath = targetDir + "/OpenSees.zip";
    QString url = officialDownloadUrl();

    if (progressCallback) progressCallback(5, tr("Téléchargement d'OpenSees depuis %1...").arg(url));
    emit downloadProgress(5, tr("Téléchargement d'OpenSees..."));

    // Outils système par leur chemin absolu (jamais résolus par le PATH, qu'un dossier quelconque peut précéder).
    const QString system32 = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32"));

    // Téléchargement via curl Windows
    QProcess curl;
    // --fail : une page d'erreur HTTP (404 du serveur officiel, W-3) n'est jamais enregistrée comme archive.
    QStringList args = {"-L", "--fail", "-o", zipPath, url};
    curl.start(QDir(system32).filePath(QStringLiteral("curl.exe")), args);

    if (!curl.waitForStarted(3000))
    {
        if (errorMessage) *errorMessage = tr("Impossible de lancer curl pour le téléchargement.");
        emit downloadCompleted(false, "", tr("Échec du lancement du téléchargement."));
        return false;
    }

    if (!curl.waitForFinished(180000)) // 3 minutes max
    {
        curl.kill();
        if (errorMessage) *errorMessage = tr("Délai dépassé pendant le téléchargement.");
        emit downloadCompleted(false, "", tr("Délai de téléchargement dépassé."));
        return false;
    }

    if (curl.exitCode() != 0 || !QFile::exists(zipPath))
    {
        QFile::remove(zipPath);
        if (errorMessage) *errorMessage = tr("Téléchargement de l'archive OpenSees impossible (%1, code curl %2). "
                                             "Installer OpenSees manuellement et indiquer son exécutable dans les préférences.")
                                              .arg(url).arg(curl.exitCode());
        emit downloadCompleted(false, "", tr("Erreur de téléchargement."));
        return false;
    }

    QString actualSha256;
    if (!TSA::AI::ModelManager::verifySha256(zipPath, officialArchiveSha256(), &actualSha256))
    {
        QFile::remove(zipPath);
        if (errorMessage) *errorMessage = tr("Archive OpenSees rejetée : empreinte SHA-256 %1, attendue %2.")
                                              .arg(actualSha256, officialArchiveSha256());
        emit downloadCompleted(false, "", tr("Archive rejetée (empreinte inattendue)."));
        return false;
    }

    if (progressCallback) progressCallback(60, tr("Extraction de l'archive OpenSees..."));
    emit downloadProgress(60, tr("Extraction en cours..."));

    // Extraction PowerShell. Les chemins passent par l'environnement du processus, jamais par le texte de la
    // commande : une apostrophe (ou « ; ») dans un nom d'utilisateur ou de dossier ne peut pas devenir du code.
    QString tempExtract = targetDir + "/temp_extract";
    QProcess extract;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TSA_OPS_ZIP"), QDir::toNativeSeparators(zipPath));
    env.insert(QStringLiteral("TSA_OPS_TMP"), QDir::toNativeSeparators(tempExtract));
    env.insert(QStringLiteral("TSA_OPS_DEST"), QDir::toNativeSeparators(targetDir));
    extract.setProcessEnvironment(env);
    const QString script = QStringLiteral("Expand-Archive -LiteralPath $env:TSA_OPS_ZIP -DestinationPath $env:TSA_OPS_TMP -Force; "
                                          "Copy-Item -Path (Join-Path $env:TSA_OPS_TMP 'OpenSees3.8.0/bin/*') -Destination (Join-Path $env:TSA_OPS_DEST 'bin') -Recurse -Force; "
                                          "Copy-Item -Path (Join-Path $env:TSA_OPS_TMP 'OpenSees3.8.0/lib/*') -Destination (Join-Path $env:TSA_OPS_DEST 'lib') -Recurse -Force; "
                                          "Remove-Item -LiteralPath $env:TSA_OPS_TMP -Recurse -Force; "
                                          "Remove-Item -LiteralPath $env:TSA_OPS_ZIP -Force");
    extract.start(QDir(system32).filePath(QStringLiteral("WindowsPowerShell/v1.0/powershell.exe")), QStringList() << "-NoProfile" << "-Command" << script);
    if (!extract.waitForFinished(60000))
    {
        extract.kill();
        if (errorMessage) *errorMessage = tr("Erreur lors de l'extraction de l'archive.");
        emit downloadCompleted(false, "", tr("Échec de l'extraction."));
        return false;
    }

    m_cachedPath.clear();
    m_cachedVersion = {};
    m_isVerified = false;

    if (isAvailable())
    {
        if (progressCallback) progressCallback(100, tr("OpenSees installé et vérifié avec succès."));
        emit downloadCompleted(true, executablePath(), tr("Installation réussie."));
        emit executableChanged(executablePath(), true);
        return true;
    }

    if (errorMessage) *errorMessage = tr("OpenSees a été extrait mais n'a pas pu être validé.");
    emit downloadCompleted(false, "", tr("Vérification échouée après extraction."));
    return false;
}

void OpenSeesManager::downloadAndInstallAsync()
{
    if (m_isDownloading) return;
    m_isDownloading = true;

    QThread* thread = QThread::create([this]() {
        QString err;
        downloadAndInstall([this](int pct, const QString& status) {
            emit downloadProgress(pct, status);
        }, &err);
        m_isDownloading = false;
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
