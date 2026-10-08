# Analyse multi-moteurs

TSA calcule par l'intermédiaire de **moteurs interchangeables** (OpenSees, Custom2D, futurs moteurs).
Le modèle TSA, l'UI, les résultats et le format `.tsa` ne dépendent d'aucun moteur : chaque moteur est
un adaptateur derrière l'interface `TSA::Analysis::AnalysisEngine`.

```text
UI Analysis (AnalysisDialog)          MainWindow::runAnalysis
        │ AnalysisContext                    │
        ▼                                    ▼
             AnalysisManager (src/Analysis/Engine)
   ┌──────────────┬──────────────────┬──────────────────────┐
   │ ScopeResolver│ ModelExtractor   │ validation générique │
   │ grille/niveau│ CalculationSnap- │ (capacités)          │
   │ /plan/sélec. │ shot + mapping   │ + engine.validate()  │
   └──────────────┴──────────────────┴──────────────────────┘
                         │ AnalysisModel
          ┌──────────────┼──────────────────┐
          ▼              ▼                  ▼
   OpenSeesEngine   Custom2DEngine     (futur moteur)
   (OpenSeesSolver) (Custom2D::ISolver)
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
| Enregistrement des moteurs intégrés | `src/Analysis/Engines/BuiltInEngines.cpp` |
| Adaptateur OpenSees | `src/Analysis/Engines/OpenSees/OpenSeesEngine.*` |
| Contrat du solveur 2D, conversion, moteur | `src/Analysis/Engines/Custom2D/*` |
| Panneaux d'options par moteur (+ registre de fabriques) | `src/UI/Analysis/AnalysisEngineOptions.*`, `OpenSeesOptionsWidget.*` |
| Fenêtre Analysis commune | `src/UI/Analysis/AnalysisDialog.*` |
| Résultats de la barre sélectionnée (Propriétés) | `src/UI/Properties/ElementResultsPanel.*` |
| Tests | `tests/test_analysis_engines.cpp` (suite `engines`, tests 130-138) |

## Portée (AnalysisScope)

| Type | Source TSA réutilisée | Plan |
| :--- | :--- | :--- |
| Modèle complet | `SelectionQuery::all` | — |
| Sélection | `SelectionManager::selectedElements()` (passée par l'UI) | — |
| Axe de grille (« A », « B », « 1 »…) | `GridDefinition` : positions, libellés, origine, rotation (même transformation que `CartesianGrid`) | vertical, u le long de l'axe, v = +Z |
| Niveau | `LevelManager` + `SelectionQuery::atElevation` | horizontal |
| Plan de travail | `WorkPlaneManager` + `SelectionQuery::onWorkPlane` | axes du WorkPlane |

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
anti-singularité du calcul 3D (rotation de forage des articulations, Ty des appuis glissants) ne
doivent pas être projetés dans un plan (une articulation d'un portique de l'axe « 2 » resterait sinon
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
propres : barres inextensibles, points par courbe. Résultats : courbes N, V, M, déformée par barre
(`ResultsModel::planarCurves`) écrites dans la note de calcul (chapitre « Courbes RDM par barre »).
Un `Custom2DEngine` construit sans solveur reste indisponible (calcul refusé, aucun résultat).

Brancher le solveur :

```cpp
// Dans le cœur scientifique TSALab (sans Qt) : TSALab/science
class MySolver final : public tsalab::planar::ISolver { /* name, version, solve, features */ };
// tsalab::planar::createBuiltInSolvers() : ajouter le solveur ; puis l'ajouter au banc de validation
// (tsalab-bench) — TSA et TSALab le reçoivent sans autre modification.
```
Puis mettre à jour `Custom2DEngine::capabilities()` selon ce que le solveur fait réellement.

## Ajouter un moteur

1. Écrire `class XEngine : public AnalysisEngine` (info, capacités, disponibilité, validate, run, cancel) ;
   `run` reçoit l'`AnalysisModel` et rend un `ResultsModel` indexé par ids TSA (via `AnalysisMapping`).
2. Une ligne dans `registerBuiltInEngines`.
3. Facultatif : un `AnalysisEngineOptionsWidget` + une ligne dans `registerBuiltInEngineOptions`.

Aucune modification de la fenêtre Analysis, de MainWindow, de l'arbre, des propriétés, du ruban, des
grilles, des WorkPlanes ni de la sélection.

## Résultats

`ResultsModel` reste le modèle commun. Ajouts : `ResultAvailability` (catégories = capacité déclarée ET
données présentes, renseignée par `AnalysisManager`), `EngineResultTable` (résultats propres à un
moteur), `AnalysisExecutionMetadata::engineId / analysisScope / analysisDimension`. Le dock « Données
d'analyse » masque les onglets non fournis et ajoute les tables propres ; le panneau Propriétés affiche
les efforts de la barre sélectionnée avec le moteur et la portée.

## Persistance

`AnalysisContext::toJson / fromJson` (schéma versionné `schemaVersion`, lecture tolérante) est prêt à
être stocké dans le `.tsa` ; il ne l'est pas encore (BUG-013) : le contexte vit pour la session, comme
les paramètres d'analyse avant ce changement.
