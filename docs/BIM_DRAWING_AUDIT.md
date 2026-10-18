# Audit BIM des outils de dessin et de l'interface — 17/10/2026

Branche `feature/bim-drawing` (depuis `feature/io-ifc`). Constat établi en lisant le code et en le recoupant avec les
tests ; chaque problème cite ses fichiers. Architecture BIM existante : `docs/BIM_ARCHITECTURE.md`,
`docs/ANALYTICAL_MODEL.md` ; échange IFC : `docs/IFC_MAPPING.md`, `docs/IFC_VALIDATION.md`.

## 1. Inventaire

| Domaine | Existant | Fichiers |
| :--- | :--- | :--- |
| Modèle central | `TSA::Model::Model` : source de vérité unique, observateurs, snapshots, révision ; éléments analytiques (nœuds, poutres, poteaux, treillis, câbles, dalles, voiles, fondations), charges (`LoadManager`), niveaux (`LevelManager`), plans de travail | `src/Model`, `src/Coordinate` |
| Couche BIM | `BimModel` dans le `Model` : produits physiques (GlobalId, catégorie et classe IFC, étage, Psets, classification), mapping produit → éléments de calcul 1:N, structure spatiale, géométrie importée | `src/BIM/Core` |
| Identifiants | ids entiers par famille (jamais réutilisés pour les produits) ; GlobalId IFC des produits, des objets analytiques, des étages | `BimModel`, `IfcGuid` |
| Historique | `pushUndoState` (snapshot avant modification), `EditTransaction` / `EditRecord`, `UndoManager`, `CommandManager` + `ICommand` (commandes de création et de modification) | `src/UndoRedo`, `src/Commands` |
| Création graphique (A) | modes `InteractionMode::Draw*` (nœud, barre, poutre, poteau, dalle, voile, fondation, treillis, câbles) traités **dans la vue** | `src/Viewer/OccView_Events.cpp`, `OccView.cpp` |
| Création graphique (B) | dialogues « Barre » et « Surface » qui créent nœuds et éléments à la réception des points | `src/UI/Dialogs/BarCreationDialog.cpp`, `SurfaceCreationDialog.cpp` |
| Création / modification (C) | `ModelingTool` : paramètres, aperçu, `apply()` encadré d'une transaction (chaîne de poutres, rectangle, portique, contreventement, arc, poteaux sur grille ; déplacer, copier, tourner, miroir, échelle, réseaux, décaler, diviser, intersecter, prolonger, ajuster, fusionner ; 10 cotations) | `src/Interaction/Tools` |
| Accrochage | `SnapEngine` (espace écran, tolérance en pixels indépendante du zoom, priorités, modes activables : nœud, extrémité, milieu, centre, intersection, perpendiculaire, proche, face, grille) — seul moteur utilisé ; `GridSnapManager` (tolérance métrique) n'a plus d'appelant hors de `src/Grid` | `src/Grid/SnapEngine.*`, `OccView::getPointUnderCursor` |
| Sélection | `SelectionManager` (registre objet OCCT ↔ élément), sélection par fenêtre / croisement, Ctrl + clic, isolation | `src/Viewer/SelectionManager.*` |
| Propriétés | `PropertyPanel` + une vue par famille (édition par le modèle, édition groupée) ; vue « Objet IFC » en lecture seule | `src/UI/Properties` |
| Navigateur | `ModelTreeWidget` : catégories analytiques (nœuds, poutres… charges, appuis, résultats), niveaux, grilles | `src/UI/ModelTree` |
| Grilles | `GridManager` hors du `Model` (fenêtre principale), persisté dans le `.tsa` | `src/Grid` |
| Matériaux, sections | `Material`, `Section` (catalogues IPE, HEA…), **copiés par valeur** dans chaque élément ; `MaterialLibrary` | `src/Model` |
| Ouvertures | `SlabOpening` dans `Slab` et `Wall` ; transmises au maillage et à l'export IFC | `Slab.h`, `Wall.h` |
| Calcul | `AnalysisManager` multi-moteurs (OpenSees, Custom2D), modèle d'analyse dérivé par snapshot ; `ResultsValidityGuard` invalide les résultats à la première modification | `src/Analysis` |
| Validation | `ModelValidator` avant calcul (`Standards`) | `src/Standards/ModelValidator.*` |
| Persistance | `.tsa` à chunks FourCC versionné (1.11), chunks inconnus ignorés ; `BIMM`, `BGEO` | `src/IO` |
| IFC | import IFC2X3 / IFC4 / IFC4X3_ADD2 (IfcOpenShell), export IFC4X3_ADD2 relu | `src/BIM/IFC` |
| Commandes | 205 commandes au catalogue des raccourcis (`cmd.*`), ruban par onglets métier | `src/UI/MainWindow_Shortcuts.cpp`, `src/UI/Ribbon` |

## 2. Problèmes constatés

| N° | Gravité | Problème | Où | Correction |
| :--- | :--- | :--- | :--- | :--- |
| D1 | critique | **Nœuds orphelins** : les modes Draw* créent les nœuds dès le clic (`getOrCreateNode`), hors historique ; Échap (`cancelCurrentDrawing`) n'en retire aucun ; une dalle abandonnée au 3ᵉ point laisse 3 nœuds | `OccView_Events.cpp` (Beam, Column, Slab, Wall, Truss, Bar), `OccView.cpp:896` | points mémorisés pendant le tracé, nœuds créés **à la validation**, dans la même transaction que l'élément |
| D2 | critique | **Annuler incomplet** : la poutre est annulée mais pas ses nœuds (snapshot pris après leur création) ; même défaut dans le dialogue « Barre » (`addNode` avant `pushUndoState`) | `OccView_Events.cpp:208`, `BarCreationDialog.cpp:394-410` | une transaction par création : nœuds + élément + produit BIM |
| D3 | haute | **Logique métier dans la vue** : création, préréglages, messages, dans le gestionnaire de souris d'`OccView` ; trois chemins de création (A, B, C) aux règles différentes ; non testable sans fenêtre | `OccView_Events.cpp`, dialogues | service de création unique, sans vue (`ElementCreation`), appelé par la vue et les dialogues |
| D4 | haute | **Valeurs inventées** : semelle 1,50 × 1,50 × 0,50 codée en dur, poteau de 3 m créé si l'on reclique au même point, barre de treillis de 0,10 sans section ni matériau choisis | `OccView_Events.cpp:258, 370, 396` | préréglages explicites (fondation, treillis), aucune hauteur implicite : clic au même point refusé avec un message |
| D5 | haute | **Pas de validation avant création** : dalle à 3 points alignés ou non plane, voile de longueur nulle, élément en double sur les mêmes nœuds acceptés | `OccView_Events.cpp`, `OccView::finishCurrentSlab` | validation géométrique avant toute modification du modèle, refus motivé |
| D6 | haute | **Croisement ≠ connexion non signalé** : une extrémité posée sur l'intérieur d'une barre existante (sans nœud) crée une barre non connectée, en silence | création des barres | avertissement « non connectée » ; aucune division automatique (connexion = décision explicite : outil Intersecter / Diviser) |
| D7 | moyenne | Coïncidence de points : fusion silencieuse avec un nœud à moins de 1 mm, tolérance locale codée en dur | `OccView::getOrCreateNode` | tolérance unique documentée (`GeometryTolerance`), nœud réutilisé seulement s'il coïncide |
| D8 | moyenne | **Quantités absentes** : aucun service de longueurs, aires, volumes, masses ; pas de nomenclature | — | service `QuantityTakeoff` dérivé du modèle, par élément, produit, matériau, section |
| D9 | moyenne | Propriétés : la palette des éléments créés dans TSA n'affiche ni l'identité BIM (GlobalId, classe IFC, étage, produit) ni les quantités | `src/UI/Properties` | panneau « Identité BIM et quantités » commun, en lecture seule, sous la vue de l'élément |
| D10 | moyenne | Navigateur sans hiérarchie spatiale BIM (projet, bâtiment, étages, produits) | `ModelTreeWidget` | branche BIM (à faire) |
| D11 | moyenne | Types : sections et matériaux copiés par valeur, pas de type partagé ; une modification de type ne se propage pas | `Beam.h`, `Material.h` | bibliothèque de types (à faire, migration du format) |
| D12 | basse | Grilles hors du `Model` (repère de grille d'un produit non calculé) | `src/Grid` | à faire |
| D13 | basse | `GridSnapManager` : recherche sans appelant (doublon de `SnapEngine`), mais réglages d'accrochage encore lus par la vue (corrigé le 18/10/2026, voir feuille de route, chantier D) | `src/Grid` | migration progressive |

Points déjà conformes (pas de refonte) : accrochage centralisé (`SnapEngine`) ; résultats périmés invalidés
(`ResultsValidityGuard`) ; ouvertures portées par l'élément ; modèle analytique distinct du modèle BIM avec mapping
explicite ; outils `ModelingTool` transactionnels et testés (tests 140-149) ; IFC (tests 400-409).

## 3. Couverture de tests

Modes Draw* : aucune création testée (3 mentions dans `test_viewer.cpp`) ; dialogues de création : aucun test.
`ModelingTool` : tests 140-149 ; BIM : 170-191 ; IFC : 400-409.
