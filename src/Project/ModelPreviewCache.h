#pragma once

// Cache des aperçus « dernier état du modèle » : une image basse résolution (480×270) capturée
// dans le viewport réel (OccView::captureViewImage) et ses métadonnées (caméra, état d'affichage,
// révision du modèle). Stockage : <AppData>/previews/<id>.png + <id>.json, id = SHA-1 du chemin.
// Repli : la miniature embarquée dans le .tsa (chunk THMB), écrite à chaque enregistrement.
// Le cache ne contient jamais le modèle lui-même : l'IA travaille sur les données TSA réelles.

#include <QDateTime>
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace TSA::Project
{

struct ProjectPreviewMetadata
{
    static constexpr int kPreviewVersion = 1;

    QString projectId;        // SHA-1 du chemin normalisé
    QString projectPath;
    QString previewPath;
    QString sourceFormat = QStringLiteral("TSA"); // futur : IFC, DXF… (le modèle structural reste l'aperçu)
    QDateTime fileLastModified; // date du fichier au moment de la capture
    QDateTime capturedAt;
    quint64 modelRevision = 0;
    QJsonObject cameraState;  // eye, center, up, scale, projection
    QJsonObject viewState;    // mode 2D, plan de travail, calques visibles…
    int previewVersion = kPreviewVersion;
    int nodeCount = 0;
    int elementCount = 0;

    QJsonObject toJson() const;
    static ProjectPreviewMetadata fromJson(const QJsonObject& o);
};

enum class PreviewSource { None, Cache, Embedded };

class ModelPreviewCache
{
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 270;

    /// Dossier du cache (modifiable pour les tests ; vide = <AppData>/previews).
    explicit ModelPreviewCache(const QString& directory = QString());

    QString directory() const { return m_dir; }
    static QString projectId(const QString& projectPath);

    std::optional<ProjectPreviewMetadata> metadata(const QString& projectPath) const;

    /// Aperçu à afficher : cache s'il est au moins aussi récent que le fichier, sinon miniature
    /// embarquée du .tsa (fichier modifié ailleurs ou jamais capturé ici). Lecture disque : peut
    /// être appelée hors du thread UI.
    QImage preview(const QString& projectPath, PreviewSource* source = nullptr) const;

    /// Écrit image + métadonnées (écriture atomique). Appelable hors du thread UI.
    bool store(const QImage& image, ProjectPreviewMetadata meta) const;
    void remove(const QString& projectPath) const;
    void rename(const QString& oldPath, const QString& newPath) const;

    /// Faut-il recapturer ? (révision ou caméra différente, ou rien en cache)
    static bool needsCapture(const std::optional<ProjectPreviewMetadata>& last, quint64 revision,
                             const QJsonObject& cameraState);

private:
    QString imagePath(const QString& id) const { return m_dir + "/" + id + ".png"; }
    QString metaPath(const QString& id) const { return m_dir + "/" + id + ".json"; }

    QString m_dir;
};

} // namespace TSA::Project
