# Plugins de l'écosystème Tsaraloha (TSA, TSALab)

Statut : phase 8 de `docs/TSARALOHA_ARCHITECTURE.md` (ADR-024), 2026-10-08.

## Principe

Un plugin est une DLL placée dans `<dossier de l'application>/plugins`. Au démarrage, `EcosystemApplication`
(commun à TSA et TSALab) appelle `TSA::Plugins::PluginManager::loadDirectory`. Chaque DLL reçoit un hôte
(`IPluginHost`) et peut ajouter :

| Ajout | Effet immédiat |
| :--- | :--- |
| Commande (`addCommand`) | registre central : console, Blueprint (nœud `cmd.<id>`), IA (`list_commands`, `propose_blueprint`) |
| Nœud Blueprint (`addNode`) | palette de l'éditeur, scripts, exécution (pur ou action) |

Une commande de plugin **ne modifie jamais le modèle directement** : elle compose les commandes existantes
(`ICommandContext::execute`), chacune avec sa propre entrée Annuler et ses notifications aux vues.

## Écrire un plugin

```cpp
#include "Plugins/PluginApi.h"   // bibliothèque standard seulement (ni Qt, ni OCCT)

class MonPlugin final : public TSA::Plugins::IPlugin
{
public:
    TSA::Plugins::PluginInfo info() const override { return { "societe.outil", "Mon outil", "1.0.0", "…" }; }
    bool initialize(TSA::Plugins::IPluginHost& host, std::string* error) override
    {
        TSA::Automation::CommandSpec spec;   // id, titre, catégorie, paramètres typés (unités), sorties
        spec.id = "societe.outil";
        host.addCommand(spec, [](TSA::Plugins::ICommandContext& c, const TSA::Automation::Arguments& a) {
            return c.execute("model.create_node", { { "position", TSA::Automation::Point3 { 0, 0, 0 } } });
        });
        return true;
    }
};
TSARALOHA_PLUGIN(MonPlugin)   // exporte tsaraloha_plugin_api_version et tsaraloha_create_plugin
```

Exemple complet : `TSALab/plugins/sample/SamplePlugin.cpp` (commande composée `sample.portal`, nœud pur
`sample.golden`), cible CMake `tsalab_sample_plugin` (sortie dans `build-*/plugins`), test L8 de TSALab.

## Règles

- **ABI** : même compilateur et même bibliothèque d'exécution que l'application (MSVC, `/MD` ou `/MDd`), car
  l'interface échange des types `std::` (chaînes, `std::function`, `std::variant`).
- `kApiVersion` (actuellement 1) : une DLL d'une autre version est refusée, avec le motif.
- Identifiants uniques, préfixés par celui du plugin ; un doublon est refusé.
- Une DLL n'est jamais déchargée (ses fonctions restent référencées par les registres).
- Arrêt (facultatif, 2026-10-10) : une DLL peut exporter `extern "C" void tsaraloha_plugin_shutdown()`, appelé une fois
  à l'arrêt du module qui la fournit ou à la fermeture de l'application (`PluginManager::shutdownFile` / `shutdownAll`).
  L'API reste en version 1 : un plugin sans ce point continue d'être chargé à l'identique.
- Un plugin peut être livré par un **module** (`module.json`, capacité `plugin`) : il n'est alors chargé que si le module
  est livré ou approuvé par l'utilisateur (docs/SDK.md §3).
- Liste et refus : menu Aide ▸ Plugins chargés (TSALab), journal de session (`PluginLoaded` / `PluginRejected`).
