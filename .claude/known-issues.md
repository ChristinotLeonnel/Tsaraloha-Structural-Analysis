# Known Issues

Last Updated: 2026-10-04. Ne pas supprimer un bug corrigé : passer son statut à FIXED (avec preuve).

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

## Corrigés (historique)

| ID | Problème | Correction | Preuve |
| :--- | :--- | :--- | :--- |
| FIX-001 | Crash au démarrage : boucle de signaux WorkPlane (débordement de pile) | garde de réentrance ViewportContainer | démarrages répétés OK (e767a81) |
| FIX-002 | Crash à la fermeture : observateurs → Model libéré | `IModelObserver::onModelDestroyed` | fermetures code 0 (ec977db) |
| FIX-003 | Charges non sauvegardées dans .tsa | chunk LOAD, format 1.1 | test 91 (4012e3a) |
| FIX-004 | Sauvegarde non atomique | QSaveFile | test 91 |
| FIX-005 | Nœuds et fondations jamais affichés (IsDone avant Build) | Build() explicite | tests 93–94 (87c59f9) |
| FIX-006 | Fantômes / éléments masqués en mode 2D et isolation | visibilité recalculée | revue de code (3dd90cc) |
| FIX-007 | Reconstruction 3D en O(N·M) avec redraw par nœud | passe unique | 41,7 s → 2,5 s (1 408 barres) |
| FIX-008 | NDC régénérée 2× à chaque ouverture | génération différée | 27 s → 0,26 s |
| FIX-009 | Résultats jamais invalidés | ResultsValidityGuard + Model::revision | test 95 (79b07f4) |
| FIX-010 | Propriétés appliquées à chaque frappe, entrées Undo parasites | keyboardTracking off + coalescence | test 97 (712c442) |
| FIX-011 | Commande en échec → entrée Undo vide | CommandManager transactionnel | test 96 |
| FIX-012 | Ctrl+A : signal + redraw par élément, un seul surligné | selectElements + highlightSelection | 154 ms sur 4 896 barres (386ff32) |
| FIX-013 | Élévation de niveau : déplacement silencieux de nœuds non rattachés | règle levelId | test 99 (46a22ac) |
| FIX-014 | Marqueur d'accrochage sélectionnable | mode de sélection -1 | revue de code |
| FIX-015 | Poutre N et poteau N (ids par famille) confondus dans CalculationSnapshot : un élément retiré du calcul | tags OpenSees uniques + ElementKey | test 105 |
| FIX-016 | `element truss $A $E` refusé par OpenSees 3.8.0 (« Invalid matTag ») : tout modèle avec treillis échouait | uniaxialMaterial + matTag | test 105 |
| FIX-017 | Efforts lus décalés (Truss localForce = 12 valeurs, CorotTruss = 0, lecteur supposait 1) | recorders séparés, basicForce, contrôle des tailles | tests 105–108 |
| FIX-018 | Script Tcl en `std::fixed` 12 décimales : inerties tronquées (~3·10⁻⁷ relatif) | 17 chiffres significatifs | test 107 |
| FIX-019 | Réactions des ressorts non enregistrées (nœuds auxiliaires) : équilibre faux avec appuis élastiques ; tags auxiliaires 90000 pouvant collisionner | report sur le nœud TSA, tags > max | test 108 |
| FIX-020 | Contrôle d'équilibre : charges non converties en N (useKiloNewtons = false) | facteurs identiques à buildLoads | test 110 |
| FIX-021 | MemberLoadDialog n'enregistrait pas la famille cible (charges sur poteau/treillis stockées « Beam ») | targetType + compatibilité des anciens fichiers | test 105 |
| FIX-022 | Export Tcl (OpenSeesAdapter) : générateur parallèle avec les mêmes défauts | délègue à OpenSeesAnalysisBuilder | tests 62, 64 |
| FIX-024 | Arbre du modèle : poutres/poteaux affichés « 0.30x0.30 m » pour des sections circulaires (Circ D20) — champs largeur×hauteur non significatifs | nom de section affiché | vérifié dans le .tsa (zlib) + capture |
| FIX-023 | « Maillage EF » affichait un maillage généré inexistant ; dalles/voiles non signalés avant calcul | estimation + confirmation (BUG-002 mitigé) | revue de code |
