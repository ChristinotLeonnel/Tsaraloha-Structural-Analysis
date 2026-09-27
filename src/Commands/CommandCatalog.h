#pragma once

#include "CommandCategory.h"
#include <string>
#include <vector>
#include <map>

namespace TSA::Commands
{

/**
 * @brief Descripteur de métadonnées d'une commande CAO dans le catalogue TSA.
 */
struct CommandDescriptor
{
    std::string id;             // Identifiant unique (ex: "cmd.create.beam", "cmd.modify.move")
    std::string name;           // Libellé court affiché dans les menus/rubans
    std::string description;    // Description ou tooltip d'aide
    std::string shortcut;       // Raccourci clavier (ex: "Ctrl+Z", "M", "Echap")
    std::string iconPath;       // Chemin de la ressource d'icône Qt (ex: ":/icons/structure/beam.svg")
    CommandCategory category;   // Catégorie fonctionnelle (CREATE, MODIFY, SELECTION, etc.)
};

/**
 * @brief Catalogue centralisé des commandes professionnelles TSA.
 * Fournit une source unique de vérité pour les menus, rubans, toolbars et palettes.
 */
class CommandCatalog
{
public:
    static CommandCatalog& instance();

    void registerCommand(const CommandDescriptor& desc);
    const CommandDescriptor* findCommand(const std::string& id) const;
    std::vector<CommandDescriptor> commandsInCategory(CommandCategory category) const;
    const std::map<std::string, CommandDescriptor>& allCommands() const { return m_commands; }

private:
    CommandCatalog();
    void initializeStandardCatalog();

    std::map<std::string, CommandDescriptor> m_commands;
};

} // namespace TSA::Commands
