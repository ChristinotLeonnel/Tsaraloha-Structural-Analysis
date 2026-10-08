#pragma once

// API de plugins de l'écosystème Tsaraloha (TSA, TSALab) — ADR-024, phase 8.
//
// Un plugin est une DLL placée dans <dossier de l'application>/plugins. Au démarrage, l'application la
// charge et appelle IPlugin::initialize avec un hôte (IPluginHost) qui lui permet d'ajouter :
//   - des COMMANDES au registre central : elles deviennent aussitôt disponibles dans la console, dans le
//     Blueprint (un nœud « cmd.<id> ») et pour l'IA (outil list_commands) ;
//   - des NŒUDS Blueprint (calcul pur ou action).
// Une commande de plugin modifie le projet en composant les commandes existantes (ICommandContext::execute) :
// chacune garde sa propre entrée Annuler et notifie les vues ; le plugin ne touche jamais le modèle directement.
//
// Ce fichier n'utilise que la bibliothèque standard (ni Qt, ni OCCT) : un plugin ne dépend que des en-têtes
// de l'API. ABI : même compilateur et même bibliothèque d'exécution que l'application (MSVC, /MD).
//
// Points d'entrée exportés par la DLL (voir TSARALOHA_PLUGIN) :
//     int tsaraloha_plugin_api_version();          // doit rendre TSA::Plugins::kApiVersion
//     TSA::Plugins::IPlugin* tsaraloha_create_plugin();   // instance détenue par l'application

#include "../Automation/CommandRegistry.h"
#include "../Blueprint/BlueprintTypes.h"

#include <functional>
#include <string>

namespace TSA::Plugins
{

inline constexpr int kApiVersion = 1;

/// Exécution d'une commande de plugin.
class ICommandContext
{
public:
    virtual ~ICommandContext() = default;
    /// Exécute une commande du registre central sur le projet (une entrée Annuler par commande modifiante).
    virtual TSA::Automation::CommandResult execute(const std::string& commandId, const TSA::Automation::Arguments& args) = 0;
};

/// Exécution d'un nœud Blueprint de plugin.
class INodeContext
{
public:
    virtual ~INodeContext() = default;
    /// Valeur d'une entrée de donnée (reliée, saisie ou par défaut).
    virtual TSA::Blueprint::Value input(const std::string& pin) = 0;
    virtual void setOutput(const std::string& pin, const TSA::Blueprint::Value& value) = 0;
    /// Échec du nœud (message affiché, nœud surligné) ; rend faux.
    virtual bool fail(const std::string& message) = 0;
    virtual void log(const std::string& line) = 0;
    /// Commande du registre central (nœud d'action seulement : nul pour un nœud pur, sans projet).
    virtual ICommandContext* commands() = 0;
};

using CommandHandler = std::function<TSA::Automation::CommandResult(ICommandContext& context, const TSA::Automation::Arguments& args)>;
/// Vrai = succès. Nœud d'action : l'hôte déclenche ensuite la sortie d'exécution « then ».
using NodeHandler = std::function<bool(INodeContext& context)>;

class IPluginHost
{
public:
    virtual ~IPluginHost() = default;
    /// Ajoute une commande (identifiant unique, de préférence préfixé par celui du plugin).
    virtual bool addCommand(const TSA::Automation::CommandSpec& spec, CommandHandler handler) = 0;
    /// Ajoute un nœud. Nœud d'action (pure = false) : entrée « exec » et sortie « then » ajoutées si absentes.
    virtual bool addNode(const TSA::Blueprint::NodeDefinition& definition, NodeHandler handler) = 0;
    virtual void log(const std::string& line) = 0;
    /// Application hôte (« TSA », « TSALab »).
    virtual std::string applicationName() const = 0;
};

struct PluginInfo
{
    std::string id;            ///< « tsalab.sample »
    std::string name;
    std::string version;
    std::string description;
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    /// Enregistre commandes et nœuds. Faux (avec *error) : plugin ignoré.
    virtual bool initialize(IPluginHost& host, std::string* error) = 0;
};

} // namespace TSA::Plugins

#ifdef _WIN32
#define TSARALOHA_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define TSARALOHA_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

/// À placer une fois dans la DLL : TSARALOHA_PLUGIN(MaClasseDePlugin)
#define TSARALOHA_PLUGIN(PluginClass)                                                                     \
    TSARALOHA_PLUGIN_EXPORT int tsaraloha_plugin_api_version() { return TSA::Plugins::kApiVersion; }      \
    TSARALOHA_PLUGIN_EXPORT TSA::Plugins::IPlugin* tsaraloha_create_plugin() { return new PluginClass(); }
