# Blueprint — programmation visuelle de l'écosystème Tsaraloha

Statut : phase 6 de `docs/TSARALOHA_ARCHITECTURE.md` (ADR-024), 2026-10-08 ; valeurs objet, réutilisation des
résultats, variables, commentaires et fichier v2 le 13/10/2026 (programmation structurelle de TSALab :
`TSALab/docs/BLUEPRINT_STRUCTUREL.md`). Moteur partagé (TSA et TSALab), éditeur partagé, affiché dans l'espace
« Blueprint » de TSALab.

## Où est le code

| Partie | Fichiers | Couche |
| :--- | :--- | :--- |
| Types, graphe | `src/Blueprint/BlueprintTypes.h`, `BlueprintGraph.*` | modèle (C++ standard) |
| Bibliothèque, exécution, profil | `src/Blueprint/BlueprintRuntime.*`, `BlueprintNodes.cpp` | modèle |
| Fichier `.tsbp` | `src/Blueprint/BlueprintFile.*` | modèle (Qt Core / JSON) |
| Script de commandes ↔ Blueprint, description pour l'IA | `src/Blueprint/BlueprintScript.*` | modèle (C++ standard) |
| Éditeur | `src/UI/Blueprint/BlueprintScene.*`, `BlueprintEditor.*` | widgets |
| Vues des objets, documentation des nœuds | `src/UI/Blueprint/BlueprintObjectViewer.*` | widgets |
| Exemples | `TSALab/lab/Research/Examples/BlueprintExamples.*`, `StructuralGraphs.*` | application TSALab |
| Tests | `tests/test_blueprint.cpp` (193–195, 197–199, 391), `TSALab/lab/Tests` (L6, L8, L15 à L22) | |

## Modèle d'exécution

- **Flux d'exécution** (broches triangulaires) : ordre des actions, à partir de chaque nœud « Début ».
- **Flux de données** (broches rondes, couleur = type) : Bool, Entier, Réel, Texte, Point (m), Liste d'ids, et
  **objets** (`ValueType::Object`) ; Entier → Réel admis ; broches génériques (Afficher, Concaténer, variables)
  acceptent tout.
- **Objets** : valeurs structurées immuables (`TSA::Automation::ObjectValue` : type stable, résumé, contenu complet,
  égalité par contenu), partagées par `ObjectRef` (pointeur partagé). Le type d'objet d'une broche
  (`PinSpec::objectType`, « Matrix », « FrameElement2D »…) est contrôlé à la connexion (« Matrix → Vector » refusé) et
  à l'exécution (objet arrivé par une broche générique). `ExecutionContext::objectAs<T>()` rend l'objet dans sa classe
  C++. Les objets ne sont ni enregistrés (`.tsbp`) ni acceptés par la ligne de commande et le serveur MCP : ce sont des
  résultats d'exécution.
- **Nœuds purs** (Math, Logique, Paramètres, Texte) : sans exécution, évalués à la demande à chaque lecture
  (une boucle relit donc l'index courant). **Nœuds d'action** : exécutés par le flux ; leurs sorties restent
  lisibles après exécution.
- **Réutilisation des résultats** : un nœud pur marqué `cacheable` (fonction de ses seules entrées : maths,
  logique, blocs scientifiques) n'est réévalué que si son type ou l'une de ses entrées change (`sameValue` :
  même alternative et même contenu, objets comparés par `ObjectValue::equals`). Ses entrées sont évaluées une fois,
  puis transmises à l'exécuteur. `EvaluationCache` (fourni par l'éditeur à chaque exécution) garde les résultats d'une
  exécution à l'autre : seule la partie du graphe touchée par une modification est recalculée ; le profil indique ↺
  pour un résultat réutilisé. Exclus : nœuds d'action, nœuds qui utilisent le projet, variables, paramètres.
- **Variables** : `var.set` (action) mémorise une valeur sous un nom pour la suite de l'exécution, `var.get` (pur,
  réévalué à chaque lecture) la relit ; accumulateurs des boucles.
- Validation avant exécution (bloquante) : types inconnus, nœud enregistré avec une version plus récente que la
  bibliothèque, liens invalides (types, objets), entrées requises, cycles de données, cycles d'exécution.
  **Avertissements** (`NodeLibrary::warnings`, non bloquants) : un nœud d'action lit la sortie d'un nœud d'action qui
  n'est pas exécuté avant lui (les sorties d'un nœud `orderedOutputs` — Séquence, boucles — s'exécutent dans l'ordre :
  ce qui suit « Terminé » vient après le corps).
- Garde-fous : 100 000 étapes, 100 000 itérations par boucle ; erreur = nœud fautif surligné, exécution arrêtée.
- **Profileur** : exécutions, réutilisations et temps exclusif par nœud (affichés sous chaque nœud après exécution).
- **Valeurs intermédiaires** : `ExecutionReport::values` (dernières sorties de chaque nœud, y compris après une erreur)
  et `variables` ; l'éditeur les affiche (onglet « Valeurs ») et ouvre un objet dans sa vue (double-clic, Entrée).
- **Paramétrique** : les nœuds `param.*` ont un nom ; `Runner::run({{"portée", 9.0}})` reconstruit avec d'autres
  valeurs sans modifier le graphe.

## Nœuds

| Catégorie | Nœuds |
| :--- | :--- |
| Événements | Début |
| Paramètres | réel, entier, booléen, texte, point |
| Math | addition, soustraction, multiplication, division, comparer, arrondir en entier, construire / décomposer un point |
| Logique | ET, OU, NON |
| Flux | Si, Séquence, Pour, Tant que |
| Variables | Affecter une variable, Lire une variable |
| Texte / Débogage | Concaténer, Afficher, Afficher le contenu (contenu complet d'un objet : matrice, tableau) |
| Projet (Modèle, Charges, Requêtes, Analyse, Résultats) | **une commande du registre central = un nœud** (`cmd.<id>`), généré automatiquement — dont `analysis.run` (Calculer), `model.set_section` (Affecter section et matériau), `results.summary` / `results.node_displacement` / `results.node_reaction` / `results.member_forces`, `query.bar` et `compare.values` (Comparer à une référence) |
| TSALab | Banc de validation (cœur scientifique) |

Ajouter une commande au registre (`src/Automation/CommandRegistry.cpp`) la rend disponible dans le Blueprint, la
console et (à terme) l'IA, sans autre code. Ajouter un nœud : `NodeLibrary::add(définition, exécuteur)` dans
`registerStandardNodes` (ou une bibliothèque de plugin, ou une bibliothèque d'application : TSALab ajoute sa
programmation structurelle à `NodeLibrary::global()` au démarrage).

Métadonnées d'un nœud (`NodeDefinition`) : `status` (standard, **validé** — logique testée contre des références —,
**expérimental**), `version` (enregistrée dans les fichiers), `documentation` (formules, hypothèses, unités, limites),
`source` (fichier d'implémentation, relatif à la racine des dépôts), `cacheable`, `orderedOutputs`. L'éditeur affiche
✓ (validé) ou ⚗ (expérimental) et ouvre la documentation et le code source (F1).

## Fichier `.tsbp`

JSON versionné (`"format": "tsbp"`, `"version": 2`) : nom, description, nœuds (id, type, `v` = version du type de
nœud, position, valeurs saisies typées), liens (`from`/`out` → `to`/`in`), commentaires (`id`, `x`, `y`, `w`, `h`,
`text`, `color`). La version 1 (sans `v` ni commentaires) est lue telle quelle ; une version future du fichier est
refusée ; un type de nœud inconnu est chargé puis signalé à la validation (plugins absents) ; un nœud d'une version plus
récente que la bibliothèque est signalé. Les objets ne sont jamais enregistrés.

Fragment de presse-papiers (`fragmentToJson` / `pasteFragment`, format `tsbp-fragment`) : nœuds choisis, liens entre eux,
commentaires ; collés avec de nouveaux identifiants et un décalage.

## Débogueur (phase 8)

- `Debugger::beforeNode` (moteur, C++ pur) est consulté avant chaque nœud d'action : `Continue`, `Step`, `Abort`.
  `BreakpointDebugger` s'arrête aux points d'arrêt ou en pas à pas et expose les sorties déjà produites.
- Éditeur : ● Point d'arrêt (pastille rouge), Déboguer (Maj+F5 dans TSALab ; au premier nœud s'il n'y a aucun
  point d'arrêt), Pas à pas (F10), Continuer (F8), ■ Arrêter ; nœud en pause surligné ; panneau « Valeurs produites ».
  La pause est une boucle d'événements locale : les vues du projet se mettent à jour, l'édition du graphe est bloquée.
- `Runner::requestStop()` (appelable depuis un autre thread) arrête avant le nœud suivant.

## Exécution en tâche de fond

`NodeDefinition::usesProject` marque les nœuds qui lisent ou modifient le projet (commandes du registre). Un graphe
qui n'en contient aucun (calcul, banc de validation) s'exécute dans un thread de travail sur une copie du graphe :
interface réactive, ■ Arrêter disponible. Un graphe qui modifie le projet reste dans le thread de l'interface (les
vues observent le modèle) ; le calcul `analysis.run` y est synchrone (`AnalysisController::runBlocking`).

## Historique de l'éditeur

Annuler / Rétablir (graphe) : instantanés du graphe (200 au plus), saisies continues d'un même champ regroupées.
Distinct de l'historique du projet, qui reçoit une entrée par commande exécutée.

## Édition, valeurs, documentation

- **Copier / Coller / Dupliquer** (Ctrl+C, Ctrl+V, Ctrl+D quand le graphe a le focus : la vue prend ces touches avant les
  raccourcis de la fenêtre, pour éviter un raccourci ambigu avec le presse-papier structural de TSA).
- **Commentaires** (touche C) : cadre titré (première ligne du texte) autour des nœuds sélectionnés, derrière eux ;
  déplacer le cadre déplace les nœuds qu'il contient, la poignée en bas à droite le redimensionne, un double-clic
  modifie son texte. Sans effet sur l'exécution.
- **Saisie des réels** : pleine précision, notation scientifique (8.356e-5), point ou virgule décimale. Les entrées
  objet ne se saisissent pas : elles se relient.
- **Onglet « Valeurs »** : sorties de la dernière exécution (nœud sélectionné, ou tous et les variables), sur une ligne
  (contenu complet en info-bulle) ; double-clic ou Entrée sur un objet : vue enregistrée pour son type
  (`registerObjectViewer` : TSALab ajoute tableau de matrice, diagrammes, portique), sinon son contenu texte.
- **Onglet « Problèmes »** : erreurs de validation, avertissements, erreur de la dernière exécution ; double-clic ou
  Entrée : aller au nœud. Après une exécution en erreur, l'onglet s'ouvre et le nœud fautif est centré.
- **Documentation** (F1, bouton « Documentation… ») : description, statut, entrées et sorties typées, documentation
  détaillée, code source (lu depuis les dépôts quand ils sont présents sur le poste).

## Script de commandes et IA

```text
a = model.create_node position=0,0,0
b = model.create_node position=6,0,0
model.create_beam start=a.id end=b.id section="IPE 300"
```
`fromCommandScript` : une ligne = un nœud de commande chaîné au précédent ; `a.id` = lien de données. `toCommandScript`
réécrit la chaîne de commandes partant du Début (boucles, maths, paramètres : signalés). `describe` : texte du graphe.
Éditeur : menu Script (importer, copier en script, copier la description). IA : outils `list_commands` et
`propose_blueprint` (script vérifié, rien n'est exécuté sans l'accord de l'ingénieur) ; accepté, le Blueprint est ouvert
dans l'éditeur de TSALab, ou exécuté sur le projet dans TSA (une entrée Annuler par commande).

## Plugins

Nœuds et commandes ajoutés par des DLL : `docs/PLUGINS.md`.
