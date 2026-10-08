#include "ProjectSession.h"

#include "ProjectManager.h"
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
{
}

ProjectSession::~ProjectSession() = default;

} // namespace TSA::Project
