#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace TSA::Analysis
{

struct OpenSeesVersionInfo
{
    QString versionString;
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool isValid = false;
    QString executablePath;
    QString banner;
};

/**
 * @brief Gestionnaire centralisé de détection, téléchargement, vérification et configuration d'OpenSees.
 * Conforme aux règles d'architecture TSA : pas de chemins en dur disséminés, non-bloquant,
 * détection déterministe sous Windows.
 */
class OpenSeesManager : public QObject
{
    Q_OBJECT

public:
    static OpenSeesManager& instance();

    // Emplacement par défaut centralisé
    static QString defaultSearchDirectory();
    static QString officialDownloadUrl();

    // État et chemins
    bool isAvailable() const;
    QString executablePath() const;
    void setCustomExecutablePath(const QString& path);
    void resetToDefaultPath();

    // Détection et vérification
    OpenSeesVersionInfo versionInfo() const;
    QString detectVersion(const QString& exePath = QString()) const;
    bool verifyExecutable(const QString& exePath = QString(), QString* errorOutput = nullptr) const;

    // Téléchargement et installation automatique
    bool downloadAndInstall(std::function<void(int percent, const QString& status)> progressCallback = nullptr,
                            QString* errorMessage = nullptr);
    void downloadAndInstallAsync();

    // Assure la disponibilité (détecte ou télécharge automatiquement)
    bool ensureAvailable(QString* errorMessage = nullptr);

signals:
    void downloadProgress(int percent, const QString& statusMessage);
    void downloadCompleted(bool success, const QString& executablePath, const QString& message);
    void executableChanged(const QString& newPath, bool isValid);

private:
    explicit OpenSeesManager(QObject* parent = nullptr);
    ~OpenSeesManager() override = default;

    QString findExecutableInternal() const;
    void loadSettings();
    void saveSettings();

    QString m_customPath;
    mutable QString m_cachedPath;
    mutable OpenSeesVersionInfo m_cachedVersion;
    mutable bool m_isVerified = false;
    bool m_isDownloading = false;
};

} // namespace TSA::Analysis
