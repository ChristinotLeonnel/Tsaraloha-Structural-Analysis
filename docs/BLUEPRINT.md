# Blueprint — programmation visuelle de l'écosystème Tsaraloha

Statut : phase 6 de `docs/TSARALOHA_ARCHITECTURE.md` (ADR-024), 2026-10-08. Moteur partagé (TSA et TSALab),
éditeur partagé, affiché dans l'espace « Blueprint » de TSALab.

## Où est le code

| Partie | Fichiers | Couche |
| :--- | :--- | :--- |
| Types, graphe | `src/Blueprint/BlueprintTypes.h`, `BlueprintGraph.*` | modèle (C++ standard) |
| Bibliothèque, exécution, profil | `src/Blueprint/BlueprintRuntime.*`, `BlueprintNodes.cpp` | modèle |
| Fichier `.tsbp` | `src/Blueprint/BlueprintFile.*` | modèle (Qt Core / JSON) |
| Script de commandes ↔ Blueprint, description pour l'IA | `src/Blueprint/BlueprintScript.*` | modèle (C++ standard) |
| Éditeur | `src/UI/Blueprint/BlueprintScene.*`, `BlueprintEditor.*` | widgets |
| Exemples | `TSALab/lab/Research/Examples/BlueprintExamples.*` | application TSALab |
| Tests | `tests/test_blueprint.cpp` (193–195, 197–198), `TSALab/lab/Tests` (L6, L8) | |

## Modèle d'exécution

- **Flux d'exécution** (broches triangulaires) : ordre des actions, à partir de chaque nœud « Début ».
- **Flux de données** (broches rondes, couleur = type) : Bool, Entier, Réel, Texte, Point (m), Liste d'ids ;
  Entier → Réel admis ; broches génériques (Afficher, Concaténer) acceptent tout.
- **Nœuds purs** (Math, Logique, Paramètres, Texte) : sans exécution, évalués à la demande à chaque lecture
  (une boucle relit donc l'index courant). **Nœuds d'action** : exécutés par le flux ; leurs sorties restent
  lisibles après exécution.
- Validation avant exécution : types inconnus, liens invalides, entrées requises, cycles de données.
- Garde-fous : 100 000 étapes, 100 000 itérations par boucle ; erreur = nœud fautif surligné, exécution arrêtée.
- **Profileur** : exécutions et temps exclusif par nœud (affichés sous chaque nœud après exécution).
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
| Texte / Débogage | Concaténer, Afficher |
| Projet (Modèle, Charges, Requêtes, Analyse, Résultats) | **une commande du registre central = un nœud** (`cmd.<id>`), généré automatiquement — dont `analysis.run` (Calculer) et `results.summary` / `results.node_displacement` |
| TSALab | Banc de validation (cœur scientifique) |

Ajouter une commande au registre (`src/Automation/CommandRegistry.cpp`) la rend disponible dans le Blueprint, la
console et (à terme) l'IA, sans autre code. Ajouter un nœud : `NodeLibrary::add(définition, exécuteur)` dans
`registerStandardNodes` (ou une bibliothèque de plugin).

## Fichier `.tsbp`

JSON versionné (`"format": "tsbp"`, `"version": 1`) : nom, description, nœuds (id, type, position, valeurs saisies
typées), liens (`from`/`out` → `to`/`in`). Une version future est refusée ; un type de nœud inconnu est chargé puis
signalé à la validation (plugins absents).

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
