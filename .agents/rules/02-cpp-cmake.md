---
title: C++ & CMake
scope: repo
applies_to: ["src/**", "tests/**", "CMakeLists.txt", "cmake/**", "CMakePresets.json"]
---

# 02 — C++ & CMake

## Style C++ attendu

- C++20 (confirmé par `DOCUMENTATION.md`, section compatibilité toolchains).
- RAII : les ressources OCCT (`Handle(...)`) et Qt (parenté `QObject`) doivent être gérées
  par leur mécanisme de possession natif ; éviter les `new`/`delete` manuels quand
  `Handle`, `std::unique_ptr` ou la parenté Qt suffisent.
- `const correctness` : les méthodes de lecture (`name()`, `section()`, `material()`,
  `area()`, etc. sur `Element`/`LinearElement`/`SurfaceElement`) doivent rester `const`.
- Ownership clair : `TSA::Model::Model` possède les éléments (`Beam`, `Column`, `Slab`,
  `Wall`, `Foundation`, `TrussMember`, `Cable`, `Node`) ; `UndoRedo::CommandManager` possède
  les `std::unique_ptr<ICommand>` en attente d'annulation/rétablissement ; ne pas dupliquer
  la possession ailleurs (pas de pointeur brut qui prétend posséder un objet déjà possédé).
- Headers : suivre la convention déjà en place (`#pragma once`, forward declarations pour
  limiter les inclusions croisées, ex. `class Model;` `struct Section;` dans `Element.h`).

## CMake

- Le projet utilise `CMakeLists.txt` à la racine + `CMakePresets.json` + scripts
  d'assistance (`scripts/setup_build.ps1`, `scripts/detect_compiler.ps1`,
  `scripts/detect_qt.ps1`, `scripts/install_mingw.ps1`).
- Cible principale : `add_executable(${PROJECT_NAME} ${SOURCES})`.
- Cible de tests : `TSA_Tests` (voir `enable_testing()`, `add_test(NAME
  CoordinatesAndLevelsTest COMMAND TSA_Tests)`).
- Ne pas introduire de nouvelle dépendance externe dans `CMakeLists.txt` /
  `cmake/SetupDependencies.cmake` sans avoir suivi le protocole de recherche préalable
  (voir la section « Directives de Recherche Préalable » dans `AGENTS.md`).
- Toute nouvelle source doit être ajoutée à la liste `SOURCES`/aux listes de fichiers de
  test existantes plutôt que compilée séparément.

## Debug / Release, Visual Studio / MinGW

- Les deux toolchains (MSVC et MinGW-w64) doivent rester compilables ; ne pas utiliser
  d'extension spécifique à un seul compilateur sans vérifier la compatibilité de l'autre.
- Les scripts PowerShell (`scripts/*.ps1`) gèrent la détection et la bascule de toolchain —
  les réutiliser plutôt que documenter une procédure manuelle parallèle.
- Respecter les options de compilation déjà en place pour `TSA_Tests`
  (`/W4 /permissive- /utf-8`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN` sous MSVC) pour toute
  nouvelle cible.

## Vérification

`TODO: VERIFY IN SOURCE` pour toute option de compilation ou dépendance non listée ici —
se référer à `CMakeLists.txt`, `cmake/SetupDependencies.cmake` et
`cmake/DeployDependencies.cmake` avant de généraliser.
