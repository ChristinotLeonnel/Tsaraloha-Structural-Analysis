#pragma once

#include "UndoManager.h"
#include "EditRecord.h"
#include "../Model/Model.h"

#include <string>

namespace TSA::UndoRedo
{

/**
 * @brief Transaction d'édition RAII sur l'historique existant (UndoManager du modèle).
 *
 * @code
 * EditTransaction tx(model, "Déplacement de 20 poteaux");
 * for (...) { ... modifier ... }
 * if (!valid) { tx.rollback(); return; }   // modèle restauré, vues notifiées
 * tx.record({"move", "Column", id, "position", "…", "…", {"geometry"}});
 * tx.commit();                              // UNE seule entrée Undo
 * @endcode
 *
 * Sans commit(), le destructeur annule la transaction (exception, retour anticipé).
 * Les transactions imbriquées sont absorbées par la transaction englobante ; un rollback
 * annule toute la transaction englobante.
 */
class EditTransaction
{
public:
    EditTransaction(TSA::Model::Model& model, const std::string& actionName)
        : m_model(model)
        , m_undo(model.undoManager())
    {
        if (m_undo)
        {
            m_undo->beginTransaction(model, actionName);
        }
    }

    ~EditTransaction()
    {
        if (!m_done)
        {
            try
            {
                rollback();
            }
            catch (...)
            {
                // Un destructeur ne doit pas propager d'exception.
            }
        }
    }

    EditTransaction(const EditTransaction&) = delete;
    EditTransaction& operator=(const EditTransaction&) = delete;

    void record(const EditRecord& rec)
    {
        if (m_undo)
        {
            m_undo->addRecord(rec);
        }
    }

    void commit()
    {
        if (m_done) return;
        m_done = true;
        if (m_undo)
        {
            m_undo->commitTransaction(m_model);
        }
        else
        {
            m_model.setModified(true);
        }
    }

    void rollback()
    {
        if (m_done) return;
        m_done = true;
        if (m_undo)
        {
            m_undo->rollbackTransaction(m_model);
        }
    }

private:
    TSA::Model::Model& m_model;
    UndoManager* m_undo = nullptr;
    bool m_done = false;
};

} // namespace TSA::UndoRedo
