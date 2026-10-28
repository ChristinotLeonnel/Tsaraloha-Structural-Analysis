# Known Issues

Last Updated: 2026-10-26 soir (préparation de l'étape 2 : BUG-006 recompté, BUG-125 à BUG-130 caractérisés, non corrigés). Avant : 2026-10-26 (D1-D6 : BUG-117 P1 corrigés, BUG-111 étape 0, BUG-118 étape E0, BUG-113 tests ajoutés, BUG-110 présenté). Précédent : 2026-10-25 (TSA Core phase 0 : BUG-105 à BUG-109 corrigés, BUG-110 à BUG-115 ouverts ; portage Linux : BUG-091 à BUG-094, BUG-096 à BUG-098 corrigés ; BUG-095, BUG-099 à 101, BUG-103 corrigés le 2026-10-25 ; BUG-102, BUG-104 ouverts ; BUG-076 complété). Ne pas supprimer un bug corrigé : passer son statut à FIXED (avec preuve).

## Ouverts

Validation Windows du 2026-10-28 (docs/reports/WINDOWS_VALIDATION_REPORT.md, identifiants W-n du rapport) :

## BUG-131 (W-1)
Area: tests — chemins UTF-8 sous MSVC
Problem: test_thumbnail_provider.cpp:66, test_mesh.cpp:329/337, test_meshview.cpp:459 passaient un chemin UTF-8 étroit à
std::fstream / std::ofstream / std::filesystem::exists (page de code ANSI) : échec avec un TEMP non ASCII (C:\Users\T. Léonnel).
Status: FIXED (2026-10-28, TSA::Core::utf8Path ; thumbnail 3/3, meshview 12/12, test 293 réussi avec le TEMP non ASCII)

## BUG-132 (W-2)
Area: IA — PythonAIBridge::findPython
Problem: sous Windows, « python » du PATH = alias WindowsApps du Microsoft Store (0 octet, code 9009) : pont IA inutilisable.
Status: FIXED (2026-10-28, « py » essayé d'abord sous Q_OS_WIN ; test 489 réussi, ai 24/24, operator 8/8)

## BUG-133 (W-3)
Area: OpenSees — téléchargement à l'exécution
Problem: OpenSeesManager::officialDownloadUrl en 404 (opensees.berkeley.edu redirige vers peer.berkeley.edu/openseesOpenSees/…,
défaut du serveur, constaté le 2026-10-28) ; downloadAndInstall ne vérifie pas l'empreinte (winget : 5E37A5A9…FB41).
Impact: HIGH pour le calcul OpenSees sur un poste neuf
Status: OPEN

## BUG-134 (W-5)
Area: tools/ci/run_test_suites.py
Problem: mesh déclarée dépendante de Gmsh seulement ; le test 294 calcule avec OpenSees.
Status: FIXED (2026-10-28)

## BUG-133 (W-3) — mise à jour 2026-10-28
Source officielle de l'archive Windows disparue (404). Contournement : OpenSees 3.8.0 conda-forge épinglé
(cmake/SetupDependencies.cmake, docs/DEPENDENCIES.md §5). Status: PARTIEL — installation vérifiée, exécution NON
validée ; OpenSeesManager::downloadAndInstall (application) vise encore l'adresse officielle.

## BUG-136 (W-10)
Area: src/Diagnostics/Logger.cpp
Problem: chemins UTF-8 étroits (ofstream, filesystem) : aucun journal écrit quand le dossier de journaux contient un
caractère non ASCII (paquet installé, profil « T. Léonnel ») ; invisible depuis le dossier de compilation.
Status: FIXED (2026-10-28, branche build/dependency-detection) — test 37.8 (échoue sans le correctif) ; journal constaté
dans AppData après relance du paquet.

## BUG-137 (dépendances, audit 2026-10-28)
Area: cmake/ (presets, détection, déploiement)
Problem: Qt figé 6.11.2 dans les presets (W-9) ; plancher Qt 6.2 faux pour les traductions (qt_add_lrelease exige 6.9) ;
windeployqt en cache d'une autre Qt possible ; DLL OCCT déployées depuis un chemin figé ; aucun contrôle de chaîne de
compilation / mélange dans CMake.
Status: FIXED sous Windows (matrice m1-m7, docs/DEPENDENCIES.md) ; Linux NON TESTÉ ; TSALab sans détection auto de Qt
(appel tsa_find_qt à ajouter dans TSALab, accord requis).

## BUG-135 (W-6, W-7, W-8)
Area: chemins UTF-8 restants, déploiement
Problem: TSALab lab/Tests/LabTests.cpp:120 (L5, même défaut que BUG-131 : 17/27, 27/27 avec TEMP ASCII) ;
test_torsion.cpp:724 (remove sur chemin étroit, fichier laissé dans TEMP) ; DeployDependencies.cmake:30 liste Qt6* comme
« introuvables » malgré EXCLUDE_REGEXES (cosmétique).
Status: PARTIEL (2026-10-28) — W-8 FIXED : DeployDependencies.cmake retire Qt6* de la liste des introuvables
(filtre d'abord placé dans RuntimeDependencies.cmake, déplacé : il affaiblissait le contrôle strict du paquet ;
vérifié : plus d'avertissement, package_TSA + verify_package OK). W-7 corrigé
(TSA::Core::utf8Path, compilé) mais NON EXÉCUTÉ : la suite torsion s'arrête au test 321 sans OpenSees. W-6 OPEN
(TSALab : autre dépôt, correctif d'une ligne à autoriser).

Défauts BUG-125 à BUG-130 : constatés et FIGÉS par la suite « undo » (tests 513-523, branche test/etape2-caracterisation,
non commitée) ; non corrigés volontairement (caractérisation avant l'étape 2). Une correction devra modifier le test
marqué « DÉFAUT FIGÉ » explicitement.

## BUG-125
Area: Panneau Propriétés — câble
Problem: valider le panneau sans rien changer modifie l'aire de la définition : applyChanges réécrit tous les champs
depuis l'affichage arrondi (CablePropertiesView.cpp:93, 275-280 ; aire 0x1.6b2f7682c2457p-12 → 0x1.6b3a0abdc923cp-12 m²
pour Ø 21 mm). Même famille que le défaut corrigé pour les poteaux (test 221) et BUG-106.
Impact: MEDIUM (modification silencieuse d'une donnée mécanique)
Status: OPEN — figé au test 514

## BUG-126
Area: Panneau Propriétés — nœud
Problem: valider le panneau sans rien changer impose la couleur « #2563EB » à un nœud sans couleur
(NodePropertiesView.cpp:269, 526).
Impact: LOW
Status: OPEN — figé au test 514

## BUG-127
Area: Undo/Redo — EditTransaction
Problem: (1) commit sans modification : entrée Annuler créée, révision +1, résultats déclarés périmés
(UndoManager.cpp:176-185) ; (2) rollback, même sans modification : différentiel vide publié, révision +1, isModified
forcé (Model_Snapshots.cpp:172, applySnapshotData), résultats périmés par ResultsValidityGuard ; (3) imbrication : un
rollback interne remet la profondeur à 0 (UndoManager.cpp:194), le commit externe ne crée pas d'entrée (178-179).
Impact: MEDIUM (une commande échouée périme les résultats ; à traiter avant la migration de l'étape 2)
Status: OPEN — figé aux tests 513 et 520

## BUG-128
Area: Cas de charge — notifications
Problem: supprimer un cas de charge (LoadCaseDialog.cpp:231-236) ou réinitialiser les cas Eurocodes (247) efface des
charges sans notification NodalLoadRemoved / MemberLoadRemoved ; seule LoadCaseChanged est émise.
Impact: MEDIUM (vues et arbre non informés du retrait des charges)
Status: OPEN — figé au test 515

## BUG-129
Area: Undo/Redo — exception après l'enregistrement
Problem: une exception levée après pushUndoState (notification d'observateur, MemberLoadCommands.cpp:80-92 ; panneaux)
laisse l'entrée Annuler et une application partielle (aucun rollback). Annuler rend l'état initial.
Impact: LOW (aucun observateur de production ne lève aujourd'hui ; à traiter par la migration vers EditTransaction)
Status: OPEN — figé aux tests 514 et 515

## BUG-130
Area: Undo/Redo — défauts mineurs caractérisés
Problem: entrées créées sans changement (gabarit de bibliothèque vide, LibraryManager.cpp:329 ; proposition IA sans id,
AIToolRegistry.cpp:392 ; charge nodale validée sans contrôle, LoadPropertiesView.cpp:339) ; appui posé sans NodeModified
(NewNodeDialog.cpp:190) ; GridSettingsDialog sans vue 3D : duplication hors historique (GridSettingsDialog.cpp:394-396) ;
grilles et charges de surface de MainWindow_Mesh (326, 359) sans notification d'observateur ; validation de la grille
active sans changement : xIsBold / yIsBold / zIsBold normalisés (vides → false), sans effet visible (test 522).
Impact: LOW
Status: OPEN — figés aux tests 515 à 522 (MainWindow_Mesh : lecture du code seulement, non atteignable par les tests)

## BUG-119
Area: Vue 3D — destruction d'OccView avec le rendu logiciel Mesa (llvmpipe)
Problem: corruption du tas (« free(): invalid next size (fast) ») à la destruction de la vue : ViewportContainer →
OccView::~OccView → V3d_Viewer → OpenGl_Window::~OpenGl_Window → libgallium. Reproduction : LIBGL_ALWAYS_SOFTWARE=1
QT_QPA_PLATFORM=xcb TSA_TestSuite --suite=views (après le test 446, deux documents) ; aussi sur GitHub Actions (Xvfb).
Présent sur eb7e6db (antérieur à BUG-110). Pas reproduit avec le pilote matériel, ni avec MALLOC_CHECK_=3 sous ce pilote.
Impact: HIGH [H] pour les postes sans pilote graphique (machines virtuelles, bureau à distance) : plantage possible à la
fermeture d'un document. Piste [H] : un OpenGl_GraphicDriver / Aspect_DisplayConnection par vue au lieu d'un seul partagé.
Windows (GitHub Actions, sans GPU, OpenGL 1.1 logiciel) : violation d'accès 0xC0000005 après le test 435 (CI 40270307383).
Cause démontrée (2026-10-26) : ~QWidget d'une fenêtre visible appelle QWindow::close() avant deleteChildren : les
surfaces natives sont détruites AVANT les OccView enfants, dont le contexte OpenGL (OpenGl_Window) est ensuite détruit
sans sa fenêtre X. Expériences : vue détruite avant son conteneur → plus de plantage ; OccView seule fermée (close())
puis détruite → plantage au premier test (logiciel), 17/17 avec le GPU. ASan (code non corrigé) : aucun rapport
(double libération exclue ; écriture hors limites dans Mesa non instrumenté non détectable par ASan).
Status: FIXED localement (Linux) — OccView::eventFilter sur la fenêtre native : SurfaceAboutToBeDestroyed → vue OCCT
libérée (Remove, caméra conservée), SurfaceCreated → vue recréée. Test 509 (fermeture/réouverture, destruction par le
parent, vue seule fermée). Validé : llvmpipe sur X réel, Xvfb (comme la CI), GPU Intel, ASan sans erreur, suite
complète 507/508, 56/56 suites. CI 40280259182 (commit 6fec972) : Linux views 18/18 (test 509 compris). Windows :
plantage toujours présent, mais à un autre endroit (après le test 435, déjà avant correctif) → défaut distinct, BUG-122.
CI 40292062083 (431c81e, avec BUG-122) : Windows « GDI Generic » views 19/19, test 509 RÉUSSI (fermeture/réouverture,
destruction par le parent) aux deux tentatives. Validé sous Windows en CI (rendu logiciel) ; pas sur poste Windows à GPU.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-119/.

## BUG-120
Area: AppPaths::occtResourcesDir sous Linux (développement)
Problem: les ressources OCCT (Shaders…) ne sont cherchées que dans resources/occt (paquet) et dans le SDK Windows
opencascade-8.0.1-vc14-64/src (non versionné) : test 286 en échec sur GitHub Actions (le poste local a le SDK Windows).
OCCT trouve lui-même ses shaders sous Linux : impact sur le test et sur un paquet Linux futur.
Status: FIXED localement — TSA_OCCT_RESOURCE_DIR = OpenCASCADE_RESOURCE_DIR de CMake (share/opencascade/resources sous
Linux, src du SDK Windows), utilisé en développement par AppPaths::occtResourcesDir (plus de nom de dossier codé en dur) ;
test 286 : OpenSees embarqué absent → [SKIP] motivé (TSA_PACKAGE_OPENSEES=OFF par défaut), jamais compté réussi ; suite
deployment de nouveau exécutée en CI. Vérifié localement (3/3) ; CI non relancée.

## BUG-122
Area: Vue 3D sous Windows avec l'OpenGL 1.1 logiciel « GDI Generic » (runners GitHub Actions, sans GPU)
Problem: suite views : violation d'accès 0xC0000005 juste après le test 435 (fin du test 435 : destruction d'une OccView
seule, ou début du test 436 : nouvelle vue, tracé), avant comme après le correctif BUG-119 (CI 40270307383 et
40280259182). OCCT signale « OpenGL driver is too old… Fallback using deprecated fixed-function pipeline ». Non reproduit
sous Linux, même avec un contexte OpenGL 1.1 émulé (MESA_GL_VERSION_OVERRIDE=1.1 : 18/18).
Impact: HIGH [H] pour les postes Windows sans pilote graphique (machines virtuelles, bureau à distance).
Cause établie (CI 40283836445, cdb) : test_views.cpp:549 setObjectSnapEnabled(true) → refreshSnapAtCursor → V3d_View::Redraw
→ OpenGl_Text::Render → OpenGl_Text::drawRect (fonction non exportée, identifiée par .pdata et désassemblage) :
`theCtx->core20fwd->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)` (source OCCT 8.0.1, OpenGl_Text.cxx:649) avec core20fwd NUL
(contexte OpenGL < 2.0) ; lecture à 0 + 0xe8. Déclenché par les textes Aspect_TODT_SUBTITLE de TSA : étiquette du
marqueur d'accrochage (SnapMarker.cpp:184) et cotations (DimensionRenderer.cpp:176). Défaut d'OCCT, contournable dans TSA.
Correctif (local, non commité, 2026-10-26) : OccView::contextHasCore20() lit OpenGl_Context::core20fwd du contexte
partagé après SetWindow (aucune commande OpenGL) ; sans elle, SnapMarker et DimensionRenderer passent en Aspect_TODT_DEKALE
(drawText, core11fwd seul) via Viewer/TextDisplayFallback.h ; détection refaite à la recréation de la surface
(restoreViewWindow, nouveau contexte) et marqueur affiché recalculé au changement de mode (sinon présentation périmée) ;
tests 510 (snap, logique) et 511 (views, vue réelle, fermeture/réouverture ; contre-épreuves : 511 échoue sans l'un ou
l'autre).
Linux : views 19/19 en llvmpipe 4.5, en MESA_GL_VERSION_OVERRIDE=1.1 et sur GPU Intel 4.6 ; rendu de fond forcé en 1.1
= SIGSEGV dans drawRect (gdb), reproduction locale de la cause. Risques restants : Aspect_TODT_DIMENSION (PrsDim, non
utilisé par TSA). OpenGl_Structure.cxx:62 est en fait protégé (core20fwd != nullptr, ligne 45). Reproduction minimale
hors TSA et correctif OCCT d'une ligne (core11fwd->glDrawArrays) vérifiés sous Linux : TSA-rapports/2026-10-26/
OCCT_signalement/ (brouillon non soumis). Preuves :
TSA-rapports/2026-10-26/preuves/BUG-122/.
Commit 1f9ce28 (CI 40289561813) : Linux RÉUSSIE (views 19/19, 509-511 PASS) ; Windows : compilation ÉCHOUÉE sur
tests/test_snap.cpp (variables « near »/« far » = macros de <windows.h>, désormais inclus via GridRenderer.h/OCCT) → tests
Windows non exécutés. Variables renommées farView/nearNode (erreur reproduite sous Linux par -Dnear= -Dfar=), commit 431c81e.
CI 40292062083 : Windows « GDI Generic » : compilation complète, views 19/19 (scénario du test 435 franchi, 436-449, 509 et
511 « core20fwd absente » réussis), snap 8/8 (510), deployment 3/3, views sous cdb sans exception ; aux deux tentatives.
Status: FIXED — validé en CI Windows (rendu logiciel GDI Generic) et Linux ; pas encore sur poste Windows MSVC 19.51.

## BUG-124
Area: TSALab — suite complète sans OpenSees
Problem: sans OpenSees, la suite TSALab reste en échec : L10 (« calcul OpenSees ») exige OpenSees et, la suite
s'arrêtant au premier échec, empêche l'exécution des tests suivants (6 / 27 dans le rapport de correction de BUG-121 (4),
apres_sans.log). D'autres tests sont suspectés, non confirmés (non atteints) ; pistes d'investigation seulement, pas des
causes démontrées : LabTests.cpp, boucle sur custom2d/opensees (vers la ligne 462), réglages opensees (vers 558-583),
L14 « OpenSees disponible » (vers 694). Distinct de BUG-121 (4) (L9 seul).
Impact: TSALab ne peut pas terminer sa suite sur un poste ou une CI sans OpenSees.
Cause démontrée (diagnostic du 2026-10-26) : (1) main : (structuralOnly || runSuite_Lab) && runSuite_Structural — un échec de
la suite Lab empêchait l'exécution des 16 tests Structural (L15-L30), pourtant indépendants d'OpenSees (16/16 avec
--structural) ; (2) L10 à L14 exigeaient OpenSees sans contrôler sa disponibilité (L10 : calcul du portique spatial ; L11 :
passage opensees de la boucle ; L12 : extraction avancée et mode léger ; L13 : chaîne FEA-01/FEA-07 ; L14 : calcul des coques).
Correction (TSALab/lab/Tests/LabTests.cpp seul, branche fix/l9-sans-opensees, avec BUG-121 (4)) : les deux suites
s'exécutent toujours ; disponibilité d'OpenSees lue dans le registre des moteurs (availability(), jamais le texte d'une
erreur) ; seules les parties dépendantes d'OpenSees sont ignorées, avec une ligne [SKIP] et le motif du moteur ; compteur
« parties ignorées » dans le bilan RESULTS. Résultats Linux Release consignés dans le rapport de correction : sans OpenSees
27/27 réussis, code de sortie 0, six parties ignorées (L9 à L14) ; avec OpenSees 27/27, aucune partie ignorée (sortie
identique à la référence) ; un faux moteur disponible qui échoue au calcul n'est pas traité comme indisponible (L9 à L14
en échec, aucun [SKIP]) ; les erreurs numériques font toujours échouer les tests (RDM-01, FEA-01 faussés) ; les suites Lab
et Structural sont toutes deux exécutées ; échecs numériques et échecs de calcul toujours détectés. Commit TSALab local
58063a70e9d54fc461f85d1fe45acaa2c277ab5f (branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul), non poussé.
Restent à valider : Windows/MSVC et poste sans Gmsh.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-124/.
Status: CORRIGÉ LOCALEMENT — validation Linux effectuée — commit TSALab créé — validation Windows en attente.

## BUG-123
Area: Tests des raccourcis sous Windows (CI)
Problem: test 261 échoué une fois (CI 40292062083, tentative 1), réussi à la relance du même commit (tentative 2) et dans
les trois exécutions Windows précédentes : intermittent. Suite arrêtée au premier échec (262-266 non exécutés).
Cause probable (non prouvée, la condition ne dit pas quel terme échoue) : writeText du test (QSaveFile::commit, sans
nouvelle tentative) refusé quand l'antivirus ou l'indexeur tient shortcut.txt — phénomène documenté dans
ShortcutManager::writeFile, qui, lui, réessaie (12 × 50 ms).
Impact: LOW — test seulement ; aucune modification de code proposée sans accord.
Status: OPEN.

## BUG-121
Area: Panneaux Propriétés (essais d'interface du 2026-10-26)
Problem: (1) CORRIGÉ : liste « Type de profilé » désactivée affichant un type faux pour une forme hors liste (UPN montré
« Profilé en I » ou « Rectangulaire ») → désignation réelle affichée (Beam/ColumnPropertiesView, test 493 complété).
(2) OUVERT : libellés des champs masqués (« Largeur (b) : », « Rayon (r) : »…) visibles sans champ (QFormLayout) ;
« Rayon (r) : » (cercle) voisin de « Rayon de congé (r) : ». (3) OUVERT : « IA non configurée » (barre d'état) non traduit.
(4) TSALab test L9 échouait sans OpenSees au lieu de signaler la partie OpenSees non exécutée. Cause (dans L9 seul) :
l'assertion exigeait que toutes les vérifications réussissent (16/16), OpenSees comprises ; l'en-tête exigeait
« OpenSees 3.8 » ; le bloc des références externes forçait le moteur opensees. La logique du banc était juste et n'a pas
été modifiée (moteur indisponible → cas skipped avec motif, vérifications non disponibles, jamais en échec), mais
checksUnavailable() mêle cas ignorés et calculs échoués d'un moteur présent : c'est au test de les distinguer.
Correction locale (TSALab, branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul) : disponibilité d'OpenSees lue
dans le registre des moteurs ; sans OpenSees, cas OpenSees exigés ignorés (non exécutés, motif « indisponible », aucune
vérification disponible) avec une ligne [SKIP] ; tout autre cas exécuté avec toutes ses vérifications réussies ; version
jamais inventée (« OpenSees (version inconnue) ») ; références externes sur custom2d sans OpenSees. Comportement avec
OpenSees inchangé. Résultats Linux Release consignés dans le rapport de correction du 2026-10-26 : A (sans OpenSees) L9
réussi, 8 vérifications Custom2D réussies, 8 OpenSees non exécutées ; B (avec OpenSees) L9 16/16, TSALab 27/27 ;
C1 et C2 (valeur attendue faussée, sans et avec OpenSees) et C4 (OpenSees disponible, calcul en échec) : L9 en échec,
vrais échecs détectés ; C3 (exécutable qui plante à la sonde) : partie ignorée avec le motif déclaré par le moteur.
Preuves : TSA-rapports/2026-10-26/preuves/BUG-121_4/. Windows/MSVC non testé. Commit TSALab local
58063a70e9d54fc461f85d1fe45acaa2c277ab5f (branche fix/l9-sans-opensees, lab/Tests/LabTests.cpp seul, « fix(tsalab): tests
du laboratoire sans OpenSees (BUG-121 (4), BUG-124) »), non poussé.
Statut (4) : CORRIGÉ LOCALEMENT — validation Linux effectuée — commit TSALab créé — validation Windows en attente.
Status: PARTIAL.

## BUG-116
Area: Tests — test 166 (arbre indexé, tests/test_model_cleanup.cpp)
Problem: un seul processEvents() après 1500 suppressions regroupées par minuteur 0 ms : échec systématique quand la suite
cleanup est lancée seule sous xcb (6/6), réussite dans la suite complète.
Status: FIXED (2026-10-26) — attente bornée (2 s) jusqu'à la condition, mêmes assertions ; 6/6 réussis (~10 ms).

## BUG-117
Area: Unités — consommateurs qui supposent kN (audit transversal du 2026-10-26, rapport hors dépôt)
Problem: P1 : raideurs d'appui exportées en kN/m dans un IFC qui déclare N (IfcExporter.cpp:132-139, 187, 423-430),
import sans facteur (IfcImporter.cpp:489-497) — VÉRIFIÉ ; comparaison Fem3D / OpenSees si OpenSees en N
(Fem3DEngine.cpp:195-300) ; export TSA3D des réactions (Tsa3dExport.cpp:376-387). P2 : affichages « kN » en dur
(résumé du dock, console, propriétés du nœud, flèches 3D, légendes des diagrammes : diagramUnit jamais renseigné,
coques) ; libellés faux (NDCGenerator « Résidu <= 0.001 kN » relatif ; moment réparti kN·m/m). P3 : équilibre Custom2D
par construction, g = 9,81 / 9,80665 (Beam::weight en N), UnitSystem : toute chaîne ≠ « N » vaut kN.
Impact: HIGH pour l'IFC (échange avec d'autres logiciels), MEDIUM ailleurs (OpenSees en N seulement).
Status: PARTIELLEMENT FIXED (2026-10-26, D4 approuvé) — P1 corrigés, reproduits avant correction : IFC (raideurs en N/m,
IfcDerivedUnit LINEARSTIFFNESSUNIT / ROTATIONALSTIFFNESSUNIT déclarées ; import : unité déclarée, sinon FORCEUNIT /
LENGTHUNIT / PLANEANGLEUNIT ; ancien export TSA reconnu par « … STEP writer » sans unité → kN/m + avertissement ; test 501,
lecteur indépendant IfcOpenShell 0.9 Python : 5000 N/m avant, 5e6 N/m après, 0 erreur de schéma) ; Fem3D / OpenSees en N
(166/240 faux écarts avant, 0 après ; test 499) ; TSA3D réactions en kN + bloc results.units (test 500, schéma mis à
jour). Références avant / après : TSA-rapports/2026-10-26/references/unites_echanges/. RESTE OPEN : P2 (affichages « kN »
en dur, diagramUnit), P3.

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

## BUG-110
Area: Sections — rayon de congé déduit du nom (src/Model/Section.h:46-49, Section.cpp:151-186)
Problem: le rayon de congé d'un profilé laminé (A, Iy, Iz, It) n'est pas enregistré : il est relu à chaque calcul dans une
table du code à partir du NOM de la section. Renommer la section (ou modifier la table) change les résultats d'un projet.
Reproduction: 2026-10-25, test 495 (première version) : section « IPE 300 » renommée → déplacements différents.
Impact: HIGH (exactitude, reproductibilité). Décision D1 (2026-10-26) : propriétés stockées, refus explicite plutôt que
recalcul ; r conservé quand une dimension change (avertissement, conversion proposée). Constat : les formules des sections
ont déjà changé sans version majeure (11f2087 en 1.8, 4457a16 en 1.10) → version conditionnelle non démontrable ;
recommandé : 2.0 inconditionnelle + masque de fonctionnalités + sectionModel / empreinte dans SECP
(TSA-rapports/2026-10-26/conceptions/BUG-110_presentation_D1.md).
Status: FIXED (2026-10-26, 2.0 approuvée) — Section::filletRadius (figé à l'entrée dans le modèle : constructeurs et
setSection), legacyRootRadius = algorithme historique figé ; format 2.0 : TSAFileHeader::requiredFeatures
(FEATURE_SECTION_PROPERTIES), chunk SECP obligatoire (r, empreinte A/Iy/Iz/It, sectionModel) ; refus explicites (fonction
inconnue, SECP absent, empreinte différente > 1e-9, formules inconnues) ; 1.x migrés à la lecture ; IFC FilletRadius et
TSA3D dimensions.r écrits et relus ; panneau de la barre : rayon conservé, conversion proposée, rayon éditable. Tests 506,
507, 508. Démonstration (ancien eb7e6db / nouveau) sur les 12 .tsa du dépôt : propriétés, rigidités, déplacements et
réactions OpenSees identiques au bit près (TSA-rapports/2026-10-26/preuves/BUG-110_migration/).
Related files: src/Model/Section.*, src/IO/TSAFile_BinaryUtils.h

## BUG-111
Area: Traçabilité des calculs (ResultsModel, OpenSeesSolver, NDC, NationalAnnexConfig)
Problem: version de moteur inventée par défaut (ResultsModel.h:197-198 « OpenSees 3.8.0 », OpenSeesSolver.cpp:421), aucune
empreinte de l'entrée, script Tcl détruit (QTemporaryDir), chunk RSLT déclaré mais jamais écrit, annexe nationale et γ/ψ non
figés dans le projet (singleton), NDC historique qui lit la version OpenSees du jour (NDCGenerator.cpp:127).
Impact: HIGH (traçabilité, NDC régénérée différente). Status: OPEN — étape 0 FAITE le 2026-10-26 (D2) : métadonnées
neuves vides (plus de « OpenSees 3.8.0 » / « France NF » par défaut), aucune version de repli, annexe non attribuée au
calcul, configuration réelle du script (BasicBuilder, RCM), horodatage UTC, messages de validation avant calcul conservés
(preparationMessages) ; NDC : moteur et version des résultats (« version non lue » sinon), plus « certifiées », annexe des
vérifications EC2/EC3 indiquée à part. Tests 503, 504 ; test Standards.8 adapté (il vérifiait les valeurs inventées).
RESTE : étapes 1-2 (empreintes, AnalysisTrace en mémoire), 3+ (format 1.14) après validation.
Related files: src/Analysis/ResultsModel.h, src/Analysis/OpenSeesSolver.cpp, src/NDC/NDCGenerator.cpp, src/Standards/NationalAnnexConfig.*

## BUG-112
Area: Annuler — ModelDiff incomplet (src/Model/ModelDiff.cpp:95-107)
Problem: isSlabDifferent ignore type, formulation, ouvertures et appuis de bord ; isWallDifferent a des lacunes similaires.
Impact: MEDIUM [H] : annuler une modification de ces champs ne rafraîchit pas la vue. Status: FIXED (2026-10-26) — ModelDiff compare type, formulation, ouvertures (sommet par sommet), appuis de bord, décalage du voile. Test 497 (reproduit avant correction ; dalle inchangée non signalée).
Related files: src/Model/ModelDiff.cpp

## BUG-113
Area: Notifications manquantes après mutation
Problem: MainWindow_Mesh.cpp:311-312 (ouvertures de dalle, sans notifySlabModified ; le voile notifie, :299) ;
LoadCaseDialog.cpp:216, 268, 279 (entrée Annuler créée, aucune notification après addLoadCase / addCombination /
removeCombination) ; OccView_Events.cpp:441 (pushUndoState sur un plan de travail, absent du snapshot : entrée vide).
Impact: MEDIUM (vues / invalidation des résultats). Status: FIXED (2026-10-26) — LoadCaseDialog : notifyLoadCaseChanged après ajout de cas, ajout / suppression de combinaison (test 498, reproduit avant correction) ; MainWindow_Mesh : ouvertures par Interaction::setSlabOpenings / setWallOpenings (une transaction, une notification ; test 502) ; OccView_Events : plus d'entrée Annuler vide pour le manipulateur de plan de travail (test 444 étendu, vrai glisser souris sous xcb : reproduit avant correction, 1 entrée et révision +1).
## BUG-114
Area: Paquet Windows — OpenSees (cmake/PackageTSA.cmake:236)
Problem: le paquet teste thirdparty/OpenSees/bin/OpenSees.exe, le dossier ne contient que gOpenSees.exe : OpenSees absent
du paquet, sans avertissement. Licence OpenSees : redistribution commerciale non autorisée sans accord (D3 du rapport).
Status: FIXED (2026-10-26) — paquet : OpenSees.exe ou gOpenSees.exe, avertissement si demandé et absent, TSA_PACKAGE_OPENSEES=OFF par défaut, verify_package.py accepte les deux noms (script exécuté sous Linux sur une arborescence factice : ancien script sans OpenSees, nouveau avec) ; ensureAvailable ne télécharge plus jamais (installation par l'action « Installer », avec accord) ; extraction PowerShell sans interpolation de chemins, outils système par chemin absolu (Windows : NON COMPILÉ NI TESTÉ). Reste : empreinte SHA-256 de l'archive téléchargée, texte de licence OpenSees.
## BUG-115
Area: Panneau Propriétés du poteau — « Tube Rectangulaire / Creux » (ColumnPropertiesView::getSectionFromUi)
Problem: choisir explicitement « Tube » produit un rectangle PLEIN nommé « Poteau Rectangulaire » (aucun champ
d'épaisseur). Depuis BUG-106, ce n'est plus déclenché par une autre modification, seulement par ce choix explicite.
Impact: MEDIUM. Status: OPEN.

## BUG-104
Area: Tests — i18n, test 457 (recherche, seuil 50 ms)
Problem: en Debug, la requête la plus lente dépasse le seuil (60 ms sur 6 000 entrées ; ~11 ms en Release) ; l'échec
arrête la suite, et les tests 458–463 ne s'exécutent pas (décompte Debug 482/490 au lieu de 489).
Reproduction: build-linux-debug, 2026-10-25, suite complète xcb.
Impact: LOW (performance réelle conforme en Release) ; masque toutefois 6 tests en Debug.
Status: OPEN — pistes : seuil selon NDEBUG, ou poursuivre la suite après cet échec.
Related files: tests/test_i18n.cpp, tests/test_main.cpp

## BUG-102
Area: OpenSees — chemin explicite de l'exécutable (src/Analysis/OpenSeesManager.cpp)
Problem: aucune option de l'interface n'écrit le chemin d'OpenSees : `setCustomExecutablePath` n'a aucun appelant ;
seule la clé QSettings `OpenSees/ExecutablePath` (organisation « TSA », application « TSA_StructuralAnalysis » ;
~/.config/TSA/TSA_StructuralAnalysis.conf sous Linux, registre sous Windows) permet de l'imposer, à la main.
Reproduction: Grep `setCustomExecutablePath` dans src/ (2026-10-25) : seule la définition.
Impact: LOW sous Windows (exécutable livré dans thirdparty/OpenSees), MEDIUM sous Linux (application lancée depuis le
bureau : OpenSees introuvable si ~/.profile n'ajoute pas son dossier au PATH).
Status: OPEN — comportement vérifié le 2026-10-25 (suite opensees) : chemin enregistré prioritaire, PATH, environnement
minimal (introuvable, message explicite). Ne pas proposer de lien symbolique (Tcl ne trouve plus lib/tcl8.6).
Related files: src/Analysis/OpenSeesManager.cpp, docs/LINUX_BUILD.md §7

## BUG-095
Area: Dépendances Linux (OpenSees, Gmsh, IfcOpenShell)
Problem: seules des versions Windows sont présentes dans thirdparty/ : OpenSees 3.8.0 (.exe, aucun binaire Linux
publié), SDK Gmsh Windows64 (intégration cmake/Gmsh.cmake réservée à MSVC), IfcOpenShell construit avec MSVC.
Sous Linux : moteur OpenSees indisponible, pas de maillage Gmsh, import IFC par le lecteur STEP interne.
Reproduction: build-linux-debug, 2026-10-24 : suites opensees, ndc, extraction, engines (136), mdd (158), displaymodes
(252), tsa3d (271), templates (284), torsion (321), demo (347), audit (369), benchmarks (367) → OpenSees ; mesh,
surfaces (354) → Gmsh ; ifc (400) → IfcOpenShell.
Impact: HIGH sous Linux (calcul impossible sans OpenSees), aucun sous Windows.
Status: FIXED (2026-10-25) — OpenSees 3.8.0 compilé (Conan 2, docs/LINUX_BUILD.md §7, ~/opt/opensees-3.8.0, PATH) ;
Gmsh : SDK Linux64 par cmake/Gmsh.cmake (§10, BUG-100, BUG-101) ; IfcOpenShell : scripts/linux/build-ifcopenshell.sh
(§11, BUG-103). Vérifié : suite complète Release (xcb) 489/490, 0 échec, test 128 ignoré (Windows seulement).
Related files: cmake/Gmsh.cmake, cmake/IfcOpenShell.cmake, src/Analysis/OpenSeesManager.cpp, docs/LINUX_BUILD.md

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

## BUG-077
Area: Import IFC / titre de la fenêtre
Problem: le projet issu d'un import IFC s'ouvre dans un nouvel onglet nommé d'après le fichier, mais la barre de titre
affiche « TSA — Sans titre * » tant qu'il n'est pas enregistré (`ProjectManager::windowTitle` ne connaît que le nom du
fichier).
Reproduction: Importer IFC… → ifc4x3_Building-Structural.ifc → onglet « ifc4x3_Building-Structural ● », titre « Sans titre ».
Impact: LOW (cosmétique ; l'onglet et l'arbre portent le bon nom)
Status: OPEN — constaté le 17/10/2026 (essai UI Automation de l'import IFC).
Related files: src/Project/ProjectManager.cpp, src/UI/MainWindow_Bim.cpp

## BUG-075
Area: Sélection 3D / nœuds
Problem: un clic simple sur un nœud dans la vue 3D ne le sélectionne pas (Propriétés vides, « 0 sél. »), avec ou
sans cotation ; la sélection par l'arbre fonctionne. Hypothèse non vérifiée : l'objet détecté en premier sous le
curseur est un axe de grille ou une étiquette (non enregistrés), d'où `clearSelection`.
Reproduction: qa_topo.tsa, vue 3D, clic sur le nœud N8 → infobulle « Nœud N8 », aucun nœud sélectionné.
Impact: MEDIUM (sélection des nœuds à la souris)
Status: OPEN — constaté le 2026-10-10 pendant la vérification des cotations ; piste : étendre
`OccView::detectedPreferringModel` à tout objet détecté non enregistré.
Related files: src/Viewer/OccView_Events.cpp, src/Viewer/OccView_Shapes.cpp

## BUG-074
Area: Document / état « modifié »
Problem: après Ctrl+S, le titre garde « * » et la fermeture redemande d'enregistrer tant que l'historique Annuler
n'est pas vide (`ProjectSession::hasUnsavedChanges` = `isModified() || canUndo()`).
Reproduction: modifier le modèle, Ctrl+S → « Enregistré : … » dans la barre d'état, mais titre « * » ; fermer →
« Enregistrer les modifications ? ».
Impact: LOW (confusion, demande d'enregistrement superflue ; aucune perte de données)
Status: OPEN — constaté le 2026-10-10 pendant la vérification de la topologie ; correction proposée : mémoriser la
profondeur de l'historique au dernier enregistrement.
Related files: src/Project/ProjectSession.cpp, src/UI/MainWindow.cpp

## BUG-002
Area: Calculation
Problem: Dalles et voiles ne sont pas transmis au calcul (`CalculationSnapshot` /
`OpenSeesAnalysisBuilder` ne traitent que barres, treillis, câbles, appuis) ; aucun maillage EF.
Reproduction: modèle avec dalle chargée → la dalle n'apparaît pas dans le script Tcl.
Impact: HIGH (limitation fonctionnelle à signaler à l'utilisateur)
Status: OPEN — mitigé le 2026-10-04 : avertissement `ModelValidator::validateForAnalysis`, confirmation
avant calcul (`confirmPlanarElementsExcluded`, MainWindow_Tools.cpp), et
`onActionMeshGen` n'affirme plus avoir généré un maillage (il ne fait qu'une estimation).
Related files: src/Analysis/CalculationSnapshot.cpp, src/Analysis/OpenSeesAnalysisBuilder.cpp

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

## BUG-013
Area: .tsa
Problem: les résultats de calcul (chunk `RSLT`) ne sont pas enregistrés : un projet rouvert doit être recalculé.
Les paramètres d'analyse, eux, sont persistés depuis le 2026-10-07 (chunk `SETT`, format 1.4, test 189).
Impact: LOW
Status: PARTIAL (paramètres FIXED, résultats OPEN)

## BUG-036
Area: Build (cache CMake local)
Problem: le 2026-10-08, `build-ninja-debug/CMakeCache.txt` avait `CMAKE_CXX_FLAGS` (et `_DEBUG`…) vides : compilation
sans /EHsc ni /Zi /Od (avertissements C4530) et test 96 en échec (destructeurs non appelés pendant le déroulement d'une
exception → pas de rollback automatique de l'EditTransaction). Cause exacte non identifiée (une configuration lancée
alors que vcvars64.bat signalait « vswhere.exe non reconnu » est suspecte).
Impact: MEDIUM (comportement des exceptions faux, sans erreur de compilation)
Status: WORKAROUND — `cmake --preset ninja-debug --fresh` rétablit les drapeaux (212/212 vérifié). Contrôle rapide :
`Select-String build-ninja-debug\CMakeCache.txt -Pattern "^CMAKE_CXX_FLAGS:"` doit contenir /EHsc.

## Corrigés (historique)

## BUG-105
Area: Vérifications EC2 / EC3 et note de calcul — unités du calcul (kN ou N)
Problem: les efforts étaient multipliés par 1000 (supposés en kN) sans lire ResultsModel::units() ; OpenSees, option « kN »
décochée, rend des N : ratios EC2/EC3 faux d'un facteur 1000. Même hypothèse dans tous les tableaux de la note (réactions,
efforts, σ, câbles N/F_pk, équilibre, extrêmes de ResultAnalyzer) et dans la note historique (présentation par défaut).
Status: FIXED (2026-10-25) — UnitSystem::forceToNewtons / forceToKiloNewtons ; ResultsDockWidget (3 sites), NDCGenerator,
NDCProjectNote (Ctx::toKN), ResultAnalyzer. Test 492 : même calcul en kN et en N → tous les tableaux des notes actuelle,
détaillée et historique identiques ; ratio EC2 = calcul direct en SI. Reproduit avant correction (test 492 en échec).
Chemin kN (défaut) : facteurs 1000 et 1 exacts, résultats bit à bit inchangés (banc 146/146, suite complète).
Related files: src/Analysis/AnalysisTypes.h, src/NDC/*, src/UI/Dock/ResultsDockWidget.cpp, tests/test_ndc_report.cpp

## BUG-106
Area: Panneaux Propriétés (barre, poteau, treillis, dalle, voile) — réécriture silencieuse
Problem: chaque validation (nom, couleur…) reconstruisait section, matériau, rotation, épaisseur depuis les champs : IPE 300
→ « Profilé I » sans congés (A, Iy, It modifiés), UPN → rectangle, poteau HEA → rectangle, treillis en cornière → rond plein,
matériau hors liste → matériau précédent de la liste, valeurs arrondies aux décimales affichées (hauteur de voile, rotation).
Status: FIXED (2026-10-25) — src/UI/Properties/EditedFields.h : seuls les champs modifiés par l'utilisateur sont réécrits ;
forme non représentable → champs de section désactivés ; matériau hors liste → aucun choix affiché. Test 493 (reproduit
avant correction). Non testé en interface réelle (essai GUI à faire).
Related files: src/UI/Properties/*PropertiesView.*, src/UI/Properties/EditedFields.h, tests/test_appearance.cpp

## BUG-107
Area: Ruban — Profilés I/H, Section rectangulaire, Section circulaire (MainWindow_Tools.cpp)
Problem: setSection + notify sans pushUndoState ni transaction : modification non annulable.
Status: FIXED (2026-10-25) — service TSA::Interaction::assignSection (src/Interaction/PropertyAssignment.*) : une entrée
Annuler, aucune si rien n'est sélectionné. Test 494 (service ; l'action du ruban ouvre une boîte modale, non automatisée).
Autres points d'entrée vérifiés annulables : panneaux, CommandRegistry, IA, types BIM.

## BUG-108
Area: Script OpenSees — injection Tcl par les noms (src/Analysis/OpenSeesAnalysisBuilder.cpp)
Problem: noms de nœud, section, matériau, cas, charges et libellés écrits bruts en commentaire Tcl : un saut de ligne
(fichier .tsa, import IFC) devenait une commande (exec), une barre oblique inverse finale absorbait la commande suivante,
une accolade déséquilibrait le bloc pattern.
Status: FIXED (2026-10-25) — tclComment() (une ligne, sans « \ », « { », « } ») sur les 7 écritures. Test 495 : aucune
commande injectée, accolades équilibrées, déplacements identiques au modèle aux noms ordinaires (OpenSees 3.8 Linux).
Reproduit avant correction (ligne « puts … » hors commentaire).

## BUG-109
Area: Paquets .tsalib non fiables (src/ExtensionSystem/ExtensionPackager.cpp)
Problem: identifiant du manifeste utilisé tel quel comme dossier d'installation (« ../x » : écriture hors du dossier des
extensions) ; décompression non plafonnée ; aucun contrôle de confinement du chemin final.
Status: FIXED (partiel, 2026-10-25) — identifiant validé (LibraryValidator::isValidId) à la création, à l'inspection et donc à
l'installation ; plafonds (10 000 fichiers, 256 Mio par fichier, 1 Gio au total) ; taille de l'en-tête zlib = taille
déclarée avant qUncompress ; chemin final confiné au dossier cible. Test 496 (reproduit avant correction). RESTE OUVERT :
extraction non atomique, masquage d'une extension officielle par une extension utilisateur de même id.

## BUG-103
Area: Import IFC sous Linux — IfcOpenShell et libgmsh (scripts/linux/build-ifcopenshell.sh)
Problem: libgmsh.so (SDK Gmsh, GCC 6) exporte ~879 instanciations std (et Boost) ; chargée avant les modules
IfcOpenShell (dlopen), elle leur fournissait ses copies : 32 symboles d'IfcOpenShell (GCC 13) liés à libgmsh, dont
std::_Rb_tree<pair<int,int>,…> → erreur de segmentation dans open_cascade_kernel::faceset_helper (thread de
process_concurrently) dès le premier fichier IFC (suite ifc, test 400).
Reproduction: 2026-10-25, gdb (pile dans std::_Rb_tree_insert_and_rebalance) puis LD_DEBUG=bindings.
Status: FIXED (2026-10-25) — IfcOpenShell lié avec -Wl,-Bsymbolic (CMAKE_SHARED_LINKER_FLAGS et
CMAKE_MODULE_LINKER_FLAGS du script) : 0 liaison ifcopenshell → libgmsh ; suite ifc 10/10 (Release, xcb). Les 3 liaisons
libstdc++ → libgmsh restantes (basic_stringbuf, ctype::do_narrow) relèvent de l'ABI stable de libstdc++.
Windows non concerné (pas d'interposition entre DLL).
Related files: scripts/linux/build-ifcopenshell.sh, cmake/IfcOpenShell.cmake, docs/LINUX_BUILD.md §11

## BUG-101
Area: Maillage Gmsh sous Linux — interposition des instanciations std (cmake/Gmsh.cmake, cmake/gmsh-hide-std.map)
Problem: le SDK Linux de Gmsh 4.15.2 est compilé avec GCC 6.5 ; les exécutables de TSA (GCC 13) exportaient ~185
instanciations std (std::regex _Executor/_Compiler/_Scanner, std::function, _Rb_tree…) que libgmsh référence, et le
chargeur ELF les substituait à celles de Gmsh (disposition mémoire différente : violation de l'ODR). Effet constaté :
corruption non déterministe, nom du groupe physique (2,2) « SL1 » effacé par l'ajout du groupe 3D (tests 293 : aller-retour
export/import 0/288 éléments ; 340 : 2 groupes physiques au lieu de 3) ; un simple gmsh::write en plus le masquait.
Reproduction: 2026-10-25, LD_DEBUG=bindings : 101 symboles de libgmsh liés à ./TSA_TestSuite ; programme autonome lié au
SDK avec les mêmes appels : pas de défaut.
Status: FIXED (2026-10-25) — script de version ld (motifs décorés _ZNSt*, _ZNKSt*, _ZSt*, typeinfo, vtable, VTT,
gardes) appliqué par INTERFACE_LINK_OPTIONS de TSA::Gmsh aux seuls exécutables, hors Windows : 0 fonction std exportée
(TSA, TSA_TestSuite, tsa-mesh, TSA_Benchmark). Vérifié Release : mesh 12/12, meshview 12/12 (340, 341 exécutés),
surfaces 12/12, benchmarks 2/2. Restent liés à l'exécutable : données copiées par relocation (vtables libstdc++,
typeinfo), sans risque. Note : dans un script de version, un motif extern "C++" entre guillemets est littéral (aucun joker).
Related files: cmake/Gmsh.cmake, cmake/gmsh-hide-std.map, docs/LINUX_BUILD.md §10

## BUG-100
Area: Maillage Gmsh sous Linux — gmsh.h_cwrap (src/Mesh/Gmsh/GmshMeshEngine.cpp)
Problem: gmsh.h_cwrap définit dans TSA des fonctions en ligne gmsh::… qui appellent l'API C ; libgmsh.so exporte son API
C++ native sous les mêmes noms décorés : l'API C de Gmsh appelait la copie de TSA (interposition ELF), qui rappelait
l'API C → récursion infinie (gmshModelMeshFieldSetNumbers ↔ gmsh::model::mesh::field::setNumbers), 13 Go de RAM et
TSA_TestSuite tué par le noyau (OOM) dès le test 289 (maillage 1D).
Reproduction: 2026-10-25, gdb sous ulimit -v : pile d'appels alternant les deux fonctions.
Status: FIXED (2026-10-25) — API C++ native (gmsh.h) hors Windows ; Windows (DLL MinGW + MSVC) garde gmsh.h_cwrap.
Vérifié : suite mesh 12/12. Windows : NON TESTÉ (inchangé par #ifdef _WIN32).
Related files: src/Mesh/Gmsh/GmshMeshEngine.cpp, cmake/Gmsh.cmake

## BUG-099
Area: OpenSees — vérification de l'exécutable (src/Analysis/OpenSeesManager.cpp)
Problem: l'exécutable était accepté si sa sortie contenait « OPENSEES_CHECK_OK » OU « OpenSees » ; la bannière contient
toujours « OpenSees ». Un binaire dont l'interpréteur Tcl ne démarre pas (init.tcl introuvable, « invalid command name
"wipe" », code de retour 0) était déclaré disponible ; detectVersion rendait « 3.8.0 » inventé si rien n'était lu.
Reproduction: 2026-10-24, OpenSees Linux sans lib/tcl8.6 à côté (aussi : lancé par un lien symbolique).
Status: FIXED (2026-10-25) — `OpenSeesManager::checkExecutable` / `interpretCheckOutput` : seul le marqueur
OPENSEES_CHECK_OK_<chiffres> prouve le fonctionnement (une recopie de l'entrée « …_%s » échoue), version ≥ 3.4.0
(plage d'OpenSeesAdapter) ; statuts NotFound, LaunchFailed (non démarré ou arrêt anormal), Timeout, TclInitFailed,
IncompatibleVersion, UnexpectedOutput, Operational, message traduit (en, mg) repris par OpenSeesEngine::availability et
ensureAvailable (hors Windows) ; detectVersion n'invente plus de version. OpenSees 3.8.0 n'a pas d'option de version
(sources : -noHeader, -suppressOutput seulement). Test 491 (sans installation : sorties réelles + faux exécutables
scriptés sous Unix). Vérifié Linux avec le vrai binaire : opérationnel, sans lib/tcl8.6 → TclInitFailed, chemin
enregistré (QSettings) et PATH. Windows : NON TESTÉ — gOpenSees.exe de thirdparty/ est un exécutable console qui contient
le même tclMain (même lecture de l'entrée standard), mais n'a pas pu être lancé ici.
Related files: src/Analysis/OpenSeesManager.cpp, src/Analysis/OpenSeesManager.h, src/Analysis/Engines/OpenSees/OpenSeesEngine.cpp, tests/test_opensees.cpp

## BUG-098
Area: Traductions (translations/*.ts, tools/i18n)
Problem: les .ts n'étaient plus à jour avant le 24/10/2026 : 127 textes du code absents (standardisation des dialogues,
Fem3D, opérateur IA…), alors que `check_translations.py` annonçait 100 % (il ne mesure que le contenu des .ts).
`update_translations.py` (-no-obsolete) aurait retiré 15 textes traduits dont la source a été reformulée.
Status: FIXED (2026-10-24) — lupdate sans -no-obsolete (128 ajoutés, 15 « vanished » gardés), puis 128 textes traduits
(en, mg ; glossaire docs/I18N_GLOSSARY.md, traductions des 15 anciens textes reprises pour leurs reformulations),
puis update_translations.py (-no-obsolete) retire les 15 et apply_translations.py écrit les traductions (0 refus).
Vérifié : check_translations --strict 6540/6540 (100 %) en et mg, 0 erreur ; i18n 15/15, ribbon 4/4 (Release).
Termes malgaches nouveaux non relus par un locuteur (comme le reste du glossaire) : solony (secours), sosona IA
(couche IA), mpandidy (opérateur, déjà en usage). Limite de l'outil : check_translations ne détecte pas des .ts en
retard sur le code ; lancer update_translations.py avant de s'y fier.
Related files: translations/tsa_en.ts, translations/tsa_mg.ts, tools/i18n/update_translations.py

## BUG-094
Area: Modules (sdk/modules, src/Modules/ModuleRegistry) — Linux
Problem: le manifeste `module.json` désigne un exécutable propre à Windows (`"command": "bin/sample_csv2tsa3d.exe"`) ;
sous Linux le convertisseur est construit sans extension, le module d'exemple est invalide.
Reproduction: `bash scripts/linux/test.sh debug --suite=modules` → Test 279 en échec (2026-10-24).
Impact: MEDIUM sous Linux (modules à convertisseur inutilisables), aucun sous Windows.
Status: FIXED (2026-10-24) — résolution du suffixe plutôt qu'une commande par plateforme (schéma tsa-module/1 et
manifestes inchangés) : `ConverterSpec::executable()` rend le chemin déclaré s'il existe, sinon « bin/x.exe » → « bin/x »
hors Windows, « bin/x » → « bin/x.exe » sous Windows ; utilisée pour la validation et le lancement. Vérifié Linux :
modules 6/6 (test 274 : variante de plateforme acceptée, chemin déclaré prioritaire ; test 279 : conversion CSV → TSA3D
réelle). Windows : NON TESTÉ. docs/SDK.md mis à jour.
Related files: sdk/modules/sample.csvimport/module.json, src/Modules/ModuleRegistry.cpp, tests/test_modules.cpp

## BUG-096
Area: Vue 3D — textures des matériaux (src/Viewer/MaterialVisual.cpp) — Linux
Problem: sans couleur choisie, la texture était chargée par OCCT depuis le fichier (WIC sous Windows, FreeImage
ailleurs) ; OCCT compilé sans FreeImage : « no image library available », texture absente.
Status: FIXED (2026-10-24) — hors Windows, décodage par Qt (RGBA, même convention de lignes que la texture neutre),
repli sur OCCT si Qt échoue ; Windows inchangé. Vérifié : message disparu de la suite complète (xcb).

## BUG-097
Area: Tests — hypothèses propres à Windows
Problem: Test 124 (une seule processEvents() pour une réponse différée par QTimer 0 ms : insuffisant avec le
répartiteur glib de X11), Test 387 (casse des chemins et lecture du registre par reg.exe).
Status: FIXED (2026-10-24) — attente bornée (2 s) ; 387 : chemin exact hors Windows, fichier INI de QSettings relu.
Vérifié Linux xcb : ai 24/24, multidoc 10/10. Windows : NON TESTÉ (branches #ifdef Q_OS_WIN inchangées).

## BUG-091
Area: Conversions numériques (toutes plateformes Unix)
Problem: sous Unix, le constructeur de QCoreApplication applique la locale du système (setlocale(LC_ALL, ""));
avec fr_FR, std::stod / strtod lisaient « 3.5 » → 3 et std::to_string écrivait « 0,500000 » (lecteur STEP/IFC,
résultats OpenSees, scripts de commandes, Blueprint, CommandRegistry, GridDefinition…). Valeurs silencieusement fausses.
Reproduction: Ubuntu 24.04, LANG=fr_FR.UTF-8 : Test 178 (axe de poutre IFC z=3 au lieu de 3.5), 198, 389 en échec ;
tous réussis avec LC_NUMERIC=C.
Status: FIXED (2026-10-24) — src/Core/NumericLocale.cpp : Q_COREAPP_STARTUP_FUNCTION rétablit LC_NUMERIC="C" (même
état que le CRT Windows). Vérifié sous fr_FR : blueprint 9/9, bim 13/13, Test 490 (nouveau, suite i18n).
Related files: src/Core/NumericLocale.cpp, tests/test_i18n.cpp

## BUG-092
Area: Tests (tests/test_main.cpp)
Problem: un `#ifdef _WIN32` posé pour la suite « thumbnail » (commit f79aef1) englobait toutes les suites ajoutées
ensuite (thumbnail → templates_wp, 25 suites) : hors Windows elles n'étaient ni compilées ni exécutées, en silence.
Status: FIXED (2026-10-24) — garde retirée ; seul le test 128 (DLL COM de l'Explorateur) est réservé à Windows, dans
tests/test_thumbnail_provider.cpp ([SKIP] ailleurs). Vérifié : les 25 suites s'exécutent sous Linux.

## BUG-093
Area: OpenSees (src/Analysis/OpenSeesManager.cpp, OpenSeesEngine.cpp) — Linux
Problem: hors Windows, OpenSeesManager retenait thirdparty/OpenSees/bin/gOpenSees.exe (execve : « Erreur de format »)
puis tentait le téléchargement automatique Windows (curl.exe, powershell.exe : « Impossible de lancer curl »).
Status: FIXED (2026-10-24) — candidats .exe et provisionnement réservés à Windows ; ailleurs, recherche de `OpenSees`
dans thirdparty/OpenSees/bin puis le PATH et message « installer OpenSees et le placer dans le PATH ». Vérifié :
suites opensees et torsion (message affiché, plus d'exécution du binaire Windows). Chaîne traduite à ajouter (lupdate).


| ID | Problème | Correction | Preuve |
| :--- | :--- | :--- | :--- |
| BUG-090 | Barre d'état à 1 920 px : l'indicateur du plan de travail était retiré faute de place avant le logo | Ordre de retrait de `fitStatusBar` : logo avant l'indicateur du plan | essai workplane_scenario.ps1 (« Plan: XY (Z = 0.00 m) » lu à 1 920 px) |
| BUG-089 | Plan déplacé (commande, navigation, manipulateur) : nom citant l'ancienne cote conservé (« RDC (Z=0.00) » à 15,93 m), dans la console et l'historique | `OccView::withCurrentPositionName` : nom positionnel périmé remplacé (niveau du modèle, référence de la grille, « Z = … m ») ; nom libre conservé | test 449 ; essai workplane_scenario.ps1 (« Z = 15.93 m » après le glisser) |
| BUG-088 | Console : une ligne « Plan de travail actif » par mouvement de souris pendant un glisser du manipulateur | `MainWindow::onWorkPlaneChanged` n'écrit pas pendant `isManipulatingWorkPlane()` ; la position finale est écrite au relâchement | essai workplane_scenario.ps1 (une ligne par glisser) |
| BUG-087 | Après l'ouverture d'un projet, un clic sur le panneau du plan de travail ne le sélectionnait pas (manipulateur 3D inaccessible) : `rebuildAllShapes` vidait le registre de sélection, panneau compris, sans le réenregistrer | Panneau réenregistré après `clearRegistry` | test 448 (échoue sans la correction) ; essai workplane_scenario.ps1 (clic → gizmo) |
| BUG-086 | Plan d'étage déplacé en X ou Y (commande ou manipulateur) ramené à l'origine globale ; une entrée « Z = … m » ajoutée au sélecteur de niveau à chaque mouvement : l'écho de la fenêtre vers le sélecteur réappliquait `setWorkPlaneAxisAndOffset` | `ViewportContainer::reflectActivePlane` (sélecteur et règles seulement) ; entrée personnalisée unique, réutilisée | test 447 (échoue sans la correction : origine (0 ; 0 ; 3)) ; essai workplane_scenario.ps1 (`WPMOVE 2 0 0` → (2 ; 0 ; 3)) |
| BUG-085 | Échap pendant un glisser du manipulateur : le plan restait déplacé (seul le panneau revenait) ; dans l'application, la touche était sans effet (`setInteractionMode(Select)` sortait aussitôt, mode déjà actif) et le relâchement validait le glisser | Plan et grille rétablis ; annulation aussi quand le mode Sélection est déjà actif | test 444 (touche reçue par la vue et raccourci « Mode sélection » ; échoue sans la correction) ; essai workplane_scenario.ps1 |
| BUG-084 | Translation du plan au manipulateur : un plan XY devenait « personnalisé » (`setLocalAxes` impose `Custom`), perdant décalage et niveau | Axes mis à jour seulement si l'orientation change | test 444 (type XY, décalage = altitude) |
| BUG-083 | Grille fantôme : plan de travail déplacé ou tourné au manipulateur 3D, grille (et niveau actif) restés sur l'ancien plan | Chemin unique `OccView::syncWorkPlaneDependents` (grille, trièdre, plan privilégié, service de coordonnées, niveau, isolation, redessin), emprunté à chaque mouvement du glisser | test 444 (échoue avec l'ancien code du glisser : plan à Z = 1,29 m, grille restée à 0) ; captures de l'essai workplane_scenario.ps1 |
| BUG-082 | Caméra capturée (aperçu des projets récents, vues nommées) avec l'œil sur la cible : en orthographique, OCCT peut ramener l'œil sur la cible (distance nulle) en gardant la direction à part ; la restauration refusait alors la caméra | Œil recalculé depuis la direction (`captureNamedView`, `cameraState`) | test 441 (vue d'élévation capturée puis restaurée exactement) |
| BUG-081 | Panneau (et grille) du plan de travail toujours de 20 m × 20 m autour de l'origine du plan : la taille par défaut, explicite, empêchait le calcul « dynamique » de jamais s'exécuter ; un bâtiment loin de l'origine n'était couvert qu'en partie | Taille du plan = minimum ; agrandi et recentré sur l'emprise du modèle projeté dans le plan | test 437 (grille couvrant les 380 nœuds du projet R+5 dans les 6 vues) ; capture de l'essai views_scenario.ps1 |
| BUG-080 | Dialogue des réglages de grille : la case « Magnétisme » modifiait `GridSnapManager`, que le calcul d'accrochage ne lit pas : sans effet réel | La case pilote `OccView::setGridSnapEnabled` (état réel, signal, ruban, barre d'état) | revue de code ; état réel vérifié par l'indicateur SNAP (essai views_scenario.ps1) |
| BUG-078 | Indicateur « SNAP: ACTIF » de la barre d'état jamais mis à jour (créé une fois) | `MainWindow::updateSnapStatus` lit l'état réel de la vue (`isGridSnapEnabled`, `isObjectSnapEnabled`) à chaque signal de changement : ACTIF, GRILLE, OBJETS ou INACTIF | essai views_scenario.ps1 : « SNAP: ACTIF | SNAP: OBJETS | SNAP: INACTIF | SNAP: GRILLE » selon le magnétisme et l'OSNAP |
| BUG-073 | Étiquette 3D d'un nœud : affichait toujours « N<identifiant interne> » et jamais son nom (la condition d'ajout du nom était toujours fausse) ; titres du panneau Propriétés idem | Étiquette 3D = `formattedName()` ; titres « <étiquette> (id N) » | GUI : renumérotation par grille → A1-1, A2-2 affichés dans la vue |
| BUG-072 | Molette dans le panneau Propriétés : en faisant défiler, la liste ou le champ numérique survolé changeait de valeur (un appui passait de « Libre » à « Appui simple ») | `PropertyPanel::eventFilter` : molette sur une liste / un champ numérique sans le focus → défilement du panneau ; un champ cliqué garde la molette | test 224 ; GUI : défilement sur les champs d'appui, valeurs inchangées |
| BUG-071 | Arbre du modèle : liste « Appuis » relue seulement au chargement (appui posé, retiré, nœud supprimé, Annuler non reflétés) | `ModelTreeWidget::scheduleSupportsRefresh` (nœud ajouté appuyé, modifié, supprimé ; une reconstruction par rafale, sans signal de sélection) | test 223 ; GUI : Encastrement → « Appuis [1] », Ctrl+Z → « [0] » |
| BUG-070 | Plusieurs nœuds sélectionnés, appui choisi dans les propriétés : seul le nœud affiché le recevait. La vue recréait la forme du nœud modifié et le resélectionnait (`selectNode`), ce qui rouvrait le panneau sur ce seul nœud et interrompait l'édition groupée avant le report ; même défaut pour barres, poteaux, câbles, treillis, dalles, voiles, fondations | `SelectionManager::restoreSelected` (remise en sélection sans signal) dans les huit `OccView::update…Shape` ; surbrillance ajoutée sans effacer celle des autres éléments | test 222 ; GUI : 2 nœuds sélectionnés, « Encastrement » → édition groupée maintenue |
| BUG-069 | « Étirer » (fenêtre Éléments filaires) sans effet pour les poutres et poteaux : les modes Poutre / Poteau créaient l'élément puis réinitialisaient le tracé sans lire la case | `OccView::finishBarSegment` : l'extrémité devient le départ suivant si « Étirer » est coché (`setBarChainMode`, relié à la case) ; champ Origine de la fenêtre mis à jour | GUI : 3 clics → 2 poutres reliées par un nœud commun ; case décochée → nouveau départ |
| BUG-068 | Valider le panneau Propriétés (barre, poteau, câble, treillis, dalle, voile, fondation) pour n'importe quel réglage enregistrait une couleur par défaut du type d'élément (poutre bleue, poteau orange…), qui masquait définitivement l'apparence du matériau | Couleur vide = apparence du matériau ; le panneau n'enregistre une couleur que si l'utilisateur en choisit une ; bouton « Matériau » pour y revenir (`ElementColorField.h`) | test 221 |
| BUG-067 | Après calcul, la déformée était peinte en cyan uniforme (câbles orange), quel que soit le matériau ; les textures de matériau (Extensions/TSALib/Textures) n'étaient jamais affichées (formes AIS_Shape sans aspect texturé) | `MaterialVisual::applyToShape` : texture native d'AIS_Shape par matériau (mode Matériaux), une ressource graphique par fichier, répétition ≈ 1 motif / m ; couleur utilisateur = grain neutre teinté ; `ResultsVisualManager` applique la même apparence à la déformée | test 220 ; GUI : portique béton / acier texturé, déformée après calcul aux couleurs et textures des matériaux |
| BUG-066 | Aperçu du projet (miniature .tsa dans l'Explorateur, Start Center) à l'envers : l'image lue dans le viewport OCCT (lignes rangées du bas vers le haut) était copiée sans retournement | `OccView::captureViewImage` retourne l'image verticalement si `Image_PixMap::IsTopDown()` est faux | GUI : projet enregistré depuis TSA, PNG d'aperçu extrait du .tsa : vue à l'endroit (poteaux posés sur le plancher, repères lisibles) |
| BUG-065 | Une charge sélectionnée dans la vue 3D (flèches) ne pouvait pas être supprimée : Suppr ne transmettait que nœuds et éléments à la suppression, aucune fonction ne retirait une charge | `TSA::Model::removeLoads` (MemberLoadCommands : charges nodales et sur barres, une entrée Annuler, notifications) appelée par `MainWindow::onActionDeleteSelected` | test 219 ; GUI : clic sur une charge répartie, Suppr → charge retirée, poutre conservée ; Ctrl+Z → charge rétablie |
| BUG-064 | Bouton « Aide » de la fenêtre Grille créé mais jamais relié (clic sans effet) ; aides des fenêtres limitées à des textes locaux, sans lien vers une documentation | Registre d'aide `src/Help/HelpTopics` (identifiants stables → pages de TSA Web, HTTPS et domaine officiel seulement, repli sur /docs) et `src/UI/HelpLauncher` (QDesktopServices, message non bloquant avec adresse copiable) ; boutons Aide reliés (barres, câbles, surfaces, grille, charges sur barre, charges nodales, cas de charge, analyse), « Documentation en ligne » et « Signaler un problème » dans le menu Aide et le ruban | tests 214-218 (suite `help`) ; `npm run check:docs` (Tsaraloha-Web) : 33 identifiants, 0 erreur |
| BUG-063 | Ctrl + clic : la surbrillance ne gardait que le dernier élément (le synchroniseur appelait highlightX, qui efface tout), et un Ctrl + clic sur un élément sélectionné le retirait à l'écran (XOR OCCT) mais l'ajoutait à la sélection du modèle : commandes et propriétés agissaient sur un ensemble différent de l'affichage. Ctrl + rectangle : même divergence ; charges jamais retirées ; aucune édition groupée après un rectangle ; éléments supprimés (Annuler, cascade) laissés dans la sélection | `SelectionManager::toggleObject / toggleElement` (bascule, élément principal = dernier ajouté), `setMultipleObjectsSelected` et `selectElements` sur des règles communes (0 → vidée, 1 → clic simple, plusieurs → surbrillance d'ensemble + édition groupée), rectangle : la sélection reprend le résultat du contexte, `pruneMissing` différé après chaque modification du modèle, surbrillance et propriétés des charges en sélection multiple | test 213 ; GUI 2026-10-10 : B1 + Ctrl B2 (deux surlignées), Ctrl B2 (retirée seule), Ctrl poteau (ajouté), Ctrl vide (conservée), zoom (conservée), clic vide (vidée), deux poutres → « ÉDITION GROUPÉE : 2 POUTRES », Suppr → 2 éléments supprimés |
| BUG-059 | Charge uniforme limitée à un intervalle [x1, x2] transmise sur toute la barre (OpenSees `beamUniform`, Custom2D a = 0, b = L) : 10 kN/m sur [1, 3] m donnait 60 kN au lieu de 20 kN | `MemberLoad::appliedRange` (règle commune) ; OpenSees : charge linéaire d'intensités égales sur [a, b] ; Custom2D : [a, b] ; résultante d'équilibre sur [a, b] | test 212 (60 kN sur le code d'origine) |
| BUG-060 | Sens d'une charge sur barre défini trois fois (résolveur chemin modèle, résolveur chemin calcul, Custom2D) plus une fois dans le rendu, avec des écarts (−q contre −\|q\| pour une direction non locale en repère local ; rendu d'un axe local en repère global alors que le calcul applique la gravité) | `LoadResolver::memberLoadVector / memberLoadLocalComponents / usesMagnitudeOnly`, seule définition, utilisée par le calcul, Custom2D et le rendu ; valeurs de calcul inchangées | test 210 ; 229 tests antérieurs inchangés |
| BUG-061 | Rendu des charges sur barre : 5 flèches fixes de même longueur, q2 et l'intervalle ignorés (pas de forme triangulaire / trapézoïdale), force ponctuelle et moment réparti dessinés comme une charge répartie, étiquette limitée à q1 | `TSA::Geometry::buildMemberLoadGlyph` (flèches régulières en nombre adapté, longueur ∝ intensité locale, sens du résolveur, enveloppe, intervalle) ; un seul objet filaire par charge ; taille liée au zoom par paliers | test 209 ; GUI 2026-10-10 : uniforme, triangulaire 0 → 15, Gravité négative, zoom avant / arrière |
| BUG-062 | Fenêtre et panneau des charges sur barre : aucune validation (intervalle inversé ou hors barre, valeurs non finies), repère et direction modifiables séparément (axe local en repère global = gravité pour les moteurs), double application silencieuse | `MemberLoad::validate`, `coordSystemFor`, `MemberLoadCommands` (tout ou rien, une entrée Annuler, doublons) ; panneau : copie validée avant modification | test 211 ; GUI : doublon confirmé, intervalle 5 → 2 m refusé |
| BUG-058 | Crash (violation d'accès) : « Sélectionner 3D » de la charge nodale, fenêtre fermée (modale, la vue était bloquée), puis clic dans la vue → rappel de sélection sur la fenêtre détruite (`NodalLoadDialog::setTargetNodeId`). Même défaut latent dans le sélecteur de point de « Nouveau nœud » | la requête porte son vrai demandeur (`OccView::pickPoint3D`, paramètre owner) ; le demandeur l'annule dans son destructeur (`InteractionManager::cancelSelectionRequestFrom`, signaux bloqués pour le sélecteur) ; vue gardée en `QPointer` ; charge nodale et nouveau nœud non modales (le clic 3D a besoin de la vue), une seule instance, fermées au changement de projet | minidump crash_dump_20261009_230928.dmp ; test 208 ; GUI 2026-10-10 : clic 3D → N8 choisi ; « Sélectionner 3D » + Fermer + clic dans la vue → aucun crash, pas de nouveau rapport |
| BUG-057 | Panneau d'une barre : appui posé sur un de ses nœuds invisible (seulement « N1 → N2 »), et vue de la barre non rafraîchie quand un de ses nœuds changeait | bloc `MemberEndNodesWidget` « Nœuds d'extrémité » (poutre, poteau, treillis) : numéro, coordonnées et appui de chaque nœud lus dans le modèle ; appui modifiable depuis la barre (propriété du nœud, une entrée Annuler) ; bouton vers les propriétés complètes du nœud ; `PropertyPanel::onNodeModified` rafraîchit la barre affichée dont un nœud change | GUI 2026-10-09 : poutre B3 tracée, N9 encastré depuis le ruban → B3 affiche « Début : N9 Encastrement » ; N4 mis en articulation depuis B3 → B1 (même nœud) affiche « Fin : N4 Articulation » ; Ctrl+Z → Libre ; bouton ↗ → propriétés de N4 |
| BUG-056 | Onglet Modifier : un bouton de commande (Déplacement 3D, Copie 3D, Rotation 3D, Déplacer l'origine…) restait coloré après l'annulation ou la fin de sa commande ; « Sélection » restait coloré pendant une autre commande ; les outils avancés, Symétrie, Diviser, Fusionner ne se coloraient jamais ; le libellé sur deux lignes d'un grand bouton était perdu à chaque changement d'état de son action | `MainWindow::syncToolActionStates` : bouton qui a lancé l'outil coloré tant que la commande est active (vue ou fenêtre de paramètres), boutons de mode = mode réel de la vue (groupe exclusif facultatif, resynchronisé après clic), appelé à chaque changement de mode et à chaque issue de `startModelingTool` ; `RibbonButton::actionEvent` rétablit le libellé du ruban | GUI 2026-10-09 : Origine, Déplacement 3D (refus sans sélection puis avec sélection), outil avancé, Rotation 3D : colorés pendant la commande, décolorés après Échap ; ruban identique au pixel près avant / après |
| BUG-055 | Après Échap (retour au mode Sélection), une fenêtre de tracé restée ouverte (barre / poutre / poteau, câble, dalle / voile) ne pilotait plus rien : il fallait la fermer et la rouvrir pour reprendre le tracé | chaque fenêtre émet `drawingResumeRequested` quand elle redevient active ; `MainWindow::activateBarDrawing / activateCableDrawing / activateSurfaceDrawing` (factorisés avec l'ouverture) réactivent le mode et le bouton du ruban, avec le type choisi dans la fenêtre ; sans effet si le mode est déjà actif (tracé en cours conservé) | GUI 2026-10-09 : poutre → Échap → « Mode Sélection actif » → clic sur la fenêtre → « Mode Dessin Poutre », poutre N9–N4 créée ; dalle reprise ; passage de la fenêtre dalle à la fenêtre barre bascule sur Dessiner Poutre |
| BUG-054 | Ruban : à chaque passage sur un onglet (Modèle, Modifier, Affichage…), les groupes se repliaient ou se dépliaient et les icônes changeaient de place, à largeur de fenêtre identique | largeurs mesurées sur des boutons pas encore polis par leur feuille de style et à travers des tailles mises en cache par Qt, rafraîchies au fil des affichages ; `RibbonPanel::measureWidths` (polissage explicite, caches vidés, mesure unique par mode, invalidée au changement de thème / police), `RibbonTab::relayout(largeur)` fonction de la seule largeur, largeur de la pile d'onglets à l'affichage, contrôle unique après le premier affichage | GUI 2026-10-09 : avant, Modèle différait de 11 301 puis 5 725 pixels entre visites ; après, 0 pixel sur 3 visites des 9 onglets (échelle 1,0 à 1 220 et 1 900 px ; échelle 1,5 avec Accueil compacté). Pas de test unitaire : le ruban est dans la couche application, hors TSA_Tests |
| BUG-049 | Annulation d'un calcul OpenSees perdue si demandée avant que le moteur ait créé son solveur, ou avant que `solveSnapshot` remette son drapeau d'arrêt à zéro : le calcul allait à son terme | drapeau partagé `AnalysisRunCallbacks::cancelRequested` armé par `AnalysisController::cancel` puis lu par `OpenSeesEngine::run` après création du solveur ; `solveSnapshot` ne réarme le drapeau qu'en fin de calcul ; `m_isRunning` atomique | test 205 (compile seulement avec la correction : l'API d'annulation n'existait pas) |
| BUG-050 | Résultats calculés sur un état antérieur publiés « à jour » : le garde de validité prenait la révision du modèle à la publication, pas au lancement (modèle modifié depuis le panneau d'analyse non modal, ou projet ouvert / créé pendant le calcul depuis le Start Center ou un glisser-déposer) | `AnalysisController` mémorise la révision au lancement, `ResultsValidityGuard::trackResults(results, révision)` invalide aussitôt ; ouvrir / créer / glisser un projet refusé pendant un calcul (`MainWindow::confirmNoRunningAnalysis`, message commun avec quitter / fermer) | test 206 (échec reproduit sur le code d'origine) ; GUI 2026-10-09 : Ctrl+O et icône Ouvrir sans effet pendant un calcul lancé par F5 (fenêtre de progression modale) — la garde n'est donc atteignable que par un appel direct ; annulation (portique 5 006 barres, non linéaire 200 pas) : « Calcul OpenSees annulé » en barre d'état, aucune boîte d'erreur, aucun résultat publié, 2 essais |
| BUG-051 | JSON des grilles (chunk GRID, historique Annuler) écrit à la main : nom tronqué à la première virgule (« Bâtiment A, aile Est » relu « Bâtiment A »), JSON invalide sur un guillemet, réels à 6 chiffres significatifs (origine 500 123,456 m relue 500 123 m) | `GridDefinition::toJsonObject / fromJsonObject` et `GridManager::serializeToJson` par QJsonDocument (chaînes échappées, réels relus à l'identique) ; ancien analyseur conservé en secours pour un JSON invalide écrit par les versions précédentes | test 207 (échec reproduit sur le code d'origine : nom relu « Bâtiment A ») ; ancien format valide et invalide relus |
| BUG-052 | Note de calcul : bois détecté (`hasTimber`) mais EN 1995-1-1 (Eurocode 5) jamais cité ni mis en bibliographie | entrée EN 1995-1-1 + référence [CEN-EN1995] | test NDC (structure en C24) ; signalé par clang-analyzer (valeur jamais lue) |
| BUG-053 | Règles graduées du viewport : boucle `for (double v = …; v += step)` sans fin si les cotes sont très grandes devant le pas (v + step == v) | indice entier borné (`tickIndexCount`), mêmes graduations | code (clang-tidy bugprone-float-loop-counter) ; GUI 2026-10-09 : règles horizontale et verticale graduées normalement (−9 à 8 m, 1 à 11 m) |
| BUG-035 | Test 20 : seuil de 350 ms sur une mesure unique, dépassé une fois sous charge | seuil évalué sur le meilleur de 3 annulations (seuil inchangé) | suite `commands` : meilleur ≈ 175 ms en Debug |
| BUG-044 | Lettres / chiffres de grille répétés à chaque niveau et sur toutes les élévations | `GridLabelLayout` (repères choisis selon la visée, une occurrence par axe), `OccView::updateGridLabelView` | test 202 ; GUI 2026-10-09 : face (1-4 au-dessus du dernier niveau), dessus (1-4 et A-D une fois), isométrie (niveau bas seul) |
| BUG-045 | Déformée en empilement de prismes ; flèche en travée ignorée (Hermite nodal seul) | `DeformedGeometry::computeMemberAxis` (stations du solveur) + `createMemberSolid` (maillage continu) | tests 203 (OpenSees : 2 appuis, console, poteau, portique, cas limites), 204 (Custom2D) ; GUI : portique courbé continu |
| BUG-046 | « Déformée seule » laissait la structure d'origine opaque (indiscernable de « Les deux ») | `OccView::setResultsStructureDisplay` (atténuée / masquée) | GUI : structure initiale atténuée sous la déformée ; mode « déformée seule » vérifié par le code seulement |
| BUG-047 | Facteur d'échelle manuel écrasé à chaque calcul | auto seulement si préréglage Auto | code (setResultsModel) ; non vérifié en GUI |
| BUG-048 | Échelle auto nulle pour une poutre sur deux appuis (nœuds immobiles) | maximum sur les axes déformés (stations) | code ; test 203 couvre les stations |
| BUG-040 | Projet sous un chemin non ASCII (« Études/Pont à poutres.tsa ») enregistré mais impossible à rouvrir : `std::ifstream` reçoit de l'UTF-8 interprété en ANSI ; idem résultats OpenSees si %TEMP% contient un accent | `TSA::Core::utf8Path` (src/Core/Utf8Path.h) dans TSAFile.cpp, OpenSeesResultsReader, OpenSeesAdapter | test 201 (échec reproduit sur le code d'origine : « Fichier introuvable ») |
| BUG-041 | Chaîne ≥ 64 Kio écrite avec une longueur 0 (cast u16 de 65 536) : JSON COOR / GRID perdu | borne 65 535 (frontière UTF-8) ; `writeChunkText` / `readChunkText` (préfixe 0 + JSON jusqu'à la fin du chunk) | test 201 (grille de 6 000 axes ; perdue avant correction) |
| BUG-042 | Nom / auteur du projet enregistrés vides par `ProjectManager::saveProject` | transmis à `TSAProjectIO::saveProject` | test 201 (nom relu vide avant correction) |
| BUG-043 | Ouverture refusée : niveaux / grilles du fichier rejeté déjà appliqués au projet resté ouvert | COOR / GRID appliqués après lecture complète (`TSAFileReader::parsePayload`) | test 201 (fichier GRID valide + NODE tronqué) |
| BUG-039 | TSALab : assertion Qt à la fermeture (« Called object is not of the correct type ») si des nœuds Blueprint étaient sélectionnés : la scène, détruite après l'éditeur, émettait selectionChanged vers BlueprintEditor::showNodeProperties | ~BlueprintEditor et ~LabMainWindow coupent les connexions de leurs enfants vers eux (même règle que ~MainWindow) | test 199 (erreur de segmentation avant correction) ; GUI 2026-10-09 : exemple portique, 5 nœuds sélectionnés, fermeture normale sans assertion |
| BUG-038 | Résultats Custom2D : équilibre global vide (réaction totale 0 kN affichée, « Équilibre : CONFORME » sans contrôle) et cas calculé sans nom | réactions sommées, résultante des charges = −ΣR au résidu du solveur près, moments déclarés non contrôlés ; `caseOrComboName` renseigné | GUI TSALab 2026-10-08 : portique plan, réaction totale 77,9 kN, SOLVER LAB « cas Tous les cas » |
| BUG-037 | MetDeDeplacement refusait une ossature sans DDL libre | neq == 0 accepté : U = 0, efforts d'encastrement parfait (TSALab/science) | benchmark « fixed-fixed-single-bar » (7/7, 2026-10-08) |
| BUG-001 | Calcul OpenSees bloquant (thread UI gelé, arrêt impossible) | calcul dans un `QThread::create`, `QProgressDialog` annulable (`AnalysisManager::cancel`), `std::atomic` d'arrêt dans `OpenSeesSolver`, mutex dans `OpenSeesEngine`, fermeture du projet refusée pendant un calcul | compilation + 212/212 ; **vérification GUI à faire** (progression, Annuler) |
| BUG-003 | Niveaux / axes / grilles hors historique Annuler ; Annuler après élévation de niveau rétablissait les nœuds sans le niveau | `ModelStateSnapshot::coordinatesJson / gridsJson` (CoordinateSystem + GridManager rattaché par `Model::setGridManager`), restauration seulement si différent, état d'affichage des grilles conservé ; fenêtre Niveaux = une `EditTransaction` (`discardUnchanged` sans changement) ; `pushUndoState` avant création / modification / duplication / suppression de grille. Plans de travail : état d'affichage, volontairement non annulés | test 188 |
| BUG-004 / BUG-009 | Commandes `cmd.isolate.*` non branchées ; module `src/View3D/Isolation` jamais compilé | menu Affichage ▸ Isolation 3D (I, Alt+I, Alt+W, H, inverser, Ctrl+H, Alt+H) dans la passe unique `OccView::updateElementIsolation` ; module mort, `tests/isolation` supprimés ; section / projection / volume / estomper retirés du catalogue (projection était en conflit avec Ctrl+Shift+I) | compilation, `check_shortcuts --strict` ; **vérification GUI à faire** |
| BUG-005 | Pas d'édition multi-objets | `TSA::Model::MultiEditSession` (seuls les champs modifiés de l'élément principal sont reportés ; nom, nœuds, coordonnées jamais) ; `PropertyPanel::setMultiSelection`, titre « ÉDITION GROUPÉE » ; nœuds (appuis), poutres, poteaux, treillis, dalles, voiles, fondations ; câbles exclus | test 191 (report, une entrée Annuler) ; **panneau à vérifier en GUI** |
| BUG-007 | Recherche linéaire d'item dans l'arbre du modèle | index id → item par catégorie (`ModelTreeWidget::m_itemIndex`) ; suppression groupée par l'index ; câbles modifiés sous la garde `m_model` | test 166 (2 000 poutres, 1 500 suppressions ≈ 60 ms) |
| BUG-010 | C4996 `TColgp_HArray1OfPnt` | `NCollection_HArray1<gp_Pnt>` | compilation sans l'avertissement |
| BUG-011 | Aucune dépendance d'en-tête Ninja (MSVC francisé) | lanceur **généré** `build/msvc_codepage.cmd` : la page de code active à la génération (celle dans laquelle CMake écrit le préfixe de rules.ninja) est imposée à chaque `cl`. Le lanceur fixe UTF-8 précédent cassait dès qu'on reconfigurait en CP850 (constaté le 2026-10-06 : objets périmés → segfault du test 18) | rules.ninja `FF` + `chcp 850`, ou `C2 A0` + `chcp 65001` ; `ninja -t deps` Model.cpp.obj = 51 en-têtes après recompilation complète |
| BUG-016 | Stations intermédiaires approchées | équilibre exact du tronçon [0, x] à partir des efforts en i et des charges ; déformée par double intégration de la courbure ; convention RDM | tests 181–184, test NDC |
| BUG-017 | Équilibre contrôlé en forces seulement | `GlobalEquilibrium` : moments appliqués / réactions, `relativeMomentResidual` | test 110 étendu, 182 |
| BUG-018 | Liens NDC `tsa://element` sans famille | `kind=` dans le lien, `ExtremumPoint::elementKind`, sélection + cadrage | compilation ; **clic dans la NDC à vérifier en GUI** |
| BUG-019 | Nœuds reliés seulement à des treillis : rotations sans rigidité | rotations bloquées (sauf ressort sur ce DDL) | test 184 (trépied) |
| BUG-020 | Boutons Qt en anglais | `qtbase_fr.qm` déployé dans `translations/`, `QTranslator` installé | fichier présent dans build-ninja-debug/translations ; **affichage à vérifier en GUI** |
| BUG-024 | Pushover / temporel calculés comme du statique | supprimés avec tout le dynamique (ADR-022) | — |
| BUG-026 | Modes Move3D / Copy3D / Rotate3D inatteignables | supprimés (InteractionManager, OccView, MainWindow) | compilation, test 26 (MoveOrigin3D) |
| BUG-027 | Relâchements d'extrémité non transmis à OpenSees | `-releasey` / `-releasez` ; avertissement pour N / V / T non transmis | test 183 (rotule) |
| BUG-029 | Collage sans métadonnées BIM | `StructuralClipboard` mémorise les produits physiques ; `BimModel::registerPasted` (registerCopies factorisé) | test 190 |
| BUG-030 | Menu Structure ▸ Conditions d'Appuis : Encastrement / Articulation / Appui simple n'assignaient rien | `MainWindow::assignSupport` (pushUndoState + setSupport + notifyNodeModified) | GUI 2026-10-06 : appui assigné, dock « Appuis : 1 », calcul OpenSees OK (δ_max 28,63 mm, équilibre conforme) |
| BUG-031 | Suppression de 6 768 éléments ≈ 6 min (Debug) : un redessin complet du viewport et une recherche linéaire dans l'arbre par élément | `OccView::scheduleRedraw` (un redessin par rafale), `ModelTreeWidget::queueRemoval/flushRemovals` (une passe par catégorie) | GUI stress_4900 : suppression 17 s, Rétablir ≈ 21 s, Annuler ≈ 17 s (≈ 30 s avant), modèle restauré sans doublon |
| BUG-032 | Barre d'état : libellés superposés à toute largeur | cause : politique `Ignored` → case de largeur nulle dans QStatusBar ; largeurs fixes + `fitStatusBar` (masquage par priorité, mise en page forcée car QStatusBar ignore LayoutRequest) ; page cachée sans taille minimale | captures à 3 largeurs, plus de chevauchement |
| BUG-034 | Dock Résultats : « Nœuds : 0 » après ouverture | `updateNodeStats` public, appelé à l'ouverture et à chaque révision du modèle | GUI : « Nœuds : 2 », puis « Appuis : 1 » après assignation |
| FIX-2026-10-07-MINIMIZE | Crash en réduisant la fenêtre agrandie (`createDIB: CreateDIBSection failed (32798x32576)`) : Windows place la fenêtre réduite en (-32000, -32000) en gardant l'état agrandi, `AppShell::updateMaximizedMargins` en tirait des marges ~25 600 px → taille minimale géante | retour anticipé si réduite (`isMinimized`/`IsIconic`) + débordement de cadre plafonné à 64 px | GUI : agrandir → réduire → restaurer (ShowWindow 3/6/9) : ancien exe reproduit l'erreur, nouveau non ; 212/212 PASS |
| FIX-2026-10-06-RESIZE | Bords de fenêtre non redimensionnables là où le workspace (fenêtres natives, ancêtres du viewport OCCT) recouvre la bordure | filtre natif `ResizeBorderFilter` : HTTRANSPARENT sur la bordure pour les fenêtres enfants | GUI : bords droit, gauche et bas en mode Workspace |
