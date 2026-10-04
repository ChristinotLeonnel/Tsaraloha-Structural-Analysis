# Changelog

Les sessions futures ajoutent une entrée datée en tête (plus récent d'abord).

## 2026-10-04

### Added

- Symétrie / copie miroir (`Model::mirrorElements`), division de poutres et poteaux en N tronçons
  (`Model::splitBeam` / `splitColumn`), fusion des nœuds confondus (`Model::findCoincidentNodes` /
  `mergeCoincidentNodes`) — nouveau fichier [CORE] `src/Model/Model_Topology.cpp`.
- UI : menu Édition + panneau ruban « Symétrie & Topologie », commandes console MIRROR/SPLIT/MERGE,
  entrées `cmd.modify.mirror|split_bars|merge_nodes` (sans raccourci) ; chaque opération = 1 EditTransaction.

### Changed

- ModelValidator : détection des nœuds coïncidents en O(N log N) (hachage spatial) au lieu de O(N²) ;
  avertissement « dalles/voiles non pris en compte par le calcul ».
- Calcul statique/modal/pushover : confirmation si le modèle contient des dalles/voiles (BUG-002).
- « Maillage EF » : présenté comme une estimation (affirmait à tort avoir généré un maillage).

### Tests

- Ajoutés : 101 (symétrie), 102 (division), 103 (fusion de nœuds), 104 (Undo transactionnel) — 125/125.

### Added (extraction OpenSees, même jour)

- `OpenSeesModelMap` (tags uniques, axes OpenSees, ressorts, dispositions des recorders), `ElementTransformation`
  (formules exactes LinearCrdTransf3d), `AnalysisTypes` (ElementKey, DenseMatrix, SparseMatrix, MatrixMetadata,
  DofMap, UnitSystem), `ResultsExport` (CSV/JSON/TXT), `ResultsContext` (JSON pour le Co-Engineering).
- Mode ADVANCED : passage matrices séparé (nodeDOFs, basicStiffness, printA -ret FullGeneral), forces
  local/global/basic. Dock « Données d'analyse (OpenSees) » ; choix LIGHT/ADVANCED dans la configuration.
- docs/OPENSEES_RESULTS.md.

### Fixed (calcul)

- FIX-015 à FIX-023 (known-issues) : collision d'ids poutre/poteau, syntaxe truss, lecture décalée des efforts,
  précision du script, réactions des ressorts, unités de l'équilibre, famille des charges sur barre, export Tcl
  parallèle, faux message de maillage.

### Tests

- 105–111 (suite `extraction`) contre le binaire OpenSees 3.8.0 — 132/132.

## 2026-10-03

### Analysis

- Initial architecture audit completed ; mémoire technique `.claude/` créée (main @ 81e34cf).
- Build de référence : PASS (ninja-debug, MSVC 19.51, Qt 6.11.2, OCCT 8.0.1) ; 2 avertissements C4996 ; tests 121/121.
- Nouveaux constats documentés : calcul synchrone (BUG-001), dalles/voiles non calculés (BUG-002),
  commandes d'isolation non branchées (BUG-004), chunks SETT/RSLT non utilisés (BUG-013).

### Changes (même journée, avant l'initialisation de la mémoire)

- `fix/audit-stability-performance` (merge 5ed3cf9) : crash au démarrage (boucle de signaux WorkPlane),
  crash à la fermeture (observateurs), dépendances Ninja perdues (avertissement + VSLANG),
  persistance des charges (.tsa 1.1, chunk LOAD), sauvegarde atomique (QSaveFile), CRC32 par table,
  affichage des nœuds/fondations, isolation 2D robuste, marqueur d'accrochage inerte, NDC différée.
- `feature/edit-system` (merge 81e34cf) : Model::revision + ResultsValidityGuard, UndoManager
  transactionnel (EditTransaction, EditRecord, coalescence, budget mémoire), CommandManager
  transactionnel, édition de propriétés fiable, SelectionQuery + selectElements, tolérance
  centralisée, règle des niveaux, docs/EDIT_SYSTEM.md.

### Fixed

- FIX-001 à FIX-014 (détail dans known-issues.md).

### Performance (Debug)

- Ouverture 1 408 barres : 58,8 s → 5,0 s ; 4 896 barres : > 78 s → 16,3 s.
- Ctrl+A sur 4 896 barres : 154 ms.
- Édition : snapshot 10 ms, Undo 54 ms, transaction 1 728 poteaux 21 ms (test 100).

### Tests

- Ajoutés : 90 (stress .tsa), 91 (charges + compat 1.0 + écriture atomique), 92 (CRC32),
  93–94 (géométrie OCCT), 95 (invalidation résultats), 96 (transactions), 97 (coalescence,
  validation), 98 (requêtes de sélection), 99 (niveaux), 100 (mesures + budget mémoire).
- 37.4 rendu indépendant du remplissage du tampon de logs.
