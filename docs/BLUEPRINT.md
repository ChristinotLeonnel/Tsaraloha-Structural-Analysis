# Blueprint — programmation visuelle de l'écosystème Tsaraloha

Statut : phase 6 de `docs/TSARALOHA_ARCHITECTURE.md` (ADR-024), 2026-10-08. Moteur partagé (TSA et TSALab),
éditeur partagé, affiché dans l'espace « Blueprint » de TSALab.

## Où est le code

| Partie | Fichiers | Couche |
| :--- | :--- | :--- |
| Types, graphe | `src/Blueprint/BlueprintTypes.h`, `BlueprintGraph.*` | modèle (C++ standard) |
| Bibliothèque, exécution, profil | `src/Blueprint/BlueprintRuntime.*`, `BlueprintNodes.cpp` | modèle |
| Fichier `.tsbp` | `src/Blueprint/BlueprintFile.*` | modèle (Qt Core / JSON) |
| Éditeur | `src/UI/Blueprint/BlueprintScene.*`, `BlueprintEditor.*` | widgets |
| Exemples | `TSALab/lab/Research/Examples/BlueprintExamples.*` | application TSALab |
| Tests | `tests/test_blueprint.cpp` (193–195), `TSALab/lab/Tests` (L6) | |

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

## Limites actuelles (à venir : phases 7–8)

Pas de débogueur pas à pas (points d'arrêt), pas d'Annuler dans l'éditeur lui-même (l'exécution, elle, passe par
l'historique du projet), pas d'exécution hors du thread de l'interface. Le calcul (`analysis.run`) est synchrone dans
un Blueprint (contrôleur partagé, `runBlocking`).
