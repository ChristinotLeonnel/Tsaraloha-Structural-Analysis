---
title: Git & Tests
scope: repo
applies_to: ["**"]
---

# 07 — Git & Testing

## Git — Gestion Automatique des Branches et Workflow

Pour toute modification du projet TSA, déterminer d'abord la nature du travail avant de modifier le code.
**Principe : NE PAS effectuer automatiquement toutes les modifications dans la branche courante.**

### 1. Conventions de Branches

| Préfixe | Usage | Exemples |
| :--- | :--- | :--- |
| `feature/<nom-court>` | Nouvelle fonctionnalité | `feature/workplane-3d`, `feature/cable-element`, `feature/result-diagrams` |
| `fix/<nom-court>` | Correction de bug existant | `fix/workplane-selection`, `fix/circular-column-display`, `fix/grid-crash` |
| `refactor/<nom-court>` | Refactorisation sans ajout fonctionnel direct | `refactor/coordinate-system`, `refactor/occview-split` |
| `perf/<nom-court>` | Amélioration de performance | `perf/occt-scene-update`, `perf/spatial-indexing` |
| `ui/<nom-court>` | Modification visuelle ou ergonomique | `ui/workplane-marker`, `ui/toolbar-elements` |
| `docs/<nom-court>` | Modification uniquement documentaire / règles | `docs/git-branching-rules`, `docs/workplane-system` |
| `test/<nom-court>` | Ajout ou refonte dédiée aux tests | `test/modular-test-suites` |

### 2. Modifications Communes / Transversales vs Indépendantes

- **Modification commune / transversale liée :**
  Si la modification est très petite, transversale, nécessaire à la tâche actuelle, directement liée à la branche courante, ou constitue une correction mineure indispensable à la fonctionnalité en cours → **rester dans la branche actuelle**.
  *Exemple :* Sur `feature/workplane-3d`, corriger une petite synchronisation du WorkPlane reste sur `feature/workplane-3d`.
- **Modification indépendante :**
  Si une modification n'est pas nécessaire à la tâche actuelle, concerne une autre fonctionnalité ou amélioration indépendante, ou risque de mélanger les livraisons → **NE PAS** l'implémenter dans la branche courante. Créer une branche dédiée (ex. `fix/cable-section-display`).

### 3. Protocole Avant Création d'une Branche

1. Toujours vérifier :
   ```bash
   git status
   git branch --show-current
   ```
2. Identifier la branche actuelle et déterminer si la modification appartient réellement à la tâche courante.
3. Ne jamais créer de branches inutilement.
4. **Branche de base :** Partir de la branche appropriée et à jour (ex. `main`).
   ```bash
   git switch main
   git pull
   git switch -c feature/<nom>
   ```

### 4. Préservation du Travail Existant (Règles Absolues)

- Ne **jamais** supprimer les modifications locales de l'utilisateur.
- Ne **jamais** faire de reset destructif (`git reset --hard` sans demande explicite).
- Ne **jamais** utiliser `git clean -fd` sans autorisation explicite.
- Ne **jamais** écraser des commits existants ou forcer avec `git push --force`.
- Ne **jamais** changer de branche avec des modifications non sauvegardées sans vérifier les conséquences.

### 5. Conventions de Commit

Format des messages de commit :
```text
feat: ...
fix: ...
refactor: ...
perf: ...
ui: ...
docs: ...
test: ...
```

### 6. Push et Fusion

- Pousser la branche de travail correspondante vers le remote après validation (`git push -u origin feature/<nom>`).
- Ne pas pousser automatiquement vers `main`.
- Ne fusionner dans `main` qu'après validation complète (compilation 0 erreur et suite de tests 100% PASS).

### 7. Checklist Obligatoire Avant Toute Modification Importante

Identifier explicitement :
- **TYPE :** `feature` / `fix` / `refactor` / `perf` / `ui` / `docs` / `test`
- **BRANCHE :** branche actuelle ou nouvelle branche
- **JUSTIFICATION :** pourquoi cette modification appartient à cette branche.

## Build

- Cible principale : `${PROJECT_NAME}` (voir `add_executable(${PROJECT_NAME} ${SOURCES})`
  dans `CMakeLists.txt`).
- Cible de tests : `TSA_Tests` (nom de sortie `TSA_TestSuite`).
- Utiliser en priorité les scripts existants (`scripts/setup_build.ps1`,
  `run.bat`, CMakePresets) plutôt que reconstruire une séquence de configuration manuelle,
  sauf besoin spécifique de diagnostic.

## Tests

- Suite existante : `tests/test_coordinates.cpp`, exécutée via la cible `TSA_Tests`
  (`add_test(NAME CoordinatesAndLevelsTest COMMAND TSA_Tests)`), état de référence actuel :
  **52/52 tests PASS** (exécutable `build/Release/TSA_TestSuite.exe`).
- Toute modification du modèle, des coordonnées, des grilles, de la persistance ou d'un
  refactoring doit obligatoirement préserver le passage à 100% de cette suite (52/52 PASS).
- **Règle absolue sur les tests :** Ne jamais commenter, désactiver ou affaiblir un test
  existant pour faire passer une modification ou un refactoring. Corriger le code jusqu'à
  satisfaction complète du banc d'essais.
- Si une fonctionnalité nouvelle n'a pas de test correspondant, l'ajouter systématiquement
  dans `tests/`.

## Régressions

Après modification :

1. compiler (Debug **et** Release si le changement touche un chemin sensible aux
   optimisations, sinon Debug suffit pour une itération rapide) ;
2. exécuter `TSA_Tests` ;
3. vérifier qu'aucun test précédemment PASS ne devient FAIL ;
4. vérifier manuellement (ou via test dédié) le comportement UI concerné ;
5. vérifier la cohérence modèle ↔ géométrie ↔ 3D (voir skill `verify-sync`).

## Validation finale

Ne considérer une tâche « terminée » qu'après compilation réussie et tests passants — ne
jamais annoncer un correctif comme validé sur la seule base d'une relecture de code.
