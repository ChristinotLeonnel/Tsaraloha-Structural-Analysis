# Current State

Last Updated: 2026-10-07 — branche feature/static-only (statique seul, ADR-022 ; corrections BUG-001 à 029)

Légende : IMPLEMENTED · PARTIAL · BROKEN · MISSING · UNKNOWN (preuve dans le code ou test exigée).

## Écosystème Tsaraloha (ADR-024, docs/TSARALOHA_ARCHITECTURE.md)
Status: IMPLEMENTED (phases 1 à 6, 2026-10-08, branche feature/tsaraloha-shared-libs) — bibliothèques partagées
Model / Graphics / Widgets, ProjectSession, SelectionSynchronizer, EcosystemApplication, cœur scientifique
tsalab_science (dépôt TSALab), registre de commandes. TSALab.exe = IDE propre sur ces bibliothèques.
Blueprint : moteur + éditeur partagés (docs/BLUEPRINT.md), vérifié dans TSALab (exemple portique exécuté).
Phase 7 IMPLEMENTED : AnalysisController dans ProjectSession (MainWindow n'orchestre plus le calcul),
AnalysisManagerPanel partagé, portée « plan du modèle », export K·U = F par Custom2D (dock Données d'analyse,
onglet K·U = F ; SOLVER LAB), commandes analysis.run / results.summary / results.node_displacement.
Vérifié : tests 196 (TSA), L7 (TSALab), GUI TSALab (portique : calcul, synthèse, SOLVER LAB LU / Cholesky / GC).
Phase 8 IMPLEMENTED (2026-10-08) : débogueur Blueprint (points d'arrêt, pas à pas, valeurs, arrêt), exécution en
tâche de fond des graphes sans accès au projet, historique de l'éditeur, script de commandes ↔ Blueprint, outils IA
list_commands / propose_blueprint, API de plugins (docs/PLUGINS.md, PluginManager chargé par EcosystemApplication),
OpenSees dans tsalab_science (OpenSeesPlanarSolver, validation croisée 14/14). Tests 197–198 (TSA), L8 (TSALab), S6.

## Build
Status: IMPLEMENTED — PASS (preset ninja-debug, -j 4)
Compiler: MSVC 19.51 (VS 18 2026 Community), C++20
Qt: 6.11.2 · OpenCASCADE: 8.0.1 · CMake: 4.3.3 (min 3.20)
Warnings: aucun connu (C4996 corrigé, BUG-010).
Note: MSVC francisé → lanceur généré `build-*/msvc_codepage.cmd` (page de code de la génération imposée à cl) :
dépendances d'en-têtes Ninja fiables quelle que soit la console (BUG-011, vérifié `ninja -t deps`).

## Tests
Status: IMPLEMENTED — 228/228 PASS le 2026-10-09 (audit QA : 205 annulation, 206 résultats périmés, 207 JSON des grilles) ; 225/225 PASS le 2026-10-09 (202 repères de grille, 203–204 déformée continue) ; 222/222 PASS le 2026-10-09 (test 201 : chemins accentués, chunk > 64 Kio, nom du projet, ouverture atomique ; TSALab 5/5, science 6/6, banc 14/14) ; 219/219 PASS le 2026-10-08 (197 débogueur Blueprint, 198 script ↔ Blueprint + IA) ; 217/217 avant (196 contrôleur d'analyse) ; 216/216 avant (192 registre de commandes, 193–195 Blueprint) ; 212/212 PASS le 2026-10-07 (ajouts : 166 arbre indexé, 181–184 extraction exacte,
188 niveaux/grilles annulables, 189 chunk SETT, 190 collage BIM, 191 édition groupée) ; test 20 sensible au temps (BUG-035) ;
197/197 avant (suite `bim` 170–180 le 2026-10-06, dont IFC 177–180) ;
186/186 avant (suite `cleanup` 160–165 le 2026-10-05) ;
180/180 avant (suite `mdd` 150–159 le 2026-10-05) ;
170/170 avant (suite `tools` 140–149 et test 139 ajoutés le 2026-10-05) ;
159/159 avant (`TSA_TestSuite.exe` ; suite `engines` tests 130–138 ajoutée le 2026-10-05 ;
tests 101–111 ajoutés le 2026-10-04 ;
suite `extraction` = validation numérique contre le binaire OpenSees 3.8.0)
Non couvert : comportement GUI (OccView, docks, mode 2D) — vérifications manuelles uniquement.

## Fenêtre / Start Center (ADR-021)
Status: IMPLEMENTED (2026-10-06, branche feature/start-center) — `AppShell` + `TitleBar` + `StartCenter` +
`NewProjectDialog` ; workspace (`MainWindow`) créé au premier projet. Vérifié dans l'application (Debug, 125 %) :
lancement → Start Center seul (fenêtre < 1 s), Nouveau projet (dialogue, fichier créé, workspace, récents), Ouvrir
(Ctrl+O, stress_4900), projet récent (simple clic = sélection, double-clic = ouverture, vue restaurée), Fermer le projet
(prompt Enregistrer / Ne pas enregistrer / Annuler) → Start Center, fermeture code propre, ligne de commande
`TSA.exe fichier.tsa`, recherche / tri / « Retirer des projets récents » (fichier conservé), menu d'application,
thème clair/sombre, agrandir / restaurer / réduire / déplacer / double-clic / redimensionnement bords et coins,
Annuler / Rétablir de la barre de titre, redimensionnement par les bords en mode Workspace, calcul OpenSees depuis
l'interface (appuis via le menu Structure). Réduire depuis l'état agrandi : crash corrigé le 2026-10-07 (FIX-2026-10-07-MINIMIZE). Non vérifié : multi-écrans / DPI mixte, Snap layouts Win11 (BUG-033).

## UI
Ruban : responsive sans scroll (Full → IconOnly → Collapsed → « Plus »), barre d'accès rapide dans la
rangée d'onglets, infobulles riches. Limite : `ThemeManager::ribbonScrollStyleSheet` devenu inutilisé ;
pas de commandes Wireframe/Shaded/Offset/Trim… (inexistantes dans TSA, donc non ajoutées) ; fenêtre < ~700 px non vérifiée.
Status: IMPLEMENTED — MainWindow + ruban + docks (arbre, visibilité, éléments, propriétés,
résultats, console, projection, diagrammes, NDC), WindowManager (layouts).

## Miniatures Explorateur
Status: IMPLEMENTED (2026-10-04) — TSAThumbnailProvider.dll + format 1.2. Vérifié par le Shell
(IShellItemImageFactory) et par tests 127–129. Non vérifié : redémarrage, HKLM, installeur.

## Projets récents / aperçus
Status: IMPLEMENTED (2026-10-04) — accueil « Projets récents », aperçus capturés dans le viewport réel,
cache + métadonnées, caméra restaurée. Vérifié dans l'application : cartes affichées avec les vrais
aperçus, recapture après changement de caméra, message « Dernière vue du projet restaurée ».
PARTIAL : état d'affichage (viewState) mémorisé mais non restauré. Voir docs/PROJECT_PREVIEWS.md.

## AI Co-Engineering
Status: IMPLEMENTED (première itération, 2026-10-04) — `src/AI` (matériel, registre/sélection, gestion
des modèles, fournisseurs, contexte, outils, vérification, RAG, orchestrateur) + `src/UI/AI` (panneau,
configuration non modale, indicateur barre d'état, ruban Outils/Analyse, menu contextuel de l'arbre).
Vérifié dans l'application (UI Automation) : détection matérielle réelle, téléchargement + SHA-256,
démarrage llama-server, diagnostic, auto-benchmark (CPU 40,8 / Vulkan 4,4 j/s), « Vérifier la
structure » et question libre (réponses fondées sur les données du .tsa). Tests 112–123.
Fournisseur Cloud « Anthropic Claude » ajouté le 2026-10-09 (préréglage `anthropic`, point d'accès
« compatible OpenAI » officiel d'Anthropic, même `OpenAICompatibleProvider` que OpenAI/Gemini/Ollama,
aucun code propre à Anthropic) ; préréglage vérifié dans la boîte de dialogue (UI Automation), jamais
appelé en vrai (nécessite une clé API Anthropic de l'utilisateur, non fournie à Claude).
Claude Code co-ingénieur (2026-10-09) : AutomationServer + pont MCP `tsaraloha-mcp.exe` (docs/MCP.md), test 200,
vérifié en conditions réelles sur TSALab ; serveurs « tsa » et « tsalab » enregistrés dans Claude Code (portée utilisateur).
MISSING : What-If, rapport IA. Voir docs/AI_COENGINEERING.md.

## Viewport
Status: IMPLEMENTED — viewport unique `OccView` (multi-port supprimé), mises à jour incrémentales,
nœuds et fondations affichés depuis le 2026-10-03. Ouverture 4 896 barres ≈ 16 s (Debug).

## Model
Status: IMPLEMENTED — nœuds, poutres, poteaux, dalles, voiles, fondations, treillis, câbles,
sections, matériaux, appuis 6 DDL, révision + observateurs.

## Selection
Status: IMPLEMENTED — clic, Ctrl+clic, fenêtre/capture, tout (Ctrl+A), inverser (Ctrl+Alt+I),
par type, même section, même matériau, niveau actif, plan de travail actif.
PARTIAL — sélection par propriété arbitraire / par proximité : MISSING.

## Properties
Status: IMPLEMENTED — une vue par type, validation, coalescence Undo ; édition groupée (2026-10-07,
`MultiEditSession`, test 191) : en sélection multiple, les champs modifiés de l'élément principal sont reportés aux
éléments du même type (câbles exclus). Panneau non vérifié en GUI.

## Isolation 3D
Status: IMPLEMENTED (code, 2026-10-07) — menu Affichage ▸ Isolation 3D (isoler sélection / type / plan de travail,
masquer, inverser, précédente, tout afficher) dans `OccView::updateElementIsolation`. Non vérifié en GUI.

## Repères de grille
Status: IMPLEMENTED (2026-10-09) — `src/Grid/GridLabelLayout` : une occurrence par axe selon la vue (plan / élévation ∥ X ou Y / 3D), option deux extrémités. Non vérifié : grilles cylindriques en GUI, ViewCube animé.

## WorkPlanes
Status: IMPLEMENTED — plans X/Y/Z détectés depuis le modèle (`CoordinateSystem::detectStructuralPlanes`),
tolérance centralisée `GeometryTolerance::planeMembership`, isolation « Isoler le plan ».

## 2D Mode
Status: IMPLEMENTED — caméra orthographique normale au plan, isolation recalculée depuis les
drapeaux (plus de fantômes), snap projeté. Vérifié par le code ; pas de test GUI automatisé.

## Modeling tools (outils de modification / dessin)
Status: IMPLEMENTED (2026-10-05) — cadre `TSA::Interaction::ModelingTool` : 14 outils de modification (déplacer,
copier, rotation, symétrie, échelle, réseaux linéaire/polaire, décaler, diviser en N / au point, intersecter,
prolonger, ajuster, fusionner) et 6 de dessin (chaîne, rectangle, portique, contreventement X, arc, poteaux sur
grille). Saisie dans la vue 3D par défaut (clics, valeur clavier, aperçu), fenêtre générique en option
(bouton « Saisie dans la vue 3D », Maj + clic). Tests 140–149 ; vérifié dans l'application : panneaux du ruban,
outil Rectangle par deux clics réels. Non vérifié en GUI : les 19 autres outils (testés au niveau modèle).
Voir docs/MODELING_TOOLS.md.

## Model cleanup (nettoyage topologique)
Status: IMPLEMENTED (2026-10-05) — `TSA::Model::ModelCleanup` : fusion des nœuds confondus, barres en double (charges
reportées), raccordement des nœuds posés sur une barre (division), croisements (option, désactivée par défaut),
nœuds parasites. Bilan sans modification (analyze sur copie), application en une transaction. Fenêtre « Nettoyer le
modèle » (Modifier › Symétrie & Topologie, menu Édition) ; proposé automatiquement avant chaque calcul (F5 / fenêtre
Analysis) si le bilan n'est pas vide. Vérifié dans l'application : fenêtre et bilan. Tests 160–165.

## Editing
Status: PARTIAL — propriétés, déplacement/rotation/copie, suppression, presse-papier, transactions,
symétrie (copie / retournement, plans X/Y/Z), division de poutres/poteaux en N tronçons, fusion des
nœuds confondus (Model_Topology.cpp, tests 101–104 ; UI vérifiée par compilation seulement).
MISSING : division d'une barre à un nœud existant / intersection de barres, symétrie par plan
quelconque, ouvertures de dalles/voiles, poignées (grips) dans le viewport.

## Undo/Redo
Status: IMPLEMENTED — snapshots + transactions + historique structuré + budget mémoire ; niveaux, axes et grilles
dans l'instantané depuis le 2026-10-07 (test 188). Plans de travail : état d'affichage, non annulés (choix assumé).

## Loads
Status: IMPLEMENTED — cas, combinaisons, charges nodales et sur barres (poutre/poteau/treillis/câble),
persistés dans le chunk LOAD (.tsa 1.1). Charges surfaciques / thermiques : MISSING.

## Structural Calculation
Status: PARTIAL — **statique seul** (ADR-022 : modal / pushover / temporel retirés, archive `archive/dynamique`).
OpenSees 3.8.0 externe (Tcl, QProcess) pour barres, treillis, câbles, appuis/ressorts ; rotules `-releasey/-releasez`
(BUG-027) ; charges trapézoïdales par forces d'encastrement parfait ; nœuds de treillis purs stabilisés (BUG-019).
Calcul dans un thread de travail avec progression annulable (BUG-001, à vérifier en GUI). Dalles et voiles non
transmis (BUG-002, avertissement avant calcul).

## Analysis engines (multi-moteurs)
Status: IMPLEMENTED (2026-10-05) — `AnalysisEngine` / registre / `AnalysisManager` ; portées : modèle complet,
sélection, axe de grille, niveau, plan de travail ; OpenSees et Custom2D (MetDeDeplacement 2). Types : statique linéaire
et non linéaire. Contexte persisté dans le .tsa (chunk SETT, 2026-10-07, test 189).

## Results
Status: IMPLEMENTED — déformée continue sur les stations du solveur (repli Hermite), structure initiale atténuée / masquée, échelle manuelle conservée (2026-10-09, BUG-045 à 048) ; ResultsModel en convention RDM (N > 0 traction, M > 0 fibre négative tendue, V = dM/dx),
stations exactes par équilibre du tronçon, déformée par double intégration de la courbure (tests 181–184) ; équilibre
contrôlé en forces ET moments (BUG-017) ; déformée / diagrammes / réactions 3D, diagrammes 2D, NDC (liens avec famille,
BUG-018), dock « Données d'analyse », export, invalidation automatique. Résultats non enregistrés dans le .tsa (BUG-013).

## Meshing
Status: MISSING — aucun mailleur EF pour dalles/voiles.

## .tsa
Status: IMPLEMENTED — chemins non ASCII, JSON COOR/GRID > 64 Kio, nom/auteur et ouverture atomique corrigés le 2026-10-09 (BUG-040 à 043) ; format 1.4 (PROJ, THMB, COOR, GRID, NODE, SUPP, BARS, COLS, SLAB, WALL, FNDN, TRUS, CABL,
LOAD, SNAP, BIMM, SETT), zlib, CRC32, écriture atomique, lecture des formats antérieurs. RSLT non écrit.

## Import/Export
Status: PARTIAL — export NDC PDF/HTML, export rapport de diagnostic ; import/export CAO
(IFC/STEP/DXF) : MISSING (aucune référence dans src/).

## Performance
Status: PARTIAL — mesures de référence : voir changelog 2026-10-03 et test 100.

## Threading
Status: PARTIAL — calcul (AnalysisManager) dans un `QThread::create` lancé par `MainWindow::runAnalysis`, rappels
renvoyés au thread UI ; reste de l'application mono-thread.

## Couche BIM (ADR-020)
Status: PARTIAL — phases 1–3, 8, 9 et 4 (sauf repère de grille) de docs/BIM_ARCHITECTURE.md (2026-10-06).
IMPLEMENTED : `src/BIM/Core` (BimTypes, IfcGuid, BimModel), `src/Core/Units.h` ; `Model::bim()` (sync paresseuse),
`bimForEdit()`, `setBim()` ; snapshot Undo ; hooks division / copie ; chunk `.tsa` BIMM (format 1.3).
IFC (`src/BIM/IFC`) : StepWriter/StepReader, IfcMapper, IfcGeometryMapper, IfcPropertyMapper, IfcRelationshipMapper,
IfcExporter (IFC4X3_ADD2, physique + analytique), IfcImporter (IFC2X3/4/4X3, analytique ou déduit de la géométrie,
unités du fichier). UI : Fichier > Importer / Exporter IFC, ruban Accueil > Projet (MainWindow_Bim.cpp).
Preuves : tests 170–180 ; IfcOpenShell 0.9 validate = 0 anomalie, 13/13 géométries ; essai GUI 2026-10-06 (import du
fichier IfcOpenShell puis export, fichier exporté valide).
MISSING : validation BIM pré-calcul, IDS, BCF, API IA, propriétés BIM dans le panneau Propriétés, AnalysisRun /
historique des versions, charges IFC (BUG-028). Collage : métadonnées transmises (BUG-029, test 190).
