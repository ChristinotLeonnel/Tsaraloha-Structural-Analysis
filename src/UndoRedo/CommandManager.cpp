#include "CommandManager.h"
#include "UndoManager.h"
#include "EditTransaction.h"
#include "../Model/Model.h"
#include "../Diagnostics/Logger.h"

namespace TSA::UndoRedo
{

CommandManager::CommandManager(TSA::Model::Model* model, UndoManager* undoManager)
    : m_model(model)
    , m_undoManager(undoManager)
{
}

bool CommandManager::executeCommand(std::unique_ptr<TSA::Commands::ICommand> command)
{
    if (!command)
        return false;

    std::string cmdName = command->name();
    TSA::Diagnostics::Logger::instance().setLastCommand(cmdName);
    TSA_LOG_INFO("Command", "CommandStarted", "Exécution de la commande : " + cmdName);

    if (m_undoManager && m_model)
    {
        // Transaction : une seule entrée Undo, créée uniquement si la commande réussit.
        // Auparavant le snapshot était empilé AVANT execute() : un échec laissait une entrée
        // Undo vide, et un échec partiel laissait le modèle à moitié modifié.
        EditTransaction tx(*m_model, cmdName);
        bool success = false;
        try
        {
            success = command->execute();
        }
        catch (const std::exception& e)
        {
            TSA_LOG_ERROR("Command", "CommandException", cmdName + " : " + e.what());
        }
        catch (...)
        {
            TSA_LOG_ERROR("Command", "CommandException", cmdName + " : exception non standard");
        }

        if (!success)
        {
            tx.rollback();
            TSA_LOG_ERROR("Command", "CommandFailed", "Échec de la commande (modèle restauré) : " + cmdName);
            return false;
        }
        tx.commit();
        TSA_LOG_INFO("Command", "CommandCompleted", "Commande exécutée avec succès : " + cmdName);
        // L'historique par snapshots fait foi : l'objet commande n'est plus conservé (la pile
        // m_undoCommands grossissait sans limite sans jamais servir dans ce mode).
        return true;
    }

    // Mode sans UndoManager : historique par commandes (execute / undo).
    bool success = command->execute();
    if (success)
    {
        TSA_LOG_INFO("Command", "CommandCompleted", "Commande exécutée avec succès : " + cmdName);
        m_undoCommands.push_back(std::move(command));
        m_redoCommands.clear();
    }
    else
    {
        TSA_LOG_ERROR("Command", "CommandFailed", "Échec de l'exécution de la commande : " + cmdName);
    }
    return success;
}

bool CommandManager::canUndo() const
{
    if (m_undoManager)
    {
        return m_undoManager->canUndo();
    }
    return !m_undoCommands.empty();
}

bool CommandManager::canRedo() const
{
    if (m_undoManager)
    {
        return m_undoManager->canRedo();
    }
    return !m_redoCommands.empty();
}

bool CommandManager::undo()
{
    TSA_LOG_INFO("Command", "CommandUndo", "Annulation de commande demandée");
    if (m_undoManager && m_model)
    {
        return m_undoManager->undo(*m_model);
    }
    if (m_undoCommands.empty())
        return false;

    auto cmd = std::move(m_undoCommands.back());
    m_undoCommands.pop_back();

    bool ok = cmd->undo();
    if (ok)
    {
        m_redoCommands.push_back(std::move(cmd));
    }
    return ok;
}

bool CommandManager::redo()
{
    TSA_LOG_INFO("Command", "CommandRedo", "Rétablissement de commande demandé");
    if (m_undoManager && m_model)
    {
        return m_undoManager->redo(*m_model);
    }
    if (m_redoCommands.empty())
        return false;

    auto cmd = std::move(m_redoCommands.back());
    m_redoCommands.pop_back();

    bool ok = cmd->execute();
    if (ok)
    {
        m_undoCommands.push_back(std::move(cmd));
    }
    return ok;
}

void CommandManager::clear()
{
    if (m_undoManager)
    {
        m_undoManager->clear();
    }
    m_undoCommands.clear();
    m_redoCommands.clear();
}

} // namespace TSA::UndoRedo
