# Known Issues

Last Updated: 2026-10-09 (audit QA, branche fix/audit-qa-2026-10-09). Ne pas supprimer un bug corrigé : passer son statut à FIXED (avec preuve).

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

| ID | Problème | Correction | Preuve |
| :--- | :--- | :--- | :--- |
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
