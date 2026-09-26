#pragma once

#include <QObject>
#include <QString>
#include <QImage>

namespace TSA::Model
{
class Model;
}

namespace TSA::Grid
{
class GridManager;
}

namespace TSA::Project
{

/**
 * @brief Gestionnaire de cycle de vie de projet (Nouveau, Ouvrir, Enregistrer, État modifié).
 * Découplé de l'affichage graphique lourd pour respecter la séparation des responsabilités.
 */
class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);
    ~ProjectManager() override = default;

    // État du fichier
    bool hasFilePath() const noexcept { return !m_currentFilePath.isEmpty(); }
    const QString& currentFilePath() const noexcept { return m_currentFilePath; }
    QString currentFileName() const;
    QString windowTitle() const;

    bool isModified() const noexcept { return m_isModified; }
    void setModified(bool modified);

    // Métadonnées projet
    const QString& projectName() const noexcept { return m_projectName; }
    void setProjectName(const QString& name) { m_projectName = name; }
    const QString& author() const noexcept { return m_author; }
    void setAuthor(const QString& author) { m_author = author; }

    // Opérations Fichier
    void newProject(TSA::Model::Model& model, TSA::Grid::GridManager* gridManager);
    bool openProject(const QString& filePath,
                     TSA::Model::Model& model,
                     TSA::Grid::GridManager* gridManager,
                     QString* outError = nullptr);
    bool saveProject(const QString& filePath,
                     const TSA::Model::Model& model,
                     const TSA::Grid::GridManager* gridManager,
                     const QImage& thumbnail = QImage(),
                     QString* outError = nullptr);
    bool saveCurrentProject(const TSA::Model::Model& model,
                            const TSA::Grid::GridManager* gridManager,
                            const QImage& thumbnail = QImage(),
                            QString* outError = nullptr);

signals:
    void projectReset();
    void projectOpened(const QString& filePath);
    void projectSaved(const QString& filePath);
    void modifiedChanged(bool isModified);
    void projectTitleChanged(const QString& title);

private:
    QString m_currentFilePath;
    QString m_projectName;
    QString m_author;
    bool m_isModified = false;
};

} // namespace TSA::Project
