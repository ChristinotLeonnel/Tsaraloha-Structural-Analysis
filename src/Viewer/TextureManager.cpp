#include "TextureManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QStandardPaths>
#include <algorithm>
#include <cctype>

namespace TSA::Viewer
{

static std::string toLowerStr(const std::string& str)
{
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

TextureManager& TextureManager::instance()
{
    static TextureManager inst;
    return inst;
}

TextureManager::TextureManager(QObject* parent)
    : QObject(parent)
{
    initialize();
}

void TextureManager::initialize(const QString& applicationDirPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();

    QString appDir = applicationDirPath;
    if (appDir.isEmpty() && QCoreApplication::instance())
    {
        appDir = QCoreApplication::applicationDirPath();
    }
    if (appDir.isEmpty())
    {
        appDir = QDir::currentPath();
    }

    // 1. Extensions/TSALib/Textures local à l'application
    QString appTex = QDir(appDir).filePath("Extensions/TSALib/Textures");
    if (QDir(appTex).exists() && !m_searchPaths.contains(appTex))
        m_searchPaths.append(appTex);

    // 2. Extensions/TSALib/Textures dans le répertoire de travail
    QString curTex = QDir(QDir::currentPath()).filePath("Extensions/TSALib/Textures");
    if (QDir(curTex).exists() && !m_searchPaths.contains(curTex))
        m_searchPaths.append(curTex);

    // 3. Répertoire direct e:/Book/Dev/TSA/Extensions/TSALib/Textures
    QString devTex = "e:/Book/Dev/TSA/Extensions/TSALib/Textures";
    if (QDir(devTex).exists() && !m_searchPaths.contains(devTex))
        m_searchPaths.append(devTex);

    // 4. Dossier utilisateur %APPDATA%/TSA/Textures
    QString userDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!userDir.isEmpty())
    {
        QString userTex = QDir(userDir).filePath("Textures");
        if (QDir(userTex).exists() && !m_searchPaths.contains(userTex))
            m_searchPaths.append(userTex);
    }

    // Balayage immédiat des chemins configurés
    m_textures.clear();
    m_resolvedCache.clear();
    for (const QString& path : m_searchPaths)
    {
        scanSearchPath(path);
    }
}

void TextureManager::addSearchPath(const QString& dirPath)
{
    if (dirPath.isEmpty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    QDir dir(dirPath);
    QString clean = dir.cleanPath(dirPath);
    if (!m_searchPaths.contains(clean))
    {
        m_searchPaths.append(clean);
        scanSearchPath(clean);
    }
}

QStringList TextureManager::searchPaths() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_searchPaths;
}

void TextureManager::scanSearchPath(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) return;

    // 1. Lire textures.json si présent
    QString manifestPath = dir.filePath("textures.json");
    if (QFile::exists(manifestPath))
    {
        QFile f(manifestPath);
        if (f.open(QIODevice::ReadOnly))
        {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject())
            {
                QJsonArray arr = doc.object()["textures"].toArray();
                for (const auto& item : arr)
                {
                    QJsonObject obj = item.toObject();
                    TextureEntry entry;
                    entry.id = obj["id"].toString();
                    entry.name = obj["name"].toString();
                    entry.category = obj["category"].toString();
                    QString relFile = obj["file"].toString();
                    entry.filePath = dir.filePath(relFile);
                    entry.repeatU = obj.contains("repeat_u") ? obj["repeat_u"].toDouble(1.0) : 1.0;
                    entry.repeatV = obj.contains("repeat_v") ? obj["repeat_v"].toDouble(1.0) : 1.0;

                    if (QFile::exists(entry.filePath))
                    {
                        m_textures[toLowerStr(entry.id.toStdString())] = entry;
                        m_textures[toLowerStr(relFile.toStdString())] = entry;
                    }
                }
            }
        }
    }

    // 2. Découverte de toutes les images PNG/JPG dans le répertoire
    QDirIterator it(path, QStringList() << "*.png" << "*.jpg" << "*.jpeg", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString filePath = it.next();
        QFileInfo fi(filePath);
        std::string baseId = toLowerStr(fi.baseName().toStdString());
        std::string fileName = toLowerStr(fi.fileName().toStdString());

        if (m_textures.find(baseId) == m_textures.end())
        {
            TextureEntry entry;
            entry.id = QString::fromStdString(baseId);
            entry.name = fi.baseName();
            entry.category = "Materials";
            entry.filePath = filePath;
            entry.repeatU = 1.0;
            entry.repeatV = 1.0;
            m_textures[baseId] = entry;
            m_textures[fileName] = entry;
        }
    }
}

QString TextureManager::resolveTexturePath(const std::string& textureNameOrPath) const
{
    if (textureNameOrPath.empty()) return QString();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Vérifier dans le cache de résolution rapide
    auto itCache = m_resolvedCache.find(textureNameOrPath);
    if (itCache != m_resolvedCache.end())
    {
        return itCache->second;
    }

    QString rawPath = QString::fromStdString(textureNameOrPath).replace('\\', '/');
    QFileInfo directFi(rawPath);

    // 1. Chemin absolu direct existant
    if (directFi.isAbsolute() && directFi.exists())
    {
        m_resolvedCache[textureNameOrPath] = directFi.absoluteFilePath();
        return directFi.absoluteFilePath();
    }

    // 2. Recherche par ID dans m_textures
    std::string lKey = toLowerStr(textureNameOrPath);
    auto itEntry = m_textures.find(lKey);
    if (itEntry != m_textures.end() && QFile::exists(itEntry->second.filePath))
    {
        m_resolvedCache[textureNameOrPath] = itEntry->second.filePath;
        return itEntry->second.filePath;
    }

    // Extraire uniquement le nom de fichier si un chemin relatif est passé (ex: "Textures/concrete.png" -> "concrete")
    QFileInfo strippedFi(rawPath);
    std::string baseOnly = toLowerStr(strippedFi.baseName().toStdString());
    itEntry = m_textures.find(baseOnly);
    if (itEntry != m_textures.end() && QFile::exists(itEntry->second.filePath))
    {
        m_resolvedCache[textureNameOrPath] = itEntry->second.filePath;
        return itEntry->second.filePath;
    }

    // 3. Recherche directe dans tous les répertoires de recherche
    for (const QString& basePath : m_searchPaths)
    {
        QDir d(basePath);
        // Essayer tel quel
        QString candidate = d.filePath(strippedFi.fileName());
        if (QFile::exists(candidate))
        {
            m_resolvedCache[textureNameOrPath] = candidate;
            return candidate;
        }

        // Essayer avec extensions courantes
        QStringList exts = { ".png", ".jpg", ".jpeg" };
        for (const QString& ext : exts)
        {
            QString withExt = d.filePath(strippedFi.baseName() + ext);
            if (QFile::exists(withExt))
            {
                m_resolvedCache[textureNameOrPath] = withExt;
                return withExt;
            }
        }
    }

    return QString();
}

bool TextureManager::hasTexture(const std::string& textureNameOrPath) const
{
    return !resolveTexturePath(textureNameOrPath).isEmpty();
}

bool TextureManager::registerTexture(const TextureEntry& entry)
{
    if (entry.id.isEmpty() || entry.filePath.isEmpty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures[toLowerStr(entry.id.toStdString())] = entry;
    m_resolvedCache.clear();
    return true;
}

void TextureManager::reloadTextures()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures.clear();
    m_resolvedCache.clear();
    for (const QString& path : m_searchPaths)
    {
        scanSearchPath(path);
    }
    int total = static_cast<int>(m_textures.size());
    emit texturesReloaded(total);
}

void TextureManager::clearCache()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resolvedCache.clear();
}

std::vector<TextureEntry> TextureManager::allTextures() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TextureEntry> result;
    std::unordered_map<std::string, bool> seen;
    for (const auto& [_, entry] : m_textures)
    {
        if (!seen[entry.filePath.toStdString()])
        {
            seen[entry.filePath.toStdString()] = true;
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<TextureEntry> TextureManager::texturesByCategory(const QString& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TextureEntry> result;
    std::unordered_map<std::string, bool> seen;
    QString lCat = category.toLower();
    for (const auto& [_, entry] : m_textures)
    {
        if (entry.category.toLower() == lCat && !seen[entry.filePath.toStdString()])
        {
            seen[entry.filePath.toStdString()] = true;
            result.push_back(entry);
        }
    }
    return result;
}

size_t TextureManager::count() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.size();
}

} // namespace TSA::Viewer
