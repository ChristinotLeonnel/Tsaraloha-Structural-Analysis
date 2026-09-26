#include "ProjectManager.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridDefinition.h"
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
    QString name = currentFileName();
    if (m_isModified)
    {
        name += " *";
    }
    return QString("TSA - %1").arg(name);
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

    m_currentFilePath.clear();
    m_projectName.clear();
    m_author.clear();

    model.setModified(false);
    setModified(false);

    emit projectReset();
    emit projectTitleChanged(windowTitle());
}

bool ProjectManager::openProject(const QString& filePath,
                                 TSA::Model::Model& model,
                                 TSA::Grid::GridManager* gridManager,
                                 QString* outError)
{
    std::string err;
    bool ok = TSA::IO::TSAProjectIO::loadFromFile(filePath, model, gridManager, &err);
    if (!ok)
    {
        if (outError)
        {
            *outError = QString::fromStdString(err);
        }
        return false;
    }

    m_currentFilePath = filePath;
    model.setModified(false);
    setModified(false);

    emit projectOpened(filePath);
    emit projectTitleChanged(windowTitle());
    return true;
}

bool ProjectManager::saveProject(const QString& filePath,
                                 const TSA::Model::Model& model,
                                 const TSA::Grid::GridManager* gridManager,
                                 const QImage& thumbnail,
                                 QString* outError)
{
    std::string err;
    bool ok = TSA::IO::TSAProjectIO::saveToFile(filePath, model, gridManager, thumbnail, &err);
    if (!ok)
    {
        if (outError)
        {
            *outError = QString::fromStdString(err);
        }
        return false;
    }

    m_currentFilePath = filePath;
    const_cast<TSA::Model::Model&>(model).setModified(false);
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
