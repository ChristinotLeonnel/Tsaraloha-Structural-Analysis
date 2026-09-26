#pragma once

#include <string>
#include <memory>

namespace TSA::Commands
{

/**
 * @brief Interface fondamentale pour toutes les commandes de modification du modèle TSA.
 */
class ICommand
{
public:
    virtual ~ICommand() = default;

    /**
     * @brief Exécute la commande sur le modèle.
     * @return true si l'exécution a réussi.
     */
    virtual bool execute() = 0;

    /**
     * @brief Annule l'action effectuée par la commande.
     * @return true si l'annulation a réussi.
     */
    virtual bool undo() = 0;

    /**
     * @brief Nom descriptif de la commande (ex: "Créer Poutre", "Déplacer Nœuds").
     */
    virtual std::string name() const = 0;
};

} // namespace TSA::Commands
