# Current State

Last Updated: 2026-10-06 — branche feature/bim-core (couche BIM, IFC export / import)

Légende : IMPLEMENTED · PARTIAL · BROKEN · MISSING · UNKNOWN (preuve dans le code ou test exigée).

## Build
Status: IMPLEMENTED — PASS (preset ninja-debug, -j 4)
Compiler: MSVC 19.51 (VS 18 2026 Community), C++20
Qt: 6.11.2 · OpenCASCADE: 8.0.1 · CMake: 4.3.3 (min 3.20)
Warnings: 2 × C4996 (`TColgp_HArray1OfPnt` déprécié, src/Geometry/CableGeometry3D.cpp:171)
Note: MSVC francisé → avertissement CMake sur le préfixe /showIncludes (BUG-011).

## Tests
Status: IMPLEMENTED — 197/197 PASS (suite `bim` 170–180 le 2026-10-06, dont IFC 177–180) ;
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
l'interface (appuis via le menu Structure). Non vérifié : multi-écrans / DPI mixte, Snap layouts Win11 (BUG-033).

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
Status: PARTIAL — une vue par type, validation (bornes + longueur nulle pour les nœuds),
coalescence Undo ; édition multi-objets MISSING (seul l'élément principal est affiché).

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
Status: PARTIAL — snapshots + transactions + historique structuré + budget mémoire ;
niveaux, grilles et WorkPlanes hors snapshot (non annulables).

## Loads
Status: IMPLEMENTED — cas, combinaisons, charges nodales et sur barres (poutre/poteau/treillis/câble),
persistés dans le chunk LOAD (.tsa 1.1). Charges surfaciques / thermiques : MISSING.

## Structural Calculation
Status: PARTIAL — OpenSees 3.8.0 externe (Tcl, QProcess) ; statique, modal, pushover pour barres, treillis,
câbles, appuis/ressorts. Correspondance TSA↔OpenSees centralisée (`OpenSeesModelMap`, tags uniques) depuis le
2026-10-04 (avant : poutre N et poteau N se confondaient, treillis refusés par OpenSees, efforts lus décalés).
Mode ADVANCED : mapping DDL, k_basic/k_local/K_e, K_global (printA, ≤ 1 500 DDL), forces local/global/basic —
docs/OPENSEES_RESULTS.md. Dalles et voiles non transmis au calcul (pas de maillage) : désormais
signalé (avertissement ModelValidator + confirmation avant calcul ; « Maillage EF » présenté comme
estimation). Exécution synchrone sur le thread UI.

## Analysis engines (multi-moteurs)
Status: IMPLEMENTED (2026-10-05) — `AnalysisEngine` / registre / `AnalysisManager` ; portées : modèle complet,
sélection, axe de grille cartésienne (A, B…, 1, 2…, grille tournée incluse), niveau, plan de travail, restriction
par niveau ; extraction 2D ; validation pilotée par capacités ; fenêtre Analysis commune (F5 = dernier contexte).
OpenSees : via `OpenSeesEngine` (résultats identiques au chemin historique, test 136).
Custom2D : IMPLEMENTED (2026-10-05) — solveur MetDeDeplacement 2 (thirdparty/MetDeDeplacement, méthode des
déplacements plane, rotules, treillis, ressorts, courbes N/V/M + déformée EI v''=M) ; résultats identiques à OpenSees
sur un portique plan (test 158) ; chapitre NDC « Courbes RDM par barre » (test 159). Vérifié dans l'application : la fenêtre s'ouvre (OpenSees 3.8.0
détecté, capacités, portée, chargement). Non vérifié en GUI : calcul lancé depuis la fenêtre, panneau Propriétés.
PARTIAL : contexte non persisté dans le .tsa (BUG-013) ; Modal / Pushover du ruban encore en appel direct d'OpenSees.

## Results
Status: IMPLEMENTED — ResultsModel (éléments indexés par (famille, id), moteur / portée tracés, catégories
disponibles, tables propres au moteur), déformée/diagrammes/réactions 3D,
diagrammes 2D, NDC, dock « Données d'analyse » (tables, K globale, matrices élémentaires, contexte IA JSON),
export CSV/JSON/TXT (`ResultsExport`) ; invalidation automatique (ResultsValidityGuard).
PARTIAL — stations intermédiaires des diagrammes approchées (BUG-016) ; équilibre contrôlé en forces seulement.

## Meshing
Status: MISSING — aucun mailleur EF pour dalles/voiles.

## .tsa
Status: IMPLEMENTED — format 1.1 (chunks PROJ, THMB, COOR, GRID, NODE, SUPP, BARS, COLS, SLAB,
WALL, FNDN, TRUS, CABL, LOAD, SNAP), zlib, CRC32, écriture atomique (QSaveFile), lecture 1.0.
Chunks déclarés non utilisés : SETT, RSLT (résultats non persistés).

## Import/Export
Status: PARTIAL — export NDC PDF/HTML, export rapport de diagnostic ; import/export CAO
(IFC/STEP/DXF) : MISSING (aucune référence dans src/).

## Performance
Status: PARTIAL — mesures de référence : voir changelog 2026-10-03 et test 100.

## Threading
Status: PARTIAL — threads uniquement pour OpenSees (`QThread::create` dans OpenSeesSolver/Manager,
chemin asynchrone) ; l'UI utilise `solveSynchronous` (bloquant).

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
historique des versions, charges IFC (BUG-028), métadonnées au collage (BUG-029).
