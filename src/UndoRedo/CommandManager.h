#pragma once

#include <memory>
#include <vector>
#include <string>
#include "../Commands/ICommand.h"

namespace TSA::Model { class Model; }

namespace TSA::UndoRedo
{
class UndoManager;

/**
 * @brief Gestionnaire centralisé pour l'exécution des commandes (Command Pattern).
 */
class CommandManager
{
public:
    explicit CommandManager(TSA::Model::Model* model = nullptr, UndoManager* undoManager = nullptr);
    ~CommandManager() = default;

    void setModel(TSA::Model::Model* model) { m_model = model; }
    void setUndoManager(UndoManager* undoManager) { m_undoManager = undoManager; }

    /**
     * @brief Exécute une commande et capture l'état Undo.
     */
    bool executeCommand(std::unique_ptr<TSA::Commands::ICommand> command);

    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void clear();

private:
    TSA::Model::Model* m_model = nullptr;
    UndoManager* m_undoManager = nullptr;
    std::vector<std::unique_ptr<TSA::Commands::ICommand>> m_undoCommands;
    std::vector<std::unique_ptr<TSA::Commands::ICommand>> m_redoCommands;
};

} // namespace TSA::UndoRedo
