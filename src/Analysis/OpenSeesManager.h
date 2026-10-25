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

/// Issue de la vérification d'un exécutable OpenSees (OpenSeesManager::checkExecutable, BUG-099).
enum class OpenSeesCheckStatus
{
    Operational,          ///< commande de contrôle exécutée (interpréteur Tcl et commandes OpenSees), version prise en charge
    NotFound,             ///< aucun fichier à ce chemin
    LaunchFailed,         ///< fichier présent mais processus non démarré, ou arrêté anormalement (bibliothèque manquante…)
    Timeout,              ///< aucune fin de processus dans le délai
    TclInitFailed,        ///< OpenSees démarre (bannière) mais sa bibliothèque Tcl (init.tcl) est introuvable
    IncompatibleVersion,  ///< OpenSees opérationnel, version antérieure à minimumSupportedVersion()
    UnexpectedOutput      ///< le programme répond mais n'exécute pas la commande de contrôle (autre programme…)
};

struct OpenSeesCheckResult
{
    OpenSeesCheckStatus status = OpenSeesCheckStatus::NotFound;
    QString version;   ///< rendue par la commande « version », sinon lue dans la bannière ; vide si inconnue
    QString message;   ///< diagnostic traduit, destiné à l'utilisateur
    QString output;    ///< sorties standard et d'erreur du processus
    bool ok() const { return status == OpenSeesCheckStatus::Operational; }
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

    /// Lance l'exécutable et lui envoie sur l'entrée standard une commande Tcl qui exige l'interpréteur et les
    /// commandes OpenSees (wipe, model) avant d'écrire « OPENSEES_CHECK_OK_<version> ». OpenSees 3.8.0 n'a pas
    /// d'option de version (seules -noHeader et -suppressOutput existent) et rend 0 même si Tcl ne s'initialise pas :
    /// seule la présence de ce marqueur prouve le fonctionnement, pas la bannière.
    OpenSeesCheckResult checkExecutable(const QString& exePath) const;
    /// Classe la sortie de la commande de contrôle (sans lancer de processus).
    static OpenSeesCheckResult interpretCheckOutput(const QString& output, int exitCode, bool crashed);
    /// Plus ancienne version prise en charge par les scripts générés (OpenSeesAdapter : 3.4.0 - 3.8.0+).
    static QString minimumSupportedVersion();
    /// Dernière vérification faite par isAvailable() (diagnostic de l'indisponibilité).
    OpenSeesCheckResult lastCheck() const;

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
    mutable OpenSeesCheckResult m_lastCheck;
    bool m_isDownloading = false;
};

} // namespace TSA::Analysis
