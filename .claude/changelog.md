# Changelog

Les sessions futures ajoutent une entrée datée en tête (plus récent d'abord).

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
