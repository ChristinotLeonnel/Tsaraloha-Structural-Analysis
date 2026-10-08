#include "ProjectSession.h"

#include "ProjectManager.h"
#include "../Analysis/AnalysisController.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridSnapManager.h"
#include "../Model/Model.h"
#include "../UndoRedo/CommandManager.h"

namespace TSA::Project
{

ProjectSession::ProjectSession(QObject* owner)
    : m_model(std::make_unique<TSA::Model::Model>())
    , m_commands(std::make_unique<TSA::UndoRedo::CommandManager>(m_model.get(), m_model->undoManager()))
    , m_grids(std::make_unique<TSA::Grid::GridManager>())
    , m_gridSnap(std::make_unique<TSA::Grid::GridSnapManager>())
    , m_project(std::make_unique<ProjectManager>(owner))
    , m_analysis(std::make_unique<TSA::Analysis::AnalysisController>(m_model.get(), m_grids.get()))
{
}

ProjectSession::~ProjectSession() = default;

bool ProjectSession::canUndo() const
{
    return m_commands->canUndo() || m_model->canUndo();
}

bool ProjectSession::canRedo() const
{
    return m_commands->canRedo() || m_model->canRedo();
}

bool ProjectSession::undo(std::string* actionName)
{
    if (actionName) *actionName = m_model->lastUndoActionName();
    if (m_commands->canUndo()) return m_commands->undo();
    return m_model->canUndo() && m_model->undo();
}

bool ProjectSession::redo(std::string* actionName)
{
    if (actionName) *actionName = m_model->lastRedoActionName();
    if (m_commands->canRedo()) return m_commands->redo();
    return m_model->canRedo() && m_model->redo();
}

bool ProjectSession::hasUnsavedChanges() const
{
    return m_model->isModified() || m_model->canUndo();
}

} // namespace TSA::Project
