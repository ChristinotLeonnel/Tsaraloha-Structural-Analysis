# Changelog

Les sessions futures ajoutent une entrée datée en tête (plus récent d'abord).

## 2026-10-29 — étape 2 : API d'édition unique, défauts Undo/Redo, dépendances du modèle (branche refactor/core-edit-api)
Contrat des transactions (BUG-127 : commit / rollback sans effet, imbrication, comparaison exacte par les octets du contenu
persistant, encodeur fourni par IO au noyau), clé de coalescence et hasChanges ; tous les appels pushUndoState de
l'interface migrés vers EditTransaction (panneaux, chargements, appuis, grilles, création, bibliothèque, IA) ; BUG-125,
126, 128, 129 corrigés, BUG-130 partiel ; Model::removeLoadCase / resetLoadCasesToDefaults. B repris (cherry-pick
8695035) et validé sous MSVC : relinks seuls (commandes de compilation inchangées), build complet sans IfcOpenShell
550/550 sans C4605/C4651. Windows : 29 suites sans OpenSees 29/29, banc Custom2D 58/58, CoreTests 5/5. OpenSees, Linux :
non exécutés. docs/EDIT_SYSTEM.md (contrat, périmètre de l'annulation).

## 2026-10-28 (soir) — dépendances portables (branche build/dependency-detection, non commitée)
SetupDependencies.cmake étendu (pas de module parallèle) : tsa_find_qt (Qt la plus récente compatible, cache périmé
remplacé, Qt6*_DIR étrangers effacés), tsa_validate_dependencies (x64, chaîne MSVC/MinGW/ELF, kit msvcAAAA, mélange,
OCCT ≥ 8.0, statut VALIDÉE / NON VALIDÉE), rapport tsa-dependencies.txt + cible tsa-dependency-report. Presets sans
Qt6_DIR (W-9). windeployqt de la Qt retenue, DLL OCCT depuis OpenCASCADE_BINARY_DIR. Traductions désactivées si Qt < 6.9.
OpenSees : PATH, TSA_OPENSEES_ARCHIVE, curl --fail + SHA-256 dans OpenSeesManager (W-3 : aucune source officielle
joignable). W-8 déplacé dans DeployDependencies. W-10 Logger non ASCII (test 37.8). docs/DEPENDENCIES.md, rapport §11.
Windows : matrice m1-m7, suites 42/0/15, paquet vérifié et lancé hors du dépôt. Linux non testé.
Suite : OpenSees 3.8.0 depuis conda-forge (12 paquets épinglés + SHA-256, installation CMake identique à l'assemblage
manuel, empreinte falsifiée rejetée) ; exécution NON validée (lancement refusé par le contrôle de sécurité). TSALab :
tsa_find_qt, presets sans Qt figé, W-6. Plancher Qt 6.2 conservé (décision utilisateur).

## 2026-10-28 — validation Windows MSVC 19.51 (branche test/windows-validation, depuis 6a5bde1, non commitée)
Rapport : docs/reports/WINDOWS_VALIDATION_REPORT.md. Release Ninja, Qt 6.12.0 (6.11.2 absent du poste), build-windows-validation.
Compilation TSA et TSALab : code 0. Suites : 42 réussies, 14 non exécutées (OpenSees absent), Custom2D 58/58, test 128 réussi,
views 19/19, undo 11/11, TSA.exe démarre et se ferme proprement. Corrigé : W-1 (tests 127, 293, 341 : chemins UTF-8 étroits →
TSA::Core::utf8Path), W-2 (PythonAIBridge::findPython : « py » d'abord sous Windows, alias Store évité, test 489), W-5
(run_test_suites.py : mesh dépend d'OpenSees). Ajouté : tsa_download_opensees (SetupDependencies.cmake, SHA-256 vérifié,
non bloquant). Ouverts : W-3 (URL officielle d'OpenSees en 404), W-6 (TSALab LabTests.cpp:120), W-7, W-8, W-9.
Suite (même jour) : W-8 corrigé (RuntimeDependencies.cmake filtre les introuvables par EXCLUDE_REGEXES, vérifié) ; W-7
corrigé (test_torsion.cpp, utf8Path), compilé, non exécuté faute d'OpenSees.

## 2026-10-26 (soir, préparation de l'étape 2) — contrôles, dépendances, filet de tests (non commités)

- A (branche test/core-static-init-sections) : `tools/check_core_static_init.py` par sections ELF (init + terminaison),
  LTO refusé, codes 0/1/2, CTest `<P>_CoreStaticInitSelfTest` ; règle précisée dans architecture.md.
- B (build/model-deps-explicites + TSALab build/tests-qtwidgets-explicite) : QtWidgets et OCCT déclarés par leurs
  utilisateurs ; `<P>_Model` lié à Core, science, Qt Core / Gui / Network, TKernel, TKMath seulement.
- C (test/etape2-caracterisation) : suite « undo » (tests 513-523) — observateur des 39 notifications, empreinte du modèle,
  transitions opération / Annuler / Rétablir (révision, ModelEdited, EditRecord), F1-F6, treillis et câbles, grilles,
  notifications positives ; BUG-006 recompté (28 appels), BUG-125 à BUG-130 caractérisés (non corrigés).
- Linux Release : A 3/3 ; B suite identique à la base, banc 146/146, TSALab 27/27 ; C 521/522 (128 Windows seul),
  mutations 8/8 détectées. Windows / MSVC : non exécuté (procédure VALIDATION_WINDOWS_39155c4.md §7-11).

## 2026-10-26 (TSA Core, étape 1) — extraction de TSA_Core (non commitée)

- Cible `<P>_Core` STATIC (50 sources : Model, Load, Cable, Coordinate sans AxisColorConfig, Annotation/Dimension,
  BIM/Core, données de Grid, UndoManager, Logger, AppPaths, InstalledCatalogs) ; QtCore + TKernel + TKMath.
- `Model/InstalledCatalogs` : Section::defaultLibrary et CableDefinition::defaultDefinitions ne dépendent plus
  d'ExtensionSystem (fournisseur enregistré par LibraryRegistry.cpp) ; comportement inchangé.
- `<P>_CoreTests` (CTest `<P>_CoreLink`). Linux Release : suite complète identique à la base (509/510, 128 Windows seul),
  56/56 suites, banc 146/146, TSALab 27/27, contrôles OK. Windows/MSVC : non compilé.
- Revue : types `SemanticVersion`, `DefinitionReference`, `MechanicalSnapshot` déplacés dans le noyau
  (`Model/DefinitionReference.h`, alias dans ExtensionSystem, `IO/TSAFile.h` inclut ExtensionTypes.h) ; `NamedViews`
  dans le noyau (52 sources) ; test 512 ; suite 510/511, identique à la base hors test 512 et durées mesurées.

## 2026-10-26 (BUG-122) — correctif commité sur fix/core-integrity (CI à examiner)

- Repli Aspect_TODT_DEKALE des textes sur fond (marqueur d'accrochage, cotations) quand le contexte OCCT n'a pas
  core20fwd (OpenGl_Text::drawRect, OCCT 8.0.1) ; détection refaite à la recréation de la surface (BUG-119) ; marqueur
  affiché recalculé au changement de mode. Tests 510, 511 ; total attendu 510.
- Linux Release : 509/510 (128 ignoré, Windows seul), exécuteur CI 56 suites réussies, banc 146/146, TSALab 27/27,
  contrôles OK ; views 19/19 en OpenGL 4.5 (llvmpipe), 1.1 (Mesa) et 4.6 (GPU Intel). Windows : non validé.

## 2026-10-26 (BUG-119, BUG-120) — commit 6fec972 poussé sur fix/core-integrity

- BUG-119 : vue OCCT libérée avant la destruction de sa fenêtre native, recréée ensuite (OccView::eventFilter) ; test 509.
- BUG-120 : ressources OCCT depuis OpenCASCADE_RESOURCE_DIR de CMake ; test 286 avec [SKIP] motivé pour OpenSees.
- BUG-121 (1) : forme hors liste affichée par sa désignation (panneaux poutre et poteau) ; test 493 complété.
- CI 40280259182 : Linux RÉUSSIE (39 suites, 348 tests, views 18/18, deployment 3/3 avec SKIP motivé, Custom2D 58/58) ;
  Windows : 38 suites réussies, 331 tests, test 128 réussi, views PLANTÉE (0xC0000005 après le test 435) → BUG-122.
- Essais d'interface (Xvfb, xdotool) : 11/16 exécutés, résultats dans TSA-rapports/2026-10-26/ESSAIS_MANUELS_RESULTATS.md.

## 2026-10-26 (fin) — BUG-110 format 2.0, traductions, CI (branche fix/core-integrity)

- CI 40267211722 (ea646c3) : Linux 37 suites réussies, 0 assertion en échec, views PLANTÉE (BUG-119), 18 non exécutées,
  Custom2D 58/58 ; Windows : Qt 6.10.3 installé, src/ entièrement compilé par MSVC 19.44, échec sur tests/test_templates.cpp
  (chaîne brute avec \" en argument de macro, préexistant) → chaîne sortie de la macro.
- Commit eb7e6db poussé (sans fusion) ; CI 40263952835 : contrôles OK ; Linux : compilation OK, 37 suites réussies, Custom2D
  58/58, 3 suites en échec (display : OpenSees absent, oubli du tableau des dépendances ; deployment : BUG-120 ;
  views : plantage BUG-119, préexistant) ; Windows : Qt 6.11.2 non installable par aqtinstall 3.3.0 → Qt 6.10.3.
- Traductions anglaises des nouveaux textes ; malgache en attente (repli sur le français vérifié dans les .qm).
- BUG-110 : propriétés des sections enregistrées, format 2.0, migration au bit près ; IFC FilletRadius, TSA3D
  dimensions.r ; panneau de la barre (rayon éditable, conversion proposée). Tests 506, 507, 508.

## 2026-10-26 (suite) — D1 à D6 : unités des échanges, informations de calcul, installation .tsalib (non commité)

- BUG-117 (P1) : raideurs IFC en N/m avec unités dérivées déclarées, import selon l'unité du fichier (ancien export TSA
  relu en kN/m) ; comparaison Fem3D / OpenSees ramenée en kN ; réactions TSA3D en kN + `results.units`. Tests 499, 500,
  501 (reproduits avant correction). Docs : IFC_MAPPING.md, TSA3D.md, schéma tsa3d-1.
- BUG-111 étape 0 : plus de valeurs inventées dans les métadonnées, NDC sur les métadonnées des résultats, UTC, messages
  de validation conservés. Tests 503, 504 ; test Standards.8 adapté.
- BUG-118 étape E0 : installation transactionnelle (préparation, copie, renommage unique), anti-masquage des extensions
  livrées (installation et découverte), contrôles de chemins renforcés, minimum_tsa_version. Test 505.
- BUG-113 : service Interaction::setSlabOpenings / setWallOpenings (test 502) ; test 444 : aucune entrée Annuler au
  relâchement du manipulateur (reproduit avant correction).
- Validation Linux Release : 503/504 (test 128 Windows seul, ignoré), banc 146/146, check_layers 0, raccourcis OK,
  dépendances 0 erreur, CTest Layers/I18nCheck OK, TSALab 27/27. Non exécutés : Windows/MSVC, CI GitHub, essai GUI.

## 2026-10-26 — TSA Core, phase 0 (suite) : BUG-112 à 114, 116, CI (branche fix/core-integrity, non commitée)

### Fixed
- BUG-112 ModelDiff dalles/voiles (test 497) ; BUG-113 notifications (test 498) ; BUG-114 OpenSees (paquet, OFF par défaut,
  aucun téléchargement implicite, extraction sans interpolation) ; BUG-116 attente du test 166 ; BUG-118 (partiel) :
  manifeste extrait lié à l'en-tête validé, flux contrôlé (test 496 étendu).

### Added
- .github/workflows/ci.yml, tools/ci/run_test_suites.py, tools/ci/check_dependencies.py.

### Verified
- Release Linux : 496/497 (test 128 Windows) ; banc OpenSees 146/146 ; banc Custom2D sans OpenSees 58/58 ;
  run_test_suites.py : sans dépendance 38 suites réussies, 0 échec, 18 non exécutées ; avec toutes les dépendances
  (xcb) 56/56 après correction du test 166.
- Windows : NON VÉRIFIÉ. Pipeline CI : NON EXÉCUTÉ (push non autorisé).

## 2026-10-25 (soir) — TSA Core, phase 0 : intégrité (branche fix/core-integrity, non commitée)

### Fixed
- BUG-105 : vérifications EC2/EC3 et note de calcul indépendantes des unités du calcul (UnitSystem::forceToNewtons /
  forceToKiloNewtons ; ResultsDockWidget, NDCGenerator, NDCProjectNote, ResultAnalyzer). Test 492.
- BUG-106 : panneaux Propriétés (barre, poteau, treillis, dalle, voile) — seuls les champs modifiés sont réécrits
  (src/UI/Properties/EditedFields.h). Test 493.
- BUG-107 : affectation de section du ruban annulable (TSA::Interaction::assignSection, src/Interaction/PropertyAssignment.*).
  Test 494.
- BUG-108 : noms neutralisés dans le script OpenSees (tclComment). Test 495.
- BUG-109 (partiel) : paquets .tsalib — identifiant validé, plafonds, confinement. Test 496.

### Verified
- Release Linux (xcb, OpenSees) : 494/495, 0 échec (test 128 propre à Windows) ; banc de validation 146/146 inchangé.
- Debug : compilation et suites ndc, appearance, drawing, extensions, opensees réussies. check_layers : 0 violation.
- Chaque défaut reproduit par son test sur le code d'origine avant correction.
- Windows (MSVC) : NON VÉRIFIÉ.

### Audit
- Audit d'architecture et conception de TSA Core (6 analyses) : rapport hors dépôt (décision D16 en attente) ; écarts
  mémoire ↔ code relevés (calcul modal présent malgré ADR-022 ; architecture.md : format 1.1 annoncé, réel 1.12).

## 2026-10-25 (portage Linux : OpenSees, Gmsh, IfcOpenShell — branche fix/debug-build-errors)

### Added
- Gmsh 4.15.2 (SDK Linux64) : `cmake/Gmsh.cmake` hors Windows, `cmake/gmsh-hide-std.map` ; docs/LINUX_BUILD.md §10.
- IfcOpenShell 0.9.0post1 : `scripts/linux/build-ifcopenshell.sh`, `cmake/IfcOpenShell.cmake` hors Windows ; §11.
- scripts/linux : détection de TSA_GMSH_SDK / TSA_IFCOPENSHELL_ROOT (common.sh, check-dependencies.sh, configure.sh).
- Test 491 (vérification de l'exécutable OpenSees) ; total attendu 490 (tests/test_main.cpp).

### Fixed
- BUG-099 (vérification OpenSees : `OpenSeesManager::checkExecutable`), BUG-100 (gmsh.h_cwrap → récursion infinie),
  BUG-101 (instanciations std interposées vers libgmsh), BUG-103 (IfcOpenShell lié avec -Bsymbolic). BUG-095 FIXED.

### Verified
- Release (xcb, OpenSees dans le PATH) : 489/490, 0 échec, 1 SKIP (test 128, Windows seulement).
- Debug : 482/490 — test 457 (seuil de performance, BUG-104) arrête sa suite (458–463 non exécutés).
- Windows (MSVC) : NON VÉRIFIÉ.

## 2026-10-24 (portage Linux Ubuntu 24.04 — branche fix/debug-build-errors)

### Added
- Presets `linux-debug` / `linux-release` (Ninja, condition hôte Linux, Qt et OCCT par CMAKE_PREFIX_PATH).
- `scripts/linux/` : common.sh, check-dependencies.sh, configure.sh, build.sh, test.sh ; `docs/LINUX_BUILD.md`.
- `src/Core/NumericLocale.cpp` (LC_NUMERIC="C" à la création de toute QCoreApplication) ; Test 490 (suite i18n).
- `.gitattributes` : `*.sh text eol=lf`.

### Added (nuit)
- OpenSees 3.8.0 compilé pour Linux (Conan 2) et validé : cas analytiques exacts, TSA_TestSuite 450/471 ; procédure
  dans docs/LINUX_BUILD.md §7. BUG-099 consigné (vérification de l'exécutable OpenSees).

### Fixed (soir)
- BUG-094 : `ConverterSpec::executable()` (commande de module résolue selon la plateforme) ; tests 274 et 279.
- BUG-096 : textures des matériaux décodées par Qt hors Windows (MaterialVisual).
- BUG-097 : tests 124 (attente bornée) et 387 (casse des chemins, INI de QSettings hors Windows).
- BUG-098 : traductions à jour (128 textes traduits en anglais et en malgache, 15 textes disparus retirés) ; docs/SDK.md, docs/LINUX_BUILD.md
  (§7 OpenSees, §8 validation Windows, §9 commit depuis Linux).

### Fixed
- BUG-091 locale numérique sous Unix ; BUG-092 suites masquées par `#ifdef _WIN32` dans test_main ; BUG-093 OpenSees
  .exe et téléchargement Windows sous Linux ; OccView : `Xw_Window` reçoit un `Aspect_Drawable` ;
  test_thumbnail_provider : test 128 réservé à Windows ; test 343 [SKIP] sous offscreen (comme 408).
- `cmake/SetupDependencies.cmake` (SDK OCCT Windows téléchargé seulement sous Windows), `cmake/IfcOpenShell.cmake`
  (pas de construction MSVC par défaut hors Windows), `cmake/Gmsh.cmake` (message exact hors MSVC).
- `.gitattributes` : 147 octets NUL parasites en fin de fichier retirés (Git le traitait comme binaire).

## 2026-10-17 (Blueprint : sous-graphes réutilisables, fichier v3 — branche feature/blueprint-structurel)

### Added
- `src/Blueprint/BlueprintSubgraph.*` : sous-graphes définis dans le graphe (`Graph::subgraphs`), corps entre
  `sub.<id>.in` et `sub.<id>.out`, instances `sub.<id>` pures (réutilisables) ou d'action, exécution imbriquée
  (journal préfixé, erreur du corps propagée, 32 niveaux), bibliothèque augmentée par graphe
  (`NodeLibrary::forGraph` / `withSubgraphs` / `setSubgraphs`), contrôles (structure, fin atteinte, récursion,
  définition absente, version plus récente), `encapsulate`, `defineSubgraph` (version + mise à jour des instances),
  `removeSubgraph`, `validateBody`, `pruneStaleLinks`.
- Fichier `.tsbp` version 3 (liste `subgraphs`) ; un graphe sans sous-graphe reste en version 2. Fichier `.tsbps`
  (sous-graphe et ses dépendances) ; copier-coller avec les définitions utilisées.
- Éditeur : menu Sous-graphes (encapsuler la sélection, ouvrir, propriétés, retour, import / export, suppression),
  double-clic sur une instance, fil d'Ariane, historique propre au corps, essai du corps seul, graphe composé à
  l'enregistrement, `BlueprintSubgraphDialog` (interface typée). Tests 392 (moteur) et 393 (éditeur) : 393/393.

### Fixed
- Réutilisation des résultats : une instance de sous-graphe pouvait rendre le résultat de l'ancienne définition
  (même type, mêmes entrées) après Annuler, un import ou une interface modifiée. `NodeDefinition::revision`
  (identité des corps) comparée par `EvaluationCache` ; trouvé par le test 393, garanti par le test 392.

## 2026-10-10 (Raccourcis clavier : gestionnaire central, shortcut.txt, rechargement à chaud, éditeur)

### Added
- `ShortcutManager` : raccourcis de 211 commandes (identifiants stables `cmd.*` du `CommandCatalog`, désormais
  utilisé à l'exécution), fichier utilisateur `shortcut.txt` (%LOCALAPPDATA%\TSA Engineering\TSA) créé s'il
  manque, rechargé à chaud (QFileSystemWatcher), configuration invalide jamais appliquée, conflits exacts et
  de préfixe détectés, alias, suites de touches façon AutoCAD (« D, A », « Z, W », « Q, N »…).
- Aide > Personnaliser les raccourcis (Ctrl+F1) : recherche, catégories, enregistreur, conflits immédiats,
  activation, restauration, ouverture du fichier ; F1 : liste générée des raccourcis actifs.
- Nouveaux raccourcis (cotation D, charges Q, appuis A, sélection Ctrl+Alt, représentations Alt+1..4,
  panneaux Ctrl+1..7, F9 / Maj+F9, Ctrl+F5, Ctrl+G, Ctrl+I / Ctrl+E IFC…) ; « Répéter la dernière commande »
  (Ctrl+Entrée) ; les champs de saisie gardent leurs touches. docs/SHORTCUTS.md, docs/shortcut.txt (généré).
  Tests 257-266 (total 287).

### Fixed
- Raccourcis dispersés (57 appels codés en dur) et documentation / aide divergentes (F10, Ctrl+I, « M »).
- F / Maj+F / R traités aussi par la vue 3D : l'ancienne touche aurait survécu à une personnalisation.
- `tools/check_shortcuts.py` : alias et suites de touches pris en compte, référence générée comparée.

## 2026-10-10 (Représentations du modèle : physique, filaire analytique, éléments finis, superposition)

### Added
- Ruban Affichage > Représentation et menu Affichage > Représentation du modèle : modèle physique, filaire
  analytique (axes nœud à nœud colorés par famille, dalles / voiles en arêtes ; sélection, accrochage, appuis,
  charges conservés), éléments finis (maillage réellement transmis au moteur au dernier calcul à jour),
  superposition (sections translucides + axes). Visuel seulement : résultats et révision inchangés.
- `SolverMesh` (structure neutre) joint aux résultats par OpenSees (`OpenSeesModelMap::solverMesh`) et Custom2D ;
  messages explicites sans calcul, avec résultats obsolètes ou moteur sans maillage ; aucune subdivision
  inventée (moteurs actuels : 1 élément fini par barre). docs/DISPLAY_MODES.md. Tests 250-256 (total 277).

## 2026-10-10 (Cotations 3D)

### Added
- Ruban Outils > Cotation et menu Modélisation > Cotation : cotations alignée, linéaire (axe automatique),
  horizontale, X, Y, Z, angulaire, de niveau, en chaîne, cumulée ; modifier, supprimer (Suppr), supprimer les
  invalides, afficher / masquer, style (unité, décimales, arrondi, texte, flèches, lignes d'attache, référence des
  niveaux, couleur, texte dans le plan). Rayon / diamètre non proposés (pas de géométrie circulaire).
- Saisie par les outils du registre (accrochage, aperçu, décalage au clavier) ; points accrochés aux nœuds
  associatifs (valeur recalculée au déplacement) ; nœud supprimé → référence invalide en rouge, réassociable ou
  supprimable ; Annuler / Rétablir.
- Rendu OCCT : calque supérieur, flèches et texte à taille constante, texte face à la caméra ou dans le plan.
- Annotations seulement : révision du modèle inchangée (résultats conservés), absentes du calcul.
- Chunk DIMS (format 1.6) ; anciens projets ouverts sans cotation. docs/DIMENSIONS.md. Tests 237-249 (total 270).

## 2026-10-10 (Topologie et numérotation des nœuds et des éléments)

### Added
- Paramètres du projet → Topologie et numérotation (Fichier, ruban Outils > Projet) : 13 stratégies de nœuds
  (séquentielle, coordonnées avec ordre et sens des axes, rangées, colonnes, couches, grille d'axes A1, niveaux,
  Cuthill-McKee inverse, BFS, DFS, personnalisée…), 5 d'éléments ; groupes et maillage affichés indisponibles
  (prérequis absents). Formats {p} {n} {g} {l}, départ, pas, chiffres, préfixes par famille, portée sélection,
  conservation des étiquettes personnalisées, détection des doublons, aperçu sans effet, correspondance
  identifiant interne ↔ étiquette ↔ tag OpenSees (export CSV). docs/TOPOLOGY.md.
- Renumérotation = étiquettes seulement : une entrée Annuler sans changement de révision (résultats conservés).
- Chunk TOPO (format 1.5) ; anciens projets ouverts avec les valeurs par défaut. Tests 225-236 (total 257).

### Fixed
- BUG-073 : l'étiquette 3D des nœuds affiche leur nom (et non l'identifiant interne).

## 2026-10-10 (Arbre : liste des appuis ; molette du panneau Propriétés)

### Fixed
- BUG-071 : la liste « Appuis » de l'arbre suit les appuis posés, retirés, les nœuds supprimés et Annuler.
- BUG-072 : la molette fait défiler le panneau Propriétés sans changer la liste ou le champ survolé. Tests 223-224
  (total 245).

## 2026-10-10 (« Étirer » et appui sur plusieurs nœuds)

### Fixed
- BUG-069 : « Étirer » enchaîne aussi les poutres et les poteaux (le nœud d'arrivée devient le départ suivant).
- BUG-070 : un appui (ou tout autre réglage) choisi dans les propriétés s'applique à tous les éléments sélectionnés
  du même type ; l'édition groupée n'est plus interrompue par la reconstruction de la forme modifiée. Test 222
  (total 243).

## 2026-10-10 (Apparence par matériau : textures, déformée, couleur utilisateur)

### Added
- Chaque matériau s'affiche avec sa propre texture (béton, acier, bois, brique…) en mode Matériaux ; la texture est
  partagée entre tous les éléments du même matériau (une ressource graphique par fichier).
- Propriétés : bouton « Matériau » qui efface la couleur propre à l'élément.

### Fixed
- BUG-067 : la déformée garde l'apparence des éléments (matériau ou couleur choisie) au lieu du cyan uniforme.
- BUG-068 : valider le panneau Propriétés n'impose plus de couleur par défaut ; seule une couleur choisie par
  l'utilisateur remplace celle du matériau, et teinte le grain de sa texture. Tests 220-221 (total 242).

## 2026-10-10 (Aperçu des projets à l'endroit)

### Fixed
- BUG-066 : l'aperçu enregistré dans le .tsa (miniature de l'Explorateur, Start Center) n'est plus à l'envers.

## 2026-10-10 (Suppression des charges dans la vue 3D)

### Fixed
- BUG-065 : Suppr supprime les charges sélectionnées dans la vue 3D (charges nodales et sur barres), avec une seule
  entrée Annuler ; nœuds et barres porteurs conservés. Test 219 (total 240).

## 2026-10-10 (Aide contextuelle vers la documentation en ligne)

### Added
- Registre central des identifiants d'aide (`src/Help/HelpTopics.cpp`, 33 entrées) et construction des adresses de la
  documentation en ligne : HTTPS et domaine officiel uniquement (`kDocsBaseUrl` de `product/ProductIdentity.h`), seuls
  la langue et la version de TSA transmises, identifiant inconnu → accueil `/docs`. TSALab : `kDocsBaseUrl` vide, les
  fenêtres gardent leur aide locale.
- `src/UI/HelpLauncher` : ouverture par `QDesktopServices::openUrl`, message non bloquant avec l'adresse si le navigateur
  ne s'ouvre pas ; boutons Aide (charges sur barre, charges nodales, cas de charge, analyse) ; menu Aide et ruban :
  « Documentation en ligne », « Signaler un problème ». Tests 214-218 (suite `help`, total 239).

### Fixed
- BUG-064 : bouton Aide de la fenêtre Grille relié (il était créé sans connexion).

## 2026-10-10 (Sélection multiple Ctrl + clic)

### Fixed
- BUG-063 : Ctrl + clic ajoute ou retire un élément sans perdre les autres ; affichage, propriétés et commandes partagent le
  même ensemble (gestionnaire unique) ; éléments supprimés retirés de la sélection. Test 213 (total 234).

## 2026-10-10 (Module de chargement : représentation, règles communes, interface)

### Fixed
- BUG-059 : charge uniforme partielle transmise sur toute la barre (OpenSees, Custom2D) ; test 212.
- BUG-060 : sens des charges sur barre unifié (`LoadResolver::memberLoadVector`), sans changement des valeurs de calcul.
- BUG-061 : représentation dense et fidèle des charges réparties (`src/Geometry/MemberLoadGlyph`) ; test 209.
- BUG-062 : validation, repère déduit de la direction, application tout ou rien et doublons ; test 211.

### Changed
- Onglet Charges par catégories : Nœud (force et couple), Barre (uniforme, trapézoïdale, ponctuelle), Surface (désactivée :
  non transmise au calcul), Poids (poids propre par cas de charge). Fenêtre de charge sur barre : intervalle pour toute
  charge répartie, aide sur le sens, application aux barres sélectionnées en une entrée Annuler.
- Densité des flèches : longueur ≈ 8 % de la hauteur visible par paliers × 1,5 (rendu recalculé seulement au changement de
  palier), espacement = longueur / 2, 3 à 60 flèches par charge, 2 sur un intervalle court.

## 2026-10-10 (Crash de la sélection 3D après fermeture d'une fenêtre)

### Fixed
- BUG-058 : crash en cliquant dans la vue après avoir fermé la fenêtre de charge nodale pendant une « Sélection 3D ».
  Le demandeur annule sa requête en se fermant ; les fenêtres Charge nodale et Nouveau nœud sont non modales, leur
  bouton de clic 3D fonctionne désormais. Test 208 (total 229).

## 2026-10-09 (Charges : valeurs à zéro, force et couple réunis)

### Changed
- Fenêtres de charge nodale et de charge sur barre : plus de valeur par défaut (Fz = −50 kN, q = −15 kN/m) ni de boutons de
  préréglage chiffrés ; tout part de zéro (une charge entièrement nulle reste refusée avec un message).
- Onglet Charges : « Force Ponctuelle » et « Moment Nodal » (qui ouvraient déjà la même fenêtre) remplacés par un seul
  bouton « Force et Couple » (icône `load_force_couple.svg`) ; fenêtre « Charge nodale : force et couple », groupe
  « Couple nodal (kNm) ». La commande texte MOMENT ouvre cette même fenêtre.

## 2026-10-09 (Panneau poutre : relâchements retirés)

### Removed
- Groupe « Relâchements aux Extrémités (Rotules) » retiré du panneau de propriétés des poutres (demande utilisateur).
  Les relâchements déjà présents sur une barre (fichier .tsa, import IFC) ne sont ni modifiés ni effacés par le panneau et
  restent transmis au calcul OpenSees (BUG-027) ; ils ne sont plus visibles ni modifiables dans ce panneau.

## 2026-10-09 (Barres : nœuds d'extrémité et appuis dans le panneau)

### Added
- BUG-057 : le panneau d'une poutre, d'un poteau ou d'une barre de treillis montre ses nœuds d'extrémité (coordonnées,
  appui), mis à jour dès qu'un nœud change ; l'appui s'y règle directement (Libre, Encastrement, Articulation, Appui
  simple), les autres appuis restent réglables dans le nœud.

## 2026-10-09 (Boutons de commande : couleur = commande active)

### Fixed
- BUG-056 : les boutons de l'onglet Modifier (et les boutons de mode) ne sont colorés que pendant leur commande ; annulée
  (Échap), terminée ou refusée, la couleur disparaît. Libellés des grands boutons conservés.

## 2026-10-09 (Fenêtres de tracé : reprise automatique de la commande)

### Changed
- BUG-055 : revenir dans une fenêtre de tracé ouverte (barres, câbles, surfaces) après Échap relance sa commande, sans la
  fermer ni la rouvrir. Échap annule toujours la commande en cours.

## 2026-10-09 (Ruban : disposition stable d'un passage à l'autre)

### Fixed
- BUG-054 : les onglets du ruban se réorganisaient à chaque visite (groupes repliés puis dépliés, icônes déplacées) sans
  changement de taille. Largeurs mesurées une fois par mode après polissage explicite (`RibbonPanel::measureWidths`),
  disposition fonction de la seule largeur disponible (`RibbonTab::relayout(int)`), largeur de la pile d'onglets prise
  à l'affichage. L'adaptation à la taille de la fenêtre (icônes seules, groupes repliés, menu « Plus ») est conservée.

## 2026-10-09 (Audit QA : analyse en tâche de fond, JSON des grilles, note de calcul)

### Fixed
- BUG-049 : annulation d'un calcul OpenSees perdue si elle arrivait avant la création du solveur (ou avant la remise à
  zéro de son drapeau d'arrêt dans `solveSnapshot`). Drapeau partagé `AnalysisRunCallbacks::cancelRequested`, armé par
  `AnalysisController::cancel` avant l'appel au moteur et relu par `OpenSeesEngine::run` après création du solveur.
  `OpenSeesSolver::m_isRunning` atomique (écrit par le thread de calcul). Test 205.
- BUG-050 : résultats calculés sur un état antérieur du modèle publiés « à jour » (panneau d'analyse non modal, projet
  ouvert pendant le calcul). Révision mémorisée au lancement, `ResultsValidityGuard::trackResults(résultats, révision)`.
  Ouvrir / créer / glisser un projet est refusé pendant un calcul (`MainWindow::confirmNoRunningAnalysis`). Test 206.
- BUG-051 : JSON des grilles (chunk GRID et historique Annuler) écrit à la main : nom tronqué à la première virgule,
  JSON invalide sur un guillemet, réels à 6 chiffres significatifs. QJsonDocument (format compact dans le .tsa) ; ancien
  analyseur conservé pour relire un JSON invalide des versions précédentes. Test 207.
- BUG-052 : note de calcul d'une structure bois sans EN 1995-1-1 (Eurocode 5). Test NDC.
- BUG-053 : graduation des règles du viewport par indice entier borné (boucle flottante sans fin pour de grandes cotes).
- BUG-035 : test 20 évalué sur le meilleur de 3 annulations (seuil inchangé).

### Changed
- Une annulation volontaire du calcul est journalisée (avertissement, barre d'état) au lieu d'une boîte d'erreur critique.
- Total attendu de la suite : 228 tests.

### Vérifications
- Build ninja-debug complet sans avertissement (/W4) ; 228/228 PASS ; TSA.exe démarre (Start Center) et se ferme (code 0).
- clang-tidy (bugprone, clang-analyzer, performance) sur 266 fichiers de src : à l'origine de BUG-052 et BUG-053 ; les
  autres signalements examinés sont des faux positifs ou des choix délibérés (gardes OCCT, copie nécessaire avant erase).

## 2026-10-09 (Grille : repères par vue ; déformée continue)

### Fixed
- BUG-044 : repères d'axes répétés à chaque niveau et aux deux extrémités (CartesianGrid générait niveaux × (2 nX + 2 nY)
  ancres, toutes dessinées ; bulles toujours couchées en XY, vues par la tranche en élévation). Nouveau
  `src/Grid/GridLabelLayout` (classification de la visée dans le repère de la grille, choix et placement) : plan = niveau
  actif, élévation ∥ Y = lettres seules au-dessus du dernier niveau, élévation ∥ X = chiffres seuls, 3D = niveau le plus
  bas ; une occurrence par axe (option `labelsBothEnds`, JSON + case « Repères aux deux extrémités »). Bulle face à la vue.
  `OccView::updateGridLabelView` sur viewCameraChanged / niveau actif, recalcul seulement si la catégorie de vue change.
  Cylindrique : un seul niveau d'étiquettes.
- BUG-045 : déformée en « empilement de blocs » (6 prismes droits indépendants par barre) et interpolation nodale seule
  (flèche en travée sous-estimée de 20 % pour q uniforme, nulle sur deux appuis). `DeformedGeometry::computeMemberAxis` :
  stations du moteur (OpenSees : double intégration de M/EI ; Custom2D : courbes MetDeDeplacement) dans le repère local du
  solveur, repli Hermite cubique Euler-Bernoulli (θz = v', θy = −w'), linéaire pour treillis / câbles ; invalide (jamais
  inventée) si déplacement absent ou non fini. `createMemberSolid` : section balayée en un seul maillage triangulé
  (transport minimal du repère, pas de torsion artificielle). Axes non amplifiés mis en cache : changer l'échelle ne
  recalcule que l'affichage.
- BUG-046 : « Déformée seule » / « Les deux » ne différaient que par la transparence de la déformée (structure d'origine
  toujours opaque). `OccView::setResultsStructureDisplay` : atténuée (Les deux, défaut) ou masquée dans la passe
  `updateElementIsolation`.
- BUG-047 : un nouveau calcul écrasait le facteur d'échelle choisi (×1, ×100, manuel) par l'échelle automatique.
- BUG-048 : échelle automatique fondée sur les seuls nœuds (poutre sur deux appuis → ×1, déformée invisible).
- Légende : « échelle réelle » / « amplifiée pour l'affichage, résultats inchangés », origine des courbes, barres sans
  déformée.

### Changed
- `BeamGeometry::sectionFrame` / `sectionOutline` : repère et contours de section partagés par la barre droite et la
  barre déformée (cas polygonaux de createBeamShape factorisés).
- Tests 202 (grids), 203 (extraction, OpenSees réel), 204 (mdd, Custom2D) : 225/225.

## 2026-10-09 (Audit et corrections : persistance .tsa, chemins non ASCII)

### Fixed
- BUG-040 : un projet enregistré sous un chemin contenant « é », « â »… ne se rouvrait pas (« Fichier introuvable ») :
  `std::ifstream` sur une chaîne UTF-8 = page de code ANSI sous MSVC. `TSA::Core::utf8Path` (src/Core/Utf8Path.h)
  pour les 4 ouvertures .tsa (TSAFile.cpp), le lecteur de résultats OpenSees et l'export Tcl.
- BUG-041 : `writeString` bornait à 65 536 puis castait en u16 → toute chaîne ≥ 64 Kio écrite vide ; les chunks
  COOR / GRID (JSON) perdaient niveaux / grilles. Borne 65 535 sur frontière UTF-8 ; `writeChunkText` /
  `readChunkText` : au-delà, préfixe 0 + JSON jusqu'à la fin du chunk (format inchangé pour < 64 Kio, rétrocompatible).
- BUG-042 : `ProjectManager::saveProject` écrivait un nom et un auteur vides (nom saisi dans « Nouveau projet » perdu
  à la réouverture).
- BUG-043 : ouverture refusée après lecture des chunks COOR / GRID → le projet resté ouvert avait reçu les niveaux et
  grilles du fichier rejeté. Application différée après la lecture complète.
- AutomationServer : identifiant non entier dans une liste (`[3.7]`) refusé au lieu d'être converti en 0 (latent :
  aucune commande intégrée n'a de paramètre IdList).
- Test 201 (reproduit les 4 bugs sur le code d'origine). 222 tests.

## 2026-10-09 (Claude Code co-ingénieur : serveur MCP)

### Added
- AutomationServer (src/Automation, base commune) : canal local `tsaraloha-<produit>` réservé au compte Windows,
  commandes du registre, scripts → Blueprint, outils de lecture de l'IA, contexte d'ingénierie, Annuler / Rétablir.
  Démarré par MainWindow (TSA) et LabMainWindow (TSALab) ; actions visibles en direct, console « MCP › … ».
- tools/mcp/TsaralohaMcp.cpp → `tsaraloha-mcp.exe` (serveur MCP stdio, 8 outils), cible CMake
  `tsaraloha_add_mcp_bridge`. Claude Code (compte de l'utilisateur, sans clé API) pilote TSA / TSALab. docs/MCP.md.
- Test 200 (dialogue MCP réel avec le pont) : 221/221. Vérifié en conditions réelles : TSALab ouvert, portique
  construit par run_command_script, calcul analysis.run, results.summary (Mmax 37,2 kN·m, ΣRz 77,9 kN).

## 2026-10-09 (Correction de la fermeture de TSALab)

### Fixed
- BUG-039 : assertion Qt à la fermeture de TSALab quand des nœuds Blueprint étaient sélectionnés (signal de la
  scène vers un éditeur déjà détruit). ~BlueprintEditor coupe les connexions de ses enfants ; ~LabMainWindow aussi.
- Test 199 (220/220).

## 2026-10-09 (Fournisseur Cloud Anthropic Claude pour l'IA Co-Engineering)

### Added
- Préréglage Cloud « Anthropic Claude (point d'accès compatible OpenAI) » dans `AIOrchestrator::reloadCloudProvider`
  (URL par défaut `https://api.anthropic.com/v1`, `contextTokens` 200000) et dans la boîte de dialogue IA
  (`AIRuntimeDialog`, combo Fournisseur). Réutilise `OpenAICompatibleProvider` (protocole Chat Completions,
  déjà partagé par OpenAI/Gemini/Ollama) : aucun code ni dépendance propre à Anthropic.
- Modèle utilisable par TSA **et** TSALab (AIOrchestrator est dans `src/AI`, partagé par les deux applications).
- Documenté dans `docs/AI_COENGINEERING.md`. Tests : 219/219 (TSA, dont `test_ai.cpp`) et 5/5 (TSALab) inchangés.
- Non vérifié en conditions réelles (aucune clé API Anthropic fournie) : à confirmer par l'utilisateur avec sa
  propre clé (console.anthropic.com) ; la couche de compatibilité OpenAI d'Anthropic est en bêta côté Anthropic.

## 2026-10-08 (Débogueur, plugins, IA sur Blueprint — ADR-024 phase 8)

### Added
- Blueprint : Debugger / BreakpointDebugger, Runner::requestStop, NodeDefinition::usesProject ; éditeur : points
  d'arrêt, Déboguer, Pas à pas, Continuer, Arrêter, valeurs produites, Annuler / Rétablir du graphe, exécution en tâche
  de fond des graphes sans accès au projet, menu Script.
- BlueprintScript : script de commandes ↔ Blueprint (variables, liens de données), description pour l'IA.
- CommandRegistry : parseCommandLine, tokenizeCommandLine, parseArgument, formatArgument ; registres globaux modifiables.
- IA : outils list_commands et propose_blueprint ; TSA exécute un Blueprint accepté (une entrée Annuler par commande).
- Plugins : src/Plugins (PluginApi.h, PluginManager), chargement au démarrage (EcosystemApplication), docs/PLUGINS.md.
- Tests 197–198 (219/219).

## 2026-10-08 (Analysis Manager — ADR-024 phase 7, branche feature/tsaraloha-shared-libs)

### Added
- AnalysisController (src/Analysis, dans ProjectSession) : registre des moteurs, réglages du projet, calcul en thread
  de travail (start / cancel / runBlocking), publication et invalidation des résultats.
- AnalysisManagerPanel (src/UI/Analysis) : moteurs et capacités, réglages, validation, calcul, synthèse.
- Portée d'analyse « plan du modèle » (ScopeType::ModelPlane, JSON `model_plane`).
- Custom2D : option « Exporter le système K·U = F » (K COO, F, U, DDL) ; dock Données d'analyse : onglet K·U = F.
- Commandes analysis.run, results.summary, results.node_displacement (console, Blueprint, IA).
- Test 196 (217/217).

### Changed
- MainWindow : plus de registre / manager / garde de résultats propres ; runAnalysis délègue au contrôleur.

### Fixed
- BUG-038 : équilibre global et nom du cas des résultats Custom2D.

## 2026-10-08 (Blueprint — ADR-024 phase 6, branche feature/tsaraloha-shared-libs)

### Added
- Blueprint (docs/BLUEPRINT.md) : moteur partagé src/Blueprint (graphe typé, flux de données / d'exécution, nœuds purs
  évalués à la demande, Si / Séquence / Pour / Tant que, paramètres nommés et reconstruction paramétrique, validation,
  garde-fous, profileur), une commande du registre = un nœud (cmd.*), nœud TSALab « Banc de validation » ; fichier
  .tsbp versionné ; éditeur partagé src/UI/Blueprint (palette, scène, liens à la souris, valeurs, exécution, profil).
- Tests 193–195 (suite blueprint) : 216/216.
- Console de TSA : commandes du registre central (identifiant à point) et « COMMANDES ».

### Fixed
- BUG-037 (MetDeDeplacement sans DDL libre) dans TSALab/science.

## 2026-10-08 (écosystème Tsaraloha — ADR-024, branche feature/tsaraloha-shared-libs)

### Changed
- Bibliothèques partagées <P>_Model / <P>_Graphics / <P>_Widgets (cmake/TSAProduct.cmake), contrôle des couches
  tools/check_layers.py (CTest). ProjectSession (modèle, commandes, grilles, projet, historique) hors de MainWindow.
- Composants partagés extraits de MainWindow / Application : SelectionSynchronizer, EcosystemApplication.
- Moteur custom2d : solveur du cœur scientifique TSALab (tsalab::planar, MetDeDeplacement déplacé dans TSALab/science).
- Retrait des points d'extension « TSALab enveloppe MainWindow » (configureShell, setLaunchPanel, décorateur).

### Added
- Automation/CommandRegistry : registre central de commandes typées (nœuds, poutres, poteaux, appuis, cas, charges,
  requêtes), une entrée Annuler par commande, ligne de commande ; test 192 (suite automation).
- docs/TSARALOHA_ARCHITECTURE.md (analyse, options A/B/C, architecture, feuille de route).

### Fixed
- Arbre du modèle : la section Charges suit les ajouts / suppressions de charges et de cas (observateur), dans TSA
  comme dans TSALab (vérifié dans l'interface de TSALab).

### Verified
- TSA 213/213 ; TSALab : application 2/2, cœur scientifique 5/5, tsalab-bench 6/6 ; 0 violation de couche ;
  TSA ouvre un projet en argument ; TSALab ouvre un .tsa, exécute des commandes console (captures d'écran).

## 2026-10-08 (base commune TSA / TSALab — ADR-023, branche feature/shared-core)

### Changed
- Identité produit sortie du code : `product/ProductIdentity.h` + `ProductShellIds.h` (constantes), `src/App/ProductInfo.h`
  (helpers Qt). ~40 fichiers de `src/` n'écrivent plus « TSA » / « .tsa » / « TSA Engineering » en dur (titres, QSettings,
  association Windows, signature de fichier, NDC, IFC, logs, rapports de crash, IA, DLL de miniatures, tests).
- Chemins absolus `E:/Book/Dev/TSA/...` (OpenSees, Extensions, textures, tests) remplacés par `TSA_SOURCE_DIR` (CMake).
- CMake : listes de sources et cibles déplacées dans `cmake/TSAProduct.cmake` (`tsa_add_product`) ; `CMakeLists.txt`
  réduit à la configuration + appel. SDK (OCCT, 3rdparty) localisés depuis la racine TSA (`TSA_DEPS_ROOT`).
- Textes neutres : « Échec de l'ouverture du projet », « Projet chargé avec succès », « signature non reconnue ».

### Added
- Points d'extension : `TSA::Product::configureShell` (product/ProductHooks.cpp), `StartCenter::setLaunchPanel`,
  `AppShell::setWorkspaceDecorator` / `workspaceCreated` / `startCenter()` / `workspace()`,
  `MainWindow::resultsModel()` / signal `resultsChanged`, `runProductSuites` / `productExpectedTests` (tests).
- Format : `NATIVE_FILE_MAGIC`, `isReadableFileMagic` (format importé optionnel, utilisé par TSALab).
- `MainWindow::saveFile` : un fichier d'un format importé n'est jamais réécrit (sans effet pour TSA).
- Start Center : renommer / dupliquer conserve l'extension du fichier.

### Verified
- TSA : 212/212 après reconfiguration `--fresh` (le cache local avait perdu `CMAKE_CXX_FLAGS` → pas de /EHsc,
  test 96 en échec ; voir known-issues BUG-036).

## 2026-10-07 (crash à la réduction de la fenêtre, branche fix/minimize-crash)

### Fixed
- Réduire la fenêtre agrandie faisait planter TSA (marges d'agrandissement calculées sur la position (-32000, -32000)
  de la fenêtre réduite) — `AppShell::updateMaximizedMargins` (FIX-2026-10-07-MINIMIZE).

## 2026-10-07 (statique seul — ADR-022 — et correction des bugs connus, branche feature/static-only)

### Removed
- Analyse modale, pushover et temporel : types d'analyse, résultats (modes, pas), générateur / lecteur OpenSees,
  chapitre NDC, actions et commandes (ruban, menus, console SEISMIC / MODAL), animation modale, mode du diagramme 2D.
  Archive : branche `archive/dynamique` + tag `v0.2.0-dynamique`.
- Modes Move3D / Copy3D / Rotate3D (BUG-026) ; module mort `src/View3D/Isolation`, `tests/isolation` (BUG-009).

### Fixed
- Résultats en convention RDM, stations exactes, déformée intégrée, charges trapézoïdales (FEP), rotules OpenSees,
  nœuds de treillis purs, équilibre en moments (BUG-016, 017, 019, 027) — tests 181–184.
- Calcul en thread de travail, progression annulable (BUG-001) ; liens NDC avec famille (BUG-018) ; traduction Qt
  française (BUG-020) ; C4996 (BUG-010).
- Isolation 3D branchée (BUG-004) ; arbre du modèle indexé (BUG-007, test 166) ; niveaux / axes / grilles annulables
  (BUG-003, test 188) ; paramètres d'analyse enregistrés, format 1.4 chunk SETT (BUG-013 partiel, test 189) ;
  métadonnées BIM au collage (BUG-029, test 190) ; édition groupée dans le panneau Propriétés (BUG-005, test 191).
- BUG-011 réapparu (reconfiguration en CP850 alors que le lanceur imposait UTF-8 → aucune dépendance d'en-tête,
  objets périmés, segfault du test 18) : lanceur généré à la page de code de la génération.

### Added
- `Model::setGridManager`, `ModelStateSnapshot::coordinatesJson / gridsJson`, `UndoManager::discardUnchangedTransaction`
  / `EditTransaction::discardUnchanged`, `Model::analysisSettingsJson`, `BimModel::registerPasted`,
  `TSA::Model::MultiEditSession`, `PropertyPanel::setMultiSelection`. `ModelTreeWidget` passe dans TSA_Core (testé).

## 2026-10-06 (correction des bugs relevés par les tests Start Center : BUG-030 à 034)

### Fixed
- BUG-030 : Encastrement / Articulation / Appui simple (menu Structure, commandes FIXED / PINNED / ROLLER) assignent
  réellement l'appui (`MainWindow::assignSupport`, une entrée Annuler).
- BUG-031 : suppression / Annuler / Rétablir en masse — viewport redessiné une fois par rafale
  (`OccView::scheduleRedraw`), arbre purgé en une passe (`ModelTreeWidget::queueRemoval/flushRemovals`).
  stress_4900 : suppression 6 min → 17 s (Debug).
- BUG-032 : barre d'état sans chevauchement (largeurs fixes, `fitStatusBar` adaptatif) ; la page cachée du shell
  n'impose plus sa taille minimale.
- BUG-034 : statistiques nœuds / appuis du dock Résultats rafraîchies à l'ouverture et à chaque modification.
- Fenêtre : redimensionnement par les bords en mode Workspace (filtre natif `ResizeBorderFilter`).

## 2026-10-06 (Start Center et barre de titre, logique AutoCAD — ADR-021)

### Added
- `src/UI/Shell` : `AppShell` (fenêtre unique, `ApplicationMode`, cadre natif Win32, géométrie « Shell/geometry »),
  `TitleBar` (bouton d'application TSA + menus du workspace, accès rapide Nouveau / Ouvrir / Enregistrer / Annuler /
  Rétablir, titre centré, Fermer le projet, thème, boutons de fenêtre).
- `src/UI/Home/StartCenter` (remplace StartPage) : logo, Nouveau projet, Ouvrir, projets récents (aperçu, nom,
  chemin, date d'ouverture), recherche, tri, sélection / double-clic, menu contextuel (dont « Retirer des projets
  récents » sans supprimer le fichier) ; `NewProjectDialog` (nom, emplacement, template).
- `MainWindow::createProject / closeProject / prepareToClose / resetWorkspace`, action « Fermer le projet ».

### Changed
- Lancement : Start Center seul ; le workspace (viewport, ruban, docks, barre d'état) est créé au premier projet.
- `MainWindow` embarquée (`Qt::Widget`) ; barre de menus non affichée (menus dans le bouton TSA) ; Nouveau / Ouvrir /
  Quitter / plein écran passent par AppShell ; accès rapide du ruban réduit (fichier / historique dans le titre).
- `LayoutManager` ne sauve / restaure la géométrie que si la fenêtre principale est top-level.
- Menus : entrées désactivées grisées (ThemeManager).

### Known
- BUG-030 à BUG-034 (stubs d'appuis, suppression massive lente, barre d'état trop large, DPI mixte non vérifié,
  statistiques nœuds du dock Résultats).

## 2026-10-06 (Résultats 3D : déformée masquée réellement)

### Fixed
- Déformée 3D restant à l'écran après l'avoir décochée : redessin du viewport après chaque changement d'état de
  `ResultsVisualManager` (déformée, mode, échelles, diagrammes, réactions, légende, animation modale) ; preset Auto
  régénère les formes ; légende rafraîchie ; case ruban synchronisée avec le dock (FIX-2026-10-06-DEFORMED).

## 2026-10-06 (IFC 4.3 : export, import, aller-retour)

### Added
- `src/BIM/IFC` : IfcStepWriter / IfcStepReader (ISO 10303-21), IfcMapper (entités, types prédéfinis, profils,
  matériaux), IfcGeometryMapper (SweptSolid + axe, repère du viewport), IfcPropertyMapper (Psets standard,
  Pset_TSA_Structural, Psets saisis), IfcRelationshipMapper, IfcExporter (IFC4X3_ADD2 : spatial, produits, modèle
  analytique, IfcRelAssignsToProduct), IfcImporter (unités, analytique ou géométrie, IfcIndexedPolyCurve).
- `BimModel::insert`, `setAnalyticalGlobalId`, `defaultProductName/Tag` ; GlobalId du modèle analytique persistant.
- UI : Fichier > Importer IFC… / Exporter IFC…, ruban Accueil > Projet (`MainWindow_Bim.cpp`).
- Tests 177–180 ; fixture `tests/data/ifcopenshell_fixture.ifc` (IfcOpenShell 0.9, mm) ; 197/197.
- docs/IFC_MAPPING.md ; statut des phases dans docs/BIM_ARCHITECTURE.md.

## 2026-10-06 (Couche BIM — phases 1–3, ADR-020)

### Added
- docs/BIM_ARCHITECTURE.md (état actuel, écarts BIM, plan en 13 phases), docs/ANALYTICAL_MODEL.md.
- `src/Core/Units.h` (unités centralisées), `src/BIM/Core` : BimTypes (catégories ↔ entités IFC 4.3), IfcGuid
  (GlobalId 22 car.), BimModel (produits physiques, mapping physique → analytique 1:N, structure spatiale, Psets,
  classifications, JSON versionné).
- `Model::bim()` / `bimForEdit()` / `setBim()` ; BIM dans `ModelStateSnapshot` ; hooks splitBeam / splitColumn /
  splitBarAt / copyTransformed.
- Chunk `.tsa` `BIMM` ; format 1.3 (docs/TSA_FILE_FORMAT.md §5.y).
- Tests 170–176 (suite `bim`) ; test 127 accepte un format ≥ 1.2 ; 193/193.

## 2026-10-05 (Nettoyage du modèle)

### Added
- `src/Model/ModelCleanup.*` : analyze (copie) / clean ; `connectCrossingBars` partagé avec l'outil Intersecter.
- `ModelCleanupDialog` (opérations, tolérance, bilan détaillé : corrections, refus, points à vérifier).
- Action « Nettoyer le modèle... » (ruban Modifier, menu Édition) ; proposition « Nettoyer puis calculer » avant calcul.
- Tests 160–165 (suite `cleanup`), dont un portique instable (traverse non reliée) calculable après nettoyage ; 186/186.

## 2026-10-05 (Moteur 2D MetDeDeplacement — ADR-019)

### Added
- `thirdparty/MetDeDeplacement` réécrit en bibliothèque autonome (mdd 2.0.0) : méthode des déplacements plane,
  rotules par condensation, treillis, ressorts, charges trapézoïdales / ponctuelles / nodales, Cholesky bande +
  Cuthill–McKee inverse, courbes N/V/M exactes, déformée EI v''=M, valeurs caractéristiques. Ancien code → legacy/
  (NOTES.md : analyse, 8 défauts constatés). Suivi par git (exception .gitignore).
- `MetDeDeplacementSolver` (Custom2D::ISolver) branché dans registerBuiltInEngines ; `Custom2D::Features`,
  `Custom2DOptionsWidget` (inextensible, points par courbe) ; rotules transmises par le snapshot.
- `ResultsModel::planarCurves`, métadonnées `calculationMethod` ; chapitre NDC « Courbes RDM par barre »
  (NDCPlanarCurves : méthode, formules, tableau, figures N/V/M/déformée) ; introduction NDC fidèle au moteur.
- Tests 150–159 (suite `mdd`) ; total 180/180.

### Changed
- Tests 130, 135, 137 : Custom2D désormais disponible (version 2.0.0, treillis, panneau d'options).

## 2026-10-05 (Outils de modification / dessin en 3D — ADR-018)

### Fixed
- Fermeture : 2 assertions Qt « class destructor may have already run » (AIOrchestrator → MainWindow
  pendant la destruction) ; ~MainWindow coupe les connexions enfants → fenêtre.
- Copie 3D d'une section circulaire affichée rectangulaire (attributs posés sans notification) ; copie du rôle,
  des treillis et des appuis ; rotation-copie 3D sans effet ; nœuds des câbles/treillis ignorés par le
  déplacement 3D ; collage de câbles/dalles sans notification. Test 139.

### Added
- `src/Interaction/Tools` : `ModelingTool`, registre, 14 outils de modification, 6 outils de dessin.
- `OccView_Tools.cpp` (mode `ModelingTool` : clics, barre sous le curseur, aperçu, saisie clavier),
  `ModelingToolDialog` (fenêtre générique), `MainWindow_ModelingTools.cpp`, bouton « Saisie dans la vue 3D »,
  panneaux « Outils de modification » (Modifier) et « Dessin rapide » (Modèle), sous-menus Édition, icônes.
- `Model::splitBarAt`, `Model::transformNodes`, `ModelElementCopy.h`.
- Tests 140–149 (suite `tools`) ; total 170/170. docs/MODELING_TOOLS.md.

### Changed
- M, Copie 3D, Ctrl+R, Symétrie, Diviser, Fusionner : passent par les outils (3D par défaut) ; Ctrl+Maj+M / Ctrl+D :
  fenêtre de paramètres.

### Removed
- `TransformDialog` (remplacé par la fenêtre générique).

## 2026-10-05 (Analyse multi-moteurs — ADR-017)

### Added
- `src/Analysis/Engine` : `AnalysisContext` (+ JSON versionné), `AnalysisScope`, `AnalysisScopeResolver` (axes de
  grille cartésienne, niveaux, plans de travail, sélection, restriction par niveau), `AnalysisModel`,
  `AnalysisMapping`, `AnalysisModelExtractor`, `AnalysisEngine` / `EngineInfo` / `AnalysisCapabilities`,
  `AnalysisEngineRegistry`, `AnalysisManager` (validation générique pilotée par capacités), `ValidationResult`.
- `src/Analysis/Engines` : `OpenSeesEngine` (chemin OpenSees existant), `Custom2DEngine` + `Custom2D::ISolver`
  (contrat du solveur 2D) + `Custom2DAdapter` (conversion plane et remappage des résultats), `registerBuiltInEngines`.
- `src/UI/Analysis` : `AnalysisDialog` (fenêtre commune), `AnalysisEngineOptionsWidget` / registre de fabriques,
  `OpenSeesOptionsWidget`. `ElementResultsPanel` (Propriétés : efforts de la barre sélectionnée, moteur, portée).
- `CalculationSnapshot::capture(model, ElementSet*)`, `SnapshotNode::definedFix`, `OpenSeesSolver::solveSnapshot`.
- `ResultsModel` : `ResultAvailability`, `EngineResultTable`, métadonnées engineId / analysisScope / analysisDimension.
- Tests 130–138 (suite `engines`) ; total 159/159. docs/ANALYSIS_ENGINES.md.

### Changed
- « Paramètres de résolution » → « Analyse (moteur, portée)... » ouvre `AnalysisDialog` ; F5 calcule le dernier
  contexte via `MainWindow::runAnalysis` (disponibilité / installation, validation, avertissements, publication).
- Dock « Données d'analyse » : titre neutre, onglets selon les catégories fournies, tables propres au moteur.

### Removed
- `src/UI/Dialogs/AnalysisConfigDialog.*` (aucun autre appelant ; contenu repris par `OpenSeesOptionsWidget`).

### Observed
- BUG-024 : pushover / temporel générés comme statique linéaire par le générateur OpenSees (non déclarés).

## 2026-10-04 (Miniatures Explorateur Windows)

### Added
- `TSAThumbnailProvider.dll` (src/ShellExtension) : IThumbnailProvider + IInitializeWithStream, CRT statique,
  dépendances système uniquement ; DllRegisterServer (HKCU) / DllInstall("machine") (HKLM).
- Format .tsa 1.2 : bloc d'aperçu PNG non compressé après le payload (src/IO/TSAPreviewBlock.h) ;
  `TSAFileReader::extractPreviewBlock` ; lecteur limité à `header.fileSize`.
- TSA : enregistrement de l'extension à chaque lancement, `SHChangeNotify(SHCNE_UPDATEITEM)` après
  enregistrement ; capture 640×480 sans cube de navigation ni trièdre.
- Tests 127–129 (suite `thumbnail`) ; total 150/150. docs/THUMBNAIL_PROVIDER.md.

### Fixed
- Test 91 : parcours des chunks jusqu'à `header.fileSize` (et non la fin du fichier).
- Fichier protégé par mot de passe : aucun aperçu écrit en clair.
- Commentaires : l'en-tête .tsa fait 272 octets (et non 256).

### Vérifié via le Shell (IShellItemImageFactory, pipeline et cache de l'Explorateur)
- Building.tsa enregistré par TSA : miniature 256×192 (≈10 ms) ; vue A puis vue B → B affichée.
- 32 / 96 / 256 / 1024 px : 32×24, 96×72, 256×192, 1024×768.
- Fichier 1.1 et fichier corrompu : WTS_E_FAILEDEXTRACTION (icône TSA), sans plantage.
- Désinstallation : clés supprimées ; regsvr32 (utilisateur) et --register-associations OK.
Non testé : redémarrage Windows, enregistrement HKLM (droits administrateur), installeur (aucun dans le
dépôt), affichage visuel dans une fenêtre de l'Explorateur (poste utilisé par l'utilisateur).

## 2026-10-04 (Projets récents & aperçus du dernier état)

### Added
- Page d'accueil « Projets récents » (`UI/Home/StartPage`, cartes avec aperçu réel, survol, menu
  contextuel) dans un `QStackedWidget` central partagé avec l'unique viewport.
- `Project/RecentProjects` (QSettings) et `Project/ModelPreviewCache` (PNG 480×270 + métadonnées :
  caméra, état d'affichage, révision ; repli sur le chunk THMB du .tsa).
- `OccView::cameraState / applyCameraState / viewState` ; caméra restaurée à la réouverture.
- Capture différée (2,5 s) sur changement de révision ou de caméra, écriture hors thread UI.
- Tests 124–126 (suite `preview`) ; total 147/147. docs/PROJECT_PREVIEWS.md.

### Fixed
- Barre d'état : chevauchement des libellés en 1920 px (somme des largeurs minimales trop grande
  après l'ajout des compteurs et de l'indicateur IA) — compteurs sans minimum, texte court,
  indicateur IA plafonné. Compilé ; non revérifié visuellement (TSA ouvert par l'utilisateur).

## 2026-10-04 (IA Co-Engineering)

### Added
- Module `src/AI` [CORE] : HardwareProfiler (CPUID, RAM, `llama-server --list-devices`), ModelRegistry +
  ModelSelector (mémoire poids + KV, GPU/hybride/CPU, contexte complet prioritaire, mesures prioritaires),
  ModelManager (inventaire TSA/HF/LM Studio, téléchargement asynchrone + SHA-256), OpenAICompatibleProvider
  (SSE, outils, annulation, délai), LocalLlamaServer (+ Job Object), EngineeringContextBuilder,
  AIToolRegistry (liste blanche, propositions + Undo), StructuralChecker, EngineeringKnowledgeBase (BM25),
  AISettings (DPAPI), AILog (métadonnées), AIOrchestrator (routage LOCAL/CLOUD/AUTO, accord Cloud,
  boucle d'outils, diagnostic, auto-benchmark).
- UI : AICoEngineeringDock, AIRuntimeDialog (non modal), MainWindow_AI.cpp, ruban, barre d'état.
- `resources/ai/model_registry.json` (7 modèles Qwen3, données vérifiées), docs/AI_COENGINEERING.md.
- Tests 112–123 (suite `ai`) ; total 144/144.

### Fixed
- Arbre du modèle : nom de section au lieu de « largeur×hauteur » (faux pour les sections circulaires).

## 2026-10-04 (UI — docks, arbre, barre d'état)

### Added

- Dock « Calques & Visibilité » : recherche, boutons Tout afficher / Tout masquer (famille structure),
  défilement vertical, familles Voiles / Fondations / Treillis / Câbles. Les cases Poutres / Poteaux /
  Dalles étaient auparavant branchées sur rien : elles pilotent maintenant
  `OccView::setElementCategoryVisible` (masque de bits appliqué dans `updateElementIsolation`,
  même mécanisme que l'isolation du plan de travail — pas de second système de visibilité).
- Arbre du modèle : barre de recherche, boutons développer/réduire, menu contextuel (actions existantes
  Cadrer sélection / Déplacer / Copier / Supprimer, branchées via `setContextActions`).
- Barre d'état : compteurs Nœuds / Éléments / Sélection (relus toutes les 500 ms, tailles de maps).
- Ruban : libellé complet (2 lignes) pour les panneaux repliés.

### Non vérifié (première passe)
- Pas de clic simulé possible dans cet environnement : filtre, Tout masquer, menu contextuel et compteurs
  validés par compilation, démarrage et rendu, pas par interaction. À tester manuellement.
- Limite : un élément créé alors qu'une famille est masquée n'est masqué qu'à la prochaine passe
  d'`updateElementIsolation`.

### Vérifié par UI Automation (PowerShell + System.Windows.Automation, Qt expose l'accessibilité)
- Tout masquer / Tout afficher : cases décochées puis recochées, nœuds/poutres/poteaux disparaissent puis
  reviennent dans le viewport (captures). Filtre de l'arbre : « Étage » ne garde que les 2 étages.
  Compteurs d'état : « Nœuds : 8   Éléments : 4   Sélection : 0 » (projet test_section_audit.tsa).
- Corrigés après observation : colonne « Element » de l'arbre tronquée (largeur 200 + dock ≥ 380 px) ;
  texte d'invite du dock Propriétés rogné (pages du QStackedWidget en politique Ignored) ;
  AnalysisConfigDialog étirait ses groupes et dépassait l'écran (stretch + hauteur minimale).
- Non vérifié : sélection d'un élément (UIA n'active pas la sélection de QTreeWidget), menu contextuel
  clic droit, Propriétés d'un élément, dock Résultats.

## 2026-10-04 (UI — ruban responsive)

### Changed

- Ruban (`src/UI/Ribbon/`) sans défilement horizontal : `RibbonTab` n'utilise plus de `QScrollArea`.
  À la resize il choisit pour chaque panneau un mode (`RibbonPanelMode`) : Full → IconOnly (petits
  boutons réduits à l'icône) → Collapsed (un bouton déroulant portant le titre, menu = actions du panneau,
  repli de droite à gauche) → menu « Plus » (derniers panneaux en sous-menus).
- `RibbonPanel` : colonnes de petits boutons limitées à 3 ; boutons larges à largeur libre et libellé sur
  deux lignes ; actions mémorisées pour construire les menus de repli.
- `RibbonButton` : infobulle riche (nom en gras, description, « Raccourci : X » extrait de l'action).
- `RibbonBar::setQuickAccess` : barre d'accès rapide (icônes) dans le coin gauche de la rangée d'onglets
  (Nouveau, Ouvrir, Enregistrer, Annuler, Rétablir, Sélection, Cadrer tout, 3D, Calcul).
- Onglets : Accueil, Modèle, Modifier, Structure, Charges, Analyse, Résultats, Affichage, Outils.
  Accueil densifié (édition, structure, appuis & charges, analyse, vue, panneaux) ; panneau « Appuis »
  ajouté à Modèle. Aucune nouvelle commande : uniquement des QAction existantes.
- Vérifié : build OK, 132/132 tests, lancement + captures à 1536 px et 1000 px (pas de scroll, repli
  progressif). Pas de test GUI automatisé.

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
