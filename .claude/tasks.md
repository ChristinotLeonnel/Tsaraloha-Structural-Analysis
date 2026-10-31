# Tasks

Last Updated: 2026-10-28. Uniquement des tâches réellement identifiées (voir known-issues.md).

## Courbes 2D superposées (2026-10-30, branche feature/diagram-multi-curves)
- [x] Diagram2DWidget multi-courbes, échelles, export, sélection 3D ; test 531 ; traductions ; docs/UI.md.
- [ ] Essai dans TSA.exe réel (sélection d'une barre → dock, puces, export) ; commit / fusion (accord requis).
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
- [ ] Dépendances : Linux NON TESTÉ ; scripts PowerShell (setup_build/detect_*) à simplifier — rapport §11.8.
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
