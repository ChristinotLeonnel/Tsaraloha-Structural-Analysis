#pragma once

// Gestion des fichiers de modèles GGUF : inventaire, téléchargement (asynchrone, SHA-256 vérifié
// au fil de l'eau), suppression. Ne charge jamais un modèle : c'est le rôle de LocalLlamaServer.

#include "ModelRegistry.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace TSA::AI
{

struct InstalledModel
{
    QString path;
    QString fileName;
    qint64 sizeBytes = 0;
    QString source;               // « TSA », « Cache Hugging Face », « LM Studio », « Personnalisé »
    const ModelSpec* spec = nullptr; // nullptr : GGUF hors registre (utilisable, besoins inconnus)
    bool sizeMatchesRegistry = true;
    bool removable = false;       // seuls les modèles du dossier TSA sont supprimables depuis TSA

    QString label() const;
};

class ModelManager : public QObject
{
    Q_OBJECT

public:
    explicit ModelManager(const ModelRegistry* registry, QObject* parent = nullptr);
    ~ModelManager() override;

    static QString modelsDirectory();     // <AppData>/ai/models
    /// Dossiers parcourus pour l'inventaire (TSA, cache Hugging Face, LM Studio).
    static QList<QPair<QString, QString>> searchLocations();

    std::vector<InstalledModel> scanInstalled() const;
    const InstalledModel* findInstalled(const std::vector<InstalledModel>& list, const QString& modelId) const;

    bool isDownloading() const { return m_reply != nullptr; }
    QString downloadingModelId() const { return m_downloadSpec ? m_downloadSpec->id : QString(); }

    /// Télécharge le modèle du registre dans modelsDirectory(). Échec immédiat si l'espace disque manque.
    bool startDownload(const QString& modelId, QString* error = nullptr);
    void cancelDownload();

    /// Supprime un modèle du dossier TSA (refus pour les fichiers d'autres applications).
    bool removeModel(const InstalledModel& model, QString* error = nullptr);

    /// Vérification d'intégrité complète d'un fichier (bloquant : appeler hors du thread UI).
    static bool verifySha256(const QString& path, const QString& expectedHex, QString* actualHex = nullptr);

signals:
    void downloadProgress(const QString& modelId, qint64 received, qint64 total);
    void downloadFinished(const QString& modelId, const QString& path);
    void downloadFailed(const QString& modelId, const QString& error);
    void inventoryChanged();

private:
    void onReadyRead();
    void onFinished();
    void failDownload(const QString& error);

private:
    const ModelRegistry* m_registry = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    const ModelSpec* m_downloadSpec = nullptr;
    std::unique_ptr<QFile> m_partFile;
    std::unique_ptr<QCryptographicHash> m_hash;
    qint64 m_received = 0;
};

} // namespace TSA::AI
