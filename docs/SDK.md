# SDK de l'écosystème Tsaraloha (TSA, TSALab)

Ce document décrit comment étendre TSA et TSALab sans modifier leur code. Il s'adresse aux développeurs
de modules.

| Besoin | Mécanisme | Contrainte |
|---|---|---|
| Ajouter un format d'import ou d'export | **convertisseur** d'un module, exécuté en processus séparé, qui produit ou lit du TSA3D | aucune (tout langage, aucune ABI) |
| Ajouter des documents (notes, rapports) | **paquet de templates** `.tsatemplate` d'un module | données seulement (`docs/TEMPLATES.md`) |
| Ajouter des commandes et des nœuds Blueprint | **plugin** DLL d'un module (`docs/PLUGINS.md`) | même compilateur et même runtime que l'hôte |
| Produire un modèle depuis un autre programme | écrire un fichier **TSA3D** (`docs/TSA3D.md`) | aucune |
| Utiliser le modèle TSA dans un programme C++ | bibliothèque `TSA_Model` (`sdk/examples/tsa3d-roundtrip`) | Qt 6, même compilateur |

## 1. Architecture et couches

```
            ┌──────────── application (TSA / TSALab : fenêtre, menus) ────────────┐
            │   dialogues Modules, Documents par templates, Import via un module    │
            ├──────────────────────── services de l'hôte ─────────────────────────┤
            │ ModuleRegistry   TemplateRepository   PluginManager   Tsa3d (IO)       │
            ├──────────────────────────── modèle ─────────────────────────────────┤
            │ Model, ResultsModel, ReportDataBuilder, TemplateEngine                │
            └──────────────────────────────────────────────────────────────────────┘
   modules : module.json ──► convertisseurs (processus) │ templates (données) │ plugin (DLL, API v1)
```

- `src/Modules`, `src/Templates`, `src/Reports` et `src/IO/Tsa3d` appartiennent à la couche **modèle**. Ils n'utilisent aucun widget (contrôle : `tools/check_layers.py`).
- Le registre ne dépend ni de l'interface, ni des templates, ni du chargeur de plugins. L'hôte lui fournit des services (`ModuleHostServices`).

### Flux d'intégration complet

1. Un fichier tiers est lu par un convertisseur, dans un processus séparé.
2. Le convertisseur produit un fichier TSA3D.
3. Le fichier est validé, puis importé : le modèle reste intact en cas de refus.
4. Le calcul est effectué par un moteur.
5. Le résultat est un `ResultsModel`, présenté comme « calculé, non validé ».
6. `ReportDataBuilder` construit les données du rapport.
7. Un template (intégré, de module ou de l'utilisateur) met ces données en page.
8. Le document est rendu en HTML ou en PDF.

## 2. Contrat d'un module (`module.json`, schéma `tsa-module/1`)

Un module est un dossier qui contient `module.json`. Le schéma JSON est `docs/schemas/tsa-module-1.schema.json`.

```json
{
  "schema": "tsa-module/1",
  "id": "societe.import-xyz",
  "name": "Import XYZ",
  "version": "1.2.0",
  "description": "…",
  "publisher": "Société",
  "license": "MIT",
  "hosts": [ { "application": "TSA", "min": "0.1.0", "max": "0.9.0" }, { "application": "TSALab", "min": "0.1.0" } ],
  "dependencies": [ { "id": "societe.base", "min": "1.0", "optional": false } ],
  "capabilities": ["process", "templates"],
  "dataTypes": ["TSA3D/1"],
  "entryPoints": {
    "importers": [ { "id": "societe.import-xyz.xyz", "title": "Fichiers XYZ", "extensions": ["xyz"],
                     "command": "bin/xyz2tsa3d.exe", "arguments": ["{input}", "{output}"] } ],
    "exporters": [],
    "plugin": "bin/xyz_plugin.dll"
  },
  "templates": ["templates/rapport-xyz.tsatemplate"],
  "resources": ["examples/exemple.xyz"]
}
```

| Élément du contrat | Champ |
|---|---|
| Identifiant | `id` en minuscules avec au moins un point. Il préfixe tous les identifiants du module (convertisseurs) |
| Nom, version, description, éditeur, licence | `name`, `version` (`majeur.mineur[.correctif]`), `description`, `publisher`, `license` |
| Compatibilité de l'hôte | `hosts` : application (`TSA` ou `TSALab`), `min`, `max` facultatif |
| Dépendances | `dependencies` : `id`, version `min`, `optional` |
| Commandes, menus, nœuds | via le `plugin` (API v1) ; les convertisseurs apparaissent dans **Fichier > Importer via un module...** |
| Types de données | `dataTypes` (ex. `TSA3D/1`) |
| Formats d'import et d'export | `entryPoints.importers` et `entryPoints.exporters` |
| Templates | `templates` : paquets `.tsatemplate` |
| Services | `ModuleHostServices` : enregistrement de templates, chargement et arrêt de plugin |
| Erreurs | `ModuleInfo.state` et `messages`, affichés dans **Aide > Modules** et dans le journal (`Module …`) |
| Initialisation et arrêt | `activate` dans l'ordre des dépendances ; `shutdown` dans l'ordre **inverse d'activation** (templates retirés, plugin notifié) |

### Validation du manifeste

Elle est faite par `ModuleManifest::fromJson` :

- schéma, identifiant, nom et versions valides ;
- au moins un hôte, avec `max` ≥ `min` ;
- chemins **confinés** au dossier du module (ni absolu, ni `..`, ni `:`), et fichiers présents ;
- identifiants des convertisseurs préfixés par l'identifiant du module, extensions présentes, `{input}` et `{output}` dans les arguments ;
- **capacités déclarées = capacités utilisées** :
  - un plugin exige `plugin` ;
  - un convertisseur exige `process` ;
  - un template exige `templates` ;
  - une capacité inconnue est refusée ;
- aucune dépendance à soi-même.

### États

| État | Signification |
|---|---|
| `invalide` | manifeste illisible ou refusé, ou identifiant déjà fourni (le module livré l'emporte) |
| `incompatible` | hôte ou version hors bornes |
| `dépendance manquante` | dépendance absente, trop ancienne ou elle-même inactive (propagation) |
| `dépendances circulaires` | cycle détecté |
| `à approuver` | module utilisateur valide, non approuvé |
| `désactivé` | désactivé pour la session |
| `prêt` | prêt à être activé |
| `actif` | activé |
| `échec` | erreur à l'activation (plugin refusé…) ; les contributions déjà faites sont retirées |

## 3. Confiance et sécurité

- **Modules livrés** (`<application>/modules`) : approuvés d'office.
- **Modules utilisateur** (`<données>/modules`, sous Windows `%LOCALAPPDATA%/<organisation>/<produit>/modules`, pour TSA `%LOCALAPPDATA%/TSA Engineering/TSA/modules`) :
  - découverts et validés, mais **jamais activés** sans approbation explicite dans **Aide > Modules > Approuver**. Tant qu'ils ne sont pas approuvés, aucun plugin n'est chargé et aucun convertisseur n'est exécuté ;
  - l'approbation mémorise `id@sha256(module.json)` dans `<configuration>/modules-trust.json` ;
  - **toute modification du manifeste exige une nouvelle approbation**.
- **Convertisseurs** :
  - exécutés hors du processus de l'application : un plantage du convertisseur n'affecte pas l'application ;
  - lancés avec le dossier du module comme dossier de travail ;
  - délai maximal de 120 s, après quoi le processus est arrêté ;
  - succès = code de sortie 0 **et** fichier produit ;
  - leur sortie est **toujours** validée par l'import TSA3D : un module n'écrit jamais dans le modèle.
- **Templates** : ce sont des données validées, sans script ni ressource externe (`docs/TEMPLATES.md` §3).
- **Plugins** : du code natif, chargé uniquement pour un module approuvé (ou livré) qui déclare `plugin`.

## 4. Stratégie d'ABI

| Mode | ABI | Usage recommandé |
|---|---|---|
| Convertisseur (processus séparé) | **aucune** : fichiers, arguments, code de sortie | formats d'échange, outils tiers, autres langages, autres compilateurs |
| Template | aucune (JSON) | documents |
| Plugin DLL (API v1) | même compilateur (MSVC), même runtime (`/MD` ou `/MDd`), `kApiVersion` = 1 | commandes et nœuds intégrés à l'interface |

- L'API v1 des plugins n'a pas changé.
- Le point d'arrêt `tsaraloha_plugin_shutdown` est **facultatif**. Il est appelé à l'arrêt du module ou à la fermeture de l'application, et un plugin existant sans ce point reste compatible.
- Une DLL n'est jamais déchargée : ses fonctions restent référencées par les registres.
- Pour toute extension qui ne demande pas d'intégration fine à l'interface, préférez le processus séparé.

## 5. Exemples du SDK

Tous ces exemples sont compilés avec le produit, ce qui garantit qu'ils compilent.

| Exemple | Contenu | Cible / vérification |
|---|---|---|
| `sdk/modules/sample.csvimport` | module complet : manifeste, convertisseur CSV vers TSA3D (C++17 standard, runtime statique), exemple `portique.csv` | `sample_csv2tsa3d` ; assemblé dans `<build>/modules/sample.csvimport` ; test 279 |
| `sdk/tools/tsa3d-validate` | validation TSA3D en ligne de commande | `tsa3d-validate`, livré |
| `sdk/tools/tsa-template` | validation, emballage et rendu de templates | `tsa-template`, livré |
| `sdk/examples/tsa3d-generator` | générateur externe en C++17, sans Qt ni TSA | `tsa3d_generator_example` |
| `sdk/examples/tsa3d-roundtrip` | intégration de la bibliothèque TSA : lecture, modèle, export | `tsa3d_roundtrip_example` |
| `sdk/examples/templates/societe.rapport-simple` | template de rapport à partir d'un modèle | test 284 |
| `TSALab/plugins/sample` | plugin DLL (commande, nœud Blueprint) | `tsalab_sample_plugin`, test L8 |

Parmi les familles d'exemples demandées, voici celles qui sont couvertes :

- **calcul** (moteur en processus séparé) : couvert par le schéma convertisseur + TSA3D ;
- **visualisation** et **intégration TSALab** : couverts par le plugin d'exemple de TSALab ;
- **rapport depuis template** : `societe.rapport-simple` ;
- **validation TSA3D** : `tsa3d-validate` ;
- **import** : `sample.csvimport`.

Un moteur de calcul tiers **intégré** comme moteur de TSA (registre `AnalysisEngineRegistry`) n'est pas exposé aux
modules. Voir les limites (§8).

### Créer un module à convertisseur

1. Écrivez un exécutable `outil <entrée> <sortie.tsa3d>`. Il renvoie 0 en cas de succès et écrit ses erreurs sur la sortie standard ou la sortie d'erreur, que l'application affiche dans la console.
2. Validez sa sortie avec `tsa3d-validate`.
3. Rédigez `module.json` (capacité `process`). Placez l'exécutable dans `bin/`, avec ses DLL éventuelles, ou liez-le statiquement.
4. Copiez le dossier dans `<données>/modules`, puis ouvrez **Aide > Modules** : le module apparaît « à approuver ». Après approbation, il est actif.
5. Utilisez **Fichier > Importer via un module...**.

## 6. API C++ des services

```cpp
#include "Modules/ModuleRegistry.h"
auto& reg = TSA::Modules::ModuleRegistry::instance();
reg.setHost("TSA", "0.1.0");
reg.discover({ AppPaths::shippedModulesDir() }, { AppPaths::userModulesDir() });
reg.activate(services);                       // ModuleHostServices
for (const auto& c : reg.importers()) { … }   // convertisseurs des modules actifs
QString log; reg.runConverter(c, input, output, &log);
reg.shutdown(services);
```

## 7. Tests

| Suite | Tests | Contenu |
|---|---|---|
| `--suite=modules` | 274-279 | manifeste, compatibilité, dépendances et cycles, confiance et empreinte, activation et arrêt ordonnés, convertisseur réel |
| `--suite=tsa3d` | 267-273 | format d'échange |
| `--suite=templates` | 280-285 | templates |

## 8. Limites connues

- La désactivation d'un module vaut pour la session ; elle n'est pas mémorisée.
- **Recharger** dans le dialogue Modules ne décharge pas une DLL de plugin déjà chargée : un redémarrage est nécessaire pour remplacer un plugin.
- Les modules ne peuvent pas encore ajouter un moteur de calcul au registre des moteurs, ni des menus propres en dehors des convertisseurs (les plugins ajoutent des commandes).
- La signature cryptographique des modules n'est pas gérée. La confiance repose sur l'approbation explicite de l'utilisateur et sur l'empreinte du manifeste.
