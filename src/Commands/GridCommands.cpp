#include "GridCommands.h"

namespace TSA::Commands
{

// --- CreateGridCommand ---
CreateGridCommand::CreateGridCommand(TSA::Grid::GridManager& manager, const TSA::Grid::GridDefinition& definition)
    : m_manager(manager), m_definition(definition)
{
}

bool CreateGridCommand::execute()
{
    TSA::Grid::GridSystem* grid = m_manager.addGrid(m_definition);
    if (grid)
    {
        m_createdId = grid->id();
        return true;
    }
    return false;
}

bool CreateGridCommand::undo()
{
    if (m_createdId.empty())
        return false;
    return m_manager.removeGrid(m_createdId);
}


// --- ModifyGridCommand ---
ModifyGridCommand::ModifyGridCommand(TSA::Grid::GridManager& manager, const std::string& gridId, const TSA::Grid::GridDefinition& newDefinition)
    : m_manager(manager), m_gridId(gridId), m_newDefinition(newDefinition)
{
    TSA::Grid::GridSystem* grid = m_manager.getGrid(gridId);
    if (grid)
    {
        m_oldDefinition = grid->definition();
    }
}

bool ModifyGridCommand::execute()
{
    return m_manager.updateGrid(m_gridId, m_newDefinition);
}

bool ModifyGridCommand::undo()
{
    return m_manager.updateGrid(m_gridId, m_oldDefinition);
}


// --- DeleteGridCommand ---
DeleteGridCommand::DeleteGridCommand(TSA::Grid::GridManager& manager, const std::string& gridId)
    : m_manager(manager), m_gridId(gridId)
{
    TSA::Grid::GridSystem* grid = m_manager.getGrid(gridId);
    if (grid)
    {
        m_oldDefinition = grid->definition();
        m_wasActive = (m_manager.activeGridId() == gridId);
    }
}

bool DeleteGridCommand::execute()
{
    return m_manager.removeGrid(m_gridId);
}

bool DeleteGridCommand::undo()
{
    TSA::Grid::GridSystem* grid = m_manager.addGrid(m_oldDefinition);
    if (grid)
    {
        if (m_wasActive)
        {
            m_manager.setActiveGridId(grid->id());
        }
        return true;
    }
    return false;
}


// --- DuplicateGridCommand ---
DuplicateGridCommand::DuplicateGridCommand(TSA::Grid::GridManager& manager, const std::string& sourceGridId)
    : m_manager(manager), m_sourceId(sourceGridId)
{
}

bool DuplicateGridCommand::execute()
{
    TSA::Grid::GridSystem* dup = m_manager.duplicateGrid(m_sourceId);
    if (dup)
    {
        m_duplicatedId = dup->id();
        return true;
    }
    return false;
}

bool DuplicateGridCommand::undo()
{
    if (m_duplicatedId.empty())
        return false;
    return m_manager.removeGrid(m_duplicatedId);
}


// --- SetActiveGridCommand ---
SetActiveGridCommand::SetActiveGridCommand(TSA::Grid::GridManager& manager, const std::string& newActiveId)
    : m_manager(manager), m_newActiveId(newActiveId)
{
    m_oldActiveId = m_manager.activeGridId();
}

bool SetActiveGridCommand::execute()
{
    m_manager.setActiveGridId(m_newActiveId);
    return true;
}

bool SetActiveGridCommand::undo()
{
    m_manager.setActiveGridId(m_oldActiveId);
    return true;
}


// --- SetGridVisibilityCommand ---
SetGridVisibilityCommand::SetGridVisibilityCommand(TSA::Grid::GridManager& manager, const std::string& gridId, bool visible)
    : m_manager(manager), m_gridId(gridId), m_visible(visible)
{
    TSA::Grid::GridSystem* grid = m_manager.getGrid(gridId);
    if (grid)
    {
        m_oldVisible = grid->isVisible();
    }
}

bool SetGridVisibilityCommand::execute()
{
    m_manager.setGridVisible(m_gridId, m_visible);
    return true;
}

bool SetGridVisibilityCommand::undo()
{
    m_manager.setGridVisible(m_gridId, m_oldVisible);
    return true;
}

} // namespace TSA::Commands
