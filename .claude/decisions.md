# Architecture Decisions

Last Updated: 2026-10-05. Décisions constatées dans le code ou prises lors des sessions de 2026-10-03.

## ADR-001
Title: Viewport unique
Decision: Un seul viewport 3D `OccView` (V3d_View + AIS_InteractiveContext), encapsulé par
`TSA::UI::ViewportContainer` (règles, sélecteur de plan X/Y/Z, bouton 2D). Le système multi-port
(src/UI/Port) a été supprimé (commit 3276633).
Reason: simplicité, un seul contexte AIS, synchronisation sélection/caméra triviale.
Status: ACTIVE

## ADR-002
Title: Modèle structural = source de vérité, synchronisation par observateurs
Decision: `TSA::Model::Model` possède toutes les données ; les vues dérivent du modèle via
`IModelObserver` (add/modify/remove par type, `onModelDiffApplied`, `onModelCleared`,
`onModelEdited`, `onModelDestroyed`). Aucune donnée métier n'est lue depuis OCCT ou un widget.
Reason: cohérence UI ↔ modèle ↔ 3D ↔ calcul (AGENTS.md §10).
Status: ACTIVE

## ADR-003
Title: Undo/Redo par snapshots complets + transactions
Decision: `UndoManager` stocke des `ModelStateSnapshot` complets ; l'annulation applique un
`ModelDiff` ciblé. Les opérations composées passent par `EditTransaction` (une entrée, rollback).
Pas de migration vers des deltas.
Reason: mesuré (test 100, 4 896 barres, Debug) : snapshot 10 ms, Undo 54 ms → le temps ne justifie
pas une refonte ; la mémoire (~3,2 Mio/snapshot) est bornée par un budget de 512 Mio.
Status: ACTIVE (à réévaluer si les modèles dépassent ~50 000 éléments)

## ADR-004
Title: Historique structuré (EditRecord) pour le co-engineering
Decision: chaque entrée d'historique peut porter des `EditRecord` (action, objet, propriété,
avant, après, impacts) ; `UndoManager::undoHistory()` les expose.
Reason: préparer un historique lisible et un assistant IA qui comprend QUI/QUOI/AVANT/APRÈS/IMPACT.
Status: ACTIVE (alimentation partielle, voir BUG-006)

## ADR-005
Title: Format .tsa binaire à chunks, compatibilité ascendante
Decision: en-tête fixe 256 o (magic `TSAF`), chunks FourCC avec taille (chunks inconnus ignorés),
zlib, CRC32 ; version mineure incrémentée pour tout ajout non cassant (1.1 = chunk LOAD) ; écriture
atomique via QSaveFile. Spécification : docs/TSA_FILE_FORMAT.md.
Status: ACTIVE

## ADR-006
Title: Invalidation des résultats par révision du modèle
Decision: `Model::revision()` (incrémentée par toute notification et `setModified(true)`) ;
`ResultsValidityGuard` invalide les résultats au premier écart avec la révision analysée.
Un Undo ne revalide jamais des résultats.
Reason: ne jamais présenter des résultats d'un modèle différent comme valides.
Status: ACTIVE

## ADR-007
Title: Sélection unique, requêtes ensemblistes dans le cœur
Decision: `SelectionManager` est l'unique système de sélection ; les sélections avancées sont des
fonctions pures `TSA::Model::SelectionQuery` produisant un `ElementSet` appliqué par
`SelectionManager::selectElements` (un seul signal).
Status: ACTIVE

## ADR-008
Title: Tolérance géométrique centralisée
Decision: `TSA::Coordinate::GeometryTolerance::planeMembership` (0,05 m) pour la détection des plans,
l'isolation 2D et la sélection « sur le plan » ; `pointCoincidence` (1e-6 m).
Status: ACTIVE

## ADR-009
Title: Niveaux — seuls les nœuds rattachés suivent un changement d'élévation
Decision: `Model::onLevelElevationChanged` ne déplace que les nœuds dont `levelId` = niveau
(attribué par `Model::addNode` à la cote d'un niveau existant) ; notification groupée.
Reason: aucun déplacement silencieux (choix validé par l'utilisateur, option a).
Status: ACTIVE

## ADR-010
Title: Calcul par OpenSees externe
Decision: le calcul passe par un snapshot immuable (`CalculationSnapshot`) traduit en script Tcl
exécuté par OpenSees via `QProcess` ; seul src/Analysis dépend d'OpenSees.
Status: ACTIVE (exécution synchrone côté UI : BUG-001)

## ADR-011
Title: Threading
Decision: tout accès Qt Widgets et OCCT (AIS/V3d) reste sur le thread UI ; les mises à jour d'UI
déclenchées au milieu d'une notification modèle sont différées (`QTimer::singleShot(0)`).
Status: ACTIVE

## ADR-013
Title: Correspondance TSA ↔ OpenSees centralisée
Decision: `OpenSeesModelMap` (construit depuis le snapshot) est l'unique source des tags OpenSees, axes locaux,
nœuds auxiliaires de ressorts et dispositions des recorders ; générateur et lecteur l'utilisent tous deux.
Éléments désignés par `ElementKey {famille, id}` partout dans les résultats ; tag OpenSees unique 1..N.
Reason: les ids TSA ne sont uniques que par famille (collision poutre/poteau constatée), et le lecteur devinait
la disposition des fichiers.
Status: ACTIVE (2026-10-04)

## ADR-014
Title: Résultats avancés optionnels par passage OpenSees séparé
Decision: `ExtractionLevel::Light` (défaut) / `Advanced`. En Advanced, un second script sans charge
(`initialize`, `record`, `nodeDOFs`, `printA -ret` en `system FullGeneral`) fournit mapping DDL, k_basic et
K_global ; le solveur de l'analyse principale n'est jamais modifié. K_global plafonnée (1 500 DDL par défaut),
stockée en COO creux ; chaque matrice porte un `MatrixMetadata` (source API/reconstruite, type, repère, exact).
Reason: OpenSees 3.8.0 n'expose la matrice du système que pour FullGeneral (dense) ; changer le solveur pour
l'obtenir modifierait le calcul.
Status: ACTIVE (2026-10-04) — voir docs/OPENSEES_RESULTS.md

## ADR-015
Title: IA de co-ingénierie — moteur local externe, protocole unique, outils en liste blanche
Decision: module `src/AI` [CORE] + `src/UI/AI`. Inférence locale par `llama-server` (llama.cpp) lancé
en processus séparé (même principe qu'OpenSees), écoute 127.0.0.1 uniquement, rattaché à TSA par un
Job Object Windows. Un seul client `OpenAICompatibleProvider` (Chat Completions + SSE + outils) sert
llama-server, Ollama et les Cloud compatibles (OpenAI, Gemini) : pas de SDK fournisseur. Le LLM ne
modifie jamais le modèle : outils de lecture + outils `propose_*` qui produisent une `ActionProposal`
appliquée seulement après acceptation (pushUndoState + notify*Modified). Contrôles du modèle
déterministes (`StructuralChecker`) indépendants du LLM. Modèles décrits par
`resources/ai/model_registry.json` (aucun nom codé ailleurs) ; choix par `ModelSelector`, corrigé par
mesures réelles (auto-benchmark) prioritaires sur les heuristiques.
Reason: pas de liaison C++ à llama.cpp ni de CUDA à distribuer ; un plantage ou une saturation mémoire
du moteur IA n'affecte ni l'UI ni le solveur ; backends GPU choisis par le moteur (Vulkan/CUDA/HIP).
Mesure réelle du 2026-10-04 : RTX 3050 via Vulkan 4,4 j/s contre CPU 40,8 j/s → « GPU détecté » ≠
« GPU plus rapide ».
Status: ACTIVE (2026-10-04) — voir docs/AI_COENGINEERING.md

## ADR-016
Title: Miniatures Explorateur — aperçu non compressé dans le .tsa, lu par une DLL autonome
Decision: format 1.2 : bloc PRVW (32 o) + PNG du viewport après le payload (`header.fileSize` = fin du
payload, déjà présent dans l'en-tête). `TSAThumbnailProvider.dll` (C++/Win32, CRT statique, sans Qt ni
OCCT) lit en-tête + PNG via IStream, met à l'échelle avec WIC ; enregistrement HKCU par TSA à chaque
lancement (DllRegisterServer) ou HKLM par installeur (DllInstall "machine").
Reason: pas de décompression ni de zlib 1.2.8 (vulnérable) dans le processus des miniatures ; temps
constant ; un seul moteur de rendu (le viewport). Le lecteur TSA n'honorait pas `payloadOffset` : un bloc
avant le payload aurait cassé les fichiers existants.
Consequence: une version de TSA < 1.2 refuse les fichiers 1.2 (CRC) ; les fichiers ≤ 1.1 restent lisibles.
Status: ACTIVE (2026-10-04) — voir docs/THUMBNAIL_PROVIDER.md

## ADR-012
Title: Performance — mises à jour locales
Decision: modification d'un élément → `update*Shape` de cet élément (et éléments connectés pour un
nœud), sans `rebuildAllShapes` ; reconstruction complète réservée à l'ouverture / reset, en une
passe et un redraw ; génération NDC différée tant que le dock n'est pas visible.
Status: ACTIVE

## ADR-017
Title: Analyse multi-moteurs — moteurs interchangeables derrière AnalysisEngine
Decision: le calcul passe par `AnalysisManager` : `AnalysisContext` (moteur, dimension, type, portée, chargement,
réglages communs, réglages JSON par moteur) → `AnalysisScopeResolver` (grille / niveau / WorkPlane / sélection, sans
nouveau système de coordonnées : GridDefinition, LevelManager, WorkPlaneManager, SelectionQuery, planeMembership) →
`AnalysisModel` DÉRIVÉ (une capture `CalculationSnapshot` restreinte, `AnalysisMapping` TSA ↔ indices, plan 2D)
→ validation générique pilotée par `AnalysisCapabilities` + `AnalysisEngine::validate` → `AnalysisEngine::run` →
`ResultsModel` indexé par ids TSA (l'adaptateur remappe). Le registre (`registerBuiltInEngines`) et le registre des
panneaux d'options (`registerBuiltInEngineOptions`, côté UI : le cœur ne dépend pas des widgets) sont les seuls points
d'ajout d'un moteur. OpenSees est adapté sans réécriture (`OpenSeesSolver::solveSnapshot`). Custom2D : contrat
`Custom2D::ISolver` en données planes pures ; sans solveur connecté, indisponible et aucun résultat.
Reason: éviter les `if (engine == …)` dans TSA ; le modèle TSA reste la seule source de vérité ; OpenSees n'est plus
la définition de l'architecture d'analyse.
Consequences: `SnapshotNode::definedFix` (blocages définis, distincts des blocages anti-singularité 3D) ;
`ResultAvailability` / `EngineResultTable` dans ResultsModel ; `AnalysisConfigDialog` remplacé par
`AnalysisDialog` + `OpenSeesOptionsWidget`. Pas de cache d'AnalysisModel (extraction O(N), grilles hors révision).
Status: ACTIVE (2026-10-05) — voir docs/ANALYSIS_ENGINES.md

## ADR-018
Title: Outils de modification / dessin — un cadre, deux modes de saisie
Decision: chaque outil est un `TSA::Interaction::ModelingTool` (cœur, testable) : paramètres typés + étapes de saisie
3D (`nextPick` Point/Barre, `addPick`, `acceptValue`, `finish`, `preview`) + `apply(model, ctx)` qui n'utilise que
les paramètres et les cibles. La vue (`OccView_Tools.cpp`, mode `InteractionMode::ModelingTool`) ne fait que saisie
et aperçu ; la fenêtre (`ModelingToolDialog`) est générée depuis les paramètres ; MainWindow exécute dans une
`EditTransaction`. Saisie 3D par défaut (QSettings), fenêtre en option. Registre `registerBuiltInModelingTools`.
Reason: demande utilisateur (modification directement en 3D, fenêtre optionnelle) sans dupliquer la logique métier
par mode ni ajouter de branches à OccView_Events.
Consequences: TransformDialog supprimé ; anciens modes Move3D/Copy3D/Rotate3D inatteignables (BUG-026) ;
`Model::splitBarAt`, `Model::transformNodes` ajoutés.
Status: ACTIVE (2026-10-05) — voir docs/MODELING_TOOLS.md

## ADR-019
Title: Moteur Custom2D = MetDeDeplacement (bibliothèque autonome dans thirdparty, suivie par git)
Decision: le modèle de l'utilisateur (méthode des rotations, entrée JSON « dessin ») est réécrit en bibliothèque C++20
sans dépendance (`mdd`) : forme matricielle générale de la méthode des déplacements, mêmes formules de base (4EI/L,
report 1/2, encastrement parfait), courbes RDM exactes et déformée par la ligne élastique. TSA la relie par
`MetDeDeplacementSolver` (combinaisons, poids propre, conventions). L'ancien code est conservé dans legacy/ (non
compilé). Les courbes transitent par `ResultsModel::planarCurves` (générique 2D) vers la NDC.
Reason: demande utilisateur (adapter et optimiser le modèle, courbes 2D dans la NDC, déformée RDM) ; l'entrée
« dessin » et les heuristiques d'étage n'étaient pas transposables au modèle TSA.
Consequences: thirdparty/MetDeDeplacement suivi (exception .gitignore), TSA_Core lié à MetDeDeplacement.
Status: ACTIVE (2026-10-05)

## ADR-020
Title: Couche BIM additive dans le Model (physique ↔ analytique 1:N, GlobalId distincts des id internes)
Decision: les éléments TSA existants restent le modèle analytique (solveurs inchangés). `TSA::BIM::BimModel`
(src/BIM/Core) porte les produits physiques (catégorie / PredefinedType IFC 4.3, GlobalId, Psets, classifications,
étage) et leur liste ordonnée d'éléments analytiques. Il est membre de `Model` (mutable, synchronisé paresseusement,
idempotent), capturé dans `ModelStateSnapshot` (synchronisé avant capture → GlobalId stables à l'Annuler), persisté
dans le chunk JSON versionné `BIMM` (format 1.3, facultatif). Division → même produit ; copie → nouveau produit.
Reason: mission BIM/openBIM (ISO 19650, IFC 4.3, IDS, BCF) avec les priorités stabilité > séparation BIM/analyse >
identifiants stables ; un modèle physique séparé dupliquerait la géométrie et casserait outils, undo et solveurs.
Consequences: tout nouvel outil qui divise / copie des éléments appelle `bimForEdit().attachSplit/registerCopies`.
Les mutations directes non notifiées restent couvertes par la signature de synchronisation. Pas de revendication de
conformité ISO / buildingSMART.
Status: ACTIVE (2026-10-06)

## ADR-021
Title: Fenêtre unique AppShell : Start Center au lancement, workspace créé à la demande, barre de titre personnalisée
Decision: `TSA::UI::AppShell` (src/UI/Shell) est la seule fenêtre top-level : `TitleBar` + `QStackedWidget`
{`StartCenter`, `MainWindow`}. `ApplicationMode` StartCenter / ProjectWorkspace. `MainWindow` devient le workspace,
construit au premier projet ouvert ou créé (pas au lancement), conservé ensuite et vidé par `closeProject()`
(`resetWorkspace`). Les commandes de cycle de vie (Nouveau, Ouvrir, Fermer, Quitter, plein écran) remontent par
signaux à AppShell. Barre de titre sans cadre système mais avec le style WS_OVERLAPPEDWINDOW conservé
(WM_NCCALCSIZE / WM_NCHITTEST) pour garder le comportement Windows natif.
Reason: démarrage sans charger viewport / docks / ruban (fenêtre visible < 1 s), séparation nette accueil ↔
modélisation sans masquer les widgets un à un, commandes principales sur la ligne du titre (logique AutoCAD).
Recréer MainWindow à chaque projet aurait réinitialisé OCCT et des dizaines de connexions pour un gain nul.
Consequences: ne jamais utiliser `MainWindow::close()` / `isFullScreen()` / `setWindowTitle` comme fenêtre (c'est une
page) : passer par les signaux ou `window()`. Nouveau code de cycle de vie dans AppShell. Le Start Center n'est
affiché que sans projet ouvert. Code Win32 limité à AppShell.cpp (exclu du PCH, lien dwmapi).
Status: ACTIVE (2026-10-06)

## ADR-022
Title: TSA se limite au calcul statique ; le dynamique est archivé
Decision: modal, pushover et temporel sont retirés du code (types d'analyse, résultats, OpenSees, NDC, UI). La version
qui les contenait est figée sur la branche `archive/dynamique` (tag `v0.2.0-dynamique`) ; `main` devient la ligne
statique. Conservés : catégorie de charge E (séisme) et combinaisons sismiques (statique équivalent possible).
Convention des efforts de `ResultsModel` : RDM (N > 0 traction ; My, Mz > 0 quand la fibre du côté négatif de l'axe
local est tendue ; Vy = dMz/dx, Vz = dMy/dx), commune à OpenSees et Custom2D.
Reason: le dynamique n'était pas fiable (pushover calculé comme du statique, BUG-024) et coûtait du temps de maintenance
sur chaque évolution du modèle ; l'utilisateur se concentre sur le statique.
Consequences: réintroduire du dynamique = repartir de `archive/dynamique` sur une branche dédiée, en adaptant la
convention RDM. Un ancien contexte JSON de type dynamique est relu comme statique linéaire.
Status: ACTIVE (2026-10-07)

## ADR-023
Title: Base technique commune TSA / TSALab : TSALab compile les sources de TSA, sans copie
Decision: les sources de `src/` et `tests/` de TSA sont la base commune des deux produits. Ce qui distingue un produit
(nom, version, extension, signature de fichier, ProgID, CLSID, QSettings, icônes, textes d'identité) est lu dans
`<ProductIdentity.h>` / `<ProductShellIds.h>` du dossier `product/` du produit compilé (TSA : `TSA/product`,
TSALab : `TSALab/product`), via `src/App/ProductInfo.h`. La construction (listes de sources, Core, exe, tests, DLL,
déploiement) est la fonction `tsa_add_product` de `cmake/TSAProduct.cmake`, appelée par les deux CMakeLists. Les
ajouts propres à un produit passent par des points d'extension (`TSA::Product::configureShell`,
`StartCenter::setLaunchPanel`, `AppShell::setWorkspaceDecorator`, `runProductSuites`) et des sources dans des
dossiers qui ne masquent jamais un chemin de `src/` (vérifié par `tsa_add_product`, FATAL_ERROR sinon).
Reason: TSALab était une copie intégrale de TSA (~470 fichiers identiques) : chaque correction devait être faite deux
fois. Désormais un bug corrigé dans TSA l'est dans TSALab à la recompilation.
Consequences: modifier `src/` impacte les deux produits : compiler et tester TSA (212 tests) ET TSALab (212 + suite
`lab`). Ne jamais réintroduire de littéral d'identité dans `src/` (passer par ProductIdentity). Ne jamais copier un
fichier commun dans TSALab : ajouter un point d'extension dans TSA. Les SDK (OCCT, 3rdparty, OpenSees, Extensions/)
sont ceux du dépôt TSA. TSA conserve ses emplacements QSettings, son format (.tsa, TSAF) et ses textes (sauf trois messages rendus neutres : échec d'ouverture, projet chargé, signature non reconnue).
Status: ACTIVE (2026-10-08)
