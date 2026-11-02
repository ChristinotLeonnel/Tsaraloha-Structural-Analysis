# Tasks

Last Updated: 2026-11-02 soir. Avant : 2026-11-01. Uniquement des tâches réellement identifiées (voir known-issues.md).

## État du projet dans le .tsa (2026-11-01, branche fix/save-dirty-state, plan docs/PROJECT_STATE_PLAN.md validé)
- [x] Étape 0 : BUG-074 (test 573). [x] Étape 1 : socle, blocs d'état, format 3.1 (test 574).
- [x] Étape 2 VSTA (test 575).
- [x] Étape 3 UIST (test 576).
- [x] Étape 4 RSLT (test 577) ; mesure taille / durée sur un grand modèle à faire.
- [x] Étape 5 DIAG (test 578).
- [x] Étape 6 PMAN (test 579) ; [x] étape 7 documentation (TSA_FILE_FORMAT § 5.w, README, ADR-026).
- [x] BUG-175, BUG-176 corrigés (2026-11-02, tests 594, 597).
- [ ] Suites : boîte des propriétés du projet, export de la note de calcul relié au manifeste, mesure RSLT sur un grand modèle ; essai manuel dans TSA.exe (Ctrl+S, réouverture).
- [x] TSALab 27/27 après étape 2 et correctif des noms de dépôts (branche fix/tests-repo-name). Fusion et push non demandés.

## Mise en place en un seul fichier (2026-11-01, branche fix/setup-build-mingw-path)
- [x] install_dependencies.py : composant tsalab, --configure ; TSAProduct.cmake : noms du dépôt TSALab ; correctifs scripts/*.ps1.
- [x] Côté TSALab : TSA_ROOT_DIR cherché sous ../TSA et ../Tsaraloha-Structural-Analysis (branche fix/tsa-root-discovery).
- [x] scripts/setup_build.ps1 + detect_*.ps1 + install_mingw.ps1 supprimés (demandé le 2026-11-01) ; commit, fusion, push demandés.
- [ ] Compilation complète TSA / TSALab non relancée après ces changements (configuration seule vérifiée).

## Dépendances en une fois, disposition thirdparty/ (2026-11-01, branche build/auto-dependencies, TSA et TSALab)
- [x] install_dependencies.py, cmake/dependencies.json, installation Linux depuis CMake, chemins thirdparty/ (TSA, TSALab), documentation.
- [x] Essais Windows : --list ; installation Windows et logique Linux dans des dossiers d'essai ; configuration CMake (dossier neuf).
- [ ] Déplacer réellement opencascade-8.0.1-vc14-64 et 3rdparty-vc14-64 sur ce poste (refusé : dossiers verrouillés) : relancer
      `python install_dependencies.py` ou CMake après fermeture du programme qui les tient (au besoin après redémarrage).
- [ ] Linux : exécuter le script de bout en bout (apt, compilation OCCT et IfcOpenShell, OpenSees linux-64 et sa commande de contrôle),
      puis compiler et tester TSA ; chemin avec espaces, volume NTFS.
- [x] Commit (TSA 38e7142, TSALab 938c9eb), fusion dans main (TSA 321f870, TSALab 02cf02c) et push, demandés le 2026-11-01.

## Correction des bugs de known-issues, du plus critique au moins critique (2026-11-02)
- [x] BUG-152 (HIGH, verdict non sécuritaire) : flambement EC3 sur les deux axes principaux, test 584.
- [x] BUG-151 (HIGH) : câble comprimé ou détendu signalé après le calcul, test 585.
- [x] BUG-153 (HIGH) : cornières sur leurs axes principaux (OpenSees et Fem3D), test 586.
- [x] Moteur Fem3D trouvé dans ../TSA-3D-FEM-Engine (60/60 suites, banc 2 097 vérifications).
- [x] BUG-133 (HIGH) : OpenSees installé depuis l'application par conda-forge (installation réelle vérifiée).
- [x] BUG-111 étapes 1-2 : trace du calcul (empreinte, script conservé, .tsa, notes), test 587.
- [x] BUG-117 P2 : affichages des résultats en kN quelle que soit l'unité du calcul, test 492 complété.
- [x] BUG-002 clos (déjà corrigé par le maillage ; fonction morte retirée).
- [x] BUG-154, BUG-155, BUG-156 corrigés ; BUG-111 terminé (annexe du projet), tests 588-591.
- [x] Branche fix/autosave-recovery (2026-11-02, non commitée) : BUG-157 sauvegarde automatique et récupération (test 592),
      BUG-171 (593), BUG-175 (594), BUG-075 (595), BUG-115 / BUG-121 (596), BUG-176 (597), BUG-147, BUG-077, BUG-102, BUG-138,
      BUG-159, BUG-162 à 166, BUG-117 P3 (g unique), BUG-169/104, BUG-173, BUG-174/179 (tests stabilisés) ; traductions en / mg 100 %.
- [x] TSALab (branche fix/opensees-timeout, non commitée) : BUG-160, OpenSeesPlanarSolver arrêté après le délai.
- [ ] Commit, fusion et push des deux branches : accord demandé.
- [ ] Restent ouverts, hors portée d'une correction ici : voir known-issues.md (décisions : BUG-144, 140, 167 ; autres dépôts :
      BUG-161, 158, 076 ; environnement : BUG-012, 021, 023, 033, 036, 168, 137 Linux ; tests intermittents : BUG-079, 123, 170 ;
      grands chantiers : BUG-006, 008, 028, 118 E1-E5 ; BUG-117 équilibre Custom2D ; BUG-124 Windows sans OpenSees).
- [ ] Traductions en / mg manquantes (88 / 107 textes, dont Style des charges, menus d'affichage) : tools/i18n.

## Affichage intelligent des charges (2026-11-01/02, branche feature/load-display-scaling)
- [x] Échelle d'après la structure, style des charges, cas affichés, test 583.
- [x] BUG-177 : textes démesurés (texte zoomable OCCT) — pixels bornés, valeurs superposées masquées, calque Topmost.
- [x] Commit, fusion dans main, push (accord du 2026-11-02).
- [ ] Contrôle à l'œil dans TSA.exe.
- [x] BUG-178 : même limite pour le mode lié au modèle d'AnnotationStyle (échelle de la vue, caméra conservée, test 373).

## Repères locaux, rotation de section, axes des charges (2026-10-31, branche refactor/load-axes-section-rotation)
- [x] Mesures Robot (tests/data/robot_rsa2024_reference.json), repère et γ, axes des charges, migration, fenêtres, tests 558-565.
- [x] Reprise : tests 101, 210, 350, 374, 375, 506 et banc (surfaces) adaptés ; surcharges « nom » de MemberLoad rétablies ; traductions.
- [x] Release : 60/60 suites, 572 tests ; banc 2 097/2 097 ; TSALab 27/27 (AnalyticalReference, ExampleModels).
- [ ] Empreinte des 104 fichiers d'exemples et bancs au bit près contre main (scripts dump_all / harnais à retrouver).
- [ ] Contrôle à l'œil dans TSA.exe des fenêtres de charge (barre, propriétés, surfaces) ; Debug ; Linux.
- [ ] Repère local des coques et voiles comparé aux panneaux de Robot ; écart de 2,3 % sur les déplacements (effort tranchant ?) à confirmer.
- [ ] Exemples examples/TSA_Projet_Batiment_Complexe_L.tsa et Pont_Haubans modifiés le 30/10 (hors de cette tâche) : à décider.
- [x] Validation à neuf (2026-11-01, Release reconstruite), commit, fusion dans main et push (TSA et TSALab).

## Copier / Coller (2026-10-31, branche fix/copy-paste-complete)
- [x] Audit, moteur unique, image des charges, tests 549-557, traductions, documentation, rapport.
- [x] Décision : rotation de section en symétrie et en rotation : règle de Robot, γ recalculé (refactor/load-axes-section-rotation) ; « gravité » retirée.
- [ ] BUG-171 : symétrie sans copie d'une dalle avec ouvertures ou charges en (u, v).
- [ ] Contrôle à l'œil dans TSA.exe (Ctrl+C / V, message sur une charge seule), Debug, Linux, fuites mémoire.
- [x] Commit (295c920), fusion dans main et push (2026-11-01, avec refactor/load-axes-section-rotation).

## Repères de la vue 3D (2026-10-31, branche feature/coordinate-axis-gizmos)
- [x] Calcul des repères, trièdre partagé, vue, menu, tests 541-548, traductions en / mg, documentation.
- [ ] Contrôle à l'œil dans TSA.exe (barre d'état au-delà de 32 éléments, menu Repères), Linux, Debug.
- [x] Commit, fusion dans main et push demandés par l'utilisateur le 2026-10-31.

## Version linéaire seule (ADR-025, 2026-10-31, branche refactor/linear-only)
- [x] Inventaire, retrait du non linéaire (moteur, OpenSees, contexte, interface, données, textes), refus des anciens projets.
- [x] Tests 532-537 ; RDM-11a/b supprimés ; RDM-13a-c, RDM-14a-d, FEA-08 à 10 ajoutés au banc ; traductions en / mg.
- [x] Release : 57/57 suites, 544 tests ; empreinte des exemples identique à l'octet ; bancs identiques au bit près ; performances.
- [x] Documentation (README, DOCUMENTATION, docs/*), mémoire, rapport LINEAR_ONLY_REFACTORING_REPORT.md.
- [x] Commits (TSA a7b163e, c3cf5f6 ; TSALab 1119d54), fusion dans main et push demandés par l'utilisateur le 2026-10-31.
- [ ] Linux non vérifié ; Debug : BUG-169 (test 457, seuil de temps). Voir le rapport (§ 8).
- [ ] Décision : câbles corotTruss précontraints résolus en un pas (raideur de précontrainte incluse, effort recalculé sur la
      géométrie déformée) : garder, ou changer de formulation (change des résultats validés) — docs/cable-system/analysis.md.
- [ ] TSALab : énumérateur tsalab::planar::AnalysisType::NonLinearStatic inutilisé ; README / SOLVER_API / AUDIT_TSA_TSALAB périmés.

## Audit technique du 2026-10-31 (docs/audit/2026-10-31/TSA_REMEDIATION_ROADMAP.md)
- [x] Audit, 9 rapports, preuves ; branche docs/audit-technique-2026-10-31, commit a7b163e, fusion dans main demandée le 2026-10-31.
- [x] BUG-148 corrigé (forces d'encastrement condensées, tests 538-540, OpenSees réintégré au banc).
- [ ] Phase 0, mise en sécurité : BUG-151 à BUG-153 (refus ou correction). BUG-149 et BUG-150 clos par retrait
      (ADR-025) ; cas de rotules et portiques aléatoires ajoutés au banc pour les moteurs qui les passent.
- [ ] Revoir les résultats déjà produits avec rotules + charge non uniforme, ou en non linéaire.
- [ ] Non exécuté : TSALab_TestSuite, tsalab_science_tests S6 (non concluant), fem3dpost_tests (délai), ASan, Linux, Debug, interface réelle.
- [ ] Régénérer les 26 journaux vidés de TSA-3D-FEM-Engine/benchmarks/OpenSees/results (après correction de BUG-161).
- [ ] Décisions : ADR-022 et modal, moteur par défaut, note Legacy, câbles, licences.

## Courbes 2D superposées (2026-10-30, branche feature/diagram-multi-curves)
- [x] Diagram2DWidget multi-courbes, échelles, export, sélection 3D ; test 531 ; traductions ; docs/UI.md.
- [ ] Essai dans TSA.exe réel (sélection d'une barre → dock, puces, export).
- [x] Commit 5403ed6, fusionné dans main avec refactor/linear-only le 2026-10-31 (choix de l'utilisateur).
- [x] BUG-146 : Qt 6.12.0 et OpenSees réinstallés ; build-windows-validation Release, 57/57 suites (2026-10-31).
- [ ] BUG-147 : noms des grandeurs de DiagramGeometry traduisibles.
- [ ] Autres bugs signalés par l'utilisateur (« plusieurs bugs ») : liste à obtenir.

## Fem3D 0.3.0 — charges sur barre (2026-10-29)
- [x] Moteur : ponctuelles, partielles, moments répartis ; tests 49/49 ; benchmarks 21-22 ; documentation Fem3D.
- [x] TSA : adaptateur, test 530, test 478 adapté ; 57/57 suites ; TSALab 27/27.
- [x] Commits et fusion dans main (TSA 5c943af, Fem3D ca4513d), puis push (2026-10-29).
- [ ] Niveau 4 des charges sur barre (audit indépendant) ; référence pour Timoshenko et moments répartis.
- [ ] BUG-144 : avenir de l'option « secours OpenSees ».

## Espace de travail 3D / Blueprint (2026-10-29, branche feature/blueprint-workspace, fusionnée dans main)
- [x] WorkspaceView (3 modes, partage H/V, préférences), navigation du Blueprint, ruban, raccourcis, icônes, traductions ; tests 524–529.
- [x] Suite complète 57/57 (528 tests, run_test_suites.py, 149 s), TSALab 27/27, check_shortcuts --strict, check_layers.
- [x] Essai graphique réel de MainWindow (UI Automation + captures) : modes, panneaux, focus, défaut, relance, Annuler / Rétablir.
- [x] Recherche des nœuds placés (groupe distinct, clavier).
- [ ] BUG-139 (vue partagée à la taille minimale), BUG-140 (Ctrl+Z dans le Blueprint : décision), BUG-141 (mini-carte).
- [ ] Compilation et tests Linux ; CI non consultée (gh absent du poste).
- [ ] BUG-143 : répondeur modal global pour les tests, délai du solveur OpenSees.

## Validation Windows (2026-10-28, docs/reports/WINDOWS_VALIDATION_REPORT.md, branche test/windows-validation)
- [x] Compilation MSVC 19.51 Release de C (6a5bde1) + TSALab eea9cd9 ; 42 suites ; Custom2D 58/58 ; W-1, W-2, W-5 corrigés.
- [ ] OpenSees : remettre l'exécutable (serveur officiel en 404, W-3) puis relancer les 14 suites, le banc et TSALab L9-L14.
- [ ] W-6 : TSALab LabTests.cpp:120 → TSA::Core::utf8Path (autre dépôt, accord requis).
- [x] W-8 corrigé et vérifié ; W-7 corrigé, compilé — [ ] à exécuter quand OpenSees sera présent (suite torsion, test 332).
- [x] OpenSees conda-forge VALIDÉ (2026-10-29) : 57/57 suites, banc 88/88, TSALab 27/27 (openblas.dll ajouté).
- [x] W-3 : SHA-256 + curl --fail dans OpenSeesManager ; TSA_OPENSEES_ARCHIVE ; OpenSees 3.8.0 conda-forge épinglé
      (TSA_OPENSEES_CONDA_PACKAGES, thirdparty/OpenSees installé) ; [ ] EXÉCUTION non validée (lancement refusé par le
      contrôle de sécurité de la session) : 14 suites OpenSees, banc, TSALab L9-L14 à lancer.
- [ ] OpenSeesManager::downloadAndInstall vise encore l'archive officielle (404) : basculer vers conda-forge ou retirer.
- [x] W-9 : presets sans Qt6_DIR, détection automatique (branche build/dependency-detection, non commitée).
- [x] W-10 Logger non ASCII corrigé (test 37.8).
- [x] TSALab : tsa_find_qt(), presets sans Qt figé, W-6 ; plancher Qt conservé à 6.2 (décision 2026-10-28).
- [ ] Dépendances : Linux NON TESTÉ (scripts PowerShell setup_build/detect_* retirés le 2026-11-01) — rapport §11.8.
- [ ] Procédure PROCEDURE_WINDOWS_v2 (A1, A2 dumpbin, B, B sans IFC) : NON exécutée ; Debug non testé ; commit à autoriser.

## TSA Core — préparation de l'étape 2 (2026-10-26) ; état au 2026-10-29 : C fusionné dans main (f31ca9c, poussé), B repris
## sur refactor/core-edit-api (validé MSVC, avec et sans IfcOpenShell), A repris (2749db0 : contrôle ELF, non enregistré sous
## MSVC — configuration et CTest Windows vérifiés) ; A2 (moteur dumpbin) : patch du worktree Linux, inaccessible ici, non appliqué

Branches distinctes, base 39155c4 (TSALab : 58063a7) ; preuves dans ../TSA-rapports/2026-10-26/mission3 à mission10.
Commits locaux : A 9a45d46, B 8695035, TSALab B1 eea9cd9, C 3b65ac3, documentation bf88548 (docs/architecture-tsa).
- [x] A — `test/core-static-init-sections` (worktree ../TSA-worktrees/core-static-init) : contrôle par sections ELF
      (init et terminaison), refus LTO, codes 0/1/2, CTest `<P>_CoreStaticInitSelfTest` (38 cas) ; Linux 3/3 ; contre-épreuve
      sur libTSA_Core.a (constructor et init_priority manqués par l'ancien script, détectés). ci.yml inchangé (le filtre
      « CoreStaticInit » sélectionne déjà le SelfTest). [ ] Clang. MSVC : 9a45d46 ne l'enregistre pas ; moteur dumpbin (A2)
      préparé NON COMMITÉ dans le worktree A (patch mission10_integration/A2_msvc_dumpbin_v2.patch), Linux 3/3, jamais
      exécuté sous Windows : [ ] premier essai Windows, puis commit.
- [x] B — `build/model-deps-explicites` (worktree ../TSA-worktrees/model-deps) + TSALab `build/tests-qtwidgets-explicite` :
      B1 QtWidgets explicite (TSALab_Tests compris), B2 Model ne lie plus QtWidgets ni l'OCCT de rendu
      (`$<COMPILE_ONLY:Qt6::Widgets>` à la même place : commandes de compilation identiques). Linux : suite identique à la base,
      banc 146/146, TSALab 27/27, NEEDED identiques. [ ] PCH MSVC (cmake_pch.cxx.obj lié dans tsa-mesh / tsa-ifc /
      tsa3d_roundtrip_example), avec et sans IfcOpenShell ; C4605 / C4651 ; version de CMake de la CI Windows.
      [ ] TKG2d, TKBO, TKBool, TKMesh : sans utilisateur direct, conservés (tirés par TKV3d / TKOffset, greffon IFC).
- [x] C — `test/etape2-caracterisation` (worktree principal) : suite « undo », tests 513-523 ; Linux Release 11/11 (×3 xcb,
      ×1 offscreen), suite complète 521/522 (128 Windows seul), 8 mutations de production détectées puis restaurées.
- [ ] Validation Windows / MSVC (A1, A2, B, B1, C) : procédure ../TSA-rapports/2026-10-26/mission10_integration/
      PROCEDURE_WINDOWS_v2.md ; RIEN d'exécuté sous Windows.
- [x] Commits locaux autorisés et faits (2026-10-26) ; [ ] push et fusion : après la validation Windows (plan d'intégration
      dans mission10_integration).
- [x] Documentation d'architecture (docs/architecture/, Mermaid + SVG) : branche docs/architecture-tsa, commit bf88548.
- [x] Étape 2 (2026-10-29, branche refactor/core-edit-api, mission autonome autorisée) : contrat des transactions (BUG-127),
      clé de coalescence (F1), hasChanges ; 8 panneaux + charges (F1), chargements (F2), appuis (F3), grilles (F4),
      création / bibliothèque (F5), IA (F6) migrés : plus aucun pushUndoState dans l'interface. BUG-125 à 129 corrigés,
      BUG-130 partiel (voir known-issues). Suite undo 11/11, 29 suites sans OpenSees 29/29, TSA_CoreTests 5/5.

Lacunes connues du filet C (non couvertes, ne pas les considérer comme testées) :
- MainWindow_Mesh.cpp:326, :359 et MainWindow_Tools.cpp:464 (couche application, non liée aux tests) ;
- exceptions dans les slots appelés par invokeMethod ; branche « column » de AIToolRegistry ; poteaux, dalles et voiles des
  gabarits de LibraryManager ; ordre exact des ModelEdited (volontairement non contractuel) ;
- notifications non vérifiées positivement : seules DimensionsChanged, ModelCleared, ModelDestroyed et les Added / Removed par
  l'API du modèle (test 523) et par les chemins de production treillis / câbles / charges (521) le sont ;
- empreinte (tests/undo_characterization.h) : PAS un comparateur exact — exclut les réglages hors historique (vues
  nommées, JSON d'analyse / topologie / maillage / échanges), les plans de travail, les champs privés sans accesseur, le
  contenu des MechanicalSnapshot (clés seules), l'état hors modèle ; BIM et affichage des grilles exclus par défaut.

## TSA Core — étape 1 : extraction de TSA_Core (autorisée le 2026-10-26, non commitée)

- [x] Cible `<P>_Core` (STATIC, QtCore + TKernel + TKMath), 50 sources ; `<P>_Model` la lie en PUBLIC (TSA et TSALab).
- [x] Inversion des catalogues : `Model/InstalledCatalogs`, fournisseur enregistré par ExtensionSystem.
- [x] `<P>_CoreTests` / CTest `<P>_CoreLink` : lié au seul noyau ; contre-épreuve (ancien Section.cpp → échec de liaison).
- [ ] Compilation MSVC (CI Windows après push, puis poste MSVC 19.51 + Qt 6.11.2).
- [x] Revue : `SemanticVersion`, `DefinitionReference`, `MechanicalSnapshot` définis une seule fois dans le noyau
      (`Model/DefinitionReference.h`), alias dans `ExtensionSystem/ExtensionTypes.h` ; `NamedViews` dans le noyau ;
      aucun en-tête du noyau n'inclut un fichier hors du noyau ; test 512 (fournisseur, ordre, numérotation, repli).
- [ ] Arête Grid/SnapMarker → Viewer/TextDisplayFallback.h (couche graphique, hors noyau) ; `CoreLink` hors CI (ctest -R).
- [x] Étape 2 : réalisée le 2026-10-29 (voir « préparation de l'étape 2 »).

Points encore ouverts de l'étape 0 (2026-10-26) : validation Windows MSVC 19.51 / Qt 6.11.2 ; BUG-121 (2) et (3) ;
BUG-123 ; essais manuels 9, 11, 13, 14, 15 ; CI de main sur a8b310a (40306365407).

## TSA Core — phase 0 : intégrité (approuvée le 2026-10-25, branche fix/core-integrity)

- [x] BUG-105 unités EC2/EC3 et note de calcul (test 492).
- [x] BUG-106 panneaux Propriétés : seuls les champs modifiés sont réécrits (test 493) — essai GUI à faire.
- [x] BUG-107 section du ruban annulable (test 494).
- [x] BUG-108 injection Tcl (test 495).
- [x] BUG-109 paquets .tsalib : id, chemins, tailles (test 496) ; [ ] extraction atomique, masquage.
- [x] BUG-110 rayon de congé : format .tsa 2.0 (chunk SECP), voir la ligne BUG-110 plus bas (commit ea646c3).
- [ ] BUG-111 traçabilité : manifeste de résultats, versions lues, empreinte, RSLT, annexe nationale figée.
- [x] BUG-112 ModelDiff dalles/voiles (test 497) ; [x] BUG-113 notifications (test 498) ; [ ] BUG-115 poteau « Tube ».
- [x] BUG-114 OpenSees : OFF par défaut, détection, aucun téléchargement implicite ; [ ] empreinte de l'archive, licence.
- [x] CI préparée : .github/workflows/ci.yml (Linux + Windows), tools/ci/run_test_suites.py (rapport par suite,
      dépendances absentes = NON EXÉCUTÉE), tools/ci/check_dependencies.py ; [ ] premier passage sur GitHub (push requis),
      épinglage des actions par SHA, OpenSees et Gmsh en CI.
- [x] BUG-117 P1 : IFC, Fem3D/OpenSees, TSA3D (tests 499-501, références avant/après) ; [ ] P2 affichages « kN », P3.
- [x] BUG-118 étape E0 (test 505) ; [ ] E1 registre qualifié / provenance, E2 magasin versionné, E3 signatures, E4, E5.
- [x] BUG-111 étape 0 (tests 503-504) ; [ ] étapes 1-2 (empreintes, AnalysisTrace en mémoire) ; [ ] 1.14 après validation.
- [x] BUG-113 : tests de régression ajoutés (502 ouvertures, 444 manipulateur).
- [x] BUG-110 : format 2.0 (SECP, requiredFeatures, migration, IFC/TSA3D, panneau) ; tests 506-508 ; démonstration
      au bit près sur le corpus. [ ] CI de la nouvelle version (push).
- [x] BUG-122 : cause établie (cdb + gdb), repli DEKALE sans core20fwd appliqué localement (tests 510-511) ; [ ] commit,
      push et CI Windows (accord à demander) ; [ ] signalement OCCT (décision de l'utilisateur).
- [x] BUG-119 (local, test 509) ; [x] BUG-120 (local) ; [ ] validation CI Linux + Windows (push à autoriser).
- [x] Essais d'interface : 11/16 exécutés (Xvfb, xdotool), défaut BUG-121(1) corrigé ; [ ] essais 9, 11, 13, 14, 15 ; [ ] Windows.
- [ ] Compilation MSVC 19.51 + Qt 6.11.2 : impossible sur ce poste Linux (aucun poste Windows accessible).
- [x] CI Windows (Qt 6.10.3, MSVC 19.44) : compilation complète, 37 suites réussies, 328 tests, test 128 RÉUSSI pour la
      première fois, Custom2D 58/58 (CI 40270307383) ; views plantée (BUG-119). [ ] Compilation sur le poste MSVC 19.51.
- [x] tools/ci/run_test_suites.py : plantage Windows (code ≥ 0xC0000000) classé PLANTÉE.
- [ ] D5 : compilation et tests Windows/MSVC, CI GitHub (push requis), essai manuel des panneaux Propriétés, traductions
      (8 textes nouveaux non traduits en et mg, .ts non modifiés) — NON EXÉCUTÉS.
- [ ] BUG-110 (A+ : r figé, chunk SECP, 1.13 / 2.0 conditionnelle) et BUG-111 (fiche AnalysisTrace, 4 empreintes,
      chunks TRCE/RSLT/EINP, 1.14) : conceptions livrées, EN ATTENTE D'APPROBATION.
- [ ] ADR : Gmsh (processus séparé ?), OpenSees (redistribution), calcul modal (ADR-022 contredit par le code).
- [x] Compiler et tester TSALab sous Linux (dossier temporaire, hors dépôt) : 27/27 le 2026-10-26 après les modifications de src/.

## Portage Linux (docs/LINUX_BUILD.md)

- [x] OCCT 8.0.1 Linux, presets linux-debug/linux-release, scripts/linux/, build Debug + Release (2026-10-24).
- [x] BUG-091 (locale numérique), BUG-092 (suites masquées), BUG-093 (OpenSees .exe sous Linux).
- [x] libxcb-cursor0, lancement de TSA et suite complète en xcb (2026-10-24).
- [x] OpenSees 3.8.0 Linux compilé et validé (docs/LINUX_BUILD.md §7).
- [x] BUG-095 : Gmsh (BUG-100, BUG-101) et IfcOpenShell (BUG-103) sous Linux ; Release 489/490 (2026-10-25).
- [x] BUG-099 : vérification de l'exécutable OpenSees (test 491) — comportement Windows encore à valider.
- [ ] BUG-102 : réglage du chemin d'OpenSees dans l'interface (setCustomExecutablePath sans appelant).
- [ ] BUG-104 : test 457 (seuil 50 ms) en Debug ; un échec masque les tests 458–463.
- [x] BUG-094 : résolution du suffixe .exe (ConverterSpec::executable), BUG-096 textures, BUG-097 tests.
- [x] BUG-098 : .ts à jour, 128 textes traduits (en, mg), 15 « vanished » retirés après reprise (6540/6540).
- [ ] Vérifier la compilation Windows (MSVC) et TSA_TestSuite des modifications des 2026-10-24 et 25 (§8).

## BIM (ADR-020, docs/BIM_ARCHITECTURE.md)

- [x] Phases 1–3 : noyau BIM, identifiants stables, mapping physique → analytique, chunk BIMM (tests 170–176).
- [x] Export IFC 4.3 (STEP) + validation IfcOpenShell ; import IFC ; test aller-retour (tests 177–180).
- [ ] BUG-028 : charges / combinaisons IFC (IfcStructuralLoadGroup…), relâchements, unités dérivées, grilles.
- [ ] Repère de grille « B-3 » des produits (information BIM, GridManager hors Model).
- [ ] Validation BIM pré-calcul, sous-ensemble IDS, abstraction BCF, API IA contrôlée.
- [x] UI : actions Exporter / Importer IFC (essai GUI 2026-10-06).
- [ ] UI : propriétés BIM (GlobalId, catégorie, Psets) dans le panneau Propriétés.
- [x] BUG-029 : métadonnées BIM au collage (test 190, 2026-10-07).
- [ ] Docs restantes : BIM_GUIDELINES.md, AI_API.md, VERSIONING.md (IFC_MAPPING.md fait).

## Fenêtre / Start Center (ADR-021)

- [x] AppShell, barre de titre, Start Center, Nouveau projet, Fermer le projet (essais GUI 2026-10-06).
- [x] BUG-030, BUG-031, BUG-032, BUG-034 et redimensionnement par les bords en mode Workspace (2026-10-06).
- [x] Crash en réduisant la fenêtre agrandie (FIX-2026-10-07-MINIMIZE, 2026-10-07).
- [ ] BUG-033 : vérifier multi-écrans / DPI mixte ; Snap layouts Windows 11 (HTMAXBUTTON).

## Statique seul (ADR-022, branche feature/static-only)

- [x] Retrait du modal / pushover / temporel (code, UI, NDC, tests) ; archive `archive/dynamique` + tag `v0.2.0-dynamique` (locaux).
- [ ] Commit, fusion dans `main`, push (et push de `archive/dynamique` + tag) — sur accord de l'utilisateur.
- [ ] Vérifications GUI : ruban sans entrées dynamiques, calcul statique depuis F5, progression / Annuler.

## Écosystème Tsaraloha (ADR-024) — feuille de route

- [x] Phases 1–6 : bibliothèques partagées, ProjectSession, IDE TSALab, cœur scientifique, registre de commandes, Blueprint.
- [x] Fusion dans main (locale, 2026-10-08) : feature/tsaraloha-shared-libs (TSA) et feature/scientific-ide (TSALab). Push : sur demande.
- [x] Console de TSA : commandes du registre (identifiant à point) et COMMANDES (2026-10-08). Les raccourcis (FIT, ISO…) restent.
- [ ] Outils IA (AIToolRegistry) : outils de lecture adossés au registre de commandes.
- [x] Phase 6 : Blueprint — runtime typé, .tsbp, éditeur dans TSALab (2026-10-08).
- [x] Phase 7 : AnalysisController (session), AnalysisManagerPanel, K·U = F (Custom2D), espaces Analyse / Résultats /
      Recherche de TSALab, commandes analysis.run / results.* (2026-10-08). M et C : sans objet (statique seul, ADR-022).
- [ ] TSA : proposer l'AnalysisManagerPanel dans un dock (aujourd'hui fenêtre Analysis + ruban).
- [x] Phase 8 : débogueur Blueprint, tâche de fond, historique de l'éditeur, API de plugins, IA sur Blueprint (script,
      propose_blueprint), OpenSees dans tsalab_science (2026-10-08).
- [ ] Vérifier en GUI avec un modèle de langage configuré : propose_blueprint de bout en bout (TSA et TSALab).
- [ ] OpenSeesPlanarSolver : rotules et charges partielles / trapézoïdales (eleLoad partiel selon la version d'OpenSees).
- [ ] Plugins : signature / liste blanche des DLL avant diffusion hors poste de développement.
- [ ] BUG-036 (cache CMake). [x] BUG-037 corrigé.
- [x] Blueprint : sous-graphes réutilisables (moteur, fichier v3 / .tsbps, éditeur), tests 392–393 (2026-10-17).
- [x] Blueprint : import d'une définition déjà présente au contenu différent — remplacée par celle du fichier (geste
      délibéré), nouvelle version, avertissements, annulable (test 393).

## CRITICAL

- [ ] (aucune tâche critique ouverte)

## HIGH

- [ ] BUG-002 — Mailler dalles/voiles pour le calcul (l'avertissement explicite avant calcul est fait le 2026-10-04).
- [x] BUG-011 — lanceur généré à la page de code de la génération (2026-10-07, `ninja -t deps` vérifié).

## MEDIUM

- [x] BUG-001 — calcul en thread + progression annulable (code) ; [ ] vérifier en GUI : progression, Annuler, fermeture refusée pendant le calcul.
- [x] BUG-003 — niveaux / axes / grilles annulables (test 188) ; plans de travail volontairement hors historique.
- [x] BUG-004 / BUG-009 — isolation branchée, module mort supprimé ; [ ] vérifier en GUI (I, H, Alt+H, Ctrl+H, Alt+I, Alt+W).
- [x] BUG-005 — édition groupée (test 191) ; [ ] vérifier le panneau en GUI (sélection de 3 poutres, changer la section).
- [ ] Vérifications manuelles GUI non automatisées : enchaînement Z→X→Y, entrée/sortie mode 2D, sélection d'un câble fin, Undo après édition de propriété, message « résultats obsolètes ».
- [ ] Édition géométrique manquante : division à un nœud existant / aux intersections, symétrie par plan quelconque, ouvertures dalles/voiles, grips viewport.
- [ ] Vérification manuelle GUI : Symétrie, Diviser les barres, Fusionner les nœuds (menu Édition / ruban), Undo de chacune.

- [x] BUG-016 — stations exactes + déformée intégrée (tests 181–184).
- [ ] Vérification manuelle GUI : dock « Données d'analyse », configuration LIGHT/ADVANCED, export, charge sur poteau via MemberLoadDialog.

## Outils de modification / dessin (ADR-018)

- [x] BUG-026 — anciens modes Move3D / Copy3D / Rotate3D supprimés.
- [ ] Vérification manuelle GUI des 19 outils autres que Rectangle (Prolonger / Ajuster / Décaler en particulier).
- [ ] Étendre Symétrie aux treillis (mirrorElements ne les gère pas) ; saisie d'un axe de rotation autre que la
      normale du plan de travail en mode 3D.
- [ ] Accrochage « milieu de barre » et « perpendiculaire » pour la saisie des outils.

## Nettoyage du modèle

- [ ] Vérification manuelle GUI : « Nettoyer puis calculer » sur un vrai projet (nœuds confondus importés).
- [ ] Étendre aux câbles (extrémités confondues : déjà couvert par la fusion) et aux voiles / dalles (nœuds sur arêtes).
- [ ] Option : nettoyage limité à la sélection ; surbrillance des nœuds signalés « à vérifier ».

## Moteur 2D MetDeDeplacement (ADR-019)

- [ ] Vérification manuelle GUI : calcul Custom2D depuis la fenêtre Analysis sur un axe de grille, NDC affichée.
- [x] BUG-027 — rotules transmises à OpenSees (test 183).
- [ ] Option : importer les anciens fichiers JSON « dessin » (legacy) dans le modèle TSA, si l'utilisateur le souhaite.
- [ ] Déformée viewport : exploiter les déplacements le long des barres (stations u, v) pour une déformée courbe.

## Analyse multi-moteurs (ADR-017)

- [x] `AnalysisContext` persisté (chunk SETT, format 1.4, test 189).
- [x] Modal / Pushover retirés (ADR-022).
- [ ] Vérification manuelle GUI : calcul depuis la fenêtre Analysis (OpenSees, portée axe), panneau « Résultats
      d'analyse » des Propriétés, onglets masqués du dock « Données d'analyse ».
- [x] BUG-024 — retiré avec le dynamique (ADR-022).

## IA Co-Engineering (ADR-015)

- [x] Fournisseur Cloud « Anthropic Claude » (préréglage `anthropic`, point d'accès compatible OpenAI) — 2026-10-09.
- [ ] Vérifier le fournisseur Anthropic Claude en conditions réelles avec une clé API (console.anthropic.com) :
      discussion complète, appels d'outils en streaming ; la couche de compatibilité OpenAI est en bêta chez Anthropic.
- [x] Claude Code co-ingénieur par MCP (AutomationServer + tsaraloha-mcp.exe, docs/MCP.md) — 2026-10-09.
- [ ] MCP : enregistrer un projet / ouvrir un fichier (aujourd'hui : l'ingénieur enregistre), captures du viewport.
- [ ] What-If : `SimulationScenario` (copie du modèle via snapshot, calcul, comparaison, jamais d'écrasement).
- [ ] Rapport d'ingénierie IA (15 sections, citations des données TSA, réutiliser le moteur NDC).
- [ ] « Expliquer avec l'IA » depuis les docks Propriétés et Résultats (actuellement : arbre + ruban).
- [ ] Distribution : embarquer une build llama.cpp testée dans `<app>/ai/llama` (détectée en priorité) ;
      aujourd'hui : llama.cpp installé via winget.
- [ ] Évaluer la qualité des réponses avec Qwen3 4B/8B sur une machine non saturée (BUG-021).
- [x] BUG-020 — qtbase_fr déployée et chargée ; [ ] vérifier les boutons en GUI.

## Projets récents / aperçus

- [ ] Restaurer aussi l'état d'affichage (viewState) via les actions de l'interface (grille, calques, 2D).
- [ ] Vérifier visuellement la barre d'état après correction du chevauchement (1920 px et 1536 px).
- [ ] Option : masquer cube de navigation / trièdre pendant la capture d'aperçu.

## Miniatures Explorateur (ADR-016)

- [ ] Installeur (Inno Setup / WiX) : copier TSAThumbnailProvider.dll à côté de TSA.exe, `regsvr32 /i:machine /n`
      à l'installation, `/u /i:machine /n` à la désinstallation.
- [ ] Vérifier après redémarrage de Windows et dans une fenêtre Explorateur (Très grandes icônes).
- [ ] Signer numériquement TSA.exe et la DLL (confiance SmartScreen / antivirus).

## LOW

- [x] BUG-017 — équilibre en moments.
- [x] BUG-018 — liens NDC avec famille ; [ ] vérifier le clic en GUI.
- [x] BUG-019 — nœuds de treillis purs (test 184).
- [ ] Persistance optionnelle des résultats avancés (lié à BUG-013).

- [ ] BUG-006 — Ajouter des `EditRecord` aux appels directs de `pushUndoState` (dialogues, viewport).
- [x] BUG-007 — index id → item (test 166).
- [ ] BUG-008 — Mesurer l'ouverture en Release ; partager la géométrie des sphères de nœuds si nécessaire.
- [x] BUG-010 — `NCollection_HArray1<gp_Pnt>`.
- [ ] BUG-013 — résultats (RSLT) non persistés ; paramètres faits (SETT).
- [ ] BUG-035 — test 20 sensible au temps (médiane de 3 essais ou seuil Debug).
- [ ] Fichiers > 2 000 lignes (OccView_Navigation.cpp, OccView_Shapes.cpp, MainWindow_Actions.cpp) : analyse de découpage avant d'y ajouter des responsabilités.

## COMPLETED

- [x] Outils de modification / dessin en saisie 3D (fenêtre optionnelle), 20 outils ; correction de la copie (section affichée) (2026-10-05, tests 139–149).

- [x] Architecture d'analyse multi-moteurs : contexte, portée par grille/niveau/WorkPlane, AnalysisModel dérivé, mapping, registre, OpenSeesEngine, Custom2DEngine (adaptateur), fenêtre Analysis commune, résultats pilotés par capacités (2026-10-05, tests 130–138).

- [x] Extraction avancée OpenSees : mapping TSA↔OpenSees, U/R/forces local-global-basic, matrices élémentaires et K_global, export, contexte IA, dock ; corrections FIX-015 à FIX-022 (2026-10-04, tests 105–111, non commité).

- [x] Symétrie, division de barres, fusion de nœuds confondus + avertissement dalles/voiles non calculés (2026-10-04, tests 101–104, non commité).

- [x] Audit stabilité/performance + corrections (merge 5ed3cf9) — FIX-001 à FIX-008, FIX-014.
- [x] Système d'édition : invalidation des résultats, transactions, historique structuré, sélection ensembliste, règle des niveaux, budget mémoire (merge 81e34cf) — FIX-009 à FIX-013.
- [x] Initialisation de la mémoire technique `.claude/` (2026-10-03).
