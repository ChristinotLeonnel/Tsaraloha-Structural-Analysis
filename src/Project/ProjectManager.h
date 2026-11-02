#pragma once

#include "../IO/TSAFile.h"
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
    /// Projet récupéré d'une copie de récupération (BUG-157) : rattaché à son fichier d'origine (vide : jamais enregistré).
    void adoptFilePath(const QString& path);

    // Métadonnées projet
    const QString& projectName() const noexcept { return m_projectName; }
    void setProjectName(const QString& name) { m_projectName = name; }
    const QString& author() const noexcept { return m_author; }
    void setAuthor(const QString& author) { m_author = author; }

    // Protection par mot de passe (BUG-167) : retenue en mémoire pour la session, appliquée à chaque enregistrement
    // (fichier et copie de récupération) ; vide = projet non protégé. Changer la protection ne touche pas au modèle :
    // l'appelant marque le projet « à enregistrer ».
    bool hasPassword() const noexcept { return !m_password.isEmpty(); }
    const QString& password() const noexcept { return m_password; }
    void setPassword(const QString& password);

    // Opérations Fichier
    void newProject(TSA::Model::Model& model, TSA::Grid::GridManager* gridManager);
    bool openProject(const QString& filePath,
                     TSA::Model::Model& model,
                     TSA::Grid::GridManager* gridManager,
                     QString* outError = nullptr);
    /// Ouvre un projet, protégé ou non ; le mot de passe accepté est retenu (projet réenregistré protégé).
    TSA::IO::TSAProjectIO::OpenStatus openProject(const QString& filePath,
                                                  TSA::Model::Model& model,
                                                  TSA::Grid::GridManager* gridManager,
                                                  const QString& password,
                                                  QString* outError);
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
    void protectionChanged(bool protectedByPassword);

private:
    QString m_currentFilePath;
    QString m_projectName;
    QString m_author;
    QString m_password;
    bool m_isModified = false;
};

} // namespace TSA::Project
