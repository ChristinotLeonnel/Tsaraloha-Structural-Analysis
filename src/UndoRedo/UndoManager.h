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
    /// @param contentChange false : changement d'étiquettes seulement (renumérotation) — le modèle n'est
    ///        pas marqué modifié ici (révision inchangée, résultats de calcul conservés) ; l'appelant
    ///        notifie par Model::notifyLabelsChanged.
    void pushState(TSA::Model::Model& model, const std::string& actionName = "",
                   const std::string& coalesceKey = "", bool contentChange = true);
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

    /// Budget mémoire de l'historique (snapshots complets) : au-delà, les entrées les plus
    /// anciennes sont libérées (la plus récente est toujours conservée). Mesure : ~3,2 Mio par
    /// snapshot pour 4 896 barres, soit ~160 Mio pour 50 niveaux ; sans budget, un modèle 10x
    /// plus grand dépasserait 1,5 Gio.
    void setMemoryBudgetBytes(size_t bytes) noexcept { m_memoryBudgetBytes = bytes; }
    size_t memoryBudgetBytes() const noexcept { return m_memoryBudgetBytes; }
    size_t approxMemoryBytes() const noexcept;
    static size_t estimateSnapshotBytes(const TSA::Model::Model::ModelStateSnapshot& snap) noexcept;

    // --- Transactions -------------------------------------------------------
    void beginTransaction(TSA::Model::Model& model, const std::string& actionName);
    void commitTransaction(TSA::Model::Model& model);
    void rollbackTransaction(TSA::Model::Model& model);
    /// Clôt la transaction SANS entrée ni restauration : réservé au cas où l'appelant a vérifié
    /// que rien n'a changé (un rollback notifierait un différentiel vide et invaliderait les résultats).
    void discardUnchangedTransaction() noexcept;
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
        size_t approxBytes = 0;
    };

    void pushEntry(HistoryEntry&& entry, TSA::Model::Model& model, bool contentChange = true);
    static void restore(TSA::Model::Model& model, const TSA::Model::Model::ModelStateSnapshot& current,
                        const TSA::Model::Model::ModelStateSnapshot& target);
    static std::string nowTimestamp();

    std::deque<HistoryEntry> m_undoStack;
    std::deque<HistoryEntry> m_redoStack;
    size_t m_maxSteps = 50;
    size_t m_memoryBudgetBytes = 512ull * 1024ull * 1024ull;

    int m_transactionDepth = 0;
    HistoryEntry m_transaction; ///< snapshot de début + enregistrements de la transaction ouverte
};

} // namespace TSA::UndoRedo
