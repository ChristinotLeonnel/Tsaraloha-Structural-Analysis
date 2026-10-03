#include "UndoManager.h"
#include "../Model/ModelDiff.h"

#include <chrono>
#include <ctime>

namespace TSA::UndoRedo
{

UndoManager::UndoManager(size_t maxSteps)
    : m_maxSteps(maxSteps)
{
}

std::string UndoManager::nowTimestamp()
{
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

void UndoManager::pushEntry(HistoryEntry&& entry, TSA::Model::Model& model)
{
    m_undoStack.push_back(std::move(entry));
    while (m_undoStack.size() > m_maxSteps)
    {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
    model.setModified(true);
}

void UndoManager::pushState(TSA::Model::Model& model, const std::string& actionName, const std::string& coalesceKey)
{
    if (m_transactionDepth > 0)
    {
        // L'état initial est déjà capturé par la transaction ouverte : une seule entrée Undo.
        model.setModified(true);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!coalesceKey.empty() && !m_undoStack.empty() && m_redoStack.empty())
    {
        HistoryEntry& top = m_undoStack.back();
        if (top.coalesceKey == coalesceKey && now - top.lastPush < COALESCE_WINDOW)
        {
            // Même édition en cours : l'état « avant » de l'entrée existante reste la référence.
            top.lastPush = now;
            model.setModified(true);
            return;
        }
    }

    HistoryEntry entry;
    entry.snapshot = model.createSnapshot(actionName);
    entry.timestamp = nowTimestamp();
    entry.coalesceKey = coalesceKey;
    entry.lastPush = now;
    pushEntry(std::move(entry), model);
}

void UndoManager::restore(TSA::Model::Model& model,
                          const TSA::Model::Model::ModelStateSnapshot& current,
                          const TSA::Model::Model::ModelStateSnapshot& target)
{
    // 1. Différentiel précis avant modification, 2. état logique, 3. notification ciblée
    //    (mise à jour locale du viewport OCCT et de l'arbre, sans reconstruction complète).
    TSA::Model::ModelDiff diff = TSA::Model::ModelDiff::compute(current, target);
    model.applySnapshotData(target);
    model.notifyModelDiffApplied(diff);
}

bool UndoManager::undo(TSA::Model::Model& model)
{
    if (m_undoStack.empty() || m_transactionDepth > 0)
        return false;

    HistoryEntry target = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    HistoryEntry redoEntry;
    redoEntry.snapshot = model.createSnapshot(target.snapshot.actionName);
    redoEntry.timestamp = target.timestamp;
    redoEntry.records = target.records;

    restore(model, redoEntry.snapshot, target.snapshot);
    m_redoStack.push_back(std::move(redoEntry));
    return true;
}

bool UndoManager::redo(TSA::Model::Model& model)
{
    if (m_redoStack.empty() || m_transactionDepth > 0)
        return false;

    HistoryEntry target = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    HistoryEntry undoEntry;
    undoEntry.snapshot = model.createSnapshot(target.snapshot.actionName);
    undoEntry.timestamp = target.timestamp;
    undoEntry.records = target.records;

    restore(model, undoEntry.snapshot, target.snapshot);
    m_undoStack.push_back(std::move(undoEntry));
    return true;
}

void UndoManager::clear() noexcept
{
    m_undoStack.clear();
    m_redoStack.clear();
    m_transactionDepth = 0;
    m_transaction = HistoryEntry{};
}

std::string UndoManager::lastUndoActionName() const
{
    return m_undoStack.empty() ? "" : m_undoStack.back().snapshot.actionName;
}

std::string UndoManager::lastRedoActionName() const
{
    return m_redoStack.empty() ? "" : m_redoStack.back().snapshot.actionName;
}

void UndoManager::beginTransaction(TSA::Model::Model& model, const std::string& actionName)
{
    if (m_transactionDepth++ > 0)
        return; // transaction imbriquée : absorbée par la transaction englobante

    m_transaction = HistoryEntry{};
    m_transaction.snapshot = model.createSnapshot(actionName);
    m_transaction.timestamp = nowTimestamp();
}

void UndoManager::commitTransaction(TSA::Model::Model& model)
{
    if (m_transactionDepth == 0)
        return;
    if (--m_transactionDepth > 0)
        return;

    pushEntry(std::move(m_transaction), model);
    m_transaction = HistoryEntry{};
}

void UndoManager::rollbackTransaction(TSA::Model::Model& model)
{
    if (m_transactionDepth == 0)
        return;

    // Un rollback annule toute la transaction englobante : un état partiellement appliqué
    // ne doit jamais subsister.
    m_transactionDepth = 0;
    const auto current = model.createSnapshot(m_transaction.snapshot.actionName);
    restore(model, current, m_transaction.snapshot);
    m_transaction = HistoryEntry{};
}

void UndoManager::addRecord(const EditRecord& record)
{
    if (m_transactionDepth > 0)
    {
        m_transaction.records.push_back(record);
    }
    else if (!m_undoStack.empty())
    {
        m_undoStack.back().records.push_back(record);
    }
}

std::vector<HistoryItem> UndoManager::undoHistory() const
{
    std::vector<HistoryItem> items;
    items.reserve(m_undoStack.size());
    for (const auto& e : m_undoStack)
    {
        items.push_back(HistoryItem{ e.snapshot.actionName, e.timestamp, e.records });
    }
    return items;
}

} // namespace TSA::UndoRedo
