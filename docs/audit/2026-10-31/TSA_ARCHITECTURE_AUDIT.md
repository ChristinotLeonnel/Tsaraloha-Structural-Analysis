# Audit d'architecture

## Conclusion

L'architecture réelle est **meilleure que ce que suggère sa cartographie brute** : aucun cycle entre en-têtes, modèle sans widget, historique unifié, calcul sur instantané immuable, noyau `TSA_Core` isolé et prouvé à l'édition de liens.

Trois faiblesses sont réelles : le contrôle des couches ne voit qu'une partie des dépendances, `MainWindow` et `OccView` concentrent trop de responsabilités, et deux règles métier existent en double, ce qui a déjà produit des défauts (BUG-149, BUG-152).

## Cartographie

| Module (`src/`) | Lignes | Fichiers | Rôle |
|---|---:|---:|---|
| UI | 60 468 | 225 | Fenêtre, ruban, docks, propriétés, dialogues, Blueprint |
| Viewer | 16 674 | 38 | Vue OCCT `OccView`, sélection, rendu des résultats |
| Model | 13 987 | 79 | Modèle, éléments, charges, câbles, sections |
| Analysis | 12 435 | 57 | Moteurs, instantané de calcul, OpenSees, résultats |
| BIM | 6 749 | 33 | Couche BIM, import et export IFC |
| NDC | 6 421 | 26 | Notes de calcul |
| AI | 6 360 | 36 | Co-ingénierie, fournisseurs, outils |
| Grid | 6 324 | 33 | Grilles, accrochage |
| Mesh | 6 288 | 20 | Maillage Gmsh, adaptation OpenSees |
| IO | 5 605 | 15 | Format `.tsa`, TSA3D |
| ExtensionSystem | 4 233 | 22 | Bibliothèques TSALib |
| Blueprint | 3 603 | 12 | Graphes exécutables |
| Geometry | 3 543 | 20 | Formes B-Rep |
| Interaction | 3 488 | 13 | Outils de modification |
| Coordinate | 3 020 | 21 | Repères, niveaux, plans de travail |
| Standards | 3 018 | 17 | Eurocodes, validation du modèle |
| Autres (20 modules) | 17 354 | 96 | Commandes, automatisation, topologie, gabarits, diagnostics… |
| **Total** | **179 570** | **763** | |

Hors `src/` : tests (37 520 lignes, 57 suites), `../3DDesplacement` (Fem3D, facultatif à la compilation), `../TSALab/science` (obligatoire), OpenSees 3.8.0 (processus externe), Gmsh 4.15.2, IfcOpenShell 0.9.0, Qt 6.12.0, OCCT 8.0.1, Eigen 3.4.1. VTK n'est pas utilisé par TSA : `HAVE_VTK` vient seulement de la configuration d'OCCT.

### Équivalence des noms demandés

| Nom demandé | Réalité |
|---|---|
| TSA_Model | `src/Model`, classe `Model` |
| TSA_Command | `src/Commands` (catalogue, raccourcis) et `src/Automation/CommandRegistry` (automatisation) |
| TSA_Selection | `Viewer/SelectionManager`, `Model/SelectionQuery` |
| TSA_UndoRedo | `src/UndoRedo` |
| TSA_Document | `src/Project` (`ProjectSession`, `ProjectManager`) |
| TSA_View, TSA_Viewport | `Viewer/OccView`, `UI/Workspace`, `UI/Ruler/ViewportContainer` |
| TSA_Geometry | `src/Geometry` |
| TSA_Mesh | `src/Mesh` |
| TSA_Solver | `Analysis/Engines` (OpenSees, Custom2D, Fem3D), `TSALab/science`, `3DDesplacement` |
| TSA_Analysis | `Analysis/Engine` (`AnalysisManager`, portée, instantané) |
| TSA_Library | `ExtensionSystem`, `Library` |
| TSA_ImportExport | `IO`, `IO/Tsa3d`, `BIM/IFC`, `NDC` (PDF, HTML) |
| TSA_Lab | Dépôt `../TSALab`, mêmes sources par `cmake/TSAProduct.cmake` |

## Constats

### ARC-01 — HIGH : le contrôle des couches est partiel

`tools/check_layers.py` annonce 0 violation, mais il ne connaît que 4 couches et ne suit que les `#include "…"`. Toute la logique (28 répertoires) tient dans la couche « model ».

Mesuré sur toutes les inclusions de `src/` (`preuves/resultats/map.txt`) : **28 modules sur 36 forment une seule composante fortement connexe**. Exemples d'arêtes contraires à l'architecture documentée (`preuves/resultats/arch.txt`) :

| Arête | Preuve |
|---|---|
| Model → ExtensionSystem | `Model/MaterialLibrary.cpp` → `LibraryRegistry.h` |
| Model → UndoRedo | `Model/Model.cpp` → `UndoManager.h` |
| Model → BIM, Annotation | `Model/Model.h` → `BimModel.h`, `Dimension.h` |
| Analysis → AI | `OpenSeesManager.cpp` → `AI/Models/ModelManager.h` |
| Viewer → NDC | `ResultsVisualManager.cpp` → `NDC/ResultAnalyzer.h` |
| Viewer → Commands | `OccView.cpp` → `ModifyCommands.h` |
| Grid → Viewer | `GridLabelRenderer.cpp` → `AnnotationStyle.h` |
| Standards ↔ Analysis | `ModelValidator.cpp` ↔ `OpenSeesSolver.cpp` |
| IO → Standards | `TSAFile.cpp` → `ModelValidator.h` |
| Automation ↔ AI, Blueprint | `AutomationServer.cpp` ↔ `AIOperator.cpp`, `BlueprintScript.cpp` |

Ce constat est à relativiser : **aucun cycle n'existe entre en-têtes** (0 sur tout `src/`). La composante connexe vient de quelques inclusions dans des `.cpp`, donc peu coûteuses à couper. Le modèle ne dépend ni de Viewer ni d'UI.

**Action** : étendre `check_layers.py` à une matrice de dépendances entre répertoires, avec une liste d'exceptions gelée, puis réduire cette liste.

### ARC-02 — MEDIUM : classes centrales surchargées

| Classe | Méthodes | Membres | Remarque |
|---|---:|---:|---|
| `OccView` | 473 | 211 | Scène, caméra, isolation, sélection, outils, résultats |
| `MainWindow` | 296 | 334 | Plus grosse fonction du dépôt : `createDockWindows`, complexité ≈ 217 |
| `Model` | 165 | 75 | Données, observateurs, historique, topologie |
| `SelectionManager` | 139 | 55 | |

Le découpage en plusieurs `.cpp` facilite la navigation sans réduire le couplage. **Action** : extraire par responsabilité seulement lors d'une évolution de la zone, sans réécriture globale (conforme à AGENTS.md §4).

### ARC-03 — MEDIUM : règles métier en double

| Double emploi | Conséquence observée |
|---|---|
| Facteur de charge non linéaire : `OpenSeesAnalysisBuilder.cpp:905` et `OpenSeesResultsReader.cpp:553` | **BUG-149** |
| Deux notes de calcul : `NDCGenerator` (Legacy) et `NDCProjectNote` | **BUG-152** : axe fort dans l'une, axe faible dans l'autre |
| Résolution des motifs de charge : `loadPatterns` et `computeGlobalEquilibrium` | Risque d'écart, BUG-156 |
| Forces d'encastrement : `BeamElementLoad::fixedEndForces` et `equivalentNodalLoads` de Fem3D | **BUG-148** : seul Fem3D condense |
| `Material::E` et `mechanical.youngModulus`, synchronisés à la main | Aucun défaut trouvé : `syncMechanical` présent partout où c'est nécessaire |
| `algorithm` et `algorithmType`, `integrator` et `integratorType` | Alignés à la main (`OpenSeesEngine.cpp`) |

**Action** : une seule fonction pour chaque règle, partagée par le générateur et le lecteur. Retirer la note Legacy, ou la faire reposer sur le même code.

### ARC-04 — MEDIUM : frontières de validation absentes

La validation dépend du moteur : OpenSees refuse une coordonnée NaN, alors que Fem3D lève une exception avant que la validation ne se termine (BUG-155). Le lecteur `.tsa` ne vérifie pas les références (BUG-154). Il manque une validation commune, exécutée une fois, avant tout moteur et à l'ouverture.

### ARC-05 — LOW : court-circuits résiduels de l'interface

`MainWindow.cpp`, `MainWindow.h` et `MainWindow_Tools.cpp` incluent `OpenSeesSolver.h` ou `OpenSeesManager.h`. `MainWindow.cpp` inclut `IO/TSAFile.h`. Le flux principal passe bien par `AnalysisController` et `ProjectManager`.

### ARC-06 — LOW : état global

36 accesseurs `instance()`. Le plus sensible est `NationalAnnexConfig`, état global qui influence les vérifications normatives sans être lié au résultat (lié à BUG-111, déjà ouvert).

### ARC-07 — INFORMATIONAL : écart entre ADR-022 et le code

ADR-022 et `.claude/CLAUDE.md` annoncent « statique seul ». Le code contient l'analyse modale : `buildModalScript`, `AnalysisType::Modal` proposé par `AnalysisDialog`, cas `FEA-05` et `FEA-06`. Une décision reste à prendre. Ne rien retirer sans elle.

## Points forts vérifiés

| Point | Preuve |
|---|---|
| Aucun cycle d'en-têtes | `arch.txt`, 0 sur tout `src/` |
| Modèle et calcul sans widget | 0 inclusion de widget dans `Analysis`, `Model`, `Commands`, `UndoRedo` |
| Toute édition passe par `EditTransaction` | 0 `pushUndoState` dans `src/UI` |
| Calcul sur instantané immuable | `AnalysisController::start` copie `PreparedAnalysis` |
| Annulation concurrente sûre | `OpenSeesEngine::cancel` protégé, `m_stopRequested` atomique |
| Noyau isolé | Cibles `TSA_Core`, `TSA_CoreTests`, contrôle des initialisations statiques |
| Ajout d'un moteur sans toucher l'interface | `AnalysisEngine`, registre, trois moteurs réels |
