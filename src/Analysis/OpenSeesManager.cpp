#include "OpenSeesManager.h"
#include "../Core/AppPaths.h"
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
        defaultDir + "/bin/OpenSees.exe",
        defaultDir + "/OpenSees.exe",
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
    QString path = executablePath();
    if (path.isEmpty() || !QFile::exists(path))
    {
        return false;
    }

    if (!m_isVerified)
    {
        m_isVerified = verifyExecutable(path);
    }

    return m_isVerified;
}

bool OpenSeesManager::verifyExecutable(const QString& exePath, QString* errorOutput) const
{
    QString path = exePath.isEmpty() ? executablePath() : exePath;
    if (path.isEmpty() || !QFile::exists(path))
    {
        if (errorOutput) *errorOutput = tr("Fichier OpenSees.exe introuvable.");
        return false;
    }

    QProcess process;
    process.setProgram(path);
    process.setWorkingDirectory(QFileInfo(path).absolutePath());

    process.start();
    if (!process.waitForStarted(3000))
    {
        if (errorOutput) *errorOutput = tr("Impossible de démarrer le processus OpenSees : %1").arg(process.errorString());
        return false;
    }

    // Envoyer une commande Tcl courte de test de version
    const char testScript[] = "wipe; model BasicBuilder -ndm 2 -ndf 2; puts [format \"OPENSEES_CHECK_OK_%s\" [version]]; exit\n";
    process.write(testScript);
    process.closeWriteChannel();

    if (!process.waitForFinished(5000))
    {
        process.kill();
        if (errorOutput) *errorOutput = tr("Délai d'attente dépassé lors de la vérification d'OpenSees.");
        return false;
    }

    QString out = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());
    QString combined = out + "\n" + err;

    if (combined.contains("OPENSEES_CHECK_OK") || combined.contains("OpenSees"))
    {
        return true;
    }

    if (errorOutput)
    {
        *errorOutput = combined.trimmed();
    }

    return false;
}

QString OpenSeesManager::detectVersion(const QString& exePath) const
{
    QString path = exePath.isEmpty() ? executablePath() : exePath;
    if (path.isEmpty() || !QFile::exists(path)) return QString();

    QProcess process;
    process.setProgram(path);
    process.setWorkingDirectory(QFileInfo(path).absolutePath());
    process.start();

    if (!process.waitForStarted(3000)) return QString();

    process.write("wipe; puts [format \"__OPS_VER__%s\" [version]]; exit\n");
    process.closeWriteChannel();

    if (!process.waitForFinished(5000))
    {
        process.kill();
        return QString();
    }

    QString out = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());
    QString combined = out + "\n" + err;

    QRegularExpression rx("__OPS_VER__([0-9.]+)");
    auto match = rx.match(combined);
    if (match.hasMatch())
    {
        return match.captured(1);
    }

    // Essai d'extraction depuis la bannière standard ("Version 3.8.0 64-Bit")
    QRegularExpression rxBanner("Version\\s+([0-9.]+)", QRegularExpression::CaseInsensitiveOption);
    auto matchBanner = rxBanner.match(combined);
    if (matchBanner.hasMatch())
    {
        return matchBanner.captured(1);
    }

    return "3.8.0";
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

    // Téléchargement via curl Windows
    QProcess curl;
    QStringList args = {"-L", "-o", zipPath, url};
    curl.start("curl.exe", args);

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
        if (errorMessage) *errorMessage = tr("Erreur lors du téléchargement de l'archive OpenSees.");
        emit downloadCompleted(false, "", tr("Erreur de téléchargement."));
        return false;
    }

    if (progressCallback) progressCallback(60, tr("Extraction de l'archive OpenSees..."));
    emit downloadProgress(60, tr("Extraction en cours..."));

    // Extraction PowerShell
    QString tempExtract = targetDir + "/temp_extract";
    QProcess extract;
    QString script = QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force; "
                             "Copy-Item -Path '%2/OpenSees3.8.0/bin/*' -Destination '%3/bin/' -Recurse -Force; "
                             "Copy-Item -Path '%2/OpenSees3.8.0/lib/*' -Destination '%3/lib/' -Recurse -Force; "
                             "Remove-Item -Path '%2' -Recurse -Force; "
                             "Remove-Item -Path '%1' -Force")
                             .arg(QDir::toNativeSeparators(zipPath))
                             .arg(QDir::toNativeSeparators(tempExtract))
                             .arg(QDir::toNativeSeparators(targetDir));

    extract.start("powershell.exe", QStringList() << "-NoProfile" << "-Command" << script);
    if (!extract.waitForFinished(60000))
    {
        extract.kill();
        if (errorMessage) *errorMessage = tr("Erreur lors de l'extraction de l'archive.");
        emit downloadCompleted(false, "", tr("Échec de l'extraction."));
        return false;
    }

    m_cachedPath.clear();
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

    return downloadAndInstall(nullptr, errorMessage);
}

} // namespace TSA::Analysis
