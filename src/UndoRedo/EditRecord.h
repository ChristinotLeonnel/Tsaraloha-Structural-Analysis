#pragma once

#include <string>
#include <vector>

namespace TSA::UndoRedo
{

/**
 * @brief Description structurée d'une modification élémentaire du modèle.
 *
 * Attachée à une entrée d'historique Undo/Redo. Répond aux questions QUI / QUOI / AVANT /
 * APRÈS / IMPACT afin qu'un historique lisible, un journal ou un assistant (co-engineering)
 * puissent comprendre une action sans relire le modèle. Les valeurs sont des textes déjà
 * formatés (unités comprises) : l'enregistrement décrit, il ne sert pas à rejouer l'action
 * (la restauration repose sur les snapshots de l'UndoManager).
 */
struct EditRecord
{
    std::string action;      ///< ex. "modify_property", "move", "delete", "create"
    std::string objectType;  ///< ex. "Beam", "Column", "Node", "LoadCase"
    int objectId = -1;       ///< identifiant de l'objet, -1 si sans objet
    std::string property;    ///< ex. "section", "x", "thickness" (vide si sans objet)
    std::string oldValue;    ///< ex. "IPE 240"
    std::string newValue;    ///< ex. "IPE 300"
    std::vector<std::string> impacts; ///< ex. "geometry", "stiffness", "results_invalidated"

    /// Résumé lisible : « Beam 42 · section : IPE 240 → IPE 300 ».
    std::string toText() const
    {
        std::string s = objectType;
        if (objectId >= 0)
            s += " " + std::to_string(objectId);
        if (!property.empty())
            s += " · " + property;
        if (!oldValue.empty() || !newValue.empty())
            s += " : " + oldValue + " → " + newValue;
        else if (!action.empty())
            s += " (" + action + ")";
        return s;
    }
};

/// Entrée d'historique telle qu'exposée à l'interface (sans le snapshot sous-jacent).
struct HistoryItem
{
    std::string actionName;
    std::string timestamp; ///< ISO 8601 local, ex. "2026-10-03T19:42:10"
    std::vector<EditRecord> records;
};

} // namespace TSA::UndoRedo
