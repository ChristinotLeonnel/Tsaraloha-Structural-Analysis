# Changelog

Les sessions futures ajoutent une entrée datée en tête (plus récent d'abord).

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
