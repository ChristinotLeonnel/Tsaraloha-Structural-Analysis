---
name: analyze-project
description: Analyser le projet TSA avant toute modification significative — recherche de l'existant, compréhension, traçage des dépendances, identification de la source de vérité.
---

# analyze-project

Objectif : analyser le projet avant modification, pour éviter de dupliquer ce qui existe
déjà et pour comprendre où se situe réellement la source de vérité pour la tâche demandée.

## Procédure

```text
SEARCH
→ UNDERSTAND
→ TRACE DEPENDENCIES
→ IDENTIFY SOURCE OF TRUTH
→ CHECK MODULARITY & RESPONSIBILITY
→ PLAN
```

### 1. SEARCH

- Chercher par nom métier (français et anglais si pertinent) dans `src/` : classes,
  fonctions, fichiers déjà liés au sujet (`SEARCH EXISTING CODE`).
- Vérifier si une classe équivalente existe déjà (`CHECK WHETHER AN EQUIVALENT CLASS ALREADY EXISTS`).
- Vérifier si un système existant peut être étendu (`CHECK WHETHER AN EXISTING SYSTEM CAN BE EXTENDED`).
- Vérifier aussi `docs/`, `AGENTS.md`, `.agents/rules/` pour du contexte déjà documenté.
- Ne pas se limiter au dossier qui semble évident : un sujet « section » touche
  `src/Model/Section.*`, `src/UI/Properties`, `src/ExtensionSystem` (TSALib) et
  potentiellement `src/Geometry`.

### 2. UNDERSTAND

- Lire l'implémentation (`.cpp`), pas seulement la déclaration (`.h`).
- Identifier les invariants déjà imposés par le code (ex. un `LinearElement` a toujours une
  `Section` et un `Material`).

### 3. TRACE DEPENDENCIES

- Qui appelle cette classe/fonction ? Qui l'observe (`IModelObserver`) ? Qui la sérialise
  (`src/IO`) ?
- Vérifier les deux sens de synchronisation (`.agents/rules/06-synchronization.md`).

### 4. IDENTIFY SOURCE OF TRUTH

- Confirmer que la propriété/donnée concernée vit bien dans `src/Model` et non dans un
  widget UI ou une variable de rendu OCCT.
- Si la source de vérité semble être ailleurs, c'est un signal d'anomalie architecturale à
  signaler avant de construire dessus (voir agent `architecture-reviewer`).

### 5. CHECK MODULARITY & RESPONSIBILITY

- Vérifier que la classe/fonction cible a une responsabilité unique clairement délimitée.
- Comparer la taille du fichier à l'échelle de surveillance :
  - `< 300` : confortable
  - `300–600` : normale
  - `600–1 000` : surveiller la responsabilité
  - `> 1 000` : analyser opportunité de découpage
  - `> 2 000` : refactorisation à envisager
  - `> 5 000` : analyse architecturale obligatoire
- Si un découpage est envisagé, appliquer le protocole en 9 points (responsabilité, dépendances,
  appels entrants/sortants, signaux Qt, OCCT, tests, CMake, risque de régression).

### 6. PLAN

- Écrire un plan court avant d'implémenter : fichiers à modifier, fichiers à créer, tests
  à ajouter/adapter, impact sur la synchronisation UI/Model/Geometry/3D.
- Ne pas passer à l'implémentation avant d'avoir un plan explicite pour toute tâche non
  triviale.

## Sortie attendue

Un résumé court : ce qui existe déjà, ce qui manque réellement, et le plan d'implémentation
proposé — avant d'écrire le moindre code.
