#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../Model/Model.h"

namespace TSA::UndoRedo
{

/**
 * @brief Gestionnaire centralisé de l'historique d'annulation / rétablissement (Undo / Redo).
 * Extrait la gestion des piles d'annulation hors du modèle de données pur.
 */
class UndoManager
{
public:
    explicit UndoManager(size_t maxSteps = 50);
    ~UndoManager() = default;

    void pushState(TSA::Model::Model& model, const std::string& actionName = "");
    bool canUndo() const noexcept { return !m_undoStack.empty(); }
    bool canRedo() const noexcept { return !m_redoStack.empty(); }
    bool undo(TSA::Model::Model& model);
    bool redo(TSA::Model::Model& model);
    void clear() noexcept;

    std::string lastUndoActionName() const;
    std::string lastRedoActionName() const;

    size_t maxSteps() const noexcept { return m_maxSteps; }
    void setMaxSteps(size_t steps) { m_maxSteps = steps; }

private:
    std::vector<TSA::Model::Model::ModelStateSnapshot> m_undoStack;
    std::vector<TSA::Model::Model::ModelStateSnapshot> m_redoStack;
    size_t m_maxSteps = 50;
};

} // namespace TSA::UndoRedo
