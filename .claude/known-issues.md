# Known Issues

Last Updated: 2026-11-03 (BUG-181 ajouté : suite display intermittente). Avant : 2026-11-02 nuit (BUG-166 terminé — CRC toujours vérifié, la partie « CRC nul » avait été annoncée corrigée à tort ; BUG-144 secours OpenSees retiré ; BUG-140 Ctrl+Z du canevas Blueprint = graphe, test 598 ; BUG-167 projets protégés AES-256-GCM, format 3.2, test 599). Avant : 2026-11-02 soir (17 bugs corrigés et retirés, tests 592-597 et suites stabilisées : BUG-157 sauvegarde automatique et récupération, BUG-171, BUG-175, BUG-176, BUG-075, BUG-115, BUG-121, BUG-147, BUG-077, BUG-102, BUG-138, BUG-135, BUG-169/104, BUG-173, BUG-174/179 (compte de référence pris après les affichages différés) ; BUG-159 à 168 : restent 161, 167, 168 ; BUG-117 : reste l'équilibre Custom2D ; BUG-137 : reste Linux ; BUG-124 : reste Windows sans OpenSees). Avant : 2026-11-02 (BUG-179 ajouté, test 343 intermittent ; BUG-154, BUG-155, BUG-156 corrigés et retirés, tests 588-590 ; BUG-111 terminé et retiré, annexe nationale du projet, test 591 ; BUG-002 retiré : dalles et voiles calculés en coques depuis le maillage du projet, suite surfaces ; BUG-117 P2 corrigé ; BUG-111 étapes 1-2 faites, test 587 ; BUG-133 corrigé et retiré : installation d'OpenSees depuis l'application par conda-forge ; BUG-173 : moteur Fem3D trouvé dans ../TSA-3D-FEM-Engine ; BUG-153 corrigé et retiré, test 586 — reste documenté : centre de cisaillement, docs/AUDIT_CALCULS.md § 8 ; BUG-151 corrigé et retiré, test 585 ; BUG-152 corrigé et retiré, test 584 ; BUG-149 et BUG-150 retirés : fonctionnalité retirée, analyse non linéaire, ADR-025 ; BUG-177 et BUG-178 corrigés et retirés, tests 583 et 373, voir changelog). Avant : 2026-11-01 (BUG-175, BUG-176 ajoutés ; BUG-174 ajouté ; BUG-074 corrigé et retiré, test 573 ; BUG-173 ajouté). Avant : 2026-11-01 (bugs corrigés retirés). Avant : 2026-10-31 (BUG-148 corrigé ; analyse non linéaire retirée, ADR-025 : BUG-149 et BUG-150 clos par retrait, conservés ; BUG-151 à 168 inchangés). Avant : audit technique du 2026-10-31 (BUG-148 à 168). Avant : 2026-10-26 soir (préparation de l'étape 2 : BUG-006 recompté, BUG-125 à BUG-130 caractérisés, non corrigés). Avant : 2026-10-26 (D1-D6 : BUG-117 P1 corrigés, BUG-111 étape 0, BUG-118 étape E0, BUG-113 tests ajoutés, BUG-110 présenté). Précédent : 2026-10-25 (TSA Core phase 0 : BUG-105 à BUG-109 corrigés, BUG-110 à BUG-115 ouverts ; portage Linux : BUG-091 à BUG-094, BUG-096 à BUG-098 corrigés ; BUG-095, BUG-099 à 101, BUG-103 corrigés le 2026-10-25 ; BUG-102, BUG-104 ouverts ; BUG-076 complété). Un bug entièrement corrigé est retiré de ce fichier (2026-11-01 : 23 entrées FIXED et la section « Corrigés (historique) », 16 entrées, retirées) ; son historique reste dans Git et dans changelog.md. Un bug corrigé en partie reste, avec ce qui demeure ouvert. Un bug d'une fonctionnalité retirée (ADR-022 dynamique, ADR-025 non linéaire) est retiré aussi : il revient avec la fonctionnalité.

## Ouverts

Audit technique du 2026-10-31 : détail, reproduction et correction de BUG-148 à BUG-168 dans docs/audit/2026-10-31/TSA_BUG_REGISTER.md.

## BUG-182
Status: OPEN (LOW, tests, constaté le 2026-11-04) — Test 435 (suite views, clic réel sur une arête du ViewCube) en échec une fois
lors d'une suite complète Release (« orientation d'arête ou de coin exacte ») ; suite views seule 5/5, suite complète suivante
réussie. Sans lien établi avec la modification du jour (icône de l'exécutable). Même famille que BUG-181 : interaction OCCT
sensible au moment (rendu différé).
Related files: tests/test_views.cpp

## BUG-181
Status: OPEN (LOW, tests, constaté le 2026-11-03) — suite `display` encore intermittente malgré BUG-174/179 : Test 371 « 32 = 14 + 17 »
(un objet de résultat affiché non compté par le gestionnaire) lors d'une suite complète Release ; le reste de la suite n'est alors pas
exécuté (588 au lieu de 601). Suite `display` seule : 3 échecs sur 7 lancements depuis PowerShell (`2>&1 | Out-String`, test en échec
non relevé), 9 réussites sur 9 depuis `cmd /c`. Sans lien établi avec fix/build-warnings (renommages de types identiques). Piste :
affichage différé encore en cours au moment du compte (tests/test_display.cpp:277).
Related files: tests/test_display.cpp, src/Viewer/ResultsVisualManager.cpp

## BUG-180
Status: OPEN (LOW, cosmétique, constaté le 2026-11-02) — un plantage laisse un fichier <clé>.lock (63 octets) dans
<AppLocalData>/recovery quand aucune copie n'avait été écrite : RecoveryStore::orphans ne le liste pas et rien ne le supprime
(12 fichiers après 6 essais d'arrêt brutal). Aucun effet sur la récupération. Piste : balayer les verrous périmés sans copie à
l'appel de orphans().
Related files: src/Project/RecoveryStore.cpp

## BUG-170
Status: OPEN (LOW, vu une fois le 2026-10-31, non reproduit) — suite `surfaces` : 12 tests réussis, ligne RESULTS écrite, puis processus
bloqué jusqu'au délai de 600 s (DÉLAI DÉPASSÉ, 622 s au lieu de 10 s), sur le bureau masqué, branche feature/coordinate-axis-gizmos. Blocage à
la sortie du processus, après les tests. 10 autres exécutions dont 6 répétées : 10 s, code 0. Cause inconnue ; lien avec les repères ni
établi ni exclu (tests/test_surfaces.cpp ne crée aucune OccView). À surveiller : capturer la pile si cela se reproduit.

## BUG-158
Status: OPEN (MEDIUM, mesuré) — Fem3D : 117 s contre 18 s pour OpenSees à 29 200 barres, environ 90 % dans SimplicialLDLT.

## BUG-161, BUG-168 (restes de BUG-159 à BUG-168)
Status: OPEN (LOW) — BUG-161 : banc Fem3D/OpenSees du dépôt du moteur (../TSA-3D-FEM-Engine) qui supprime ses références et
conclut « 0 écart » sans comparaison (autre dépôt). BUG-168 : clé d'API de l'IA en clair hors Windows (trousseau Linux /
macOS : libsecret ou QtKeychain, dépendance à décider). BUG-159, 160, 162 à 167 corrigés le 2026-11-02 (changelog).
Reste lié à BUG-167 : protection par mot de passe sous Linux (OpenSSL) écrite mais jamais compilée ni testée.

Espace de travail (2026-10-29, essais réels dans TSA.exe, branche feature/blueprint-workspace) :

## BUG-139
Status: OPEN — vue partagée à la taille minimale de la fenêtre (960 × 560 logiques) avec les panneaux gauche et droit
ouverts : zone centrale plus étroite que les minimums vue 3D (240 px) + canevas (280 px) ; le Blueprint est rogné.
Contournement : mode Blueprint, mode focus (Ctrl+Shift+F) ou fermer une colonne de panneaux. Piste : replier palette et
inspecteur de l'éditeur sous un seuil, ou relever la largeur minimale du workspace en vue partagée.

## BUG-141
Status: OPEN (mineur) — mini-carte peu lisible pour un graphe très allongé (une rangée de 400 nœuds : bande quasi
invisible) ; « Cadrer tout » plafonné à 5 % : un tel graphe n'entre pas entier dans la vue.

## BUG-143
Status: OPEN (tests, inspection du code, non reproduit) — risques de blocage : boîtes modales sans répondeur (suite
mesh 390 / 299), OpenSeesSolver sans délai, wait() sans délai des contrôleurs ; TSA_TestSuite sans délai interne et
écrivant dans le dossier courant (fichiers test_bug110_*.tsa, empty_project.tsa à la racine du dépôt). docs/TESTING.md.

## BUG-137 (dépendances, audit 2026-10-28)
Area: cmake/ (presets, détection, déploiement)
Status: FIXED sous Windows (matrice m1-m7, docs/DEPENDENCIES.md) ; TSALab appelle tsa_find_qt. RESTE : Linux NON TESTÉ.

## BUG-130
Area: Undo/Redo — défauts mineurs caractérisés
Problem: entrées créées sans changement (gabarit de bibliothèque vide, LibraryManager.cpp:329 ; proposition IA sans id,
AIToolRegistry.cpp:392 ; charge nodale validée sans contrôle, LoadPropertiesView.cpp:339) ; appui posé sans NodeModified
(NewNodeDialog.cpp:190) ; GridSettingsDialog sans vue 3D : duplication hors historique (GridSettingsDialog.cpp:394-396) ;
grilles et charges de surface de MainWindow_Mesh (326, 359) sans notification d'observateur ; validation de la grille
active sans changement : xIsBold / yIsBold / zIsBold normalisés (vides → false), sans effet visible (test 522).
Impact: LOW
Status: OPEN (cosmétique seul) — FIXED le 2026-10-29 sauf un point — corrigés : entrées sans changement (bibliothèque, IA, charge nodale),
appui posé sans NodeModified, appuis linéiques notifiés, duplication de grille sans vue 3D (GridSettingsDialog::setModel),
charges surfaciques notifiées (IModelObserver::onSurfaceLoadsChanged / Model::notifySurfaceLoadsChanged). Reste : gras de
grille normalisés à la validation (sans effet visible). Tests 514-523.

## BUG-124
Area: TSALab — suite complète sans OpenSees
Status: CODE CORRIGÉ (TSALab main, commit 58063a7, avec BUG-121 (4)) ; validé sous Linux sans et avec OpenSees, sous Windows avec
OpenSees (27/27, 2026-11-02). RESTE : exécution Windows sans OpenSees (exécutable masqué) non faite.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-124/.

## BUG-123
Area: Tests des raccourcis sous Windows (CI)
Problem: test 261 échoué une fois (CI 40292062083, tentative 1), réussi à la relance du même commit (tentative 2) et dans
les trois exécutions Windows précédentes : intermittent. Suite arrêtée au premier échec (262-266 non exécutés).
Cause probable (non prouvée, la condition ne dit pas quel terme échoue) : writeText du test (QSaveFile::commit, sans
nouvelle tentative) refusé quand l'antivirus ou l'indexeur tient shortcut.txt — phénomène documenté dans
ShortcutManager::writeFile, qui, lui, réessaie (12 × 50 ms).
Impact: LOW — test seulement ; aucune modification de code proposée sans accord.
Status: OPEN. 2026-11-01 : test 262 (« fichier remplacé une deuxième fois : toujours surveillé ») échoué une fois en suite
complète (Release, branche fix/no-wheel-on-fields), 5/5 réussi en suite seule : même famille (écriture de shortcut.txt).

## BUG-117
Area: Unités — consommateurs qui supposent kN (audit transversal du 2026-10-26, rapport hors dépôt)
Status: PARTIEL — P1 corrigés le 2026-10-26 (IFC, Fem3D / OpenSees en N, TSA3D ; tests 499-501), P2 le 2026-11-02 (affichages
indépendants de l'unité du calcul ; test 492), P3 en partie le 2026-11-02 : une seule valeur de g (TSA::Units::kGravity = 9,81,
celle des moteurs : Beam, Cable, LoadManager ; test 58.1) ; UnitSystem reconnaît « MN » explicitement. RESTE OPEN : équilibre
Custom2D vérifié par construction (résidu calculé avec les efforts du même solveur : ne détecte pas une erreur d'assemblage) —
piste : résidu à partir des réactions et des charges appliquées, indépendamment de la résolution.
Related files: src/Analysis/Engines/Custom2D/

## BUG-118
Area: Paquets .tsalib — suite de BUG-109 (conception du 2026-10-26, hors dépôt)
Problem: manifeste extrait ≠ manifeste validé (C4) — CORRIGÉ le 2026-10-26 (empreinte de manifest.json = en-tête, test
496) ; flux non contrôlé — CORRIGÉ (stream.status(), en-tête relu comparé) ; restent : extraction non atomique, masquage
d'une extension (registre indexé par id de fiche, recherche approximative), ref.libraryId jamais renseigné (déchargement
sans effet), appartenance par préfixe, import de dossier sans approbation, minimum_tsa_version non vérifié, SNAP
(TSAFile.cpp:357-392) qui écrit org.tsaraloha.tsalib 1.0.0 pour toute fiche, « signé » annoncé à tort (ExtensionPackager.h:45).
Status: OPEN — étape E0 FAITE le 2026-10-26 (D3) : copie du paquet en préparation (<parent>/.staging/<uuid>), inspection et
extraction de la copie, création exclusive des fichiers, un seul renommage final ; refus si la cible n'est pas vide (pas de
mise à jour en place), si l'id est celui d'une extension livrée, si minimum_tsa_version (déclarée) > version de l'hôte ;
inspection : octets en trop, doublons de casse, noms réservés Windows, point / espace final, caractères interdits ;
découverte : le premier dossier (livré) l'emporte, conflit dans discoveryWarnings() ; import de dossier réservé au mode
développeur ; commentaire « signé » retiré. Test 505. RESTE : E1 (registre qualifié, provenance SNAP), E2 (magasin
versionné, journal, reprise, retour arrière), E3 (minisign/libsodium), E4, E5.

## BUG-076
Area: Tests / intégration continue (vue 3D OCCT)
Problem: toute suite qui crée une `OccView` s'arrête (code 0xC0000409, `abort()` du CRT, sans message) sous la
plateforme Qt `offscreen` : la fenêtre n'a pas de vrai HWND, OCCT (`WNT_Window`, contexte WGL) ne peut créer son
contexte OpenGL. Le workflow de publication (Setup-Livrable, `QT_QPA_PLATFORM: offscreen` à l'étape Tests) fait donc
échouer `display`, `audit`, `multidoc` et `TSA_AllTests` — ce n'était pas un défaut du code des suites.
Reproduction: `set QT_QPA_PLATFORM=offscreen` puis `TSA_TestSuite.exe --suite=display` → 0xC0000409 dès la première
vue ; avec `QT_QPA_PLATFORM=windows` (vraie fenêtre) : display 6/6, audit 4/4, multidoc 10/10 (17/10/2026, build-ci
propre). Équivalent local de la CI (offscreen) : 26/30 tests CTest réussis, exactement ces quatre en échec. Même résultat avec le binaire
antérieur aux travaux IFC (TSA_TestSuite du 17/10 13:54) : défaut préexistant, indépendant d'IFC.
Impact: HIGH pour la publication (étape Tests de la CI en échec), aucun impact pour l'utilisateur.
Status: OPEN — piste : exécuter ces suites avec la plateforme `windows` sur le runner (session de bureau des runners
GitHub Windows ; OpenGL logiciel à vérifier : Mesa llvmpipe si le pilote Microsoft ne fournit que GL 1.1), ou les
annoncer sautées sous `offscreen` comme le test 408 de la suite `ifc`. À valider par une exécution réelle de la CI.
18/10/2026 — correctif préparé dans Setup-Livrable (branche locale `feature/tests-vue3d`, non publiée) :
`scripts/run_tests.ps1` en deux passes ; passe `offscreen` sans les suites à vue 3D, passe en plateforme `windows`
pour AllTests, audit, display, multidoc, drawing et ifc (408 et 419 réellement exécutés), avec le rendu OpenGL
logiciel de Mesa 26.2.3 (llvmpipe, empreinte figée) posé à côté de l'exécutable des tests puis retiré. Échec explicite
en session 0, si une suite attendue manque, si un test est sauté ou si le total de la suite complète est incomplet.
Essai local (build-ninja-release, 79dfdea) : offscreen 25/25 ; windows + Mesa 6/6, suite complète 427/427, rendu
« Mesa — llvmpipe, OpenGL 4.6 » (test 370), aucun [SKIP]. Garde-fous éprouvés sur de fausses suites CTest (test
sauté avec CTest « réussi », arrêt 0xC0000409, suite absente : échec dans les trois cas). Reste à vérifier sur le
runner : session interactive (session ≠ 0) et durée de la passe native.
24/10/2026 — même défaut sous Linux : `QT_QPA_PLATFORM=offscreen` → « X Error … BadWindow » et arrêt (suites
display, multidoc, meshview après le test 343, désormais [SKIP] comme 408). scripts/linux/test.sh choisit « xcb » quand
un affichage X11 et le plugin xcb de Qt (libxcb-cursor0) sont disponibles.
Related files: tests/test_display.cpp, tests/test_audit.cpp, tests/test_multidoc*.cpp, Setup-Livrable/.github/workflows/release.yml, Setup-Livrable/scripts/run_tests.ps1

## BUG-079
Area: Tests / mesure de temps (test 20, suite `commands`)
Problem: après BUG-035 (meilleur de 3 annulations, seuil de 350 ms inchangé), le test 20 échoue encore de temps en
temps quand la machine est chargée : deux échecs sur une dizaine d'exécutions complètes le 18/10/2026, chaque fois
pendant une compilation ou une autre suite en parallèle ; il repasse seul et dans les exécutions suivantes (suite
complète native 427/427 à 07:49 le 18/10/2026).
Impact: MEDIUM pour la CI (échec possible sur un runner partagé), aucun pour l'utilisateur.
Status: OPEN — seuil volontairement non modifié. Pistes à évaluer avant toute décision : mesurer le temps CPU du
processus plutôt que le temps écoulé, ou isoler les mesures de performance dans une suite à part lancée sans
parallélisme ; ne pas assouplir le seuil pour faire passer la CI.
Related files: tests/test_commands.cpp

## BUG-006
Area: Undo/Redo
Problem: 28 appels de production à `Model::pushUndoState` dans 20 fichiers (recompté le 2026-10-26 sur 39155c4 ;
l'ancien décompte « 37 », OccView_Events et MainWindow_Transform compris, est périmé : ces deux fichiers n'en ont plus).
Familles : F1 panneaux 10, F2 chargements 9, F3 appuis 3, F4 grilles 3, F5 création / bibliothèques 2, F6 IA 1. S'y
ajoutent 5 appels à `pushLabelUndoState` (Annotation/DimensionService.cpp ×4, Topology/TopologyNumberingService.cpp).
Fonctionnels, mais sans `EditRecord` sauf poutre, poteau (section, matériau) et nœud (coordonnées).
Impact: LOW
Status: OPEN
Notes: comportement figé AVANT toute migration vers EditTransaction par la suite « undo » (tests 513-523, branche
test/etape2-caracterisation, non commitée) ; 25 appels sur 28 atteints (MainWindow_Mesh.cpp:326, :359 et
MainWindow_Tools.cpp:464 sont dans la couche application, non liée aux tests). Défauts figés : BUG-125 à BUG-130.

## BUG-008
Area: Viewport / Performance
Problem: Ouverture d'un modèle de 4 896 barres ≈ 16 s en Debug (reconstruction 3D ≈ 13 s : une
sphère + un label par nœud, un solide par barre). Non mesuré en Release.
Impact: LOW
Status: OPEN
Notes: piste : partager la géométrie des sphères de nœuds (TopLoc_Location).

## BUG-012
Area: Environment
Problem: Processus TSA.exe « clones WER » bloqués (anciens crashs) qui verrouillent l'exécutable
(LNK1168) ; non terminables (accès refusé).
Impact: LOW
Status: OPEN — contournement : renommer TSA.exe ; disparaissent au redémarrage.

## BUG-021
Area: AI / performance
Problem: sur le poste de développement (AutoCAD, Revit, Chrome ouverts, ≈3 Go de RAM libre), Qwen3 4B
Q5_K_M tombe à ≈1,3 jeton/s (CPU comme GPU) : pages du modèle évincées. Le chargement en `--no-mmap`
ne termine pas en 4 min. Le sélecteur en tient compte (RAM libre mesurée) et recommande alors un
modèle compact ; le diagnostic signale l'écart mesure/estimation.
Impact: MEDIUM (qualité des réponses limitée sur machine chargée)
Status: OPEN (contournement : fermer les applications lourdes ; modèle plus léger)

## BUG-022
Area: IO / compatibilité
Problem: une version de TSA antérieure au format 1.2 ne peut pas ouvrir un fichier 1.2 (bloc d'aperçu lu
comme payload → CRC invalide). Lecture des anciens formats par la version courante : OK (test 127).
Impact: MEDIUM si des versions antérieures sont distribuées ; nul en développement.
Status: OPEN (documenté, ADR-016)

## BUG-023
Area: Platform / Explorateur
Problem: après désinstallation de l'extension, les miniatures déjà en cache Windows (thumbcache) restent
visibles tant que les fichiers ne changent pas. Constaté via IShellItemImageFactory.
Impact: LOW (comportement Windows ; Nettoyage de disque › Miniatures)
Status: OPEN (documenté)

## BUG-025
Area: Tests GUI (UI Automation)
Problem: un `Invoke` UIA sur une action qui ouvre une fenêtre modale (`exec()`) ne rend pas la main et bloque les
requêtes UIA suivantes vers TSA (timeouts) ; l'application reste réactive.
Notes: vérification de la fenêtre Analysis faite par capture d'écran (Invoke lancé dans un job séparé).
Impact: LOW (outillage de test)
Status: OPEN

## BUG-028
Area: BIM / IFC
Problem: export IFC partiel — charges / cas / combinaisons, relâchements d'extrémité, excentrements, appuis orientés et
grilles non exportés ; unités dérivées (module, masse volumique) non déclarées ; GlobalId des relations et Psets
recréés à chaque export ; fondations sans objet analytique IFC. Liste tenue à jour dans docs/IFC_MAPPING.md §4.
Impact: MEDIUM (échange incomplet, pas de perte dans le .tsa)
Status: OPEN
Related files: src/BIM/IFC/IfcExporter.cpp, docs/IFC_MAPPING.md

## BUG-033
Area: UI / Fenêtre
Problem: non vérifiable sur le poste de test (un seul écran à 125 %, Windows 10) : DPI différent par écran et
Windows 11 « Snap layouts » au survol du bouton Agrandir (HTMAXBUTTON non renvoyé : le renvoyer exigerait de gérer
les clics non clients du bouton). Le redimensionnement par les bords couverts par les fenêtres natives du workspace
est corrigé (voir FIX-2026-10-06-RESIZE).
Impact: LOW
Status: OPEN (à vérifier sur Windows 11 / multi-écrans)
Related files: src/UI/Shell/AppShell.cpp

## BUG-036
Area: Build (cache CMake local)
Problem: le 2026-10-08, `build-ninja-debug/CMakeCache.txt` avait `CMAKE_CXX_FLAGS` (et `_DEBUG`…) vides : compilation
sans /EHsc ni /Zi /Od (avertissements C4530) et test 96 en échec (destructeurs non appelés pendant le déroulement d'une
exception → pas de rollback automatique de l'EditTransaction). Cause exacte non identifiée (une configuration lancée
alors que vcvars64.bat signalait « vswhere.exe non reconnu » est suspecte).
Impact: MEDIUM (comportement des exceptions faux, sans erreur de compilation)
Status: WORKAROUND — `cmake --preset ninja-debug --fresh` rétablit les drapeaux (212/212 vérifié). Contrôle rapide :
`Select-String build-ninja-debug\CMakeCache.txt -Pattern "^CMAKE_CXX_FLAGS:"` doit contenir /EHsc.
