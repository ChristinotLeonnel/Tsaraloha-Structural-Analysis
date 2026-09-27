---
title: Git & Tests
scope: repo
applies_to: ["**"]
---

# 07 — Git & Testing

## Git

- Ne **jamais** exécuter automatiquement, sans demande explicite de l'utilisateur pour
  cette action précise :
  - `git push`
  - `git reset --hard`
  - `git clean -fd`
  - `git commit`
- Les commandes de lecture (`git status`, `git log`, `git diff`, `git show`) sont
  autorisées librement pour l'analyse.
- Ne jamais supprimer un fichier existant du dépôt sans demande explicite.

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
