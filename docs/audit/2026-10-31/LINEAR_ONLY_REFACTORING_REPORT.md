# Retrait de l'analyse non linéaire — rapport de refactorisation

Date : 2026-10-31. Branche `refactor/linear-only`, créée depuis le commit `5403ed6` (base de l'audit du même jour).
**Aucun commit, aucune fusion, aucun push.** Décision d'architecture : ADR-025 (`.claude/decisions.md`).

> **TSA prend actuellement en charge l'analyse structurale linéaire. L'analyse non linéaire n'est pas disponible dans
> cette version.**

## Résumé

L'analyse non linéaire était réellement implémentée, uniquement dans le chemin OpenSees : 7 algorithmes itératifs,
4 intégrateurs, les transformations P-Delta et corotationnelle, des treillis corotationnels, un calcul en plusieurs pas
et la lecture du facteur de charge. Tout ce code a été retiré. Aucun fichier source n'était exclusivement non linéaire :
le retrait s'est fait à l'intérieur des fichiers partagés, sans changer CMake ni les dépendances.

Le calcul linéaire n'a pas bougé :

- les 28 scripts Tcl et les 76 fichiers de résultats des 9 projets d'exemple sont **identiques à l'octet près** avant et
  après ;
- les **15 312** valeurs de banc communes aux deux exécutions sont identiques **au bit près**, avec les mêmes statuts ;
- 57 suites sur 57 réussissent, avec **544 tests** (538 avant, plus 6 nouveaux), 0 échec ;
- le fuzzing de la voie de calcul, rejoué avec la même graine que l'audit, donne exactement les mêmes résultats.

Compilation Debug complète réussie : 126 tests réussis sur 12 suites, avec un seul échec, un seuil de temps propre au
Debug (test 457). TSALab : 27/27, après la mise à jour d'un compte de cas dans son test L9.

Un ancien projet qui demandait une analyse non linéaire **s'ouvre**, conserve toutes ses données, affiche un message,
et **son calcul est refusé**. Il n'est jamais recalculé en linéaire à l'insu de l'utilisateur.

BUG-149 et BUG-150 sont clos par retrait de la fonctionnalité et restent dans le registre comme défauts historiques.
**BUG-148 (CRITICAL) reste ouvert** : OpenSees donne des efforts faux, sans alerte, sur une barre à rotule portant une
charge ponctuelle ou trapézoïdale. Il est hors du périmètre de cette mission.

## 1. Fonctionnalités non linéaires identifiées

Chaque élément a été vérifié dans le code de `5403ed6`, rien n'a été supposé.

### 1.1 Présentes dans le code, retirées

| Fonctionnalité | Emplacement (version d'origine) | Effet réel |
|---|---|---|
| Type d'analyse statique non linéaire | `ResultsModel.h` (`AnalysisType::NonLinearStatic`), `AnalysisContext.cpp` (clé `nonlinear_static`), capacité `supportsNonlinear` (`AnalysisEngine.h`, OpenSees : `true`) | proposé dans la fenêtre Analysis |
| Newton-Raphson, Newton avec recherche linéaire, Newton modifié, Krylov-Newton, BFGS, Broyden, Secant Newton | `OpenSeesAnalysisBuilder.h` (`NonlinearAlgorithm`, `algorithmToTcl`, `toTclString`) | commande `algorithm` du script non linéaire |
| Load Control en plusieurs pas, Displacement Control, Arc-Length, MinUnbalDispNorm | `IntegratorType`, `buildAnalysisCommands` | boucle de `numSteps` pas, test de convergence, `TSA_OPS_CONVERGENCE_FAIL` |
| Pas, taille de pas, tolérance, itérations, nœud et DDL contrôlés, incrément de déplacement, « enregistrer tous les incréments » | `AnalysisParameters` (14 champs retirés en tout, avec l'algorithme, l'intégrateur et les deux formulations), bloc JSON `opensees` | réglages du calcul non linéaire |
| Transformations P-Delta et corotationnelle | `GeomTransfType`, `buildElements` (`geomTransf <type>`) | proposées quel que soit le type d'analyse (BUG-150) |
| Treillis corotationnels | `TrussFormulation`, `OpenSeesModelMap.cpp` (truss → `corotTruss`) | changeait l'élément des treillis |
| Facteur de charge et historique des pas statiques | `OpenSeesResultsReader.cpp` (λ = pas × taille de pas : BUG-149), `ResultsModel::finalLoadFactor`, `OpenSeesSolver.cpp` (`TSA_OPS_LOAD_FACTOR`, `getTime`, « Non-convergence ») | efforts des stations multipliés par λ |
| Contrôle du nœud de pilotage | `OpenSeesEngine::validate` | refus si le nœud est hors portée |
| Interface | panneau OpenSees (groupes « Formulation », « Résolution non linéaire », « Contrôle du déplacement », case « Enregistrer tous les incréments ») ; fenêtre Analysis (type, ligne de capacité « Non linéaire ») ; panneau de gestion (« statique non linéaire ») ; dock Résultats (« Pas de calcul / Incréments », facteur λ) | options visibles |
| Cases sans effet sur le calcul | « Traction seule (Non-linéarité géométrique) » (propriétés du câble), « Élément tendu uniquement (Tension-only) » (création) | laissaient croire à un comportement non linéaire calculé |
| Drapeaux de câble jamais lus par un calcul | `CableAnalysisProperties` : `largeDisplacement`, `geometricNonlinearity`, `sagEffectEnabled`, `minTensionThreshold` | aucun (données mortes) |
| Banc | type `nonlinear_static` accepté par `parseBenchmark` ; cas RDM-11a et RDM-11b | 5 vérifications non linéaires |
| Textes | `RequirementsCatalog.cpp` (« Comportement non-linéaire… validés », statut Implemented), `ExternalLibraryCatalog.cpp`, note de calcul (« non linéaire géométrique selon les options du moteur »), synonyme de commande « non linéaire », commentaires | annonces inexactes |
| Documentation | README, DOCUMENTATION, ANALYSIS_OPENSEES, OPENSEES_CALCULATION_METHODS, UI (décrivait un `AnalysisConfigDialog` disparu), AUDIT_CALCULS, BENCHMARKS, cable-system | décrivait le non linéaire comme disponible |

### 1.2 Cherchées, absentes du calcul

| Recherche | Constat |
|---|---|
| Plasticité, rotules plastiques, hystérésis, non-linéarité matérielle | absentes : tous les matériaux sont élastiques. Les mots « plastique » du code sont des résistances plastiques EC3 (vérifications réglementaires) ou des matériaux d'affichage OCCT |
| Post-flambement, grands déplacements hors OpenSees | absents |
| Statique ou dynamique non linéaire dans Fem3D | absente : Fem3D est linéaire (`SimplicialLDLT`), rien à retirer |
| Non linéaire dans Custom2D / TSALab | l'adaptateur Custom2D traduisait `NonLinearStatic` (corrigé) ; `tsalab::planar::AnalysisType::NonLinearStatic` existe dans TSALab mais n'est pas utilisé (laissé, dépôt séparé) |
| Pushover, temporel | déjà retirés (ADR-022), aucune trace dans le code |

### 1.3 Proches du non linéaire, conservés volontairement

| Élément | Raison |
|---|---|
| `integrator LoadControl 1.0`, `test NormDispIncr 1e-06 50` | exigés par la commande `analysis` d'OpenSees pour le pas statique unique ; `algorithm Linear` n'évalue pas le test |
| `StepResults`, `addStepResults`, `setActiveStep` | portent les formes propres de l'analyse modale (linéaire) |
| Câbles en `corotTruss` + `InitStrain` | porte des résultats validés ; voir § 10, limite mesurée |
| Module d'Ernst (`Cable::equivalentErnstModulus`) | formule du modèle, testée, jamais utilisée par le calcul (documenté) |
| Donnée `tensionOnly` | lue et écrite par les bibliothèques, le `.tsa` et TSA3D ; sans effet de calcul (documenté) |
| Point d'extension `AnalysisEngineOptionsWidget::setAnalysisContext` | générique, sans lien avec le non linéaire ; plus utilisé par OpenSees |

## 2. Fichiers supprimés

| Fichier | Raison |
|---|---|
| `tests/benchmarks/rdm/RDM-11a.json` | cas exclusivement non linéaire (`nonlinear_static`) |
| `tests/benchmarks/rdm/RDM-11b.json` | cas exclusivement non linéaire (P-Delta, `nonlinear_static`) |

Aucun fichier C++ supprimé : aucun n'était exclusivement non linéaire. `CMakeLists.txt` est inchangé, aucune source,
bibliothèque ni option à retirer.

## 3. Fichiers modifiés

`git diff --shortstat` (hors audit et fichiers d'exemple modifiés avant la mission) : 78 fichiers, 938 lignes ajoutées,
1 176 retirées.

| Domaine | Fichiers |
|---|---|
| Moteur et OpenSees | `src/Analysis/OpenSeesAnalysisBuilder.{h,cpp}`, `OpenSeesModelMap.cpp`, `OpenSeesResultsReader.cpp`, `OpenSeesSolver.{h,cpp}`, `ResultsModel.{h,cpp}`, `Engine/AnalysisEngine.h`, `Engine/AnalysisContext.{h,cpp}`, `Engine/AnalysisManager.{h,cpp}`, `AnalysisController.{h,cpp}`, `Engines/OpenSees/OpenSeesEngine.{h,cpp}`, `Engines/Custom2D/Custom2DAdapter.cpp` ; commentaires : `ElementTransformation.h`, `Viewer/ResultsVisualManager.h`, `Standards/ModelValidator.h` |
| Interface | `src/UI/Analysis/OpenSeesOptionsWidget.{h,cpp}`, `AnalysisDialog.{h,cpp}`, `AnalysisManagerPanel.cpp`, `Dock/ResultsDockWidget.cpp`, `MainWindow_Tools.cpp`, `Properties/CablePropertiesView.{h,cpp}`, `Dialogs/CableCreationDialog.{h,cpp}` |
| Données | `src/Model/Cable/CableAnalysisProperties.h`, `Cable.h`, `IO/TSAFileReader_Chunks.cpp`, `IO/TSAFileWriter_Chunks.cpp` |
| Banc, rapports, textes | `src/Validation/BenchmarkSuite.cpp`, `Reports/ReportDataBuilder.cpp` (un calcul modal était étiqueté « statique non linéaire »), `NDC/NDCProjectNote.cpp`, `Standards/RequirementsCatalog.cpp`, `Standards/ExternalLibraryCatalog.cpp`, `Commands/CommandSynonyms.cpp`, `Commands/CommandCategory.h`, `ExtensionSystem/ExtensionTypes.h`, `ExtensionSystem/ExtensionManager.cpp` |
| Tests | `tests/test_analysis_engines.cpp` (tests 131, 132, 135, 137, 476 adaptés ; 532-536 ajoutés), `test_benchmarks.cpp` (537), `test_opensees.cpp`, `undo_characterization.h`, `test_main.cpp` ; 10 cas ajoutés dans `tests/benchmarks` (§ 9.2) ; `docs/schemas/tsa-benchmark-1.schema.json` |
| Traductions | `translations/tsa_en.ts`, `tsa_mg.ts` : 52 textes retirés, 16 ajoutés et traduits dans les deux langues |
| Documentation | `README.md`, `DOCUMENTATION.md`, `docs/ANALYSIS_ENGINES.md`, `ANALYSIS_OPENSEES.md`, `AUDIT_CALCULS.md`, `BENCHMARKS.md`, `NORMATIVE_SYSTEM.md`, `OPENSEES_CALCULATION_METHODS.md`, `OPENSEES_RESULTS.md`, `ROADMAP.md`, `TSA_FILE_FORMAT.md`, `UI.md`, `cable-system/{analysis,cable-types,overview}.md` |
| Mémoire et audit | `.claude/{CLAUDE,architecture,changelog,current-state,decisions,known-issues,tasks}.md`, `docs/audit/2026-10-31/TSA_BUG_REGISTER.md`, ce rapport, `preuves/linear-only/` |

Non touchés, modifiés avant la mission par l'utilisateur : `TSA.code-workspace`, `examples/TSA_Projet_Batiment_Complexe_L.tsa`,
`examples/TSA_Projet_Pont_Haubans_80-180-80m.tsa`.

Dépôt TSALab : un seul fichier, `lab/Tests/LabTests.cpp`, dont le test L9 attend désormais 36 cas dans le banc partagé.
Modification faite sur la branche `refactor/linear-only` de TSALab, sans commit.

## 4. Fonctionnalités conservées

| Bloc | Contenu |
|---|---|
| A. Modélisation | nœuds, barres, poutres, poteaux, treillis, câbles, dalles et voiles maillés, sections, matériaux, appuis et ressorts, relâchements, charges nodales, sur barre, surfaciques, poids propre, cas et combinaisons |
| B. Analyse linéaire | statique linéaire (OpenSees, Fem3D, Custom2D) et modale (OpenSees) ; assemblage, résolution, déplacements, réactions, efforts aux stations, équilibre ; mode ADVANCED (matrices, numérotation des DDL) ; portées, sélection, axes, niveaux |
| C. Post-traitement | déformée, diagrammes N, V, M, rotations, contraintes de fibre, réactions, diagrammes 2D, formes propres, données d'analyse, export ; animation visuelle de la déformée (non dynamique) |
| D. Vérifications réglementaires | inchangées et distinctes de l'analyse : EC2 flexion simple, EC3 flambement par flexion, câbles EN 1993-1-11. Elles ne sont pas validées du seul fait que l'analyse est linéaire ; la note de calcul liste ce qui n'est pas vérifié, dont les effets du second ordre |

## 5. Dépendances retirées

Aucune. OpenSees reste le moteur par défaut (statique linéaire, modale, coques). Fem3D, Gmsh, IfcOpenShell, Eigen, Qt et
OCCT servent tous au calcul linéaire ou à la modélisation.

## 6. Changements d'interface

| Endroit | Avant | Après |
|---|---|---|
| Fenêtre Analysis, type | statique linéaire, statique non linéaire, modale | statique linéaire, modale |
| Fenêtre Analysis, capacités | « ✓ Statique ✓ Non linéaire » | « ✓ Statique linéaire ✓ Modale » |
| Fenêtre Analysis, ancien projet | rien | bandeau orange : fonctionnalité demandée, « Valider cette fenêtre remplace cette demande… ; Annuler la conserve et aucun calcul n'est lancé » |
| Options OpenSees | 4 groupes, dont 3 non linéaires (12 réglages) | « Système et contraintes » seul : solveur, gestionnaire de contraintes, extraction, plafond K, unités |
| Panneau de gestion de l'analyse | « Calcul : statique non linéaire » possible | « type « nonlinear_static » non pris en charge » pour un ancien projet |
| Dock Résultats | « Pas de calcul / Incréments », « Pas i / n (λ = …) » | « Modes propres » ; « Résultat unique (statique linéaire) » |
| Câbles | cases « Traction seule (Non-linéarité géométrique) », « Tension-only » | retirées ; l'aide de création dit qu'une compression n'est pas relâchée |
| Ouverture d'un ancien projet | réglages relus, non linéaire calculé | message non bloquant et ligne de console : fonctionnalité non prise en charge, données conservées |
| F5 sur un ancien projet | calcul non linéaire | refus avant toute autre question (« Analyse non prise en charge ») |

Le ruban, les menus, les raccourcis et les dialogues de combinaisons ne contenaient aucune option non linéaire.

## 7. Changements du moteur

- **Script statique unique** : `constraints`, `numberer RCM`, `system`, `test NormDispIncr 1e-06 50`, `algorithm Linear`,
  `integrator LoadControl 1.0`, `analysis Static`, `analyze 1` ; `geomTransf Linear` pour toutes les barres ; treillis
  `truss`. Le script modal est inchangé.
- **Lecture des résultats** : plus de facteur de charge ni d'historique de pas statiques ; les efforts des stations sont
  ceux de la charge complète, comme avant pour le linéaire (valeurs identiques au bit près, § 9).
- **Anciens réglages, jamais convertis en silence** :
  - `AnalysisContext::unsupportedAnalysisType` garde la clé lue (`nonlinear_static`, `pushover`…) et `toJson()` la
    réécrit telle quelle ;
  - `AnalysisEngine::unsupportedSettings` (nouveau, défaut vide) laisse chaque moteur nommer ses anciens réglages qui
    changeaient un calcul. OpenSees : `geomTransf` 1 ou 2, `trussFormulation` 1 ou 3. Les autres anciennes clés étaient
    sans effet sur un calcul linéaire et sont ignorées ;
  - `AnalysisManager::unsupportedFeatures` : `prepare()` rend une erreur, `run()` refuse aussi, même avec un modèle
    préparé valide.
- **Format `.tsa` inchangé** : chunk `SETT` en texte (la clé d'origine est conservée) ; chunk `CABL` à disposition
  identique, ses trois champs historiques lus puis ignorés et écrits avec leurs anciennes valeurs par défaut. Un fichier
  écrit par cette version s'ouvre dans les versions précédentes.
- **Banc** : `parseBenchmark` refuse tout type autre que `linear_static` et `modal`, ainsi que les moteurs inconnus ; un
  moteur facultatif absent de la compilation (Fem3D) fait ignorer le cas au lieu de l'échouer.

## 8. Tests exécutés

Poste : Windows 11, MSVC 19.51, Qt 6.12.0, OCCT 8.0.1, OpenSees 3.8.0. Binaires Release de `build-windows-validation`.
Toutes les exécutions ont eu lieu sans fenêtre visible : bureau Windows séparé, ou processus `CREATE_NO_WINDOW`.

| Exécution | Résultat |
|---|---|
| Compilation Release, `-j 4` | réussie, 0 erreur ; dépendances d'en-têtes Ninja valides (`#deps 35`, BUG-011) |
| `tools/ci/run_test_suites.py`, 57 suites | **57/57, 544 réussis, 0 échec, 1 ignoré** (test 487, fournisseur IA réel, comme avant) |
| Nouveaux tests | 532 ancien type refusé (`prepare` et `run`, type inconnu, clé absente) ; 533 anciens réglages OpenSees refusés, clés inertes ignorées, panneau limité ; 534 script sans aucune commande non linéaire, un seul `analyze` ; 535 aller-retour `.tsa` d'un ancien projet : ouvert, données conservées, message, aucun résultat, demande réécrite, bandeau, choix linéaire qui lève le refus ; 536 fenêtre Analysis sans libellé non linéaire ; 537 banc : `nonlinear_static` refusé à la lecture, P-Delta refusé au calcul |
| Tests adaptés | 131, 132, 135, 137, 476 (capacités, types proposés, réglages conservés) ; contrôles de conversion Tcl des algorithmes retirés de `test_opensees.cpp` |
| Empreinte des 9 exemples (`dump_all.py`, harnais recompilé) | **104/104 fichiers identiques à l'octet** |
| Bancs `TSA_Benchmark` | § 9 |
| Contrôles numériques du harnais | identiques à ceux d'avant |
| Performances (harnais `perf`) | § 9 |
| Fuzzing de la lecture, pont à haubans (64 câbles, chunk CABL) | 600 fichiers modifiés, **0 plantage**, 0 délai |
| Fuzzing de la voie de calcul, rejoué à l'identique (semence `TSA_Demo_Batiment_R+5.tsa`, graine 44, 400 mutations, comme l'audit) | **identique à l'audit** : mêmes 7 plantages préexistants (BUG-154, BUG-155) aux mêmes itérations, mêmes 393 cas gérés ; aucun nouveau plantage |
| Fem3D | bibliothèque `3DDesplacement` non modifiée (aucun code non linéaire à retirer) ; `fem3d_tests` non relancé. Côté TSA, les tests Fem3D de la suite `engines` (476-480, 530) et les cas `fem3d` du banc passent |
| `TSA_CoreTests` (entrée CTest `TSA_CoreLink`) | 5/5 |
| `tools/check_layers.py` | 0 violation |
| `tools/check_shortcuts.py --strict` | 283 commandes, 141 raccourcis cohérents |
| `tools/i18n/check_translations.py --strict` | 0 erreur ; manques préexistants seulement (4 en anglais, 23 en malgache) |
| TSALab (compilé après TSA, jamais en parallèle) | compilation 225 étapes, 0 erreur. `TSALab_TestSuite` sur bureau masqué : **27/27**, partie OpenSees comprise, après une correction du test L9. Première exécution : 21/27. L9 échouait sur le seul nombre de cas du banc partagé, figé à 28 alors que la mission en laisse 36, tous valides ; l'arrêt au premier échec a empêché L10 à L14 de tourner. Correction : attente 28 → 36, `lab/Tests/LabTests.cpp`, branche `refactor/linear-only` de TSALab, non commitée. `tsalab_science_tests` non relancé : le cœur scientifique n'est pas modifié |
| Debug | `build-ninja-debug`, configuré mais jamais compilé auparavant : compilation complète de 561 étapes, 0 erreur, les 35 mêmes avertissements préexistants qu'en Release. 12 suites liées au calcul, aux données et aux textes (engines, opensees, extraction, io, cables, loads, benchmarks, standards, ndc, audit, i18n, undo) sur bureau masqué : **126 réussis, 1 échec**. L'échec est le test 457, un seuil de temps de réponse : pire recherche dans un index synthétique de 6 000 entrées, 347 à 410 ms en Debug pour 50 ms exigés, 18 ms en Release. Il se reproduit sans aucune autre charge, et le code mesuré n'est pas modifié. L'arrêt au premier échec (BUG-104) a empêché les tests 458 à 463 de tourner en Debug. Les 45 autres suites n'ont pas été exécutées en Debug |
| Linux | **non vérifié** : aucun outil Linux ni distribution WSL sur ce poste. Le code retiré ne contenait rien de propre à une plateforme |
| Interface réelle | non lancée (aucune fenêtre sur ce poste, consigne de l'utilisateur). La fenêtre Analysis et le panneau OpenSees sont construits et lus hors écran par les tests 533, 535 et 536. Le message affiché par `MainWindow` à l'ouverture d'un ancien projet n'est couvert par aucun test automatique |

## 9. Résultats avant / après

### 9.1 Tests

| | Avant (audit) | Après |
|---|---:|---:|
| Suites | 57/57 | 57/57 |
| Tests réussis | 538 | 544 |
| Échecs | 0 | 0 |
| Ignorés | 1 | 1 |

### 9.2 Bancs, vérification par vérification

| Série | Avant | Après | Valeurs communes identiques |
|---|---|---|---:|
| Banc officiel `tests/benchmarks` | 146 réussies | **1 682 réussies**, 0 échec (36 cas) | 141 / 141 |
| Portiques aléatoires, solveur indépendant | 10 908 réussies, 1 770 échecs | 10 908 réussies, 1 770 échecs | 12 678 / 12 678 |
| Rotules (`cases-hinge`) | 69 réussies, 21 échecs | 69 réussies, 21 échecs | 90 / 90 |
| Cas supplémentaires (`cases-more`) | 1 181 réussies, 10 échecs | 1 181 réussies, 7 échecs, 3 non disponibles | 1 188 / 1 191 |
| Non linéaire (`cases-nl`) | 5 réussies, 4 échecs | 3 réussies ; 2 fichiers refusés à la lecture | 3 / 3 |
| Unités (`cases-units`, OpenSees en N) | 629 réussies, 583 échecs | 629 réussies, 583 échecs | 1 212 / 1 212 |

« Identiques » signifie même nombre en virgule flottante, au bit près. Les échecs restants sont ceux d'avant, avec les
mêmes valeurs : BUG-148 pour les rotules, la famille C et `PIN2` ; pour `cases-units`, forces et moments rendus en N
et comparés à des références en kN (rapport exact de 1 000, propre à cette série d'essai). Les seuls changements de
statut sont voulus :

- `PDELTA-LIN` (P-Delta demandé en statique linéaire, BUG-150) : 3 vérifications fausses sans alerte → calcul refusé ;
- `NLIN-uniforme`, `NLIN-trapeze` (BUG-149) : faux → refusés à la lecture ; `NLIN-control-lineaire` : mêmes valeurs.

Cas ajoutés au banc officiel, tous réussis :

| Cas | Origine (audit) | Moteurs | Écart max. |
|---|---|---|---:|
| RDM-13a, 13b, 13c | `HINGE-01-uniform`, `GERBER-01-uniform`, `PIN2-uniform` | OpenSees, Fem3D, Custom2D | 8e-14 % |
| RDM-14a à 14d | `HINGE-02-point`, `HINGE-03-trap-constant`, `HINGE-04-tri-up`, `PIN2-point` | Fem3D, Custom2D (OpenSees exclu : BUG-148) | 1,5e-7 % |
| FEA-08, FEA-09 | portiques aléatoires A72, B87 | OpenSees, Fem3D | 5e-8 % |
| FEA-10 | portique aléatoire à rotules C74 | Fem3D (OpenSees exclu : BUG-148) | 2e-9 % |

### 9.3 Performances

Harnais `perf`, même machine, plage des mesures disponibles (ms ; avant : `preuves/resultats/perf-api.txt` de l'audit).

| Portique | Mesure | Avant | Après |
|---|---|---:|---:|
| 1 125 barres | enregistrement | 92-95 | 91 |
| | calcul OpenSees | 144-147 | 144-146 |
| | calcul Fem3D | 72-74 | 68 |
| 8 330 barres | enregistrement | 424-481 | 408-425 |
| | ouverture | 136-183 | 128-131 |
| | calcul OpenSees | 1 431-1 433 | 1 425-1 444 |
| | calcul Fem3D | 4 638-4 660 | 4 647-4 764 |
| 29 200 barres | enregistrement | 1 927 | 1 879 |
| | calcul OpenSees | 18 252 | 17 672 |
| | calcul Fem3D | 117 301 | 104 896 |

Aucun écart au-delà du bruit de mesure. Le premier enregistrement de la première série (403 ms) était un démarrage à
froid ; la série répétée donne 91 ms.

## 10. Risques et limitations restants

| Sujet | État | Conséquence |
|---|---|---|
| **BUG-148** (CRITICAL) | ouvert, inchangé | OpenSees, moteur par défaut : efforts faux sans alerte pour une barre à rotule portant une charge ponctuelle, trapézoïdale ou triangulaire. Fem3D et Custom2D sont justes. À traiter en priorité |
| **Câbles** (`corotTruss` précontraint) | conservé tel quel, limite mesurée | résolu en un seul pas linéaire. La raideur géométrique de la précontrainte est incluse : une corde de 2 × 5 m sous 20 kN donne exactement la raideur 2·T₀/a. Mais l'effort est recalculé sur la géométrie déformée : 26,25 kN au lieu de 20 kN et somme des réactions 1,31 kN pour 1 kN appliqué quand u/a = 2,5 %. Le résidu croît comme (u/L)² et vaut environ 2 × 10⁻⁵ sur le pont à haubans d'essai. Changer de formulation changerait des résultats validés : **décision à prendre** (docs/cable-system/analysis.md, `preuves/linear-only/resultats/cable-probe.txt`) |
| BUG-151 | ouvert | un câble comprimé reste actif, sans avertissement du calcul ; `tensionOnly` est une donnée descriptive |
| BUG-152, 153, 154-168 | ouverts, inchangés | voir le registre |
| Anciens projets non linéaires | refusés jusqu'au choix de l'utilisateur | un projet `nonlinear_static` ne calcule plus sans intervention : c'est voulu. Ses résultats non linéaires antérieurs ne sont pas reproductibles dans cette version |
| Anciens types dynamiques (`pushover`…) | désormais refusés | ADR-022 les relisait en statique linéaire ; ADR-025 remplace cette règle |
| Test de convergence | constante `1e-06 50` | la tolérance enregistrée n'est plus reprise. Sans effet : `algorithm Linear` n'évalue pas le test |
| TSALab | partage `src/` | message au calcul seulement : pas d'avertissement à l'ouverture d'un ancien projet dans TSALab. Énumérateur `NonLinearStatic` inutilisé et docs TSALab périmées (`README.md` lignes 261-263, `docs/SOLVER_API.md`, `docs/AUDIT_TSA_TSALAB.md`), dépôt séparé non modifié |
| Traductions malgaches | écrites pour cette mission | à relire par un locuteur |
| Debug, test 457 (BUG-169, LOW) | seuil de temps non tenu en Debug | test de performance calibré pour une version optimisée ; à rendre conditionnel au type de compilation, ou à mesurer en Release seulement |
| Interface réelle, Linux, ASan | non exécutés | voir § 8 |
| Vérifications réglementaires | inchangées | partielles ; ne pas les présenter comme validées parce que l'analyse est linéaire |

## Reproduire

Depuis un dossier hors dépôt, sans fenêtre (scripts dans `preuves/scripts` et `preuves/linear-only/scripts`) :

```powershell
python build_hidden.py <build> <journal>                              # vcvars64 + cmake --build -j 4, sans console
python run_hidden_desktop.py <journal> <TSA> python tools/ci/run_test_suites.py --exe build-windows-validation/TSA_TestSuite.exe --out <dossier> --available opensees,gmsh,ifcopenshell,display --timeout 600
python dump_all.py <harnais.exe> <dossier> ; python compare_dumps.py <avant> <après>
python hidden_bench.py <cas> <sortie> <travail> <journal> ; python compare_bench.py <avant/latest.json> <après/latest.json>
python perf_run.py <harnais.exe> <travail> <journal> 3
python cable_probe.py <travail>                                        # câble précontraint, corotTruss contre truss
```
