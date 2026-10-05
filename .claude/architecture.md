# Architecture (réelle) — TSA

Last Updated: 2026-10-05 (analyse multi-moteurs, branche feature/multi-engine-analysis). Code > documentation.

## 1. Cibles CMake

```text
TSA_Core  (bibliothèque OBJECT, CORE_SOURCES, PCH Qt/OCCT/STL partagé)
 ├── TSA          (exe : UI, viewer, rendu, plateforme, ressources) — src/main.cpp → Application
 └── TSA_Tests    (exe TSA_TestSuite : tests/*.cpp, suites --suite=...)
```
Fichiers [CORE] : Model (dont SelectionQuery), Coordinate, Grid (logique), Commands, UndoRedo, IO,
Analysis, NDC, Standards, ExtensionSystem ; Geometry : `GEOMETRY_CORE_SOURCES` (BeamGeometry,
DeformedGeometry, DiagramGeometry, SupportGeometry) + `CABLE_GEOMETRY_SOURCES` (CableGeometry3D) ;
Viewer : `VIEWER_CORE_SOURCES` (MaterialVisual, TextureManager, ProjectionManager, ViewManager).
Exe uniquement : UI ; `VIEWER_MAIN_SOURCES` (OccView*, **SelectionManager**, ResultsVisualManager) ;
`GEOMETRY_MAIN_SOURCES` (SlabGeometry, WallGeometry, FoundationGeometry). Vérifier `CMakeLists.txt`
avant de référencer une classe depuis les tests (TSA_Tests ne lie que TSA_Core). `src/View3D/Isolation/*` et `tests/isolation/*` ne sont
**pas compilés** (code mort).

## 2. Cartographie des sous-systèmes

| Subsystem | Directory | Main classes | Important files | Dependencies |
| :--- | :--- | :--- | :--- | :--- |
| Bootstrap | src/App, src/main.cpp | `Application` | App/Application.cpp | Logger, CrashHandler, MainWindow |
| Main window | src/UI | `MainWindow` (monolithique, 4 fichiers) | MainWindow.cpp, MainWindow_Actions.cpp (menus, docks, connexions), MainWindow_Tools.cpp (analyse, NDC), MainWindow_Transform.cpp | quasiment tout |
| Viewport 3D | src/Viewer | `OccView` (QWidget + IModelObserver) | OccView.cpp (init, picking/snap), OccView_Shapes.cpp (formes AIS), OccView_Navigation.cpp (caméra, WorkPlane, 2D, isolation, visibilité), OccView_Events.cpp (souris/clavier) | AIS_InteractiveContext, Geometry, SelectionManager, GridRenderer |
| Caméra / projection | src/Viewer | `ViewManager`, `ProjectionManager` | ViewManager.cpp, ProjectionManager.cpp | V3d_View |
| Sélection | src/Viewer, src/Model | `SelectionManager` (QObject), `SelectionQuery` (fonctions), `ElementSet` | SelectionManager.cpp, Model/SelectionQuery.cpp | AIS handles ↔ ids |
| Modèle structural | src/Model | `Model`, `Node`, `Beam`, `Column`, `Slab`, `Wall`, `Foundation`, `TrussMember`, `Cable`, `Section`, `Material`, `MaterialLibrary`, `StructuralClipboard`, `ModelDiff` | Model.cpp, Model_Snapshots.cpp, Model_Transformations.cpp | LoadManager, CoordinateSystem, UndoManager |
| Charges | src/Model/Load | `LoadManager`, `LoadCase`, `LoadCombination`, `NodalLoad`, `MemberLoad` | LoadManager.cpp | Model |
| Géométrie | src/Geometry | `BeamGeometry`, `SlabGeometry`, `WallGeometry`, `FoundationGeometry`, `CableGeometry3D`, `SupportGeometry`, `DeformedGeometry`, `DiagramGeometry` | *.cpp | OCCT BRep/BRepPrimAPI |
| Coordonnées / niveaux / WorkPlanes | src/Coordinate | `CoordinateSystem`, `LevelManager` (QObject), `Level`, `WorkPlane`, `WorkPlaneManager`, `CoordinateTransformationService` (singleton), `GeometryTolerance` | CoordinateSystem.cpp (detectStructuralPlanes), WorkPlane.cpp | gp_Ax3 |
| Grilles / snap | src/Grid | `GridManager`, `GridSystem`, `CartesianGrid`, `CylindricalGrid`, `GridRenderer`, `GridSnapManager`, `SnapManager` | GridRenderer.cpp | OCCT AIS |
| Undo/Redo & commandes | src/UndoRedo, src/Commands | `UndoManager`, `EditTransaction`, `EditRecord`, `CommandManager`, `ICommand`, `Create*Command`, `Move/Rotate/DeleteElementsCommand`, `ModifyWorkPlaneCommand`, `GridCommands`, `CommandCatalog` | UndoManager.cpp, CommandManager.cpp, CommandCatalog.cpp | Model snapshots |
| Propriétés | src/UI/Properties | `PropertyPanel` (QStackedWidget de vues) + `Node/Beam/Column/Slab/Wall/Foundation/TrussMember/Cable/Load/WorkPlanePropertiesView` (`IElementPropertyView`) | PropertyPanel.cpp | Model, MaterialLibrary |
| Arbre / docks | src/UI/ModelTree, src/UI/Dock | `ModelTreeWidget` (observer), `VisibilityDock`, `ResultsDockWidget`, `LogConsoleDock`, `ProjectionViewDock`, `StructuralElementsDock`, `ViewportContainer` (règles, combos plan X/Y/Z, bouton 2D) | ModelTreeWidget.cpp, Ruler/ViewportContainer.cpp | Model, OccView |
| Fichier .tsa | src/IO, src/Project | `TSAFileWriter`, `TSAFileReader`, `TSAProjectIO`, `ProjectManager`, `TSAPreviewGenerator` | TSAFile.cpp, TSAFile{Reader,Writer}_Chunks.cpp, TSAFileFormat.h | Model snapshot, GridManager, QSaveFile, zlib (qCompress) |
| Calcul | src/Analysis | `CalculationSnapshot`, `OpenSeesAnalysisBuilder` (Tcl), `OpenSeesSolver`, `OpenSeesManager`, `OpenSeesResultsReader`, `ResultsModel`, `ResultsValidityGuard`, `LoadResolver` | OpenSeesSolver.cpp | QProcess (OpenSees externe) |
| Outils de modification / dessin | src/Interaction/Tools, src/Viewer/OccView_Tools.cpp, src/UI/Tools, src/UI/MainWindow_ModelingTools.cpp | `ModelingTool`, `ModelingToolRegistry`, `ToolContext`, `ModelingToolDialog` | Tools/ModifyTools.cpp, Tools/DrawTools.cpp | Model (move/transform/copy/mirror/split/merge), EditTransaction ; docs/MODELING_TOOLS.md |
| Analyse multi-moteurs | src/Analysis/Engine, src/Analysis/Engines, src/UI/Analysis | `AnalysisContext`, `AnalysisScope`, `AnalysisScopeResolver`, `AnalysisModel`, `AnalysisMapping`, `AnalysisModelExtractor`, `AnalysisEngine`, `AnalysisCapabilities`, `AnalysisEngineRegistry`, `AnalysisManager`, `OpenSeesEngine`, `Custom2DEngine` (+ `Custom2D::ISolver`, `Custom2DAdapter`), `AnalysisDialog`, `AnalysisEngineOptionsRegistry`, `OpenSeesOptionsWidget`, `ElementResultsPanel` | Engine/AnalysisManager.cpp, Engines/BuiltInEngines.cpp | Model, GridManager, SelectionQuery, CalculationSnapshot, ResultsModel ; docs/ANALYSIS_ENGINES.md |
| IA Co-Engineering | src/AI, src/UI/AI | `AIOrchestrator`, `IAIProvider`/`OpenAICompatibleProvider`, `LocalLlamaServer`, `HardwareProfiler`, `ModelRegistry`/`ModelSelector`, `ModelManager`, `EngineeringContextBuilder`, `AIToolRegistry`, `StructuralChecker`, `EngineeringKnowledgeBase`, `AICoEngineeringDock`, `AIRuntimeDialog` | AI/Core/AIOrchestrator.cpp, UI/MainWindow_AI.cpp | Model, ResultsModel, LoadValidation, SelectionManager, Qt Network, llama-server (processus externe) |
| Résultats 3D | src/Viewer | `ResultsVisualManager` (déformée, diagrammes, réactions, modal) | ResultsVisualManager.cpp | ResultsModel, OccView context |
| Note de calcul | src/NDC | `NDCGenerator`, `ReportManager`, `NDCViewerWidget`, `NDCExporter` | NDCGenerator.cpp | ResultsModel, Standards |
| Normes | src/Standards | `ModelValidator`, `RequirementsCatalog`, `NationalAnnexConfig`, Design (EC2/EC3) | ModelValidator.cpp | Model |
| Extensions TSALib | src/ExtensionSystem, Extensions/TSALib | `ExtensionManager`, `LibraryLoader/Registry/Validator/Cache/Packager` | — | JSON |
| Diagnostics | src/Diagnostics | `Logger` (singleton, ring buffer), `CrashHandler` (SEH, minidump), `DiagnosticReport` | — | dbghelp |

## 3. Composants critiques

```text
Component: Structural model (source de vérité)
Class: TSA::Model::Model
File: src/Model/Model.h / Model.cpp / Model_Snapshots.cpp
Responsibility: éléments, nœuds, charges (LoadManager), snapshots, révision, observateurs
Dependencies: LoadManager, CoordinateSystem/LevelManager, WorkPlaneManager, UndoManager
Used by: tout
Status: stable
Notes: revision() augmente à chaque notification ; notify*Modified obligatoire après mutation directe.

Component: Viewport
Class: OccView
File: src/Viewer/OccView*.cpp (≈ 7 000 lignes au total)
Responsibility: scène AIS, mise à jour incrémentale par observateur (update*Shape / remove*Shape),
  ModelDiff (Undo), caméra, WorkPlane, mode 2D, isolation, snap, sélection OCCT
Dependencies: AIS_InteractiveContext, V3d_View, Geometry builders, SelectionManager, GridRenderer
Used by: MainWindow, ViewportContainer, dialogs
Status: stable (fichiers > 2 000 lignes : découpage à analyser avant ajout de responsabilités)

Component: Selection
Class: TSA::Viewer::SelectionManager (+ TSA::Model::SelectionQuery)
Responsibility: maps id↔AIS, ensembles sélectionnés, signaux par type, selectElements(ElementSet)
Used by: OccView, MainWindow (synchro arbre/propriétés/viewport)
Status: stable

Component: Undo/Redo
Class: TSA::UndoRedo::UndoManager / EditTransaction / CommandManager
Responsibility: snapshots complets (50 niveaux, budget 512 Mio), transactions, coalescence, EditRecord
Status: stable (niveaux/grilles/WorkPlanes hors snapshot)

Component: Serialization
Class: TSAFileWriter / TSAFileReader / TSAProjectIO
Responsibility: format binaire .tsa 1.1 (en-tête 256 o, chunks FourCC, zlib, CRC32 table, QSaveFile)
Status: stable

Component: Analysis engines (multi-moteurs)
Class: AnalysisManager + AnalysisEngineRegistry + AnalysisEngine (OpenSeesEngine, Custom2DEngine)
File: src/Analysis/Engine/*, src/Analysis/Engines/*, src/UI/Analysis/*
Responsibility: contexte → portée (grille/niveau/WorkPlane/sélection) → AnalysisModel dérivé (CalculationSnapshot
  restreint + AnalysisMapping + plan 2D) → validation générique (capacités) + moteur → run → ResultsModel (ids TSA)
Dependencies: Model, GridManager (lecture), SelectionQuery, CalculationSnapshot, ResultsModel, ModelValidator
Used by: MainWindow (onActionAnalysisConfig / onActionRunSolve → runAnalysis → publishResults), tests suite engines
Status: stable (Custom2D : adaptateur prêt, solveur non connecté — aucun calcul)
Notes: ajouter un moteur = adaptateur + ligne dans registerBuiltInEngines (+ panneau d'options facultatif).
  Les raccourcis Modal / Pushover du ruban appellent encore OpenSeesSolver directement.

Component: Calculation
Class: OpenSeesSolver (+ CalculationSnapshot, OpenSeesModelMap, OpenSeesAnalysisBuilder, OpenSeesResultsReader,
       ElementTransformation, ResultsExport, ResultsContext)
Responsibility: snapshot immuable → mapping TSA↔OpenSees → script Tcl → OpenSees 3.8.0 externe → ResultsModel
  (+ passage « matrices » séparé en mode ADVANCED). Référence : docs/OPENSEES_RESULTS.md
Notes: OpenSeesAdapter (export Tcl du dialogue des cas de charge) délègue au même générateur.
Status: partiel (barres/treillis/câbles/appuis ; pas de dalles ni voiles ; exécution synchrone sur le thread UI)
```

## 4. Flux principaux

```text
Édition propriété : PropertyView::applyChanges → pushUndoState(nom, nom) → mutation → notify*Modified
  → OccView::update*Shape (forme locale) / ModelTree ligne / ResultsValidityGuard (invalide résultats)
Undo : UndoManager::undo → ModelDiff::compute → applySnapshotData → notifyModelDiffApplied (mise à jour ciblée)
Ouverture .tsa : MainWindow::loadFile → ProjectManager::openProject → TSAFileReader → Model::restoreSnapshot
  → onModelCleared → OccView::rebuildAllShapes (une passe) ; log ProjectLoadTiming
Calcul (F5 / fenêtre Analysis) : MainWindow::runAnalysis(AnalysisContext) → AnalysisManager::prepare (portée → AnalysisModel → validation)
  → AnalysisManager::run → AnalysisEngine::run (OpenSeesEngine → OpenSeesSolver::solveSnapshot) → ResultsModel → publishResults
Calcul modal / pushover (ruban) : MainWindow_Tools → OpenSeesSolver::solveSynchronous → ResultsModel → ResultsValidityGuard::trackResults
Sélection : clic/fenêtre (OccView_Events) → SelectionManager → signaux → MainWindow (arbre, propriétés, highlight)
```

## 5. Zones de risque

- Mémoire / ownership : pointeurs bruts `Model*` dans les observateurs (protégés par
  `onModelDestroyed`) ; `ResultsVisualManager` possédé à la fois par `unique_ptr` et parent
  QObject `OccView` (fonctionne car détruit d'abord par le unique_ptr, mais ambigu) ;
  `Handle(AIS_*)` gardés dans les maps d'OccView ET de SelectionManager (cohérence assurée par
  register/unregister à chaque update*Shape).
- Performance : snapshot complet par entrée Undo (~3,2 Mio / 4 896 barres) ; recherches
  linéaires par id dans ModelTreeWidget ; `Model::isNodeFree` O(éléments) par nœud ; picking/snap
  O(nœuds) par mouvement souris (acceptable mesuré) ; calcul OpenSees synchrone.
- `.tsa` : versions 1.0 (sans LOAD) et 1.1 lisibles ; chunks inconnus ignorés via chunkSize ;
  niveaux/coordonnées dans COOR, grilles dans GRID ; pas de migration nécessaire à ce jour.
