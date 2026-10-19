# Analyse multi-moteurs

TSA calcule par l'intermédiaire de **moteurs interchangeables** (OpenSees, Custom2D, Fem3D, futurs moteurs).
Le modèle TSA, l'UI, les résultats et le format `.tsa` ne dépendent d'aucun moteur : chaque moteur est
un adaptateur derrière l'interface `TSA::Analysis::AnalysisEngine`.

```text
UI Analysis (AnalysisDialog, AnalysisManagerPanel)   console / Blueprint / IA (analysis.run)
        │ AnalysisContext                                  │
        ▼                                                  ▼
   AnalysisController (src/Analysis, session : réglages du projet, thread de travail, publication,
        │               invalidation des résultats) — MainWindow (TSA) et LabMainWindow (TSALab) n'en sont que des vues
        ▼
             AnalysisManager (src/Analysis/Engine)
   ┌──────────────┬──────────────────┬──────────────────────┐
   │ ScopeResolver│ ModelExtractor   │ validation générique │
   │ grille/niveau│ CalculationSnap- │ (capacités)          │
   │ /plan/sélec. │ shot + mapping   │ + engine.validate()  │
   └──────────────┴──────────────────┴──────────────────────┘
                         │ AnalysisModel
          ┌──────────────┼──────────────────┐
          ▼              ▼                  ▼
   OpenSeesEngine   Custom2DEngine     Fem3DEngine (facultatif)
   (OpenSeesSolver) (Custom2D::ISolver) (bibliothèque fem3d, ../3DDesplacement)
          └──────────────┴──────────────────┘
                         │ ResultsModel (ids TSA)
                         ▼
     Viewport · docks Résultats / Données d'analyse · Propriétés · NDC
```

## Fichiers

| Rôle | Fichier |
| :--- | :--- |
| Contexte (moteur, dimension, type, portée, chargement, réglages) + JSON versionné | `src/Analysis/Engine/AnalysisContext.*` |
| Portée résolue, plan 2D, `AnalysisModel`, `AnalysisMapping`, extraction | `src/Analysis/Engine/AnalysisModel.*` |
| Interface moteur, `EngineInfo`, `AnalysisCapabilities`, disponibilité | `src/Analysis/Engine/AnalysisEngine.h` |
| Registre | `src/Analysis/Engine/AnalysisEngineRegistry.*` |
| Orchestration + validation générique | `src/Analysis/Engine/AnalysisManager.*` |
| Contrôleur partagé (session) : moteurs, réglages du projet, calcul en tâche de fond, résultats | `src/Analysis/AnalysisController.*` (`ProjectSession::analysis()`) |
| Gestionnaire d'analyse (panneau partagé : moteurs, réglages, validation, calcul, synthèse) | `src/UI/Analysis/AnalysisManagerPanel.*` |
| Commandes `analysis.run` (dont `self_weight=oui\|non`), `results.summary`, `results.node_displacement`, `results.node_reaction`, `results.member_forces`, `query.bar`, `compare.values` (console, Blueprint, IA) | `src/Automation/CommandRegistry.cpp` |
| Enregistrement des moteurs intégrés | `src/Analysis/Engines/BuiltInEngines.cpp` |
| Adaptateur OpenSees | `src/Analysis/Engines/OpenSees/OpenSeesEngine.*` |
| Contrat du solveur 2D, conversion, moteur | `src/Analysis/Engines/Custom2D/*` |
| Moteur EF 3D Fem3D (dépôt voisin, facultatif) : conversion, moteur, secours et comparaison | `src/Analysis/Engines/Fem3D/*`, `src/UI/Analysis/Fem3DOptionsWidget.*` |
| Panneaux d'options par moteur (+ registre de fabriques) | `src/UI/Analysis/AnalysisEngineOptions.*`, `OpenSeesOptionsWidget.*` |
| Fenêtre Analysis commune | `src/UI/Analysis/AnalysisDialog.*` |
| Résultats de la barre sélectionnée (Propriétés) | `src/UI/Properties/ElementResultsPanel.*` |
| Tests | `tests/test_analysis_engines.cpp` (suite `engines`, tests 130-138, 196, 476-480) ; TSALab `lab/Tests` (L7) |

## Portée (AnalysisScope)

| Type | Source TSA réutilisée | Plan |
| :--- | :--- | :--- |
| Modèle complet | `SelectionQuery::all` | — |
| Sélection | `SelectionManager::selectedElements()` (passée par l'UI) | — |
| Axe de grille (« A », « B », « 1 »…) | `GridDefinition` : positions, libellés, origine, rotation (même transformation que `CartesianGrid`) | vertical, u le long de l'axe, v = +Z |
| Niveau | `LevelManager` + `SelectionQuery::atElevation` | horizontal |
| Plan de travail | `WorkPlaneManager` + `SelectionQuery::onWorkPlane` | axes du WorkPlane |
| Plan du modèle (`model_plane`) | tous les nœuds du modèle, plan détecté à chaque résolution | vertical si les nœuds sont alignés en plan (u horizontal, v = +Z), sinon horizontal (même cote) ; refusé si le modèle n'est pas plan |

Appartenance au plan : `GeometryTolerance::planeMembership` (ADR-008) — un élément est retenu si tous
ses nœuds sont dans le plan. Une portée peut être restreinte à un niveau (intersection, ex. « Axe B ∩
niveau 2 » = poutres de l'axe B à la cote du niveau 2).

## Extraction (AnalysisModel)

`AnalysisModelExtractor::extract` fait **une** capture `CalculationSnapshot::capture(model, &scope)` :
barres de la portée, nœuds qu'elles relient, charges sur ces nœuds / barres, tous les cas et
combinaisons. Aucune géométrie OCCT n'est construite. L'`AnalysisModel` contient aussi :

- `AnalysisMapping` : nœud TSA ↔ indice d'analyse (1..N), `ElementKey` ↔ indice (= tag du snapshot) ;
- `plane` et `planarCoordinates` (u, v) pour une analyse 2D, `outOfPlaneNodes` ;
- l'inventaire non transmissible : dalles, voiles, fondations de la portée, barres reliées à la portée
  mais exclues (`crossingElements`), charges hors portée.

`SnapshotNode::definedFix` conserve les blocages **définis par l'utilisateur** : les blocages
anti-singularité du calcul 3D (appui à fourche des articulations, rotations hors plan des structures
planes, Ty des appuis glissants, voir docs/OPENSEES_RESULTS.md) ne doivent pas être projetés dans un plan (une articulation d'un portique de l'axe « 2 » resterait sinon
encastrée en rotation).

Pas de cache : l'extraction est O(éléments) et le contexte peut changer sans modifier le modèle
(grilles hors révision du modèle).

## Validation

`AnalysisManager::prepare` = validation générique (déduite des capacités) + `engine->validate()` :
dimension et type supportés, plan requis en 2D, nœuds hors plan, familles d'éléments, longueurs nulles,
sections/matériaux, coques (`planarElementPolicy` : `Reject` → erreur explicite ; `ExcludeWithWarning`
→ avertissement), appuis, ressorts, cas/combinaisons existants. Sur le modèle complet,
`ModelValidator::validateForAnalysis` (contrôles normatifs historiques) est ajouté.

## Moteurs

### OpenSees (`opensees`)

Réutilise sans modification le chemin existant via `OpenSeesSolver::solveSnapshot` (même workflow que
`solveSynchronous`, sur le snapshot de la portée). `parametersFromContext` traduit contexte + bloc JSON
d'options en `AnalysisParameters`. Version lue sur l'exécutable (jamais inventée). Capacités : 3D,
barres/treillis/câbles/ressorts, statique linéaire et non linéaire, matrices en mode ADVANCED. Le
modal, le pushover et le temporel ont été retirés (ADR-022, branche `archive/dynamique`).

### Custom2D (`custom2d`)

Emplacement d'intégration des solveurs d'ossatures planes du cœur scientifique **TSALab** (ADR-024,
`docs/TSARALOHA_ARCHITECTURE.md`). Contrat : `tsalab::planar` (`TSALab/science/include/tsalab/planar/
PlanarSolver.h`, alias `Custom2D::` dans TSA via `Custom2DSolver.h` ; `ISolver`, `Input`, `Output`, système
`K·U = F` exportable) : données planes pures, indices contigus, unités kN / m / kPa, conventions de signe
documentées. `Custom2DAdapter` convertit (projection des nœuds, appuis, ressorts, inertie de flexion
dans le plan, charges ; pertes signalées) et remappe les résultats (déplacements et réactions en 3D
global ; efforts dans les axes locaux des résultats OpenSees ; tables propres remappées sur les ids TSA).
Solveur branché : premier de `tsalab::planar::createBuiltInSolvers()`, **MetDeDeplacement 2**
(`TSALab/science/engines/MetDeDeplacement`, méthode des déplacements, pont `tsalab::planar::MetDeDeplacementSolver`,
validé par le banc `tsalab-bench` : solution analytique + validation croisée K·U = F) — rotules (relâchements My/Mz de la poutre selon l'axe parallèle à la
normale du plan), treillis, ressorts, combinaisons (mêmes règles qu'OpenSees), poids propre. Options
propres : barres inextensibles, points par courbe, **export du système** (`exportSystem`) : K globale (COO),
F, U et numérotation des DDL (`AdvancedResults::kGlobal / loadVector / displacementVector / dofMap`, DDL U, V, RN
du plan) affichés par le dock « Données d'analyse » (onglets K globale, K·U = F, DDL) et rejoués par SOLVER LAB
(TSALab) ; sans DDL libre, la raison est donnée (`kGlobalUnavailableReason`). Équilibre global : réactions réelles,
résultante des charges = −ΣR au résidu contrôlé par le solveur près ; moments non contrôlés (signalé au journal). Résultats : courbes N, V, M, déformée par barre
(`ResultsModel::planarCurves`) écrites dans la note de calcul (chapitre « Courbes RDM par barre »).
Un `Custom2DEngine` construit sans solveur reste indisponible (calcul refusé, aucun résultat).

Second solveur plan du cœur scientifique : `tsalab::planar::OpenSeesPlanarSolver` (C++ pur : script Tcl 2D
elasticBeamColumn / truss / zeroLength, processus OpenSees sans fenêtre, post-traitement des barres commun avec
MetDeDeplacement). Il sert de référence indépendante au banc (`tsalab-bench` : 14/14, test S6 : déplacements,
réactions et efforts identiques à 1e-6 sur les 7 benchmarks) ; il n'est pas le solveur du moteur custom2d. Exécutable :
`setOpenSeesExecutable` (TSA lui transmet celui d'OpenSeesManager), sinon `TSALAB_OPENSEES`, sinon celui des dépôts ;
absent, il est ignoré et signalé. Non pris en charge (refus explicite) : rotules de barres fléchies, charges réparties
partielles ou trapézoïdales sur barre fléchie, export de K. `TSALAB_OPENSEES_KEEP=1` conserve script et résultats.

Brancher le solveur :

```cpp
// Dans le cœur scientifique TSALab (sans Qt) : TSALab/science
class MySolver final : public tsalab::planar::ISolver { /* name, version, solve, features */ };
// tsalab::planar::createBuiltInSolvers() : ajouter le solveur ; puis l'ajouter au banc de validation
// (tsalab-bench) — TSA et TSALab le reçoivent sans autre modification.
```

Puis mettre à jour `Custom2DEngine::capabilities()` selon ce que le solveur fait réellement.

### Fem3D (`fem3d`) — moteur secondaire

Moteur éléments finis 3D de TSA, développé dans le dépôt voisin **3DDesplacement** (TSA-3D-FEM-Engine, bibliothèque
`fem3d`, C++20 + Eigen, sans Qt). **Facultatif** : CMake le cherche dans `../3DDesplacement` (`TSA_FEM3D_DIR`) ;
présent, il est compilé (`TSA_HAS_FEM3D`) et enregistré **après** OpenSees, qui reste le moteur 3D par défaut tant que
la qualification de Fem3D dans TSA n'est pas complète (niveau 5, `3DDesplacement/docs/validation/QUALIFICATION.md`).

- **Périmètre** (capacités) : statique linéaire 3D ; poutres et poteaux d'Euler–Bernoulli (rigidité exacte, comme
  `elasticBeamColumn`) ; rotules My, Mz ; appuis et ressorts ; charges nodales ; charges réparties uniformes ou
  linéaires sur toute la longueur ; poids propre. Validé contre OpenSees 3.8.0 dans son dépôt (20 cas × 2
  formulations, 2 657 valeurs sans écart) et dans TSA (test 477 : portique spatial, stations comprises).
- **Conversion** (`Fem3DAdapter`) : réutilise `OpenSeesModelMap` (barres, v_xz, ressorts), `loadPatterns` et
  `beamElementLoads` (cas, combinaison, poids propre, charges locales) ; les axes locaux de Fem3D sont comparés à ceux
  de TSA (refus s'ils diffèrent). Résultats : efforts d'extrémité Fem3D (convention `localForce`) → stations par
  `OpenSeesResultsReader::beamResultsFromEndForces`, la même reconstruction que pour OpenSees ; équilibre global et
  contrôle des blocages automatiques par les fonctions communes.
- **Hors périmètre** (refus avec la raison) : charges ponctuelles ou partielles sur barre, moments répartis, coques ;
  relâchements de N, V ou T (erreur). Treillis, câbles, non-linéaire, modal : refusés par la validation générique.
- **Options** (`mode`, `fallback`) : `fem3d` (défaut) ou `compare` (Fem3D puis OpenSees ; résultats affichés = Fem3D,
  table « Comparaison Fem3D / OpenSees », avertissement au-delà de max(1e-6·|réf|, 1e-9·échelle), aucune fusion) ;
  secours `none` (défaut : refus) ou `opensees` (annoncé à la validation, calcul par OpenSees,
  `AnalysisRunResult::producedBy` → résultats étiquetés OpenSees). Un mécanisme ou une donnée invalide n'est jamais
  confié à un autre moteur.
- Pas de maillage du solveur joint (`setSolverMesh`) : un élément fini par barre, sans nœud auxiliaire.

Documentation du moteur : `3DDesplacement/docs/` (théorie, conventions, audit indépendant, plan de validation,
qualification, bibliographie, limites).

## Ajouter un moteur

1. Écrire `class XEngine : public AnalysisEngine` (info, capacités, disponibilité, validate, run, cancel) ;
   `run` reçoit l'`AnalysisModel` et rend un `ResultsModel` indexé par ids TSA (via `AnalysisMapping`).
2. Une ligne dans `registerBuiltInEngines`.
3. Facultatif : un `AnalysisEngineOptionsWidget` + une ligne dans `registerBuiltInEngineOptions`.
4. Recommandé : joindre le maillage réellement transmis au solveur (`results.setSolverMesh`, structure
   `SolverMesh`) ; le mode d'affichage « Modèle éléments finis » l'affiche (docs/DISPLAY_MODES.md).

Aucune modification de la fenêtre Analysis, de MainWindow, de l'arbre, des propriétés, du ruban, des
grilles, des WorkPlanes ni de la sélection.

## Résultats

`ResultsModel` reste le modèle commun. Ajouts : `ResultAvailability` (catégories = capacité déclarée ET
données présentes, renseignée par `AnalysisManager`), `EngineResultTable` (résultats propres à un
moteur), `AnalysisExecutionMetadata::engineId / analysisScope / analysisDimension` (identité du moteur qui a
réellement produit les résultats : `AnalysisRunResult::producedBy` pour un calcul délégué). Le dock « Données
d'analyse » masque les onglets non fournis et ajoute les tables propres ; le panneau Propriétés affiche
les efforts de la barre sélectionnée avec le moteur et la portée.

## Persistance

`AnalysisContext::toJson / fromJson` (schéma versionné `schemaVersion`, lecture tolérante) est enregistré avec le
projet (chunk SETT, BUG-013 corrigé) par `AnalysisController::storeContextInModel` et relu par
`restoreContextFromModel` (moteur absent de l'installation → premier moteur disponible).

## Contrôleur d'analyse partagé (ADR-024, phase 7)

`TSA::Analysis::AnalysisController` (couche modèle, Qt Core) appartient à `ProjectSession` : registre des moteurs,
`AnalysisManager`, contexte du projet, `start()` (thread de travail, signaux `started / progressChanged / logMessage /
finished`), `cancel()`, `runBlocking()` (tests, scripts, Blueprint), `results()` + `ResultsValidityGuard`
(`resultsChanged`, `resultsBecameStale` différé). Les questions à l'utilisateur (installation d'un moteur, nettoyage,
avertissements) restent dans l'application. TSA (`MainWindow::runAnalysis`) et TSALab (`AnalysisManagerPanel`)
l'utilisent ; la console, le Blueprint et l'IA passent par les commandes `analysis.run` (moteur, axe de grille,
« * » modèle complet, « plan » plan du modèle, export du système) et `results.*`.
