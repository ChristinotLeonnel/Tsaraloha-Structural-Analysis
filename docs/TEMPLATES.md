# Templates de documents (notes de calcul, rapports)

Tous les documents produits par TSA et TSALab passent par un **template**. Le C++ ne fait plus de mise en page.
Ce document s'adresse à deux publics : les utilisateurs qui choisissent et personnalisent un template, et les
auteurs qui en créent un.

## 1. Architecture

```
moteur de calcul ──► ResultsModel ──┐
modèle (Model) ─────────────────────┼─► données du rapport (JSON) ─► template ─► moteur de templates ─► HTML ─► rendu (HTML / PDF)
NDCGenerator ─► NDCDocument ────────┘        src/Reports, src/NDC     .tsatemplate   src/Templates       src/Reports/DocumentRenderer
```

Chaque couche a un rôle séparé :

| Couche | Rôle | Fichiers |
|---|---|---|
| Moteur de calcul | produit les résultats | `src/Analysis` |
| Modèle de données du rapport | construit un JSON documenté, sans mise en page, sans valeur inventée | `src/Reports/ReportDataBuilder` (`tsa-report-data/1`), `src/NDC/NDCTemplateData` (`tsa-ndc/1`) |
| Template de présentation | données, styles, images : jamais de code | `.tsatemplate`, `resources/templates/builtin/` |
| Moteur de templates | substitue les données | `src/Templates/TemplateEngine` |
| Rendu | HTML autonome, PDF | `src/Reports/DocumentRenderer` |
| Dépôt | origines, import, export, versions, personnalisation | `src/Templates/TemplateRepository` |

Les dépendances vont toujours dans le même sens : `Templates` ne dépend de rien, `Reports` dépend de `Templates`
et du modèle, `NDC` dépend de `Templates` et de `Reports`. Il n'y a aucune dépendance circulaire.

## 2. Choix du moteur de templates

Le moteur est un sous-ensemble de **Mustache** (logic-less), écrit dans TSA sur `QJsonValue`
(`src/Templates/TemplateEngine.cpp`, environ 400 lignes).

| Critère | Choix retenu |
|---|---|
| Licence | code propre au projet : aucune licence tierce à respecter |
| Dépendances | Qt Core seulement, déjà requis |
| Sécurité | aucune exécution de code, aucun accès aux fichiers ni au réseau, profondeur d'inclusion et taille de sortie bornées |
| Portabilité des templates | syntaxe Mustache standard : un template simple s'écrit et se teste avec n'importe quel outil Mustache |

Alternatives écartées :

- **Inja / Jinja2-like** : expressions et boucles programmables, donc plus de surface d'attaque pour des templates importés ; en plus, une dépendance à nlohmann/json.
- **Grantlee / KTextTemplate** : dépendance lourde (Qt Qml/Script selon les versions), licence LGPL à suivre.
- **XSLT** : Qt XmlPatterns a disparu de Qt 6.

### Syntaxe prise en charge

| Syntaxe | Effet |
|---|---|
| `{{nom}}` | valeur échappée pour le HTML (`& < > " '`) |
| `{{{nom}}}` ou `{{& nom}}` | valeur brute (contenu HTML déjà produit par l'application) |
| `{{#nom}}…{{/nom}}` | section. Une liste est répétée pour chaque élément ; un objet devient le contexte ; une valeur vraie affiche le bloc |
| `{{^nom}}…{{/nom}}` | section inverse, affichée si la valeur est absente, fausse, vide, `0` ou une liste vide |
| `{{> partiel}}` | inclusion d'un fichier du même paquet (`partiel`, `partials/partiel.html` ou `partiel.html`) |
| `{{! texte}}` | commentaire |
| `a.b.c`, `{{.}}` | noms pointés, élément courant |

Règles de rendu :

- Une balise de section, de commentaire ou d'inclusion seule sur sa ligne supprime la ligne entière (règle « standalone » de Mustache).
- Un booléen s'affiche « oui » ou « non ». Un nombre entier s'affiche sans décimale.
- Ne sont **pas** pris en charge, et sont refusés explicitement : le changement de délimiteurs (`{{= =}}`), les lambdas, l'indentation des partiels.

### Diagnostics

- **Erreurs** : syntaxe invalide, partiel introuvable, liste ou objet affiché comme une valeur, inclusions trop profondes, sortie trop volumineuse. Une erreur rend le rendu invalide : le document n'est pas produit.
- **Variables absentes** : la variable est rendue vide **et** signalée dans `missingVariables`. Le dialogue les affiche. Les tests exigent qu'aucun rapport intégré n'en produise.
- **Avertissements** : par exemple une option refusée, auquel cas la valeur par défaut est utilisée.

## 3. Format de paquet `.tsatemplate`

Un paquet est un fichier JSON unique, autonome et portable. Son schéma est `docs/schemas/tsa-template-package-1.schema.json`.

```json
{
  "format": "tsa-template-package/1",
  "manifest": {
    "id": "societe.note-beton",
    "name": "Note béton armé",
    "version": "1.2.0",
    "description": "…",
    "author": "Bureau X",
    "license": "CC-BY-4.0",
    "engine": "tsa-template/1",
    "dataSchema": "tsa-report-data/1",
    "applications": ["TSA", "TSALab"],
    "reports": [
      { "id": "general", "name": "Rapport général", "entry": "main.html",
        "requires": ["project.title", "model", "results"] }
    ],
    "options": [
      { "id": "primaryColor", "label": "Couleur", "type": "color", "default": "#1a56db" },
      { "id": "logo", "label": "Logo", "type": "image", "default": "" }
    ]
  },
  "files": {
    "main.html": { "text": "…" },
    "partials/style.html": { "text": "…" },
    "logo.png": { "base64": "iVBORw0…" }
  }
}
```

Les paquets intégrés, comme les sources d'un auteur, peuvent aussi être un **dossier** contenant `manifest.json`
et les fichiers. `TemplatePackage::loadDirectory` lit ce dossier ; l'export produit toujours le fichier unique.

### Champs du manifeste

| Champ | Contenu |
|---|---|
| `id` | minuscules, au moins un point (`societe.nom`) |
| `version` | `majeur.mineur[.correctif]` |
| `engine` | `tsa-template/1`. Un moteur plus récent est refusé avec le message « mettre à jour l'application » |
| `dataSchema` | `tsa-report-data/1` (modèle et résultats) ou `tsa-ndc/1` (note de calcul structurée) |
| `reports` | un ou plusieurs rapports (fichier d'entrée et données obligatoires `requires`). Si une donnée obligatoire manque, le rendu est refusé et la liste des données manquantes est donnée |
| `options` | paramètres de l'utilisateur, de type `text`, `color` (`#rrggbb`), `bool`, `number`, `choice` (`choices`) ou `image` (URI `data:image/…;base64,` fournie par l'application) |
| `basedOn`, `baseHash` | renseignés automatiquement sur une copie personnalisée |
| autres champs | conservés tels quels à l'export |

### Données reçues par le template

Le template reçoit les données du schéma déclaré, auxquelles s'ajoutent :

- `options` : les valeurs effectives des options ;
- `assets` : les images du paquet en URI `data:`. La clé est le chemin dont les caractères non alphanumériques sont remplacés par `_`, par exemple `{{{assets.logo_png}}}` ;
- `template` : `id`, `name`, `version`, `report`, `reportName`.

### Validation et sécurité

Un paquet est une **donnée**. La validation, appliquée à chaque chargement, import et personnalisation, refuse :

- les balises `<script>`, `<iframe>`, `<frame>`, `<object>`, `<embed>`, `<applet>`, `<form>`, `<base>`, `<link>` et `<meta http-equiv>` ;
- les gestionnaires `on…=`, les URL `javascript:` et `vbscript:`, les `expression()` CSS ;
- les ressources distantes : `@import`, `url(http…)`, `src="http…"`, `file:`, `//…`. Le document produit doit rester autonome ;
- les chemins absolus, ou contenant `..`, `\` ou `:` ;
- les types de fichiers autres que `html htm css txt md svg json png jpg jpeg gif` ;
- une syntaxe de template invalide, un partiel absent, un rapport sans fichier d'entrée ;
- un paquet de plus de 32 Mo ou de plus de 500 fichiers.

Le moteur lui-même n'exécute rien. Ces règles protègent donc surtout le HTML exporté quand il est ouvert dans un navigateur.

## 4. Dépôt et origines

Le dossier utilisateur est `<données>/templates`. Son emplacement exact est `QStandardPaths::AppLocalDataLocation` :
sous Windows, `%LOCALAPPDATA%/Tsaraloha/<produit>/templates`.

| Origine | Emplacement | Modifiable |
|---|---|---|
| intégré | ressources de l'application (`:/templates/builtin/<id>/`) | non : on le personnalise par copie |
| module | fourni par un module actif (`templates` de `module.json`), retiré à l'arrêt du module | non |
| utilisateur | `<templates>/*.tsatemplate` | oui |
| importé | `<templates>/imported/`, avec l'empreinte d'import dans `index.json` ; « modifié » si le contenu a changé depuis l'import | oui |
| personnalisé | `<templates>/custom/<id>.tsatemplate`, avec `basedOn` et `baseHash` ; « modifié » si le contenu diffère de l'original | oui |
| ancienne version | `<templates>/versions/` : chaque version remplacée par un import ou une personnalisation | export, suppression |

Fonctionnement du dépôt :

- **Paquet effectif** pour un même identifiant : personnalisé, sinon importé, sinon utilisateur, sinon module, sinon intégré. L'original reste toujours disponible.
- **Aucun remplacement silencieux** : la version précédente est déplacée dans `versions/`.
- **Paquets invalides** : ils ne sont pas chargés. Ils sont listés (`problems()`) avec leurs erreurs.

## 5. Templates intégrés

| Paquet | Schéma | Rapports |
|---|---|---|
| `tsa.ndc.standard` | `tsa-ndc/1` | `ndc` : note de calcul complète (couverture, sommaire, listes des figures et tableaux, chapitres) |
| `tsa.reports.base` | `tsa-report-data/1` | `general`, `beam`, `column`, `portal`, `plane-2d`, `structure-3d`, `mesh`, `modal`, `displacements-forces`, `verification` |

`tsa.ndc.standard` reproduit **octet par octet** la mise en page historique `NDCDocument::toHtml` (test 282, sur
cinq configurations et sur une note réelle de `NDCGenerator`). L'ancien générateur est conservé : il sert de
référence de non-régression et de **repli explicite** quand le template choisi (`ReportConfiguration::templateId`)
est introuvable ou en erreur. `NDCExporter::renderHtml` signale ce repli dans ses diagnostics.

Les rapports de `tsa.reports.base` n'inventent aucun résultat :

- chaque bloc de données porte `available` et, s'il est indisponible, `reason`, que le template affiche ;
- le rapport `modal` affiche « Analyse modale non disponible… », car les moteurs de TSA sont statiques ;
- le rapport `verification` affiche l'absence de vérifications structurées dans les résultats ;
- le rapport `plane-2d` déclare « non applicable » un modèle qui n'est pas plan ;
- les résultats sont toujours présentés comme « calculés, non validés par un ingénieur ».

Ces options sont communes aux rapports de base : titre du document, couleur des titres, logo (incorporé), en-tête
(texte), pied de page (texte), affichage de l'en-tête et du pied.

Pour modifier les templates intégrés, éditez `resources/templates/builtin/<id>/`, puis lancez
`python tools/update_templates_qrc.py`. Le test 283 vérifie que les ressources compilées correspondent aux sources.

## 6. Modèle de données `tsa-report-data/1`

Les valeurs sont déjà formatées avec leur unité : les templates ne calculent rien.

| Chemin | Contenu |
|---|---|
| `project.*` | `title`, `description`, `number`, `documentNumber`, `revision`, `status`, `engineer`, `organization`, `client`, `date` |
| `application.*`, `generated` | produit, version, date de génération |
| `model.counts.*` | `nodes`, `members`, `beams`, `columns`, `trusses`, `cables`, `slabs`, `walls`, `foundations`, `supports`, `loadCases`, `combinations` |
| `model.geometry` | `dx`, `dy`, `dz`, `planar`, `plane` (`XZ`, `YZ`, `XY` ou vide) |
| `model.levels[]` | `name`, `elevation` |
| `model.nodes[]`, `model.supports[]` | `id`, `name`, `x`, `y`, `z`, `support` (libellé), `supported` |
| `model.members[]`, `model.beams[]`, `model.columns[]` | `ref`, `family`, `familyLabel`, `name`, `nodeI`, `nodeJ`, `length`, `section`, `material`, `results` (voir ci-dessous) |
| `model.materials[]`, `model.sections[]` | caractéristiques (E, ν, masse volumique, fk ; A, Iy, Iz) |
| `model.loadCases[]`, `model.combinations[]`, `model.nodalLoads[]`, `model.memberLoads[]` | charges |
| `results` | `available`, `reason` ou `status`, `engine`, `case`, `timestamp`, `analysisType`, `units`, puis `displacements`, `reactions`, `memberForces` |
| `results.displacements` | `available`, `unit`, `rows[]`, `max{node,value}` |
| `results.reactions` | `available`, `rows[]`, `sum{fx,fy,fz}` |
| `results.memberForces` | `available`, `rows[]` (barres et leurs `results`) |
| `model.members[].results` | `available`, extrema sur les stations calculées : `Nmax`, `Nmin`, `Vy`, `Vz`, `T`, `My`, `Mz`, `deflection`, `stations` |
| `mesh` | `available`, `engine`, `description`, `nodes`, `internalNodes`, `cells`, `byType[]` |
| `modal`, `verification` | `available` (faux aujourd'hui) et `reason` |

Le schéma `tsa-ndc/1` est construit par `src/NDC/NDCTemplateData.cpp`. Il comprend :

- `project`, `page`, `style`, `show` et `logoSvg` ;
- `chapters[].sections[]`, avec `paragraphs`, `keyValues`, `tables` et `figures` ;
- `allFigures` et `allTables`.

## 7. Utilisation dans l'application

Le dialogue s'ouvre par **Résultats > Documents par templates...**, commande `cmd.report.templates`. Il permet de :

- choisir le template et le rapport, avec affichage de l'origine, de la version et de l'état (modifié ou d'origine, remplacé, module) ;
- régler les options générées depuis le manifeste : titre, couleur, logo (le fichier image est incorporé), en-tête, pied de page ;
- voir un aperçu, avec les diagnostics (erreurs, données absentes, ressources externes) ;
- exporter en **HTML autonome** ou en **PDF** (A4 ou A3, portrait ou paysage) ;
- gérer les templates :
  - **Importer** : validation, archivage de la version précédente ;
  - **Exporter** : le template ;
  - **Personnaliser** : les options courantes deviennent les valeurs par défaut d'une copie ;
  - **Modifier un fichier** : l'édition crée une copie personnalisée, validée avant enregistrement ;
  - **Supprimer** : copies utilisateur, importées et personnalisées, anciennes versions.

La note de calcul du dock NDC, ses exports HTML et PDF passent par le template `ReportConfiguration::templateId`
(par défaut `tsa.ndc.standard`).

## 8. Guide de création d'un template

1. Partez d'un template existant : **Exporter le template**, ou copie de `resources/templates/builtin/tsa.reports.base`.
2. Changez `id` (votre domaine, par exemple `societe.rapport-acier`), `name`, `version`, `author` et `license`.
3. Écrivez le HTML et le CSS dans le paquet. Les images vont dans le paquet et sont référencées par `{{{assets.…}}}`.
4. Pour chaque donnée facultative, testez sa disponibilité et affichez la raison quand elle manque :
   ```html
   {{#results.available}} … {{/results.available}}
   {{^results.available}}<p class="na">Résultats non disponibles : {{results.reason}}.</p>{{/results.available}}
   ```
5. Déclarez dans `requires` les données sans lesquelles le rapport n'a pas de sens.
6. Validez et emballez le paquet. Deux méthodes :
   - importez le dossier emballé dans l'application ;
   - ou écrivez un petit programme qui appelle `TemplatePackage::loadDirectory` puis `saveFile`.

   L'exemple `sdk/examples/templates/` contient un paquet minimal.
7. Vérifiez dans l'aperçu que les diagnostics ne signalent ni erreur, ni donnée absente, ni ressource externe.

## 9. Limites connues

- Le rendu PDF passe par `QTextDocument` : CSS simplifié (pas de flexbox ni de grille, pagination approximative). Le HTML exporté conserve la mise en page complète pour un navigateur.
- Les données des rapports de base sont celles du dernier calcul (un cas ou une combinaison). Les enveloppes multi-combinaisons ne font pas partie de `tsa-report-data/1`.
- Les unités affichées sont celles du modèle (kN, m) et du moteur. La conversion d'unités au choix de l'utilisateur n'est pas encore une option des rapports de base.
- La personnalisation conserve l'identifiant d'origine. Pour diffuser une variante distincte, exportez-la puis changez son `id`.
