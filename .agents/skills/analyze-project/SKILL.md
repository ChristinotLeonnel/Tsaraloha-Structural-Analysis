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
→ PLAN
```

### 1. SEARCH

- Chercher par nom métier (français et anglais si pertinent) dans `src/` : classes,
  fonctions, fichiers déjà liés au sujet.
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

### 5. PLAN

- Écrire un plan court avant d'implémenter : fichiers à modifier, fichiers à créer, tests
  à ajouter/adapter, impact sur la synchronisation UI/Model/Geometry/3D.
- Ne pas passer à l'implémentation avant d'avoir un plan explicite pour toute tâche non
  triviale.

## Sortie attendue

Un résumé court : ce qui existe déjà, ce qui manque réellement, et le plan d'implémentation
proposé — avant d'écrire le moindre code.
