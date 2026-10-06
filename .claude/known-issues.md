# Known Issues

Last Updated: 2026-10-06. Ne pas supprimer un bug corrigé : passer son statut à FIXED (avec preuve).

## Ouverts

## BUG-001
Area: Calculation / UI
Problem: Le calcul OpenSees est lancé par `OpenSeesSolver::solveSynchronous` depuis
MainWindow_Tools.cpp (3 appels) : boucle `waitForFinished(100)` sur le thread UI. L'interface est
gelée pendant le calcul et `m_stopRequested` ne peut pas être positionné (pas de traitement d'événements).
Reproduction: lancer un calcul sur un grand modèle ; la fenêtre ne répond plus jusqu'à la fin.
Impact: MEDIUM
Status: OPEN
Related files: src/Analysis/OpenSeesSolver.cpp, src/UI/MainWindow_Tools.cpp
Notes: un chemin `solveAsync` (QThread) existe déjà : à évaluer avant toute modification.

## BUG-002
Area: Calculation
Problem: Dalles et voiles ne sont pas transmis au calcul (`CalculationSnapshot` /
`OpenSeesAnalysisBuilder` ne traitent que barres, treillis, câbles, appuis) ; aucun maillage EF.
Reproduction: modèle avec dalle chargée → la dalle n'apparaît pas dans le script Tcl.
Impact: HIGH (limitation fonctionnelle à signaler à l'utilisateur)
Status: OPEN — mitigé le 2026-10-04 : avertissement `ModelValidator::validateForAnalysis`, confirmation
avant calcul statique/modal/pushover (`confirmPlanarElementsExcluded`, MainWindow_Tools.cpp), et
`onActionMeshGen` n'affirme plus avoir généré un maillage (il ne fait qu'une estimation).
Related files: src/Analysis/CalculationSnapshot.cpp, src/Analysis/OpenSeesAnalysisBuilder.cpp

## BUG-003
Area: Undo/Redo
Problem: Niveaux, grilles et WorkPlanes ne font pas partie de `ModelStateSnapshot` : leurs
modifications ne sont pas annulables, et un Undo après une modification d'élévation de niveau
restaure les nœuds sans le niveau.
Impact: MEDIUM
Status: OPEN
Related files: src/Model/Model.h (ModelStateSnapshot), src/Coordinate/LevelManager.*, src/UndoRedo/UndoManager.cpp

## BUG-004
Area: Commands / Shortcuts
Problem: Commandes d'isolation déclarées dans CommandCatalog (`cmd.isolate.same_type` Alt+I,
`cmd.isolate.workplane` Alt+W, `cmd.isolate.section` Ctrl+I, `cmd.isolate.projection` Ctrl+Shift+I)
et documentées dans docs/shortcuts.txt, mais branchées nulle part (aucune référence hors du
catalogue). Le module `src/View3D/Isolation` qui devait les porter n'est pas compilé.
Impact: MEDIUM (raccourcis documentés inopérants)
Status: OPEN
Related files: src/Commands/CommandCatalog.cpp, src/View3D/Isolation/*, tests/isolation/*

## BUG-005
Area: Properties
Problem: Édition multi-objets absente : en sélection multiple, seul l'élément principal est
affiché/modifiable.
Impact: MEDIUM
Status: OPEN
Related files: src/UI/Properties/PropertyPanel.cpp, src/UI/MainWindow_Actions.cpp (multipleSelectionChanged)

## BUG-006
Area: Undo/Redo
Problem: 37 points d'entrée appellent `Model::pushUndoState` directement (dialogues, OccView_Events,
MainWindow_Transform) : fonctionnels, mais sans `EditRecord` (historique structuré incomplet).
Impact: LOW
Status: OPEN

## BUG-007
Area: Performance / Model Tree
Problem: `ModelTreeWidget::on*Modified/Removed` recherchent l'item par parcours linéaire des enfants
(O(N) par notification) ; coûteux lors de grosses modifications groupées.
Impact: LOW
Status: OPEN
Related files: src/UI/ModelTree/ModelTreeWidget.cpp

## BUG-008
Area: Viewport / Performance
Problem: Ouverture d'un modèle de 4 896 barres ≈ 16 s en Debug (reconstruction 3D ≈ 13 s : une
sphère + un label par nœud, un solide par barre). Non mesuré en Release.
Impact: LOW
Status: OPEN
Notes: piste : partager la géométrie des sphères de nœuds (TopLoc_Location).

## BUG-009
Area: Code mort
Problem: `src/View3D/Isolation/*` et `tests/isolation/*` absents de CMakeLists.txt (jamais compilés).
Impact: LOW
Status: OPEN (décision à prendre : brancher pour BUG-004 ou supprimer)

## BUG-010
Area: Geometry
Problem: Avertissement C4996 `TColgp_HArray1OfPnt` déprécié (OCCT 8.0).
Impact: LOW
Status: OPEN
Related files: src/Geometry/CableGeometry3D.cpp:171

## BUG-011
Area: Build
Problem: MSVC francisé : préfixe /showIncludes avec espace insécable dépendant de la page de code
(0xFF en CP850, C2 A0 en 65001). Si configure et build ne tournent pas sous la même page de code,
Ninja n'enregistre aucune dépendance d'en-tête → objets périmés, crashs aléatoires.
Impact: HIGH (contenu, pas résolu)
Status: OPEN — mitigé (avertissement CMake, VSLANG=1033 dans les presets, AGENTS.md)
Notes: vérifier `ninja -t deps <obj>` ; solution durable : pack de langue anglais de Visual Studio.

## BUG-012
Area: Environment
Problem: Processus TSA.exe « clones WER » bloqués (anciens crashs) qui verrouillent l'exécutable
(LNK1168) ; non terminables (accès refusé).
Impact: LOW
Status: OPEN — contournement : renommer TSA.exe ; disparaissent au redémarrage.

## BUG-013
Area: .tsa
Problem: Chunks `SETT` (paramètres d'analyse) et `RSLT` (résultats) déclarés mais non écrits :
paramètres et résultats non persistés (recalcul nécessaire après réouverture).
Notes (2026-10-05): le contexte d'analyse multi-moteurs (`AnalysisContext::toJson`, schéma versionné, réglages par
moteur) est prêt à être écrit dans un chunk ; non branché (MainWindow::m_analysisContext vit pour la session).
Impact: LOW
Status: OPEN

## BUG-016
Area: Results / Diagrams
Problem: `buildElementResults` (OpenSeesResultsReader) construit les stations intermédiaires par interpolation
linéaire des efforts d'extrémité + superposition de `q1` brut (sans direction, axe local ni unités ; ajouté à
My seulement). Les diagrammes intermédiaires (et maxBendingMoment / NDC qui les lisent) sont donc approchés,
voire faux pour des charges non gravitaires ou en N. Comportement historique, conservé tel quel.
Impact: HIGH (exactitude des diagrammes et de la NDC)
Status: OPEN — les valeurs d'extrémité et les forces brutes (ADVANCED) sont exactes.
Related files: src/Analysis/OpenSeesResultsReader.cpp (buildElementResults)
Notes: piste exacte : enregistrer `sectionForce` aux points d'intégration ou reconstruire M(x), V(x) à partir
de basicForce + charges eleLoad résolues en local (même LoadResolver que buildLoads).

## BUG-017
Area: Calculation
Problem: Équilibre contrôlé en forces uniquement (ΣF + ΣR) ; pas de contrôle des moments.
Impact: LOW
Status: OPEN

## BUG-018
Area: NDC
Problem: liens `tsa://element?id=N` de la NDC sans famille (poutre N / poteau N ambigus).
Impact: LOW
Status: OPEN
Related files: src/NDC/NDCGenerator.cpp

## BUG-019
Area: Calculation
Problem: nœuds reliés uniquement à des treillis/câbles en modèle -ndf 6 : DDL de rotation sans rigidité
(singularité possible). Non vérifié.
Impact: UNKNOWN
Status: OPEN (à vérifier par un test OpenSees)

## BUG-020
Area: AI / UI
Problem: textes des boutons standard Qt (QMessageBox « Yes / No ») en anglais : aucune traduction Qt
(qtbase_fr) n'est chargée par l'application. Observé dans la confirmation de téléchargement de modèle.
Impact: LOW (cosmétique, toutes les boîtes standard de TSA)
Status: OPEN

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

## BUG-024
Area: Calculation / OpenSees
Problem: `OpenSeesAnalysisBuilder::buildAnalysisCommands` ne traite que Modal et NonLinearStatic ; les types
`Pushover` et `DynamicTimeHistory` tombent dans la branche statique linéaire (`algorithm Linear`, un pas), et
`OpenSeesResultsReader` ne lit pas de pas de pushover. L'action « Analyse Pushover » du ruban lance donc un calcul
statique linéaire.
Reproduction: lire src/Analysis/OpenSeesAnalysisBuilder.cpp (buildAnalysisCommands) ; constaté le 2026-10-05.
Impact: MEDIUM
Status: OPEN — mitigé : `OpenSeesEngine::capabilities()` ne déclare ni pushover ni temporel, la fenêtre Analysis
ne les propose plus. Le raccourci ruban (appel direct d'OpenSeesSolver) reste inchangé.
Related files: src/Analysis/OpenSeesAnalysisBuilder.cpp, src/UI/MainWindow_Tools.cpp (onActionPushover)

## BUG-025
Area: Tests GUI (UI Automation)
Problem: un `Invoke` UIA sur une action qui ouvre une fenêtre modale (`exec()`) ne rend pas la main et bloque les
requêtes UIA suivantes vers TSA (timeouts) ; l'application reste réactive.
Notes: vérification de la fenêtre Analysis faite par capture d'écran (Invoke lancé dans un job séparé).
Impact: LOW (outillage de test)
Status: OPEN

## BUG-026
Area: Viewport / Code mort
Problem: les modes `InteractionMode::Move3D / Copy3D / Rotate3D` (branches d'OccView_Events, updateTransformPreview,
signaux pointToPoint*, MainWindow::onPointToPoint*Requested) ne sont plus déclenchés : les actions M, Copie 3D et
Ctrl+R lancent désormais les outils `move` / `copy` / `rotate`. Code conservé mais inatteignable.
Impact: LOW
Status: OPEN — à supprimer (vérifier InteractionManager::isTransformMode et la synchro des boutons).

## BUG-027
Area: Calculation / OpenSees
Problem: les relâchements d'extrémité des poutres (EndRelease) ne sont pas transmis au script OpenSees (aucun
traitement dans OpenSeesAnalysisBuilder) : une rotule de barre est calculée comme un encastrement. Le snapshot les
porte depuis le 2026-10-05 (utilisés par Custom2D).
Impact: MEDIUM
Status: OPEN
Related files: src/Analysis/OpenSeesAnalysisBuilder.cpp, src/Analysis/CalculationSnapshot.h

## BUG-028
Area: BIM / IFC
Problem: export IFC partiel — charges / cas / combinaisons, relâchements d'extrémité, excentrements, appuis orientés et
grilles non exportés ; unités dérivées (module, masse volumique) non déclarées ; GlobalId des relations et Psets
recréés à chaque export ; fondations sans objet analytique IFC. Liste tenue à jour dans docs/IFC_MAPPING.md §4.
Impact: MEDIUM (échange incomplet, pas de perte dans le .tsa)
Status: OPEN
Related files: src/BIM/IFC/IfcExporter.cpp, docs/IFC_MAPPING.md

## BUG-029
Area: BIM / Presse-papiers
Problem: le collage (`StructuralClipboard`) ne transmet pas les métadonnées BIM (nom, Psets, classification, regroupement
1:N) : chaque élément collé reçoit un produit physique 1:1 par la synchronisation. Copier / Répéter (copyTransformed)
les transmet (registerCopies).
Impact: LOW
Status: OPEN
Related files: src/Model/StructuralClipboard.cpp, src/BIM/Core/BimModel.cpp

## BUG-033
Area: UI / Fenêtre
Problem: non vérifiable sur le poste de test (un seul écran à 125 %, Windows 10) : DPI différent par écran et
Windows 11 « Snap layouts » au survol du bouton Agrandir (HTMAXBUTTON non renvoyé : le renvoyer exigerait de gérer
les clics non clients du bouton). Le redimensionnement par les bords couverts par les fenêtres natives du workspace
est corrigé (voir FIX-2026-10-06-RESIZE).
Impact: LOW
Status: OPEN (à vérifier sur Windows 11 / multi-écrans)
Related files: src/UI/Shell/AppShell.cpp

## Corrigés (historique)

| ID | Problème | Correction | Preuve |
| :--- | :--- | :--- | :--- |
| BUG-030 | Menu Structure ▸ Conditions d'Appuis : Encastrement / Articulation / Appui simple n'assignaient rien | `MainWindow::assignSupport` (pushUndoState + setSupport + notifyNodeModified) | GUI 2026-10-06 : appui assigné, dock « Appuis : 1 », calcul OpenSees OK (δ_max 28,63 mm, équilibre conforme) |
| BUG-031 | Suppression de 6 768 éléments ≈ 6 min (Debug) : un redessin complet du viewport et une recherche linéaire dans l'arbre par élément | `OccView::scheduleRedraw` (un redessin par rafale), `ModelTreeWidget::queueRemoval/flushRemovals` (une passe par catégorie) | GUI stress_4900 : suppression 17 s, Rétablir ≈ 21 s, Annuler ≈ 17 s (≈ 30 s avant), modèle restauré sans doublon |
| BUG-032 | Barre d'état : libellés superposés à toute largeur | cause : politique `Ignored` → case de largeur nulle dans QStatusBar ; largeurs fixes + `fitStatusBar` (masquage par priorité, mise en page forcée car QStatusBar ignore LayoutRequest) ; page cachée sans taille minimale | captures à 3 largeurs, plus de chevauchement |
| BUG-034 | Dock Résultats : « Nœuds : 0 » après ouverture | `updateNodeStats` public, appelé à l'ouverture et à chaque révision du modèle | GUI : « Nœuds : 2 », puis « Appuis : 1 » après assignation |
| FIX-2026-10-06-RESIZE | Bords de fenêtre non redimensionnables là où le workspace (fenêtres natives, ancêtres du viewport OCCT) recouvre la bordure | filtre natif `ResizeBorderFilter` : HTTRANSPARENT sur la bordure pour les fenêtres enfants | GUI : bords droit, gauche et bas en mode Workspace |
