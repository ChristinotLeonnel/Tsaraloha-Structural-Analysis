# TSA — Tsaraloha Structural Analysis

Mémoire technique persistante pour Claude Code. Initialisée le 2026-10-03 sur `main` @ `81e34cf`.
Les règles détaillées du projet restent dans `AGENTS.md` (racine) et `.agents/` : ce fichier les
complète, il ne les remplace pas. En cas de divergence, **le code réel fait foi** : corriger
alors cette documentation.

## Project Identity

Application desktop Windows de modélisation et d'analyse de structures de génie civil (poutres,
poteaux, dalles, voiles, fondations, treillis, câbles), viewport 3D OCCT, calcul via OpenSees
(processus externe), note de calcul (NDC) Eurocodes.

Technologies constatées :
- C++20 (`CMAKE_CXX_STANDARD 20`), MSVC 19.51 (Visual Studio 18 / 2026 Community)
- Qt 6.11.2 (`C:/Qt/6.11.2/msvc2022_64`) : Core, Gui, Widgets, Svg
- Open CASCADE Technology 8.0.1 (`opencascade-8.0.1-vc14-64/`)
- CMake ≥ 3.20 (installé : 4.3.3), Ninja (presets `ninja-debug` / `ninja-release`)
- OpenSees (exécutable externe piloté par `QProcess`, scripts Tcl générés)

## Architecture (résumé)

```text
UI (src/UI)  →  Commands / UndoRedo (src/Commands, src/UndoRedo)  →  Model (src/Model, source de vérité)
                                                                       ├→ Geometry (src/Geometry) → Viewer OCCT (src/Viewer)
                                                                       ├→ IO .tsa (src/IO)
                                                                       └→ Analysis multi-moteurs (src/Analysis/Engine : AnalysisManager → OpenSees | Custom2D) → Results → NDC (src/NDC)
Transversal : Coordinate (niveaux, WorkPlanes), Grid, ExtensionSystem (TSALib), Diagnostics, Standards
```
Synchronisation modèle → vues : `TSA::Model::IModelObserver` (OccView, ModelTreeWidget,
PropertyPanel, ProjectStatusOverlay, ResultsValidityGuard). Détails : `architecture.md`.

## Important Rules

- Travailler avec l'architecture existante ; pas d'architecture ni de système parallèle
  (un seul SelectionManager, un seul UndoManager, un seul viewport `OccView`).
- Chercher la classe / fonction existante avant d'en créer une (Grep sur `src/`).
- Toute modification du modèle : `Model::pushUndoState(nom, cléCoalescence)` ou
  `TSA::UndoRedo::EditTransaction`, puis `notify*Modified` / `ModelDiff` — jamais de mutation
  silencieuse (les vues et l'invalidation des résultats en dépendent).
- Préserver la compatibilité `.tsa` (chunks FourCC, version mineure pour un ajout non cassant,
  voir `docs/TSA_FILE_FORMAT.md`).
- Ne jamais manipuler widgets Qt ni contexte OCCT hors du thread UI.
- Ne pas supprimer du code sans vérifier ses appelants (Grep) et CMake (`CORE_SOURCES` vs `SOURCES`).
- Après une modification d'en-tête partagé, vérifier que Ninja a des dépendances (voir
  known-issues BUG-011) ; compiler puis exécuter `TSA_TestSuite.exe` (attendu : tous PASS).
- Raccourcis : enregistrer dans `src/Commands/CommandCatalog.cpp`, mettre à jour
  `docs/shortcuts.txt`, vérifier `python tools/check_shortcuts.py --strict` ET l'absence de
  conflit dans `src/UI/*.cpp` (le script ne lit que le catalogue).
- Git : branche dédiée (`feature/`, `fix/`, `perf/`, `docs/`…), commits `type: message`,
  ne jamais pousser sur `main` sans demande explicite.

## Build & test (référence)

```powershell
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && cmake --preset ninja-debug && cmake --build --preset ninja-debug -- -k 0 -j 4"
build-ninja-debug\TSA_TestSuite.exe            # toutes les suites ; --suite=io|model|commands|viewer...
```
- `-j 4` : au-delà, erreurs transitoires « mémoire virtuelle pour PCH » (C3859) sur ce poste.
- `LNK1168 impossible d'ouvrir TSA.exe` : un clone WER d'un ancien crash verrouille l'exe ;
  renommer `TSA.exe` en `TSA_running_<hhmmss>.exe` puis relier (les clones disparaissent au
  redémarrage de Windows).
- Les logs INFO ne sont vidés qu'en fermeture propre (`build-ninja-debug/logs/sessions/`).

## Current Architecture
→ `architecture.md`

## Current State
→ `current-state.md`

## Known Issues
→ `known-issues.md`

## Architecture Decisions
→ `decisions.md`

## Current Tasks
→ `tasks.md`

## Change History
→ `changelog.md`

Documentation projet existante (ne pas dupliquer) : `docs/ARCHITECTURE.md`, `docs/EDIT_SYSTEM.md`,
`docs/TSA_FILE_FORMAT.md`, `docs/MODEL.md`, `docs/OCCT.md`, `docs/UI.md`, `docs/shortcuts.txt`,
`docs/ANALYSIS_ENGINES.md` (moteurs d'analyse : ajouter un moteur sans toucher à l'UI).

## FUTURE SESSION RULE

DO NOT perform a full project audit for every task.

Before starting a task:

1. Read .claude/CLAUDE.md
2. Read .claude/current-state.md
3. Read .claude/known-issues.md when relevant
4. Read .claude/decisions.md when architectural decisions are involved
5. Inspect only the files/subsystems relevant to the requested task
6. Verify dependencies directly connected to those files
7. Modify the code
8. Build/test
9. Update the .claude documentation

A full architectural audit is only allowed when:

- explicitly requested;
- the architecture has significantly changed;
- documentation is outdated;
- a severe unexplained problem requires it.

## TASK WORKFLOW

For each new task:

```text
READ MEMORY
↓
UNDERSTAND TASK
↓
IDENTIFY RELEVANT SUBSYSTEM
↓
TARGETED INSPECTION
↓
IMPLEMENT
↓
BUILD
↓
TEST
↓
UPDATE MEMORY
```

Do NOT:

```text
New task
↓
Full project audit
↓
Read entire repository
↓
Start working
```

## Memory maintenance

After each significant task, update `current-state.md`, `known-issues.md`, `tasks.md`,
`changelog.md`. Update `architecture.md` / `decisions.md` only after a real architectural
change. Never invent a class or feature, never mark a bug FIXED without a test or a
reproduced verification, never mark a feature IMPLEMENTED without evidence in the code.
