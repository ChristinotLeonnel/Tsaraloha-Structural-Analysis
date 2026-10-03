#pragma once

#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <memory>
#include "../Model/Model.h"
#include "EditRecord.h"

namespace TSA::UndoRedo
{

/**
 * @brief Historique Undo/Redo du modèle (snapshots) avec transactions et enregistrements
 * structurés.
 *
 * - pushState() : une entrée par action (comportement historique, conservé).
 * - Transactions (beginTransaction / commitTransaction / rollbackTransaction, via la classe
 *   RAII EditTransaction) : une opération composée produit UNE seule entrée ; les pushState()
 *   émis pendant la transaction (ex. par les fonctions appelées) sont absorbés ; un rollback
 *   restaure le modèle et notifie les vues par différentiel.
 * - Chaque entrée porte des EditRecord (objet, propriété, avant/après, impact).
 */
class UndoManager
{
public:
    explicit UndoManager(size_t maxSteps = 50);
    ~UndoManager() = default;

    /// @param coalesceKey si non vide, des appels successifs avec la même clé à moins de
    ///        COALESCE_WINDOW d'intervalle (et sans autre action entre eux) forment UNE seule entrée
    ///        (ex. crans successifs d'un spinbox de propriété sur le même objet).
    void pushState(TSA::Model::Model& model, const std::string& actionName = "",
                   const std::string& coalesceKey = "");
    static constexpr std::chrono::milliseconds COALESCE_WINDOW{ 2000 };

    bool canUndo() const noexcept { return !m_undoStack.empty() && m_transactionDepth == 0; }
    bool canRedo() const noexcept { return !m_redoStack.empty() && m_transactionDepth == 0; }

    bool undo(TSA::Model::Model& model);
    bool redo(TSA::Model::Model& model);

    void clear() noexcept;

    std::string lastUndoActionName() const;
    std::string lastRedoActionName() const;

    size_t maxSteps() const noexcept { return m_maxSteps; }
    void setMaxSteps(size_t steps) { m_maxSteps = steps; }

    // --- Transactions -------------------------------------------------------
    void beginTransaction(TSA::Model::Model& model, const std::string& actionName);
    void commitTransaction(TSA::Model::Model& model);
    void rollbackTransaction(TSA::Model::Model& model);
    bool inTransaction() const noexcept { return m_transactionDepth > 0; }

    // --- Historique structuré ------------------------------------------------
    /// Ajoute un enregistrement à la transaction en cours, sinon à la dernière entrée Undo.
    void addRecord(const EditRecord& record);
    /// Entrées annulables, de la plus ancienne à la plus récente.
    std::vector<HistoryItem> undoHistory() const;
    size_t undoCount() const noexcept { return m_undoStack.size(); }
    size_t redoCount() const noexcept { return m_redoStack.size(); }

private:
    struct HistoryEntry
    {
        TSA::Model::Model::ModelStateSnapshot snapshot; ///< état AVANT l'action
        std::string timestamp;
        std::vector<EditRecord> records;
        std::string coalesceKey;
        std::chrono::steady_clock::time_point lastPush{};
    };

    void pushEntry(HistoryEntry&& entry, TSA::Model::Model& model);
    static void restore(TSA::Model::Model& model, const TSA::Model::Model::ModelStateSnapshot& current,
                        const TSA::Model::Model::ModelStateSnapshot& target);
    static std::string nowTimestamp();

    std::deque<HistoryEntry> m_undoStack;
    std::deque<HistoryEntry> m_redoStack;
    size_t m_maxSteps = 50;

    int m_transactionDepth = 0;
    HistoryEntry m_transaction; ///< snapshot de début + enregistrements de la transaction ouverte
};

} // namespace TSA::UndoRedo
