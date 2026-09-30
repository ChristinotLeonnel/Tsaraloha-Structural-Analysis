# Architecture — TSA

> Document vivant. Toute divergence avec le code doit être corrigée en faveur du code, puis
> ce document mis à jour. Voir aussi `AGENTS.md` et `.agents/rules/01-architecture.md`.

## Vue d'ensemble

TSA est une application desktop C++/Qt6/OCCT de modélisation et d'analyse de structures de
génie civil. L'architecture réelle constatée dans `src/` :

```text
UI (src/UI : Ribbon, Dock, Properties, ModelTree, PortArea, Diagrams)
  ↓ signals/slots, Commands
Commands / UndoRedo (src/Commands, src/UndoRedo)
  ↓ modifie
Structural Model (src/Model : Elements, Loads, Combinations, Sections, Materials)
  ├── construit via → Geometry (src/Geometry : Elements, Deformed, Diagrams)
  │                     ↓ affichée par
  │                   OCCT / Viewer (src/Viewer : OccView, ResultsVisualManager)
  └── résolu via → Analysis (src/Analysis : CalculationSnapshot, OpenSeesSolver)
                      ↓ produit
                    Results (ResultsModel)
                      ↓ exporté via
                    Note de Calcul (src/NDC : NDCGenerator, NDCExporter)
```

## Modules

| Module | Rôle | Document dédié |
|---|---|---|
| `src/Model` | Source de vérité métier (éléments, sections, matériaux, nœuds, charges) | `MODEL.md` |
| `src/Geometry` | Construction des `TopoDS_Shape` depuis le modèle et les résultats | `OCCT.md` |
| `src/Viewer` | Viewer OCCT (AIS/V3d), sélection 3D, apparence, visualiseur de résultats | `OCCT.md` |
| `src/UI` | Ribbon, Dock, Properties, ModelTree, Dialogs, Widgets, Theme, Ruler, Port, Diagrams | `UI.md` |
| `src/Analysis` | Intégration OpenSees, snapshots immuables, solveur asynchrone, ResultsModel | `ANALYSIS_OPENSEES.md` |
| `src/NDC` | Générateur & visualiseur de Note de Calcul (Eurocodes, IEEE 1063, PDF/HTML) | `NDC_SYSTEM.md` |
| `src/Commands` | Command Pattern (`ICommand`) pour toute modification du modèle | `MODEL.md` |
| `src/UndoRedo` | `CommandManager`, `UndoManager` (undo/redo par snapshot d'état) | `MODEL.md` |
| `src/Coordinate` | `CoordinateSystem`, `Point3D`, `LevelManager`/`Level`, coordonnées cylindriques | `COORDINATES.md` |
| `src/Grid` | Grilles 3D paramétriques, accrochage (snap), rendu de grille | `TODO: VERIFY IN SOURCE` |
| `src/ExtensionSystem` | Système d'extensions dynamique TSALib (sections/matériaux) | `docs/TSALIB_SYSTEM.md` (existant) |
| `src/IO` | Sérialisation du format fichier `.tsa` | `docs/TSA_FILE_FORMAT.md` (existant) |
| `src/Diagnostics` | Diagnostics/télémétrie internes | `docs/TSA_DIAGNOSTICS.md` (existant) |
| `src/Interaction` | Interactions utilisateur dans le viewport 3D | `TODO: VERIFY IN SOURCE` |
| `src/Project`, `src/App`, `src/main.cpp` | Bootstrap de l'application | `TODO: VERIFY IN SOURCE` |
| `Extensions/TSALib` | Bibliothèque(s) d'extension packagées | `docs/TSALIB_SYSTEM.md` (existant) |

## Principes directeurs

1. **Le modèle est la source de vérité** — jamais l'UI, jamais une variable graphique,
   jamais une `Shape` OCCT (voir `MODEL.md`).
2. **Réutiliser avant de créer** — rechercher l'existant dans `src/` avant toute nouvelle
   classe/abstraction.
3. **Synchronisation bidirectionnelle** — `UI → Model → Geometry → 3D` et
   `Sélection 3D → Model → UI`, via `TSA::Model::IModelObserver` (voir `MODEL.md` et
   `.agents/rules/06-synchronization.md`).
4. **Pas de dépendance inversée** — `src/Model` ne dépend jamais de `src/UI` ni de
   `src/Viewer`.

## Documentation existante à ne pas dupliquer

Ce dépôt contenait déjà, avant la mise en place de ce framework :
`DOCUMENTATION.md` (manuel utilisateur et technique complet), `README.md` (build/lancement/
tests), `docs/TSALIB_SYSTEM.md`, `docs/TSA_DIAGNOSTICS.md`, `docs/TSA_FILE_FORMAT.md`,
`docs/cable-system/*` (système de câbles détaillé). Les fichiers `docs/ARCHITECTURE.md`,
`MODEL.md`, `UI.md`, `OCCT.md`, `COORDINATES.md`, `SECTIONS.md`, `MATERIALS.md`,
`ROADMAP.md` complètent cet ensemble avec une vue orientée agents IA/architecture — ils ne
remplacent aucun de ces documents existants.

## Normes Internationales & Documentation

Le développement et la documentation de TSA respectent le cadre normatif international :
- **IEEE Std 1063-2001 (R2007)** et **ISO/IEC/IEEE 26514:2022** : Structure formelle, métadonnées, clarté et complétude des documents utilisateurs (voir `docs/USER_MANUAL_SHORTCUTS.md` et `docs/shortcuts.txt`).
- **ISO/IEC/IEEE 12207:2017 & 29148:2018** : Processus ordonné du cycle de vie logiciel et traçabilité stricte des exigences.
- **ISO/IEC/IEEE 29119** : Validation continue par la suite de tests automatisée (`tests/`).
- **ISO/IEC 25010:2023** : Préservation des attributs qualité (fiabilité, maintenabilité, performance, adéquation fonctionnelle).
- **Eurocodes (EN 1990 à EN 1999)** : Normes de référence de calcul des structures en génie civil (repère orthonormé direct, unités SI).

Voir `.agents/rules/08-documentation-and-standards.md` pour le détail des règles obligatoires.

## Roadmap

Voir `ROADMAP.md`.
