#include "ModelManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QRegularExpression>

namespace TSA::AI
{

QString InstalledModel::label() const
{
    return spec ? spec->label() : fileName;
}

ModelManager::ModelManager(const ModelRegistry* registry, QObject* parent)
    : QObject(parent)
    , m_registry(registry)
    , m_network(new QNetworkAccessManager(this))
{
}

ModelManager::~ModelManager()
{
    cancelDownload();
}

QString ModelManager::modelsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ai/models";
}

QList<QPair<QString, QString>> ModelManager::searchLocations()
{
    const QString home = QDir::homePath();
    return {
        { modelsDirectory(), QStringLiteral("TSA") },
        { home + "/.cache/huggingface/hub", QStringLiteral("Cache Hugging Face") },
        { home + "/.lmstudio/models", QStringLiteral("LM Studio") },
        { home + "/.cache/lm-studio/models", QStringLiteral("LM Studio") },
    };
}

std::vector<InstalledModel> ModelManager::scanInstalled() const
{
    std::vector<InstalledModel> result;
    QStringList seenCanonical;
    for (const auto& [dir, source] : searchLocations())
    {
        if (!QFileInfo::exists(dir)) continue;
        QDirIterator it(dir, { "*.gguf" }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QFileInfo fi(it.next());
            const QString name = fi.fileName();
            // Projecteurs vision et fragments de modèles découpés : pas des modèles autonomes.
            if (name.startsWith("mmproj", Qt::CaseInsensitive) || name.contains(QRegularExpression("-0000[2-9]-of-")))
                continue;
            const QString canonical = fi.canonicalFilePath();
            if (seenCanonical.contains(canonical)) continue;
            seenCanonical << canonical;

            InstalledModel m;
            m.path = fi.absoluteFilePath();
            m.fileName = name;
            m.sizeBytes = fi.size();
            m.source = source;
            m.removable = (source == QStringLiteral("TSA"));
            m.spec = m_registry ? m_registry->findByFileName(name) : nullptr;
            if (m.spec) m.sizeMatchesRegistry = (m.sizeBytes == m.spec->fileSizeBytes);
            if (!m.spec && source == QStringLiteral("TSA")) m.source = QStringLiteral("Personnalisé");
            result.push_back(m);
        }
    }
    return result;
}

const InstalledModel* ModelManager::findInstalled(const std::vector<InstalledModel>& list, const QString& modelId) const
{
    for (const auto& m : list)
        if (m.spec && m.spec->id == modelId && m.sizeMatchesRegistry) return &m;
    return nullptr;
}

bool ModelManager::startDownload(const QString& modelId, QString* error)
{
    if (m_reply)
    {
        if (error) *error = QStringLiteral("Un téléchargement est déjà en cours.");
        return false;
    }
    const ModelSpec* spec = m_registry ? m_registry->find(modelId) : nullptr;
    if (!spec || spec->downloadUrl().isEmpty())
    {
        if (error) *error = QStringLiteral("Modèle inconnu ou sans source de téléchargement : %1").arg(modelId);
        return false;
    }
    QDir().mkpath(modelsDirectory());
    const QStorageInfo storage(modelsDirectory());
    if (storage.isValid() && storage.bytesAvailable() < spec->fileSizeBytes + (512LL << 20))
    {
        if (error)
            *error = QStringLiteral("Espace disque insuffisant : %1 Go requis, %2 Go libres.")
                         .arg(spec->fileSizeGB() + 0.5, 0, 'f', 1)
                         .arg(storage.bytesAvailable() / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
        return false;
    }

    m_partFile = std::make_unique<QFile>(modelsDirectory() + "/" + spec->hfFile + ".part");
    if (!m_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error) *error = QStringLiteral("Impossible d'écrire %1").arg(m_partFile->fileName());
        m_partFile.reset();
        return false;
    }
    m_hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    m_received = 0;
    m_downloadSpec = spec;

    QNetworkRequest req{ QUrl(spec->downloadUrl()) };
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TSA-Structural-Analysis"));
    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &ModelManager::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &ModelManager::onFinished);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 rec, qint64 total) {
        if (m_downloadSpec)
            emit downloadProgress(m_downloadSpec->id, rec, total > 0 ? total : m_downloadSpec->fileSizeBytes);
    });
    return true;
}

void ModelManager::onReadyRead()
{
    if (!m_reply || !m_partFile) return;
    const QByteArray chunk = m_reply->readAll();
    m_hash->addData(chunk);
    m_received += chunk.size();
    if (m_partFile->write(chunk) != chunk.size())
        failDownload(QStringLiteral("Erreur d'écriture disque."));
}

void ModelManager::failDownload(const QString& error)
{
    const QString id = m_downloadSpec ? m_downloadSpec->id : QString();
    if (m_reply)
    {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_partFile)
    {
        m_partFile->close();
        m_partFile->remove();
        m_partFile.reset();
    }
    m_hash.reset();
    m_downloadSpec = nullptr;
    emit downloadFailed(id, error);
}

void ModelManager::onFinished()
{
    if (!m_reply || !m_downloadSpec) return;
    if (m_reply->error() != QNetworkReply::NoError)
    {
        failDownload(m_reply->errorString());
        return;
    }
    onReadyRead(); // derniers octets éventuels
    const ModelSpec* spec = m_downloadSpec;
    const QString actual = QString::fromLatin1(m_hash->result().toHex());
    m_partFile->close();

    if (m_received != spec->fileSizeBytes)
    {
        failDownload(QStringLiteral("Taille inattendue : %1 octets reçus, %2 attendus.").arg(m_received).arg(spec->fileSizeBytes));
        return;
    }
    if (!spec->sha256.isEmpty() && actual != spec->sha256)
    {
        failDownload(QStringLiteral("Somme SHA-256 incorrecte : fichier corrompu ou modifié."));
        return;
    }

    const QString finalPath = modelsDirectory() + "/" + spec->hfFile;
    QFile::remove(finalPath);
    if (!m_partFile->rename(finalPath))
    {
        failDownload(QStringLiteral("Impossible de finaliser %1").arg(finalPath));
        return;
    }
    m_partFile.reset();
    m_hash.reset();
    m_reply->deleteLater();
    m_reply = nullptr;
    m_downloadSpec = nullptr;
    emit downloadFinished(spec->id, finalPath);
    emit inventoryChanged();
}

void ModelManager::cancelDownload()
{
    if (m_reply) failDownload(QStringLiteral("Téléchargement annulé."));
}

bool ModelManager::removeModel(const InstalledModel& model, QString* error)
{
    if (!model.removable)
    {
        if (error) *error = QStringLiteral("Ce fichier appartient à une autre application (%1) : TSA ne le supprime pas.").arg(model.source);
        return false;
    }
    if (!QFile::remove(model.path))
    {
        if (error) *error = QStringLiteral("Suppression impossible (fichier en cours d'utilisation ?) : %1").arg(model.path);
        return false;
    }
    emit inventoryChanged();
    return true;
}

bool ModelManager::verifySha256(const QString& path, const QString& expectedHex, QString* actualHex)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return false;
    const QString actual = QString::fromLatin1(h.result().toHex());
    if (actualHex) *actualHex = actual;
    return expectedHex.isEmpty() || actual == expectedHex.toLower();
}

} // namespace TSA::AI
