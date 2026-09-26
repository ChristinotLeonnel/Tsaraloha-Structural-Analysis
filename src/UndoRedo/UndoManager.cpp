#include "UndoManager.h"
#include "../Model/ModelDiff.h"

namespace TSA::UndoRedo
{

UndoManager::UndoManager(size_t maxSteps)
    : m_maxSteps(maxSteps)
{
}

void UndoManager::pushState(TSA::Model::Model& model, const std::string& actionName)
{
    m_undoStack.push_back(model.createSnapshot(actionName));
    if (m_undoStack.size() > m_maxSteps)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
    model.setModified(true);
}

bool UndoManager::undo(TSA::Model::Model& model)
{
    if (m_undoStack.empty())
        return false;

    TSA::Model::Model::ModelStateSnapshot currentSnap = model.createSnapshot(m_undoStack.back().actionName);
    m_redoStack.push_back(currentSnap);

    TSA::Model::Model::ModelStateSnapshot target = m_undoStack.back();
    m_undoStack.pop_back();

    // 1. Calculer le différentiel précis avant modification
    TSA::Model::ModelDiff diff = TSA::Model::ModelDiff::compute(currentSnap, target);

    // 2. Mettre à jour l'état logique des données du modèle
    model.applySnapshotData(target);

    // 3. Notifier différentiellement les observateurs (mise à jour ciblée du viewport OCCT et de l'arbre)
    model.notifyModelDiffApplied(diff);
    return true;
}

bool UndoManager::redo(TSA::Model::Model& model)
{
    if (m_redoStack.empty())
        return false;

    TSA::Model::Model::ModelStateSnapshot currentSnap = model.createSnapshot(m_redoStack.back().actionName);
    m_undoStack.push_back(currentSnap);

    TSA::Model::Model::ModelStateSnapshot target = m_redoStack.back();
    m_redoStack.pop_back();

    // 1. Calculer le différentiel précis
    TSA::Model::ModelDiff diff = TSA::Model::ModelDiff::compute(currentSnap, target);

    // 2. Mettre à jour l'état logique
    model.applySnapshotData(target);

    // 3. Notification différentielle
    model.notifyModelDiffApplied(diff);
    return true;
}

void UndoManager::clear() noexcept
{
    m_undoStack.clear();
    m_redoStack.clear();
}

std::string UndoManager::lastUndoActionName() const
{
    return m_undoStack.empty() ? "" : m_undoStack.back().actionName;
}

std::string UndoManager::lastRedoActionName() const
{
    return m_redoStack.empty() ? "" : m_redoStack.back().actionName;
}

} // namespace TSA::UndoRedo
