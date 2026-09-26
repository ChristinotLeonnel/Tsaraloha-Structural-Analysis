#pragma once

#include "ICommand.h"
#include "../Grid/GridManager.h"
#include "../Grid/GridDefinition.h"
#include <string>

namespace TSA::Commands
{

class CreateGridCommand : public ICommand
{
public:
    CreateGridCommand(TSA::Grid::GridManager& manager, const TSA::Grid::GridDefinition& definition);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Créer grille: " + m_definition.name(); }
    const std::string& createdGridId() const { return m_createdId; }

private:
    TSA::Grid::GridManager& m_manager;
    TSA::Grid::GridDefinition m_definition;
    std::string m_createdId;
};

class ModifyGridCommand : public ICommand
{
public:
    ModifyGridCommand(TSA::Grid::GridManager& manager, const std::string& gridId, const TSA::Grid::GridDefinition& newDefinition);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Modifier grille: " + m_newDefinition.name(); }

private:
    TSA::Grid::GridManager& m_manager;
    std::string m_gridId;
    TSA::Grid::GridDefinition m_newDefinition;
    TSA::Grid::GridDefinition m_oldDefinition;
};

class DeleteGridCommand : public ICommand
{
public:
    DeleteGridCommand(TSA::Grid::GridManager& manager, const std::string& gridId);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Supprimer grille: " + m_oldDefinition.name(); }

private:
    TSA::Grid::GridManager& m_manager;
    std::string m_gridId;
    TSA::Grid::GridDefinition m_oldDefinition;
    bool m_wasActive = false;
};

class DuplicateGridCommand : public ICommand
{
public:
    DuplicateGridCommand(TSA::Grid::GridManager& manager, const std::string& sourceGridId);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Dupliquer grille"; }
    const std::string& duplicatedGridId() const { return m_duplicatedId; }

private:
    TSA::Grid::GridManager& m_manager;
    std::string m_sourceId;
    std::string m_duplicatedId;
};

class SetActiveGridCommand : public ICommand
{
public:
    SetActiveGridCommand(TSA::Grid::GridManager& manager, const std::string& newActiveId);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Activer grille"; }

private:
    TSA::Grid::GridManager& m_manager;
    std::string m_newActiveId;
    std::string m_oldActiveId;
};

class SetGridVisibilityCommand : public ICommand
{
public:
    SetGridVisibilityCommand(TSA::Grid::GridManager& manager, const std::string& gridId, bool visible);
    bool execute() override;
    bool undo() override;
    std::string name() const override { return m_visible ? "Afficher grille" : "Masquer grille"; }

private:
    TSA::Grid::GridManager& m_manager;
    std::string m_gridId;
    bool m_visible;
    bool m_oldVisible = true;
};

} // namespace TSA::Commands
