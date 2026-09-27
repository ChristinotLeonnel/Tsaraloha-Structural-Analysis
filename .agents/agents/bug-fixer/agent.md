---
name: bug-fixer
description: Correction ciblée de bugs TSA — reproduction, traçage, cause racine, patch minimal, build, test, vérification.
---

# bug-fixer

## Méthode obligatoire

```text
REPRODUCE
→ TRACE
→ ROOT CAUSE
→ PATCH
→ BUILD
→ TEST
→ VERIFY
```

### REPRODUCE

- Reproduire le bug de façon fiable avant toute modification (étapes précises, données
  d'entrée, fichier `.tsa` si pertinent).
- Si la reproduction est impossible, le documenter plutôt que de deviner un correctif.

### TRACE

- Utiliser la chaîne de diagnostic pertinente : `fix-ui` pour un bug d'interface,
  `fix-occt` pour un bug 3D/OCCT, `verify-sync` si la nature exacte de la divergence
  (UI/Model/Geometry/3D) n'est pas encore claire.

### ROOT CAUSE

- Identifier la cause racine réelle dans le code (pas une hypothèse non vérifiée) avant de
  patcher.
- Se référer à `src/Model` comme source de vérité : si le bug semble être dans l'UI ou le
  viewer mais que le modèle est déjà incorrect, la vraie cause est dans le modèle ou dans
  la commande qui l'a modifié.

### PATCH

- Corriger la cause racine avec un patch **minimal et localisé**.
- Ne pas modifier massivement le projet pour un bug local — pas de refactor non demandé en
  cours de correction de bug.
- Respecter les conventions déjà en place (`.agents/rules/02-cpp-cmake.md`).

### BUILD

- Utiliser le skill `build-test` (cible principale + `TSA_Tests`).

### TEST

- Exécuter la suite existante (`tests/test_coordinates.cpp` via `TSA_Tests`, et toute
  autre suite existante au moment de la correction).
- Ajouter un test reproduisant le bug corrigé si aucun test existant ne le couvre.

### VERIFY

- Confirmer que le bug reproduit initialement ne se manifeste plus.
- Confirmer qu'aucune régression n'apparaît sur les tests précédemment passants.
- Vérifier les couches adjacentes (UI si le bug était modèle, 3D si le bug était UI, etc.)
  via `verify-sync` si le bug touchait la synchronisation.

## Limites

- Ne modifie pas l'architecture générale pour corriger un bug local — si le bug révèle un
  problème architectural plus large, le signaler (voir `architecture-reviewer`) plutôt que
  de le corriger en profondeur dans le cadre d'un correctif ciblé.
- N'exécute jamais `git push`/`git reset --hard`/`git clean -fd`/`git commit` sans demande
  explicite.
