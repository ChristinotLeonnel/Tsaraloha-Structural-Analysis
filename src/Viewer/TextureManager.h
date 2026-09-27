#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <QObject>

namespace TSA::Viewer
{

struct TextureEntry
{
    QString id;          // ex: "concrete"
    QString name;        // ex: "Concrete C25/30"
    QString category;    // "Concrete", "Steel", "Timber", etc.
    QString filePath;    // Chemin absolu ou relatif validé vers le PNG/JPG
    double repeatU = 1.0;
    double repeatV = 1.0;
};

/**
 * @brief Gestionnaire de textures externes pour le rendu PBR OpenCASCADE.
 * Gère la découverte dynamique, le cache en mémoire et le rechargement à chaud (hot-reload).
 */
class TextureManager : public QObject
{
    Q_OBJECT

public:
    static TextureManager& instance();

    // Initialisation avec répertoires par défaut
    void initialize(const QString& appDir = QString());

    // Gestion des chemins de recherche de textures
    void addSearchPath(const QString& dirPath);
    QStringList searchPaths() const;

    // Résolution d'un chemin de texture (ID ou chemin partiel vers fichier réel)
    QString resolveTexturePath(const std::string& textureNameOrPath) const;
    bool hasTexture(const std::string& textureNameOrPath) const;

    // Enregistrement manuel
    bool registerTexture(const TextureEntry& entry);

    // Découverte et rechargement dynamique (hot reload)
    void reloadTextures();
    void clearCache();

    // Consultation du catalogue
    std::vector<TextureEntry> allTextures() const;
    std::vector<TextureEntry> texturesByCategory(const QString& category) const;
    size_t count() const;

signals:
    void texturesReloaded(int count);

public:
    explicit TextureManager(QObject* parent = nullptr);
    ~TextureManager() override = default;

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    void scanSearchPath(const QString& path);

private:
    mutable std::mutex m_mutex;
    QStringList m_searchPaths;
    std::unordered_map<std::string, TextureEntry> m_textures;
    mutable std::unordered_map<std::string, QString> m_resolvedCache;
};

} // namespace TSA::Viewer
