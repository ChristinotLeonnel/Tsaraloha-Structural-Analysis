#include "ProjectManager.h"
#include "App/ProductInfo.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridDefinition.h"
#include "../Grid/GridCoordinateSync.h"
#include "../Grid/GridSystem.h"
#include "../Coordinate/CoordinateSystem.h"
#include "../IO/TSAFile.h"
#include <QFileInfo>

namespace TSA::Project
{

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

QString ProjectManager::currentFileName() const
{
    if (m_currentFilePath.isEmpty())
    {
        return tr("Sans titre");
    }
    return QFileInfo(m_currentFilePath).fileName();
}

QString ProjectManager::windowTitle() const
{
    // Projet sans fichier mais nommé (import IFC…) : son nom, comme l'onglet et l'arbre (BUG-077)
    QString name = !hasFilePath() && !m_projectName.isEmpty() ? m_projectName : currentFileName();
    if (m_isModified)
    {
        name += " *";
    }
    return QString("%1 - %2").arg(TSA::Product::name(), name);
}

void ProjectManager::adoptFilePath(const QString& path)
{
    m_currentFilePath = path;
    emit projectTitleChanged(windowTitle());
}

void ProjectManager::setModified(bool modified)
{
    if (m_isModified != modified)
    {
        m_isModified = modified;
        emit modifiedChanged(m_isModified);
        emit projectTitleChanged(windowTitle());
    }
}

void ProjectManager::newProject(TSA::Model::Model& model, TSA::Grid::GridManager* gridManager)
{
    model.clear();
    model.clearUndoRedo();

    if (gridManager)
    {
        gridManager->clearAllGrids();
        TSA::Grid::GridDefinition def("Grille Bâtiment", TSA::Grid::GridType::Cartesian);
        def.setOrigin(0.0, 0.0, 0.0);
        auto* defaultGrid = gridManager->addGrid(def);
        if (defaultGrid)
        {
            gridManager->setActiveGridId(defaultGrid->id());
        }
    }
    // Niveaux et repère du nouveau projet : ceux de sa grille (Model::clear ne touche pas au repère, qui gardait les
    // niveaux du projet précédent).
    const TSA::Grid::GridSystem* active = gridManager ? gridManager->activeGrid() : nullptr;
    if (auto* lm = model.levelManager())
    {
        if (active) TSA::Grid::applyGridLevels(active->definition(), *lm);
        else lm->clear();
    }
    if (auto* cs = model.coordinateSystem())
        TSA::Grid::syncCoordinateSystem(active ? &active->definition() : nullptr, *cs);

    m_currentFilePath.clear();
    m_projectName.clear();
    m_author.clear();
    setPassword(QString());

    model.markSaved();   // point d'enregistrement de l'historique (BUG-074)
    setModified(false);

    emit projectReset();
    emit projectTitleChanged(windowTitle());
}

void ProjectManager::setPassword(const QString& password)
{
    if (password == m_password) return;
    const bool was = hasPassword();
    m_password = password;
    if (was != hasPassword()) emit protectionChanged(hasPassword());
}

bool ProjectManager::openProject(const QString& filePath,
                                 TSA::Model::Model& model,
                                 TSA::Grid::GridManager* gridManager,
                                 QString* outError)
{
    return openProject(filePath, model, gridManager, QString(), outError) == TSA::IO::TSAProjectIO::OpenStatus::Ok;
}

TSA::IO::TSAProjectIO::OpenStatus ProjectManager::openProject(const QString& filePath,
                                                              TSA::Model::Model& model,
                                                              TSA::Grid::GridManager* gridManager,
                                                              const QString& password,
                                                              QString* outError)
{
    QString qErr;
    QImage thumbnail;
    QString name, author;
    const auto status = TSA::IO::TSAProjectIO::loadProtectedProject(filePath, model, gridManager, password, &name, &author, &thumbnail, &qErr);
    if (status != TSA::IO::TSAProjectIO::OpenStatus::Ok)
    {
        if (outError)
        {
            *outError = qErr;
        }
        return status;
    }
    m_projectName = name;
    m_author = author;
    // Mot de passe retenu seulement pour un projet protégé (un mot de passe donné pour un projet en clair est sans objet)
    setPassword(TSA::IO::TSAProjectIO::isPasswordProtected(filePath) ? password : QString());

    // Repère reconstruit depuis la grille active ; niveaux fixes des anciennes versions remplacés par ceux de la grille
    TSA::Grid::reconcileProject(model, gridManager);

    if (m_projectName.trimmed().isEmpty())
    {
        m_projectName = QFileInfo(filePath).baseName();
    }

    m_currentFilePath = filePath;
    model.markSaved();   // point d'enregistrement de l'historique (BUG-074)
    setModified(false);

    emit projectOpened(filePath);
    emit projectTitleChanged(windowTitle());
    return TSA::IO::TSAProjectIO::OpenStatus::Ok;
}

bool ProjectManager::saveProject(const QString& filePath,
                                 const TSA::Model::Model& model,
                                 const TSA::Grid::GridManager* gridManager,
                                 const QImage& thumbnail,
                                 QString* outError)
{
    // Nom et auteur du projet réécrits dans le chunk PROJ : auparavant une chaîne vide était
    // enregistrée, et le nom saisi dans « Nouveau projet » était perdu à la réouverture.
    QString err;
    // Projet protégé : fichier chiffré (BUG-167) ; sinon en clair, comme avant
    bool ok = TSA::IO::TSAProjectIO::saveProtectedProject(filePath, model, gridManager, m_projectName, m_author, m_password,
                                                          thumbnail, &err);
    if (!ok)
    {
        if (outError)
        {
            *outError = err;
        }
        return false;
    }

    m_currentFilePath = filePath;
    const_cast<TSA::Model::Model&>(model).markSaved();   // point d'enregistrement (BUG-074)
    setModified(false);

    emit projectSaved(filePath);
    emit projectTitleChanged(windowTitle());
    return true;
}

bool ProjectManager::saveCurrentProject(const TSA::Model::Model& model,
                                        const TSA::Grid::GridManager* gridManager,
                                        const QImage& thumbnail,
                                        QString* outError)
{
    if (m_currentFilePath.isEmpty())
    {
        if (outError)
        {
            *outError = tr("Aucun chemin de fichier défini pour l'enregistrement.");
        }
        return false;
    }
    return saveProject(m_currentFilePath, model, gridManager, thumbnail, outError);
}

} // namespace TSA::Project
