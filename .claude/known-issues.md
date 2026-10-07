# Known Issues

Last Updated: 2026-10-07 (branche feature/static-only). Ne pas supprimer un bug corrigé : passer son statut à FIXED (avec preuve).

## Ouverts

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
Problem: 37 points d'entrée appellent `Model::pushUndoState` directement (dialogues, OccView_Events,
MainWindow_Transform) : fonctionnels, mais sans `EditRecord` (historique structuré incomplet).
Impact: LOW
Status: OPEN

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

## BUG-035
Area: Tests
Problem: test 20 (« Undo diff computation is fast < 350 ms in Debug », 5 000 barres) mesure ≈ 280 ms ; il a dépassé
le seuil une fois sous charge (suite complète, 2026-10-06) puis est repassé 4 fois sur 4. Un échec interrompt la
suite `commands` (tests 25, 50, 96, 97, 100 non exécutés).
Impact: LOW (faux négatif possible)
Status: OPEN — piste : mesurer la médiane de 3 essais, ou relever le seuil en Debug.
Related files: tests/test_commands.cpp:370

## Corrigés (historique)

| ID | Problème | Correction | Preuve |
| :--- | :--- | :--- | :--- |
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
